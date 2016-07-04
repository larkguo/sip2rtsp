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
#include "sip.h"

struct eXosip_t *excontext = NULL;

static void sip_add_outboundproxy(osip_message_t *msg,const char *outboundproxy);
static int sip_uas_process_acl(core *co,struct eXosip_t *context,eXosip_event_t *je);
static int sip_uas_process_invite(core *co,struct eXosip_t *context,eXosip_event_t *je);
static int sip_uas_process_terminated(core *co,struct eXosip_t *context,eXosip_event_t *je);
static int sip_uas_process_other(core *co,struct eXosip_t *context,eXosip_event_t *je);
static int sdp_message_media_pt_process(core *co,sdp_message_t *sdp,int pos);
static int sip_sdp_answer(IN core *co, INOUT sdp_message_t  *rtsp_sdp,
	IN rtsp_transport_parse_t *video_transport, IN rtsp_transport_parse_t *audio_transport);

static void 
sip_add_outboundproxy(osip_message_t *msg, const char *outboundproxy){
	char head[HEAD_BUFF_DEFAULT_LEN]={0};
	snprintf(head,sizeof(head)-1,"<%s;lr>",outboundproxy);
	
	osip_list_special_free(&msg->routes,(void (*)(void*))osip_route_free);
	osip_message_set_route(msg,head);
}


static int 
sip_uas_process_acl(core *co,struct eXosip_t *context,eXosip_event_t *je)
{	
	int ret = 0;
	int status = 403;
	osip_message_t *answer = NULL;
	
	/* TODO: check fromUser, fromIP, and so on */
	if( ret < 0){
		eXosip_lock(context);
		eXosip_call_build_answer(context,je->tid,status,&answer);
		if(answer){
			eXosip_call_send_answer(context,je->tid,status,answer);
		}
		eXosip_unlock(context);
	}
	return ret;
}

/* return the value of attr "field" for payload pt at line pos (field=rtpmap,fmtp...)*/
static const char *
sdp_message_a_attr_value_get_with_pt(sdp_message_t *sdp,
	int pos,int pt,const char *field)
{
	int i,tmppt=0,scanned=0;
	char *tmp;
	sdp_attribute_t *attr;
	for(i=0;(attr=sdp_message_attribute_get(sdp,pos,i))!=NULL;i++){
		if(strncmp(field,attr->a_att_field,strlen(field))==0 && attr->a_att_value!=NULL){
			int nb = sscanf(attr->a_att_value,"%i %n",&tmppt,&scanned);
			/* the return value may depend on how %n is interpreted by the libc:see manpage*/
			if(nb == 1 || nb==2 ){
				if(pt==tmppt){
					tmp=attr->a_att_value+scanned;
					if(strlen(tmp)>0)
						return tmp;
				}
			}
		}
	}
	return NULL;
}

static int 
sdp_message_media_pt_process(core *co,sdp_message_t *sdp,int pos)
{
	int i,tmppt=0,scanned=0;
	char *tmp = NULL;
	sdp_attribute_t *attr = NULL;
	char *number=NULL;
	int pt_new = -1;
	int pt_old = -1;
	char pt_str[32]={0};
	char *mtype = NULL;

	if( NULL == co || NULL == sdp )
		return -1;
	
	mtype = sdp_message_m_media_get(sdp, pos);
	if( NULL == mtype)
		return -1;
	
	if( 0 == strncasecmp("audio", mtype, strlen("audio")) ){
		core_payload_get(co,stream_audio_rtp,side_sip,NULL,0, &pt_new);
	}else if( 0 == strncasecmp("video", mtype, strlen("video")) ){
		core_payload_get(co,stream_video_rtp,side_sip,NULL,0, &pt_new);
	}else{
		return 0;
	}
	number = sdp_message_m_payload_get(sdp, pos, 0);
	if(NULL == number )
		return -1;
	pt_old = atoi(number);
	if(pt_old == pt_new){
		return 0;
	}
	
	/* m= */
	snprintf (pt_str, sizeof (pt_str)-1, "%i", pt_new);	
	log(co,LOG_DEBUG,"meida payload %s=>%s\n", number, pt_str);
	
	sdp_message_m_payload_del(sdp,pos,0);
	sdp_message_m_payload_add(sdp,pos, osip_strdup(pt_str));
	
	/* a= */			
	for(i=0;(attr=sdp_message_attribute_get(sdp,pos,i))!=NULL;i++){
		if( attr->a_att_value!=NULL){
			int nb = sscanf(attr->a_att_value,"%i %n",&tmppt,&scanned);
			/* the return value may depend on how %n is interpreted by the libc:see manpage*/
			if(nb == 1 || nb==2 ){
				if(pt_old==tmppt){ 
					tmp=attr->a_att_value+scanned;
					char buff[1024]={0};
					if(strlen(tmp)>0){
						snprintf(buff, sizeof(buff)-1, "%i %s", pt_new,tmp);
						log(co,LOG_DEBUG,"attribute %s %s->%s\n",attr->a_att_field,attr->a_att_value,buff);
						osip_free(attr->a_att_value);
						attr->a_att_value = osip_strdup(buff);
					}
				}
			}
		}
	}
	return 0;
}

