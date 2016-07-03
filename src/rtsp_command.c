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
* Copyright (C) Cisco Systems Inc. 2000, 2001.  All Rights Reserved.
* 
* Contributor(s): 
*              Bill May        wmay@cisco.com
*/
/*
* rtsp_command.c - process API calls to send/receive rtsp commands
*/
#include "rtsp_private.h"
#include "sdp.h"

/*
* rtsp_build_common()
* Builds a common header based on rtsp_command_t information.
*/
static int rtsp_build_common (char *buffer,
							  uint32_t maxlen,
							  uint32_t *at,
							  rtsp_client_t *client,
							  rtsp_command_t *cmd,
							  const char *session)
{
	int ret;

	/*
	* The below is ugly, but it will allow us to remove a lot of lines
	* of code.  SNPRINTF_CHECK makes sure (for this routine), that we
	* don't have more data in the buffer than allowed - it will return
	* an error code if that happens.
	*/
#define SNPRINTF_CHECK(fmt, value) \
	ret = snprintf(buffer + *at, maxlen - *at, (fmt), (value)); \
	if (ret == -1) { \
	return (-1); \
	}\
	*at += ret;

	SNPRINTF_CHECK("CSeq: %u\r\n", client->next_cseq);
	if (client->cookie) {
		SNPRINTF_CHECK("Cookie: %s\r\n", client->cookie);
	}

	if (cmd && cmd->accept) {
		SNPRINTF_CHECK("Accept: %s\r\n", cmd->accept);
	}
	if (cmd && cmd->accept_encoding) {
		SNPRINTF_CHECK("Accept-Encoding: %s\r\n", cmd->accept_encoding);
	}
	if (cmd && cmd->accept_language) {
		SNPRINTF_CHECK("Accept-Language: %s\r\n", cmd->accept_language);
	}
	if (cmd && cmd->authorization) {
		SNPRINTF_CHECK("Authorization: %s\r\n", cmd->authorization);
	}
	if (cmd && cmd->bandwidth != 0) {
		SNPRINTF_CHECK("Bandwidth: %u\r\n", cmd->bandwidth);
	}
	if (cmd && cmd->blocksize != 0) {
		SNPRINTF_CHECK("Blocksize: %u\r\n", cmd->blocksize);
	}
	if (cmd && cmd->cachecontrol) {
		SNPRINTF_CHECK("Cache-Control: %s\r\n", cmd->cachecontrol);
	}
	if (cmd && cmd->conference) {
		SNPRINTF_CHECK("Conference: %s\r\n", cmd->conference);
	}
	if (cmd && cmd->from) {
		SNPRINTF_CHECK("From: %s\r\n", cmd->from);
	}
	if (cmd && cmd->proxyauth) {
		SNPRINTF_CHECK("Proxy-Authorization: %s\r\n", cmd->proxyauth);
	}
	if (cmd && cmd->proxyrequire) {
		SNPRINTF_CHECK("Proxy-Require: %s\r\n", cmd->proxyrequire);
	}
	if (cmd && cmd->range) {
		SNPRINTF_CHECK("Range: %s\r\n", cmd->range);
	}
	if (cmd && cmd->referer) {
		SNPRINTF_CHECK("Referer: %s\r\n", cmd->referer);
	}
	if (cmd && cmd->scale != 0.0) {
		SNPRINTF_CHECK("Scale: %g\r\n", cmd->scale);
	}
	if (session) {
		SNPRINTF_CHECK("Session: %s\r\n", session);
	} else if (cmd && cmd->session) {
		SNPRINTF_CHECK("Session: %s\r\n", cmd->session);
	}
	if (cmd && cmd->speed != 0.0) {
		SNPRINTF_CHECK("Speed: %g\r\n", cmd->speed);
	}
	if (cmd && cmd->transport) {
		SNPRINTF_CHECK("Transport: %s\r\n", cmd->transport);
	}

	SNPRINTF_CHECK("User-Agent: %s\r\n",
		(cmd && cmd->useragent != NULL ?  cmd->useragent : UA_STRING));
	if (cmd && cmd->User) {
		SNPRINTF_CHECK("%s", cmd->User);
	}
#undef SNPRINTF_CHECK
	return (0);
}

