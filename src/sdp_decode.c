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
* sdp_decode.c
*
* decode SDP format from file into session description structures
* defined in sdp.h
*
* October, 2000
* Bill May (wmay@cisco.com)
* Cisco Systems, Inc.
*/

#include "sdp.h"
#include "sdp_decode_private.h"

#define ADV_SPACE(a) {while (isspace(*(a)) && (*(a) != '\0'))(a)++;}

static const char *SPACES=" \t";

#define FREE_CHECK(a,b) if (a->b != NULL) { free(a->b); a->b = NULL;}

/*****************************************************************************
* Memory free routines - frees memory associated with various structures
*****************************************************************************/
static void free_bandwidth_desc (bandwidth_t *bptr)
{
	bandwidth_t *q;
	while (bptr != NULL) {
		q = bptr;
		bptr = q->next;
		FREE_CHECK(q, user_band);
		free(q);
	}
}

static void free_category_list (category_list_t **cptr)
{
	category_list_t *p;
	if (*cptr == NULL) return;

	while (*cptr != NULL) {
		p = *cptr;
		*cptr = p->next;
		free(p);
	}
}

static void free_connect_desc (connect_desc_t *cptr)
{
	FREE_CHECK(cptr, conn_type);
	FREE_CHECK(cptr, conn_addr);
}

/*
* free_media_desc()
* Frees all memory associated with a media descriptor(mptr)
*/
static void free_media_desc (media_desc_t *mptr)
{
	free_bandwidth_desc(mptr->media_bandwidth);
	mptr->media_bandwidth = NULL;
	free_connect_desc(&mptr->media_connect);
	sdp_free_format_list(&mptr->fmt);
	sdp_free_string_list(&mptr->unparsed_a_lines);
	FREE_CHECK(mptr, media);
	FREE_CHECK(mptr, media_desc);
	FREE_CHECK(mptr, proto);
	FREE_CHECK(mptr, sdplang);
	FREE_CHECK(mptr, lang);
	FREE_CHECK(mptr, orient_user_type);
	FREE_CHECK(mptr, control_string);
	FREE_CHECK(mptr, key.key);
	mptr->parent = NULL;
	free(mptr);
}

static void free_time_desc (session_time_desc_t *time)
{
	time_repeat_desc_t *rptr;

	if (time->next != NULL) {
		free_time_desc(time->next);
		time->next = NULL;
	}
	while (time->repeat != NULL) {
		rptr = time->repeat;
		time->repeat = rptr->next;
		free(rptr);
	}

	free(time);
}

/*
* sdp_free_session_desc()
* Inputs - sptr - pointer to session_description list to free
*/
void sdp_free_session_desc (session_desc_t *sptr)
{
	session_desc_t *p;
	media_desc_t *mptr, *q;

	p = sptr;
	while (p != NULL) {
		sptr = p;
		p = p->next;

		sptr->next = NULL;
		mptr = sptr->media;
		sptr->media = NULL;

		while (mptr != NULL) {
			q = mptr;
			mptr = q->next;
			free_media_desc(q);
		}

		FREE_CHECK(sptr, etag);
		FREE_CHECK(sptr, orig_username);
		FREE_CHECK(sptr, control_string);
		FREE_CHECK(sptr, create_addr_type);
		FREE_CHECK(sptr, create_addr);
		FREE_CHECK(sptr, session_name);
		FREE_CHECK(sptr, session_desc);
		FREE_CHECK(sptr, uri);
		FREE_CHECK(sptr, key.key);
		FREE_CHECK(sptr, keywds);
		FREE_CHECK(sptr, lang);
		FREE_CHECK(sptr, tool);
		FREE_CHECK(sptr, charset);
		FREE_CHECK(sptr, sdplang);
		FREE_CHECK(sptr, conf_type_user);

		if (sptr->time_desc != NULL) {
			free_time_desc(sptr->time_desc);
			sptr->time_desc = NULL;
		}

		free_bandwidth_desc(sptr->session_bandwidth);
		sptr->session_bandwidth = NULL;
		free_category_list(&sptr->category_list);
		free_connect_desc(&sptr->session_connect);
		sdp_free_string_list(&sptr->admin_phone);
		sdp_free_string_list(&sptr->admin_email);
		sdp_free_string_list(&sptr->unparsed_a_lines);

		while (sptr->time_adj_desc != NULL) {
			time_adj_desc_t *aptr;
			aptr = sptr->time_adj_desc;
			sptr->time_adj_desc = aptr->next;
			free(aptr);
		}

		free(sptr);
	}
}

/*****************************************************************************
* Utility routines - string manipulation, etc
*****************************************************************************/
/*
* get_next_line will get the next line to process, and stick it in
* lptr.
*
* Inputs:
*    lptr - buffer to store into
*    decode - pointer to where to get next line
*    buflen - max length of buffer
*
* Outputs:
*    TRUE - have a new line.
*    FALSE - all done.
*/
static int get_next_line (char **polptr,
						  sdp_decode_info_t *decode,
						  uint32_t *buflen)
{
	char *fret;
	int len;
	uint32_t buflen_left;
	const char *cptr;

	if (decode->isMem) {
		cptr = decode->memptr;

		if (*cptr == '\0') return FALSE;

		while (*cptr != '\0' && *cptr != '\n' && *cptr != '\r') cptr++;

		len = cptr - decode->memptr;
		if (*buflen <= len + 1) {
			if (len > 65535) {
				printf( "Max line length of 65535 exceeded %u\n",len);
				return (FALSE);
			}
			*polptr = realloc(*polptr, len + 1);
			*buflen = len + 1;
		}
		memcpy(*polptr, decode->memptr, len);
		(*polptr)[len] = '\0';
		decode->memptr += len;
		while (*decode->memptr == '\n' || *decode->memptr == '\r')
			decode->memptr++;
	} else {
		char *ptr;
		/* File reads...*/
		if (decode->ifile == NULL)
			return FALSE;
		if (*buflen == 0) {
			*polptr = (char *)malloc(1024);
			*buflen = 1024;
		}

		/* Read file until we hit the end, or a non-blank line read*/
		ptr = *polptr;
		buflen_left = *buflen;
		len = 0;

		while (1) {
			fret = fgets(ptr, buflen_left, decode->ifile);
			if (fret == NULL) {
				if (len > 0) {
					(*polptr)[len] = '\0'; /* make sure*/
					return (TRUE);
				}
				return (FALSE);
			}

			len = strlen(ptr);
			if (ptr[len - 1] == '\n' || ptr[len - 1] == '\r') {
				/* we have an end of line*/
				len--;
				while (len >= 0 &&
					(ptr[len] == '\n' || ptr[len] == '\r')) {
						ptr[len] = '\0';
						len--;
				}
				return (TRUE);
			}

			/* too long...*/
			buflen_left -= len;
			if (*buflen + 1024 > 65535) {
				printf( "Max line length of 65535 exceeded %u\n",*buflen);
				return (FALSE);
			}
			*buflen += 1024;
			buflen_left += 1024;
			*polptr = realloc(*polptr, *buflen);
			ptr = *polptr + *buflen - buflen_left;
		}
	}

	return TRUE;
}

