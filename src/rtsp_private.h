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

#ifndef __RTSP_PRIVATE_H__
#define __RTSP_PRIVATE_H__

#include "rtsp.h"


#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#ifndef MIN
#define MIN(a,b) (((a)>(b)) ? (b) : (a))
#endif
#ifndef MAX
#define MAX(a,b) (((a)>(b)) ? (a) : (b))
#endif

#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef INOUT
#define INOUT
#endif

#define HOST_BUFF_DEFAULT_LEN 128
#define HEAD_BUFF_DEFAULT_LEN 512
#define RECV_BUFF_DEFAULT_LEN 2048


typedef enum{
	MEDIA_DATA,
	MEDIA_VIDEO,
	MEDIA_AUDIO,
}media_type;


typedef struct rtsp_transport_parse_t {
	int have_unicast;
	int have_multicast;
	unsigned short client_port;
	unsigned short server_port;
	char source[HOST_BUFF_DEFAULT_LEN];
	char destination[HOST_BUFF_DEFAULT_LEN];
	int have_ssrc;
	unsigned int  ssrc;
	unsigned int interleave_port;
	int use_interleaved;
} rtsp_transport_parse_t;

/*
* Some useful macros.
*/
#define ADV_SPACE(a) {while (isspace(*(a)) && (*(a) != '\0'))(a)++;}
#define CHECK_AND_FREE(a) if ((a) != NULL) { free((a)); (a) = NULL;}

/*
* Session structure.
*/
struct rtsp_session_ {
	struct rtsp_session_ *next;
	rtsp_client_t *parent;
	char *session;
	char *url;
};

/*
* client main structure
*/

struct rtsp_client_ {
	/*
	* Information about the server we're talking to.
	*/
	char *orig_url;
	char *url;
	char *server_name;
	uint16_t redirect_count;
	int useTCP;
	struct in_addr server_addr;
	struct addrinfo *addr_info;
	uint16_t port;

	/*
	* Communications information - socket, receive buffer
	*/
#ifndef _WIN32
	int server_socket;
#else
	socket server_socket;
#endif
	int recv_timeout;
	
	/*
	* rtsp information gleamed from other packets
	*/
	uint32_t next_cseq;
	char *cookie;
	rtsp_decode_t *decode_response;
	char *session;
	rtsp_session_t *session_list;

	/*
	* receive buffer
	*/
	uint32_t m_buffer_len, m_offset_on;
	char m_resp_buffer[RECV_BUFF_DEFAULT_LEN + 1];

	/*
	* auth
	*/
	char *authorization; 

	/*
	* rtsp session timeout
	*/
	int session_timeout;
	time_t last_update;

	/* core */
	core *co;

	/* sdp */
	char sdp_buf[RECV_BUFF_DEFAULT_LEN + 1];
	rtsp_transport_parse_t video_transport;
	rtsp_transport_parse_t audio_transport;

	int need_reconnect; 
};

#ifdef __cplusplus
extern "C" {
#endif

	void clear_decode_response(IN rtsp_decode_t *decode);

	void free_rtsp_client(IN rtsp_client_t *rptr);
	void free_session_info(IN rtsp_session_t *session);
	rtsp_client_t *rtsp_create_client_common(IN core *co,IN const char *url, IN int *perr);
	int rtsp_dissect_url(INOUT rtsp_client_t *rptr, IN const char *url);
	/* communications routines */
	int rtsp_create_socket(INOUT rtsp_client_t *client);
	void rtsp_close_socket(INOUT rtsp_client_t *client);

	int rtsp_send2(IN rtsp_client_t *client, IN const char *buff, IN uint32_t len);
	void rtsp_flush(IN rtsp_client_t *client);
	int rtsp_receive_socket(IN rtsp_client_t *client, OUT char *buffer, IN uint32_t len,
		IN uint32_t msec_timeout, IN int wait);

	int rtsp_get_response(INOUT rtsp_client_t *client);

	int rtsp_setup_redirect(INOUT rtsp_client_t *client);

	int rtsp_send_and_get(IN rtsp_client_t *client,IN char *buffer,IN uint32_t buflen);

	int rtsp_recv(IN rtsp_client_t *client, OUT char *buffer, IN uint32_t len);

	int rtsp_bytes_in_buffer(IN rtsp_client_t *client);
	
	void rtsp_url_split(OUT char *proto,IN int proto_size,
				  OUT char *authorization,IN int authorization_size,
				  OUT char *hostname,IN int hostname_size,
				 OUT  int *port_ptr, OUT char *path, IN int path_size, IN const char *url);

#ifdef __cplusplus
}
#endif

#endif

