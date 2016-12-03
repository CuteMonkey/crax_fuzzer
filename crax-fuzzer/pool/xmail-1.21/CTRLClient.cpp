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
#include "MD5.h"
#include "Errors.h"

#define STD_CTRL_PORT               6017
#define STD_CTRL_TIMEOUT            90
#define CTRL_LISTFOLLOW_RESULT      100
#define CTRL_WAITDATA_RESULT        101
#define CCLN_ERR_BAD_USAGE          (-10000)

///////////////////////////////////////////////////////////////////////////////
//  Needed by library functions ( START )
///////////////////////////////////////////////////////////////////////////////
bool bServerDebug = false;
int iLogRotateHours = 24;

char *SvrGetLogsDir(char *pszLogsDir, int iMaxPath)
{

	SysSNPrintf(pszLogsDir, iMaxPath - 1, ".");

	return (pszLogsDir);

}

///////////////////////////////////////////////////////////////////////////////
//  Needed by library functions ( END )
///////////////////////////////////////////////////////////////////////////////

int CClnGetResponse(BSOCK_HANDLE hBSock, char *pszError, int iMaxError,
		    int *piErrorCode, int iTimeout)
{

	char szRespBuffer[2048] = "";

	if (BSckGetString(hBSock, szRespBuffer, sizeof(szRespBuffer) - 1, iTimeout) == NULL)
		return (ErrGetErrorCode());

	if ((szRespBuffer[0] != '+') && (szRespBuffer[0] != '-')) {
		ErrSetErrorCode(ERR_CCLN_INVALID_RESPONSE, szRespBuffer);
		return (ERR_CCLN_INVALID_RESPONSE);
	}

	char *pszToken = szRespBuffer + 1;

	if (piErrorCode != NULL)
		*piErrorCode = atoi(pszToken) * ((szRespBuffer[0] == '-') ? -1 : +1);

	for (; isdigit(*pszToken); pszToken++);

	if (*pszToken != ' ') {
		ErrSetErrorCode(ERR_CCLN_INVALID_RESPONSE, szRespBuffer);
		return (ERR_CCLN_INVALID_RESPONSE);
	}

	for (; *pszToken == ' '; pszToken++);

	if (pszError != NULL) {
		strncpy(pszError, pszToken, iMaxError);
		pszError[iMaxError - 1] = '\0';
	}

	return (0);

}

int CClnRecvTextFile(const char *pszFileName, BSOCK_HANDLE hBSock, int iTimeout)
{

	bool bCloseFile = false;
	FILE *pFile = stdout;

	if (pszFileName != NULL) {
		if ((pFile = fopen(pszFileName, "wt")) == NULL) {
			ErrSetErrorCode(ERR_FILE_CREATE, pszFileName);
			return (ERR_FILE_CREATE);
		}

		bCloseFile = true;
	}

	char szBuffer[2048] = "";

	while (BSckGetString(hBSock, szBuffer, sizeof(szBuffer) - 1, iTimeout) != NULL) {
		if (strcmp(szBuffer, ".") == 0)
			break;

		if (szBuffer[0] == '.')
			fprintf(pFile, "%s\n", szBuffer + 1);
		else
			fprintf(pFile, "%s\n", szBuffer);
	}

	if (bCloseFile)
		fclose(pFile);

	return (0);

}

int CClnSendTextFile(const char *pszFileName, BSOCK_HANDLE hBSock, int iTimeout)
{

	bool bCloseFile = false;
	FILE *pFile = stdin;

	if (pszFileName != NULL) {
		if ((pFile = fopen(pszFileName, "rt")) == NULL) {
			ErrSetErrorCode(ERR_FILE_OPEN, pszFileName);
			return (ERR_FILE_OPEN);
		}

		bCloseFile = true;
	}

	char szBuffer[2048] = "";

	while (MscFGets(szBuffer, sizeof(szBuffer) - 1, pFile) != NULL) {
		if (szBuffer[0] == '.')
			for (int ii = strlen(szBuffer); ii >= 0; ii--)
				szBuffer[ii + 1] = szBuffer[ii];

		if (BSckSendString(hBSock, szBuffer, iTimeout) <= 0) {
			fclose(pFile);
			return (ErrGetErrorCode());
		}
	}

	if (bCloseFile)
		fclose(pFile);

	return (BSckSendString(hBSock, ".", iTimeout));

}