/*
* strtou64()
* Converts string to uint32_t number up to next space, with error checking.
* Inputs:
*    lptr - pointer to pointer to string.  *lptr will be modified
*    num - return value
* Outputs - TRUE - sucess, FALSE, failure
*/
static int strtou64 (char **lptr, uint32_t *num)
{
	char *sep;

	*num = 0;
	ADV_SPACE((*lptr));
	sep = strsep(lptr, SPACES);
	if (sep == NULL || *lptr == NULL) {
		return (FALSE);
	}
	*num = 0;
	while (*sep != '\0') {
		if (isdigit(*sep)) {
			*num *= 10;
			*num += *sep - '0';
			sep++;
		} else {
			return (FALSE);
		}
	}
	return (TRUE);
}

/*
* str_to_time_offset()
* converts typed-time field (number or [#d][#h][#m][#s])
*
* Inputs -
*   str - \0 terminated string to translate
*   retval - pointer to return value
* Returns -
*   TRUE - valid string processed;
*   FALSE - invalid string
*/
static int str_to_time_offset (const char *str, uint32_t *retval)
{
	uint32_t value;
	uint32_t accum;
	char temp;

	value = 0;
	accum = 0;

	if (!isdigit(*str)) return (FALSE);

	while (*str != '\0') {
		if (isdigit(*str)) {
			accum *= 10;
			accum += (*str - '0');
		} else {
			temp = tolower(*str);
			if (temp == 'd') {
				value += accum * SEC_PER_DAY;
				accum = 0;
			} else if (temp == 'h') {
				value += accum * SEC_PER_HOUR;
				accum = 0;
			} else if (temp == 'm') {
				value += accum * SEC_PER_MINUTE;
				accum = 0;
			} else if (temp == 's') {
				value += accum;
				accum = 0;
			} else {
				printf( "Illegal character %c in time offset\n", temp);
				return (FALSE);
			}
		}
		str++;
	}
	value += accum;
	*retval = value;
	return (TRUE);
}

/*
* time_adj_order_in_list()
* order linked list by adj_time values.  We don't allow 2 items in the
* list - "new" value might be free'd at end
* Inputs:
*   start - pointer to start of list
*   new - pointer to new value
* Returns:
*   pointer to head of list - new value might be free'd.
*/
static time_adj_desc_t *time_adj_order_in_list (time_adj_desc_t *start,
												time_adj_desc_t *new)
{
	time_adj_desc_t *p, *q;
	if (start == NULL)
		return (new);

	p = start;
	q = NULL;
	while (p != NULL) {
		if (new->adj_time == p->adj_time) {
			printf( "Duplicate time %ld in adj description\n", p->adj_time);
			free(new);
			return (start);
		}
		if (new->adj_time < p->adj_time) {
			new->next = p;
			if (q == NULL) {
				return (new);
			} else {
				q->next = new;
				return (start);
			}
		}
		q = p;
		p = p->next;
	}
	q->next = new;
	return (start);
}


/*****************************************************************************
* Line parsing routines
*****************************************************************************/
/*
* sdp_decode_parse_a_bool()
* parses a boolean field.
*/
static int sdp_decode_parse_a_bool (int arg,
									char *lptr,
									session_desc_t *sptr,
									media_desc_t *mptr)
{
	switch (arg) {
	  case 0:
		  if (mptr != NULL) mptr->recvonly = TRUE;
		  else sptr->recvonly = TRUE;
		  break;
	  case 1:
		  if (mptr != NULL) mptr->sendrecv = TRUE;
		  else sptr->sendrecv = TRUE;
		  break;
	  case 2:
		  if (mptr != NULL) mptr->sendonly = TRUE;
		  else sptr->sendonly = TRUE;
		  break;
	}
	return (0);
}

/*
* sdp_decode_parse_a_rtpmap()
* parses a=rtpmap:<fmt> <name>/<clockrate>[/<enc_param>]
*/
static int sdp_decode_parse_a_fmtp (int arg,
									char *lptr,
									session_desc_t *sptr,
									media_desc_t *mptr)
{
	format_list_t *fptr;
	int len;

	if (mptr == NULL)
		return (-1);

	/*
	* See our format matches a value in the media's format list.
	*/
	fptr = sdp_find_format_in_line(mptr->fmt,lptr) ;

	if (fptr == NULL) {
		printf( "Can't find fmtp format %s in media format list\n", lptr);
		return (-1);
	}

	len = strlen(fptr->fmt);
	lptr += len;
	lptr++;
	ADV_SPACE(lptr);
	fptr->fmt_param = strdup(lptr);
	if (fptr->fmt_param == NULL) {
		return (-1);
	}
	return (0);
}

/*
* sdp_decode_parse_a_rtpmap()
* parses a=rtpmap:<fmt> <name>/<clockrate>[/<enc_param>]
*/
static int sdp_decode_parse_a_rtpmap (int arg,
									  char *lptr,
									  session_desc_t *sptr,
									  media_desc_t *mptr)
{
	char *enc, *slash, *temp;
	format_list_t *fptr;
	int len;
	uint32_t a, b;

	if (mptr == NULL)
		return (-1);

	/*
	* See our format matches a value in the media's format list.
	*/
	fptr = sdp_find_format_in_line(mptr->fmt,lptr) ;

	if (fptr == NULL) {
		printf( "Can't find rtpmap format %s in media list\n", lptr);
		return (-1);
	}

	len = strlen(fptr->fmt);
	/*
	* Matches entry left in fptr.  Decode rest of line
	*/
	enc = lptr + len;
	ADV_SPACE(enc);
	slash = strchr(enc, '/');
	if (slash != NULL) {
		*slash++ = '\0';
		temp = enc;
		while ((!(isspace(*temp))) && *temp != '\0') temp++;
		*temp = '\0';

		/* enc points to encode name*/
		ADV_SPACE(slash);
		temp = strsep(&slash, " \t/");
		if (temp == NULL) {
			printf( "Can't find seperator after encode name in rtpmap\n");
			return (-1);
		}

		if (sscanf(temp, "%u", &a) == 0) {
			printf( "Couldn't decode rtp clockrate %s\n", temp);
			return (-1);
		}

		b = 0;
		if (slash != NULL) {
			ADV_SPACE(slash);
			if (*slash == '/') {
				slash++;
				ADV_SPACE(slash);
			}
			if (isdigit(*slash)) {
				sscanf(slash, "%u", &b);
			}
		}
	} else {
		printf( "a=rtpmap clock rate is missing.\n");
		printf( "Most likely, you're decoding SDP from Apple's broadcaster\n");
		printf( "They initially misinterpreted RFC3016, but should fix it at some point\n");
		printf( "You may see audio/video at the wrong rate\n");
		a = 90000;
		b = 0;
	}

	fptr->rtpmap = malloc(sizeof(rtpmap_desc_t));
	if (fptr->rtpmap == NULL)
		return (-1);
	fptr->rtpmap->encode_name = strdup(enc);
	fptr->rtpmap->clock_rate = a;
	fptr->rtpmap->encode_param = b;

	return (0);
}

