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
 
#include <time.h>
#include "rtsp_client.h"
#include "rtpproxy.h"
#include "core.h"
#include "log.h"

int 
core_remote_addr_set(core *co, stream_mode mode, b2b_side side, 
	char *host, int port)
{	
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if( NULL != host){
		co->b2bstreams[mode].remote[side].sin_addr.s_addr = inet_addr(host);
	}
	if(port > 0){
		co->b2bstreams[mode].remote[side].sin_port = htons(port);
	}
	
	return 0;
}

int 
core_remote_addr_get(core *co, stream_mode mode, b2b_side side, 
	char *host,int host_len, int *port)
{
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if( NULL != host && host_len > 0){
		char *tmp = NULL;;
		tmp = inet_ntoa(co->b2bstreams[mode].remote[side].sin_addr);
		if( NULL != tmp){
			snprintf(host, host_len, "%s", tmp);
		}
	}
	if( NULL != port){
		*port = ntohs(co->b2bstreams[mode].remote[side].sin_port);
	}
	return 0;
}

int 
core_payload_set(core *co,stream_mode mode,b2b_side side,
	char *mime_type,int media_format)
{	
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if( NULL != mime_type){
		strncpy(co->b2bstreams[mode].payload[side].mime_type,mime_type, 
					sizeof(co->b2bstreams[mode].payload[side].mime_type)-1);
	}
	if(media_format >= 0){
		co->b2bstreams[mode].payload[side].media_format = media_format;
	}
	return 0;
}

int 
core_payload_get(core *co,stream_mode mode,b2b_side side,
	char *mime_type,int mime_type_len,int *media_format)
{	
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if( NULL != mime_type && mime_type_len > 0){
		strncpy(mime_type,co->b2bstreams[mode].payload[side].mime_type,mime_type_len);
	}
	if(NULL != media_format){
		*media_format = co->b2bstreams[mode].payload[side].media_format  ;
	}
	return 0;
}

int 
core_local_addr_get(core *co, stream_mode mode, b2b_side side, 
	char *host,int host_len,int *port)
{
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if( NULL != host && host_len > 0){
		char *tmp = NULL;;
		tmp = inet_ntoa(co->b2bstreams[mode].local[side].sin_addr);
		if( NULL != tmp){
			snprintf(host, host_len, "%s", tmp);
		}
	}
	if( NULL != port){
		*port = ntohs(co->b2bstreams[mode].local[side].sin_port);
	}
	return 0;
}

int 
core_rtpproxy_get(core *co)
{
	return co->rtpproxy;
}

int 
core_rtp_start_port_get(core *co)
{
	return co->rtp_start_port;
}

int 
core_rtp_end_port_get(core *co)
{
	return co->rtp_end_port;
}

int 
core_init(core *co)
{
	int i, j ;
	memset(co,0, sizeof(core));
	co->rtpproxy = 1;
	co->rtp_start_port = 9000;
	co->rtp_end_port = 9100;
	co->symmetric_rtp = 1;
	co->expiry = 3600;
	co->session_timeout = 60;
	co->log_level = LOG_ERR;
	
	for (i = 0; i < stream_max; i++) {
		for (j = 0; j < side_max; j++) {
			co->b2bstreams[i].fds[j] = -1;
		}
	}
	co->log_queue = (osip_fifo_t *)osip_malloc(sizeof (osip_fifo_t));
	if (co->log_queue == NULL){
		return -1;
	}
  
	osip_fifo_init(co->log_queue);
	return 0;
}

int 
core_exit(core *co)
{
	log(co,LOG_NOTICE,"program has terminated.\n");
	
	streams_stop(co);
	if (co->log_fd != NULL && co->log_fd != stdout){
		fclose(co->log_fd);
	}
	
	cfg_destroy(co->cfg);
	osip_fifo_free(co->log_queue);
	return 0;
}

