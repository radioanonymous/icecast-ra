/* Icecast
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2000-2004, Jack Moffitt <jack@xiph.org,
 *                      Michael Smith <msmith@xiph.org>,
 *                      oddsock <oddsock@xiph.org>,
 *                      Karl Heyes <karl@xiph.org>,
 *                      radioneko <kawaii.neko@pochta.ru>
 *                      and others (see AUTHORS for details).
 */

/** 
 * Client authentication functions
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "auth.h"
#include "auth_radio.h"
#include "source.h"
#include "client.h"
#include "cfgfile.h"
#include "httpp/httpp.h"
#include "md5.h"
#include "global.h"

#include "logging.h"
#define CATMODULE "auth_radio"

typedef struct s_radio_user
{
    char					*md5id;					/**< md5 hash of password */
	char					*id;					/**< user identifier (usually jid) */
	time_t					blocked_till;			/**< account is blocked till specified time (0 - not blocked) */
	char					*nick;					/**< nickname */
	char					*skype;					/**< skype */
	struct s_radio_user		*next;
} radio_user;

typedef struct {
    char		*filename;
    rwlock_t	file_rwlock;
	radio_user	*users;
    time_t		mtime;
} radio_auth_state;

static auth_result radio_adduser (auth_t *auth, const char *username, const char *password);
static auth_result radio_deleteuser(auth_t *auth, const char *username);
static auth_result radio_userlist(auth_t *auth, xmlNodePtr srcnode);
static radio_user *free_user (radio_user *user);

static void radio_clear(auth_t *self)
{
	radio_auth_state *state = self->state;
	free(state->filename);
	while (state->users)
		state->users = free_user(state->users);
	thread_rwlock_destroy(&state->file_rwlock);
	free(state);
}


/* md5 hash */
static char *get_hash(const char *data, size_t len)
{
    struct MD5Context context;
    unsigned char digest[16];

    MD5Init(&context);

    MD5Update(&context, (const unsigned char *)data, len);

    MD5Final(digest, &context);

    return util_bin_to_hex(digest, 16);
}


static radio_user *free_user(radio_user *user)
{
	radio_user *u = user->next;
	if (user->md5id)
		free(user->md5id);
	if (user->id)
		free(user->id);
	if (user->nick)
		free(user->nick);
	if (user->skype)
		free(user->skype);
	return u;
}

static void free_users(radio_user *user)
{
	while (user)
		user = free_user(user);
}

static int is_separator(char c)
{
	return !(c & ~0x1f) || c == ':';
}

static void update_acl(radio_auth_state *auth)
{
	int result = 0;
	struct stat st;
	if (stat(auth->filename, &st) == 0 && S_ISREG(st.st_mode) && st.st_mtime != auth->mtime) {
		FILE *in;
		thread_rwlock_wlock(&auth->file_rwlock);
		in = fopen(auth->filename, "r");
		if (in) {
			radio_user *u = NULL;
			char line[4096];
			int failure = -1;
			auth->mtime = st.st_mtime;
			while (fgets(line, sizeof(line), in)) {
				failure++;
				if (*line == '#' || !(*line & ~0x1f))
					continue;
				unsigned i;
				char *p = line, *v[5];
				memset(v, 0, sizeof(v));
				for (i = 0; i < sizeof(v) / sizeof(*v); i++) {
					while (*p && is_separator(*p))
						p++;
					if (!*p)
						break;
					/* Beginning of the identifier */
					v[i] = p;
					while (*p && !is_separator(*p))
						p++;
					if (!*p)
						break;
					*p++ = 0;
				}
				/* Validate data */
				if (strlen(v[0]) != 32 || i != sizeof(v) / sizeof(*v))
					goto stop;
				/* Create new user */
				radio_user *n = calloc(1, sizeof(radio_user));
				n->md5id = strdup(v[0]);
				n->id = strdup(v[1]);
				n->blocked_till = atoll(v[2]);
				n->nick = strdup(v[3]);
				n->skype = strdup(v[4]);
				n->next = u;
				u = n;
			}
			failure = -1;
stop:
			if (failure == -1) {
				free_users(auth->users);
				auth->users = u;
			} else {
				free_users(u);
			}
			fclose(in);
		}
		thread_rwlock_unlock(&auth->file_rwlock);
	}
}


static void radio_auth (auth_client *auth_user)
{
    auth_t *auth = auth_user->auth;
    radio_auth_state *state = auth->state;
	const char *pass = auth_user->client->password;
	char *md5 = get_hash(pass, strlen(pass));
	radio_user *u;
	update_acl(state);
	thread_rwlock_rlock(&state->file_rwlock);
	for (u = state->users; u; u = u->next)
		if (memcmp(md5, u->md5id, 32) == 0)
			break;
	if (u && u->blocked_till < time(NULL)) {
		auth_user->client->flags |= CLIENT_AUTHENTICATED;
	}
	free(md5);
}


int  auth_get_radio_auth (auth_t *authenticator, config_options_t *options)
{
    radio_auth_state *state;

    authenticator->stream_auth = radio_auth;
    authenticator->release = radio_clear;
    authenticator->adduser = radio_adduser;
    authenticator->deleteuser = radio_deleteuser;
    authenticator->listuser = radio_userlist;

    state = calloc(1, sizeof(radio_auth_state));

    while(options) {
        if(!strcmp(options->name, "filename"))
            state->filename = strdup(options->value);
        options = options->next;
    }

    if(!state->filename) {
        free(state);
        ERROR0("No filename given in options for authenticator.");
        return -1;
    }

    authenticator->state = state;
    DEBUG1("Configured htpasswd authentication using password file %s", 
            state->filename);

    thread_rwlock_create(&state->file_rwlock);

	update_acl(state);

    return 0;
}


static auth_result radio_adduser (auth_t *auth, const char *username, const char *password)
{
    return AUTH_FAILED;
}


static auth_result radio_deleteuser(auth_t *auth, const char *username)
{
    return AUTH_FAILED;
}


static auth_result radio_userlist(auth_t *auth, xmlNodePtr srcnode)
{
    return AUTH_FAILED;
}

