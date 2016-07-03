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
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

/* RTSP head files */
#include "rtsp_client.h"

static void 
do_relative_url_to_absolute (char **control_string,
										 const char *base_url,
										 int dontfree)
{
	char *str, *cpystr;
	uint32_t cblen, malloclen;

	malloclen = cblen = strlen(base_url);

	if (base_url[cblen - 1] != '/') malloclen++;
	/*
	* If the control string is just a *, use the base url only
	*/
	cpystr = *control_string;
	if (strcmp(cpystr, "*") != 0)
	{
		if (*cpystr == '/') cpystr++;

		/* duh - add 1 for \0...*/
		str = (char *)malloc(strlen(cpystr) + malloclen + 1);
		if (str == NULL)
			return;
		strcpy(str, base_url);
		if (base_url[cblen - 1] != '/')
		{
			strcat(str, "/");
		}
		if (*cpystr == '/') cpystr++;
		strcat(str, cpystr);
	}
	else
	{
		str = strdup(base_url);
	}
	if (dontfree == 0)
		free(*control_string);
	*control_string = str;
}

/*
* convert_relative_urls_to_absolute - for every url inside the session
* description, convert relative to absolute.
*/
void 
convert_relative_urls_to_absolute (session_desc_t *sdp,
		const char *base_url)
{
	media_desc_t *media = NULL;

	if (base_url == NULL)
		return;

	if ((sdp->control_string != NULL) &&
		(strncmp(sdp->control_string, "rtsp://", strlen("rtsp://"))) != 0)
	{
		do_relative_url_to_absolute(&sdp->control_string, base_url, 0);
	}

	for (media = sdp->media; media != NULL; media = media->next)
	{
		if ((media->control_string != NULL) &&
			(strncmp(media->control_string, "rtsp://", strlen("rtsp://")) != 0))
		{
			do_relative_url_to_absolute(&media->control_string, base_url, 0);
		}
	}
}


static void
CvtHex (IN HASH Bin, OUT HASHHEX Hex)
{
	unsigned short i;
	unsigned char j;

	for (i = 0; i < HASHLEN; i++) {
		j = (Bin[i] >> 4) & 0xf;
		if (j <= 9)
			Hex[i * 2] = (j + '0');
		else
			Hex[i * 2] = (j + 'a' - 10);
		j = Bin[i] & 0xf;
		if (j <= 9)
			Hex[i * 2 + 1] = (j + '0');
		else
			Hex[i * 2 + 1] = (j + 'a' - 10);
	};
	Hex[HASHHEXLEN] = '\0';
}

/* calculate H(A1) as per spec */
static void
DigestCalcHA1 (IN const char *pszAlg, IN const char *pszUserName, IN const char *pszRealm, IN const char *pszPassword, IN const char *pszNonce, IN const char *pszCNonce, OUT HASHHEX SessionKey)
{
	osip_MD5_CTX Md5Ctx;
	HASH HA1;

	osip_MD5Init (&Md5Ctx);
	osip_MD5Update (&Md5Ctx, (unsigned char *) pszUserName, (unsigned int) strlen (pszUserName));
	osip_MD5Update (&Md5Ctx, (unsigned char *) ":", 1);
	osip_MD5Update (&Md5Ctx, (unsigned char *) pszRealm, (unsigned int) strlen (pszRealm));
	osip_MD5Update (&Md5Ctx, (unsigned char *) ":", 1);
	osip_MD5Update (&Md5Ctx, (unsigned char *) pszPassword, (unsigned int) strlen (pszPassword));
	osip_MD5Final ((unsigned char *) HA1, &Md5Ctx);
	if ((pszAlg != NULL) && osip_strcasecmp (pszAlg, "md5-sess") == 0) {
		osip_MD5Init (&Md5Ctx);
		osip_MD5Update (&Md5Ctx, (unsigned char *) HA1, HASHLEN);
		osip_MD5Update (&Md5Ctx, (unsigned char *) ":", 1);
		osip_MD5Update (&Md5Ctx, (unsigned char *) pszNonce, (unsigned int) strlen (pszNonce));
		osip_MD5Update (&Md5Ctx, (unsigned char *) ":", 1);
		osip_MD5Update (&Md5Ctx, (unsigned char *) pszCNonce, (unsigned int) strlen (pszCNonce));
		osip_MD5Final ((unsigned char *) HA1, &Md5Ctx);
	}
	CvtHex (HA1, SessionKey);
}

