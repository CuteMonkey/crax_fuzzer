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
#include "BuffSock.h"
#include "MiscUtils.h"
#include "StrUtils.h"
#include "SvrUtils.h"
#include "Base64Enc.h"
#include "DynDNS.h"

#define DYNDNS_REG_TIMEOUT          30

int DynDnsSetup(SVRCFG_HANDLE hSvrConfig)
{

	bool bReleaseHandle = false;

	if (hSvrConfig == INVALID_SVRCFG_HANDLE) {
		if ((hSvrConfig = SvrGetConfigHandle()) == INVALID_SVRCFG_HANDLE)
			return (ErrGetErrorCode());

		bReleaseHandle = true;
	}

	char *pszDynDnsCfg = SvrGetConfigVar(hSvrConfig, "DynDnsSetup");

	if (pszDynDnsCfg != NULL) {
		char **ppszTokens = StrTokenize(pszDynDnsCfg, ", ");

		if (ppszTokens == NULL) {
			ErrorPush();
			SysFree(pszDynDnsCfg);
			if (bReleaseHandle)
				SvrReleaseConfigHandle(hSvrConfig);
			return (ErrorPop());
		}

		SysFree(pszDynDnsCfg);

		int iTokensCount = StrStringsCount(ppszTokens);

		if (iTokensCount < 3) {
			StrFreeStrings(ppszTokens);
			if (bReleaseHandle)
				SvrReleaseConfigHandle(hSvrConfig);

			ErrSetErrorCode(ERR_DYNDNS_CONFIG);
			return (ERR_DYNDNS_CONFIG);
		}

		char *pszUsername = (iTokensCount > 3) ? ppszTokens[3] : NULL;
		char *pszPassword = (iTokensCount > 4) ? ppszTokens[4] : NULL;

		if (DynDnsRegisterDomainHTTP(ppszTokens[0], atoi(ppszTokens[1]), ppszTokens[2],
					     pszUsername, pszPassword) < 0) {
			ErrorPush();
			StrFreeStrings(ppszTokens);
			if (bReleaseHandle)
				SvrReleaseConfigHandle(hSvrConfig);
			return (ErrorPop());
		}

		StrFreeStrings(ppszTokens);
	}

	if (bReleaseHandle)
		SvrReleaseConfigHandle(hSvrConfig);

	return (0);

}

int DynDnsRegisterDomainHTTP(char const *pszServer, int iPortNo,
			     char const *pszHTTPRegString, char const *pszUsername,
			     char const *pszPassword)
{

	SYS_SOCKET SockFD;
	SYS_INET_ADDR SvrAddr;
	SYS_INET_ADDR SockAddr;

	if (MscCreateClientSocket(pszServer, iPortNo, SOCK_STREAM, &SockFD, &SvrAddr,
				  &SockAddr, DYNDNS_REG_TIMEOUT) < 0)
		return (ErrGetErrorCode());

	char szIP[128] = "???.???.???.???";
	char szRegString[512] = "";
	char szHTTPRequest[2048] = "";

	sprintf(szRegString, pszHTTPRegString, SysInetNToA(SockAddr, szIP));

	if ((pszUsername == NULL) || (pszPassword == NULL)) {

		sprintf(szHTTPRequest,
			"GET %s HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Connection: close\r\n" "\r\n", szRegString, pszServer, iPortNo);

	} else {
		sprintf(szHTTPRequest, "%s:%s", pszUsername, pszPassword);

		unsigned int uEnc64Length = 0;
		char szEncAuth[512] = "";

		encode64(szHTTPRequest, strlen(szHTTPRequest), szEncAuth,
			 sizeof(szEncAuth), &uEnc64Length);

		sprintf(szHTTPRequest,
			"GET %s HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Authorization: Basic %s\r\n"
			"Connection: close\r\n"
			"\r\n", szRegString, pszServer, iPortNo, szEncAuth);

	}

	int iRequestLength = strlen(szHTTPRequest);

	if (SysSend(SockFD, szHTTPRequest, iRequestLength, DYNDNS_REG_TIMEOUT) != iRequestLength) {
		ErrorPush();
		SysCloseSocket(SockFD);
		return (ErrorPop());
	}

	char szRespFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szRespFile);

	FILE *pRespFile = fopen(szRespFile, "w+b");

	if (pRespFile == NULL) {
		CheckRemoveFile(szRespFile);
		SysCloseSocket(SockFD);

		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	int iRecvData;
	char szBuffer[512] = "";

	while ((iRecvData =
		SysRecvData(SockFD, szBuffer, sizeof(szBuffer), DYNDNS_REG_TIMEOUT)) > 0) {

		fwrite(szBuffer, iRecvData, 1, pRespFile);

	}

	SysCloseSocket(SockFD);

///////////////////////////////////////////////////////////////////////////////
//  Parse HTTP response
///////////////////////////////////////////////////////////////////////////////
	fseek(pRespFile, 0, SEEK_SET);

	fclose(pRespFile);

	SysRemove(szRespFile);

	return (0);

}