/*
* sdp_decode_parse_a_cat()
* parses a=category:foo.bar...
*/
static int sdp_decode_parse_a_cat (int arg,
								   char *orig_line,
								   session_desc_t *sptr,
								   media_desc_t *mptr)
{
	char *sep, *line, *lptr;
	int errret;
	uint32_t cat;
	category_list_t *cptr, *new;

	if (sptr->category_list != NULL) {
		return (-1);
	}
	errret = 0;
	cptr = NULL; /* shut up compiler*/
	line = strdup(orig_line);
	lptr = line;
	while (NULL!=(sep = strsep(&lptr, " \t."))) {
		if (*sep != '\0') {
			cat = 0;
			while (isdigit(*sep)) {
				cat *= 10;
				cat += *sep - '0';
				sep++;
			}
			if (cat == 0) {
				break;
			}
			new = malloc(sizeof(category_list_t));
			if (new == NULL) {
				break;
			}
			new->category = cat;
			new->next = NULL;
			if (sptr->category_list == NULL) {
				cptr = sptr->category_list = new;
			} else {
				cptr->next = new;
				cptr = new;
			}
		}
	}
	if (errret != 0) {
		free_category_list(&sptr->category_list);
	}
	free(line);
	return (errret);
}

/*
* sdp_decode_parse_a_frame()
* parses a=framerate:<float number>
*/
static int sdp_decode_parse_a_frame (int arg,
									 char *lptr,
									 session_desc_t *sptr,
									 media_desc_t *mptr)
{
	char *endptr;

	if (mptr == NULL) {
		return (-1);
	}

	mptr->framerate = strtod(lptr, &endptr);
	if (endptr == lptr || endptr == NULL) return (-1);
	ADV_SPACE(endptr);
	if (*endptr != '\0') {
		printf( "Garbage at end of frame rate `%s\'\n", endptr);
		return (-1);
	}
	mptr->framerate_present = TRUE;
	return (0);
}

static int convert_npt (char *from, char *to, double *ret)
{
	int decimal = FALSE;
	double accum;
	double mult;

	*ret = 0.0;
	mult = 0.0;
	accum = 0.0;

	while ((to == NULL && *from != '\0') || from < to) {
		if (isdigit(*from)) {
			if (decimal == FALSE) {
				accum *= 10.0;
				accum += (double)(*from )- (double)'0';
			} else {
				accum += (((double)(*from) - (double) '0') * mult);
				mult /= 10.0;
			}
		} else if (*from == ':') {
			accum *= 60.0;
			*ret += accum;
			accum = 0;
		} else if (*from == '.') {
			decimal = TRUE;
			mult = .1;
		} else if (strncmp(from,"beginning",strlen("beginning"))==0){
			*ret=0;
			return(TRUE);
		} else {
			printf( "Illegal character in NPT string %c\n", *from);
			return (FALSE);
		}
		from++;
	}
	*ret += accum;
	return (TRUE);
}

static int convert_pts (char *from, char *to, double *ret)
{
	int decimal = FALSE;
	double accum;
	double mult;

	*ret = 0.0;
	mult = 0.0;
	accum = 0.0;

	while ((to == NULL && *from != '\0') || from < to) {
		if (isdigit(*from)) {
			if (decimal == FALSE) {
				accum *= 10.0;
				accum += (double) *from - (double) '0';
			} else {
				accum += (((double) *from - (double) '0') * mult);
				mult /= 10.0;
			}
		} else if (*from == ':') {
			accum *= 60.0;
			*ret += accum;
			accum = 0;
		} else if (*from == '.') {
			decimal = TRUE;
			mult = .1;
		} else if (strncmp(from,"beginning",strlen("beginning"))==0){
			*ret=0;
			return(TRUE);
		} else {
			printf( "Illegal character in NPT string %c\n", *from);
			return (FALSE);
		}
		from++;
	}
	*ret += accum;
	return (TRUE);
}

static int convert_smpte (char *from, char *to, uint16_t fps, double *ret)
{
	int decimal = FALSE;
	double accum;
	double mult;
	unsigned int colon;

	*ret = 0.0;
	mult = 0.0;
	accum = 0.0;
	colon = 0;

	if (fps == 0) fps = 30;

	while ((to == NULL && *from != '\0') || from < to) {
		if (isdigit(*from)) {
			if (decimal == FALSE) {
				accum *= 10.0;
				accum += ((double)*from - (double)'0');
			} else {
				accum += (((double) *from - (double) '0') * mult);
				mult /= 10.0;
			}
		} else if (*from == ':') {
			*ret += accum;
			if (colon > 1)
				*ret *= fps;
			else
				*ret *= 60.0;
			accum = 0.0;
			colon++;
		} else if (*from == '.') {
			decimal = TRUE;
			mult = .1;
		} else {
			printf( "Illegal character in SMPTE decode %c\n", *from);
			return (FALSE);
		}
		from++;
	}
	*ret += accum;
	if (colon <= 2) *ret *= fps;
	return (TRUE);
}

static int sdp_decode_parse_a_range (int arg,
									 char *lptr,
									 session_desc_t *sptr,
									 media_desc_t *mptr)
{
	char *dash;
	char *second;
	range_desc_t *rptr;

	if (mptr == NULL) rptr = &sptr->session_range;
	else rptr = &mptr->media_range;

	if (rptr->have_range) return (-1);
	if (strncmp(lptr, "npt", strlen("npt")) == 0) {
		lptr += strlen("npt");
		rptr->range_is_npt = TRUE;
	} else if (strncmp(lptr, "smpte", strlen("smpte")) == 0) {
		lptr += strlen("smpte");
		rptr->range_is_npt = FALSE;
		if (*lptr == '-') {
			lptr++;
			if (strncmp(lptr, "30-drop", strlen("30-drop")) == 0) {
				rptr->range_smpte_fps = 0;
				lptr += strlen("30-drop");
			} else {
				while (isdigit(*lptr)) {
					rptr->range_smpte_fps *= 10;
					rptr->range_smpte_fps += *lptr - '0';
					lptr++;
				}
			}
		} else {
			rptr->range_smpte_fps = 0;
		}
	}else if(strncmp(lptr, "pts", strlen("pts")) == 0){
		lptr += strlen("pts");
		rptr->range_is_npt = FALSE;
		rptr->range_is_pts = TRUE;
	}else {
		printf( "range decode - unknown keyword %s\n", lptr);
		return (-1);
	}
	ADV_SPACE(lptr);
	if (*lptr != '=') {
		printf( "range decode - no =\n");
		return (-1);
	}
	lptr++;
	ADV_SPACE(lptr);
	dash = strchr(lptr, '-');
	if (dash == NULL) return (-1);
	if (rptr->range_is_npt) {
		if (convert_npt(lptr, dash, &rptr->range_start) == FALSE) {
			printf( "Couldn't decode range from npt %s\n", lptr);
			return (-1);
		}
	} else {
		if(rptr->range_is_pts)
		{
			if (convert_npt(lptr, dash, &rptr->range_start) == FALSE)  {
				printf( "Couldn't decode range from pts %s\n", lptr);
				return (-1);
			}
		}
		else
		{
			if (convert_smpte(lptr,
				dash,
				rptr->range_smpte_fps,
				&rptr->range_start) == FALSE) {
					printf( "Couldn't decode range from smpte %s\n", lptr);
					return (-1);
			}
		}
	}

	second = dash + 1;
	ADV_SPACE(second);
	if (*second != '\0') {
		if (rptr->range_is_npt) {
			if(strncmp(second, "end", strlen("end")))
			{
				if (convert_npt(second, NULL, &rptr->range_end) == FALSE) {
					printf( "Couldn't decode range to npt %s\n", lptr);
					return (-1);
				}
			}
			else{
				rptr->range_end_infinite = TRUE;
			}
		} else {
			if(rptr->range_is_pts)
			{
				if(strncmp(second, "end", strlen("end")))
				{
					if (convert_pts(second, NULL, &rptr->range_end) == FALSE) {
						printf( "Couldn't decode range to pts %s\n", lptr);
						return (-1);
					}
				}else{
					rptr->range_end_infinite = TRUE;}
			}
			else
			{
				if (convert_smpte(second,
					NULL,
					rptr->range_smpte_fps,
					&rptr->range_end) == FALSE) {
						printf( "Couldn't decode range to smpte %s\n", lptr);
						return (-1);
				}
			}
		}
	} else {
		rptr->range_end_infinite = TRUE;
	}
	rptr->have_range = TRUE;
	return (0);
}

