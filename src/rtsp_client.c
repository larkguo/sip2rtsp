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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#if !defined(WIN32) && !defined(_WIN32_WCE)
#include <sys/time.h>
#endif

/* RTSP head files */
#include "rtsp_client.h"


/*--- Global variable---*/
static rtsp_client_t *rtsp_client = NULL;
static  const char transport_str[] =" RTP/AVP;unicast;destination=%s;client_port=%d-%d";
static  const char auth_fmt[] =	"Digest username=\"%s\", realm=%s,nonce=%s,uri=\"%s\", response=\"%s\"";
static time_t rtsp_systemtime_get(time_t * t);

int 
rtsp_open (core *co,int call_id,char *video_host, uint16_t video_port, 
	char *audio_host, uint16_t audio_port , 
	rtsp_transport_parse_t *video_transport,rtsp_transport_parse_t *audio_transport,
	char *sdp_buff, int sdp_buff_len, int *status)
{
	int ret = 0;
	rtsp_command_t cmd;
	rtsp_decode_t *decode = NULL;
	session_desc_t *sdp = NULL;
	media_desc_t *media = NULL;
	sdp_decode_info_t *sdpdecode = NULL;
	rtsp_session_t *session = NULL;
	int translated;
	char transport_buf[HEAD_BUFF_DEFAULT_LEN]={0};
	media_type media_type; 
	char auth_str[HEAD_BUFF_DEFAULT_LEN]={0};
	char *wwwauth = NULL;
	osip_www_authenticate_t *auth = NULL;
	HASHHEX response;
	
	if( NULL != rtsp_client ){
		*status = 180;
		return -1;
	}
	
	rtsp_client = rtsp_create_client(co,co->rtsp_url, &ret);
	if(NULL == rtsp_client)	{
		*status = 404;
		return -1;
	}
	
	rtsp_currentcall_set(call_id);
	osip_www_authenticate_init (&auth);

	/* describe */
	memset(&cmd, 0, sizeof(cmd));
	free_decode_response(decode);
	decode = NULL;
	ret = rtsp_send_describe(rtsp_client, &cmd, &decode);
	if( NULL == decode){
		ret = -1;
		goto go_out;
	}

	*status = atoi(decode->retcode);
	if( 401 == *status || 407 == *status ){
		if( NULL != decode->www_authenticate ) wwwauth = decode->www_authenticate ;
		else if (NULL != decode->proxy_authenticate ) wwwauth = decode->proxy_authenticate ;
		else if( NULL != decode->authorization ) wwwauth = decode->authorization ;
		if( NULL != wwwauth )	{
			CHECK_AND_FREE(rtsp_client->authorization);
			rtsp_client->authorization = strdup(wwwauth);
			CHECK_AND_FREE(cmd.authorization);
			cmd.authorization = strdup(auth_str);

			if( NULL != auth)	{
				ret = osip_www_authenticate_parse (auth, wwwauth);
				if( 0 == ret )	{
					ret=rtsp_compute_digest_response(co->rtsp_url,co->rtsp_username,
						co->rtsp_password,auth->realm, auth->nonce,"DESCRIBE",response);
					if( 0 == ret )	{
						snprintf(auth_str,sizeof(auth_str)-1,auth_fmt,co->rtsp_username,
							auth->realm,auth->nonce,co->rtsp_url, response);
						CHECK_AND_FREE(cmd.authorization);
						cmd.authorization = strdup(auth_str);
						free_decode_response(decode);
						decode = NULL;
						ret = rtsp_send_describe(rtsp_client, &cmd, &decode);
						if(NULL != decode ) *status = atoi(decode->retcode);
					}
				}
			}
		}else{
			ret = -1;
		 	goto go_out;
		}
	}

	if( NULL == decode->body)	{
		ret = -1;
		goto go_out;
	}
		
	strncpy(sdp_buff, decode->body, sdp_buff_len-1);
	sdpdecode = set_sdp_decode_from_memory(decode->body);
	if (sdpdecode == NULL)	{
		log(rtsp_client->co,LOG_DEBUG,"Couldn't get sdp decode\n");
		ret = -1;
		goto go_out;
	}

	if (sdp_decode(sdpdecode, &sdp, &translated) != 0){
		log(rtsp_client->co,LOG_DEBUG,"Couldn't decode sdp\n");
		ret = -1;
		goto go_out;
	}

	convert_relative_urls_to_absolute (sdp, co->rtsp_url);
	media = sdp->media;	
	if( NULL != media && NULL != media->media){
		if (strncasecmp(media->media, "video",strlen("video")) == 0) {
			media_type = MEDIA_VIDEO;
			snprintf(transport_buf,sizeof(transport_buf)-1,transport_str,
				video_host, video_port,video_port+1);
		}else if (strncasecmp(media->media, "audio",strlen("audio")) == 0) {
			media_type = MEDIA_AUDIO;
			snprintf(transport_buf,sizeof(transport_buf)-1,transport_str,
				audio_host, audio_port,audio_port+1);
		}
	}
	cmd.transport = transport_buf;
	if( NULL != auth ){
		ret = rtsp_compute_digest_response(co->rtsp_url,co->rtsp_username,
			co->rtsp_password,auth->realm,auth->nonce,"SETUP",response);
		if( 0 == ret ){
			snprintf(auth_str,sizeof(auth_str)-1,auth_fmt,co->rtsp_username,
				auth->realm,auth->nonce,co->rtsp_url,response);
			CHECK_AND_FREE(cmd.authorization);
			cmd.authorization = strdup(auth_str);
		}
	}

	/* setup */
	free_decode_response(decode);
	decode = NULL;
	ret = rtsp_send_setup(rtsp_client,media->control_string,&cmd,&session,&decode,0);
	if (ret != RTSP_RESPONSE_GOOD || NULL == decode){
		log(rtsp_client->co,LOG_DEBUG,"Response to setup is %d\n", ret);
		goto go_out;
	}

	rtsp_sessiontimeout_set(decode->session_timeout);
	*status = atoi(decode->retcode);
	rtsp_client->session = strdup(session->session);
	if( MEDIA_VIDEO == media_type ){
		ret = process_rtsp_transport(video_transport,decode->transport,"RTP/AVP");
	}else if(MEDIA_AUDIO ==  media_type){
		ret = process_rtsp_transport(audio_transport,decode->transport,"RTP/AVP");
	}

	media = (sdp->media)->next;
	while(media!= NULL){
		if( NULL != media->media){
			media_type = 0;
			if (strncasecmp(media->media, "video",strlen("video")) == 0) {
				media_type = MEDIA_VIDEO;
				snprintf(transport_buf,sizeof(transport_buf)-1,transport_str,
					video_host, video_port,video_port+1);
			}else if (strncasecmp(media->media, "audio",strlen("audio")) == 0) {
				media_type = MEDIA_AUDIO;
				snprintf(transport_buf,sizeof(transport_buf)-1,transport_str,
					audio_host, audio_port,audio_port+1);
			}
		}

		cmd.transport = transport_buf;
		free_decode_response(decode);
		decode = NULL;
		ret=rtsp_send_setup(rtsp_client,media->control_string,&cmd,&session,&decode,1);
		if (ret != RTSP_RESPONSE_GOOD || NULL == decode){
			log(rtsp_client->co,LOG_DEBUG,"Response to setup is %d\n", ret);
			goto go_out;
		}

		rtsp_sessiontimeout_set(decode->session_timeout);
		*status = atoi(decode->retcode);
		if( MEDIA_VIDEO == media_type )	{
			ret = process_rtsp_transport(video_transport,decode->transport, "RTP/AVP");
		}else if(MEDIA_AUDIO == media_type)	{
			ret = process_rtsp_transport(audio_transport,decode->transport, "RTP/AVP");
		}
		media = media->next;
	}

go_out:
	sdp_decode_info_free(sdpdecode);
	CHECK_AND_FREE(cmd.authorization);
	sdp_free_session_desc(sdp);
	free_decode_response(decode);
	osip_www_authenticate_free (auth);

	return ret;
}

