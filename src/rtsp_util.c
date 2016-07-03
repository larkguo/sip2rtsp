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
* rtsp_util.c - mixture of various utilities needed for rtsp client
*/

#include "rtsp_private.h"


/*
* free_session_info()
*Frees memory associated with that structure and clears it.
*/

void free_session_info (rtsp_session_t *session)
{
	CHECK_AND_FREE(session->session);
	CHECK_AND_FREE(session->url);
	free(session);
}

/*
* clear_decode_response()
* Frees memory associated with that structure and clears it.
*/
void clear_decode_response (rtsp_decode_t *resp)
{
	CHECK_AND_FREE(resp->retresp);
	CHECK_AND_FREE(resp->body);
	CHECK_AND_FREE(resp->accept);
	CHECK_AND_FREE(resp->accept_encoding);
	CHECK_AND_FREE(resp->accept_language);
	CHECK_AND_FREE(resp->allow_public);
	CHECK_AND_FREE(resp->authorization);
	CHECK_AND_FREE(resp->bandwidth);
	CHECK_AND_FREE(resp->blocksize);
	CHECK_AND_FREE(resp->cache_control);
	CHECK_AND_FREE(resp->content_base);
	CHECK_AND_FREE(resp->content_encoding);
	CHECK_AND_FREE(resp->content_language);
	CHECK_AND_FREE(resp->content_location);
	CHECK_AND_FREE(resp->content_type);
	CHECK_AND_FREE(resp->cookie);
	CHECK_AND_FREE(resp->date);
	CHECK_AND_FREE(resp->expires);
	CHECK_AND_FREE(resp->from);
	CHECK_AND_FREE(resp->if_modified_since);
	CHECK_AND_FREE(resp->last_modified);
	CHECK_AND_FREE(resp->location);
	CHECK_AND_FREE(resp->proxy_authenticate);
	CHECK_AND_FREE(resp->proxy_require);
	CHECK_AND_FREE(resp->range);
	CHECK_AND_FREE(resp->referer);
	CHECK_AND_FREE(resp->require);
	CHECK_AND_FREE(resp->retry_after);
	CHECK_AND_FREE(resp->rtp_info);
	CHECK_AND_FREE(resp->scale);
	CHECK_AND_FREE(resp->server);
	CHECK_AND_FREE(resp->session);
	CHECK_AND_FREE(resp->speed);
	CHECK_AND_FREE(resp->transport);
	CHECK_AND_FREE(resp->unsupported);
	CHECK_AND_FREE(resp->user_agent);
	CHECK_AND_FREE(resp->via);
	CHECK_AND_FREE(resp->www_authenticate);
	resp->content_length = 0;
	resp->cseq = 0;
	resp->close_connection = FALSE;
}

/*
* free_decode_response()
* frees memory associated with response, along with response.
*/
void free_decode_response (rtsp_decode_t *decode)
{
	if (decode != NULL) {
		clear_decode_response(decode);
		free(decode);
	}
}



/*
* rtsp_set_and_decode_url()
* will decode the url, make sure it matches RTSP url information,
* pulls out the server name and port, then gets the server address
*/
int rtsp_dissect_url (rtsp_client_t *rptr, const char *url)
{
	const char *uptr;
	char host[HOST_BUFF_DEFAULT_LEN]={0};
	int port = -1;
	
	if (rptr->url != NULL || rptr->server_name != NULL) {
		return (-1);
	}

	uptr = url;
	rptr->url = strdup(url);
	if (strncmp("rtsp://", url, strlen("rtsp://")) == 0) {
		rptr->useTCP = TRUE;
		uptr += strlen("rtsp://");
	} else {
		return(-1);
	}

	rtsp_url_split(NULL, 0, NULL, 0, host, sizeof(host), &port, NULL, 0, url);
	if( port > 0) rptr->port = port;
	else rptr->port = 554;
	
	rptr->server_name = strdup(host); 

	return (0);
}

/*
* rtsp_setup_redirect()
* Sets up URLs, does the connect for redirects.  Need to handle
* 300 case (multiple choices).  Imagine that if we had that, we'd just
* loop through the body until we found a server that we could connect
* with.
*/
int rtsp_setup_redirect (rtsp_client_t *client)
{
	rtsp_decode_t *decode;
	int ret;
	if (client->decode_response == NULL)
		return (-1);

	client->redirect_count++;
	if (client->redirect_count > 5) 
		return (-1);

	decode = client->decode_response;
	if (decode->location == NULL)
		return (-1);

	if (client->orig_url == NULL) {
		client->orig_url = client->url;
		client->url = NULL;
	} else {
		CHECK_AND_FREE(client->url);
	}

	CHECK_AND_FREE(client->server_name);
	rtsp_close_socket(client);

	ret = rtsp_dissect_url(client, decode->location);
	if (ret != 0) return (ret);

	return (rtsp_create_socket(client));

}

/* return last occurrence of needle in haystack */
static const char *my_strrstr (const char *haystack, const char *needle)
{
	int needle_len = strlen(needle);
	int haystack_len = strlen(haystack);

	haystack_len -= needle_len;
	while (haystack_len >= 0) {
		if (strncmp(haystack + haystack_len, needle, needle_len) == 0) {
			return (haystack + haystack_len);
		}
		haystack_len--;
	}
	return (NULL);
}

