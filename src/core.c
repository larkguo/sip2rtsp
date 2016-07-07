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
core_remote_addr_set(core *co,int callid,stream_mode mode,b2b_side side, 
	char *host, int port)
{	
	if(NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}

	if(side_sip == side){ /* sip */
		int i = -1;
		for(i = 0; i < MAX_SIPCALL; i++) {
			if(callid == co->sipcall[i].callid ){
				if(NULL != host){
					co->sipcall[i].remote[mode].sin_addr.s_addr = inet_addr(host);
				}
				if(port > 0){
					co->sipcall[i].remote[mode].sin_port = htons(port);
				}
			}
		}
	}else{ /* rtsp */
		if(NULL != host){
			co->rtsp.remote[mode].sin_addr.s_addr = inet_addr(host);
		}
		if(port > 0){
			co->rtsp.remote[mode].sin_port = htons(port);
		}
	}
	
	return 0;
}

int 
core_remote_addr_get(core *co,int callid,stream_mode mode,b2b_side side, 
	char *host,int host_len, int *port)
{
	char *tmp_host = NULL;
	int	tmp_port = -1;
	
	if(NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}

	if(side_sip == side){ /* sip */
		int i = -1;
		for(i = 0; i < MAX_SIPCALL; i++) {
			if(callid == co->sipcall[i].callid ){
				tmp_host = inet_ntoa(co->sipcall[i].remote[mode].sin_addr);
				tmp_port = ntohs(co->sipcall[i].remote[mode].sin_port);
				break;
			}
		}
	}else{ /* rtsp */
		tmp_host = inet_ntoa(co->rtsp.remote[mode].sin_addr);
		tmp_port = ntohs(co->rtsp.remote[mode].sin_port);
	}
	
	if(NULL != host && host_len > 0){
		if(NULL != tmp_host){
			snprintf(host, host_len, "%s", tmp_host);
		}
	}
	if( NULL != port){
		*port = tmp_port;
	}
		
	return 0;
}

int 
core_payload_set(core *co,int callid,stream_mode mode,b2b_side side,
	char *mime_type,int media_format)
{	
	if(NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}

	if(side_sip == side){ /* sip */
		int i = -1;
		for(i = 0; i < MAX_SIPCALL; i++) {
			if(callid == co->sipcall[i].callid){
				if(NULL != mime_type){
					strncpy(co->sipcall[i].payload[mode].mime_type,mime_type,
						sizeof(co->sipcall[i].payload[mode].mime_type)-1);
				}
				if(media_format >= 0){
					co->sipcall[i].payload[mode].media_format = media_format;
				}
				log(co,LOG_DEBUG, "call(%d-%d) stream(%d) payload_set %d:%s\n",
					i,co->sipcall[i].callid, mode, 
					co->sipcall[i].payload[mode].media_format,co->sipcall[i].payload[mode].mime_type);
				break;
			}
		}
	}else{ /* rtsp */
		if(NULL != mime_type){
			strncpy(co->rtsp.payload[mode].mime_type,mime_type,
				sizeof(co->rtsp.payload[mode].mime_type)-1);
		}
		if(media_format >= 0){
			co->rtsp.payload[mode].media_format = media_format;
		}
	}

	return 0;
}

int 
core_payload_get(core *co,int callid,stream_mode mode,b2b_side side,
	char *mime_type,int mime_type_len,int *media_format)
{	
	if(NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}

	if(side_sip == side){ /* sip */
		int i = -1;
		for(i = 0; i < MAX_SIPCALL; i++) {
			if(callid == co->sipcall[i].callid ){
				if(NULL != mime_type && mime_type_len > 0){
					strncpy(mime_type,co->sipcall[i].payload[mode].mime_type,mime_type_len);
				}
				if(NULL != media_format){
					*media_format = co->sipcall[i].payload[mode].media_format;
				}
				break;
			}
		}
	}else{ /* rtsp */
		if(NULL != mime_type && mime_type_len > 0){
			strncpy(mime_type,co->rtsp.payload[mode].mime_type,mime_type_len);
		}
		if(NULL != media_format){
			*media_format = co->rtsp.payload[mode].media_format;
		}
	}
	
	return 0;
}

