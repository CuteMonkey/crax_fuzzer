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
#include "ResLocks.h"
#include "MiscUtils.h"
#include "StrUtils.h"
#include "MD5.h"
#include "SvrUtils.h"
#include "UsrUtils.h"
#include "UsrAuth.h"
#include "POP3Svr.h"
#include "POP3Utils.h"
#include "MessQueue.h"
#include "MailDomains.h"
#include "MailConfig.h"
#include "AppDefines.h"
#include "MailSvr.h"

#define POP3SRV_ACCEPT_TIMEOUT  4
#define STD_POP3_TIMEOUT        30
#define POP3_LISTEN_SIZE        64
#define POP3_WAIT_SLEEP         2
#define MAX_CLIENTS_WAIT        300
#define POP3_IPMAP_FILE         "pop3.ipmap.tab"
#define POP3_LOG_FILE           "pop3"
#define POP3_SERVER_NAME        "[" APP_NAME_VERSION_STR " POP3 Server]"

enum POP3States {
	stateInit,
	stateUser,
	stateLogged,

	stateExit
};

struct POP3Session {
	int iPOP3State;
	SHB_HANDLE hShbPOP3;
	POP3Config *pPOP3Cfg;
	SVRCFG_HANDLE hSvrConfig;
	int iBadLoginWait;
	SYS_INET_ADDR PeerInfo;
	char szSvrFQDN[MAX_HOST_NAME];
	char szSvrDomain[MAX_HOST_NAME];
	char szUser[MAX_ADDR_NAME];
	char szPassword[256];
	POP3_HANDLE hPOPSession;
	char szTimeStamp[256];
};

