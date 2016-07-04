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
 
#include <time.h>
#include "rtsp_client.h"
#include "rtpproxy.h"

static int sock_address_get(int socket, char *ipbuf, int ipbuf_len, int *port);
static int sock_create(core*co, stream_mode mode, b2b_side side);
static int pairsock_create(core*co, stream_mode mode, b2b_side side,int *port);

static int 
nonblocking_set(int fd)
{
	int flags;

	/* If they have O_NONBLOCK, use the Posix way to do it */
#ifndef WIN32
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
		flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctlsocket(fd, FIONBIO, &flags);
#endif
}

int 
payload_init(core *co)
{
	int i, j;
	for (i = 0; i < stream_max; i++) {
		for (j = 0; j < side_max; j++) {
			/* payload */
			memset(&co->b2bstreams[i].payload[j],0,sizeof(co->b2bstreams[i].payload[j]));
			co->b2bstreams[i].payload[j].media_format = -1;
		}
	}
	return 0;
}

static int 
sock_address_get(int sock, char *ipbuf, int ipbuf_len, int *port)
{
	int ret = 0;
	struct sockaddr_in addr;
	socklen_t slen = sizeof(addr);
	
	ret = getsockname(sock, (struct sockaddr *)&addr, &slen);
	if( 0 == ret){
		switch (((struct sockaddr *)&addr)->sa_family) {
		case AF_INET:
			if( NULL != ipbuf && ipbuf_len > 0){
				inet_ntop(((struct sockaddr *)&addr)->sa_family,
					&(((struct sockaddr_in *) &addr)->sin_addr),ipbuf,sizeof(ipbuf));
			}
			if( NULL != port ){
				*port = ntohs(((struct sockaddr_in *) &addr)->sin_port);
			}
			break;
		case AF_INET6:
			if( NULL != ipbuf && ipbuf_len > 0){
				inet_ntop(((struct sockaddr *)&addr)->sa_family,
					&(((struct sockaddr_in6 *)&addr)->sin6_addr),ipbuf,sizeof(ipbuf));
			}
			if( NULL != port ){
				*port = ntohs(((struct sockaddr_in6 *)&addr)->sin6_port);
			}
			break;
		default:
			break;
		}
	}
	return ret;
}

static int 
sock_create(core *co, stream_mode mode, b2b_side side)
{
	int sock = -1;
	int ret = -1;
	int port = 0;
	
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	
	memset(&co->b2bstreams[mode].remote[side],0,
		sizeof(co->b2bstreams[mode].remote[side]));
	co->b2bstreams[mode].local[side].sin_family      = AF_INET;

	if(side_sip == side){
		co->b2bstreams[mode].local[side].sin_addr.s_addr = inet_addr(co->sip_localip);
	}
	if(side_rtsp == side){
		co->b2bstreams[mode].local[side].sin_addr.s_addr = inet_addr(co->rtsp_localip);
	}
			
	sock =  socket(AF_INET, SOCK_DGRAM, 0);
	ret = bind(sock,(struct sockaddr *)&co->b2bstreams[mode].local[side], 
		sizeof(co->b2bstreams[mode].local[side]));	
	if( ret < 0 ){
		log(co,LOG_DEBUG,"rtpproxy bind(%s:%d)=%d failed!\n",
			co->sip_localip,ntohs(co->b2bstreams[mode].local[side].sin_port),ret);

		return -1;
	}
	co->b2bstreams[mode].fds[side] = sock;
	ret = sock_address_get(sock,NULL, 0, &port);
	if( 0 == ret ){
		co->b2bstreams[mode].local[side].sin_port = htons(port);
	}
	if( co->b2bstreams[mode].fds[side]  >  co->maxfd){
		co->maxfd = co->b2bstreams[mode].fds[side];
	}
			
	nonblocking_set(co->b2bstreams[mode].fds[side]);
	return sock;
}

static int 
pairsock_create(core *co, stream_mode mode, b2b_side side,int *port)
{
	int rtp_sock = -1;
	int rtcp_sock = -1;
	int rtp_port = *port;
	int start_port = core_rtp_start_port_get(co);
	int end_port = core_rtp_end_port_get(co);
	int try_times = 0;
		
	if(end_port < (start_port+8) )
		end_port = start_port+8;
	
try_nextport:		
	if(try_times > 3 ) {
		log(co,LOG_ERR,"bind error times %d, start_port=%d\n", try_times, start_port);
		return -1;
	}
	
	if( rtp_port < start_port || rtp_port > end_port )
		rtp_port = start_port;
	
	co->b2bstreams[mode].local[side].sin_port	= htons(rtp_port);
	co->b2bstreams[mode+1].local[side].sin_port = htons(rtp_port+1);
	rtp_sock = sock_create(co, mode,side);
	rtcp_sock = sock_create(co, mode+1,side);
	if( rtp_sock <= 0 || rtcp_sock <= 0 ){
		if( rtp_sock > 0)  close(rtp_sock);
		if( rtcp_sock > 0)  close(rtcp_sock);
		rtp_port += 2;
		try_times += 1;
		goto try_nextport;
	}
	
	*port = rtp_port+2;
	return 0;
	
}

