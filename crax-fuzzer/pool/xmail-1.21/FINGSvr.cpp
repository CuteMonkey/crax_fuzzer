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
#include "SList.h"
#include "BuffSock.h"
#include "StrUtils.h"
#include "ResLocks.h"
#include "MiscUtils.h"
#include "SvrUtils.h"
#include "UsrUtils.h"
#include "ExtAliases.h"
#include "UsrMailList.h"
#include "MessQueue.h"
#include "SMTPUtils.h"
#include "MailConfig.h"
#include "AppDefines.h"
#include "MailSvr.h"
#include "FINGSvr.h"

#define FINGSRV_ACCEPT_TIMEOUT  4
#define FING_LISTEN_SIZE        64
#define FING_WAIT_SLEEP         2
#define MAX_CLIENTS_WAIT        300
#define FING_IPMAP_FILE         "finger.ipmap.tab"
#define FING_LOG_FILE           "finger"
#define FING_SERVER_NAME        "[" APP_NAME_VERSION_STR " FINGER Server]"

static int FINGCheckPeerIP(SYS_SOCKET SockFD);
static FINGConfig *FINGGetConfigCopy(SHB_HANDLE hShbFING);
static int FINGLogEnabled(SHB_HANDLE hShbFING, FINGConfig * pFINGCfg = NULL);
static unsigned int FINGClientThread(void *pThreadData);
static int FINGLogSession(char const *pszSockHost, char const *pszSockDomain,
			  SYS_INET_ADDR & PeerInfo, char const *pszQuery);
static int FINGHandleSession(SHB_HANDLE hShbFING, BSOCK_HANDLE hBSock);
static int FINGProcessQuery(char const *pszQuery, BSOCK_HANDLE hBSock,
			    FINGConfig * pFINGCfg, char const *pszSockDomain,
			    SVRCFG_HANDLE hSvrConfig);
static int FINGDumpUser(char const *pszUser, char const *pszDomain,
			BSOCK_HANDLE hBSock, FINGConfig * pFINGCfg);
static int FINGDumpMailingList(UserInfo * pUI, BSOCK_HANDLE hBSock, FINGConfig * pFINGCfg);

static int FINGCheckPeerIP(SYS_SOCKET SockFD)
{

	char szIPMapFile[SYS_MAX_PATH] = "";

	CfgGetRootPath(szIPMapFile, sizeof(szIPMapFile));
	StrNCat(szIPMapFile, FING_IPMAP_FILE, sizeof(szIPMapFile));

	if (SysExistFile(szIPMapFile)) {
		SYS_INET_ADDR PeerInfo;

		if (SysGetPeerInfo(SockFD, PeerInfo) < 0)
			return (ErrGetErrorCode());

		if (MscCheckAllowedIP(szIPMapFile, PeerInfo, true) < 0)
			return (ErrGetErrorCode());
	}

	return (0);

}

static FINGConfig *FINGGetConfigCopy(SHB_HANDLE hShbFING)
{

	FINGConfig *pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

	if (pFINGCfg == NULL)
		return (NULL);

	FINGConfig *pFINGCfgCopy = (FINGConfig *) SysAlloc(sizeof(FINGConfig));

	if (pFINGCfgCopy != NULL)
		memcpy(pFINGCfgCopy, pFINGCfg, sizeof(FINGConfig));

	ShbUnlock(hShbFING);

	return (pFINGCfgCopy);

}

