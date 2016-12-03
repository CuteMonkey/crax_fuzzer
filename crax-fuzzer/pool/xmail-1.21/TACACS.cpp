/*
 *  XMail by Davide Libenzi ( Intranet and Internet mail server )
 *  Copyright (C) 1999,..,2004  Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#include "SysInclude.h"
#include "SysDep.h"
#include "SvrDefines.h"
#include "ShBlocks.h"
#include "StrUtils.h"
#include "SList.h"
#include "MailConfig.h"
#include "MailSvr.h"
#include "MiscUtils.h"
#include "TACACS.h"

/*

#define ERR_TACACS_AUTH_FAILED      (-???)
#define ERR_TACACS_AUTH_UNAVAILABLE (-???)
#define ERR_TACACS_FILE_NOT_FOUND   (-???)
#define ERR_TACACS_SERVER_NOT_FOUND (-???)

{ERR_TACACS_AUTH_FAILED, "TACACS authentication failed", NULL},
{ERR_TACACS_AUTH_UNAVAILABLE, "TACACS authentication server unavailable", NULL},
{ERR_TACACS_FILE_NOT_FOUND, "TACACS configuration file not found", NULL},
{ERR_TACACS_SERVER_NOT_FOUND, "TACACS server for domain not found", NULL},

    */

#define TACACS_TIMEOUT          15
#define TACACS_SEND_RETRIES     3
#define TACACS_DPORT            110
#define TACACS_LPORT            0

#define TACS_CONFIG_FILE        "tacacsservers.tab"
#define TACS_ALIAS_LINE_MAX     1024

#define	TACACS_SVC_NAME         "tacacs"
#define	TACACS_PORT             49
#define XTA_VERSION             0x80

#define	TA_QUERY                1
#define	TA_ANSWER               2
#define	TA_CHANGE               3
#define	TA_FOLLOW               4

#define	TA_A_ACCEPTED           1
#define	TA_A_REJECTED           2

#define TA_A_NONE               0
#define	TA_A_EXPIRING           1
#define	TA_A_PASSWORD           2
#define	TA_A_DENIED             3
#define	TA_A_NOROUTE            8	/* Dialup routing not allowed           */
#define	TA_A_LOGINREQ           9	/* Login required for requested action  */

#define XTACACSSIZE             sizeof(xtacacstype)

#define	XTA_LOGIN               1
#define	XTA_ANSWER              2
#define	XTA_CHANGE              3
#define	XTA_FOLLOW              4
#define XTA_CONNECT             5
#define XTA_ENABLE              6
#define XTA_LOGOUT              7
#define XTA_RELOAD              8
#define XTA_SLIPON              9
#define XTA_SLIPOFF             10
#define XTA_SLIPADDR            11
#define XTA_ARAP_AUTH           12
#define XTA_CHAP_AUTH           13

#define	XTA_A_ACCEPTED          1
#define	XTA_A_REJECTED          2

#define XTA_A_NONE              0
#define	XTA_A_EXPIRING          1	/* Account expiring                     */
#define	XTA_A_PASSWORD          2	/* Wrong password                       */
#define	XTA_A_DENIED            3	/* Permission denied                    */
#define	XTA_A_NOROUTE           8	/* Dialup routing not permitted         */
#define	XTA_A_LOGINREQ          9	/* Login required for requested action  */

enum TacsFields {
	tacsDomain = 0,
	tacsServer,
	tacsPort,

	tacsMax
};

struct xtacacstype {
	SYS_UINT8 version;	/* version of protocol      */
	SYS_UINT8 type;		/* Type of query/response   */
	SYS_UINT16 trans;	/* transaction ID           */
	SYS_UINT8 namelen;	/* length of name           */
	SYS_UINT8 pwlen;	/* length of password       */
	SYS_UINT8 response;	/* response code            */
	SYS_UINT8 reason;	/* reason for response      */
	SYS_UINT32 uuid;	/* user id code assigned    */
	SYS_UINT32 dhost;	/* destination host         */
	SYS_UINT16 dport;	/* destination port         */
	SYS_UINT16 lport;	/* local line number        */
	SYS_UINT32 flags;	/* misc flags               */
	SYS_UINT16 accesslist;	/* access list for user     */
/*                  user name[]                             */
/*                  password[]                              */
};

static char *TacsGetConfigFilePath(char *pszTacsFile);
static int TacsGetServerName(char const *pszDomain, char *pszTacsServer, int &iPortNo);

static char *TacsGetConfigFilePath(char *pszTacsFile)
{

	CfgGetRootPath(pszTacsFile);

	strcat(pszTacsFile, TACS_CONFIG_FILE);

	return (pszTacsFile);

}

