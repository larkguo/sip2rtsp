
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
 */

#include "transport_parse.h"

#define TRANSPORT_PARSE(a) static char *transport_parse_## a (char *transport, rtsp_transport_parse_t *r)
#define ADV_SPACE(a) {while (isspace(*(a)) && (*(a) != '\0'))(a)++;}


char *convert_number (char *transport, uint32_t *value)
{
	*value = 0;
	while (isdigit(*transport)) {
		*value *= 10;
		*value += *transport - '0';
		transport++;
	}
	return (transport);
}

char *convert_hex (char *transport, uint32_t *value)
{
	*value = 0;
	while (isxdigit(*transport)) {
		*value *= 16;
		if (isdigit(*transport))
			*value += *transport - '0';
		else
			*value += tolower(*transport) - 'a' + 10;
		transport++;
	}
	return (transport);
}

TRANSPORT_PARSE(unicast)
{
	ADV_SPACE(transport);
	r->have_unicast = 1;
	if (*transport == '\0') return (transport);

	if (*transport != ';')
		return (NULL);
	transport++;
	ADV_SPACE(transport);
	return (transport);
}

TRANSPORT_PARSE(multicast)
{
	ADV_SPACE(transport);
	r->have_multicast = 1;
	if (*transport == '\0') return (transport);

	if (*transport != ';')
		return (NULL);
	transport++;
	ADV_SPACE(transport);

	return (transport);
}

TRANSPORT_PARSE(client_port)
{
	uint32_t fromport, toport;

	if (*transport++ != '=') {
		return (NULL);
	}
	ADV_SPACE(transport);
	transport = convert_number(transport, &fromport);
	ADV_SPACE(transport);

	r->client_port = fromport;

	if (*transport == ';') {
		transport++;
		return (transport);
	}
	if (*transport == '\0') {
		return (transport);
	}
	if (*transport != '-') {
		return (NULL);
	}
	transport++;
	ADV_SPACE(transport);
	transport = convert_number(transport, &toport);
	if (toport < fromport || toport > fromport + 1) {
		return (NULL);
	}
	ADV_SPACE(transport);
	if (*transport == ';') {
		transport++;
	}
	return(transport);
}

TRANSPORT_PARSE(server_port)
{
	uint32_t fromport, toport;

	if (*transport++ != '=') {
		return (NULL);
	}
	ADV_SPACE(transport);
	transport = convert_number(transport, &fromport);
	ADV_SPACE(transport);

	r->server_port = fromport;

	if (*transport == ';') {
		transport++;
		return (transport);
	}
	if (*transport == '\0') {
		return (transport);
	}
	if (*transport != '-') {
		return (NULL);
	}
	transport++;
	ADV_SPACE(transport);
	transport = convert_number(transport, &toport);
	if (toport < fromport || toport > fromport + 1) {
		return (NULL);
	}
	ADV_SPACE(transport);
	if (*transport == ';') {
		transport++;
	}
	return(transport);
}

TRANSPORT_PARSE(destination)
{
	char *ptr;
	uint32_t addrlen;

	if (*transport != '=') {
		return (NULL);
	}
	transport++;
	ADV_SPACE(transport);
	ptr = transport;
	while (*transport != ';' && *transport != '\0') transport++;
	addrlen = transport - ptr;
	if (addrlen == 0) {
		return (NULL);
	}
	strncpy(r->destination, ptr, MIN(addrlen,sizeof(r->destination)-1));	
	if (*transport == ';') transport++;
	return (transport);
}

TRANSPORT_PARSE(source)
{
	char *ptr;
	uint32_t addrlen;
	
	if (*transport != '=') {
		return (NULL);
	}
	transport++;
	ADV_SPACE(transport);
	ptr = transport;
	while (*transport != ';' && *transport != '\0') transport++;
	addrlen = transport - ptr;
	if (addrlen == 0) {
		return (NULL);
	}
	strncpy(r->source, ptr, MIN(addrlen,sizeof(r->source)-1));	
	if (*transport == ';') transport++;
	return (transport);
}


TRANSPORT_PARSE(ssrc)
{
	uint32_t ssrc;
	if (*transport != '=') {
		return (NULL);
	}
	transport++;
	ADV_SPACE(transport);
	transport = convert_hex(transport, &ssrc);
	ADV_SPACE(transport);
	if (*transport != '\0') {
		if (*transport != ';') {
			return (NULL);
		}
		transport++;
	}
	r->have_ssrc = 1;
	r->ssrc = ssrc;
	return (transport);
}

TRANSPORT_PARSE(interleave)
{
	uint32_t chan, chan2;
	if (*transport != '=') {
		return (NULL);
	}
	transport++;
	ADV_SPACE(transport);
	transport = convert_number(transport, &chan);
	chan2 = r->interleave_port;
	if (chan != chan2) {
		return NULL;
	}
	ADV_SPACE(transport);
	if (*transport != '\0') {
		if (*transport != '-') {
			return (NULL);
		}
		transport++;
		transport = convert_number(transport, &chan2);
		if (chan + 1 != chan2) {
			return (NULL);
		}

		if (*transport == '\0') return (transport);
	}
	if (*transport != ';') return (NULL);
	transport++;
	return (transport);
}

#define TTYPE(a,b) {a, sizeof(a), b}

struct {
	const char *name;
	uint32_t namelen;
	char *(*routine)(char *transport, rtsp_transport_parse_t *);
} transport_types[] = 
{
	TTYPE("unicast", transport_parse_unicast),
	TTYPE("multicast", transport_parse_multicast),
	TTYPE("client_port", transport_parse_client_port),
	TTYPE("server_port", transport_parse_server_port),
	TTYPE("port", transport_parse_server_port),
	TTYPE("destination", transport_parse_destination),
	TTYPE("source", transport_parse_source),
	TTYPE("ssrc", transport_parse_ssrc),
	TTYPE("interleaved", transport_parse_interleave),
	{NULL, 0, NULL},
}; 

int process_rtsp_transport (rtsp_transport_parse_t *parse,
							char *transport,
							const char *proto)
{
	size_t protolen;
	int ix;

	if (transport == NULL) 
		return (-1);

	protolen = strlen(proto);

	if (strncasecmp(transport, proto, protolen) != 0) {
		return (-1);
	}
	transport += protolen;
	if (*transport == '/') {
		transport++;
		if (parse->use_interleaved) {
			if (strncasecmp(transport, "TCP", strlen("TCP")) != 0) {
				return (-1);
			}
			transport += strlen("TCP");
		} else {
			if (strncasecmp(transport, "UDP", strlen("UDP")) != 0) {
				return (-1);
			}
			transport += strlen("UDP");
		}
	}
	if (*transport != ';') {
		return (-1);
	}
	transport++;
	do {
		ADV_SPACE(transport);
		for (ix = 0; transport_types[ix].name != NULL; ix++) {
			if (strncasecmp(transport, 
				transport_types[ix].name, 
				transport_types[ix].namelen - 1) == 0) {
					transport += transport_types[ix].namelen - 1;
					ADV_SPACE(transport);
					transport = (transport_types[ix].routine)(transport, parse);
					break;
			}
		}
		if (transport_types[ix].name == NULL) {

			while (*transport != ';' && *transport != '\0') transport++;
			if (*transport != '\0') transport++;
		}
	} while (transport != NULL && *transport != '\0');

	if (transport == NULL) {
		return (-1);
	}
	return (0);
}


