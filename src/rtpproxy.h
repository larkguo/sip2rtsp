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
 
#ifndef __RTPPROXY_H__
#define __RTPPROXY_H__

#include "sip.h"
#include "log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rtp_header_t{
#if BYTE_ORDER == BIG_ENDIAN
	uint16_t version:2;
	uint16_t padbit:1;
	uint16_t extbit:1;
	uint16_t cc:4;
	uint16_t markbit:1;
	uint16_t payload_type:7;
#else
	uint16_t cc:4;
	uint16_t extbit:1;
	uint16_t padbit:1;
	uint16_t version:2;
	uint16_t payload_type:7;
	uint16_t markbit:1;
#endif
	uint16_t seq_number;
	uint32_t timestamp;
	uint32_t ssrc;
	uint32_t csrc[16];
} rtp_header;

int payload_init(core *co);
int streams_init(core *co);
int streams_loop(core *co);
int streams_stop(core *co);
int stream_call_stop(core *co, int callid);
int sock_pair_create(core*co,int callid,stream_mode mode,b2b_side side);

#ifdef __cplusplus
}
#endif

#endif

