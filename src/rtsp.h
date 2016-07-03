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
* rtsp.h - API for generic RTSP client
*/
#ifndef __RTSP_CLIENT_H__
#define __RTSP_CLIENT_H__ 1

#include <string.h>     /* for string manipulation                          */
#include <stdio.h>
#include <stdlib.h> 
#include <time.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/tcp.h>
#include <linux/ip.h>
#include <linux/udp.h>
#include <netdb.h>
#include <errno.h>

#include <ctype.h>
#include "sdp.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif



	/*
	* rtsp_command_t.  Structure that contains information required by
	* RTSP for headers when sending commands.  For most commands, 1 or
	* more fields are required to be set.  See prototypes for individual
	* commands for which fields are required.
	*/

	typedef struct rtsp_command_t {
		char *accept;
		char *accept_encoding;
		char *accept_language;
		char *authorization;
		uint32_t bandwidth;
		uint32_t blocksize;
		char *cachecontrol;
		char *conference;
		char *connection;
		char *from;
		char *proxyauth;
		char *proxyrequire;
		char *range;
		char *referer;
		double scale;
		char *session;
		double speed;
		char *transport;
		char *useragent;
		char *User;
	} rtsp_command_t;

	/*
	* rtsp_decode_t.  Structure containing information about the response
	* from a RTSP command.  Some information will be required by application.
	* User can move string memory from this structure, but must free memory
	* after using it.  Make sure to set field to NULL for memory moved from
	* this structure.
	* User must call free_decode_response when completed
	*/
	typedef struct rtsp_decode_t {
		uint32_t content_length;
		uint32_t cseq;
		int close_connection;
		char retcode[4];         /* 3 byte return code - \0 delimited */
		char *retresp;
		char *body;              /* Contains body returned */
		char *accept;
		char *accept_encoding;
		char *accept_language;
		char *allow_public;
		char *authorization;
		char *bandwidth;
		char *blocksize;
		char *cache_control;
		char *content_base;
		char *content_encoding;
		char *content_language;
		char *content_location;
		char *content_type;
		char *cookie;
		char *date;
		char *expires;
		char *from;
		char *if_modified_since;
		char *last_modified;
		char *location;
		char *proxy_authenticate;
		char *proxy_require;
		char *range;
		char *referer;
		char *require;
		char *retry_after;
		char *rtp_info;
		char *scale;
		char *server;
		char *session;
		char *speed;
		char *transport;
		char *unsupported;
		char *user_agent;
		char *via;
		char *www_authenticate;
		int session_timeout;
	} rtsp_decode_t;

	/*
	* rtsp_client_t - handle for client session.  Will remain same through
	* redirects.
	*/
	typedef struct rtsp_client_ rtsp_client_t;

	/*
	* rtsp_session_t - handle for sessions created with a client.  A session
	* is defined as an stream - audio, video or other.
	*/
	typedef struct rtsp_session_ rtsp_session_t;

	struct rtp_packet;

	typedef void (*rtp_callback_f)(void *,
		unsigned char interleaved,
	struct rtp_packet *,
		int len);
	/*
	* free_decode_response - call this after the decode response has been
	* used.  It will free all memory under it.
	*/
	void free_decode_response(rtsp_decode_t *decode);

	/*
	* free_rtsp_client - call this after session has been closed to free
	* all memory allocated by the client
	*/
	void free_rtsp_client(rtsp_client_t *client);

	/*
	* rtsp_create_client - create RTSP client.
	* Input - url - url to connect to.
	*         err - pointer to error value (only when return value is NULL)
	*               values should be errno values
	* Output - pointer to rtsp_client handle
	*/
	rtsp_client_t *rtsp_create_client(core *co, const char *url, int *err);
	rtsp_client_t *rtsp_create_client_for_rtp_tcp(const char *url, int *err);
	/*
	* rtsp message function error messages
	*/