/*
* sdp_decode_parse_a_int()
* parses a=<name>:<uint>
*/
static int sdp_decode_parse_a_int (int arg,
								   char *orig_line,
								   session_desc_t *sptr,
								   media_desc_t *mptr)
{
	uint32_t num;

	num = 0;
	if (!isdigit(*orig_line)) {
		return (-1);
	}
	while (isdigit(*orig_line)) {
		num *= 10;
		num += *orig_line - '0';
		orig_line++;
	}
	ADV_SPACE(orig_line);
	if (*orig_line != '\0') {
		printf( "Garbage at end of integer %s\n", orig_line);
		return(-1);
	}

	switch (arg) {
  case 0:
	  if (mptr == NULL) return (-1);
	  mptr->ptime = num;
	  mptr->ptime_present = TRUE;
	  break;
  case 1:
	  if (mptr == NULL) return (-1);
	  mptr->quality = num;
	  mptr->quality_present = TRUE;
	  break;
	}
	return (0);
}

/*
* check_value_list_or_user()
* This will compare string in lptr with items in list.  If strncmp()
* matches, and the next value after the match in lptr is a space or \0,
* we return the index in list + 1.
* If no entrys are on the list, we'll strdup the value, and store in
* *uservalue
*/
static int check_value_list_or_user (char *lptr,
									 const char **list,
									 char **user_value)
{
	uint32_t len;
	int cnt;

	cnt = 1;
	while (*list != NULL) {
		len = strlen(*list);
		if (strncmp(lptr, *list, len) == 0) {
			return (cnt);
		}
		cnt++;
		list++;
	}
	*user_value = strdup(lptr);
	return(cnt);
}

const char *type_values[] = {
	"broadcast", /* CONFERENCE_TYPE_BROADCAST*/
	"meeting",   /* CONFERENCE_TYPE_MEETING  */
	"moderated", /* CONFERENCE_TYPE_MODERATED*/
	"test",      /* CONFERENCE_TYPE_TEST     */
	"H332",      /* CONFERENCE_TYPE_H332     */
	NULL         /* CONFERENCE_TYPE_USER     */
};

static const char *orient_values[] = {
	"portrait", /* ORIENT_TYPE_PORTRAIT  */
	"landscape",/* ORIENT_TYPE_LANDSCAPE */
	"seascape", /* ORIENT_TYPE_SEASCAPE  */
	NULL        /* ORIENT_TYPE_USER      */
};

/*
* sdp_decode_parse_a_str()
* parses a=<identifier>:<name>
* Will usually save the value of <name> in a field in the media_desc_t or
* session_desc_t.
*/
static int sdp_decode_parse_a_str (int arg,
								   char *lptr,
								   session_desc_t *sptr,
								   media_desc_t *mptr)
{
	switch (arg) {
	  case 0: /* keywds*/
		  if (sptr->keywds != NULL) {
			  return (-1);
		  }
		  sptr->keywds = strdup(lptr);
		  break;
	  case 1: /* tool*/
		  if (sptr->tool != NULL) {
			  return (-1);
		  }
		  sptr->tool = strdup(lptr);
		  break;
	  case 2: /* charset*/
		  if (sptr->charset != NULL) {
			  return (-1);
		  }
		  sptr->charset = strdup(lptr);
		  break;
	  case 3: /* sdplang*/
		  if (mptr != NULL) {
			  if (mptr->sdplang != NULL) {
				  return (-1);
			  }
			  mptr->sdplang = strdup(lptr);
		  } else {
			  if (sptr->sdplang != NULL) {
				  return (-1);
			  }
			  sptr->sdplang = strdup(lptr);
		  }
		  break;
	  case 4: /* lang*/
		  if (mptr != NULL) {
			  if (mptr->lang != NULL) {
				  return (-1);
			  }
			  mptr->lang = strdup(lptr);
		  } else {
			  if (sptr->lang != NULL) {
				  return (-1);
			  }
			  sptr->lang = strdup(lptr);
		  }
		  break;
	  case 5: /* type*/
		  if (sptr->conf_type != 0) {
			  return (-1);
		  }
		  sptr->conf_type = check_value_list_or_user(lptr,
			  type_values,
			  &sptr->conf_type_user);
		  break;
	  case 6: /* orient*/
		  if (mptr == NULL || mptr->orient_type != 0) {
			  return (-1);
		  }
		  mptr->orient_type = check_value_list_or_user(lptr,
			  orient_values,
			  &mptr->orient_user_type);
		  break;
	  case 7: /* control*/
		  if (mptr == NULL) {
			  if (sptr->control_string != NULL) {
				  return (-1);
			  }
			  sptr->control_string = strdup(lptr);
		  } else {
			  if (mptr->control_string != NULL) {
				  return (-1);
			  }
			  mptr->control_string = strdup(lptr);
		  }
		  break;
	  case 8:
		  if (sptr->etag != NULL) {
			  return (-1);
		  }
		  sptr->etag = strdup(lptr);
		  break;
	}
	return (0);
}


/*
* This structure provides the information needed by the parsing
* engine in sdp_decode_parse_a.  This function processes lines of
* the format a=<identifier>[:<options>].
* name - <identifier>
* len - sizeof(identifier) (saves on CPU time)
* have_colon - if a colon is necessary
* remove_spaces_after_colon - if colon is necessary, and we want
*   to remove spaces before <option>.  Do not use this if other character
*   sets may come into play.
* parse_routine - routine to call if keyword matched.  Return 0 if successful-
*   -1 will save the whole "a=..." string.
* arg - value to pass to parse_routine
*/
static struct {
	char *name;
	uint32_t len;
	int have_colon;
	int remove_spaces_after_colon;
	int (*parse_routine)(int arg, char *lptr,
		session_desc_t *sptr, media_desc_t *mptr);
	int arg;
} a_types[] =
{
	{ "rtpmap", sizeof("rtpmap"), TRUE, TRUE, sdp_decode_parse_a_rtpmap, 0 },
	{ "cat", sizeof("cat"), TRUE, TRUE, sdp_decode_parse_a_cat, 0 },
	{ "fmtp", sizeof("fmtp"), TRUE, TRUE, sdp_decode_parse_a_fmtp, 0 },
	{ "keywds", sizeof("keywds"), TRUE, FALSE, sdp_decode_parse_a_str, 0},
	{ "tool", sizeof("tool"), TRUE, TRUE, sdp_decode_parse_a_str, 1},
	{ "charset", sizeof("charset"), TRUE, TRUE, sdp_decode_parse_a_str, 2},
	{ "sdplang", sizeof("sdplang"), TRUE, TRUE, sdp_decode_parse_a_str, 3},
	{ "lang", sizeof("lang"), TRUE, TRUE, sdp_decode_parse_a_str, 4},
	{ "type", sizeof("type"), TRUE, TRUE, sdp_decode_parse_a_str, 5},
	{ "orient", sizeof("orient"), TRUE, TRUE, sdp_decode_parse_a_str, 6},
	{ "control", sizeof("control"), TRUE, TRUE, sdp_decode_parse_a_str, 7},
	{ "etag", sizeof("etag"), TRUE, TRUE, sdp_decode_parse_a_str, 8},
	{ "recvonly", sizeof("recvonly"), FALSE, FALSE, sdp_decode_parse_a_bool, 0},
	{ "sendrecv", sizeof("sendrecv"), FALSE, FALSE, sdp_decode_parse_a_bool, 1},
	{ "sendonly", sizeof("sendonly"), FALSE, FALSE, sdp_decode_parse_a_bool, 2},
	{ "ptime", sizeof("ptime"), TRUE, TRUE, sdp_decode_parse_a_int, 0 },
	{ "quality", sizeof("quality"), TRUE, TRUE, sdp_decode_parse_a_int, 1},
	{ "framerate", sizeof("framerate"), TRUE, TRUE, sdp_decode_parse_a_frame, 0},
	{ "range", sizeof("range"), TRUE, TRUE, sdp_decode_parse_a_range, 0 },
	{ NULL, 0, FALSE, FALSE, NULL },
};