static int 
rtsp_media_process(core *co,sdp_message_t *rtsp_sdp)
{
	int i,j;
	char *mtype=NULL,*number=NULL;

	if(NULL == co  || NULL == rtsp_sdp)
		return -1;

	for(i = 0; !sdp_message_endof_media(rtsp_sdp, i) ; i++){
		mtype = sdp_message_m_media_get(rtsp_sdp, i);
		
		/* for each payload type */
		for(j=0;((number=sdp_message_m_payload_get(rtsp_sdp, i,j)) != NULL); j++){
			char *p = NULL;
			const char *rtpmap=NULL;
			char mime_type[64]= {0};
			int ptn = atoi(number);
			 
			/* get the rtpmap associated to this codec, if any */
			rtpmap = sdp_message_a_attr_value_get_with_pt(rtsp_sdp, i,ptn,"rtpmap");
			if(NULL != rtpmap) 
				strncpy(mime_type,rtpmap,sizeof(mime_type)-1);
			p = strchr(mime_type,'/');
			if(p)  *p='\0';

			if(0 == strcasecmp("video", mtype)){
				core_payload_set(co,stream_video_rtp,side_rtsp,mime_type,ptn);
				break;
			}else if(0 == strcasecmp("audio", mtype) ){
				core_payload_set(co,stream_audio_rtp,side_rtsp,mime_type,ptn);
				break;
			}
		}
	}
	return 0;
}

