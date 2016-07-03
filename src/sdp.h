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
* sdp.h - Session Description Protocol (RFC 2327) decoder/encoder
*/
#ifndef __SDP_H__
#define __SDP_H__
#include <string.h>     /* for string manipulation                          */
#include <stdio.h>
#include <stdlib.h> 
#include <time.h>
#include <errno.h>
#include "sdp_error.h"
#include <ctype.h>
#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define NTP_TO_UNIX_TIME 2208988800UL

#ifdef __cplusplus
extern "C" {
#endif

	typedef unsigned int     uint32_t;
	typedef unsigned short   uint16_t;
	/*typedef unsigned long time_t;*/
	typedef struct string_list_t {
		struct string_list_t *next;
		char *string_val;
	} string_list_t;


	typedef enum {
		BANDWIDTH_MODIFIER_NONE = 0,
		BANDWIDTH_MODIFIER_CT,
		BANDWIDTH_MODIFIER_AS,
		BANDWIDTH_MODIFIER_USER
	} bandwidth_modifier_t;

	/*
	* These next 2 set of defines are used in session_desc_t field conf_type
	* and media_desc_t field orient_type.  They need to be in this order
	* (0 for no value, user value at end), so the processing routine in
	* sdp_decode.c can use an array lookup.  See type_values, orient_values
	* static variables and the check_value_list_or_user() routine.
	* If you want to add a value, add before the user value, in the #define
	* and in the static variable.
	*/
#define CONFERENCE_TYPE_NONE      0
#define CONFERENCE_TYPE_BROADCAST 1
#define CONFERENCE_TYPE_MEETING   2
#define CONFERENCE_TYPE_MODERATED 3
#define CONFERENCE_TYPE_TEST      4
#define CONFERENCE_TYPE_H332      5
#define CONFERENCE_TYPE_OTHER     6

#define ORIENT_TYPE_NONE 0
#define ORIENT_TYPE_PORTRAIT 1
#define ORIENT_TYPE_LANDSCAPE 2
#define ORIENT_TYPE_SEASCAPE 3
#define ORIENT_TYPE_USER 4

	typedef struct bandwidth_t {
		struct bandwidth_t *next;
		bandwidth_modifier_t modifier;
		unsigned long bandwidth;
		char *user_band;
	} bandwidth_t;

	typedef struct category_list_t {
		struct category_list_t *next;
		uint32_t category;
	} category_list_t;

	typedef struct connect_desc_t {
		char *conn_type;
		char *conn_addr;
		uint32_t	ttl;
		uint32_t	num_addr;
		int used;
	} connect_desc_t;

	typedef enum key_types_t {
		KEY_TYPE_NONE = 0,
		KEY_TYPE_PROMPT,
		KEY_TYPE_CLEAR,
		KEY_TYPE_BASE64,
		KEY_TYPE_URI
	} key_types_t;

	typedef struct key_desc_t {
		key_types_t key_type;
		char *key;
	} key_desc_t;

	typedef struct rtpmap_desc_t {
		char *encode_name;
		uint32_t clock_rate;
		uint32_t encode_param;
	} rtpmap_desc_t;

	typedef struct format_list_t {
		struct format_list_t *next;
		struct media_desc_t *media;
		char *fmt;
		rtpmap_desc_t *rtpmap;
		char *fmt_param;
	} format_list_t;

	/*
	* Range information - either session or media ranges.
	* have_range is set if rest is valid.
	*
	* if range_is_npt is set, range_start and range_end will have number
	* of seconds.
	*
	* if range_is_npt is false, range_start and range_end will have number
	* of frames, based on range_smpte_fps frames per second (range_smpte_fps
	* is 0 for drop-30 format).
	*
	* if range_end_infinite is set, range_end is invalid
	*/
	typedef struct range_desc_t {
		/* range information a=range: */
		int have_range;
		int range_is_npt;
		int range_is_pts;
		double range_start;
		double range_end;
		uint16_t range_smpte_fps;
		int range_end_infinite;
	} range_desc_t;

	/*
	* basic structure for definition of a media.
	*/
	typedef struct media_desc_t {
		struct media_desc_t *next;
		struct session_desc_t *parent;
		char *media;  /* media name*/
		char *proto;  /* protocol used*/
		char *sdplang;
		char *lang;
		char *media_desc;      /* description string*/
		char *control_string;  /* rtsp control string*/
		format_list_t *fmt;    /* All possible formats for this media*/
		string_list_t *unparsed_a_lines;  /* Any unparsed lines*/
		int recvonly, sendrecv, sendonly;
		uint16_t port;       /* ip port*/
		uint16_t num_ports;   /* number of ports*/
		uint32_t ptime;       
		int ptime_present;
		uint32_t quality;
		int quality_present;
		double framerate;
		int framerate_present;
		connect_desc_t  media_connect;
		range_desc_t    media_range;
		bandwidth_t  *media_bandwidth;
		int orient_type;
		char *orient_user_type;
		key_desc_t key;
		void *USER;
	} media_desc_t;

	typedef struct time_adj_desc_t {
		struct time_adj_desc_t *next;
		time_t adj_time;
		int offset;
	} time_adj_desc_t;


#define MAX_REPEAT_OFFSETS 16
	typedef struct time_repeat_desc_t {
		struct time_repeat_desc_t *next;
		uint32_t repeat_interval;
		uint32_t active_duration;
		uint32_t   offset_cnt;
		uint32_t offsets[MAX_REPEAT_OFFSETS];
	} time_repeat_desc_t;

	typedef struct session_time_desc_t {
		struct session_time_desc_t *next;
		time_t start_time;
		time_t end_time;
		time_repeat_desc_t *repeat;
	} session_time_desc_t;

	/*
	* session_desc_t describe a session.  It can have 1 or more media
	*/
	typedef struct session_desc_t {
		struct session_desc_t *next;
		/* o= fields */
		char *orig_username;
		uint32_t   session_id;
		uint32_t   session_version;
		char *create_addr_type;
		char *create_addr;
		category_list_t *category_list;
		/* s= field */
		char *session_name;
		/* i= field */
		char *session_desc;
		/* u = field */
		char *uri;
		/* Administrator Information */
		string_list_t *admin_phone;
		string_list_t *admin_email;
		/* connect info */
		connect_desc_t session_connect;
		range_desc_t   session_range;
		bandwidth_t *session_bandwidth;
		/* key type */
		key_desc_t key;
		char *keywds;
		char *tool;
		char *charset;
		char *sdplang;
		char *lang;
		char *control_string;
		char *etag;
		/* start/end times, in an array form */
		session_time_desc_t *time_desc;
		time_adj_desc_t *time_adj_desc;
		/* media descriptions */
		media_desc_t *media;
		/* unparsed lines */
		string_list_t *unparsed_a_lines;
		int conf_type;
		char *conf_type_user;
		int recvonly, sendrecv, sendonly;
		/* For user use - nothing done internal */
		void *USER;
	} session_desc_t;

	typedef struct sdp_decode_info_ sdp_decode_info_t;

	/* for use in sdp_dump.c*/
	extern const char *type_values[];

	/*
	* decode routines.  You want to first create a sdp_decode_info_t
	* by calling either set_sdp_decode_from_memory or set_sdp_decode_from_filename
	*
	* Then call the sdp_decode routine to get the session_desc_t.  It will
	* handle more than 1 session at a time.  That number will be stored in
	* *translated.
	*/
	sdp_decode_info_t *set_sdp_decode_from_memory(const char *memptr);

	sdp_decode_info_t *set_sdp_decode_from_filename(const char *filename);

	void sdp_decode_info_free(sdp_decode_info_t *free);

	int sdp_decode(sdp_decode_info_t *decode,
		session_desc_t **retval,
		int *translated);

	void sdp_free_session_desc(session_desc_t *sptr);

	/*
	* dump routines
	*/
	void session_dump_one(session_desc_t *sptr);
	void session_dump_list(session_desc_t *sptr);

	/*
	* encode routines
	*/
	int sdp_encode_one_to_file(session_desc_t *sptr,
		const char *filename,
		int append);
	int sdp_encode_list_to_file(session_desc_t *sptr,
		const char *filename,
		int append);
	/*
	* NOTE - sdp_encode_[one|list]_to_memory require freeing memory
	*/
	int sdp_encode_one_to_memory(session_desc_t *sptr, char **mem);
	int sdp_encode_list_to_memory (session_desc_t *sptr, char **mem, int *count);




	/* utils */
	format_list_t *sdp_add_format_to_list(media_desc_t *mptr, char *val);
	void sdp_free_format_list (format_list_t **fptr);

	int sdp_add_string_to_list(string_list_t **list, char *val);
	void sdp_free_string_list (string_list_t **list);

	void sdp_time_offset_to_str(uint32_t val, char *buff, uint32_t buflen);
	format_list_t *sdp_find_format_in_line(format_list_t *head, char *lptr);
	void sdp_smpte_to_str(double value, uint16_t fps, char *buffer);

	media_desc_t *sdp_find_media_type(session_desc_t *sptr, const char *name);
	const char *find_unparsed_a_value(string_list_t *lptr, const char *value);
#ifdef __cplusplus
}
#endif
#endif /* ifdef __SDP_H__ */