/*
* sdp_decode_parse_a()
* decodes a= lines, or stores the complete string in media or session
* unparsed_a_lines list.
*/
static int sdp_decode_parse_a (char *lptr,
							   char *line,
							   session_desc_t *sptr,
							   media_desc_t *mptr)
{
	int ix;
	int errret;
	int parsed;
	char *after;

	ix = 0;
	errret = 0;
	parsed = FALSE;
	/*
	* go through above array, looking for a complete match
	*/
	while (a_types[ix].name != NULL) {
		if (strncmp(lptr,
			a_types[ix].name,
			a_types[ix].len - 1) == 0) {
				after = lptr + a_types[ix].len - 1;
				if (!(isspace(*after) ||
					*after == ':' ||
					*after == '\0')) {
						/* partial match - not good enough*/
						continue;
				}

				parsed = TRUE;

				/*
				* Have a match.  If specified, look for colon, and remove space
				* after colon
				*/
				if (a_types[ix].have_colon) {
					ADV_SPACE(after);
					if (*after != ':') {
						errret = ESDP_ATTRIBUTES_NO_COLON;
						break;
					}
					after++;
					if (a_types[ix].remove_spaces_after_colon) {
						ADV_SPACE(after);
					}
				}
				/*
				* Call the correct parsing routine
				*/
				errret = (a_types[ix].parse_routine)(a_types[ix].arg, after, sptr, mptr);
				break;
		}
		ix++;
	}
	/*
	* Worse comes to worst, store the whole line
	*/
	if (parsed == FALSE || errret != 0) {
		if (sdp_add_string_to_list(mptr == NULL ?
			&sptr->unparsed_a_lines :
		&mptr->unparsed_a_lines,
			line) == FALSE) {
				return (ENOMEM);
		}
	}
	return (0);
}

/*
* sdp_decode_parse_bandwidth()
* parses b=<modifier>:<value>
* Inputs: lptr - pointer to line, bptr - pointer to store in
* Outputs: TRUE - valid, FALSE, invalid
*/
static int sdp_decode_parse_bandwidth (char *lptr,
									   bandwidth_t **bptr)
{
	char *cptr, *endptr;
	bandwidth_t *new, *p;
	bandwidth_modifier_t modifier;
	uint32_t temp;
	cptr = strchr(lptr, ':');
	if (cptr == NULL) {
		printf( "No colon in bandwidth\n");
		return (ESDP_BANDWIDTH);
	}
	*cptr++ = '\0';

	if (strncmp(lptr, "as", strlen("as")) == 0) {
		modifier = BANDWIDTH_MODIFIER_AS;
	} else if (strncmp(lptr, "ct", strlen("ct")) == 0) {
		modifier = BANDWIDTH_MODIFIER_CT;
	} else {
		modifier = BANDWIDTH_MODIFIER_USER;
		endptr = cptr - 2;
		while (isspace(*endptr) && endptr >lptr) *endptr-- = '\0';
	}

	if (*cptr == '\0') {
		printf( "No bandwidth in bandwidth\n");
		return (ESDP_BANDWIDTH);
	}
	temp = (uint32_t)strtoul(cptr, &endptr, 10);
	if (*endptr != '\0') {
		if(*endptr=='.')
		{
			temp++;
		}
		else
		{
			printf( "Error in decoding bandwidth value %s\n", cptr);
			return (ESDP_BANDWIDTH);
		}
	}

	new = malloc(sizeof(bandwidth_t));
	if (new == NULL) {
		return (ENOMEM);
	}
	new->modifier = modifier;
	if (modifier == BANDWIDTH_MODIFIER_USER) {
		new->user_band = strdup(lptr);
		if (new->user_band == NULL) {
			free(new);
			return (ENOMEM);
		}
	} else {
		new->user_band = NULL;
	}
	new->bandwidth = temp;
	new->next = NULL;
	if (*bptr == NULL) {
		*bptr = new;
	} else {
		p = *bptr;
		while (p->next != NULL) p = p->next;
		p->next = new;
	}
	return (0);
}

/*
* sdp_decode_parse_connect()
* parse c=<network type> <address type> <connect address>
* Inputs: lptr, connect pointer
* Outputs - error code or 0 if parsed correctly
*/
static int sdp_decode_parse_connect (char *lptr, connect_desc_t *cptr)
{
	char *sep, *beg;

	cptr->ttl = 0;
	cptr->num_addr = 0;

	/* <network type> should be IN*/
	sep = strsep(&lptr, SPACES);
	if (sep == NULL ||
		lptr == NULL ||
		strcmp(sep, "IN") != 0) {
			printf( "IN statement missing from c\n");
			return (ESDP_CONNECT);
	}

	/* <address type> - should be IPV4*/
	ADV_SPACE(lptr);
	sep = strsep(&lptr, SPACES);
	if (sep == NULL || lptr == NULL) {
		printf( "No connection type in c=\n");
		return (ESDP_CONNECT);
	}
	cptr->conn_type = strdup(sep);

	/* Address - first look if we have a / - that indicates multicast, and a
	ttl.*/
	ADV_SPACE(lptr);
	sep = strchr(lptr, '/');
	if (sep == NULL) {
		/* unicast address*/
		cptr->conn_addr = strdup(lptr);
		cptr->used = TRUE;
		return (0);
	}

	/* Okay - multicast address.  Take address up to / (get rid of trailing
	spaces)*/
	beg = lptr;
	lptr = sep + 1;
	sep--;
	while (isspace(*sep) && sep > beg) sep--;
	sep++;
	*sep = '\0';
	cptr->conn_addr = strdup(beg);

	/* Now grab the ttl*/
	ADV_SPACE(lptr);
	sep = strsep(&lptr, " \t/");
	if (!isdigit(*sep)) {
		free_connect_desc(cptr);
		return (ESDP_CONNECT);
	}
	sscanf(sep, "%u", &cptr->ttl);
	/* And see if we have a number of ports*/
	if (lptr != NULL) {
		/* we have a number of ports, as well*/
		ADV_SPACE(lptr);
		if (!isdigit(*lptr)) {
			free_connect_desc(cptr);
			return (ESDP_CONNECT);
		}
		sscanf(lptr, "%u", &cptr->num_addr);
	}
	cptr->used = TRUE;
	return (0);
}