/*
* rtsp_send_describe - send the describe client to a server
*/
int rtsp_send_describe (rtsp_client_t *client,
						rtsp_command_t *cmd,
						rtsp_decode_t **decode_result)
{
	char buffer[RECV_BUFF_DEFAULT_LEN] = {0};
	uint32_t maxlen, buflen;
	int ret;
	rtsp_decode_t *decode;

	*decode_result = NULL;
	client->redirect_count = 0;

	do {
		maxlen = sizeof(buffer);
		buflen = snprintf(buffer, maxlen, "DESCRIBE %s RTSP/1.0\r\n", client->url);

		if (rtsp_build_common(buffer, maxlen, &buflen, client, cmd, NULL) == -1) {
			return (RTSP_RESPONSE_RECV_ERROR);
		}

		ret = snprintf(buffer + buflen, maxlen - buflen, "\r\n");
		if (ret == -1) {
			return (RTSP_RESPONSE_RECV_ERROR);
		}
		buflen += ret;

		ret = rtsp_send_and_get(client, buffer, buflen);
		decode = client->decode_response;

		if (ret == RTSP_RESPONSE_GOOD) {
			*decode_result = client->decode_response;
			client->decode_response = NULL;
			return (RTSP_RESPONSE_GOOD);
		} else if (ret != RTSP_RESPONSE_REDIRECT) {
			if (ret != RTSP_RESPONSE_RECV_ERROR &&
				decode != NULL) {
					*decode_result = client->decode_response;
					client->decode_response = NULL;	   
			}
			return (ret);
		}
		/*
		* Handle this through the redirects
		*/
	}  while (ret == RTSP_RESPONSE_REDIRECT);

	return (RTSP_RESPONSE_RECV_ERROR);
}

/*
* rtsp_send_setup - When we get the describe, this will set up a
* particular stream.  Use the session handle for all further commands for
* the stream (play, pause, teardown).
*/
int rtsp_send_setup (rtsp_client_t *client,
					 const char *url,
					 rtsp_command_t *cmd,
					 rtsp_session_t **session_result,
					 rtsp_decode_t **decode_result,
					 int is_aggregate)
{
	char buffer[RECV_BUFF_DEFAULT_LEN] = {0}, *temp;
	uint32_t maxlen, buflen;
	int ret;
	rtsp_decode_t *decode;
	rtsp_session_t *sptr;

	*decode_result = NULL;
	*session_result = NULL;
	client->redirect_count = 0;

	if (cmd == NULL || cmd->transport == NULL) {
		return (RTSP_RESPONSE_MISSING_OR_BAD_PARAM);
	}

	if (strncmp(url, "rtsp://", strlen("rtsp://")) != 0) {
		return (RTSP_RESPONSE_BAD_URL);
	}

	temp = strchr(url + strlen("rtsp://"), '/');
	if (temp == NULL) {
		return (RTSP_RESPONSE_BAD_URL);
	}
	if (strncmp(url, client->url, temp - url) != 0) {
		s2r_log(client->co,LOG_DEBUG,"Bad url %s\n", url);
		s2r_log(client->co,LOG_DEBUG, "Should be %s\n", client->url);
		return (RTSP_RESPONSE_BAD_URL);
	}

	maxlen = sizeof(buffer);
	buflen = snprintf(buffer, maxlen, "SETUP %s RTSP/1.0\r\n", url);

	if (rtsp_build_common(buffer,
		maxlen,
		&buflen,
		client,
		cmd,
		is_aggregate ? client->session : NULL) == -1) {
			return (RTSP_RESPONSE_RECV_ERROR);
	}

	ret = snprintf(buffer + buflen, maxlen - buflen, "\r\n");
	if (ret == -1) {
		return (RTSP_RESPONSE_RECV_ERROR);
	}
	buflen += ret;

	ret = rtsp_send_and_get(client, buffer, buflen);
	decode = client->decode_response;

	if (ret == RTSP_RESPONSE_GOOD) {
		*decode_result = client->decode_response;
		client->decode_response = NULL;
#ifndef IPTV_COMPATIBLE
		if ((*decode_result)->session == NULL) {
			return (RTSP_RESPONSE_BAD);
		}
#endif
		temp=strchr((*decode_result)->session,';');
		if(temp!=NULL)
		{
			int i=0;
			char *c=NULL;

			char *session_timeout= NULL;
			session_timeout = strstr((*decode_result)->session,"timeout");
			if( NULL != session_timeout )
				sscanf(session_timeout, " timeout = %d",&((*decode_result)->session_timeout));
	
			while ((*decode_result)->session[i]!=';'&& (*decode_result)->session[i]!='\0') i++;
			c = (char *)malloc(i+1);
			strncpy(c,(*decode_result)->session,i);
			c[i]='\0';
			free((*decode_result)->session);
			(*decode_result)->session=c;
		}
		if (is_aggregate && client->session != NULL) {
			if (strcmp(client->session, (*decode_result)->session) != 0) {	   
				return (RTSP_RESPONSE_BAD);
			}
		}
		sptr = client->session_list;
		while (sptr != NULL) {
			if (strcmp(sptr->url, url) == 0)
				break;
			sptr = sptr->next;
		}
		if (sptr == NULL) {
			sptr = malloc(sizeof(rtsp_session_t));
			if (sptr == NULL) {
				return (RTSP_RESPONSE_RECV_ERROR);
			}
			sptr->url = strdup(url);
			if ((*decode_result)->session != NULL)
				sptr->session = strdup((*decode_result)->session);
			else
				sptr->session = NULL;
			sptr->parent = client;
			sptr->next = client->session_list;
			client->session_list = sptr;
			if (is_aggregate && client->session == NULL)
				client->session = sptr->session;
		}
		*session_result = sptr;
		return (RTSP_RESPONSE_GOOD);
	} else {
		if (ret != RTSP_RESPONSE_RECV_ERROR &&
			decode != NULL) {
				*decode_result = client->decode_response;
				client->decode_response = NULL;
				s2r_log(client->co,LOG_DEBUG,"Error code %s %s\n",decode->retcode,decode->retresp);
		}
		return (ret);
	}

	return (RTSP_RESPONSE_RECV_ERROR);
}