/* calculate request-digest/response-digest as per HTTP Digest spec */
void 
DigestCalcResponse(
	IN HASHHEX HA1,         /* H(A1) */
	IN const char * pszNonce,     /* nonce from server */
	IN const char * pszNonceCount,  /* 8 hex digits */
	IN const char * pszCNonce,    /* client nonce */
	IN const char * pszQop,       /* qop-value: "", "auth", "auth-int" */
	IN const char * pszMethod,    /* method from the request */
	IN const char * pszDigestUri, /* requested URL */
	IN HASHHEX HEntity,     /* H(entity body) if qop="auth-int" */
	OUT HASHHEX Response    /* request-digest or response-digest */
	)
{
	osip_MD5_CTX Md5Ctx;
	HASH HA2;
	HASH RespHash;
	HASHHEX HA2Hex;

	/* calculate H(A2) */
	osip_MD5Init(&Md5Ctx);
	if (pszMethod)    osip_MD5Update(&Md5Ctx, (unsigned char*)pszMethod,
		strlen(pszMethod));
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	if (pszDigestUri) osip_MD5Update(&Md5Ctx, (unsigned char*)pszDigestUri,
		strlen(pszDigestUri));

	if (pszQop!=NULL) {
		goto auth_withqop;
	};

	/* auth_withoutqop: */
	osip_MD5Final((unsigned char *)HA2, &Md5Ctx);
	CvtHex(HA2, HA2Hex);

	/* calculate response */
	osip_MD5Init(&Md5Ctx);
	osip_MD5Update(&Md5Ctx, (unsigned char*)HA1, HASHHEXLEN);
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	if (pszNonce)    osip_MD5Update(&Md5Ctx, (unsigned char*)pszNonce, strlen(pszNonce));
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);

	goto end;

auth_withqop:

	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	osip_MD5Update(&Md5Ctx, (unsigned char*)HEntity, HASHHEXLEN);
	osip_MD5Final((unsigned char *)HA2, &Md5Ctx);
	CvtHex(HA2, HA2Hex);

	/* calculate response */
	osip_MD5Init(&Md5Ctx);
	osip_MD5Update(&Md5Ctx, (unsigned char*)HA1, HASHHEXLEN);
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	if (pszNonce)    osip_MD5Update(&Md5Ctx, (unsigned char*)pszNonce, strlen(pszNonce));
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	if (pszNonceCount)osip_MD5Update(&Md5Ctx, (unsigned char*)pszNonceCount, strlen(pszNonceCount));
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	if (pszCNonce)   osip_MD5Update(&Md5Ctx, (unsigned char*)pszCNonce, strlen(pszCNonce));
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);
	if (pszQop)      osip_MD5Update(&Md5Ctx, (unsigned char*)pszQop, strlen(pszQop));
	osip_MD5Update(&Md5Ctx, (unsigned char*)":", 1);

end:
	osip_MD5Update(&Md5Ctx, (unsigned char*)HA2Hex, HASHHEXLEN);
	osip_MD5Final((unsigned char *)RespHash, &Md5Ctx);
	CvtHex(RespHash, Response);
}

int 
rtsp_compute_digest_response(const char *rquri, 
	const char *username, const char *passwd, 
	const char *realm, const char *nonce, const char *method,
	char *response)
{
	char *pszNonce = NULL;
	char *pszCNonce = NULL;
	const char *pszUser = username;
	char *pszRealm = NULL;
	const char *pszPass = NULL;
	char *szNonceCount = NULL;
	const char *pszMethod = (char *) method; 
	const char *pszURI = rquri;

	HASHHEX HA1;
	HASHHEX HA2 = "";
	//HASHHEX Response;
	char *pha1 = NULL;

	if ( realm == NULL) {
		pszRealm = osip_strdup ("");
	}
	else {
		pszRealm = osip_strdup_without_quote (realm);
	}

	pszPass = passwd;

	if (nonce == NULL) {
		osip_free (pszRealm);
		return -1;
	}
	pszNonce = osip_strdup_without_quote (nonce);
	
	// The "response" field is computed as:
	//    md5(md5(<username>:<realm>:<password>):<nonce>:md5(<cmd>:<url>))
	DigestCalcHA1 ("MD5", pszUser, pszRealm, pszPass, pszNonce, pszCNonce, HA1);
	pha1 = HA1;
	DigestCalcResponse((char *) pha1, pszNonce, szNonceCount, pszCNonce, NULL, pszMethod, pszURI, HA2, response);
	
	osip_free (pszNonce);
	osip_free (pszCNonce);
	osip_free (pszRealm);
	osip_free (szNonceCount);

	return 0;
}