static int TacsGetServerName(char const *pszDomain, char *pszTacsServer, int &iPortNo)
{

	char szTacsFile[SYS_MAX_PATH] = "";

	TacsGetConfigFilePath(szTacsFile);

	FILE *pTacsFile = fopen(szTacsFile, "rt");

	if (pTacsFile == NULL) {
		ErrSetErrorCode(ERR_TACACS_FILE_NOT_FOUND);
		return (ERR_TACACS_FILE_NOT_FOUND);
	}

	char szTacsLine[TACS_ALIAS_LINE_MAX] = "";

	while (MscGetConfigLine(szTacsLine, sizeof(szTacsLine) - 1, pTacsFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szTacsLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= tacsMax) &&
		    StrIWildMatch(pszDomain, ppszStrings[tacsDomain])) {
			strcpy(pszTacsServer, ppszStrings[tacsServer]);

			iPortNo = atoi(ppszStrings[tacsPort]);

			StrFreeStrings(ppszStrings);
			fclose(pTacsFile);

			return (0);
		}

		StrFreeStrings(ppszStrings);
	}

	fclose(pTacsFile);

	ErrSetErrorCode(ERR_TACACS_SERVER_NOT_FOUND);

	return (ERR_TACACS_SERVER_NOT_FOUND);

}

int TacsAuthenticate(char const *pszDomain, char const *pszUsername,
		     char const *pszPassword, int iServicePort)
{
///////////////////////////////////////////////////////////////////////////////
//  Get TACACS server coordinates
///////////////////////////////////////////////////////////////////////////////
	int iPortNo = TACACS_PORT;
	char szTacsServer[MAX_HOST_NAME] = "";

	if (TacsGetServerName(pszDomain, szTacsServer, iPortNo) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  If not specified use POP3 port ( TACACS_DPORT )
///////////////////////////////////////////////////////////////////////////////
	if (iServicePort < 0)
		iServicePort = TACACS_DPORT;

///////////////////////////////////////////////////////////////////////////////
//  Open TACACS server socket
///////////////////////////////////////////////////////////////////////////////
	SYS_SOCKET SockFD;
	SYS_INET_ADDR SvrAddr;
	SYS_INET_ADDR SockAddr;

	if (MscCreateClientSocket(szTacsServer, iPortNo, SOCK_DGRAM, &SockFD, &SvrAddr,
				  &SockAddr, TACACS_TIMEOUT) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Build TACACS request packet
///////////////////////////////////////////////////////////////////////////////
	char szBuffer[1024] = "";
	xtacacstype *pTacs = (xtacacstype *) szBuffer;

	ZeroData(szBuffer);

	pTacs->type = XTA_LOGIN;
	pTacs->version = XTA_VERSION;
	pTacs->trans = htons((unsigned short) SysGetCurrentThreadId());
	pTacs->reason = XTA_A_NONE;
	pTacs->dhost = (SYS_UINT32) SysGetAddrAddress(SockAddr);
	pTacs->dport = htons((unsigned short) iServicePort);
	pTacs->lport = htons(TACACS_LPORT);
	pTacs->namelen = (SYS_UINT8) strlen(pszUsername);
	pTacs->pwlen = (SYS_UINT8) strlen(pszPassword);

	int iQueryLenght = XTACACSSIZE + pTacs->namelen + pTacs->pwlen;

	memcpy(&szBuffer[XTACACSSIZE], pszUsername, pTacs->namelen);
	memcpy(&szBuffer[XTACACSSIZE + pTacs->namelen], pszPassword, pTacs->pwlen);

	for (int iSendLoops = 0; iSendLoops < TACACS_SEND_RETRIES; iSendLoops++) {
///////////////////////////////////////////////////////////////////////////////
//  Send packet
///////////////////////////////////////////////////////////////////////////////
		if (SysSendDataTo(SockFD, (const struct sockaddr *) &SvrAddr, sizeof(SvrAddr),
				  szBuffer, iQueryLenght, TACACS_TIMEOUT) != iQueryLenght)
			continue;

///////////////////////////////////////////////////////////////////////////////
//  Receive packet lenght
///////////////////////////////////////////////////////////////////////////////
		SYS_INET_ADDR RecvAddr;
		SYS_UINT8 RespBuffer[1024];

		ZeroData(RecvAddr);
		ZeroData(RespBuffer);

		int iPacketLenght =
		    SysRecvDataFrom(SockFD, (struct sockaddr *) &RecvAddr, sizeof(RecvAddr),
				    (char *) RespBuffer, sizeof(RespBuffer), TACACS_TIMEOUT);

		if (iPacketLenght < XTACACSSIZE)
			continue;

		xtacacstype *pRespTacs = (xtacacstype *) RespBuffer;

		if (pRespTacs->response != 1) {
			SysCloseSocket(SockFD);

			ErrSetErrorCode(ERR_TACACS_AUTH_FAILED);
			return (ERR_TACACS_AUTH_FAILED);
		}

		SysCloseSocket(SockFD);

		return (0);
	}

	SysCloseSocket(SockFD);

	ErrSetErrorCode(ERR_TACACS_AUTH_UNAVAILABLE);

	return (ERR_TACACS_AUTH_UNAVAILABLE);

}
