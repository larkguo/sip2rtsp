/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * Contributor(s):
 *              larkguo@gmail.com
 */

#ifndef __CORE_H__
#define __CORE_H__

#include	<netinet/in.h>
#include	<fcntl.h>	
#include <osip2/osip_mt.h>
#include <eXosip2/eXosip.h>
#include <osipparser2/osip_md5.h>
#include <osip2/osip_fifo.h>
#include "cfg.h"

#define _GNU_SOURCE
#define PROG_NAME "sip2rtsp"
#define PROG_VER  "1.0.0"
#define UA_STRING PROG_NAME " v" PROG_VER

typedef enum{
	stream_audio_rtp = 0,
	stream_audio_rtcp,
	stream_video_rtp,
	stream_video_rtcp,
	stream_max,
}stream_mode;

typedef enum{
	side_sip = 0,
	side_rtsp,
	side_max,
}b2b_side;

typedef struct poayload_type_t {
	int media_format;	/* 0: PCMU, 8:PCMA */
	char mime_type[64]; 	/* PCMU, H264 */
}payload_type;

typedef struct b2bstream_t {
	int fds[side_max];
	payload_type payload[side_max];
	struct sockaddr_in	 remote[side_max];
	struct sockaddr_in	 local[side_max];
	
} b2bstream;

/* sip2rtsp co struct */
typedef struct core_t {

	/* config */
	char *cfg_file;
	Cfg *cfg;
	
	/* debug */
	char *log_file;
	FILE *log_fd;
	int log_level; /* 0-7 , 0:LOG_EMERG, 7:LOG_DEBUG */
	osip_fifo_t *log_queue;
	struct osip_thread *log_thread;
	
	/* sip */
	int localport ;
	char *localip ;
	char *contact ;
	char *fromuser ;
	char *firewallip ;
	char *proxy ;
	char *outboundproxy;
	char *authusername;
	char *authpassword;
	int expiry;
	
	/* rtsp */
	char *rtsp_url ;
	char *rtsp_username;
	char *rtsp_password;
	int session_timeout;

	/* rtpproxy */
	int symmetric_rtp;
	int rtpproxy;
	unsigned short rtp_start_port;
	unsigned short rtp_end_port;
	int maxfd;
	b2bstream b2bstreams[stream_max];
} core;


#ifdef __cplusplus
extern "C" {
#endif

int core_remote_addr_set(core *co, stream_mode mode, b2b_side side, 
	char *host, int port);

int core_remote_addr_get(core *co, stream_mode mode, b2b_side side, 
	char *host,int host_len, int *port);

int core_payload_set(core *co,stream_mode mode,b2b_side side,
	char *mime_type,int media_format);

int core_payload_get(core *co,stream_mode mode,b2b_side side,
	char *mime_type,int mime_type_len,int *media_format);

int core_local_addr_get(core *co,stream_mode mode, b2b_side side, 
	char *host,int host_len,int *port);

int core_rtpproxy_get(core *co);
int core_rtp_start_port_get(core *co);
int core_rtp_end_port_get(core *co);
int core_init(core *co);
int core_show(core *co);
int core_exit(core *co);

#ifdef __cplusplus
}
#endif

#endif