/*
* sdp_decode_parse_key()
*/
static int sdp_decode_parse_key (char *lptr, key_desc_t *kptr)
{
	if (strncmp(lptr, "prompt", strlen("prompt")) == 0) {
		/* handle prompt command*/
		kptr->key_type = KEY_TYPE_PROMPT;
		return (0);
	}
	if (strncmp(lptr, "clear", strlen("clear")) == 0) {
		kptr->key_type = KEY_TYPE_CLEAR;
		lptr += strlen("clear");
	} else if (strncmp(lptr, "base64", strlen("base64")) == 0) {
		kptr->key_type = KEY_TYPE_BASE64;
		lptr += strlen("base64");
	} else if (strncmp(lptr, "uri", strlen("uri")) == 0) {
		kptr->key_type = KEY_TYPE_URI;
		lptr += strlen("uri");
	} else {
		printf( "key statement keyword error %s\n", lptr);
		return (ESDP_KEY);
	}
	ADV_SPACE(lptr);
	if (*lptr != ':') {
		return (ESDP_KEY);
	}
	lptr++;
	/* Because most of the types can have spaces, we take everything after
	the colon here.  To eliminate the whitespace, use ADV_SPACE(lptr);*/
	kptr->key = strdup(lptr);
	return (0);
}


/*
* sdp_decode_parse_media()
* decodes m= lines.
* m=<media> <port>[/<numport>] <proto> <fmt list>
* Inputs:
*   lptr - pointer to line
*   sptr - pointer to session description to modify
* Outputs:
*   pointer to new media description
*/
static media_desc_t *sdp_decode_parse_media (char *lptr,
											 session_desc_t *sptr,
											 int *err)
{
	char *mdesc, *proto, *sep;
	media_desc_t *new, *mp;
	uint32_t read, port_no;
	//string_list_t *q;

	*err = 0;
	/* m=<media> <port> <transport> <fmt list>*/
	mdesc = strsep(&lptr, SPACES);
	if (mdesc == NULL || lptr == NULL) {
		printf( "No media type\n");
		*err = ESDP_MEDIA;
		return (NULL);
	}

	/* <port>*/
	ADV_SPACE(lptr);
	read = 0;
	if (!isdigit(*lptr)) {
		printf( "Illegal port number in media %s\n", lptr);
		*err = ESDP_MEDIA;
		return (NULL);
	}
	while (isdigit(*lptr)) {
		read *= 10;
		read += *lptr - '0';
		lptr++;
	}
	ADV_SPACE(lptr);

	/* number of ports (optional)*/
	if (*lptr == '/') {
		lptr++;
		ADV_SPACE(lptr);
		if (!isdigit(*lptr)) {
			printf( "Illegal port number in media %s\n", lptr);
			*err = ESDP_MEDIA;
			return (NULL);
		}
		sep = strsep(&lptr, SPACES);
		if (lptr == NULL) {
			printf( "Missing keywords in media\n");
			*err = ESDP_MEDIA;
			return (NULL);
		}
		sscanf(sep, "%u", &port_no);
		ADV_SPACE(lptr);
	} else {
		port_no = 0;
	}

	/* <transport> (protocol)*/
	proto = strsep(&lptr, SPACES);
	if (proto == NULL || lptr == NULL) {
		printf( "No transport in media\n");
		*err = ESDP_MEDIA;
		return (NULL);
	}
	ADV_SPACE(lptr);
	if (!isalnum(*lptr)) {
		*err = ESDP_MEDIA;
		return (NULL);
	}

	/* malloc memory and set.*/
	new = malloc(sizeof(media_desc_t));
	if (new == NULL) {
		*err = ENOMEM;
		return (NULL);
	}
	memset(new, 0, sizeof(media_desc_t));
	new->media = strdup(mdesc);
	new->port = (uint16_t)read;
	new->proto = strdup(proto);
	new->num_ports = (unsigned short)port_no;

	/* parse format list - these are not necessarilly lists of numbers
	so we store as strings.*/
	//q = NULL;
	do {
		sep = strsep(&lptr, SPACES);
		if (sep != NULL) {
			if (sdp_add_format_to_list(new, sep) == NULL) {
				free_media_desc(new);
				*err = ENOMEM;
				return (NULL);
			}
			if (lptr != NULL) {
				ADV_SPACE(lptr);
			}
		}
	} while (sep != NULL);

	new->parent = sptr;
	/* Add to list of media*/
	if (sptr->media == NULL) {
		sptr->media = new;
	} else {
		mp = sptr->media;
		while (mp->next != NULL) mp = mp->next;
		mp->next = new;
	}

	return (new);
}

/*
* sdp_decode_parse_origin()
* parses o= line
* o=<username> <session id> <version> <network type> <addr type> <addr>
*
* Inputs:
*   lptr - pointer to line to parse
*   sptr - session desc
* Output - TRUE, valid, FALSE, invalid
*/
static int sdp_decode_parse_origin (char *lptr, session_desc_t *sptr)
{
	char *username, *sep;

	/* Username - leave null if "-"*/
	username = strsep(&lptr, SPACES);
	if (username == NULL || lptr == NULL) {
		printf( "o=: no username\n");
		return (ESDP_ORIGIN);
	}
	ADV_SPACE(lptr);
	if (strcmp(username, "-") != 0) {
		sptr->orig_username = strdup(username);
	}

	if (strtou64(&lptr, &sptr->session_id) == FALSE) {
		printf( "Non-numeric session id\n");
		return (ESDP_ORIGIN);
	}

	if (strtou64(&lptr, &sptr->session_version) == FALSE) {
		printf( "Non-numeric session version\n");
		return (ESDP_ORIGIN);
	}

	ADV_SPACE(lptr);
	sep = strsep(&lptr, SPACES);
	if ((sep == NULL) ||
		(lptr == NULL) ||
		(strcmp(sep, "IN") != 0)) {
			printf( "o=: no IN statement\n");
			return (ESDP_ORIGIN);
	}

	ADV_SPACE(lptr);
	sep = strsep(&lptr, SPACES);
	if (sep == NULL || lptr == NULL) {
		printf( "o=: No creation address type\n");
		return (ESDP_ORIGIN);
	}
	sptr->create_addr_type = strdup(sep);

	ADV_SPACE(lptr);
	sep = strsep(&lptr, SPACES);
	if (sep == NULL) {
		printf( "o=: No creation address\n");
		return (ESDP_ORIGIN);
	}
	sptr->create_addr = strdup(sep);

	return (0);
}