static int 
sip_media_process(core *co,sdp_message_t *sip_sdp)
{
	int i,j,k;
	sdp_media_t *med = NULL;
	char *mtype=NULL,*number=NULL,*c_addr=NULL;
	char video_host[HOST_BUFF_DEFAULT_LEN]={0};
	char audio_host[HOST_BUFF_DEFAULT_LEN]={0};
	sdp_connection_t *conn=NULL;
	uint16_t video_port = 0;
	uint16_t audio_port = 0;
	sdp_attribute_t *attr = NULL;
	stream_dir video_dir = stream_sendrecv;
	stream_dir audio_dir = stream_sendrecv;
				
	if(NULL == co || NULL == sip_sdp )
		return -1;
	
	c_addr = sdp_message_c_addr_get(sip_sdp, -1, 0);
	if(c_addr){
		snprintf(audio_host, sizeof(audio_host)-1,"%s", c_addr);
		snprintf(video_host, sizeof(video_host)-1,"%s", c_addr);
	}

	/* m= */
	for (i = 0; !sdp_message_endof_media(sip_sdp, i) ; i++){
		mtype = sdp_message_m_media_get(sip_sdp, i);
		conn = sdp_message_connection_get(sip_sdp, i, 0);
		med = osip_list_get(&sip_sdp->m_medias, i);
		if( NULL == med )
			continue;

		if( 0 == strcasecmp("video", mtype)){
			for(j = 0; (attr = sdp_message_attribute_get(sip_sdp,i,j))!=NULL;j++){
				if (strncmp("sendrecv",attr->a_att_field, strlen("sendrecv"))==0){
					video_dir = stream_sendrecv;
				}else if(strncmp("sendonly",attr->a_att_field, strlen("sendonly"))==0){
					video_dir = stream_sendonly;
				}else if(strncmp("recvonly",attr->a_att_field, strlen("recvonly"))==0){
					video_dir = stream_recvonly;
				}else if(strncmp("inactive",attr->a_att_field, strlen("inactive"))==0){
					video_dir = stream_inactive;
				}
			}
			
			/* rtpproxy sip video */
			video_port = atoi(med->m_port);
			if(NULL != conn && NULL != conn->c_addr)
				snprintf(video_host, sizeof(video_host)-1,"%s",conn->c_addr);
			core_remote_addr_set(co,stream_video_rtp,side_sip,video_host,0);
			core_remote_addr_set(co,stream_video_rtcp,side_sip,video_host,0);
			core_remote_addr_set(co,stream_video_rtp,side_sip,NULL,video_port);
			core_remote_addr_set(co,stream_video_rtcp,side_sip,NULL,video_port+1);
		}else if(0 == strcasecmp("audio", mtype)){
			for(j = 0;(attr = sdp_message_attribute_get(sip_sdp,i,j))!=NULL;j++){
				if(strncmp("sendrecv",attr->a_att_field, strlen("sendrecv"))==0){
					audio_dir = stream_sendrecv;
				}else if(strncmp("sendonly",attr->a_att_field, strlen("sendonly"))==0){
					audio_dir = stream_sendonly;
				}else if(strncmp("recvonly",attr->a_att_field, strlen("recvonly"))==0){
					audio_dir = stream_recvonly;
				}else if(strncmp("inactive",attr->a_att_field, strlen("inactive"))==0){
					audio_dir = stream_inactive;
				}
			}
			/* rtpproxy sip audio */
			audio_port =  atoi(med->m_port);
			if(NULL != conn && NULL != conn->c_addr)
				snprintf(audio_host, sizeof(audio_host)-1,"%s",conn->c_addr);	
			core_remote_addr_set(co,stream_audio_rtp,side_sip,audio_host,0);
			core_remote_addr_set(co,stream_audio_rtcp,side_sip,audio_host,0);
			core_remote_addr_set(co,stream_audio_rtp,side_sip,NULL,audio_port);
			core_remote_addr_set(co,stream_audio_rtcp,side_sip,NULL,audio_port+1);
		}
		
		/* for each payload type */
		for(k=0;((number=sdp_message_m_payload_get(sip_sdp, i,k)) != NULL); k++){
			char *p = NULL;
			const char *rtpmap=NULL;
			char sip_mime_type[64]= {0};
			char rtsp_mime_type[64]= {0};
			int ptn = atoi(number);
	
			/* rtpmap */
			rtpmap = sdp_message_a_attr_value_get_with_pt(sip_sdp, i,ptn,"rtpmap");
			if(NULL != rtpmap) 
				strncpy(sip_mime_type,rtpmap,sizeof(sip_mime_type)-1);
			p = strchr(sip_mime_type,'/');
			if(p)  *p='\0';
			if( 0 == strcasecmp("video", mtype)){
				core_payload_get(co,stream_video_rtp,side_rtsp,rtsp_mime_type,sizeof(rtsp_mime_type),NULL);
				if(0==strcasecmp(sip_mime_type,rtsp_mime_type)){
					core_payload_set(co,stream_video_rtp,side_sip,sip_mime_type,ptn);
					break;
				}
			}else if(0==strcasecmp("audio", mtype)){
				core_payload_get(co,stream_audio_rtp,side_rtsp,rtsp_mime_type,sizeof(rtsp_mime_type),NULL);
				if(0==strcasecmp(sip_mime_type,rtsp_mime_type)){
					core_payload_set(co,stream_audio_rtp,side_sip,sip_mime_type,ptn);
					break;
				}
			}
		}
	}

	/* if  PCMU/PCMA  no rtpmap */
	{
		char sip_mime_type[64]= {0};
		char rtsp_mime_type[64]= {0};
		int ptn = -1;
		core_payload_get(co,stream_audio_rtp,side_sip,sip_mime_type,sizeof(sip_mime_type), &ptn);
		if( sip_mime_type[0] == '\0' || ptn < 0){
			core_payload_get(co,stream_audio_rtp,side_rtsp,rtsp_mime_type,sizeof(rtsp_mime_type),NULL);
			if( 0 == strcasecmp("PCMU", rtsp_mime_type)){
				core_payload_set(co,stream_audio_rtp,side_sip,rtsp_mime_type, 0);
			}else if(0 == strcasecmp("PCMA", rtsp_mime_type)){
				core_payload_set(co,stream_audio_rtp,side_sip,rtsp_mime_type,8);
			}
		}
	}
	
	if(0 == strncmp(video_host,"0.0.0.0", strlen("0.0.0.0")) 
		|| stream_inactive == video_dir || stream_sendonly == video_dir ){
		rtsp_videodir_set(stream_inactive);
	}else{
		rtsp_videodir_set(stream_sendrecv);
	}
	if(0 == strncmp(audio_host,"0.0.0.0", strlen("0.0.0.0")) 
		|| stream_inactive == audio_dir || stream_sendonly == audio_dir ){
		rtsp_audiodir_set(stream_inactive);
	}else{
		rtsp_audiodir_set(stream_sendrecv);
	}
	
	return 0;
}

static int 
rtpproxy_media_process(core *co,sdp_message_t *sip_sdp,sdp_message_t *rtsp_sdp)
{
	int rtpproxy = 1;
	
	if(NULL == co )
		return 0;
	
	rtpproxy = core_rtpproxy_get(co);
	if( !rtpproxy )
		return 0;
	
	/* 1. rtsp */
	rtsp_media_process(co, rtsp_sdp);
	
	/* 2. sip */
	sip_media_process(co, sip_sdp);

	return 0;
}