/*
* check_session - make sure that the session is correct for that command
*/
static int check_session (rtsp_session_t *session,
						  rtsp_command_t *cmd)
{
	rtsp_session_t *sptr;
	rtsp_client_t *client;

	client = session->parent;
	if (client == NULL) {
		s2r_log(client->co,LOG_DEBUG,"Session doesn't point to parent\n");
		return (FALSE);
	}

	sptr = client->session_list;
	while (sptr != session && sptr != NULL) sptr = sptr->next;
	if (sptr == NULL) {
		s2r_log(client->co,LOG_DEBUG,"session not found in client list\n");
		return (FALSE);
	}

	if ((cmd != NULL) &&
		(cmd->session != NULL) &&
		(strcmp(cmd->session, session->session) != 0)) {
			s2r_log(client->co,LOG_DEBUG, "Have cmd->session set wrong\n");
			return (FALSE);
	}
	return (TRUE);
}

static int rtsp_send_play_or_pause (const char *command,
									const char *url,
									const char *session,
									rtsp_client_t *client,
									rtsp_command_t *cmd,
									rtsp_decode_t **decode_result)
{
	char buffer[RECV_BUFF_DEFAULT_LEN] = {0};
	uint32_t maxlen, buflen;
	int ret;
	rtsp_decode_t *decode;

	*decode_result = NULL;
	if (client->server_socket < 0) {
		return (RTSP_RESPONSE_CLOSED_SOCKET);
	}

	maxlen = sizeof(buffer);
	buflen = snprintf(buffer, maxlen, "%s %s RTSP/1.0\r\n", command, url);

	if (rtsp_build_common(buffer, maxlen, &buflen,
		client, cmd, session) == -1) {
			return (RTSP_RESPONSE_RECV_ERROR);
	}

	ret = snprintf(buffer + buflen, maxlen - buflen, "\r\n");
	if (ret == -1) {
		return (RTSP_RESPONSE_RECV_ERROR);
	}
	buflen += ret;
	ret = rtsp_send_and_get(client, buffer, buflen);
	decode = client->decode_response;

	if (ret == RTSP_RESPONSE_GOOD) {
		*decode_result = client->decode_response;
		client->decode_response = NULL;

		return (RTSP_RESPONSE_GOOD);
	} else {
		s2r_log(client->co,LOG_DEBUG,"%s return code %d\n", command, ret);
		if (ret != RTSP_RESPONSE_RECV_ERROR &&
			decode != NULL) {
				*decode_result = client->decode_response;
				client->decode_response = NULL;
				s2r_log(client->co,LOG_DEBUG,"Error code %s %s\n",decode->retcode,decode->retresp);
		}
		return (ret);
	}

	return (RTSP_RESPONSE_RECV_ERROR);
}

