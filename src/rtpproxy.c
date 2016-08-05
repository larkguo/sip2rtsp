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
static int sock_create(core*co,int callid,stream_mode mode, b2b_side side);

int 
payload_init(core *co)
{
	int i, j;

	/* payload */	
	for(i = 0; i < stream_max; i++) {
		memset(&co->rtsp.payload[i],0,sizeof(co->rtsp.payload[i]));
		co->rtsp.payload[i].media_format = -1;

		for(j = 0; j< co->maxcalls; j++) {
			memset(&co->sipcall[j].payload[i],0,sizeof(co->rtsp.payload[i]));
			co->sipcall[j].payload[i].media_format = -1;
		}
	}
	
	return 0;
}

int 
sock_blocking_set(int sockfd)
{
#ifdef WIN32
	u_long val = 0;
	return ioctlsocket(sockfd, FIONBIO, &val);
#else
	int val;

	val = fcntl(sockfd, F_GETFL, 0);
	if (fcntl(sockfd, F_SETFL, val & ~O_NONBLOCK) == -1) {
		return -1;
	}
#endif
	return 0;
}

int 
sock_noblocking_set(int sockfd)
{
#ifdef WIN32
	u_long val = 1;
	return ioctlsocket(sockfd, FIONBIO, &val);
#else /* WIN32 */
	int val;

	val = fcntl(sockfd, F_GETFL, 0);
	if (fcntl(sockfd, F_SETFL, val | O_NONBLOCK) == -1) {
		return -1;
	}
#endif /* WIN32 */
	return 0;
}