/*
* sdp_decode_parse_time()
* decode t= statements
*
* Inputs:
*   sptr - pointer to session_desc_t to write into.
*   lptr - pointer to line.  Should point at first number (spaces removed)
*
* Outputs:
*   pointer to session_time_desc_t to use as current one.
*   NULL if string invalid
*/
static session_time_desc_t *sdp_decode_parse_time (char *lptr,
												   session_desc_t *sptr,
												   int *err)
{
	session_time_desc_t *tptr;
	uint32_t start, end;

	*err = 0;
	if (!isdigit(*lptr)) {
		*err = ESDP_TIME;
		printf( "t= statement has illegal first character %c\n", *lptr);
		return (NULL);
	}

	start = end = 0;

	while (isdigit(*lptr)) {
		start *= 10;
		start += *lptr - '0';
		lptr++;
	}

	ADV_SPACE(lptr);

	if (!isdigit(*lptr)) {
		printf("t= statement has illegal character after 1st number %c\n",*lptr);
		*err = ESDP_TIME;
		return (NULL);
	}

	while (isdigit(*lptr)) {
		end *= 10;
		end += *lptr - '0';
		lptr++;
	}

	tptr = malloc(sizeof(session_time_desc_t));
	if (tptr == NULL) {
		*err = ENOMEM;
		return (NULL);
	}

	tptr->next = NULL;
	tptr->repeat = NULL;

	/* Convert from NTP time to Unix time (unless time is 0)*/
	tptr->start_time = (start == 0) ? 0 : (start - NTP_TO_UNIX_TIME);
	tptr->end_time = (end == 0) ? 0 : (end - NTP_TO_UNIX_TIME);

	/* Add to end of linked list of time descriptions*/
	if (sptr->time_desc == NULL) {
		sptr->time_desc = tptr;
	} else {
		session_time_desc_t *q;
		q = sptr->time_desc;
		while (q->next != NULL) q = q->next;
		q->next = tptr;
	}

	return (tptr);

}

/*
* sdp_decode_parse_time_adj()
* decode z= fields.
* Inputs: lptr - pointer to line to decode
*   session - pointer to session_descriptor to add
* Returns:
*   TRUE - valid line
*   FALSE - invalid line
*/
static int sdp_decode_parse_time_adj (char *lptr,
									  session_desc_t *session)
{
	char *sep;
	int valid;
	time_adj_desc_t *start_aptr, *aptr;
	time_t adj_time;
	unsigned int offset;
	int possign;
	int err;

	if (!isdigit(*lptr)) {
		printf( "Illegal character for z= field %s\n", lptr);
		return (ESDP_TIME_ADJ);
	}

	start_aptr = NULL;
	valid = TRUE;
	/*
	* parse pairs of <adjustment time> <offset>
	*/
	err = ESDP_TIME_ADJ;

	while (NULL!=(sep = strsep(&lptr, SPACES)) && valid == TRUE) {
		if (lptr == NULL) {
			valid = FALSE;
			continue;
		}
		/* process <adjustment time> - adjust it from NTP to unix time*/
		sscanf(sep, "%ld", &adj_time);

		/* Check for negative sign for offset.*/
		ADV_SPACE(lptr);
		if (*lptr == '-') {
			possign = FALSE;
			lptr++;
		} else possign = TRUE;
		ADV_SPACE(lptr);

		sep = strsep(&lptr, SPACES);
		if (adj_time == 0 || sep == NULL) {
			valid = FALSE;
			continue;
		}
		adj_time -= NTP_TO_UNIX_TIME;

		/* Process offset - could be positive or negative.*/
		if (str_to_time_offset(sep, &offset) == FALSE) {
			valid = FALSE;
			continue;
		}

		if (possign == FALSE) offset = 0 - offset;

		/*
		* create a time structure - order the link list by value
		* of adj_times
		*/
		aptr = malloc(sizeof(time_adj_desc_t));
		if (aptr == NULL) {
			valid = FALSE;
			err = ENOMEM;
			continue;
		}

		aptr->next = NULL;
		aptr->adj_time = adj_time;
		aptr->offset = offset;
		if (lptr != NULL) {
			ADV_SPACE(lptr);
		}

		start_aptr = time_adj_order_in_list(start_aptr, aptr);

	}

	if (valid == FALSE) {
		while (start_aptr != NULL) {
			aptr = start_aptr;
			start_aptr = aptr->next;
			free(aptr);
		}
		return (err);
	}

	if (start_aptr == NULL) return (err);

	while (start_aptr != NULL) {
		aptr = start_aptr->next;
		start_aptr = aptr->next;
		aptr->next = NULL;
		session->time_adj_desc = time_adj_order_in_list(session->time_adj_desc,
			aptr);
	}

	return (0);
}


/*
* sdp_decode_parse_time_repeat
* parse "r=" field in SDP description, store info off of current time
* pointer
*
* Inputs : lptr - pointer to decode buffer
*    current_time - current time pointer returned by sdp_decode_parse_time()
*
* Outputs - TRUE - decoded successfully - FALSE - error
*/
static int sdp_decode_parse_time_repeat (char *lptr,
										 session_time_desc_t *current_time)
{
	time_repeat_desc_t *rptr;
	char *sep;
	uint32_t interval, duration;

	if (current_time == NULL) {
		printf( "r= before or without time\n");
		return (ESDP_REPEAT_NOTIME);
	}

	sep = strsep(&lptr, SPACES);
	if (sep == NULL || lptr == NULL) {
		printf( "Interval not found in repeat statment\n");
		return (ESDP_REPEAT);
	}

	if (str_to_time_offset(sep, &interval) == FALSE) {
		printf( "Illegal string conversion in repeat\n");
		return (ESDP_REPEAT);
	}

	ADV_SPACE(lptr);
	sep = strsep(&lptr, SPACES);
	if (sep == NULL || lptr == NULL) {
		printf( "No duration in repeat statement\n");
		return (ESDP_REPEAT);
	}

	if (str_to_time_offset(sep, &duration) == FALSE) {
		return (ESDP_REPEAT);
	}

	if (duration == 0 || interval == 0) {
		printf( "duration or interval are 0 in repeat\n");
		return (ESDP_REPEAT);
	}

	ADV_SPACE(lptr);

	rptr = malloc(sizeof(time_repeat_desc_t));
	if (rptr == NULL)
		return (ENOMEM);

	rptr->next = NULL;
	rptr->offset_cnt = 0;
	rptr->repeat_interval = interval;
	rptr->active_duration = duration;

	/* Read offset fields - we set a maximum of 16 here.*/
	while (NULL!=(sep = strsep(&lptr, SPACES)) &&
		rptr->offset_cnt < MAX_REPEAT_OFFSETS) {
			if (str_to_time_offset(sep, &rptr->offsets[rptr->offset_cnt]) == FALSE) {
				printf( "Illegal repeat offset - number %d\n", rptr->offset_cnt);
				free(rptr);
				return (ESDP_REPEAT);
			}
			rptr->offset_cnt++;
			if (lptr != NULL) {
				ADV_SPACE(lptr);
			}
	}

	if (rptr->offset_cnt == 0 || sep != NULL) {
		printf( "No listed offset in repeat\n");
		free(rptr);
		return (ESDP_REPEAT);
	}

	if (current_time->repeat == NULL) {
		current_time->repeat = rptr;
	} else {
		time_repeat_desc_t *q;
		q = current_time->repeat;
		while (q->next != NULL) q = q->next;
		q->next = rptr;
	}
	return (0);
}

/*****************************************************************************
* Main library routines
*****************************************************************************/