static int rtsp_send_get_or_set_parameter (const char *command,
										   const char *url,
										   const char *session,
										   rtsp_client_t *client,
										   rtsp_command_t *cmd,
										   rtsp_decode_t **decode_result)
{
	char buffer[RECV_BUFF_DEFAULT_LEN] = {0};
	uint32_t maxlen, buflen;
	int ret;
	rtsp_decode_t *decode;

	*decode_result = NULL;
	if (client->server_socket < 0) {
		return (RTSP_RESPONSE_CLOSED_SOCKET);
	}

	maxlen = sizeof(buffer);
	buflen = snprintf(buffer, maxlen, "%s %s RTSP/1.0\r\n", command, url);

	if (rtsp_build_common(buffer, maxlen, &buflen,
		client, cmd, session) == -1) {
			return (RTSP_RESPONSE_RECV_ERROR);
	}

	ret = snprintf(buffer + buflen, maxlen - buflen, "\r\n");
	if (ret == -1) {
		return (RTSP_RESPONSE_RECV_ERROR);
	}
	buflen += ret;


	ret = rtsp_send_and_get(client, buffer, buflen);
	decode = client->decode_response;

	if (ret == RTSP_RESPONSE_GOOD) {
		*decode_result = client->decode_response;
		client->decode_response = NULL;

		return (RTSP_RESPONSE_GOOD);
	} else {
		if (ret != RTSP_RESPONSE_RECV_ERROR &&
			decode != NULL) {
				*decode_result = client->decode_response;
				client->decode_response = NULL;

		}
		return (ret);
	}

	return (RTSP_RESPONSE_RECV_ERROR);
}


/*
* rtsp_send_play - send play command.  It helps if Range is set
*/
int rtsp_send_play (rtsp_session_t *session,
					rtsp_command_t *cmd,
					rtsp_decode_t **decode_result)
{
	if (check_session(session, cmd) == FALSE) {
		return (RTSP_RESPONSE_MISSING_OR_BAD_PARAM);
	}

	return (rtsp_send_play_or_pause("PLAY",
		session->url,
		session->session,
		session->parent,
		cmd,
		decode_result));
}

/*
* rtsp_send_pause - send a pause on a particular session
*/
int rtsp_send_pause (rtsp_session_t *session,
					 rtsp_command_t *cmd,
					 rtsp_decode_t **decode_result)
{
	if (check_session(session, cmd) == FALSE) {
		return (RTSP_RESPONSE_MISSING_OR_BAD_PARAM);
	}

	return (rtsp_send_play_or_pause("PAUSE",
		session->url,
		session->session,
		session->parent,
		cmd,
		decode_result));
}

int rtsp_send_aggregate_play (rtsp_client_t *client,
							  const char *aggregate_url,
							  rtsp_command_t *cmd,
							  rtsp_decode_t **decode_result)
{
	return (rtsp_send_play_or_pause("PLAY",
		aggregate_url,
		client->session,
		client,
		cmd,
		decode_result));
}

int rtsp_send_aggregate_pause (rtsp_client_t *client,
							   const char *aggregate_url,
							   rtsp_command_t *cmd,
							   rtsp_decode_t **decode_result)
{
	return (rtsp_send_play_or_pause("PAUSE",
		aggregate_url,
		client->session,
		client,
		cmd,
		decode_result));
}

