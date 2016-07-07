/*
* The contents of this file are subject to the Mozilla Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/MPL/
* 
* Software distributed under the License is distributed on an "AS
* IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
* implied. See the License for the specific language governing
* rights and limitations under the License.
* 
* The Original Code is MPEG4IP.
* 
* The Initial Developer of the Original Code is Cisco Systems Inc.
* Portions created by Cisco Systems Inc. are
* Copyright (C) Cisco Systems Inc. 2001.  All Rights Reserved.
* 
* Contributor(s): 
*              Bill May        wmay@cisco.com
*/

#include "rtsp_private.h"

/*
* free_rtsp_client()
* frees all memory associated with rtsp client information
*/
void 
free_rtsp_client(rtsp_client_t *client)
{
	rtsp_session_t *p = NULL;
	
	if( NULL == client)
		return;
	
	/*if(rptr->thread != NULL) {
	rtsp_close_thread(rptr);
	} else*/ {
		rtsp_close_socket(client);
#ifdef _WINDOWS
		WSACleanup();
#endif
	}

	while(client->session_list != NULL) {
		p = client->session_list;
		client->session_list = client->session_list->next;
		free_session_info(p);
	}
		
	CHECK_AND_FREE(client->session);
	CHECK_AND_FREE(client->orig_url);
	CHECK_AND_FREE(client->url);
	CHECK_AND_FREE(client->server_name);
	CHECK_AND_FREE(client->cookie);
	free_decode_response(client->decode_response);
	client->decode_response = NULL;
	CHECK_AND_FREE(client->authorization);
	free(client);
}


rtsp_client_t *
rtsp_create_client_common(core *co,const char *url, int *perr)
{
	int err;
	rtsp_client_t *client;

	client = malloc(sizeof(rtsp_client_t));
	if(client == NULL) {
		*perr = ENOMEM;
		return (NULL);
	}
	memset(client, 0, sizeof(rtsp_client_t));
	
	client->url = NULL;
	client->orig_url = NULL;
	client->server_name = NULL;
	client->cookie = NULL;
	client->recv_timeout = 3 * 1000;  /* default timeout is 3 seconds.*/
	client->server_socket = -1;
	client->next_cseq = 1;
	client->session = NULL;
	client->m_offset_on = 0;
	client->m_buffer_len = 0;
	client->m_resp_buffer[RECV_BUFF_DEFAULT_LEN] = '\0';
	client->need_reconnect = 0;
	
	client->authorization = NULL;
	client->session_timeout = 0;
	client->co = co;
	err = rtsp_dissect_url(client, url);
	if(err != 0) {
		log(co,LOG_WARNING,"Couldn't decode url[%s] %d\n", url, err);
		*perr = err;
		free_rtsp_client(client);
		return (NULL);
	}
	return (client);
}


rtsp_client_t *
rtsp_create_client(core *co,const char *url, int *err)
{
	rtsp_client_t *client = NULL;

#ifdef _WINDOWS
	WORD wVersionRequested;
	WSADATA wsaData;
	int ret;

	wVersionRequested = MAKEWORD( 2, 0 );

	ret = WSAStartup(wVersionRequested, &wsaData );
	if( ret != 0 ) {
		/* Tell the user that we couldn't find a usable */
		/* WinSock DLL.*/
		*err = ret;
		return (NULL);
	}
#endif
	client = rtsp_create_client_common(co,url, err);
	if(client == NULL) return (NULL);
	*err = rtsp_create_socket(client);
	if(*err != 0) {
		log(co,LOG_WARNING,"Couldn't connect %s\n",url);
		free_rtsp_client(client);
		return (NULL);
	}
	return (client);
}


int 
rtsp_send_and_get(rtsp_client_t *client,
					   char *buffer,
					   uint32_t buflen)
{
	int ret;
	log(client->co,LOG_NOTICE,"rtsp send -->\n%s\n", buffer);
	ret = rtsp_send2(client, buffer, buflen);
	if(ret < 0) {
		return (RTSP_RESPONSE_RECV_ERROR);
	}
	
	ret = rtsp_get_response(client);
	return ret;
}