static int 
sip_sdp_mediainfo_get(core *co, sdp_message_t *sip_sdp, 
	OUT char *video_host, IN int video_host_len, OUT int *video_port, 
	OUT char *audio_host, IN int audio_host_len, OUT int *audio_port)
{
	int rtpproxy = 1;
	if( NULL == co )
		return -1;
	rtpproxy = core_rtpproxy_get(co);
	if(rtpproxy){ /* rtpproxy */
		payload_init(co);
			
		snprintf(audio_host, audio_host_len,"%s", co->sip_localip);
		snprintf(video_host, video_host_len,"%s", co->sip_localip);
		core_local_addr_get(co,stream_audio_rtp,side_rtsp,NULL,0,audio_port);
		core_local_addr_get(co,stream_video_rtp,side_rtsp,NULL,0,video_port);
	}else{ /* no rtpproxy */
		int i,j;
		sdp_connection_t *conn=NULL;
		sdp_media_t *med = NULL;
		char *mtype=NULL,*c_addr=NULL;
		sdp_attribute_t *attr = NULL;
		stream_dir video_dir = stream_sendrecv;
		stream_dir audio_dir = stream_sendrecv;
		if( NULL == sip_sdp )
			return -1;
		
		c_addr = sdp_message_c_addr_get(sip_sdp, -1, 0);
		if(c_addr)	{
			snprintf(audio_host, audio_host_len,"%s", c_addr);
			snprintf(video_host, video_host_len,"%s", c_addr);
		}
		for(i=0; !sdp_message_endof_media (sip_sdp, i) ; i++){
			mtype = sdp_message_m_media_get(sip_sdp, i);
			conn = sdp_message_connection_get(sip_sdp, i, 0);
			med = osip_list_get(&sip_sdp->m_medias, i);
			if(med != NULL){
				if(strncasecmp("audio", mtype,strlen("audio")) == 0){
					*audio_port =  atoi(med->m_port);
					if(NULL != conn && NULL != conn->c_addr)
						snprintf(audio_host, audio_host_len,"%s",conn->c_addr);	
					for(j = 0;(attr = sdp_message_attribute_get(sip_sdp,i,j))!=NULL;j++){
						if(strncmp("sendrecv",attr->a_att_field,strlen("sendrecv"))==0){
							audio_dir = stream_sendrecv;
						}else if(strncmp("sendonly",attr->a_att_field,strlen("sendonly"))==0){
							audio_dir = stream_sendonly;
						}else if(strncmp("recvonly",attr->a_att_field,strlen("recvonly"))==0){
							audio_dir = stream_recvonly;
						}else if(strncmp("inactive",attr->a_att_field,strlen("inactive"))==0){
							audio_dir = stream_inactive;
						}
					}
				}else if ( strncasecmp("video", mtype,strlen("video")) == 0)	{
					*video_port = atoi(med->m_port);
					if(NULL != conn && NULL != conn->c_addr)
						snprintf(video_host, video_host_len,"%s",conn->c_addr);	
					for(j = 0;(attr=sdp_message_attribute_get(sip_sdp,i,j))!=NULL;j++){
						if (strncmp("sendrecv",attr->a_att_field,strlen("sendrecv"))==0){
							video_dir = stream_sendrecv;
						}else if(strncmp("sendonly",attr->a_att_field,strlen("sendonly"))==0){
							video_dir = stream_sendonly;
						}else if(strncmp("recvonly",attr->a_att_field,strlen("recvonly"))==0){
							video_dir = stream_recvonly;
						}else if(strncmp("inactive",attr->a_att_field,strlen("inactive"))==0){
							video_dir = stream_inactive;
						}
					}
				}	
			}
		}

		if(0 == strncmp(video_host,"0.0.0.0", strlen("0.0.0.0")) 
			|| stream_inactive == video_dir || stream_sendonly == video_dir ){
			rtsp_videodir_set(stream_inactive);
		}else{
			rtsp_videodir_set(stream_sendrecv);
		}
		if(0 == strncmp(audio_host,"0.0.0.0", strlen("0.0.0.0")) 
			|| stream_inactive == audio_dir || stream_sendonly == audio_dir ){
			rtsp_audiodir_set(stream_inactive);
		}else{
			rtsp_audiodir_set(stream_sendrecv);
		}
	}
	return 0;
}

