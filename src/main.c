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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "rtpproxy.h"
#include "sip.h"
#include "log.h"

static void 
usage(void)
{
        printf("\nUsage: " UA_STRING "\n"
                "\t-h -- help\n"
                "\t-f -- config file\n"
                "\n\texample:\n"
                "\tsip2rtsp -f sip2rtsp.cfg\n\n");
}

int 
main(int argc, char *argv[])
{
	int c;
	core co;
	int ret = 0;
	
	ret = core_init(&co);
	if( ret != 0){
		printf("core_init fialed!\n");
		return -1;
	}
	
	for(;;) {
#define short_options "hf:"
#ifdef _GNU_SOURCE
	    int option_index = 0;

	    static struct option long_options[] = {
	      {"configfile", required_argument, NULL, 'f'},
	      {"help", no_argument, NULL, 'h'},
	      {NULL, 0, NULL, 0}
	    };

   		c = getopt_long(argc, argv, short_options, long_options, &option_index);
#else
    		c = getopt(argc, argv, short_options);
#endif
		
		if(c == -1)
			break;

		switch(c) {
		case 'f':
			co.cfg_file= optarg;
			break;
		case 'h':
			usage();
			return 0;
		default:
			break;
		}
	}

	co.cfg = cfg_new(co.cfg_file,'=');
	if(NULL == co.cfg ){
		printf("ERROR: Could not open config file %s\n",co.cfg_file);
		return -1;
	}
	co.log_level = cfg_get_int(co.cfg,"debug","level", LOG_ERR);
	co.log_file = cfg_get_string(co.cfg,"debug","logfile", NULL);		
	co.contact = cfg_get_string(co.cfg,"sip","contact", NULL);
	co.expiry = cfg_get_int(co.cfg,"sip","expiry", 3600);
	co.firewallip = cfg_get_string(co.cfg,"sip","firewallip", NULL);
	co.sip_localip = cfg_get_string(co.cfg,"sip","localip", "0.0.0.0");
	co.sip_localport = cfg_get_int(co.cfg,"sip","localport", 5060);
	co.proxy = cfg_get_string(co.cfg,"sip","proxy", NULL);
	co.fromuser = cfg_get_string(co.cfg,"sip","from", NULL);
	co.outboundproxy = cfg_get_string(co.cfg,"sip","outboundproxy", NULL);
	co.authusername = cfg_get_string(co.cfg,"sip","authusername", NULL);
	co.authpassword = cfg_get_string(co.cfg,"sip","authpassword", NULL);
	co.rtsp_localip = cfg_get_string(co.cfg,"rtsp","localip", "0.0.0.0");
	co.rtsp_url = cfg_get_string(co.cfg,"rtsp","url", NULL);
	co.rtsp_username = cfg_get_string(co.cfg,"rtsp","username", NULL);
	co.rtsp_password = cfg_get_string(co.cfg,"rtsp","password", NULL);
	co.session_timeout = cfg_get_int(co.cfg,"sip","session_timeout", 60);
	co.rtpproxy = cfg_get_int(co.cfg,"rtp","proxy", 1);
	co.rtp_start_port = cfg_get_int(co.cfg,"rtp","start_port", 9000);
	co.rtp_end_port = cfg_get_int(co.cfg,"rtp","end_port", 9100);
	if(co.rtp_start_port % 2 != 0 ||
		co.rtp_start_port <= 1024 ||
		co.rtp_end_port > 65535 ||
		(co.rtp_start_port+40) > co.rtp_end_port) {
		printf("rtp_start_port %d or rtp_end_port %d invalid\n",co.rtp_start_port,co.rtp_end_port);
		co.rtp_start_port = 9000;
		co.rtp_end_port = 9100;
	}	
	co.rtp_current_port = co.rtp_start_port;
	co.symmetric_rtp = cfg_get_int(co.cfg,"rtp","symmetric", 1);
	if(!co.proxy || !co.fromuser || !co.rtsp_url) {
		usage();
		return -1;
	}

	log_init(&co);
	
	/* INIT Log File and Log LEVEL  */ 
	co.log_thread = osip_thread_create(20000, log_loop, &co);
	if(co.log_thread == NULL) {
		printf("pthread_create failed\n");
		return -1;
	}

	log(&co,LOG_INFO,"%s  up and running\n"
		"proxy=%s\n"
		"outboundproxy=%s\n"
		"fromuser=%s\n"
		"contact=%s\n"
		"expiry=%d\n"
		"sip_localip=%s\n"
		"sip_localport=%d\n"
		"authusername=%s\n"
		"authpassword=%s\n"
		"rtsp_localip=%s\n"
		"rtsp_url=%s\n"
		"rtsp_username=%s\n"
		"rtsp_password=%s\n"
		"session_timeout=%d\n"
		"rtpproxy=%d\n"
		"rtp_start_port=%d\n"
		"rtp_end_port=%d\n"
		"symmetric_rtp=%d\n"
		"cfg_file=%s\n"
		"log_file=%s\n"
		"log_level=%d\n",
		UA_STRING,
		co.proxy,
		co.outboundproxy,
		co.fromuser,
		co.contact,
		co.expiry,
		co.sip_localip,
		co.sip_localport,
		co.authusername,
		co.authpassword,
		co.rtsp_localip,
		co.rtsp_url,
		co.rtsp_username,
		co.rtsp_password,
		co.session_timeout,
		co.rtpproxy,
		co.rtp_start_port,
		co.rtp_end_port,
		co.symmetric_rtp,
		co.cfg_file,
		co.log_file,
		co.log_level);

	ret = sip_init(&co);
	if( 0 != ret ) {
		log(&co,LOG_ERR,"sip_init failed!\n");
		return -1;
	}

	ret = streams_init(&co);
	if( 0 != ret ) {
		log(&co,LOG_ERR,"streams_init failed!\n");
		return -1;
	}

	/* main loop */
	sip_uas_loop(&co);

	/* exit */
	streams_stop(&co);
	core_exit(&co);
	
	return ret;
}