int 
rtsp_play(core *co)
{
	rtsp_command_t cmd;
	rtsp_decode_t *decode = NULL;
	int ret;
	char auth_str[HEAD_BUFF_DEFAULT_LEN]={0};
	osip_www_authenticate_t *auth = NULL;
	HASHHEX response;
		
	if( NULL == rtsp_client|| NULL == co )
		return -1;

	/* if SIP hold/pause,then no Play */
	if(stream_sendonly==rtsp_client->audio_dir
		||stream_inactive==rtsp_client->audio_dir
		||stream_sendonly==rtsp_client->video_dir
		||stream_inactive==rtsp_client->video_dir)
		return 0;

	memset(&cmd, 0, sizeof(rtsp_command_t));
	cmd.transport = NULL;
	cmd.range = "npt=0.0-";
	
	if( NULL != rtsp_client->authorization){
		osip_www_authenticate_init (&auth);
		ret = osip_www_authenticate_parse (auth, rtsp_client->authorization);
		
		ret = rtsp_compute_digest_response(co->rtsp_url, co->rtsp_username, 
			co->rtsp_password, auth->realm, auth->nonce,"PLAY",response);
		if( 0 == ret )	{
			snprintf(auth_str,sizeof(auth_str)-1, auth_fmt, co->rtsp_username, 
				auth->realm, auth->nonce,co->rtsp_url, response);
			CHECK_AND_FREE(cmd.authorization);
			cmd.authorization = strdup(auth_str);
		}
	}
	ret = rtsp_send_aggregate_play(rtsp_client,co->rtsp_url,&cmd,&decode);  
	if (ret != RTSP_RESPONSE_GOOD)	{
		log(rtsp_client->co,LOG_DEBUG,"response to play is %d\n", ret);
	}else{
		rtsp_client->last_update = rtsp_systemtime_get(NULL);
	}

	CHECK_AND_FREE(cmd.authorization);
	osip_www_authenticate_free (auth);
	free_decode_response(decode);
	
	return 0;
}