int 
core_local_addr_get(core *co,int callid,stream_mode mode, b2b_side side, 
	char *host,int host_len,int *port)
{
	char *tmp_host = NULL;
	int tmp_port = -1;
	
	if(NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if(side_sip == side){ /* sip */
		int i = -1;
		for(i = 0; i < MAX_SIPCALL; i++) {
			if( callid == co->sipcall[i].callid ){
				tmp_host = inet_ntoa(co->sipcall[i].local[mode].sin_addr);
				tmp_port = ntohs(co->sipcall[i].local[mode].sin_port);
				break;
			}
		}
	}else{ /* rtsp */
		tmp_host = inet_ntoa(co->rtsp.local[mode].sin_addr);
		tmp_port = ntohs(co->rtsp.local[mode].sin_port);
	}

	if(NULL != host && host_len > 0){
		if(NULL != tmp_host){
			snprintf(host, host_len, "%s", tmp_host);
		}
	}
	if( NULL != port){
		*port = tmp_port;
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
core_rtp_current_port_get(core *co)
{
	return co->rtp_current_port;
}

int 
core_rtp_current_port_set(core *co, int port)
{
	if(port >= co->rtp_start_port && port <= co->rtp_end_port){
		co->rtp_current_port = port;
		return 0;
	}
	return -1;
}

int 
core_init(core *co)
{
	int i;
	memset(co,0, sizeof(core));
	co->rtpproxy = 1;
	co->rtp_start_port = 9000;
	co->rtp_end_port = 9100;
	co->rtp_current_port = co->rtp_start_port;
	co->symmetric_rtp = 1;
	co->expiry = 3600;
	co->session_timeout = 60;
	co->log_level = LOG_ERR;

	for(i = 0; i < MAX_SIPCALL; i++) {
		co->sipcall[i].callid = -1;
		co->sipcall[i].audio_dir = stream_sendrecv;
		co->sipcall[i].video_dir = stream_sendrecv;
	}
	
	co->log_queue = (osip_fifo_t *)osip_malloc(sizeof(osip_fifo_t));
	if(co->log_queue == NULL){
		return -1;
	}
  
	osip_fifo_init(co->log_queue);
	return 0;
}

int 
core_exit(core *co)
{
	log(co,LOG_NOTICE,"program has terminated.\n");

	if(co->log_fd != NULL && co->log_fd != stdout){
		fclose(co->log_fd);
	}
	
	cfg_destroy(co->cfg);
	osip_fifo_free(co->log_queue);
	return 0;
}

/* 
* sip<==>sip_side-rtsp_side<==>rtsp 
*/	
int 
core_show(core *co)
{
	uint32_t ip1,ip2,ip3,ip4,ip5,ip6,ip7,ip8;
	int port1,port2,port3,port4,port5,port6,port7,port8;
	int i = -1;
	stream_dir dir = stream_sendrecv;
	char dir_str[32]={0};

	for(i = 0; i < MAX_SIPCALL; i++) {
		if(-1 == co->sipcall[i].callid) 	continue;

		/* audio */
		ip1 = (&co->sipcall[i].remote[stream_audio_rtp])->sin_addr.s_addr;
		ip2 = (&co->sipcall[i].local[stream_audio_rtp])->sin_addr.s_addr;
		ip3 = (&co->rtsp.local[stream_audio_rtp])->sin_addr.s_addr;
		ip4 = (&co->rtsp.remote[stream_audio_rtp])->sin_addr.s_addr;
		port1 = ntohs((&co->sipcall[i].remote[stream_audio_rtp])->sin_port);
		port2 = ntohs((&co->sipcall[i].local[stream_audio_rtp])->sin_port);
		port3 = ntohs((&co->rtsp.local[stream_audio_rtp])->sin_port);
		port4 = ntohs((&co->rtsp.remote[stream_audio_rtp])->sin_port);
		dir = core_sipcall_dir_get(co,co->sipcall[i].callid,stream_audio_rtp);
		if(stream_sendonly == dir || stream_inactive == dir) {
			strncpy(dir_str,"==",sizeof(dir_str)-1);
		}else{
			strncpy(dir_str,"<==",sizeof(dir_str)-1);
		}
		log(co,LOG_INFO,
			"call(%d-%d) %d.%d.%d.%d:%d %s %d.%d.%d.%d:%d(%d:%s)-%d.%d.%d.%d:%d(%d:%s) %s %d.%d.%d.%d:%d\n",
			i,co->sipcall[i].callid,
			(ip1 >> 0)&0x000000FF,(ip1 >> 8)&0x000000FF,(ip1 >> 16)&0x000000FF,(ip1 >> 24)&0x000000FF,
			port1,
			dir_str,
			(ip2 >> 0)&0x000000FF,(ip2 >> 8)&0x000000FF,(ip2 >> 16)&0x000000FF,(ip2 >> 24)&0x000000FF,
			port2,
			co->sipcall[i].payload[stream_audio_rtp].media_format,
			co->sipcall[i].payload[stream_audio_rtp].mime_type,
			(ip3 >> 0)&0x000000FF,(ip3 >> 8)&0x000000FF,(ip3 >> 16)&0x000000FF,(ip3 >> 24)&0x000000FF,
			port3,
			co->rtsp.payload[stream_audio_rtp].media_format,
			co->rtsp.payload[stream_audio_rtp].mime_type,
			dir_str,
			(ip4 >> 0)&0x000000FF,(ip4 >> 8)&0x000000FF,(ip4 >> 16)&0x000000FF,(ip4 >> 24)&0x000000FF,
			port4);

		/* video */
		ip5 = (&co->sipcall[i].remote[stream_video_rtp])->sin_addr.s_addr;
		ip6 = (&co->sipcall[i].local[stream_video_rtp])->sin_addr.s_addr;
		ip7 = (&co->rtsp.local[stream_video_rtp])->sin_addr.s_addr;
		ip8 = (&co->rtsp.remote[stream_video_rtp])->sin_addr.s_addr;
		port5 = ntohs((&co->sipcall[i].remote[stream_video_rtp])->sin_port);
		port6 = ntohs((&co->sipcall[i].local[stream_video_rtp])->sin_port);
		port7 = ntohs((&co->rtsp.local[stream_video_rtp])->sin_port);
		port8 = ntohs((&co->rtsp.remote[stream_video_rtp])->sin_port);
		dir = core_sipcall_dir_get(co,co->sipcall[i].callid,stream_video_rtp);
		if(stream_sendonly == dir || stream_inactive == dir) {
			strncpy(dir_str,"==",sizeof(dir_str)-1);
		}else{
			strncpy(dir_str,"<==",sizeof(dir_str)-1);
		}
		log(co,LOG_INFO,
			"call(%d-%d) %d.%d.%d.%d:%d %s %d.%d.%d.%d:%d(%d:%s)-%d.%d.%d.%d:%d(%d:%s) %s %d.%d.%d.%d:%d\n",
			i,co->sipcall[i].callid,
			(ip5 >> 0)&0x000000FF,(ip5 >> 8)&0x000000FF,(ip5 >> 16)&0x000000FF,(ip5 >> 24)&0x000000FF,
			port5,
			dir_str,
			(ip6 >> 0)&0x000000FF,(ip6 >> 8)&0x000000FF,(ip6 >> 16)&0x000000FF,(ip6 >> 24)&0x000000FF,
			port6,
			co->sipcall[i].payload[stream_video_rtp].media_format,
			co->sipcall[i].payload[stream_video_rtp].mime_type,
			(ip7 >> 0)&0x000000FF,(ip7 >> 8)&0x000000FF,(ip7 >> 16)&0x000000FF,(ip7 >> 24)&0x000000FF,
			port7,
			co->rtsp.payload[stream_video_rtp].media_format,
			co->rtsp.payload[stream_video_rtp].mime_type,
			dir_str,
			(ip8 >> 0)&0x000000FF,(ip8 >> 8)&0x000000FF,(ip8 >> 16)&0x000000FF,(ip8 >> 24)&0x000000FF,
			port8);
	}
	
	return 0;
}


int 
core_sipcallnum_get(core *co)
{
	return co->sipcallnum;
}

int 
core_sipcallnum_add(core *co)
{
	co->sipcallnum++;
	
	log(co,LOG_DEBUG,"add sipcallnum=%d\n",co->sipcallnum);
	
	return co->sipcallnum;
}

int 
core_sipcallnum_sub(core *co)
{
	co->sipcallnum--;
	log(co,LOG_DEBUG,"sub sipcallnum=%d\n",co->sipcallnum);
	return co->sipcallnum;
}

int 
core_sipcall_release(core *co,int callid)
{
	int i = -1;
	for(i = 0; i < MAX_SIPCALL; i++) {
		if(callid == co->sipcall[i].callid){
			co->sipcall[i].callid = -1;
			core_sipcallnum_sub(co);
		}
	}
	return 0;
}

int 
core_sipcall_set(core *co,struct eXosip_t *context,eXosip_event_t *je)
{
	int i = -1;
	int  oldest_callid = -1;
	int  oldest_dialogid = -1;
	int  oldest_index = -1;
	int callid = je->cid;
	int dialogid = je->did;
	
	/* find */
	for(i = 0; i < MAX_SIPCALL; i++) {
		if(callid == co->sipcall[i].callid){
			return 0;
		}
	}

	/* new set */
	for(i = 0; i < MAX_SIPCALL; i++) {
		if( -1 == co->sipcall[i].callid){
			co->sipcall[i].callid = callid;
			co->sipcall[i].dialogid = dialogid;
			core_sipcallnum_add(co);
			return 0;
		}else{
			if(oldest_callid > co->sipcall[i].callid || -1 == oldest_callid){
				oldest_callid = co->sipcall[i].callid;
				oldest_dialogid = co->sipcall[i].dialogid;
				oldest_index = i;
			}
		}
	}

	/* no space,replace oldest */
	if(oldest_index >= 0 && oldest_index < MAX_SIPCALL){
		int ret = -1;
		eXosip_lock(context);
		ret = eXosip_call_terminate(context,oldest_callid,oldest_dialogid);
		eXosip_unlock(context);
		log(co,LOG_DEBUG,"eXosip_call_terminate call(%d-%d:%d)=%d\n",
			oldest_index,oldest_callid,oldest_dialogid,ret);
		
		co->sipcall[oldest_index].callid = callid;
		co->sipcall[oldest_index].dialogid = dialogid;
		log(co,LOG_INFO,"maxcalls %d full,replace oldest call(%d-%d:%d) to call(%d-%d:%d)\n",
			MAX_SIPCALL,oldest_index,oldest_callid,oldest_dialogid,oldest_index,callid,dialogid);
		
		return 0;
	}
	
	return -1;
}

int 
core_audiodir_set(core *co,int callid,stream_dir dir)
{
	int i = -1;

	for(i = 0; i < MAX_SIPCALL; i++) {
		if(callid == co->sipcall[i].callid){
			co->sipcall[i].audio_dir = dir;
		}
	}

	return 0;
}

int 
core_videodir_set(core *co,int callid,stream_dir dir)
{
	int i = -1;

	for(i = 0; i < MAX_SIPCALL; i++) {
		if(callid == co->sipcall[i].callid){
			co->sipcall[i].video_dir = dir;
		}
	}
	return 0;
}

stream_dir 
core_sipcall_dir_get(core *co,int callid,stream_mode mode)
{
	int i = -1;

	for(i = 0; i < MAX_SIPCALL; i++) {
		if(callid == co->sipcall[i].callid){
			if(stream_audio_rtp == mode || stream_audio_rtcp == mode) {	
				return co->sipcall[i].audio_dir;
			}
			if(stream_video_rtp == mode || stream_video_rtcp == mode) {	
				return co->sipcall[i].video_dir;
			}
		}
	}
	return stream_sendrecv;
}