static POP3Config *POP3GetConfigCopy(SHB_HANDLE hShbPOP3);
static int POP3ThreadCountAdd(long lCount, SHB_HANDLE hShbPOP3, POP3Config * pPOP3Cfg = NULL);
static int POP3LogEnabled(SHB_HANDLE hShbPOP3, POP3Config * pPOP3Cfg = NULL);
static int POP3CheckPeerIP(SYS_SOCKET SockFD);
static unsigned int POP3ClientThread(void *pThreadData);
static int POP3CheckSysResources(SVRCFG_HANDLE hSvrConfig);
static int POP3InitSession(SHB_HANDLE hShbPOP3, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3LogSession(POP3Session & POP3S);
static int POP3HandleSession(SHB_HANDLE hShbPOP3, BSOCK_HANDLE hBSock);
static void POP3ClearSession(POP3Session & POP3S);
static int POP3HandleCommand(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_USER(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleBadLogin(BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_PASS(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_APOP(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_STAT(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_LIST(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_UIDL(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_QUIT(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_RETR(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_TOP(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_DELE(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_NOOP(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_LAST(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);
static int POP3HandleCmd_RSET(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S);

static POP3Config *POP3GetConfigCopy(SHB_HANDLE hShbPOP3)
{

	POP3Config *pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3);

	if (pPOP3Cfg == NULL)
		return (NULL);

	POP3Config *pPOP3CfgCopy = (POP3Config *) SysAlloc(sizeof(POP3Config));

	if (pPOP3CfgCopy != NULL)
		memcpy(pPOP3CfgCopy, pPOP3Cfg, sizeof(POP3Config));

	ShbUnlock(hShbPOP3);

	return (pPOP3CfgCopy);

}

static int POP3ThreadCountAdd(long lCount, SHB_HANDLE hShbPOP3, POP3Config * pPOP3Cfg)
{

	int iDoUnlock = 0;

	if (pPOP3Cfg == NULL) {
		if ((pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	if ((pPOP3Cfg->lThreadCount + lCount) > pPOP3Cfg->lMaxThreads) {
		if (iDoUnlock)
			ShbUnlock(hShbPOP3);

		ErrSetErrorCode(ERR_SERVER_BUSY);
		return (ERR_SERVER_BUSY);
	}

	pPOP3Cfg->lThreadCount += lCount;

	if (iDoUnlock)
		ShbUnlock(hShbPOP3);

	return (0);

}

static int POP3LogEnabled(SHB_HANDLE hShbPOP3, POP3Config * pPOP3Cfg)
{

	int iDoUnlock = 0;

	if (pPOP3Cfg == NULL) {
		if ((pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	unsigned long ulFlags = pPOP3Cfg->ulFlags;

	if (iDoUnlock)
		ShbUnlock(hShbPOP3);

	return ((ulFlags & POP3F_LOG_ENABLED) ? 1 : 0);

}

static int POP3CheckPeerIP(SYS_SOCKET SockFD)
{

	char szIPMapFile[SYS_MAX_PATH] = "";

	CfgGetRootPath(szIPMapFile, sizeof(szIPMapFile));
	StrNCat(szIPMapFile, POP3_IPMAP_FILE, sizeof(szIPMapFile));

	if (SysExistFile(szIPMapFile)) {
		SYS_INET_ADDR PeerInfo;

		if (SysGetPeerInfo(SockFD, PeerInfo) < 0)
			return (ErrGetErrorCode());

		if (MscCheckAllowedIP(szIPMapFile, PeerInfo, true) < 0)
			return (ErrGetErrorCode());
	}

	return (0);

}

static unsigned int POP3ClientThread(void *pThreadData)
{

	SYS_SOCKET SockFD = (SYS_SOCKET) (unsigned long) pThreadData;

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
//  Check IP permission
///////////////////////////////////////////////////////////////////////////////
	if (POP3CheckPeerIP(SockFD) < 0) {
		ErrorPush();

		UPopSendErrorResponse(hBSock, ErrGetErrorCode(), STD_POP3_TIMEOUT);

		BSckDetach(hBSock, 1);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Increase threads count
///////////////////////////////////////////////////////////////////////////////
	if (POP3ThreadCountAdd(+1, hShbPOP3) < 0) {
		ErrorPush();

		UPopSendErrorResponse(hBSock, ErrGetErrorCode(), STD_POP3_TIMEOUT);

		BSckDetach(hBSock, 1);
		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Handle client session
///////////////////////////////////////////////////////////////////////////////
	POP3HandleSession(hShbPOP3, hBSock);

///////////////////////////////////////////////////////////////////////////////
//  Decrease thread count
///////////////////////////////////////////////////////////////////////////////
	POP3ThreadCountAdd(-1, hShbPOP3);

///////////////////////////////////////////////////////////////////////////////
//  Unlink socket to the bufferer and close it
///////////////////////////////////////////////////////////////////////////////
	BSckDetach(hBSock, 1);

	return (0);

}

unsigned int POP3ThreadProc(void *pThreadData)
{

	POP3Config *pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3);

	if (pPOP3Cfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		return (ErrorPop());
	}

	int iNumSockFDs = 0;
	SYS_SOCKET SockFDs[MAX_POP3_ACCEPT_ADDRESSES];

	if (MscCreateServerSockets(pPOP3Cfg->iNumAddr, pPOP3Cfg->SvrAddr, pPOP3Cfg->iPort,
				   POP3_LISTEN_SIZE, SockFDs, iNumSockFDs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		ShbUnlock(hShbPOP3);
		return (ErrorPop());
	}

	ShbUnlock(hShbPOP3);

	SysLogMessage(LOG_LEV_MESSAGE, "%s started\n", POP3_SERVER_NAME);

	for (;;) {
		int iNumConnSockFD = 0;
		SYS_SOCKET ConnSockFD[MAX_POP3_ACCEPT_ADDRESSES];

		if (MscAcceptServerConnection(SockFDs, iNumSockFDs, ConnSockFD,
					      iNumConnSockFD, POP3SRV_ACCEPT_TIMEOUT) < 0) {
			unsigned long ulFlags = POP3F_STOP_SERVER;

			pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3);

			if (pPOP3Cfg != NULL)
				ulFlags = pPOP3Cfg->ulFlags;

			ShbUnlock(hShbPOP3);

			if (ulFlags & POP3F_STOP_SERVER)
				break;
			else
				continue;
		}

		for (int ss = 0; ss < iNumConnSockFD; ss++) {
			SYS_THREAD hClientThread =
			    SysCreateServiceThread(POP3ClientThread, ConnSockFD[ss]);

			if (hClientThread != SYS_INVALID_THREAD)
				SysCloseThread(hClientThread, 0);
			else
				SysCloseSocket(ConnSockFD[ss]);

		}
	}

	for (int ss = 0; ss < iNumSockFDs; ss++)
		SysCloseSocket(SockFDs[ss]);

///////////////////////////////////////////////////////////////////////////////
//  Wait for client completion
///////////////////////////////////////////////////////////////////////////////
	for (int iTotalWait = 0; (iTotalWait < MAX_CLIENTS_WAIT); iTotalWait += POP3_WAIT_SLEEP) {
		pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3);

		if (pPOP3Cfg == NULL)
			break;

		long lThreadCount = pPOP3Cfg->lThreadCount;

		ShbUnlock(hShbPOP3);

		if (lThreadCount == 0)
			break;

		SysSleep(POP3_WAIT_SLEEP);
	}

	SysLogMessage(LOG_LEV_MESSAGE, "%s stopped\n", POP3_SERVER_NAME);

	return (0);

}

static int POP3CheckSysResources(SVRCFG_HANDLE hSvrConfig)
{
///////////////////////////////////////////////////////////////////////////////
//  Check virtual memory
///////////////////////////////////////////////////////////////////////////////
	int iMinValue = SvrGetConfigInt("Pop3MinVirtMemSpace", -1, hSvrConfig);

	if ((iMinValue > 0) && (SvrCheckVirtMemSpace(1024 * (unsigned long) iMinValue) < 0))
		return (ErrGetErrorCode());

	return (0);

}

static int POP3InitSession(SHB_HANDLE hShbPOP3, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	ZeroData(POP3S);
	POP3S.iPOP3State = stateInit;
	POP3S.hShbPOP3 = hShbPOP3;
	POP3S.hSvrConfig = INVALID_SVRCFG_HANDLE;
	POP3S.pPOP3Cfg = NULL;
	POP3S.hPOPSession = INVALID_POP3_HANDLE;

	if ((POP3S.hSvrConfig = SvrGetConfigHandle()) == INVALID_SVRCFG_HANDLE)
		return (ErrGetErrorCode());

	if ((POP3CheckSysResources(POP3S.hSvrConfig) < 0) ||
	    (SysGetPeerInfo(BSckGetAttachedSocket(hBSock), POP3S.PeerInfo) < 0)) {
		SvrReleaseConfigHandle(POP3S.hSvrConfig);
		return (ErrGetErrorCode());
	}
///////////////////////////////////////////////////////////////////////////////
//  Get connection socket host name
///////////////////////////////////////////////////////////////////////////////
	char szIP[128] = "???.???.???.???";

	if (MscGetSockHost(BSckGetAttachedSocket(hBSock), POP3S.szSvrFQDN) < 0)
		StrSNCpy(POP3S.szSvrFQDN, SysInetNToA(POP3S.PeerInfo, szIP));
	else {
///////////////////////////////////////////////////////////////////////////////
//  Try to get a valid domain from the FQDN
///////////////////////////////////////////////////////////////////////////////
		if (MDomGetClientDomain(POP3S.szSvrFQDN, POP3S.szSvrDomain,
					sizeof(POP3S.szSvrDomain) - 1) < 0)
			StrSNCpy(POP3S.szSvrDomain, POP3S.szSvrFQDN);
	}

///////////////////////////////////////////////////////////////////////////////
//  If "POP3Domain" is defined, it's taken as default POP3 domain that means
//  that users of such domain can log using only the name part of their email
//  address
///////////////////////////////////////////////////////////////////////////////
	char *pszDefDomain = SvrGetConfigVar(POP3S.hSvrConfig, "POP3Domain");

	if (pszDefDomain != NULL) {
		StrSNCpy(POP3S.szSvrDomain, pszDefDomain);

		SysFree(pszDefDomain);
	}
///////////////////////////////////////////////////////////////////////////////
//  As a last tentative We try to get "RootDomain" to set POP3 domain
///////////////////////////////////////////////////////////////////////////////
	if (IsEmptyString(POP3S.szSvrDomain)) {
		char *pszRootDomain = SvrGetConfigVar(POP3S.hSvrConfig, "RootDomain");

		if (pszRootDomain == NULL) {
			SvrReleaseConfigHandle(POP3S.hSvrConfig);
			ErrSetErrorCode(ERR_NO_DOMAIN);
			return (ERR_NO_DOMAIN);
		}

		StrSNCpy(POP3S.szSvrDomain, pszRootDomain);

		SysFree(pszRootDomain);
	}

	if ((POP3S.pPOP3Cfg = POP3GetConfigCopy(hShbPOP3)) == NULL) {
		SvrReleaseConfigHandle(POP3S.hSvrConfig);
		return (ErrGetErrorCode());
	}

	POP3S.iBadLoginWait = POP3S.pPOP3Cfg->iBadLoginWait;

///////////////////////////////////////////////////////////////////////////////
//  Create timestamp for APOP command
///////////////////////////////////////////////////////////////////////////////
	sprintf(POP3S.szTimeStamp, "<%lu.%lu@%s>",
		(unsigned long) time(NULL), SysGetCurrentThreadId(), POP3S.szSvrDomain);

	return (0);

}

static int POP3LogSession(POP3Session & POP3S)
{

	char szTime[256] = "";

	MscGetTimeNbrString(szTime, sizeof(szTime) - 1);

	RLCK_HANDLE hResLock = RLckLockEX(SVR_LOGS_DIR SYS_SLASH_STR POP3_LOG_FILE);

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	char szIP[128] = "???.???.???.???";

	MscFileLog(POP3_LOG_FILE, "\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\n", POP3S.szSvrFQDN, POP3S.szSvrDomain, SysInetNToA(POP3S.PeerInfo, szIP),
		   szTime, POP3S.szUser, POP3S.szPassword);

	RLckUnlockEX(hResLock);

	return (0);

}

static int POP3HandleSession(SHB_HANDLE hShbPOP3, BSOCK_HANDLE hBSock)
{

///////////////////////////////////////////////////////////////////////////////
//  Session structure declaration and init
///////////////////////////////////////////////////////////////////////////////
	POP3Session POP3S;

	if (POP3InitSession(hShbPOP3, hBSock, POP3S) < 0) {
		ErrorPush();

		UPopSendErrorResponse(hBSock, ErrGetErrorCode(), STD_POP3_TIMEOUT);

		return (ErrorPop());
	}

	char szIP[128] = "???.???.???.???";

	SysLogMessage(LOG_LEV_MESSAGE, "POP3 client connection from [%s]\n",
		      SysInetNToA(POP3S.PeerInfo, szIP));

///////////////////////////////////////////////////////////////////////////////
//  Send welcome message
///////////////////////////////////////////////////////////////////////////////
	char szTime[256] = "";

	MscGetTimeStr(szTime, sizeof(szTime) - 1);

	if (BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			    "+OK %s %s service ready; %s", POP3S.szTimeStamp,
			    POP3_SERVER_NAME, szTime) < 0) {
		POP3ClearSession(POP3S);
		return (ErrGetErrorCode());
	}
///////////////////////////////////////////////////////////////////////////////
//  Command loop
///////////////////////////////////////////////////////////////////////////////
	char szCommand[1024] = "";

	while (!SvrInShutdown() && (POP3S.iPOP3State != stateExit) &&
	       (BSckGetString(hBSock, szCommand, sizeof(szCommand) - 1,
			      POP3S.pPOP3Cfg->iSessionTimeout) != NULL) &&
	       (MscCmdStringCheck(szCommand) == 0)) {
///////////////////////////////////////////////////////////////////////////////
//  Retrieve a fresh new copy of configuration and test shutdown flag
///////////////////////////////////////////////////////////////////////////////
		SysFree(POP3S.pPOP3Cfg);

		POP3S.pPOP3Cfg = POP3GetConfigCopy(hShbPOP3);

		if ((POP3S.pPOP3Cfg == NULL) || (POP3S.pPOP3Cfg->ulFlags & POP3F_STOP_SERVER))
			break;

///////////////////////////////////////////////////////////////////////////////
//  Handle coomand
///////////////////////////////////////////////////////////////////////////////
		POP3HandleCommand(szCommand, hBSock, POP3S);

	}

	SysLogMessage(LOG_LEV_MESSAGE, "POP3 client exit [%s]\n",
		      SysInetNToA(POP3S.PeerInfo, szIP));

	POP3ClearSession(POP3S);

	return (0);

}

static void POP3ClearSession(POP3Session & POP3S)
{

	if (POP3S.hPOPSession != INVALID_POP3_HANDLE) {
		UPopReleaseSession(POP3S.hPOPSession, (POP3S.iPOP3State == stateExit) ? 1 : 0);

		POP3S.hPOPSession = INVALID_POP3_HANDLE;
	}

	if (POP3S.hSvrConfig != INVALID_SVRCFG_HANDLE)
		SvrReleaseConfigHandle(POP3S.hSvrConfig), POP3S.hSvrConfig =
		    INVALID_SVRCFG_HANDLE;

	if (POP3S.pPOP3Cfg != NULL)
		SysFree(POP3S.pPOP3Cfg), POP3S.pPOP3Cfg = NULL;

}

static int POP3HandleCommand(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	int iCmdResult = -1;

	if (StrCmdMatch(pszCommand, "USER"))
		iCmdResult = POP3HandleCmd_USER(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "PASS"))
		iCmdResult = POP3HandleCmd_PASS(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "APOP"))
		iCmdResult = POP3HandleCmd_APOP(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "STAT"))
		iCmdResult = POP3HandleCmd_STAT(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "LIST"))
		iCmdResult = POP3HandleCmd_LIST(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "UIDL"))
		iCmdResult = POP3HandleCmd_UIDL(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "QUIT"))
		iCmdResult = POP3HandleCmd_QUIT(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "RETR"))
		iCmdResult = POP3HandleCmd_RETR(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "TOP"))
		iCmdResult = POP3HandleCmd_TOP(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "DELE"))
		iCmdResult = POP3HandleCmd_DELE(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "NOOP"))
		iCmdResult = POP3HandleCmd_NOOP(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "LAST"))
		iCmdResult = POP3HandleCmd_LAST(pszCommand, hBSock, POP3S);
	else if (StrCmdMatch(pszCommand, "RSET"))
		iCmdResult = POP3HandleCmd_RSET(pszCommand, hBSock, POP3S);
	else
		BSckSendString(hBSock, "-ERR Invalid command", POP3S.pPOP3Cfg->iTimeout);

	return (iCmdResult);

}

static int POP3HandleCmd_USER(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateInit) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		POP3S.iPOP3State = stateInit;

		BSckSendString(hBSock, "-ERR Invalid syntax", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	char szAccountUser[MAX_ADDR_NAME] = "";
	char szAccountDomain[MAX_HOST_NAME] = "";

	if (StrSplitString
	    (ppszTokens[1], POP3_USER_SPLITTERS, szAccountUser, sizeof(szAccountUser),
	     szAccountDomain, sizeof(szAccountDomain)) < 0) {
		StrFreeStrings(ppszTokens);

		BSckSendString(hBSock, "-ERR Invalid username", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	StrFreeStrings(ppszTokens);

	StrSNCpy(POP3S.szUser, szAccountUser);

	if (strlen(szAccountDomain) > 0)
		StrSNCpy(POP3S.szSvrDomain, szAccountDomain);

	POP3S.iPOP3State = stateUser;

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			"+OK Password required for %s@%s", POP3S.szUser, POP3S.szSvrDomain);

	return (0);

}

static int POP3HandleBadLogin(BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.pPOP3Cfg->ulFlags & POP3F_HANG_ON_BADLOGIN) {
///////////////////////////////////////////////////////////////////////////////
//  Exit if POP3F_HANG_ON_BADLOGIN is set
///////////////////////////////////////////////////////////////////////////////

		POP3S.iPOP3State = stateExit;

	} else {
///////////////////////////////////////////////////////////////////////////////
//  Otherwise sleep and doubles the sleeptime
///////////////////////////////////////////////////////////////////////////////
		SysSleep(POP3S.iBadLoginWait);
		POP3S.iBadLoginWait += POP3S.iBadLoginWait;

		POP3S.iPOP3State = stateInit;
	}

	BSckSendString(hBSock, "-ERR Invalid auth or access denied", POP3S.pPOP3Cfg->iTimeout);
	return (0);

}

static int POP3HandleCmd_PASS(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateUser) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		POP3S.iPOP3State = stateInit;

		BSckSendString(hBSock, "-ERR Invalid syntax", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	StrSNCpy(POP3S.szPassword, ppszTokens[1]);

	StrFreeStrings(ppszTokens);

///////////////////////////////////////////////////////////////////////////////
//  Log POP3 session
///////////////////////////////////////////////////////////////////////////////
	if (POP3LogEnabled(POP3S.hShbPOP3, POP3S.pPOP3Cfg))
		POP3LogSession(POP3S);

///////////////////////////////////////////////////////////////////////////////
//  Check the presence of external authentication modules. If authentication
//  succeed "pszPassword" is set to NULL that instruct "UPopBuildSession"
//  to not make local authentication
///////////////////////////////////////////////////////////////////////////////
	char const *pszPassword = POP3S.szPassword;
	int iAuthResult = UAthAuthenticateUser(AUTH_SERVICE_POP3,
					       POP3S.szSvrDomain, POP3S.szUser, POP3S.szPassword);

	if (iAuthResult < 0) {
		if (iAuthResult != ERR_NO_EXTERNAL_AUTH_DEFINED) {
			ErrorPush();

			POP3HandleBadLogin(hBSock, POP3S);

			return (ErrorPop());
		}
	} else
		pszPassword = NULL;

///////////////////////////////////////////////////////////////////////////////
//  Create POP3 session
///////////////////////////////////////////////////////////////////////////////
	if ((POP3S.hPOPSession = UPopBuildSession(POP3S.szSvrDomain, POP3S.szUser,
						  pszPassword,
						  &POP3S.PeerInfo)) == INVALID_POP3_HANDLE) {
		ErrorPush();

		POP3HandleBadLogin(hBSock, POP3S);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Save the user connection IP to use for SMTP authentication
///////////////////////////////////////////////////////////////////////////////
	UPopSaveUserIP(POP3S.hPOPSession);

	POP3S.iPOP3State = stateLogged;

	int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
	unsigned long ulMBSize = UPopGetSessionMBSize(POP3S.hPOPSession);

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			"+OK Maildrop has %d messages (%lu bytes)", iMsgCount, ulMBSize);

	return (0);

}

static int POP3HandleCmd_APOP(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateInit) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 3)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		POP3S.iPOP3State = stateInit;

		BSckSendString(hBSock, "-ERR Invalid syntax", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}
///////////////////////////////////////////////////////////////////////////////
//  Process parameters
///////////////////////////////////////////////////////////////////////////////
	char szAccountUser[MAX_ADDR_NAME] = "";
	char szAccountDomain[MAX_HOST_NAME] = "";

	if (StrSplitString
	    (ppszTokens[1], POP3_USER_SPLITTERS, szAccountUser, sizeof(szAccountUser),
	     szAccountDomain, sizeof(szAccountDomain)) < 0) {
		StrFreeStrings(ppszTokens);

		BSckSendString(hBSock, "-ERR Invalid username", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	StrSNCpy(POP3S.szUser, szAccountUser);
	StrSNCpy(POP3S.szPassword, ppszTokens[2]);

	if (strlen(szAccountDomain) > 0)
		StrSNCpy(POP3S.szSvrDomain, szAccountDomain);

	StrFreeStrings(ppszTokens);

///////////////////////////////////////////////////////////////////////////////
//  Log POP3 session
///////////////////////////////////////////////////////////////////////////////
	if (POP3LogEnabled(POP3S.hShbPOP3, POP3S.pPOP3Cfg))
		POP3LogSession(POP3S);

///////////////////////////////////////////////////////////////////////////////
//  Check the presence of external authentication modules. If authentication
//  succeed "pszPassword" is set to NULL that instruct "UPopBuildSession"
//  to not make local authentication
///////////////////////////////////////////////////////////////////////////////
	char const *pszPassword = POP3S.szPassword;
	int iAuthResult = UAthAuthenticateUser(AUTH_SERVICE_POP3,
					       POP3S.szSvrDomain, POP3S.szUser, POP3S.szPassword);

	if (iAuthResult < 0) {
		if (iAuthResult != ERR_NO_EXTERNAL_AUTH_DEFINED) {
			ErrorPush();

			POP3HandleBadLogin(hBSock, POP3S);

			return (ErrorPop());
		}
	} else
		pszPassword = NULL;

///////////////////////////////////////////////////////////////////////////////
//  Do APOP authentication ( only if the external one is not performed )
///////////////////////////////////////////////////////////////////////////////
	if ((pszPassword != NULL) &&
	    (UPopAuthenticateAPOP(POP3S.szSvrDomain, POP3S.szUser, POP3S.szTimeStamp,
				  pszPassword) < 0)) {
		ErrorPush();

		POP3HandleBadLogin(hBSock, POP3S);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Create POP3 session ( the NULL as third parameter force to not perform
//  user authentication that has been done before )
///////////////////////////////////////////////////////////////////////////////
	if ((POP3S.hPOPSession = UPopBuildSession(POP3S.szSvrDomain, POP3S.szUser,
						  NULL, &POP3S.PeerInfo)) == INVALID_POP3_HANDLE)
	{
		ErrorPush();

		POP3HandleBadLogin(hBSock, POP3S);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Save the user connection IP to use for SMTP authentication
///////////////////////////////////////////////////////////////////////////////
	UPopSaveUserIP(POP3S.hPOPSession);

	POP3S.iPOP3State = stateLogged;

	int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
	unsigned long ulMBSize = UPopGetSessionMBSize(POP3S.hPOPSession);

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			"+OK Maildrop has %d messages (%lu bytes)", iMsgCount, ulMBSize);

	return (0);

}

static int POP3HandleCmd_STAT(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
	unsigned long ulMBSize = UPopGetSessionMBSize(POP3S.hPOPSession);

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout, "+OK %d %lu", iMsgCount, ulMBSize);

	return (0);

}

static int POP3HandleCmd_LIST(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgIndex = -1;
	int iNumArgs = sscanf(pszCommand, "%*s %d", &iMsgIndex);

	if (iNumArgs < 1) {
		int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
		int iMsgTotal = UPopGetSessionMsgTotal(POP3S.hPOPSession);
		unsigned long ulMBSize = UPopGetSessionMBSize(POP3S.hPOPSession);

		BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
				"+OK %d %lu", iMsgCount, ulMBSize);

		for (int ii = 0; ii < iMsgTotal; ii++) {
			unsigned long ulMessageSize = 0;

			if (UPopGetMessageSize(POP3S.hPOPSession, ii + 1, ulMessageSize) == 0) {

				BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
						"%d %lu", ii + 1, ulMessageSize);

			}
		}

		BSckSendString(hBSock, ".", POP3S.pPOP3Cfg->iTimeout);
	} else {
		unsigned long ulMessageSize = 0;

		if (UPopGetMessageSize(POP3S.hPOPSession, iMsgIndex, ulMessageSize) < 0)
			BSckSendString(hBSock, "-ERR No such message", POP3S.pPOP3Cfg->iTimeout);
		else {

			BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
					"+OK %d %lu", iMsgIndex, ulMessageSize);

		}

	}

	return (0);

}

static int POP3HandleCmd_UIDL(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgIndex = -1;
	int iNumArgs = sscanf(pszCommand, "%*s %d", &iMsgIndex);

	if (iNumArgs < 1) {
		int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
		int iMsgTotal = UPopGetSessionMsgTotal(POP3S.hPOPSession);

		BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout, "+OK %d", iMsgCount);

		for (int ii = 0; ii < iMsgTotal; ii++) {
			char szMessageUIDL[256] = "";

			if (UPopGetMessageUIDL(POP3S.hPOPSession, ii + 1, szMessageUIDL) == 0) {

				BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
						"%d %s", ii + 1, szMessageUIDL);

			}
		}

		BSckSendString(hBSock, ".", POP3S.pPOP3Cfg->iTimeout);
	} else {
		char szMessageUIDL[256] = "";

		if (UPopGetMessageUIDL(POP3S.hPOPSession, iMsgIndex, szMessageUIDL) < 0)
			BSckSendString(hBSock, "-ERR No such message", POP3S.pPOP3Cfg->iTimeout);
		else {

			BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
					"+OK %d %s", iMsgIndex, szMessageUIDL);

		}

	}

	return (0);

}

static int POP3HandleCmd_QUIT(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	POP3S.iPOP3State = stateExit;

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			"+OK %s closing session", POP3_SERVER_NAME);

	return (0);

}

static int POP3HandleCmd_RETR(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgIndex = -1;

	if (sscanf(pszCommand, "%*s %d", &iMsgIndex) < 1) {
		BSckSendString(hBSock, "-ERR Invalid syntax", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	return (UPopSessionSendMsg(POP3S.hPOPSession, iMsgIndex, hBSock));

}

static int POP3HandleCmd_TOP(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgIndex = -1;
	int iNumLines = 0;

	if (sscanf(pszCommand, "%*s %d %d", &iMsgIndex, &iNumLines) < 2) {
		BSckSendString(hBSock, "-ERR Invalid syntax", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	return (UPopSessionTopMsg(POP3S.hPOPSession, iMsgIndex, iNumLines, hBSock));

}

static int POP3HandleCmd_DELE(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgIndex = -1;

	if (sscanf(pszCommand, "%*s %d", &iMsgIndex) < 1) {
		BSckSendString(hBSock, "-ERR Invalid syntax", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	if (UPopDeleteMessage(POP3S.hPOPSession, iMsgIndex) < 0) {
		UPopSendErrorResponse(hBSock, ErrGetErrorCode(), POP3S.pPOP3Cfg->iTimeout);

		return (ErrGetErrorCode());
	}

	BSckSendString(hBSock, "+OK Message deleted", POP3S.pPOP3Cfg->iTimeout);

	return (0);

}

static int POP3HandleCmd_NOOP(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
	unsigned long ulMBSize = UPopGetSessionMBSize(POP3S.hPOPSession);

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			"+OK Maildrop has %d messages (%lu bytes)", iMsgCount, ulMBSize);

	return (0);

}

static int POP3HandleCmd_LAST(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	int iMsgLast = UPopGetSessionLastAccessed(POP3S.hPOPSession);

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout, "+OK %d", iMsgLast);

	return (0);

}

static int POP3HandleCmd_RSET(const char *pszCommand, BSOCK_HANDLE hBSock, POP3Session & POP3S)
{

	if (POP3S.iPOP3State != stateLogged) {
		BSckSendString(hBSock, "-ERR Command not valid here", POP3S.pPOP3Cfg->iTimeout);
		return (-1);
	}

	UPopResetSession(POP3S.hPOPSession);

	int iMsgCount = UPopGetSessionMsgCurrent(POP3S.hPOPSession);
	unsigned long ulMBSize = UPopGetSessionMBSize(POP3S.hPOPSession);

	BSckVSendString(hBSock, POP3S.pPOP3Cfg->iTimeout,
			"+OK Maildrop has %d messages (%lu bytes)", iMsgCount, ulMBSize);

	return (0);

}