int 
rtsp_pause(core *co)
{
	rtsp_command_t cmd;
	rtsp_decode_t *decode = NULL;
	int ret ;
	char auth_str[HEAD_BUFF_DEFAULT_LEN]={0};
	osip_www_authenticate_t *auth = NULL;
	HASHHEX response;
	
	if( NULL == rtsp_client|| NULL == co )
		return -1;
	
	memset(&cmd, 0, sizeof(rtsp_command_t));
	cmd.transport = NULL;

	if( NULL != rtsp_client->authorization){
		osip_www_authenticate_init (&auth);
		ret = osip_www_authenticate_parse (auth, rtsp_client->authorization);
		
		ret = rtsp_compute_digest_response(co->rtsp_url, co->rtsp_username, co->rtsp_password,  auth->realm, auth->nonce,"PAUSE",response);
		if( 0 == ret )	{
			snprintf(auth_str,sizeof(auth_str)-1, auth_fmt, co->rtsp_username, auth->realm, auth->nonce,co->rtsp_url, response);
			CHECK_AND_FREE(cmd.authorization);
			cmd.authorization = strdup(auth_str);
		}
	}
	ret = rtsp_send_aggregate_pause(rtsp_client,co->rtsp_url,&cmd,&decode);
	if (ret != RTSP_RESPONSE_GOOD)
		log(rtsp_client->co,LOG_DEBUG,"response to play is %d\n", ret);

	CHECK_AND_FREE(cmd.authorization);
	osip_www_authenticate_free (auth);
	free_decode_response(decode);
	return 0;

}

int 
rtsp_getparam(core *co)
{
	rtsp_command_t cmd;
	rtsp_decode_t *decode = NULL ;
	int ret;
	char auth_str[HEAD_BUFF_DEFAULT_LEN]={0};
	osip_www_authenticate_t *auth = NULL;
	HASHHEX response;
	
	if( NULL == rtsp_client || NULL == co )
		return -1;
	
	memset(&cmd, 0, sizeof(rtsp_command_t));
	cmd.transport = NULL;
	
	if( NULL != rtsp_client->authorization){
		osip_www_authenticate_init (&auth);
		ret = osip_www_authenticate_parse (auth, rtsp_client->authorization);
		
		ret = rtsp_compute_digest_response(co->rtsp_url, co->rtsp_username, 
			co->rtsp_password,  auth->realm, auth->nonce,"GETPARAM",response);
		if( 0 == ret )	{
			snprintf(auth_str,sizeof(auth_str)-1, auth_fmt, co->rtsp_username, 
				auth->realm, auth->nonce,co->rtsp_url, response);
			CHECK_AND_FREE(cmd.authorization);
			cmd.authorization = strdup(auth_str);
		}
	}
	
	ret = rtsp_send_get_parameter(rtsp_client,co->rtsp_url, &cmd, &decode);
	if (ret != RTSP_RESPONSE_GOOD)
		log(rtsp_client->co,LOG_DEBUG,"response to get_parameter is %d\n", ret);

	CHECK_AND_FREE(cmd.authorization);
	osip_www_authenticate_free (auth);
	free_decode_response(decode);
	return 0;

}