int CClnSubmitCommand(BSOCK_HANDLE hBSock, char const *pszCommand,
		      char *pszError, int iMaxError, char const *pszIOFile, int iTimeout)
{

	if (BSckSendString(hBSock, pszCommand, iTimeout) < 0)
		return (ErrGetErrorCode());

	int iErrorCode = 0;

	if (CClnGetResponse(hBSock, pszError, iMaxError, &iErrorCode, iTimeout) < 0)
		return (ErrGetErrorCode());

	if (iErrorCode < 0) {
		ErrSetErrorCode(ERR_CCLN_ERROR_RESPONSE, pszError);
		return (ERR_CCLN_ERROR_RESPONSE);
	}

	if (iErrorCode == CTRL_LISTFOLLOW_RESULT) {

		if (CClnRecvTextFile(pszIOFile, hBSock, iTimeout) < 0)
			return (ErrGetErrorCode());

	} else if (iErrorCode == CTRL_WAITDATA_RESULT) {

		if (CClnSendTextFile(pszIOFile, hBSock, iTimeout) < 0)
			return (ErrGetErrorCode());

		if (CClnGetResponse(hBSock, pszError, iMaxError, &iErrorCode, iTimeout) < 0)
			return (ErrGetErrorCode());

	}

	return (0);

}