static int 
sip_sdp_answer(IN core *co, INOUT sdp_message_t *rtsp_sdp,
	IN rtsp_transport_parse_t *video_transport,IN rtsp_transport_parse_t *audio_transport)
{
	int i;
	sdp_connection_t *conn=NULL;
	sdp_media_t *med = NULL;
	char *mtype=NULL;
	char tohost[HOST_BUFF_DEFAULT_LEN]={0};
	int audio_index = -1;
	int video_index = -1;
	sdp_media_t *audio_media = NULL;
	sdp_media_t *video_media = NULL;
	int rtpproxy = 1;
	char *o_addr = NULL;

	if(NULL == co || NULL == rtsp_sdp)
		return -1;
	rtpproxy = core_rtpproxy_get(co);
	
	rtsp_url_split(NULL,0,NULL,0,tohost,sizeof(tohost),NULL,NULL,0,co->rtsp_url);

	if(rtpproxy){
		/* o= */	
		o_addr = sdp_message_o_addr_get(rtsp_sdp);
		if(o_addr) {
			log(co,LOG_DEBUG,"o=%s->%s\n",rtsp_sdp->o_addr,co->sip_localip);
			osip_free(rtsp_sdp->o_addr);
		}
		rtsp_sdp->o_addr = osip_strdup(co->sip_localip);

		/* c= */
		conn = sdp_message_connection_get(rtsp_sdp, -1, 0);
		if(conn) {
			log(co,LOG_DEBUG,"c=%s->%s\n",conn->c_addr,co->sip_localip);
			osip_free(conn->c_addr);
			conn->c_addr = osip_strdup(co->sip_localip);
		}else{
			log(co,LOG_DEBUG,"add c=%s\n",co->sip_localip);
			sdp_message_c_connection_add(rtsp_sdp,-1,osip_strdup("IN"),osip_strdup("IP4"),
				osip_strdup(co->sip_localip), NULL, NULL);
		}
	}
	
	/* m= */
	for(i=0; !sdp_message_endof_media(rtsp_sdp, i) ; i++){
		mtype = sdp_message_m_media_get(rtsp_sdp, i);
		conn = sdp_message_connection_get(rtsp_sdp, i, 0);
		med = osip_list_get(&rtsp_sdp->m_medias, i);
		if (med != NULL){
			char port_str[16]={0};
			int port = 0;
			if(strncasecmp("video", mtype, strlen("video")) == 0){
				video_index = i;
				video_media = med;
				if(rtpproxy){ /* rtpproxy */
					osip_free(med->m_port);
					core_local_addr_get(co,stream_video_rtp,side_sip,NULL,0,&port);
					snprintf(port_str, sizeof (port_str)-1, "%i", port);	
					med->m_port= osip_strdup(port_str); 
					sdp_message_media_pt_process(co,rtsp_sdp,i);

					if(video_transport->source[0] == '\0'){
						core_remote_addr_set(co,stream_video_rtp,side_rtsp,tohost,0);
						core_remote_addr_set(co,stream_video_rtcp,side_rtsp,tohost,0);
					}else{
						core_remote_addr_set(co,stream_video_rtp,side_rtsp,video_transport->source,0);
						core_remote_addr_set(co,stream_video_rtcp,side_rtsp,video_transport->source,0);
					}
					core_remote_addr_set(co,stream_video_rtp,side_rtsp,NULL,video_transport->server_port);
					core_remote_addr_set(co,stream_video_rtcp,side_rtsp,NULL,video_transport->server_port+1);
						
					if(NULL != conn){
						osip_free(conn->c_addr);
						conn->c_addr = osip_strdup(co->sip_localip);
					}else{
						sdp_message_c_connection_add(rtsp_sdp,i,osip_strdup("IN"),osip_strdup("IP4"),
								osip_strdup(co->sip_localip),NULL,NULL);
					}
				}else { /* no rtpproxy */
					port = atoi(med->m_port);
					if(0 == port)	{	
	  					osip_free(med->m_port);
						snprintf (port_str, sizeof (port_str)-1, "%i", video_transport->server_port);	
						med->m_port = osip_strdup(port_str);  
					}
					if(NULL != conn){
						osip_free(conn->c_addr);
						/* live555rtspproxy RTSP_ALLOW_CLIENT_DESTINATION_SETTING */
						if(video_transport->source[0] != '\0' ) {
							conn->c_addr = osip_strdup(video_transport->source);
						}else{ /* camera ? */
							conn->c_addr = osip_strdup(tohost);
						}
					}else{
						if(video_transport->source[0] != '\0' ){
							sdp_message_c_connection_add(rtsp_sdp,i,osip_strdup("IN"),osip_strdup("IP4"),
								osip_strdup(video_transport->source),NULL,NULL);
						}else{
							sdp_message_c_connection_add(rtsp_sdp,i,osip_strdup("IN"),osip_strdup("IP4"),
								osip_strdup(tohost),NULL,NULL);
						}
					}	
				}
			}
			if(strncasecmp("audio", mtype, strlen("audio")) == 0 ){
				audio_index = i;
				audio_media = med;
				if(rtpproxy){ /* rtpproxy */
					osip_free(med->m_port);
					core_local_addr_get(co,stream_audio_rtp,side_sip,NULL,0,&port);
					snprintf (port_str, sizeof (port_str)-1, "%i", port);	
					med->m_port= osip_strdup(port_str);  
					sdp_message_media_pt_process(co,rtsp_sdp,i);
					if(audio_transport->source[0] == '\0' ){
						core_remote_addr_set(co,stream_audio_rtp,side_rtsp,tohost,0);
						core_remote_addr_set(co,stream_audio_rtcp,side_rtsp,tohost,0);
					}else{
						core_remote_addr_set(co,stream_audio_rtp,side_rtsp,audio_transport->source,0);
						core_remote_addr_set(co,stream_audio_rtcp,side_rtsp,audio_transport->source,0);
					}
					core_remote_addr_set(co,stream_audio_rtp,side_rtsp,NULL,audio_transport->server_port);
					core_remote_addr_set(co,stream_audio_rtcp,side_rtsp,NULL,audio_transport->server_port+1);

					if(NULL != conn){
						osip_free(conn->c_addr);
						conn->c_addr = osip_strdup(co->sip_localip);
					}else{
						sdp_message_c_connection_add(rtsp_sdp,i,osip_strdup("IN"),osip_strdup("IP4"),
								osip_strdup(co->sip_localip),NULL,NULL);
					}
				}else{ /* no rtpproxy */
					port = atoi(med->m_port);
					if(0 == port){
	  					osip_free(med->m_port);
						snprintf(port_str, sizeof (port_str)-1, "%i", audio_transport->server_port);		
						med->m_port= osip_strdup(port_str);  
					}
					if(NULL != conn)	{
						osip_free(conn->c_addr);
						/* live555rtspproxy RTSP_ALLOW_CLIENT_DESTINATION_SETTING */
						if(audio_transport->source[0] != '\0' ) {
							conn->c_addr = osip_strdup(audio_transport->source);
						}else{ /* camera ? */
							conn->c_addr = osip_strdup(tohost);
						}
					}else{
						if(audio_transport->source[0] != '\0' ){
							sdp_message_c_connection_add(rtsp_sdp,i,osip_strdup("IN"),osip_strdup("IP4"),
								osip_strdup(audio_transport->source),NULL,NULL);
						}else{
							sdp_message_c_connection_add(rtsp_sdp,i,osip_strdup("IN"),osip_strdup("IP4"),
								osip_strdup(tohost),NULL,NULL);
						}
					}
				}
			}	
		}
	}

	/* sip only audio or video */
	if(rtpproxy){ /* rtpproxy */
		char video_host[HOST_BUFF_DEFAULT_LEN]={0};
		char audio_host[HOST_BUFF_DEFAULT_LEN]={0};
		int video_port = -1;
		int audio_port = -1;
		core_remote_addr_get(co,stream_video_rtp,side_sip,video_host,sizeof(video_host),&video_port);
		core_remote_addr_get(co,stream_audio_rtp,side_sip,audio_host,sizeof(audio_host),&audio_port);
		if(video_port <= 0 ){/* sip no video */
			if(video_index >= 0){
				osip_list_remove(&rtsp_sdp->m_medias,video_index);
				log(co,LOG_INFO,"sip no video,remove rtsp video=%d\n",video_index);
				video_index = -1;
			}
		}
		if(audio_port <= 0){/* sip no audio */
			if(audio_index >= 0){
				osip_list_remove(&rtsp_sdp->m_medias,audio_index);
				log(co,LOG_INFO,"sip no audio,remove rtsp audio=%d\n",audio_index);	
				audio_index = -1;
			}
		}
		
		/* audio before video */
		if(video_index >= 0 && audio_index >=0 && video_index < audio_index ){
			log(co,LOG_INFO,"video=%d(%p) before audio=%d(%p),reverse!\n",
				video_index,video_media,audio_index,audio_media);	
			osip_list_remove(&rtsp_sdp->m_medias,audio_index);
			osip_list_add(&rtsp_sdp->m_medias,audio_media, 0);
		}
	}
	
	return 0;
}