int 
core_show(core *co)
{
	uint32_t ip1,ip2,ip3,ip4;
	
	ip1 = (&co->b2bstreams[stream_audio_rtp].remote[side_sip])->sin_addr.s_addr;
	ip2 = (&co->b2bstreams[stream_audio_rtp].local[side_sip])->sin_addr.s_addr;
	ip3 = (&co->b2bstreams[stream_audio_rtp].local[side_rtsp])->sin_addr.s_addr;
	ip4 = (&co->b2bstreams[stream_audio_rtp].remote[side_rtsp])->sin_addr.s_addr;
	
	/* sip<==>sip_side-rtsp_side<==>rtsp */	
	log(co,LOG_INFO,
		"audio %d.%d.%d.%d:%d<==>%d.%d.%d.%d:%d(%d:%s)-%d.%d.%d.%d:%d(%d:%s)<==>%d.%d.%d.%d:%d\n",
		(ip1 >> 0)&0x000000FF,(ip1 >> 8)&0x000000FF,(ip1 >> 16)&0x000000FF,(ip1 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_audio_rtp].remote[side_sip])->sin_port),
		(ip2 >> 0)&0x000000FF,(ip2 >> 8)&0x000000FF,(ip2 >> 16)&0x000000FF,(ip2 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_audio_rtp].local[side_sip])->sin_port),
		co->b2bstreams[stream_audio_rtp].payload[side_sip].media_format,
		co->b2bstreams[stream_audio_rtp].payload[side_sip].mime_type,
		(ip3 >> 0)&0x000000FF,(ip3 >> 8)&0x000000FF,(ip3 >> 16)&0x000000FF,(ip3 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_audio_rtp].local[side_rtsp])->sin_port),
		co->b2bstreams[stream_audio_rtp].payload[side_rtsp].media_format,
		co->b2bstreams[stream_audio_rtp].payload[side_rtsp].mime_type,
		(ip4 >> 0)&0x000000FF,(ip4 >> 8)&0x000000FF,(ip4 >> 16)&0x000000FF,(ip4 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_audio_rtp].remote[side_rtsp])->sin_port));

	ip1 = (&co->b2bstreams[stream_video_rtp].remote[side_sip])->sin_addr.s_addr;
	ip2 = (&co->b2bstreams[stream_video_rtp].local[side_sip])->sin_addr.s_addr;
	ip3 = (&co->b2bstreams[stream_video_rtp].local[side_rtsp])->sin_addr.s_addr;
	ip4 = (&co->b2bstreams[stream_video_rtp].remote[side_rtsp])->sin_addr.s_addr;
	log(co,LOG_INFO,
		"video %d.%d.%d.%d:%d<==>%d.%d.%d.%d:%d(%d:%s)-%d.%d.%d.%d:%d(%d:%s)<==>%d.%d.%d.%d:%d\n",
		(ip1>> 0)&0x000000FF,(ip1 >> 8)&0x000000FF,(ip1 >> 16)&0x000000FF,(ip1 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_video_rtp].remote[side_sip])->sin_port),
		(ip2 >> 0)&0x000000FF,(ip2 >> 8)&0x000000FF,(ip2 >> 16)&0x000000FF,(ip2 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_video_rtp].local[side_sip])->sin_port),
		co->b2bstreams[stream_video_rtp].payload[side_sip].media_format,
		co->b2bstreams[stream_video_rtp].payload[side_sip].mime_type,
		(ip3 >> 0)&0x000000FF,(ip3 >> 8)&0x000000FF,(ip3 >> 16)&0x000000FF,(ip3 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_video_rtp].local[side_rtsp])->sin_port),
		co->b2bstreams[stream_video_rtp].payload[side_rtsp].media_format,
		co->b2bstreams[stream_video_rtp].payload[side_rtsp].mime_type,
		(ip4 >> 0)&0x000000FF,(ip4 >> 8)&0x000000FF,(ip4 >> 16)&0x000000FF,(ip4 >> 24)&0x000000FF,
		ntohs((&co->b2bstreams[stream_video_rtp].remote[side_rtsp])->sin_port));
	return 0;
}