#define RTSP_RESPONSE_RECV_ERROR -1
#define RTSP_RESPONSE_BAD -2
#define RTSP_RESPONSE_MISSING_OR_BAD_PARAM -3
#define RTSP_RESPONSE_BAD_URL -4
#define RTSP_RESPONSE_CLOSED_SOCKET -5
#define RTSP_RESPONSE_REDIRECT  1
#define RTSP_RESPONSE_GOOD 0
#define RTSP_RESPONSE_MALFORM_HEADER -6
	/*
	* rtsp_send_describe - send describe message
	* Input - client - handle for session
	*         cmd - pointer to command structure
	*         resp - pointer to pointer for response structure - free after using
	* Output - error message above
	*/
	int rtsp_send_describe(rtsp_client_t *client,
		rtsp_command_t *cmd,
		rtsp_decode_t **resp);

	/*
	* rtsp_send_setup - send set up message.
	*   requires cmd->transport to be set up.
	* Inputs - client - handle for session
	*          url - "sub"-url for this stream.  Should be related to url for
	*                session
	*          cmd - pointer to command structure
	*          session_result - pointer to session handle
	*          decode_result - pointer to pointer for decode result - free when
	*                          done
	* Outputs - see error values
	*/
	int rtsp_send_setup(rtsp_client_t *client,
		const char *url,
		rtsp_command_t *cmd,
		rtsp_session_t **session_result,
		rtsp_decode_t **decode_result,
		int is_aggregate);

	/*
	* rtsp_send_pause - send a pause message for a stream.
	* Inputs - session - handle returned by setup message
	*          cmd - pointer to command structure
	*          decode_result - p to p of decode resp structure - free when done
	* Outputs - see error result
	*/
	int rtsp_send_pause(rtsp_session_t *session,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);

	/*
	* rtsp_send_play - send play message for an set-up session
	* You most likely want to set cmd->range.
	* Inputs - session - handle returned by setup message
	*          cmd - pointer to command structure
	*          decode_result - p to p of decode resp structure - free when done
	* Outputs - see error result
	*/
	int rtsp_send_play(rtsp_session_t *session,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);
	/*
	* rtsp_send_aggregate_play - send play message for an set-up session
	* You most likely want to set cmd->range.
	* Inputs - client - client pointer.
	*          aggregate_url - pointer to aggregate url.
	*          cmd - pointer to command structure
	*          decode_result - p to p of decode resp structure - free when done
	* Outputs - see error result
	*/
	int rtsp_send_aggregate_play(rtsp_client_t *client,
		const char *aggregate_url,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);


	/*
	* rtsp_send_aggregate_pause - send play message for an set-up session
	* You most likely want to set cmd->range.
	* Inputs - client - client pointer.
	*          aggregate_url - pointer to aggregate url.
	*          cmd - pointer to command structure
	*          decode_result - p to p of decode resp structure - free when done
	* Outputs - see error result
	*/
	int rtsp_send_aggregate_pause(rtsp_client_t *client,
		const char *aggregate_url,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);


	/*
	* rtsp_send_teardown - send a teardown for a particular session
	* You most likely want to set cmd->range.
	* Inputs - session - handle returned by setup message
	*          cmd - pointer to command structure
	*          decode_result - p to p of decode resp structure - free when done
	* Outputs - see error result
	*/
	int rtsp_send_teardown(rtsp_session_t *session,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);

	int rtsp_send_aggregate_teardown (rtsp_client_t *client,
		const char *url,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);


	int rtsp_is_url_my_stream(rtsp_session_t *session, const char *url,
		const char *content_base, const char *session_name);

	struct in_addr get_server_ip_address(rtsp_session_t *session);

	int rtsp_send_set_parameter (rtsp_client_t *client,
		const char *aggregate_url,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);

	int rtsp_send_get_parameter (rtsp_client_t *client,
		const char *aggregate_url,
		rtsp_command_t *cmd,
		rtsp_decode_t **decode_result);
	/*
	* rtsp_set_loglevel - set debug output level.
	* Input - loglevel - levels from syslog.h
	*/

#ifdef __cplusplus
}
#endif

#endif