static int 
sip_uas_process_invite(core *co,struct eXosip_t *context,eXosip_event_t *je)
{
	int ret = 0;
	int status = 603;
	osip_message_t *answer = NULL;
	sdp_message_t  *sip_sdp = NULL;
	sdp_message_t  *rtsp_sdp = NULL;
	int video_port = 0;
	int audio_port = 0;
	char video_host[HOST_BUFF_DEFAULT_LEN]={0};
	char audio_host[HOST_BUFF_DEFAULT_LEN]={0};
	char rtsp_sdp_buff[RECV_BUFF_DEFAULT_LEN]={0};
	char *sdp_offer=NULL;
	char *sdp_answer=NULL;
	rtsp_transport_parse_t video_transport;
	rtsp_transport_parse_t audio_transport;
	
	memset(&video_transport,0,sizeof(video_transport));
	memset(&audio_transport,0,sizeof(audio_transport));
	
	/* sip request */ 
	eXosip_lock(context);
	sip_sdp = eXosip_get_remote_sdp(context,je->did);
	eXosip_unlock(context);
	if( NULL == sip_sdp) {
		goto go_out;
	}
	
	sdp_message_to_str(sip_sdp,&sdp_offer);	
	log(co,LOG_NOTICE, "-->sip invite\n%s\n",sdp_offer);
	
	sip_sdp_mediainfo_get(co,sip_sdp,video_host,sizeof(video_host)-1,&video_port,
		audio_host,sizeof(audio_host)-1,&audio_port);

	/* rtsp request & response */
	ret = rtsp_open(co,je->cid,video_host,video_port,audio_host,audio_port, 
		&video_transport, &audio_transport,rtsp_sdp_buff,sizeof(rtsp_sdp_buff),&status);
	if(0 != ret ){
		goto go_out;
	}

	/* sip response */
	sdp_message_init(&rtsp_sdp);
	if( NULL == rtsp_sdp )  {
		goto go_out;
	}
	ret = sdp_message_parse(rtsp_sdp,rtsp_sdp_buff);
	if(0 != ret ){
		log(co,LOG_ERR, "rtsp_sdp parse ret=%d\n",ret);
		goto go_out;
	}

	/* set  ip/port/payload */
	rtpproxy_media_process(co, sip_sdp,rtsp_sdp);

	/* replace ip/port/payload */
	sip_sdp_answer(co,rtsp_sdp,&video_transport,&audio_transport);

	sdp_message_to_str(rtsp_sdp,&sdp_answer);
	status = 200;
go_out:
	sdp_message_free(rtsp_sdp);
	sdp_message_free(sip_sdp);
	answer = NULL;
	eXosip_lock(context);
	eXosip_call_build_answer(context,je->tid,status,&answer);
	if (answer){
		if( NULL != sdp_answer){
			osip_message_set_body(answer, sdp_answer, strlen(sdp_answer));
			osip_message_set_content_type(answer, "application/sdp");
		}
		eXosip_call_send_answer(context,je->tid,status,answer);
		log(co,LOG_NOTICE,"<--sip response %d\n%s\n",status,sdp_answer==NULL?"":sdp_answer);
		
	}
	eXosip_unlock(context);
	osip_free(sdp_answer);
	osip_free(sdp_offer);
	return status == 200 ? 0 : -1;
}