int 
rtsp_stop(core *co)
{
	rtsp_command_t cmd;
	rtsp_decode_t *decode = NULL;
	int ret;
	char auth_str[HEAD_BUFF_DEFAULT_LEN]={0};
	osip_www_authenticate_t *auth = NULL;
	HASHHEX response;
	
	if( NULL == rtsp_client|| NULL == co )
		return -1;
	
	memset(&cmd, 0, sizeof(rtsp_command_t));
	cmd.transport = NULL;
	
	if( NULL != rtsp_client->authorization){
		osip_www_authenticate_init (&auth);
		ret = osip_www_authenticate_parse (auth, rtsp_client->authorization);
		
		ret = rtsp_compute_digest_response(co->rtsp_url, co->rtsp_username,
			co->rtsp_password,  auth->realm, auth->nonce,"TEARDOWN",response);
		if( 0 == ret )	{
			snprintf(auth_str,sizeof(auth_str)-1, auth_fmt, co->rtsp_username, 
				auth->realm, auth->nonce,co->rtsp_url, response);
			CHECK_AND_FREE(cmd.authorization);
			cmd.authorization = strdup(auth_str);
		}
	}
	
	ret = rtsp_send_aggregate_teardown(rtsp_client,co->rtsp_url,&cmd,&decode);
	if (ret != RTSP_RESPONSE_GOOD)
		log(rtsp_client->co,LOG_DEBUG,"Teardown response %d\n", ret);
	
	osip_www_authenticate_free (auth);
	CHECK_AND_FREE(cmd.authorization);
	free_decode_response(decode);
	free_rtsp_client(rtsp_client);
	rtsp_client=NULL;
	
	return 0;
}

int 
rtsp_audiodir_set(stream_dir dir)
{
	if( NULL == rtsp_client )
		return -1;
	rtsp_client->audio_dir = dir;
	return 0;
}

int 
rtsp_videodir_set(stream_dir dir)
{
	if( NULL == rtsp_client )
		return -1;
	rtsp_client->video_dir = dir;
	return 0;
}

int 
rtsp_sessiontimeout_set(int timeout)
{
	if( NULL == rtsp_client || timeout <= 0)
		return -1;
	
	if( rtsp_client->session_timeout <= 0 )
		rtsp_client->session_timeout = timeout;
	
	if( rtsp_client->session_timeout > timeout )
		rtsp_client->session_timeout = timeout;

	return 0;
}

int 
rtsp_sessiontimeout_get(int *timeout)
{
	if( NULL == rtsp_client )
		return -1;
	*timeout = rtsp_client->session_timeout;
	return 0;
}

int 
rtsp_currentcall_set(int call_id)
{
	if( NULL == rtsp_client || call_id <= 0)
		return -1;
	
	if( rtsp_client->call_id <= 0 )
		rtsp_client->call_id = call_id;

	return 0;
}

int 
rtsp_currentcall_get(int *call_id)
{
	if( NULL == rtsp_client )
		return -1;
	*call_id = rtsp_client->call_id;
	return 0;
}

static time_t
rtsp_systemtime_get(time_t * t)
{
	struct timeval now_monotonic;

	gettimeofday(&now_monotonic, NULL);
	if (t != NULL) {
		*t = now_monotonic.tv_sec;
	}
	return now_monotonic.tv_sec;
}

void 
rtsp_automatic_action(core *co)
{
	time_t interval  = 0;
	int session_timeout = 0;
	 time_t now;
	  
	if( NULL == rtsp_client || NULL == co )
		return;

	rtsp_sessiontimeout_get(&session_timeout);	
	if( session_timeout <= 0)
		session_timeout =  co->session_timeout ;
	now = rtsp_systemtime_get(NULL);
	interval = now - rtsp_client->last_update;
	if( interval >  (session_timeout/2) ){
		rtsp_getparam(co);
		rtsp_client->last_update = now;
	}
}

