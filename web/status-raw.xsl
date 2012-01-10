<xsl:stylesheet xmlns:xsl = "http://www.w3.org/1999/XSL/Transform" version = "1.0" >
<xsl:output method="xml" media-type="text/xml" indent="yes" encoding="UTF-8"/>
    

<xsl:template match = "/icestats" >
<status>

<!--mount point stats-->
<xsl:for-each select="source">
<mount id="{@mount}">

<xsl:if test="server_name">
<stream-title><xsl:value-of select="server_name"/></stream-title>
</xsl:if>
<xsl:if test="server_description">
<stream-description><xsl:value-of select="server_description"/></stream-description>
</xsl:if>
<xsl:if test="server_type">
<content-type><xsl:value-of select="server_type"/></content-type>
</xsl:if>
<xsl:if test="stream_start">
<mount-start><xsl:value-of select="stream_start"/></mount-start>
</xsl:if>
<xsl:if test="bitrate">
<bitrate><xsl:value-of select="bitrate"/></bitrate>
</xsl:if>
<xsl:if test="quality">
<quality><xsl:value-of select="quality"/></quality>
</xsl:if>
<xsl:if test="video_quality">
<video-quality><xsl:value-of select="video_quality"/></video-quality>
</xsl:if>
<xsl:if test="frame_size">
<framesize><xsl:value-of select="frame_size"/></framesize>
</xsl:if>
<xsl:if test="frame_rate">
<framerate><xsl:value-of select="frame_rate"/></framerate>
</xsl:if>
<xsl:if test="listeners">
<listeners><xsl:value-of select="listeners"/></listeners>
</xsl:if>
<xsl:if test="listener_peak">
<peak-listeners><xsl:value-of select="listener_peak"/></peak-listeners>
</xsl:if>
<xsl:if test="genre">
<genre><xsl:value-of select="genre"/></genre>
</xsl:if>
<xsl:if test="server_url">
<server-url><xsl:value-of select="server_url"/></server-url>
</xsl:if>
<current-song><xsl:if test="artist"><xsl:value-of select="artist"/> - </xsl:if><xsl:value-of select="title"/></current-song>
</mount>
</xsl:for-each>
</status>
</xsl:template>
</xsl:stylesheet>