static int FINGLogEnabled(SHB_HANDLE hShbFING, FINGConfig * pFINGCfg)
{

	int iDoUnlock = 0;

	if (pFINGCfg == NULL) {
		if ((pFINGCfg = (FINGConfig *) ShbLock(hShbFING)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	unsigned long ulFlags = pFINGCfg->ulFlags;

	if (iDoUnlock)
		ShbUnlock(hShbFING);

	return ((ulFlags & FINGF_LOG_ENABLED) ? 1 : 0);

}

static unsigned int FINGClientThread(void *pThreadData)
{

	SYS_SOCKET SockFD = (SYS_SOCKET) (unsigned long) pThreadData;

///////////////////////////////////////////////////////////////////////////////
//  Check peer IP address serivce access permissions
///////////////////////////////////////////////////////////////////////////////
	if (FINGCheckPeerIP(SockFD) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SysCloseSocket(SockFD);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Increase threads count
///////////////////////////////////////////////////////////////////////////////
	FINGConfig *pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

	if (pFINGCfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SysCloseSocket(SockFD);
		return (ErrorPop());
	}

	++pFINGCfg->lThreadCount;

	ShbUnlock(hShbFING);

///////////////////////////////////////////////////////////////////////////////
//  Link socket to the bufferer
///////////////////////////////////////////////////////////////////////////////
	BSOCK_HANDLE hBSock = BSckAttach(SockFD);

	if (hBSock == INVALID_BSOCK_HANDLE) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SysCloseSocket(SockFD);
		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Handle client session
///////////////////////////////////////////////////////////////////////////////
	FINGHandleSession(hShbFING, hBSock);

///////////////////////////////////////////////////////////////////////////////
//  Unlink socket to the bufferer and close it
///////////////////////////////////////////////////////////////////////////////
	BSckDetach(hBSock, 1);

///////////////////////////////////////////////////////////////////////////////
//  Decrease thread count
///////////////////////////////////////////////////////////////////////////////
	pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

	if (pFINGCfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		return (ErrorPop());
	}

	--pFINGCfg->lThreadCount;

	ShbUnlock(hShbFING);

	return (0);

}

unsigned int FINGThreadProc(void *pThreadData)
{

	FINGConfig *pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

	if (pFINGCfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		return (ErrorPop());
	}

	int iNumSockFDs = 0;
	SYS_SOCKET SockFDs[MAX_FING_ACCEPT_ADDRESSES];

	if (MscCreateServerSockets(pFINGCfg->iNumAddr, pFINGCfg->SvrAddr, pFINGCfg->iPort,
				   FING_LISTEN_SIZE, SockFDs, iNumSockFDs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		ShbUnlock(hShbFING);
		return (ErrorPop());
	}

	ShbUnlock(hShbFING);

	SysLogMessage(LOG_LEV_MESSAGE, "%s started\n", FING_SERVER_NAME);

	for (;;) {
		int iNumConnSockFD = 0;
		SYS_SOCKET ConnSockFD[MAX_FING_ACCEPT_ADDRESSES];

		if (MscAcceptServerConnection(SockFDs, iNumSockFDs, ConnSockFD,
					      iNumConnSockFD, FINGSRV_ACCEPT_TIMEOUT) < 0) {
			unsigned long ulFlags = FINGF_STOP_SERVER;

			pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

			if (pFINGCfg != NULL)
				ulFlags = pFINGCfg->ulFlags;

			ShbUnlock(hShbFING);

			if (ulFlags & FINGF_STOP_SERVER)
				break;
			else
				continue;
		}

		for (int ss = 0; ss < iNumConnSockFD; ss++) {
			SYS_THREAD hClientThread =
			    SysCreateServiceThread(FINGClientThread, ConnSockFD[ss]);

			if (hClientThread != SYS_INVALID_THREAD)
				SysCloseThread(hClientThread, 0);
			else
				SysCloseSocket(ConnSockFD[ss]);

		}
	}

	for (int ss = 0; ss < iNumSockFDs; ss++)
		SysCloseSocket(SockFDs[ss]);

///////////////////////////////////////////////////////////////////////////////
//  Wait for clients completion
///////////////////////////////////////////////////////////////////////////////
	for (int iTotalWait = 0; (iTotalWait < MAX_CLIENTS_WAIT); iTotalWait += FING_WAIT_SLEEP) {
		pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

		if (pFINGCfg == NULL)
			break;

		long lThreadCount = pFINGCfg->lThreadCount;

		ShbUnlock(hShbFING);

		if (lThreadCount == 0)
			break;

		SysSleep(FING_WAIT_SLEEP);
	}

	SysLogMessage(LOG_LEV_MESSAGE, "%s stopped\n", FING_SERVER_NAME);

	return (0);

}

static int FINGLogSession(char const *pszSockHost, char const *pszSockDomain,
			  SYS_INET_ADDR & PeerInfo, char const *pszQuery)
{

	char szTime[256] = "";

	MscGetTimeNbrString(szTime, sizeof(szTime) - 1);

	RLCK_HANDLE hResLock = RLckLockEX(SVR_LOGS_DIR SYS_SLASH_STR FING_LOG_FILE);

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	char szIP[128] = "???.???.???.???";

	MscFileLog(FING_LOG_FILE, "\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\n", pszSockHost, pszSockDomain, SysInetNToA(PeerInfo, szIP),
		   szTime, pszQuery);

	RLckUnlockEX(hResLock);

	return (0);

}

static int FINGHandleSession(SHB_HANDLE hShbFING, BSOCK_HANDLE hBSock)
{

	FINGConfig *pFINGCfg = FINGGetConfigCopy(hShbFING);

///////////////////////////////////////////////////////////////////////////////
//  Get client socket info
///////////////////////////////////////////////////////////////////////////////
	SYS_INET_ADDR PeerInfo;

	if (SysGetPeerInfo(BSckGetAttachedSocket(hBSock), PeerInfo) < 0) {
		ErrorPush();
		BSckSendString(hBSock, ErrGetErrorString(ErrorFetch()), pFINGCfg->iTimeout);
		SysFree(pFINGCfg);

		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString(ErrorFetch()));
		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Get server socket FQDN
///////////////////////////////////////////////////////////////////////////////
	char szSvrFQDN[MAX_HOST_NAME] = "";

	if (MscGetSockHost(BSckGetAttachedSocket(hBSock), szSvrFQDN) < 0) {
		ErrorPush();
		BSckSendString(hBSock, ErrGetErrorString(ErrorFetch()), pFINGCfg->iTimeout);
		SysFree(pFINGCfg);

		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString(ErrorFetch()));
		return (ErrorPop());
	}

	char szSockHost[MAX_HOST_NAME] = "";
	char szSockDomain[MAX_HOST_NAME] = "";

	MscSplitFQDN(szSvrFQDN, szSockHost, szSockDomain);

	char szIP[128] = "???.???.???.???";

	SysLogMessage(LOG_LEV_MESSAGE, "FINGER client connection from [%s]\n",
		      SysInetNToA(PeerInfo, szIP));

	char szQuery[1024] = "";

	if ((BSckGetString(hBSock, szQuery, sizeof(szQuery) - 1, pFINGCfg->iTimeout) != NULL) &&
	    (MscCmdStringCheck(szQuery) == 0)) {
///////////////////////////////////////////////////////////////////////////////
//  Log FINGER question
///////////////////////////////////////////////////////////////////////////////
		if (FINGLogEnabled(hShbFING))
			FINGLogSession(szSockHost, szSockDomain, PeerInfo, szQuery);

		SysLogMessage(LOG_LEV_MESSAGE, "FINGER query [%s] : \"%s\"\n",
			      SysInetNToA(PeerInfo, szIP), szQuery);

		SVRCFG_HANDLE hSvrConfig = SvrGetConfigHandle();

		FINGProcessQuery(szQuery, hBSock, pFINGCfg, szSockDomain, hSvrConfig);

		if (hSvrConfig != INVALID_SVRCFG_HANDLE)
			SvrReleaseConfigHandle(hSvrConfig);
	}

	SysFree(pFINGCfg);

	SysLogMessage(LOG_LEV_MESSAGE, "FINGER client exit [%s]\n", SysInetNToA(PeerInfo, szIP));

	return (0);

}

static int FINGProcessQuery(char const *pszQuery, BSOCK_HANDLE hBSock,
			    FINGConfig * pFINGCfg, char const *pszSockDomain,
			    SVRCFG_HANDLE hSvrConfig)
{

///////////////////////////////////////////////////////////////////////////////
//  Check for verbose query
///////////////////////////////////////////////////////////////////////////////
	int iVerbose = 0;

	if (pszQuery[0] == '/') {
		if (toupper(pszQuery[1]) != 'W') {
			BSckSendString(hBSock, "Invalid query", pFINGCfg->iTimeout);

			ErrSetErrorCode(ERR_FINGER_QUERY_FORMAT);
			return (ERR_FINGER_QUERY_FORMAT);
		}

		++iVerbose;
		pszQuery += 2;
	}
///////////////////////////////////////////////////////////////////////////////
//  Discard spaces
///////////////////////////////////////////////////////////////////////////////
	for (; *pszQuery == ' '; pszQuery++);

	if (*pszQuery == '\0') {
		BSckSendString(hBSock, "Empty query not allowed", pFINGCfg->iTimeout);

		ErrSetErrorCode(ERR_FINGER_QUERY_FORMAT);
		return (ERR_FINGER_QUERY_FORMAT);
	}
///////////////////////////////////////////////////////////////////////////////
//  Split user-domain
///////////////////////////////////////////////////////////////////////////////
	char szUser[MAX_ADDR_NAME] = "";
	char szDomain[MAX_ADDR_NAME] = "";

	if (strchr(pszQuery, '@') != NULL) {
		if (USmtpSplitEmailAddr(pszQuery, szUser, szDomain) < 0) {
			ErrorPush();

			BSckSendString(hBSock, "Invalid query", pFINGCfg->iTimeout);

			return (ErrorPop());
		}
	} else
		StrSNCpy(szUser, pszQuery);

///////////////////////////////////////////////////////////////////////////////
//  Check if indirect query
///////////////////////////////////////////////////////////////////////////////
	if (strchr(szDomain, '@') != NULL) {
		BSckSendString(hBSock, "Indirect query not allowed", pFINGCfg->iTimeout);

		ErrSetErrorCode(ERR_FINGER_QUERY_FORMAT);
		return (ERR_FINGER_QUERY_FORMAT);
	}
///////////////////////////////////////////////////////////////////////////////
//  Setup domain name in case of username only query
///////////////////////////////////////////////////////////////////////////////
	if (IsEmptyString(szDomain)) {
		if (SvrConfigVar("POP3Domain", szDomain, sizeof(szDomain), hSvrConfig) < 0) {
			if (strlen(pszSockDomain) == 0) {
				if (SvrConfigVar
				    ("RootDomain", szDomain, sizeof(szDomain), hSvrConfig) < 0) {
					ErrorPush();

					BSckSendString(hBSock, "User not found",
						       pFINGCfg->iTimeout);

					return (ErrorPop());
				}
			} else
				StrSNCpy(szDomain, pszSockDomain);
		}
	}
///////////////////////////////////////////////////////////////////////////////
//  Dump user
///////////////////////////////////////////////////////////////////////////////
	if (FINGDumpUser(szUser, szDomain, hBSock, pFINGCfg) < 0) {
		ErrorPush();

		BSckSendString(hBSock, "User not found", pFINGCfg->iTimeout);

		return (ErrorPop());
	}

	return (0);

}

static int FINGDumpUser(char const *pszUser, char const *pszDomain,
			BSOCK_HANDLE hBSock, FINGConfig * pFINGCfg)
{

///////////////////////////////////////////////////////////////////////////////
//  Lookup user
///////////////////////////////////////////////////////////////////////////////
	char szRealAddr[MAX_ADDR_NAME] = "";
	UserInfo *pUI = UsrGetUserByNameOrAlias(pszDomain, pszUser, szRealAddr);

	if (pUI != NULL) {
		if (UsrGetUserType(pUI) == usrTypeUser) {
///////////////////////////////////////////////////////////////////////////////
//  Local user case
///////////////////////////////////////////////////////////////////////////////
			char *pszRealName = UsrGetUserInfoVar(pUI, "RealName");
			char *pszHomePage = UsrGetUserInfoVar(pUI, "HomePage");
			char szRespBuffer[2048] = "";

			sprintf(szRespBuffer,
				"EMail       : %s\r\n"
				"  Real Name : %s\r\n"
				"  Home Page : %s",
				szRealAddr,
				(pszRealName != NULL) ? pszRealName : "??",
				(pszHomePage != NULL) ? pszHomePage : "??");

			BSckSendString(hBSock, szRespBuffer, pFINGCfg->iTimeout);

			if (pszRealName != NULL)
				SysFree(pszRealName);

			if (pszHomePage != NULL)
				SysFree(pszHomePage);
		} else {
///////////////////////////////////////////////////////////////////////////////
//  Local mailing list case
///////////////////////////////////////////////////////////////////////////////

			FINGDumpMailingList(pUI, hBSock, pFINGCfg);
		}

		UsrFreeUserInfo(pUI);
	} else {
		ErrSetErrorCode(ERR_USER_NOT_FOUND);
		return (ERR_USER_NOT_FOUND);
	}

	return (0);

}

static int FINGDumpMailingList(UserInfo * pUI, BSOCK_HANDLE hBSock, FINGConfig * pFINGCfg)
{

	USRML_HANDLE hUsersDB = UsrMLOpenDB(pUI);

	if (hUsersDB == INVALID_USRML_HANDLE)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Mailing list scan
///////////////////////////////////////////////////////////////////////////////
	MLUserInfo *pMLUI = UsrMLGetFirstUser(hUsersDB);

	for (; pMLUI != NULL; pMLUI = UsrMLGetNextUser(hUsersDB)) {
		char szUser[MAX_ADDR_NAME] = "";
		char szDomain[MAX_ADDR_NAME] = "";

		if (USmtpSplitEmailAddr(pMLUI->pszAddress, szUser, szDomain) < 0) {
			ErrorPush();
			UsrMLFreeUser(pMLUI);
			UsrMLCloseDB(hUsersDB);
			return (ErrorPop());
		}
///////////////////////////////////////////////////////////////////////////////
//  Dump user
///////////////////////////////////////////////////////////////////////////////
		FINGDumpUser(szUser, szDomain, hBSock, pFINGCfg);

		UsrMLFreeUser(pMLUI);
	}

	UsrMLCloseDB(hUsersDB);

	return (0);

}
