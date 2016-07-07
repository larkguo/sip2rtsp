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

#ifndef __RTSP_CLIENT_H__
#define __RTSP_CLIENT_H__

#include "rtsp.h"
#include "rtsp_auth.h"
#include "transport_parse.h"
#include "core.h"


int rtsp_open(core *co, int call_id,char *video_host, uint16_t video_port, 
	char *audio_host, uint16_t audio_port , 
	rtsp_transport_parse_t *video_transport,rtsp_transport_parse_t *audio_transport,
	char *sdp_buff, int sdp_buff_len, int *status);

int rtsp_play(core *co);
int rtsp_pause(core *co);
int rtsp_stop(core *co);
int rtsp_getparam(core *co);

int rtsp_sessiontimeout_set(int timeout);
int rtsp_sessiontimeout_get(int *timeout);
void rtsp_automatic_action(core *co);

#endif

