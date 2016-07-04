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
 

#include <errno.h>
#include <syslog.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "log.h"

#define BUFF_SIZE        (2048)

static const char *
log_strlevel(int level)
{
	switch(level) {
		case LOG_DEBUG:
			return "DBUG";
		case LOG_INFO:
			return "INFO";
		case LOG_NOTICE:
			return "NOTI";
		case LOG_WARNING:
			return "WARN";
		case LOG_ERR:
			return "ERR";
		case LOG_CRIT:
			return "CRIT";
		case LOG_ALERT:
			return "ALERT";
		case LOG_EMERG:
			return "EMERG";
		default:
			break;
	}

	return NULL;
}

void
log_write(core *co, const char *file,int line,int level, const char *format, ...)
{
	int lg;
	va_list ap;
	char *buf = NULL;
	char fmt[]="%s|%s:%d ";

	if(level > co->log_level)
		return;
	buf = (char *)malloc(BUFF_SIZE*sizeof(char));
	if( NULL == buf){
		printf("malloc NULL\n");
	}
	
	va_start(ap, format);

	lg=snprintf(buf, BUFF_SIZE, fmt, log_strlevel(level),file,line);
	if (lg < BUFF_SIZE){
		lg+=vsnprintf(buf+lg, BUFF_SIZE-lg, format, ap);
	}
	buf[BUFF_SIZE-1]='\0';
	osip_fifo_add(co->log_queue,buf);
	
	va_end(ap);
}

int 
log_init(core *co)
{
	co->log_fd = NULL;
	
	if (co->log_level > 0){
		if (co->log_file != NULL){
			co->log_fd = fopen(co->log_file,"w+");
		}
	}
	return 0;
}
	
void *
log_loop (void *arg)
{
	core *co= (core *)arg;
	char *buf = NULL;
	FILE *fd = stdout;
	
	if( NULL != co->log_fd){
		fd = co->log_fd;
	}
	
	for (;;) {
		buf = (char *)osip_fifo_get(co->log_queue);
		if( NULL != buf){
			fprintf(fd,"%s",buf);
			fflush(fd);
			free(buf);
		}
	}
	return NULL;
}