static int 
sock_address_get(int sock, char *ipbuf, int ipbuf_len, int *port)
{
	int ret = 0;
	struct sockaddr_in addr;
	socklen_t slen = sizeof(addr);
	
	ret = getsockname(sock,(struct sockaddr *)&addr,&slen);
	if(0 == ret){
		switch(((struct sockaddr *)&addr)->sa_family) {
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
sock_create(core *co,int callid,stream_mode mode, b2b_side side)
{
	int sock = -1;
	int ret = -1;
	int port = 0;
	
	if( NULL == co || 
		mode >= stream_max || mode < stream_audio_rtp || 
		side >= side_max || side < side_sip){
		return -1;
	}
	if(side_rtsp == side){
		memset(&co->rtsp.remote[mode],0,sizeof(co->rtsp.remote[mode]));
		co->rtsp.local[mode].sin_family      = AF_INET;
		co->rtsp.local[mode].sin_addr.s_addr = inet_addr(co->rtsp_localip);
		
		sock =  socket(AF_INET, SOCK_DGRAM, 0);
		ret = bind(sock,(struct sockaddr *)&co->rtsp.local[mode],sizeof(co->rtsp.local[mode]));	
		if( ret < 0 ){
			log(co,LOG_DEBUG,"rtpproxy rtsp bind(%s:%d)=%d failed!\n",
				co->rtsp_localip,ntohs(co->rtsp.local[mode].sin_port),ret);
			return -1;
		}
		co->rtsp.fds[mode] = sock;
		ret = sock_address_get(sock,NULL, 0, &port);
		if( 0 == ret ){
			co->rtsp.local[mode].sin_port = htons(port);
		}				
		sock_noblocking_set(co->rtsp.fds[mode]);
	}else{
		int j = -1;
		for(j = 0; j < co->maxcalls; j++) {
			if(co->sipcall[j].callid == callid){
				memset(&co->sipcall[j].remote[mode],0,sizeof(co->sipcall[j].remote[mode]));
				co->sipcall[j].local[mode].sin_family      = AF_INET;
				co->sipcall[j].local[mode].sin_addr.s_addr = inet_addr(co->sip_localip);

				sock =  socket(AF_INET, SOCK_DGRAM, 0);
				ret = bind(sock,(struct sockaddr *)&co->sipcall[j].local[mode],sizeof(co->sipcall[j].local[mode]));	
				if( ret < 0 ){
					log(co,LOG_DEBUG,"rtpproxy sip bind(%s:%d)=%d failed!\n",
						co->sip_localip,ntohs(co->sipcall[j].local[mode].sin_port),ret);
					return -1;
				}
				co->sipcall[j].fds[mode] = sock;
				ret = sock_address_get(sock,NULL, 0, &port);
				if( 0 == ret ){
					co->sipcall[j].local[mode].sin_port = htons(port);
				}						
				sock_noblocking_set(co->sipcall[j].fds[mode]);
			}	
		}
	}
	return sock;
}

int 
sock_pair_create(core *co,int callid,stream_mode mode, b2b_side side)
{
	int rtp_sock = -1;
	int rtcp_sock = -1;
	int try_times = 0;
	int current_port = core_rtp_current_port_get(co);
	int start_port = core_rtp_start_port_get(co);
	int end_port = core_rtp_end_port_get(co);

try_nextport:		
	if(try_times > 3 ) {
		log(co,LOG_ERR,"bind error times %d, current_port=%d\n",try_times,current_port);
		return -1;
	}
	
	if(current_port >= end_port )
		current_port = start_port;

	/* rtsp */
	if(side_rtsp == side){
		if(co->rtsp.fds[mode] > 0)  return 0;
		
		co->rtsp.local[mode].sin_port	= htons(current_port);
		co->rtsp.local[mode+1].sin_port = htons(current_port+1);
		rtp_sock = sock_create(co,callid,mode,side);
		rtcp_sock = sock_create(co,callid,mode+1,side);
		if( rtp_sock <= 0 || rtcp_sock <= 0 ){
			if(rtp_sock > 0)  close(rtp_sock);
			if(rtcp_sock > 0)  close(rtcp_sock);
			current_port += 2;
			try_times += 1;
			goto try_nextport;
		}
		current_port += 2;
		core_rtp_current_port_set(co,current_port);
	}else{ /* sip */
		int j = -1;
		for(j = 0; j < co->maxcalls; j++) {
			if(co->sipcall[j].callid == callid){
				if(co->sipcall[j].fds[mode] > 0)  return 0;
				
				co->sipcall[j].local[mode].sin_port	= htons(current_port);
				co->sipcall[j].local[mode+1].sin_port = htons(current_port+1);
				rtp_sock = sock_create(co,callid,mode,side);
				rtcp_sock = sock_create(co,callid,mode+1,side);
				if( rtp_sock <= 0 || rtcp_sock <= 0 ){
					if(rtp_sock > 0)  close(rtp_sock);
					if(rtcp_sock > 0)  close(rtcp_sock);
					current_port += 2;
					try_times += 1;
					goto try_nextport;
				}
				current_port += 2;
				core_rtp_current_port_set(co,current_port);
			}
		}
	}
	return 0;
}

int 
streams_init(core *co)
{
	int rtpproxy = core_rtpproxy_get(co);
	int ret = 0;
	
	if(0 == rtpproxy)
		return 0;

	/* create rtp and rtcp */
	ret = sock_pair_create(co, -1,stream_audio_rtp, side_rtsp);
	if(0==ret) ret = sock_pair_create(co, -1,stream_video_rtp, side_rtsp);
	if( ret < 0) {
		return ret;
	}
	
	payload_init(co);
	core_show(co);
	return 0;
}

int 
stream_call_stop(core *co, int callid)
{
	int i, j ;
	int fd;
	int rtpproxy = core_rtpproxy_get(co);
	
	if(0 == rtpproxy)
		return 0;
		
	for(j = 0; j < co->maxcalls; j++) {
		if(co->sipcall[j].callid == callid) {
			for(i = 0; i < stream_max; i++) {
				fd = co->sipcall[j].fds[i];
				if(fd >= 0) {
					close(fd);
					co->sipcall[j].fds[i] = -1;
				}
			}
		}
	}
	
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
	
	for(i = 0; i < stream_max; i++) {
		fd = co->rtsp.fds[i];
		if(fd >= 0) {
			close(fd);
			co->rtsp.fds[i] = -1;
		}
		
		for(j = 0; j < co->maxcalls; j++) {
			fd = co->sipcall[j].fds[i];
			if(fd >= 0) {
				close(fd);
				co->sipcall[j].fds[i] = -1;
			}
		}
	}
	return 0;
}

static int 
streams_rtsp_loop(core *co)
{
	int				fd,nready;
	fd_set			readset; 
	ssize_t			recvlen;
	int				slen;
	struct timeval		tv;
	int 				i, j ;
	char				buf[RECV_BUFF_DEFAULT_LEN] = {0};
	rtp_header *		rtp = NULL;
	int 				maxfd = 0;
	int 				ret = -1;
	struct sockaddr_storage sa;
	slen = sizeof(struct sockaddr_in);
			
	tv.tv_sec = 0;
	tv.tv_usec = 10000; 
	FD_ZERO(&readset);

	for(i = 0; i < stream_max; i++) {
		fd = co->rtsp.fds[i];
		if(fd > 0){
			FD_SET(fd, &readset);
			if(maxfd < fd) maxfd = fd; 
		}
	}
	
	nready = select(maxfd+1, &readset, NULL, NULL, &tv);
	if(nready <= 0)
		return 0;
	
	/* rtsp */
	for(i = 0; i < stream_max; i++) {
		fd = co->rtsp.fds[i];
		if(FD_ISSET(fd, &readset)) {
			/* symmetricRTP */
			if(co->symmetric_rtp){
				recvlen = recvfrom(fd,buf,sizeof(buf),0,
					(struct sockaddr *)&co->rtsp.remote[i],(socklen_t *)&slen); 
			}else{
				recvlen = recvfrom(fd,buf,sizeof(buf),0,
					(struct sockaddr *)&sa,(socklen_t *)&slen); 
			}

			/* rtcp */
			if(stream_audio_rtcp == i || stream_video_rtcp == i)
				goto sendtosip;

			/* rtp header len > 12 */
			if( recvlen <= 12 ){ 
				goto sendtosip;
			}

			/* Check RTP version */
			rtp = (rtp_header*)buf;
			if( 2 != rtp->version ) {
				goto sendtosip;
			}
			
			/* Check RTP payloadType */
			if(rtp->payload_type != co->rtsp.payload[i].media_format){
				goto sendtosip;
			}
sendtosip: 
			for(j = 0; j < co->maxcalls; j++) {
				stream_dir  dir ;
				
				if(co->sipcall[j].callid <= 0)	{
					continue;
				}
				if(co->sipcall[j].fds[i]<= 0)	{
					continue;
				}
				
				dir = core_sipcall_dir_get(co,co->sipcall[j].callid,i);
				if(stream_inactive == dir || stream_sendonly == dir)	{
					continue;
				}
				
				/* payload_type map */
				if(co->sipcall[j].payload[i].media_format >= 0 ){
					rtp->payload_type = co->sipcall[j].payload[i].media_format;
				}
				ret = sendto(co->sipcall[j].fds[i],buf,recvlen,0,
					(struct sockaddr *)&co->sipcall[j].remote[i],slen);
				if(ret < 0){
					log(co,LOG_DEBUG,"call(%d-%d) stream %d length=%d sendto failed:%d\n",
						j,co->sipcall[j].callid, i, recvlen, ret);
				}
			}
		}
	}
	
	return 0;
}


static int 
streams_sip_loop(core *co)
{
	int				fd,nready;
	fd_set			readset; 
	int				slen;
	struct timeval 	tv;
	int 				i, j ;
	char				buf[RECV_BUFF_DEFAULT_LEN] = {0};
	int 				maxfd = 0;

	slen = sizeof(struct sockaddr_in);
			
	tv.tv_sec = 0;
	tv.tv_usec = 10000; 
	FD_ZERO(&readset);

	for(i = 0; i < stream_max; i++) {
		for(j = 0; j < co->maxcalls; j++) {
			fd = co->sipcall[j].fds[i];
			if(fd > 0){
				FD_SET(fd, &readset);
				if(maxfd < fd) maxfd = fd; 
			}
		}
	}
	
	nready = select(maxfd+1, &readset, NULL, NULL, &tv);
	if(nready <= 0)
		return 0;

	for(i = 0; i < stream_max; i++) {
		for(j = 0; j < co->maxcalls; j++) {
			if(co->sipcall[j].callid > 0){
				fd = co->sipcall[j].fds[i];
				if(FD_ISSET(fd, &readset)) {
					recvfrom(fd,buf,sizeof(buf),0,
						(struct sockaddr *)&co->sipcall[j].remote[i],(socklen_t *)&slen); 
				}
			}
		}
	}

	return 0;
}

int streams_loop(core *co)
{	
	int callnum = core_sipcallnum_get(co);
	int rtpproxy = core_rtpproxy_get(co);
	
	if( rtpproxy && callnum > 0){
		streams_rtsp_loop(co);
		if(co->symmetric_rtp){
			streams_sip_loop(co);
		}
	}else{
		osip_usleep(50000);
	}
	return 0;
}