static int 
sip_uas_process_terminated(core *co,struct eXosip_t *context,eXosip_event_t *je)
{
	int current_cid = -1;
	rtsp_currentcall_get(&current_cid);
	if( current_cid == je->cid )
		rtsp_stop(co);
	return 0;
}

static int 
sip_uas_process_other(core *co,struct eXosip_t *context,eXosip_event_t *je)
{
	osip_message_t *answer=NULL;
	eXosip_lock(context);
	eXosip_call_build_answer(context,je->tid,200,&answer);
	if (answer)
		eXosip_call_send_answer(context,je->tid,200,answer);
	eXosip_unlock(context);
	return 0;
}

static int 
sip_uas_register(core *co,struct eXosip_t *context)
{
	int regid = -1;
	osip_message_t *reg = NULL;
	int ret = 0;
	
	regid = eXosip_register_build_initial_register(excontext,co->fromuser,
				co->proxy,co->contact,co->expiry,&reg);
	if (regid < 1) {
		log(co,LOG_ERR, "sip_register_build_initial_register failed %d\n",regid);
		return -1;
	}
	sip_add_outboundproxy(reg,co->outboundproxy);
	
	ret = eXosip_register_send_register(excontext,regid,reg);
	if (ret != 0) {
		log(co,LOG_ERR, "sip_register_send_register failed %d\n",regid);
		return -1;
	}
	return 0;
}