static int rtsp_send_teardown_common (rtsp_client_t *client,
									  const char *url,
									  const char *session,
									  rtsp_command_t *cmd,
									  rtsp_decode_t **decode_result)
{
	char buffer[RECV_BUFF_DEFAULT_LEN] = {0};
	uint32_t maxlen, buflen;
	int ret;
	rtsp_decode_t *decode;

	*decode_result = NULL;
	if (client->server_socket < 0) {
		return (RTSP_RESPONSE_CLOSED_SOCKET);
	}

	maxlen = sizeof(buffer);
	buflen = snprintf(buffer, maxlen, "TEARDOWN %s RTSP/1.0\r\n", url);

	if (rtsp_build_common(buffer, maxlen, &buflen,
		client, cmd, session) == -1) {
			return (RTSP_RESPONSE_RECV_ERROR);
	}

	ret = snprintf(buffer + buflen, maxlen - buflen, "\r\n");
	if (ret == -1) {
		return (RTSP_RESPONSE_RECV_ERROR);
	}
	buflen += ret;

	ret = rtsp_send_and_get(client, buffer, buflen);
	decode = client->decode_response;
	if (ret == RTSP_RESPONSE_GOOD) {

		*decode_result = client->decode_response;
		client->decode_response = NULL;
		return (RTSP_RESPONSE_GOOD);
	} else {
		s2r_log(client->co,LOG_DEBUG,"TEARDOWN return code %d\n", ret);
		if (ret != RTSP_RESPONSE_RECV_ERROR &&
			decode != NULL) {
				*decode_result = client->decode_response;
				client->decode_response = NULL;
				s2r_log(client->co,LOG_DEBUG, "Error code %s %s\n",decode->retcode,decode->retresp);
		}
		return (ret);
	}

	return (RTSP_RESPONSE_RECV_ERROR);
}
/*
* rtsp_send_teardown.  Sends a teardown for a session.  We might eventually
* want to provide a teardown for the base url, rather than one for each
* session
*/
int rtsp_send_teardown (rtsp_session_t *session,
						rtsp_command_t *cmd,
						rtsp_decode_t **decode_result)
{
	int ret;
	rtsp_client_t *client;
	rtsp_session_t *sptr;
	if (check_session(session, cmd) == FALSE) {
		return (RTSP_RESPONSE_MISSING_OR_BAD_PARAM);
	}
	client = session->parent;

	ret = rtsp_send_teardown_common(client,
		session->url,
		session->session,
		cmd,
		decode_result);
	if (ret == RTSP_RESPONSE_GOOD) {
		if (client->session_list == session) {
			client->session_list = session->next;
		} else {
			sptr = client->session_list;
			while (sptr->next != session) sptr = sptr->next;
			sptr->next = session->next;
		}
		free_session_info(session);
	}
	return (ret);
}

int rtsp_send_aggregate_teardown (rtsp_client_t *client,
								  const char *url,
								  rtsp_command_t *cmd,
								  rtsp_decode_t **decode_result)
{
	int ret;
	rtsp_session_t *p;
	ret = rtsp_send_teardown_common(client,
		url,
		client->session,
		cmd,
		decode_result);
	if (ret == RTSP_RESPONSE_GOOD) {
		while (client->session_list != NULL) {
			p = client->session_list;
			client->session_list = client->session_list->next;
			free_session_info(p);
		}
	}
	return (ret);
}

int rtsp_send_set_parameter (rtsp_client_t *client,
							 const char *aggregate_url,
							 rtsp_command_t *cmd,
							 rtsp_decode_t **decode_result)
{

	return (rtsp_send_get_or_set_parameter("SET_PARAMETER",
		aggregate_url,
		client->session,
		client,
		cmd,
		decode_result));
}


int rtsp_send_get_parameter (rtsp_client_t *client,
							 const char *aggregate_url,
							 rtsp_command_t *cmd,
							 rtsp_decode_t **decode_result)
{

	return (rtsp_send_get_or_set_parameter("GET_PARAMETER",
		aggregate_url,
		client->session,
		client,
		cmd,
		decode_result));
}