/*
* sdp_decode()
* main routine
* Inputs:
*   decode - pointer to sdp_decode_info_t set before calling.
*   retlist - pointer to pointer of list head
*   translated - pointer to return number translated
* Outputs:
*   error code or 0.  Negative error codes are local, positive are errorno.
*   retlist - pointer to list of session_desc_t.
*   translated - number translated.
*/
int sdp_decode (sdp_decode_info_t *decode,
				session_desc_t **retlist,
				int *translated)
{
	char *line, *lptr;
	char code;
	int errret;
	uint32_t linelen;

	session_desc_t *first_session,*sptr;
	media_desc_t *current_media;
	session_time_desc_t *current_time;
	if ((decode == NULL) || (decode->isSet != TRUE)) {
		return EINVAL;
	}

	*retlist = first_session = NULL;
	sptr = NULL;
	current_media = NULL;
	current_time = NULL;
	*translated = 0;
	errret = 0;
	line = NULL;
	linelen = 0;
	while (errret == 0 && get_next_line(&line, decode, &linelen) != FALSE) {
		lptr = line;
		ADV_SPACE(lptr);
		/*
		* All spaces ?  Just go to next line.
		*/
		if (*lptr == '\0')
			continue;


		/*
		* Let's be strict about 1 character code
		*/
		code = *lptr++;
		ADV_SPACE(lptr);
		if (*lptr == '=') {
			lptr++;
			ADV_SPACE(lptr);
			if ((sptr == NULL) && (tolower(code) != 'v')) {
				printf( "Version not 1st statement\n");
				errret = ESDP_INVVER;
				break;
			}
			switch (tolower(code)) {
			  case 'v': {
				  int ver;
				  if ((sscanf(lptr, "%u", &ver) != 1) ||
					  (ver != 0)) {
						  errret = ESDP_INVVER;
						  //printf( "SDP Version not correct, %s\n", line);
						  break;
				  }
				  /*
				  * Next session...
				  */
				  sptr = malloc(sizeof(session_desc_t));
				  if (sptr == NULL) {
					  errret = ENOMEM;
					  break;
				  }
				  memset(sptr, 0, sizeof(session_desc_t));
				  if (first_session == NULL) {
					  *retlist = first_session = sptr;
				  } else {
					  session_desc_t *p;
					  p = first_session;
					  while (p->next != NULL) p = p->next;
					  p->next = sptr;
				  }
				  *translated = *translated + 1;
				  current_media = NULL;
				  current_time = NULL;
				  break;
						}
			  case 'o':
				  errret = sdp_decode_parse_origin(lptr, sptr);
				  break;
			  case 's':
				  sptr->session_name = strdup(lptr);
				  if (sptr->session_name == NULL) {
					  errret = ENOMEM;
				  }
				  break;
			  case 'i':
				  if (current_media != NULL) {
					  current_media->media_desc = strdup(lptr);
					  if (current_media->media_desc == NULL) {
						  errret = ENOMEM;
					  }
				  } else {
					  sptr->session_desc = strdup(lptr);
					  if (sptr->session_desc == NULL) {
						  errret = ENOMEM;
					  }
				  }
				  break;
			  case 'u':
				  sptr->uri = strdup(lptr);
				  if (sptr->uri == NULL) {
					  errret = ENOMEM;
				  }
				  break;
			  case 'e':
				  if (sdp_add_string_to_list(&sptr->admin_email, lptr) == FALSE) {
					  errret = ENOMEM;
				  }
				  break;
			  case 'p':
				  if (sdp_add_string_to_list(&sptr->admin_phone, lptr) == FALSE) {
					  errret = ENOMEM;
				  }
				  break;
			  case 'c':
				  errret = sdp_decode_parse_connect(lptr,
					  current_media ?
					  &current_media->media_connect :
				  &sptr->session_connect);
				  break;
			  case 'b':
				  errret= sdp_decode_parse_bandwidth(lptr,
					  current_media != NULL ?
					  &current_media->media_bandwidth :
				  &sptr->session_bandwidth);
				  break;
			  case 't':
				  current_time = sdp_decode_parse_time(lptr, sptr, &errret);
				  if (current_time == NULL) {
					  errret = ESDP_TIME;
				  }
				  break;
			  case 'r':
				  if (current_time != NULL) {
					  errret = sdp_decode_parse_time_repeat(lptr, current_time);
				  }
				  break;
			  case 'z':
				  errret = sdp_decode_parse_time_adj(lptr, sptr);
				  break;
			  case 'k':
				  errret = sdp_decode_parse_key(lptr,
					  current_media == NULL ? &sptr->key :
					  &current_media->key);
				  break;
			  case 'a':
				  errret = sdp_decode_parse_a(lptr, line, sptr, current_media);
				  break;
			  case 'm':
				  current_media = sdp_decode_parse_media(lptr, sptr, &errret);
				  break;
			  default:
				  printf( "unknown code - %s\n", line);
				  errret = ESDP_UNKNOWN_LINE;
			}
		} else {
			/* bigger than 1 character code*/
			errret = ESDP_UNKNOWN_LINE;
		}
	}


	if (line != NULL) {
		free(line);
	}

	if (errret != 0) {
		if (sptr != NULL) {
			if (first_session == sptr) {
				*retlist = NULL;
			} else {
				session_desc_t *p;
				p = first_session;
				while (p->next != sptr) {
					p = p->next;
				}
				p->next = NULL;
			}
			sdp_free_session_desc(sptr);
		}
		return (errret);
	}
	return (0);
}

/*
* set_sdp_decode_from_memory()
* Allows sdp decode to be run from a memory block.
*
* Inputs: memptr - pointer to memory.  Won't be touched.
* Outputs: pointer to sdp_decode_info_t to be used.
*/
sdp_decode_info_t *set_sdp_decode_from_memory (const char *memptr)
{
	sdp_decode_info_t *decode_ptr;

	decode_ptr = malloc(sizeof(sdp_decode_info_t));
	if (decode_ptr == NULL)
		return (NULL);

	memset(decode_ptr, 0, sizeof(sdp_decode_info_t));

	decode_ptr->isSet = TRUE;
	decode_ptr->isMem = TRUE;
	decode_ptr->memptr = memptr;
	decode_ptr->filename = NULL;
	decode_ptr->ifile = NULL;
	return (decode_ptr);
}

/*
* set_sdp_decode_from_filename()
* Allows sdp decode to be run on a file
*
* Inputs: filename - name of file to open
* Outputs: pointer to sdp_decode_info_t to be used.
*/
sdp_decode_info_t *set_sdp_decode_from_filename (const char *filename)
{
	sdp_decode_info_t *decode_ptr;

	decode_ptr = malloc(sizeof(sdp_decode_info_t));
	if (decode_ptr == NULL)
		return (NULL);

	memset(decode_ptr, 0, sizeof(sdp_decode_info_t));
	decode_ptr->isSet = TRUE;
	decode_ptr->isMem = FALSE;
	decode_ptr->memptr = NULL;
	decode_ptr->filename = filename;
	decode_ptr->ifile = fopen(filename, "r");
	if (decode_ptr->ifile == NULL) {
		free(decode_ptr);
		return (NULL);
	}

	return (decode_ptr);
}

void sdp_decode_info_free (sdp_decode_info_t *decode)
{
	if(NULL == decode) return;
	if ( NULL != decode->ifile) {
		fclose(decode->ifile);
		decode->ifile = NULL;
	}
	free(decode);
}