int 
sip_init(core *co)
{
	int ret = OSIP_SUCCESS;
	int try_maxnum = 3;
	
	excontext = eXosip_malloc();
	
try_bind:	
	if (eXosip_init(excontext)) {
		log(co,LOG_ERR, "sip_init failed\n");
		return -1;
	}
	ret = eXosip_listen_addr(excontext,IPPROTO_UDP,co->sip_localip,co->sip_localport,AF_INET,0);
	if ( OSIP_SUCCESS != ret ) {
		log(co,LOG_INFO, "sip_listen_addr %s:%d failed(%d)\n",co->sip_localip,co->sip_localport,ret);
		co->sip_localport += 1;
		try_maxnum -= 1;
		if(try_maxnum <= 0) {
			return -1;
		}else{
			osip_usleep (10000);
			eXosip_quit(excontext);
			goto try_bind;
		}
	}
	
	log(co,LOG_INFO,"sip_listen_addr %s:%d ok\n",co->sip_localip,co->sip_localport);
	 
	if(co->sip_localip) {
		eXosip_masquerade_contact(excontext,co->sip_localip,co->sip_localport);

		if( NULL != co->fromuser) {
			char contactbuf[HOST_BUFF_DEFAULT_LEN]={0};
			osip_from_t *from=NULL;
			osip_contact_t *contact=NULL;
			
			ret = osip_contact_init(&contact);
			if( 0 == ret){
				ret = osip_contact_parse(contact,co->contact);
				if( 0 != ret){
					ret = osip_from_init(&from);
					if(0 == ret){
						ret = osip_from_parse(from,co->fromuser);
						if(0 == ret) {
							snprintf(contactbuf,sizeof(contactbuf)-1,"<sip:%s@%s:%d>",
								from->url->username, co->sip_localip,co->sip_localport);
							co->contact = osip_strdup(contactbuf);
							log(co,LOG_INFO,"build contact %s\n",co->contact);
						}
						osip_from_free(from);
					}
				}
				osip_from_free(contact);
			}
		}
	}
	
	if(co->firewallip) {
		log(co,LOG_DEBUG, "firewall %s:%i\n",co->firewallip,co->sip_localport);
		eXosip_masquerade_contact(excontext,co->firewallip,co->sip_localport);
	}

	eXosip_set_user_agent(excontext, UA_STRING);

	if(co->authusername && co->authpassword) {
		if(eXosip_add_authentication_info(excontext,co->authusername,
			co->authusername,co->authpassword,NULL,NULL)) {
			log(co,LOG_INFO,"sip_add_authentication_info failed\n");
			return -1;
		}
	}
	
	eXosip_lock(excontext);
	sip_uas_register(co,excontext);
	eXosip_unlock(excontext);	

	return 0;

}

int 
sip_uas_loop(core *co)
{
	int ret;
	eXosip_event_t *je = NULL;
	
	for(;;) {
		if(!(je = eXosip_event_wait(excontext,0,0))) {
			
			eXosip_lock(excontext);
			eXosip_automatic_refresh(excontext); /* auto send register */
			eXosip_unlock(excontext);	
			if( 1 == co->rtpproxy)
				streams_loop(co);
			else 
				osip_usleep(10000);
			rtsp_automatic_action(co);	
			continue;
		}
		log(co,LOG_INFO,"sip(%d-%d-%d-%d) %d(%s)\n",
			je->cid,je->did,je->tid,je->rid,je->type,je->textinfo);
			
		eXosip_lock(excontext);
		eXosip_automatic_action(excontext);
		eXosip_unlock(excontext);
		
		switch(je->type) {
		case EXOSIP_REGISTRATION_SUCCESS:
			break;
		case EXOSIP_REGISTRATION_FAILURE:
			break;
		case EXOSIP_CALL_ACK:
			break;
		case EXOSIP_CALL_CLOSED:
		case EXOSIP_CALL_CANCELLED:
		case EXOSIP_CALL_RELEASED:	
			sip_uas_process_terminated(co,excontext,je);
			break;
		case EXOSIP_CALL_INVITE:
			ret = sip_uas_process_acl(co,excontext,je);
			if( 0 == ret ){
				rtsp_stop(co);
				ret = sip_uas_process_invite(co,excontext,je);
				if( 0 == ret ){
					rtsp_play(co);
					core_show(co);
				}
			}
			break;
		case EXOSIP_CALL_REINVITE:	
			rtsp_stop(co);
			ret = sip_uas_process_invite(co,excontext,je);
			if( 0 == ret ){
				rtsp_play(co);
				core_show(co);
			}	
			break;
		case EXOSIP_CALL_MESSAGE_NEW:
		case EXOSIP_MESSAGE_NEW:
		case EXOSIP_IN_SUBSCRIPTION_NEW:
		case EXOSIP_SUBSCRIPTION_NOTIFY:
			sip_uas_process_other(co,excontext,je);
			break;
		default:
			log(co,LOG_DEBUG, "recieved unknown sip event\n");
			break;

		}
		
		eXosip_event_free(je);
	}

	eXosip_quit(excontext);
	
	return 0;
}

