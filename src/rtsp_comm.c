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
* rtsp_comm.c - contains communication routines.
*/

#include "rtsp_private.h"


/*
* rtsp_create_socket()
* creates and connects socket to server.  Requires rtsp_info_t fields
* port, server_addr, server_name be set.
* returns 0 for success, -1 for failure
*/
static int rtsp_get_server_address (rtsp_client_t *client)
{
#ifdef HAVE_IPv6
	struct addrinfo hints;
	char port[32];
	int error = -1;

	snprintf(port, sizeof(port), "%d", client->port);
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	error = getaddrinfo(client->server_name, port, &hints, &client->addr_info);
	if (error) {
		log(client->co,LOG_DEBUG,"Can't get server address client %s - error %d\n",
			client->server_name, error);
		return error;
	}
#else
	int error = -1;
	struct hostent *host; 
	if (inet_aton(client->server_name, &client->server_addr) != 0) return 0; 

	host = gethostbyname(client->server_name);
	if (host == NULL) {
		log(client->co,LOG_DEBUG,"Can't get server host name %s\n", client->server_name);
		return (error);
	}
	memcpy(&client->server_addr, host->h_addr,4);	
	//client->server_addr = *(struct in_addr *)host->h_addr;
#endif
	return 0;
}

int rtsp_create_socket (rtsp_client_t *client)
{
#ifndef HAVE_IPv6
	struct sockaddr_in sockaddr;
#endif
	int result;

	/* Do we have a socket already - if so, go ahead*/

	if (client->server_socket != -1) {
		return (0);
	}

	if (client->server_name == NULL) {
		log(client->co,LOG_DEBUG, "No server name in create socket\n");
		return (-1);
	}
	result = rtsp_get_server_address(client);
	if (result != 0) return -1;

#ifdef HAVE_IPv6
	client->server_socket = socket(client->addr_info->ai_family,
		client->addr_info->ai_socktype,
		client->addr_info->ai_protocol);
#else
	client->server_socket = socket(AF_INET, SOCK_STREAM, 0);
#endif

	if (client->server_socket == -1) {
		log(client->co,LOG_DEBUG, "Couldn't create socket\n");
		return (-1);
	}

#ifndef HAVE_IPv6
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(client->port);
	sockaddr.sin_addr = client->server_addr;
#endif

	result = connect(client->server_socket,
#ifndef HAVE_IPv6
		(struct sockaddr *)&sockaddr,
		sizeof(sockaddr)
#else
		client->addr_info->ai_addr,
		client->addr_info->ai_addrlen
#endif
		);
	if (result < 0){
		log(client->co,LOG_DEBUG, "Couldn't connect socket - error %s\n",strerror(result));
		return (-1);
	}

	return (0);
}


/*
* rtsp_send()
* Sends a buffer over connected socket.  If socket isn't connected,
* tries that first.
* Buffer must be formatted to RTSP spec.
* Inputs:
*   client - pointer to rtsp_client_t for client session
*   buff - pointer to buffer
*   len  - length of buffer
* Outputs:
*   0 - success, -1 failure
*/
int rtsp_send2 (rtsp_client_t *client, const char *buff, uint32_t len)
{
	int ret;
		
	if (client->server_socket == -1) {
		if (rtsp_create_socket(client) != 0)
			return (-1);
	}

	ret = send(client->server_socket, buff, len, MSG_NOSIGNAL);
	return (ret);
}

/*
* rtsp_receive()
* Receives a response from server with a timeout.  If recv returns a
* full buffer, and the last character is not \r or \n, will make a
* bigger buffer and try to receive.
*
* Will set fields in rtsp_client_t.  Relevent fields are:
*   recv_buff - pointer to receive buffer (malloc'ed so we can always add
*           \0 at end).
*   recv_buff_len - max size of receive buffer.
*   recv_buff_used - number of bytes received.
*   recv_buff_parsed - used by above routine in case we got more than
*      1 response at a time.
*/
int rtsp_receive_socket (rtsp_client_t *client, char *buffer, uint32_t len,
						 uint32_t msec_timeout, int wait)
{
	int ret;
	fd_set read_set;
	struct timeval timeout;

	if (msec_timeout != 0 && wait != 0) {
		FD_ZERO(&read_set);
		FD_SET(client->server_socket, &read_set);
		timeout.tv_sec = (long)msec_timeout / 1000;
		timeout.tv_usec = ((long)msec_timeout % 1000) * 1000;
		ret = select(client->server_socket + 1, &read_set, NULL, NULL, &timeout);
		if (ret <= 0) {
			log(client->co,LOG_DEBUG, "Response timed out %d %d\n", msec_timeout, ret);
			if (ret == -1) {
				log(client->co,LOG_DEBUG, "Error is %s\n",strerror(ret));
			}
			return (-1);
		}
	}

	ret = recv(client->server_socket, buffer, len, MSG_NOSIGNAL);
	return (ret);
}

/*
* rtsp_close_socket
* closes the socket.  Duh.
*/
void rtsp_close_socket (rtsp_client_t *client)
{
	if (client->server_socket != -1)
		close(client->server_socket);
	client->server_socket = -1;
#ifdef HAVE_IPv6
	if (client->addr_info != NULL) {
		freeaddrinfo(client->addr_info);
		client->addr_info = NULL;
	}
#endif
}