BSOCK_HANDLE CClnConnectServer(char const *pszServer, int iPortNo,
			       char const *pszUsername, char const *pszPassword,
			       bool bUseMD5Auth, int iTimeout)
{
///////////////////////////////////////////////////////////////////////////////
//  Get server address
///////////////////////////////////////////////////////////////////////////////
	SYS_INET_ADDR SvrAddr;

	if (MscGetServerAddress(pszServer, SvrAddr, iPortNo) < 0)
		return (INVALID_BSOCK_HANDLE);

///////////////////////////////////////////////////////////////////////////////
//  Try connect to server
///////////////////////////////////////////////////////////////////////////////
	SYS_SOCKET SockFD = SysCreateSocket(AF_INET, SOCK_STREAM, 0);

	if (SockFD == SYS_INVALID_SOCKET)
		return (INVALID_BSOCK_HANDLE);

	if (SysConnect(SockFD, &SvrAddr, sizeof(SvrAddr), iTimeout) < 0) {
		SysCloseSocket(SockFD);
		return (INVALID_BSOCK_HANDLE);
	}

	BSOCK_HANDLE hBSock = BSckAttach(SockFD);

	if (hBSock == INVALID_BSOCK_HANDLE) {
		SysCloseSocket(SockFD);
		return (INVALID_BSOCK_HANDLE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Read welcome message
///////////////////////////////////////////////////////////////////////////////
	int iErrorCode = 0;
	char szRTXBuffer[2048] = "";

	if (CClnGetResponse(hBSock, szRTXBuffer, sizeof(szRTXBuffer), &iErrorCode, iTimeout) < 0) {
		BSckDetach(hBSock, 1);
		return (INVALID_BSOCK_HANDLE);
	}

	if (iErrorCode < 0) {
		BSckDetach(hBSock, 1);

		ErrSetErrorCode(ERR_CCLN_ERROR_RESPONSE, szRTXBuffer);
		return (INVALID_BSOCK_HANDLE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Prepare login
///////////////////////////////////////////////////////////////////////////////
	char szTimeStamp[256] = "";

	if (!bUseMD5Auth ||
	    (MscExtractServerTimeStamp(szRTXBuffer, szTimeStamp, sizeof(szTimeStamp)) == NULL))
		sprintf(szRTXBuffer, "\"%s\"\t\"%s\"", pszUsername, pszPassword);
	else {
///////////////////////////////////////////////////////////////////////////////
//  Perform MD5 authentication
///////////////////////////////////////////////////////////////////////////////
		char *pszHash = StrSprint("%s%s", szTimeStamp, pszPassword);

		if (pszHash == NULL) {
			BSckDetach(hBSock, 1);
			return (INVALID_BSOCK_HANDLE);
		}

		char szMD5[128] = "";

		do_md5_string(pszHash, strlen(pszHash), szMD5);

		SysFree(pszHash);

///////////////////////////////////////////////////////////////////////////////
//  Add a  #  char in head of password field
///////////////////////////////////////////////////////////////////////////////
		sprintf(szRTXBuffer, "\"%s\"\t\"#%s\"", pszUsername, szMD5);
	}

///////////////////////////////////////////////////////////////////////////////
//  Send login
///////////////////////////////////////////////////////////////////////////////
	if (BSckSendString(hBSock, szRTXBuffer, iTimeout) < 0) {
		BSckDetach(hBSock, 1);
		return (INVALID_BSOCK_HANDLE);
	}

	if (CClnGetResponse(hBSock, szRTXBuffer, sizeof(szRTXBuffer), &iErrorCode, iTimeout) < 0) {
		BSckDetach(hBSock, 1);
		return (INVALID_BSOCK_HANDLE);
	}

	if (iErrorCode < 0) {
		BSckDetach(hBSock, 1);

		ErrSetErrorCode(ERR_CCLN_ERROR_RESPONSE, szRTXBuffer);
		return (INVALID_BSOCK_HANDLE);
	}

	return (hBSock);

}

int CClnQuitConnection(BSOCK_HANDLE hBSock, int iTimeout)
{

	CClnSubmitCommand(hBSock, "\"quit\"", NULL, 0, NULL, iTimeout);

	BSckDetach(hBSock, 1);

	return (0);

}

int CClnLogError(void)
{

	char *pszError = ErrGetErrorStringInfo(ErrGetErrorCode());

	if (pszError == NULL)
		return (ErrGetErrorCode());

	fprintf(stderr, "%s\n", pszError);

	SysFree(pszError);

	return (0);

}

void CClnShowUsage(char const *pszProgName)
{

	fprintf(stderr,
		"use :  %s  [-snuptfc]  ...\n"
		"options :\n"
		"       -s server        = set server address\n"
		"       -n port          = set server port [%d]\n"
		"       -u user          = set username\n"
		"       -p pass          = set password\n"
		"       -t timeout       = set timeout [%d]\n"
		"       -f filename      = set I/O filename [stdin/stdout]\n"
		"       -c               = disable MD5 authentication\n",
		pszProgName, STD_CTRL_PORT, STD_CTRL_TIMEOUT);

}

int CClnExec(int iArgCount, char *pszArgs[])
{

	int ii;
	int iPortNo = STD_CTRL_PORT;
	int iTimeout = STD_CTRL_TIMEOUT;
	bool bUseMD5Auth = true;
	char szServer[MAX_HOST_NAME] = "";
	char szUsername[256] = "";
	char szPassword[256] = "";
	char szIOFile[SYS_MAX_PATH] = "";

	for (ii = 1; ii < iArgCount; ii++) {
		if (pszArgs[ii][0] != '-')
			break;

		switch (pszArgs[ii][1]) {
		case ('s'):
			if (++ii < iArgCount)
				StrSNCpy(szServer, pszArgs[ii]);
			break;

		case ('n'):
			if (++ii < iArgCount)
				iPortNo = atoi(pszArgs[ii]);
			break;

		case ('u'):
			if (++ii < iArgCount)
				StrSNCpy(szUsername, pszArgs[ii]);
			break;

		case ('p'):
			if (++ii < iArgCount)
				StrSNCpy(szPassword, pszArgs[ii]);
			break;

		case ('t'):
			if (++ii < iArgCount)
				iTimeout = atoi(pszArgs[ii]);
			break;

		case ('f'):
			if (++ii < iArgCount)
				StrSNCpy(szIOFile, pszArgs[ii]);
			break;

		case ('c'):
			bUseMD5Auth = false;
			break;

		default:
			return (CCLN_ERR_BAD_USAGE);
		}
	}

	if ((strlen(szServer) == 0) || (strlen(szUsername) == 0) ||
	    (strlen(szPassword) == 0) || (ii == iArgCount))
		return (CCLN_ERR_BAD_USAGE);

	int iFirstParam = ii;
	int iCmdLength = 0;

	for (; ii < iArgCount; ii++)
		iCmdLength += strlen(pszArgs[ii]) + 4;

	char *pszCommand = (char *) SysAlloc(iCmdLength + 1);

	if (pszCommand == NULL)
		return (ErrGetErrorCode());

	for (ii = iFirstParam; ii < iArgCount; ii++) {
		if (ii == iFirstParam)
			sprintf(pszCommand, "\"%s\"", pszArgs[ii]);
		else
			sprintf(pszCommand + strlen(pszCommand), "\t\"%s\"", pszArgs[ii]);
	}

	BSOCK_HANDLE hBSock = CClnConnectServer(szServer, iPortNo, szUsername, szPassword,
						bUseMD5Auth, iTimeout);

	if (hBSock == INVALID_BSOCK_HANDLE) {
		ErrorPush();
		SysFree(pszCommand);
		return (ErrorPop());
	}

	char szRTXBuffer[2048] = "";

	if (CClnSubmitCommand(hBSock, pszCommand, szRTXBuffer, sizeof(szRTXBuffer),
			      (strlen(szIOFile) != 0) ? szIOFile : NULL, iTimeout) < 0) {
		ErrorPush();
		CClnQuitConnection(hBSock, iTimeout);
		SysFree(pszCommand);
		return (ErrorPop());
	}

	SysFree(pszCommand);

	CClnQuitConnection(hBSock, iTimeout);

	return (0);

}

#ifndef __CTRLCLNT_LIBRARY__

int main(int iArgCount, char *pszArgs[])
{

	if (SysInitLibrary() < 0) {
		CClnLogError();
		return (1);
	}

	int iExecResult = CClnExec(iArgCount, pszArgs);

	if (iExecResult == CCLN_ERR_BAD_USAGE) {
		CClnShowUsage(pszArgs[0]);
		SysCleanupLibrary();
		return (2);
	} else if (iExecResult < 0) {
		CClnLogError();
		SysCleanupLibrary();
		return (3);
	}

	SysCleanupLibrary();

	return (0);

}

#endif				// #ifndef __CTRLCLNT_LIBRARY__
