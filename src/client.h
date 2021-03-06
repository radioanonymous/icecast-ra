/* Icecast
 *
 * This program is distributed under the GNU General Public License, version 2.
 * A copy of this license is included with this source.
 *
 * Copyright 2000-2004, Jack Moffitt <jack@xiph.org, 
 *                      Michael Smith <msmith@xiph.org>,
 *                      oddsock <oddsock@xiph.org>,
 *                      Karl Heyes <karl@xiph.org>
 *                      and others (see AUTHORS for details).
 */

/* client.h
**
** client data structions and function definitions
**
*/
#ifndef __CLIENT_H__
#define __CLIENT_H__

typedef struct _client_tag client_t;
typedef struct _worker_t worker_t;

#include "cfgfile.h"
#include "connection.h"
#include "refbuf.h"
#include "httpp/httpp.h"
#include "compat.h"
#include "thread/thread.h"

struct _worker_t
{
    int running;
    int count, pending_count;
    spin_t lock;
    int wakeup_fd[2];

    client_t *pending_clients;
    client_t **pending_clients_tail,
             *clients;
    client_t **last_p;
    thread_type *thread;
    struct timespec current_time;
    uint64_t time_ms;
    uint64_t wakeup_ms;
    struct _worker_t *next;
};


extern worker_t *workers;
extern int worker_count;
extern rwlock_t workers_lock;

struct _client_functions
{
    int  (*process)(struct _client_tag *client);
    void (*release)(struct _client_tag *client);
};

struct _client_tag
{
    uint64_t schedule_ms;

    /* various states the client could be in */
    unsigned int flags;

    /* position in first buffer */
    unsigned int pos;

    client_t *next_on_worker;

    /* functions to process client */
    struct _client_functions *ops;

    /* function to check if refbuf needs updating */
    int (*check_buffer)(struct _client_tag *client);

    /* generic handle */
    void *shared_data;

    /* current mountpoint */
    const char *mount;

    /* the clients connection */
    connection_t connection;

    /* the client's http headers */
    http_parser_t *parser;

    /* reference to incoming connection details */
    listener_t *server_conn;

    /* is client getting intro data */
    long intro_offset;

    /* where in the queue the client is */
    refbuf_t *refbuf;

    /* byte count in queue */
    uint64_t queue_pos;

    /* Client username, if authenticated */
    char *username;

    /* Client password, if authenticated */
    char *password;

    /* Format-handler-specific data for this client */
    void *format_data;

    /* the worker the client is attached to */
    worker_t *worker;

    time_t timer_start;
    uint64_t counter;

    /* function to call to release format specific resources */
    void (*free_client_data)(struct _client_tag *client);

    /* http response code for this client */
    int respcode;
};

void client_register (client_t *client);
void client_destroy(client_t *client);
void client_send_504(client_t *client, char *message);
int  client_send_416(client_t *client);
int  client_send_404(client_t *client, const char *message);
int  client_send_401(client_t *client, const char *realm);
int  client_send_403(client_t *client, const char *reason);
int  client_send_403redirect (client_t *client, const char *mount, const char *reason);
int  client_send_400(client_t *client, const char *message);
int  client_send_302(client_t *client, const char *location);
int  client_send_bytes (client_t *client, const void *buf, unsigned len);
int  client_read_bytes (client_t *client, void *buf, unsigned len);
void client_set_queue (client_t *client, refbuf_t *refbuf);
int  client_compare (void *compare_arg, void *a, void *b);

int  client_change_worker (client_t *client, worker_t *dest_worker);
void client_add_worker (client_t *client);
worker_t *find_least_busy_handler (void);
void workers_adjust (int new_count);
void worker_wakeup (worker_t *worker);


/* client flags bitmask */
#define CLIENT_ACTIVE               (001)
#define CLIENT_AUTHENTICATED        (002)
#define CLIENT_IS_SLAVE             (004)
#define CLIENT_IN_FSERVE            (010)
#define CLIENT_NO_CONTENT_LENGTH    (020)
#define CLIENT_HAS_INTRO_CONTENT    (040)
#define CLIENT_SKIP_ACCESSLOG       (0100)
#define CLIENT_HAS_MOVED            (0200)
#define CLIENT_IP_BAN_LIFT          (0400)
#define CLIENT_FORMAT_BIT           (010000)

#endif  /* __CLIENT_H__ */