/* 
* removes the overlap between the base_url and the control_string
* the base_url will never contain /trackID= x, so remove it
* Example: rm_rtsp_overlap("rtsp://www.blah.com/foo", "foo/trackId=2")
*          will return "rtsp://www.blah.com/foo/trackId=2"
*/
static char *rm_rtsp_overlap (const char *control_string, const char *base_url)
{
	char *str = NULL;
	uint32_t cblen = 0; 
	char *file_ptr = strrchr(control_string, '/'); 

	if (file_ptr != NULL && file_ptr != control_string) {
		char *path = NULL;
		char *last_path_in_base = NULL;
		uint32_t control_len = strlen(control_string);
		uint32_t file_len = strlen(file_ptr);
		uint32_t last_path_len = 0;

		/* path will contain control str without /trackID = x at the end */
		path = (char *)malloc(control_len - file_len + 1);
		if (path == NULL) 
			exit(EXIT_FAILURE);
		strncpy (path, control_string, control_len - file_len);
		path[control_len - file_len] = '\0';

		last_path_in_base = strdup(my_strrstr(base_url, path));
		if (last_path_in_base != NULL) 
			last_path_len = strlen(last_path_in_base);
		if (last_path_in_base == NULL 
			|| last_path_len != strlen(path)) {
				/* couldn't find path in base url or isn't at end of base url */
				free(path);
				free(last_path_in_base);
				return ((char *)NULL);
		} 
		else {
			/* make sure there is one and only one '/' between the base url
			* and the control string */
			cblen = strlen(base_url) - last_path_len;
			if (*control_string != '/') {
				cblen +=1;
			}
			str = (char *)malloc(cblen + control_len +1);
			if (str == NULL) 
				exit(EXIT_FAILURE);
			/* copy base_url up to last occurrence of path */
			strncpy(str, base_url, cblen);
			str[cblen] = '\0';
			strcat(str, control_string);
			free(path);
		}
		free(last_path_in_base);
	}
	return ((char *)str);
}


/* 
* attempts to match the session url with  
* content_base/url or session_name/url
* if content base and url overlap, or if session_name and 
* url overlap, remove the overlap and attempt to match again
* return 1 if matched, 0 otherwise
*/
int rtsp_is_url_my_stream (rtsp_session_t *session,
						   const char *url,
						   const char *content_base,
						   const char *session_name)
{ 
	char *session_url = session->url;
	const char *end;
	int is_match = 0;

	end = my_strrstr(session_url, url); 
	if (end != NULL  || strcmp(url,"*") == 0) {
		if (strncmp(session_url, content_base,
			strlen(session_url) - strlen(end)) == 0
			|| strncmp(session_url, session_name, 
			strlen(session_url) - strlen(end)) == 0) {
				is_match = 1;
		}
		else {
			/* url isn't contained in the session_url */
			/* check if there is an overlap */
			char *str1 = rm_rtsp_overlap(url, content_base);
			char *str2 = rm_rtsp_overlap(url, session_name);
			if (strcmp(session_url, str1) == 0 
				|| strcmp(session_url, str2) == 0)  
				is_match = 1;
			free(str1);
			free(str2);
		}
	}
	return (is_match);
}

struct in_addr get_server_ip_address (rtsp_session_t *session)
{
	return session->parent->server_addr;
}


size_t rtsp_strlcpy(char *dst, const char *src, size_t size)
{
	size_t len = 0;
	while (++len < size && *src)
		*dst++ = *src++;
	if (len <= size)
		*dst = 0;
	return len + strlen(src) - 1;
}

void rtsp_url_split(char *proto, int proto_size,
				  char *authorization, int authorization_size,
				  char *hostname, int hostname_size,
				  int *port_ptr, char *path, int path_size, const char *url)
{
	const char *p, *ls, *ls2, *at, *at2, *col, *brk;

	if (port_ptr)
		*port_ptr = -1;
	if (proto_size > 0)
		proto[0] = 0;
	if (authorization_size > 0)
		authorization[0] = 0;
	if (hostname_size > 0)
		hostname[0] = 0;
	if (path_size > 0)
		path[0] = 0;

	/* parse protocol */
	if ((p = strchr(url, ':'))) {
		rtsp_strlcpy(proto, url, MIN(proto_size, p + 1 - url));
		p++; /* skip ':' */
		if (*p == '/')
			p++;
		if (*p == '/')
			p++;
	} else {
		/* no protocol means plain filename */
		rtsp_strlcpy(path, url, path_size);
		return;
	}

	/* separate path from hostname */
	ls = strchr(p, '/');
	ls2 = strchr(p, '?');
	if (!ls)
		ls = ls2;
	else if (ls && ls2)
		ls = MIN(ls, ls2);
	if (ls)
		rtsp_strlcpy(path, ls, path_size);
	else
		ls = &p[strlen(p)];  // XXX

	/* the rest is hostname, use that to parse auth/port */
	if (ls != p) {
		/* authorization (user[:pass]@hostname) */
		at2 = p;
		while ((at = strchr(p, '@')) && at < ls) {
			rtsp_strlcpy(authorization, at2,MIN(authorization_size, at + 1 - at2));
			p = at + 1; /* skip '@' */
		}

		if (*p == '[' && (brk = strchr(p, ']')) && brk < ls) {
			/* [host]:port */
			rtsp_strlcpy(hostname, p + 1,MIN(hostname_size, brk - p));
			if (brk[1] == ':' && port_ptr)
				*port_ptr = atoi(brk + 2);
		} else if ((col = strchr(p, ':')) && col < ls) {
			rtsp_strlcpy(hostname, p,MIN(col + 1 - p, hostname_size));
			if (port_ptr)
				*port_ptr = atoi(col + 1);
		} else
			rtsp_strlcpy(hostname, p,MIN(ls + 1 - p, hostname_size));
	}
}