int 
streams_init(core *co)
{
	int rtpproxy = core_rtpproxy_get(co);
	int rtp_port = core_rtp_start_port_get(co);
	int ret = 0;
	
	if(0 == rtpproxy)
		return 0;

	/* create rtp and rtcp */
	ret = pairsock_create(co, stream_audio_rtp, side_sip, &rtp_port);
	if(0==ret) ret = pairsock_create(co, stream_video_rtp, side_sip, &rtp_port);
	if(0==ret) ret = pairsock_create(co, stream_audio_rtp, side_rtsp, &rtp_port);
	if(0==ret) ret = pairsock_create(co, stream_video_rtp, side_rtsp, &rtp_port);
	if( ret < 0) {
		return ret;
	}
	
	payload_init(co);
	core_show(co);
	return 0;
}

int 
streams_stop(core *co)
{
	int i, j ;
	int fd;
	int rtpproxy = core_rtpproxy_get(co);
	
	if(0 == rtpproxy)
		return 0;
	
	for (i = 0; i < stream_max; i++) {
		for (j = 0; j < side_max; j++) {
			fd = co->b2bstreams[i].fds[j];
			if(fd >= 0) {
				close(fd);
				co->b2bstreams[i].fds[j] = -1;
			}
		}
	}
	return 0;
}

int streams_loop(core *co)
{
	int					fd,nready;
	fd_set				readset; 
	ssize_t				recvlen;
	int			slen;
	struct timeval tv;
	int i, j ;
	
	tv.tv_sec = 0;
	tv.tv_usec = 10000; 
	FD_ZERO(&readset);

	for (i = 0; i < stream_max; i++) {
		for (j = 0; j < side_max; j++) {
			fd = co->b2bstreams[i].fds[j];
			if( fd > 0) FD_SET(fd, &readset);
		}
	}
	
	nready = select(co->maxfd, &readset, NULL, NULL, &tv);
	if( nready <= 0)
		return 0;
	
	for (i = 0; i < stream_max; i++) {
		for (j = 0; j < side_max; j++) {
			fd = co->b2bstreams[i].fds[j];
			if (FD_ISSET(fd, &readset)) {
				char		buf[RECV_BUFF_DEFAULT_LEN];
				int k =  (j== side_sip) ?  side_rtsp : side_sip;
				int to_payloadtype = -1;
				int from_payloadtype = -1;
				int from_version = -1;
				rtp_header *rtp = NULL;
				struct sockaddr_storage sa;
			
				slen = sizeof(struct sockaddr_in);
				
				/* symmetricRTP */
				if(co->symmetric_rtp != 0){
					recvlen = recvfrom(fd,buf,sizeof(buf),0,
						(struct sockaddr *)&co->b2bstreams[i].remote[j],(socklen_t *)&slen); 
				}else{
					recvlen = recvfrom(fd,buf,sizeof(buf),0,
						(struct sockaddr *)&sa,(socklen_t *)&slen); 
				}
				
				/* rtcp */
				if(stream_audio_rtcp == i || stream_video_rtcp == i)
					goto b2bsend;
				
				if( recvlen <= 12 ){ /* rtp header len > 12 */
					goto b2bsend;
				}

				rtp= (rtp_header*)buf;
				
				/* Check RTP version */
				from_version = rtp->version;
				if( 2 != from_version ) {
					goto b2bsend;
				}
				
				/* Check RTP payloadType */
				from_payloadtype = rtp->payload_type;
				if( from_payloadtype!=co->b2bstreams[i].payload[j].media_format){
					goto b2bsend;
				}

				/* payload_type map */
				to_payloadtype = co->b2bstreams[i].payload[k].media_format;
				if( to_payloadtype >= 0 ){
					rtp->payload_type = to_payloadtype;
				}
b2bsend:
				sendto(co->b2bstreams[i].fds[k], buf, recvlen, 0, 
						(struct sockaddr *)&co->b2bstreams[i].remote[k], slen);
			}
		}
	}

	return 0;
}



