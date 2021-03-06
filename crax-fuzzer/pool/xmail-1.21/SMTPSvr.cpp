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
#include "SList.h"
#include "ShBlocks.h"
#include "BuffSock.h"
#include "ResLocks.h"
#include "StrUtils.h"
#include "UsrUtils.h"
#include "SvrUtils.h"
#include "MessQueue.h"
#include "SMAILUtils.h"
#include "QueueUtils.h"
#include "MiscUtils.h"
#include "Base64Enc.h"
#include "MD5.h"
#include "UsrMailList.h"
#include "SMTPSvr.h"
#include "SMTPUtils.h"
#include "MailDomains.h"
#include "POP3Utils.h"
#include "Filter.h"
#include "MailConfig.h"
#include "AppDefines.h"
#include "MailSvr.h"

#define SMTP_MAX_LINE_SIZE      2048
#define SMTPSRV_ACCEPT_TIMEOUT  4
#define STD_SMTP_TIMEOUT        30
#define SMTP_LISTEN_SIZE        64
#define SMTP_WAIT_SLEEP         2
#define MAX_CLIENTS_WAIT        300
#define SMTP_IPMAP_FILE         "smtp.ipmap.tab"
#define SMTP_IPPROP_FILE        "smtp.ipprop.tab"
#define SMTP_LOG_FILE           "smtp"
#define SMTP_SERVER_NAME        "[" APP_NAME_VERSION_STR " ESMTP Server]"
#define SMTP_PRE_DATA_FILTER    "pre-data"
#define SMTP_POST_DATA_FILTER   "post-data"
#define SMTP_FILTER_REJECT_CODE 3
#define PLAIN_AUTH_PARAM_SIZE   1024
#define LOGIN_AUTH_USERNAME     "Username:"
#define LOGIN_AUTH_PASSWORD     "Password:"
#define SVR_SMTP_AUTH_FILE      "smtpauth.tab"
#define SVR_SMTPAUTH_LINE_MAX   512
#define SVR_SMTP_EXTAUTH_FILE   "smtpextauth.tab"
#define SVR_SMTP_EXTAUTH_LINE_MAX   1024
#define SVR_SMTP_EXTAUTH_TIMEOUT    60
#define SVR_SMTP_EXTAUTH_PRIORITY   SYS_PRIORITY_NORMAL
#define SVR_SMTP_EXTAUTH_SUCCESS    0

#define SMTPF_RELAY_ENABLED     (1 << 0)
#define SMTPF_MAIL_LOCKED       (1 << 1)
#define SMTPF_MAIL_UNLOCKED     (1 << 2)
#define SMTPF_AUTHENTICATED     (1 << 3)
#define SMTPF_VRFY_ENABLED      (1 << 4)
#define SMTPF_MAPPED_IP         (1 << 5)
#define SMTPF_NORDNS_IP         (1 << 6)
#define SMTPF_ETRN_ENABLED      (1 << 7)
#define SMTPF_NOEMIT_AUTH       (1 << 8)
#define SMTPF_WHITE_LISTED      (1 << 9)
#define SMTPF_BLOCKED_IP        (1 << 10)

#define SMTPF_STATIC_MASK       (SMTPF_MAPPED_IP | SMTPF_NORDNS_IP | SMTPF_WHITE_LISTED | SMTPF_BLOCKED_IP)
#define SMTPF_AUTH_MASK         (SMTPF_RELAY_ENABLED | SMTPF_MAIL_UNLOCKED | SMTPF_AUTHENTICATED | \
                                        SMTPF_VRFY_ENABLED | SMTPF_ETRN_ENABLED)
#define SMTPF_RESET_MASK        (SMTPF_AUTH_MASK | SMTPF_STATIC_MASK | SMTPF_NOEMIT_AUTH)

#define SMTP_FILTER_FL_BREAK    (1 << 4)
#define SMTP_FILTER_FL_MASK     SMTP_FILTER_FL_BREAK

enum SMTPStates {
	stateInit = 0,
	stateHelo,
	stateAuthenticated,
	stateMail,
	stateRcpt,

	stateExit
};

struct SMTPSession {
	int iSMTPState;
	SHB_HANDLE hShbSMTP;
	SMTPConfig *pSMTPCfg;
	SVRCFG_HANDLE hSvrConfig;
	SYS_INET_ADDR PeerInfo;
	SYS_INET_ADDR SockInfo;
	int iCmdDelay;
	unsigned long ulMaxMsgSize;
	char szSvrFQDN[MAX_ADDR_NAME];
	char szSvrDomain[MAX_ADDR_NAME];
	char szClientFQDN[MAX_ADDR_NAME];
	char szClientDomain[MAX_ADDR_NAME];
	char szDestDomain[MAX_ADDR_NAME];
	char szLogonUser[128];
	char szMsgFile[SYS_MAX_PATH];
	FILE *pMsgFile;
	char *pszFrom;
	char *pszRcpt;
	char *pszSendRcpt;
	int iRcptCount;
	int iErrorsCount;
	int iErrorsMax;
	SYS_UINT64 ullMessageID;
	char szMessageID[128];
	char szTimeStamp[256];
	unsigned long ulSetupFlags;
	unsigned long ulFlags;
	char *pszCustMsg;
	char szRejMapName[256];
};

enum SmtpAuthFields {
	smtpaUsername = 0,
	smtpaPassword,
	smtpaPerms,

	smtpaMax
};

static SMTPConfig *SMTPGetConfigCopy(SHB_HANDLE hShbSMTP);
static int SMTPLogEnabled(SHB_HANDLE hShbSMTP, SMTPConfig * pSMTPCfg = NULL);
static int SMTPCheckPeerIP(SYS_SOCKET SockFD);
static int SMTPThreadCountAdd(long lCount, SHB_HANDLE hShbSMTP, SMTPConfig * pSMTPCfg = NULL);
static unsigned int SMTPClientThread(void *pThreadData);
static int SMTPCheckSysResources(SVRCFG_HANDLE hSvrConfig);
static int SMTPCheckMapsList(SYS_INET_ADDR const &PeerInfo, char const *pszMapList,
			     char *pszMapName, int iMaxMapName, int &iMapCode);
static int SMTPApplyIPProps(SMTPSession & SMTPS);
static int SMTPDoIPBasedInit(SMTPSession & SMTPS, char *&pszSMTPError);
static int SMTPInitSession(SHB_HANDLE hShbSMTP, BSOCK_HANDLE hBSock,
			   SMTPSession & SMTPS, char *&pszSMTPError);
static int SMTPLoadConfig(SMTPSession & SMTPS, char const *pszSvrConfig);
static int SMTPApplyPerms(SMTPSession & SMTPS, char const *pszPerms);
static int SMTPApplyUserConfig(SMTPSession & SMTPS, UserInfo * pUI);
static int SMTPLogSession(SMTPSession & SMTPS, char const *pszSender,
			  char const *pszRecipient, char const *pszStatus,
			  unsigned long ulMsgSize);
static int SMTPSendError(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszFormat, ...);
static int SMTPHandleSession(SHB_HANDLE hShbSMTP, BSOCK_HANDLE hBSock);
static void SMTPClearSession(SMTPSession & SMTPS);
static void SMTPResetSession(SMTPSession & SMTPS);
static int SMTPHandleCommand(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPCheckReturnPath(const char *pszCommand, char **ppszRetDomains,
			       SMTPSession & SMTPS, char *&pszSMTPError);
static int SMTPTryPopAuthIpCheck(SMTPSession & SMTPS, char const *pszUser, char const *pszDomain);
static int SMTPAddMessageInfo(SMTPSession & SMTPS);
static int SMTPCheckMailParams(const char *pszCommand, char **ppszRetDomains,
			       SMTPSession & SMTPS, char *&pszSMTPError);
static int SMTPHandleCmd_MAIL(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPCheckRelayCapability(SMTPSession & SMTPS, char const *pszDestDomain);
static int SMTPCheckForwardPath(char **ppszFwdDomains, SMTPSession & SMTPS,
				char *&pszRealRcpt, char *&pszSMTPError);
static int SMTPHandleCmd_RCPT(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPGetFilterFile(const char *pszFiltID, char *pszFileName, int iMaxName);
static int SMTPFilterMacroSubstitutes(char **ppszCmdTokens, SMTPSession & SMTPS);
static char *SMTPGetFilterRejMessage(char const *pszMsgFilePath);
static int SMTPLogFilter(SMTPSession & SMTPS, char const * const *ppszExec, int iExecResult,
			 int iExitCode, char const *pszType, char const *pszInfo);
static int SMTPPreFilterExec(SMTPSession & SMTPS, FilterTokens *pToks, char **ppszPEError);
static int SMTPRunFilters(SMTPSession & SMTPS, char const *pszFilterPath, char const *pszType,
			  char *&pszError);
static int SMTPFilterMessage(SMTPSession & SMTPS, const char *pszFiltID, char *&pszError);
static int SMTPHandleCmd_DATA(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPAddReceived(int iType, char const *pszAuth, char const *const *ppszMsgInfo,
			   char const *pszMailFrom, char const *pszRcptTo,
			   char const *pszMessageID, FILE * pMailFile);
static char *SMTPTrimRcptLine(char *pszRcptLn);
static int SMTPSubmitPackedFile(SMTPSession & SMTPS, const char *pszPkgFile);
static int SMTPHandleCmd_HELO(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPHandleCmd_EHLO(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPListExtAuths(FILE * pRespFile, SMTPSession & SMTPS);
static int SMTPExternalAuthSubstitute(char **ppszAuthTokens, char const *pszChallenge,
				      char const *pszDigest, char const *pszSecretsFile);
static int SMTPCreateSecretsFile(char const *pszSecretsFile);
static int SMTPExternalAuthenticate(BSOCK_HANDLE hBSock, SMTPSession & SMTPS,
				    char **ppszAuthTokens);
static int SMTPDoAuthExternal(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthType);
static int SMTPDoAuthPlain(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthParam);
static int SMTPDoAuthLogin(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthParam);
static char *SMTPGetAuthFilePath(char *pszFilePath, int iMaxPath);
static char *SMTPGetExtAuthFilePath(char *pszFilePath, int iMaxPath);
static int SMTPTryApplyLocalAuth(SMTPSession & SMTPS, char const *pszUsername,
				 char const *pszPassword);
static int SMTPGetUserSmtpPerms(UserInfo * pUI, SVRCFG_HANDLE hSvrConfig, char *pszPerms,
				int iMaxPerms);
static int SMTPTryApplyLocalCMD5Auth(SMTPSession & SMTPS, char const *pszChallenge,
				     char const *pszUsername, char const *pszDigest);
static int SMTPTryApplyUsrPwdAuth(SMTPSession & SMTPS, char const *pszUsername,
				  char const *pszPassword);
static int SMTPTryApplyCMD5Auth(SMTPSession & SMTPS, char const *pszChallenge,
				char const *pszUsername, char const *pszDigest);
static int SMTPDoAuthCramMD5(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthParam);
static int SMTPHandleCmd_AUTH(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPSendMultilineResponse(BSOCK_HANDLE hBSock, int iTimeout, FILE * pRespFile);
static int SMTPHandleCmd_RSET(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPHandleCmd_NOOP(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPHandleCmd_HELP(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPHandleCmd_QUIT(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPHandleCmd_VRFY(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);
static int SMTPHandleCmd_ETRN(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS);

static SMTPConfig *SMTPGetConfigCopy(SHB_HANDLE hShbSMTP)
{

	SMTPConfig *pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP);

	if (pSMTPCfg == NULL)
		return (NULL);

	SMTPConfig *pSMTPCfgCopy = (SMTPConfig *) SysAlloc(sizeof(SMTPConfig));

	if (pSMTPCfgCopy != NULL)
		memcpy(pSMTPCfgCopy, pSMTPCfg, sizeof(SMTPConfig));

	ShbUnlock(hShbSMTP);

	return (pSMTPCfgCopy);

}

static int SMTPLogEnabled(SHB_HANDLE hShbSMTP, SMTPConfig * pSMTPCfg)
{

	int iDoUnlock = 0;

	if (pSMTPCfg == NULL) {
		if ((pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	unsigned long ulFlags = pSMTPCfg->ulFlags;

	if (iDoUnlock)
		ShbUnlock(hShbSMTP);

	return ((ulFlags & SMTPF_LOG_ENABLED) ? 1 : 0);

}

static int SMTPCheckPeerIP(SYS_SOCKET SockFD)
{

	char szIPMapFile[SYS_MAX_PATH] = "";

	CfgGetRootPath(szIPMapFile, sizeof(szIPMapFile));
	StrNCat(szIPMapFile, SMTP_IPMAP_FILE, sizeof(szIPMapFile));

	if (SysExistFile(szIPMapFile)) {
		SYS_INET_ADDR PeerInfo;

		if (SysGetPeerInfo(SockFD, PeerInfo) < 0)
			return (ErrGetErrorCode());

		if (MscCheckAllowedIP(szIPMapFile, PeerInfo, true) < 0)
			return (ErrGetErrorCode());
	}

	return (0);

}

static int SMTPThreadCountAdd(long lCount, SHB_HANDLE hShbSMTP, SMTPConfig * pSMTPCfg)
{

	int iDoUnlock = 0;

	if (pSMTPCfg == NULL) {
		if ((pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	if ((pSMTPCfg->lThreadCount + lCount) > pSMTPCfg->lMaxThreads) {
		if (iDoUnlock)
			ShbUnlock(hShbSMTP);

		ErrSetErrorCode(ERR_SERVER_BUSY);
		return (ERR_SERVER_BUSY);
	}

	pSMTPCfg->lThreadCount += lCount;

	if (iDoUnlock)
		ShbUnlock(hShbSMTP);

	return (0);

}

static unsigned int SMTPClientThread(void *pThreadData)
{

	SYS_SOCKET SockFD = (SYS_SOCKET) (unsigned long) pThreadData;

///////////////////////////////////////////////////////////////////////////////
//  Link socket to the bufferer
///////////////////////////////////////////////////////////////////////////////
	BSOCK_HANDLE hBSock = BSckAttach(SockFD);

	if (hBSock == INVALID_BSOCK_HANDLE) {
		ErrorPush();
		SysCloseSocket(SockFD);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check IP permission
///////////////////////////////////////////////////////////////////////////////
	if (SMTPCheckPeerIP(SockFD) < 0) {
		ErrorPush();

		BSckVSendString(hBSock, STD_SMTP_TIMEOUT, "421 %s - %s",
				SMTP_SERVER_NAME, ErrGetErrorString(ErrorFetch()));

		BSckDetach(hBSock, 1);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Increase threads count
///////////////////////////////////////////////////////////////////////////////
	if (SMTPThreadCountAdd(+1, hShbSMTP) < 0) {
		ErrorPush();

		BSckVSendString(hBSock, STD_SMTP_TIMEOUT, "421 %s - %s",
				SMTP_SERVER_NAME, ErrGetErrorString(ErrorFetch()));

		BSckDetach(hBSock, 1);
		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Handle client session
///////////////////////////////////////////////////////////////////////////////
	SMTPHandleSession(hShbSMTP, hBSock);

///////////////////////////////////////////////////////////////////////////////
//  Decrease threads count
///////////////////////////////////////////////////////////////////////////////
	SMTPThreadCountAdd(-1, hShbSMTP);

///////////////////////////////////////////////////////////////////////////////
//  Unlink socket from the bufferer and close it
///////////////////////////////////////////////////////////////////////////////
	BSckDetach(hBSock, 1);

	return (0);

}

unsigned int SMTPThreadProc(void *pThreadData)
{

	SMTPConfig *pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP);

	if (pSMTPCfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		return (ErrorPop());
	}

	int iNumSockFDs = 0;
	SYS_SOCKET SockFDs[MAX_SMTP_ACCEPT_ADDRESSES];

	if (MscCreateServerSockets(pSMTPCfg->iNumAddr, pSMTPCfg->SvrAddr, pSMTPCfg->iPort,
				   SMTP_LISTEN_SIZE, SockFDs, iNumSockFDs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		ShbUnlock(hShbSMTP);
		return (ErrorPop());
	}

	ShbUnlock(hShbSMTP);

	SysLogMessage(LOG_LEV_MESSAGE, "%s started\n", SMTP_SERVER_NAME);

	for (;;) {
		int iNumConnSockFD = 0;
		SYS_SOCKET ConnSockFD[MAX_SMTP_ACCEPT_ADDRESSES];

		if (MscAcceptServerConnection(SockFDs, iNumSockFDs, ConnSockFD,
					      iNumConnSockFD, SMTPSRV_ACCEPT_TIMEOUT) < 0) {
			unsigned long ulFlags = SMTPF_STOP_SERVER;

			pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP);

			if (pSMTPCfg != NULL)
				ulFlags = pSMTPCfg->ulFlags;

			ShbUnlock(hShbSMTP);

			if (ulFlags & SMTPF_STOP_SERVER)
				break;
			else
				continue;
		}

		for (int ss = 0; ss < iNumConnSockFD; ss++) {
			SYS_THREAD hClientThread =
			    SysCreateServiceThread(SMTPClientThread, ConnSockFD[ss]);

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
	for (int iTotalWait = 0; (iTotalWait < MAX_CLIENTS_WAIT); iTotalWait += SMTP_WAIT_SLEEP) {
		pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP);

		if (pSMTPCfg == NULL)
			break;

		long lThreadCount = pSMTPCfg->lThreadCount;

		ShbUnlock(hShbSMTP);

		if (lThreadCount == 0)
			break;

		SysSleep(SMTP_WAIT_SLEEP);
	}

	SysLogMessage(LOG_LEV_MESSAGE, "%s stopped\n", SMTP_SERVER_NAME);

	return (0);

}

static int SMTPCheckSysResources(SVRCFG_HANDLE hSvrConfig)
{
///////////////////////////////////////////////////////////////////////////////
//  Check disk space
///////////////////////////////////////////////////////////////////////////////
	int iMinValue = SvrGetConfigInt("SmtpMinDiskSpace", -1, hSvrConfig);

	if ((iMinValue > 0) && (SvrCheckDiskSpace(1024 * (unsigned long) iMinValue) < 0))
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Check virtual memory
///////////////////////////////////////////////////////////////////////////////
	if (((iMinValue = SvrGetConfigInt("SmtpMinVirtMemSpace", -1, hSvrConfig)) > 0) &&
	    (SvrCheckVirtMemSpace(1024 * (unsigned long) iMinValue) < 0))
		return (ErrGetErrorCode());

	return (0);

}

static int SMTPCheckMapsList(SYS_INET_ADDR const &PeerInfo, char const *pszMapList,
			     char *pszMapName, int iMaxMapName, int &iMapCode)
{

	for (;;) {
		char const *pszColon = strchr(pszMapList, ':');

		if (pszColon == NULL)
			break;

		int iRetCode = atoi(pszColon + 1);
		int iMapLength = Min((int) (pszColon - pszMapList), MAX_HOST_NAME - 1);
		char szMapName[MAX_HOST_NAME] = "";

		strncpy(szMapName, pszMapList, iMapLength);
		szMapName[iMapLength] = '\0';

		if (USmtpDnsMapsContained(PeerInfo, szMapName)) {
			if (pszMapName != NULL)
				StrNCpy(pszMapName, szMapName, iMaxMapName);

			iMapCode = iRetCode;

			char szIP[128] = "???.???.???.???";
			char szMapSpec[MAX_HOST_NAME + 128] = "";

			SysInetNToA(PeerInfo, szIP);
			SysSNPrintf(szMapSpec, sizeof(szMapSpec) - 1, "%s:%s", szMapName, szIP);

			ErrSetErrorCode(ERR_MAPS_CONTAINED, szMapSpec);
			return (ERR_MAPS_CONTAINED);
		}

		if ((pszMapList = strchr(pszColon, ',')) == NULL)
			break;

		++pszMapList;
	}

	return (0);

}

static int SMTPApplyIPProps(SMTPSession & SMTPS)
{

	char szIPPropFile[SYS_MAX_PATH] = "";

	CfgGetRootPath(szIPPropFile, sizeof(szIPPropFile));
	StrNCat(szIPPropFile, SMTP_IPPROP_FILE, sizeof(szIPPropFile));

	int ii;
	char **ppszProps;

	if ((ppszProps = MscGetIPProperties(szIPPropFile, SMTPS.PeerInfo)) == NULL)
		return (0);

	for (ii = 1; ppszProps[ii] != NULL; ii++) {
		int iNameLen;
		char *pszName = ppszProps[ii];
		char *pszVal = strchr(pszName, '=');

		if (pszVal == NULL)
			continue;

		iNameLen = pszVal - pszName;
		pszVal++;

		if (strncmp(pszName, "WhiteList", CStringSize("WhiteList")) == 0) {
			if (atoi(pszVal))
				SMTPS.ulFlags |= SMTPF_WHITE_LISTED;
		}
	}

	StrFreeStrings(ppszProps);

	return (0);

}

static int SMTPDoIPBasedInit(SMTPSession & SMTPS, char *&pszSMTPError)
{

///////////////////////////////////////////////////////////////////////////////
//  Check if SMTP client is in "spammers.tab" file
///////////////////////////////////////////////////////////////////////////////
	char *pszChkInfo = NULL;

	if (USmtpSpammerCheck(SMTPS.PeerInfo, pszChkInfo) < 0) {
		int iMapCode = 1, iErrorCode = ErrGetErrorCode();

		if (pszChkInfo != NULL) {
			char szMapCode[32] = "";

			if (StrParamGet(pszChkInfo, "code", szMapCode, sizeof(szMapCode) - 1))
				iMapCode = atoi(szMapCode);

			SysFree(pszChkInfo);
		}

		if (iMapCode > 0) {
			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, "", "", "SNDRIP=EIPSPAM", 0);

			pszSMTPError = SvrGetConfigVar(SMTPS.hSvrConfig, "SmtpMsgIPBanSpammers");

			ErrSetErrorCode(iErrorCode);
			return (iErrorCode);
		} else if (iMapCode == 0)
			SMTPS.ulFlags |= SMTPF_BLOCKED_IP;
		else
			SMTPS.iCmdDelay = Max(SMTPS.iCmdDelay, Abs(iMapCode));
	}

///////////////////////////////////////////////////////////////////////////////
//  Custom maps checking
///////////////////////////////////////////////////////////////////////////////
	char *pszMapsList = SvrGetConfigVar(SMTPS.hSvrConfig, "CustMapsList");

	if (pszMapsList != NULL) {
		int iMapCode = 0;
		char *pszCfgError = NULL;

		if (SMTPCheckMapsList(SMTPS.PeerInfo, pszMapsList, SMTPS.szRejMapName,
				      sizeof(SMTPS.szRejMapName) - 1, iMapCode) < 0) {
			if (iMapCode == 1) {
				ErrorPush();

				if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg)) {
					char *pszError =
						StrSprint("SNDRIP=EIPMAP (%s)", SMTPS.szRejMapName);

					SMTPLogSession(SMTPS, "", "",
						       (pszError !=
							NULL) ? pszError : "SNDRIP=EIPMAP", 0);

					if (pszError != NULL)
						SysFree(pszError);
				}

				if ((pszCfgError =
				     SvrGetConfigVar(SMTPS.hSvrConfig,
						     "SmtpMsgIPBanMaps")) != NULL) {
					pszSMTPError =
						StrSprint("%s (%s)", pszCfgError, SMTPS.szRejMapName);

					SysFree(pszCfgError);
				} else
					pszSMTPError =
						StrSprint
						("550 Denied due inclusion of your IP inside (%s)",
						 SMTPS.szRejMapName);

				SysFree(pszMapsList);
				return (ErrorPop());
			}

			if (iMapCode == 0)
				SMTPS.ulFlags |= SMTPF_MAPPED_IP;
			else
				SMTPS.iCmdDelay = Max(SMTPS.iCmdDelay, Abs(iMapCode));
		}

		SysFree(pszMapsList);
	}
///////////////////////////////////////////////////////////////////////////////
//  RDNS client check
///////////////////////////////////////////////////////////////////////////////
	int iCheckValue = SvrGetConfigInt("SMTP-RDNSCheck", 0, SMTPS.hSvrConfig);

	if ((iCheckValue != 0) && (SysGetHostByAddr(SMTPS.PeerInfo, SMTPS.szClientFQDN) < 0)) {
		if (iCheckValue > 0)
			SMTPS.ulFlags |= SMTPF_NORDNS_IP;
		else
			SMTPS.iCmdDelay = Max(SMTPS.iCmdDelay, -iCheckValue);
	}

	return (0);

}

static int SMTPInitSession(SHB_HANDLE hShbSMTP, BSOCK_HANDLE hBSock,
			   SMTPSession & SMTPS, char *&pszSMTPError)
{

	ZeroData(SMTPS);
	SMTPS.iSMTPState = stateInit;
	SMTPS.hShbSMTP = hShbSMTP;
	SMTPS.hSvrConfig = INVALID_SVRCFG_HANDLE;
	SMTPS.pSMTPCfg = NULL;
	SMTPS.pMsgFile = NULL;
	SMTPS.pszFrom = NULL;
	SMTPS.pszRcpt = NULL;
	SMTPS.pszSendRcpt = NULL;
	SMTPS.iRcptCount = 0;
	SMTPS.iErrorsCount = 0;
	SMTPS.iErrorsMax = 0;
	SMTPS.iCmdDelay = 0;
	SMTPS.ulMaxMsgSize = 0;
	SetEmptyString(SMTPS.szMessageID);
	SetEmptyString(SMTPS.szDestDomain);
	SetEmptyString(SMTPS.szClientFQDN);
	SetEmptyString(SMTPS.szClientDomain);
	SetEmptyString(SMTPS.szLogonUser);
	SetEmptyString(SMTPS.szRejMapName);
	SMTPS.ulFlags = 0;
	SMTPS.ulSetupFlags = 0;
	SMTPS.pszCustMsg = NULL;

	SysGetTmpFile(SMTPS.szMsgFile);

	if ((SMTPS.hSvrConfig = SvrGetConfigHandle()) == INVALID_SVRCFG_HANDLE)
		return (ErrGetErrorCode());

	if ((SMTPCheckSysResources(SMTPS.hSvrConfig) < 0) ||
	    (SysGetPeerInfo(BSckGetAttachedSocket(hBSock), SMTPS.PeerInfo) < 0) ||
	    (SysGetSockInfo(BSckGetAttachedSocket(hBSock), SMTPS.SockInfo) < 0) ||
	    (SMTPApplyIPProps(SMTPS) < 0)) {
		ErrorPush();
		SvrReleaseConfigHandle(SMTPS.hSvrConfig);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  If the remote IP is not white-listed, do all IP based checks
///////////////////////////////////////////////////////////////////////////////
	if (((SMTPS.ulFlags & SMTPF_WHITE_LISTED) == 0) &&
	    (SMTPDoIPBasedInit(SMTPS, pszSMTPError) < 0)) {
		ErrorPush();
		SvrReleaseConfigHandle(SMTPS.hSvrConfig);
		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Get maximum errors count allowed in an SMTP session
///////////////////////////////////////////////////////////////////////////////
	SMTPS.iErrorsMax = SvrGetConfigInt("SMTP-MaxErrors", 0, SMTPS.hSvrConfig);

///////////////////////////////////////////////////////////////////////////////
//  Setup SMTP domain
///////////////////////////////////////////////////////////////////////////////
	char *pszSvrDomain = SvrGetConfigVar(SMTPS.hSvrConfig, "SmtpServerDomain");
	char szIP[128] = "???.???.???.???";

	if (pszSvrDomain != NULL) {
		StrSNCpy(SMTPS.szSvrDomain, pszSvrDomain);
		StrSNCpy(SMTPS.szSvrFQDN, pszSvrDomain);

		SysFree(pszSvrDomain);
	} else {
		if (MscGetSockHost(BSckGetAttachedSocket(hBSock), SMTPS.szSvrFQDN) < 0)
			StrSNCpy(SMTPS.szSvrFQDN, SysInetNToA(SMTPS.SockInfo, szIP));
		else {
///////////////////////////////////////////////////////////////////////////////
//  Try to get a valid domain from the FQDN
///////////////////////////////////////////////////////////////////////////////
			if (MDomGetClientDomain(SMTPS.szSvrFQDN, SMTPS.szSvrDomain,
						sizeof(SMTPS.szSvrDomain) - 1) < 0)
				StrSNCpy(SMTPS.szSvrDomain, SMTPS.szSvrFQDN);
		}

///////////////////////////////////////////////////////////////////////////////
//  Last attempt, try fetch the "RootDomain" variable ...
///////////////////////////////////////////////////////////////////////////////
		if (IsEmptyString(SMTPS.szSvrDomain)) {
			if ((pszSvrDomain =
			     SvrGetConfigVar(SMTPS.hSvrConfig, "RootDomain")) == NULL) {
				SvrReleaseConfigHandle(SMTPS.hSvrConfig);
				ErrSetErrorCode(ERR_NO_DOMAIN);
				return (ERR_NO_DOMAIN);
			}

			StrSNCpy(SMTPS.szSvrDomain, pszSvrDomain);

			SysFree(pszSvrDomain);
		}
	}

///////////////////////////////////////////////////////////////////////////////
//  Create timestamp
///////////////////////////////////////////////////////////////////////////////
	sprintf(SMTPS.szTimeStamp, "<%lu.%lu@%s>",
		(unsigned long) time(NULL), SysGetCurrentThreadId(), SMTPS.szSvrDomain);

	if ((SMTPS.pSMTPCfg = SMTPGetConfigCopy(hShbSMTP)) == NULL) {
		ErrorPush();
		SvrReleaseConfigHandle(SMTPS.hSvrConfig);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Get maximum accepted message size
///////////////////////////////////////////////////////////////////////////////
	SMTPS.ulMaxMsgSize = 1024 * (unsigned long) SvrGetConfigInt("MaxMessageSize",
								    0, SMTPS.hSvrConfig);

///////////////////////////////////////////////////////////////////////////////
//  Check if the emission of "X-Auth-User:" is diabled
///////////////////////////////////////////////////////////////////////////////
	if (SvrGetConfigInt("DisableEmitAuthUser", 0, SMTPS.hSvrConfig))
		SMTPS.ulFlags |= SMTPF_NOEMIT_AUTH;

///////////////////////////////////////////////////////////////////////////////
//  Try to load specific configuration
///////////////////////////////////////////////////////////////////////////////
	char szConfigName[128] = "";

	sprintf(szConfigName, "SmtpConfig-%s", SysInetNToA(SMTPS.SockInfo, szIP));

	char *pszSvrConfig = SvrGetConfigVar(SMTPS.hSvrConfig, szConfigName);

	if ((pszSvrConfig != NULL) ||
	    ((pszSvrConfig = SvrGetConfigVar(SMTPS.hSvrConfig, "SmtpConfig")) != NULL)) {
		SMTPLoadConfig(SMTPS, pszSvrConfig);

		SysFree(pszSvrConfig);
	}

	SMTPS.ulFlags |= SMTPS.ulSetupFlags;

///////////////////////////////////////////////////////////////////////////////
//  Get custom message to append to the SMTP response
///////////////////////////////////////////////////////////////////////////////
	SMTPS.pszCustMsg = SvrGetConfigVar(SMTPS.hSvrConfig, "CustomSMTPMessage");

	return (0);

}

static int SMTPLoadConfig(SMTPSession & SMTPS, char const *pszSvrConfig)
{

	char **ppszCfgTokens = StrTokenize(pszSvrConfig, ",");

	if (ppszCfgTokens == NULL)
		return (ErrGetErrorCode());

	for (int ii = 0; ppszCfgTokens[ii] != NULL; ii++) {

		if (stricmp(ppszCfgTokens[ii], "mail-auth") == 0)
			SMTPS.ulSetupFlags |= SMTPF_MAIL_LOCKED;

	}

	StrFreeStrings(ppszCfgTokens);

	return (0);

}

static int SMTPApplyPerms(SMTPSession & SMTPS, char const *pszPerms)
{

	for (int ii = 0; pszPerms[ii] != '\0'; ii++) {

		switch (pszPerms[ii]) {
		case ('M'):
			SMTPS.ulFlags |= SMTPF_MAIL_UNLOCKED;
			break;

		case ('R'):
			SMTPS.ulFlags |= SMTPF_RELAY_ENABLED;
			break;

		case ('V'):
			SMTPS.ulFlags |= SMTPF_VRFY_ENABLED;
			break;

		case ('T'):
			SMTPS.ulFlags |= SMTPF_ETRN_ENABLED;
			break;

		case ('Z'):
			SMTPS.ulMaxMsgSize = 0;
			break;
		}

	}

///////////////////////////////////////////////////////////////////////////////
//  Clear bad ip mask and command delay
///////////////////////////////////////////////////////////////////////////////
	SMTPS.ulFlags &= ~(SMTPF_MAPPED_IP | SMTPF_NORDNS_IP | SMTPF_BLOCKED_IP);

	SMTPS.iCmdDelay = 0;

	return (0);

}

static int SMTPApplyUserConfig(SMTPSession & SMTPS, UserInfo * pUI)
{
///////////////////////////////////////////////////////////////////////////////
//  Retrieve and apply permissions
///////////////////////////////////////////////////////////////////////////////
	char *pszValue = NULL;
	char szPerms[128] = "";

	if (SMTPGetUserSmtpPerms(pUI, SMTPS.hSvrConfig, szPerms, sizeof(szPerms) - 1) < 0)
		return (ErrGetErrorCode());

	SMTPApplyPerms(SMTPS, szPerms);

///////////////////////////////////////////////////////////////////////////////
//  Check "MaxMessageSize" override
///////////////////////////////////////////////////////////////////////////////
	if ((pszValue = UsrGetUserInfoVar(pUI, "MaxMessageSize")) != NULL) {
		SMTPS.ulMaxMsgSize = 1024 * (unsigned long) atol(pszValue);

		SysFree(pszValue);
	}
///////////////////////////////////////////////////////////////////////////////
//  Check if the emission of "X-Auth-User:" is diabled
///////////////////////////////////////////////////////////////////////////////
	if ((pszValue = UsrGetUserInfoVar(pUI, "DisableEmitAuthUser")) != NULL) {
		if (atoi(pszValue))
			SMTPS.ulFlags |= SMTPF_NOEMIT_AUTH;
		else
			SMTPS.ulFlags &= ~SMTPF_NOEMIT_AUTH;

		SysFree(pszValue);
	}

	return (0);

}

static int SMTPLogSession(SMTPSession & SMTPS, char const *pszSender,
			  char const *pszRecipient, char const *pszStatus,
			  unsigned long ulMsgSize)
{

	char szTime[256] = "";

	MscGetTimeNbrString(szTime, sizeof(szTime) - 1);

	RLCK_HANDLE hResLock = RLckLockEX(SVR_LOGS_DIR SYS_SLASH_STR SMTP_LOG_FILE);

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	char szIP[128] = "???.???.???.???";

	MscFileLog(SMTP_LOG_FILE, "\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%s\""
		   "\t\"%lu\""
		   "\t\"%s\""
		   "\n", SMTPS.szSvrFQDN, SMTPS.szSvrDomain, SysInetNToA(SMTPS.PeerInfo, szIP),
		   szTime, SMTPS.szClientDomain, SMTPS.szDestDomain, pszSender, pszRecipient,
		   SMTPS.szMessageID, pszStatus, SMTPS.szLogonUser, ulMsgSize,
		   SMTPS.szClientFQDN);

	RLckUnlockEX(hResLock);

	return (0);

}

static int SMTPSendError(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszFormat, ...)
{

	char *pszBuffer = NULL;

	STRSPRINTF(pszBuffer, pszFormat, pszFormat);

	if (pszBuffer == NULL)
		return (ErrGetErrorCode());

	if (SMTPS.pszCustMsg == NULL) {
		if (BSckSendString(hBSock, pszBuffer, SMTPS.pSMTPCfg->iTimeout) < 0) {
			ErrorPush();
			SysFree(pszBuffer);
			return (ErrorPop());
		}
	} else {
		if (BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
				    "%s - %s", pszBuffer, SMTPS.pszCustMsg) < 0) {
			ErrorPush();
			SysFree(pszBuffer);
			return (ErrorPop());
		}
	}

	SysFree(pszBuffer);

///////////////////////////////////////////////////////////////////////////////
//  Increase the number of errors we encountered in this session. If the
//  maximum number of allowed errors is not zero, and the current number of
//  errors exceed the maximum number, we set the state to 'exit' so that the
//  SMTP connection will be dropped
///////////////////////////////////////////////////////////////////////////////
	SMTPS.iErrorsCount++;
	if ((SMTPS.iErrorsMax > 0) && (SMTPS.iErrorsCount >= SMTPS.iErrorsMax)) {
		char szIP[128] = "";

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom != NULL ? SMTPS.pszFrom: "",
				       "", "SMTP=EERRS", 0);

		SysLogMessage(LOG_LEV_MESSAGE, "SMTP forced exit (too many errors: %d) [%s]\n",
			      SMTPS.iErrorsCount, SysInetNToA(SMTPS.PeerInfo, szIP));

		SMTPS.iSMTPState = stateExit;
	}

	return (0);

}

static int SMTPHandleSession(SHB_HANDLE hShbSMTP, BSOCK_HANDLE hBSock)
{

///////////////////////////////////////////////////////////////////////////////
//  Session structure declaration and init
///////////////////////////////////////////////////////////////////////////////
	char *pszSMTPError = NULL;
	SMTPSession SMTPS;

	if (SMTPInitSession(hShbSMTP, hBSock, SMTPS, pszSMTPError) < 0) {
		ErrorPush();
		if (pszSMTPError != NULL) {
			BSckSendString(hBSock, pszSMTPError, STD_SMTP_TIMEOUT);

			SysFree(pszSMTPError);
		} else
			BSckVSendString(hBSock, STD_SMTP_TIMEOUT,
					"421 %s service not available (%d), closing transmission channel",
					SMTP_SERVER_NAME, ErrorFetch());

		return (ErrorPop());
	}

	char szIP[128] = "???.???.???.???";

	SysLogMessage(LOG_LEV_MESSAGE, "SMTP client connection from [%s]\n",
		      SysInetNToA(SMTPS.PeerInfo, szIP));

///////////////////////////////////////////////////////////////////////////////
//  Send welcome message
///////////////////////////////////////////////////////////////////////////////
	char szTime[256] = "";

	MscGetTimeStr(szTime, sizeof(szTime) - 1);

	if (BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
			    "220 %s %s service ready; %s", SMTPS.szTimeStamp,
			    SMTP_SERVER_NAME, szTime) < 0) {
		ErrorPush();
		SMTPClearSession(SMTPS);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Command loop
///////////////////////////////////////////////////////////////////////////////
	char szCommand[1024] = "";

	while (!SvrInShutdown() && (SMTPS.iSMTPState != stateExit) &&
	       (BSckGetString(hBSock, szCommand, sizeof(szCommand) - 1,
			      SMTPS.pSMTPCfg->iSessionTimeout) != NULL) &&
	       (MscCmdStringCheck(szCommand) == 0)) {
///////////////////////////////////////////////////////////////////////////////
//  Retrieve a fresh new configuration copy and test shutdown flag
///////////////////////////////////////////////////////////////////////////////
		SysFree(SMTPS.pSMTPCfg);

		SMTPS.pSMTPCfg = SMTPGetConfigCopy(hShbSMTP);

		if ((SMTPS.pSMTPCfg == NULL) || (SMTPS.pSMTPCfg->ulFlags & SMTPF_STOP_SERVER))
			break;

///////////////////////////////////////////////////////////////////////////////
//  Handle command
///////////////////////////////////////////////////////////////////////////////
		SMTPHandleCommand(szCommand, hBSock, SMTPS);

	}

	SysLogMessage(LOG_LEV_MESSAGE, "SMTP client exit [%s]\n",
		      SysInetNToA(SMTPS.PeerInfo, szIP));

	SMTPClearSession(SMTPS);

	return (0);

}

static void SMTPClearSession(SMTPSession & SMTPS)
{

	if (SMTPS.pMsgFile != NULL)
		fclose(SMTPS.pMsgFile), SMTPS.pMsgFile = NULL;

	SysRemove(SMTPS.szMsgFile);

	if (SMTPS.hSvrConfig != INVALID_SVRCFG_HANDLE)
		SvrReleaseConfigHandle(SMTPS.hSvrConfig), SMTPS.hSvrConfig =
		    INVALID_SVRCFG_HANDLE;

	if (SMTPS.pSMTPCfg != NULL)
		SysFree(SMTPS.pSMTPCfg), SMTPS.pSMTPCfg = NULL;

	if (SMTPS.pszFrom != NULL)
		SysFree(SMTPS.pszFrom), SMTPS.pszFrom = NULL;

	if (SMTPS.pszRcpt != NULL)
		SysFree(SMTPS.pszRcpt), SMTPS.pszRcpt = NULL;

	if (SMTPS.pszSendRcpt != NULL)
		SysFree(SMTPS.pszSendRcpt), SMTPS.pszSendRcpt = NULL;

	if (SMTPS.pszCustMsg != NULL)
		SysFree(SMTPS.pszCustMsg), SMTPS.pszCustMsg = NULL;

}

static void SMTPResetSession(SMTPSession & SMTPS)
{

	SMTPS.ulFlags = SMTPS.ulSetupFlags | (SMTPS.ulFlags & SMTPF_RESET_MASK);
	SMTPS.ullMessageID = 0;
	SMTPS.iRcptCount = 0;
	SetEmptyString(SMTPS.szMessageID);

	if (SMTPS.pMsgFile != NULL)
		fclose(SMTPS.pMsgFile), SMTPS.pMsgFile = NULL;

	SysRemove(SMTPS.szMsgFile);

	SetEmptyString(SMTPS.szDestDomain);

	if (SMTPS.pszFrom != NULL)
		SysFree(SMTPS.pszFrom), SMTPS.pszFrom = NULL;

	if (SMTPS.pszRcpt != NULL)
		SysFree(SMTPS.pszRcpt), SMTPS.pszRcpt = NULL;

	if (SMTPS.pszSendRcpt != NULL)
		SysFree(SMTPS.pszSendRcpt), SMTPS.pszSendRcpt = NULL;

	SMTPS.iSMTPState = (SMTPS.ulFlags & SMTPF_AUTHENTICATED) ? stateAuthenticated :
	    Min(SMTPS.iSMTPState, stateHelo);

}

static int SMTPHandleCommand(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

///////////////////////////////////////////////////////////////////////////////
//  Delay protection against massive spammers
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.iCmdDelay > 0)
		SysSleep(SMTPS.iCmdDelay);

///////////////////////////////////////////////////////////////////////////////
//  Command parsing and processing
///////////////////////////////////////////////////////////////////////////////
	int iCmdResult = -1;

	if (StrINComp(pszCommand, MAIL_FROM_STR) == 0)
		iCmdResult = SMTPHandleCmd_MAIL(pszCommand, hBSock, SMTPS);
	else if (StrINComp(pszCommand, RCPT_TO_STR) == 0)
		iCmdResult = SMTPHandleCmd_RCPT(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "DATA"))
		iCmdResult = SMTPHandleCmd_DATA(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "HELO"))
		iCmdResult = SMTPHandleCmd_HELO(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "EHLO"))
		iCmdResult = SMTPHandleCmd_EHLO(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "AUTH"))
		iCmdResult = SMTPHandleCmd_AUTH(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "RSET"))
		iCmdResult = SMTPHandleCmd_RSET(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "VRFY"))
		iCmdResult = SMTPHandleCmd_VRFY(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "ETRN"))
		iCmdResult = SMTPHandleCmd_ETRN(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "NOOP"))
		iCmdResult = SMTPHandleCmd_NOOP(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "HELP"))
		iCmdResult = SMTPHandleCmd_HELP(pszCommand, hBSock, SMTPS);
	else if (StrCmdMatch(pszCommand, "QUIT"))
		iCmdResult = SMTPHandleCmd_QUIT(pszCommand, hBSock, SMTPS);
	else
		SMTPSendError(hBSock, SMTPS, "500 Syntax error, command unrecognized");

	return (iCmdResult);

}

static int SMTPTryPopAuthIpCheck(SMTPSession & SMTPS, char const *pszUser, char const *pszDomain)
{
///////////////////////////////////////////////////////////////////////////////
//  Load user info
///////////////////////////////////////////////////////////////////////////////
	UserInfo *pUI = UsrGetUserByNameOrAlias(pszDomain, pszUser);

	if (pUI == NULL)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Perform IP file checking
///////////////////////////////////////////////////////////////////////////////
	if (UPopUserIpCheck(pUI, &SMTPS.PeerInfo, SMTPS.pSMTPCfg->uPopAuthExpireTime) < 0) {
		ErrorPush();
		UsrFreeUserInfo(pUI);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Apply user configuration
///////////////////////////////////////////////////////////////////////////////
	if (SMTPApplyUserConfig(SMTPS, pUI) < 0) {
		ErrorPush();
		UsrFreeUserInfo(pUI);
		return (ErrorPop());
	}

	UsrFreeUserInfo(pUI);

	return (0);

}

static int SMTPCheckReturnPath(const char *pszCommand, char **ppszRetDomains,
			       SMTPSession & SMTPS, char *&pszSMTPError)
{

	int iDomainCount = StrStringsCount(ppszRetDomains);

	if (iDomainCount == 0) {
		if (!SvrTestConfigFlag("AllowNullSender", true, SMTPS.hSvrConfig)) {
			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, "", "", "SNDR=EEMPTY", 0);

			pszSMTPError = SysStrDup("501 Syntax error in return path");

			ErrSetErrorCode(ERR_BAD_RETURN_PATH);
			return (ERR_BAD_RETURN_PATH);
		}

		if (SMTPS.pszFrom != NULL)
			SysFree(SMTPS.pszFrom);

		SMTPS.pszFrom = SysStrDup("");

		return (0);
	}

	char szMailerUser[MAX_ADDR_NAME] = "";
	char szMailerDomain[MAX_ADDR_NAME] = "";

	if (USmtpSplitEmailAddr(ppszRetDomains[0], szMailerUser, szMailerDomain) < 0) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, ppszRetDomains[0], "", "SNDR=ESYNTAX", 0);

		pszSMTPError = SysStrDup("501 Syntax error in return path");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check mailer domain for DNS/MX entries
///////////////////////////////////////////////////////////////////////////////
	if (SvrTestConfigFlag("CheckMailerDomain", false, SMTPS.hSvrConfig) &&
	    (USmtpCheckMailDomain(SMTPS.hSvrConfig, szMailerDomain) < 0)) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, ppszRetDomains[0], "", "SNDR=ENODNS", 0);

		pszSMTPError = SysStrDup("505 Your domain has not DNS/MX entries");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check if mail come from a spammer address ( spam-address.tab )
///////////////////////////////////////////////////////////////////////////////
	if (USmtpSpamAddressCheck(ppszRetDomains[0]) < 0) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, ppszRetDomains[0], "", "SNDR=ESPAM", 0);

		if ((pszSMTPError =
		     SvrGetConfigVar(SMTPS.hSvrConfig, "SmtpMsgIPBanSpamAddress")) == NULL)
			pszSMTPError = SysStrDup("504 You are registered as spammer");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check SMTP after POP3 authentication
///////////////////////////////////////////////////////////////////////////////
	if (SvrTestConfigFlag("EnableAuthSMTP-POP3", true, SMTPS.hSvrConfig))
		SMTPTryPopAuthIpCheck(SMTPS, szMailerUser, szMailerDomain);

///////////////////////////////////////////////////////////////////////////////
//  Check extended mail from parameters
///////////////////////////////////////////////////////////////////////////////
	if (SMTPCheckMailParams(pszCommand, ppszRetDomains, SMTPS, pszSMTPError) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Setup From string
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.pszFrom != NULL)
		SysFree(SMTPS.pszFrom);

	SMTPS.pszFrom = SysStrDup(ppszRetDomains[0]);

	return (0);

}

static int SMTPAddMessageInfo(SMTPSession & SMTPS)
{

	return (USmtpAddMessageInfo(SMTPS.pMsgFile, SMTPS.szClientDomain, SMTPS.PeerInfo,
				    SMTPS.szSvrDomain, SMTPS.SockInfo, SMTP_SERVER_NAME));

}

static int SMTPCheckMailParams(const char *pszCommand, char **ppszRetDomains,
			       SMTPSession & SMTPS, char *&pszSMTPError)
{

	char const *pszParams = strrchr(pszCommand, '>');

	if (pszParams == NULL)
		pszParams = pszCommand;

///////////////////////////////////////////////////////////////////////////////
//  Check the SIZE parameter
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.ulMaxMsgSize != 0) {
		char const *pszSize = pszParams;

		while ((pszSize = StrIStr(pszSize, " SIZE=")) != NULL) {
			pszSize += CStringSize(" SIZE=");

			if (isdigit(*pszSize) &&
			    (SMTPS.ulMaxMsgSize < (unsigned long) atol(pszSize))) {
				if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
					SMTPLogSession(SMTPS,
						       (ppszRetDomains[0] !=
							NULL) ? ppszRetDomains[0] : "", "",
						       "SIZE=EBIG",
						       (unsigned long) atol(pszSize));

				pszSMTPError =
				    SysStrDup("552 Message exceeds fixed maximum message size");

				ErrSetErrorCode(ERR_MESSAGE_SIZE);
				return (ERR_MESSAGE_SIZE);
			}
		}

	}

	return (0);

}

static int SMTPHandleCmd_MAIL(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	if ((SMTPS.iSMTPState != stateHelo) && (SMTPS.iSMTPState != stateAuthenticated)) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "503 Bad sequence of commands");

		ErrSetErrorCode(ERR_SMTP_BAD_CMD_SEQUENCE);
		return (ERR_SMTP_BAD_CMD_SEQUENCE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Split the RETURN PATH
///////////////////////////////////////////////////////////////////////////////
	char **ppszRetDomains = USmtpGetPathStrings(pszCommand);

	if (ppszRetDomains == NULL) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "501 Syntax error in parameters or arguments: (%d)",
			      ErrorFetch());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check RETURN PATH
///////////////////////////////////////////////////////////////////////////////
	char *pszSMTPError = NULL;

	if (SMTPCheckReturnPath(pszCommand, ppszRetDomains, SMTPS, pszSMTPError) < 0) {
		ErrorPush();
		StrFreeStrings(ppszRetDomains);
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "%s", pszSMTPError);
		SysFree(pszSMTPError);
		return (ErrorPop());
	}

	StrFreeStrings(ppszRetDomains);

///////////////////////////////////////////////////////////////////////////////
//  If the incoming IP is "mapped" stop here
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.ulFlags & (SMTPF_MAPPED_IP | SMTPF_NORDNS_IP | SMTPF_BLOCKED_IP)) {
		char *pszSMTPError = NULL;

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg)) {
			if (SMTPS.ulFlags & SMTPF_MAPPED_IP) {
				char *pszError =
					StrSprint("SNDRIP=EIPMAP (%s)", SMTPS.szRejMapName);

				SMTPLogSession(SMTPS, SMTPS.pszFrom, "",
					       (pszError != NULL) ? pszError : "SNDRIP=EIPMAP",
					       0);

				if (pszError != NULL)
					SysFree(pszError);
			} else if (SMTPS.ulFlags & SMTPF_NORDNS_IP)
				SMTPLogSession(SMTPS, SMTPS.pszFrom, "", "SNDRIP=ERDNS", 0);
			else
				SMTPLogSession(SMTPS, SMTPS.pszFrom, "", "SNDRIP=EIPSPAM", 0);
		}
		if (SMTPS.ulFlags & SMTPF_BLOCKED_IP)
			pszSMTPError = SvrGetConfigVar(SMTPS.hSvrConfig, "SmtpMsgIPBanSpammers");

		SMTPResetSession(SMTPS);

		if (pszSMTPError == NULL)
			SMTPSendError(hBSock, SMTPS, "551 Server access forbidden by your IP");
		else {
			SMTPSendError(hBSock, SMTPS, "%s", pszSMTPError);
			SysFree(pszSMTPError);
		}

		ErrSetErrorCode(ERR_SMTP_USE_FORBIDDEN);
		return (ERR_SMTP_USE_FORBIDDEN);
	}
///////////////////////////////////////////////////////////////////////////////
//  If MAIL command is locked stop here
///////////////////////////////////////////////////////////////////////////////
	if ((SMTPS.ulFlags & SMTPF_MAIL_LOCKED) && !(SMTPS.ulFlags & SMTPF_MAIL_UNLOCKED)) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "551 Server use forbidden");

		ErrSetErrorCode(ERR_SMTP_USE_FORBIDDEN);
		return (ERR_SMTP_USE_FORBIDDEN);
	}
///////////////////////////////////////////////////////////////////////////////
//  Prepare mail file
///////////////////////////////////////////////////////////////////////////////
	if ((SMTPS.pMsgFile = fopen(SMTPS.szMsgFile, "w+b")) == NULL) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ERR_FILE_CREATE);

		ErrSetErrorCode(ERR_FILE_CREATE, SMTPS.szMsgFile);
		return (ERR_FILE_CREATE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Write message infos ( 1st row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	if (SMTPAddMessageInfo(SMTPS) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Write domain ( 2nd row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	if (StrWriteCRLFString(SMTPS.pMsgFile, SMTPS.szSvrDomain) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Get SMTP message ID and write it to file ( 3th row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	if (SvrGetMessageID(&SMTPS.ullMessageID) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());
		return (ErrorPop());
	}

	sprintf(SMTPS.szMessageID, "S" SYS_LLX_FMT, SMTPS.ullMessageID);

	if (StrWriteCRLFString(SMTPS.pMsgFile, SMTPS.szMessageID) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Write MAIL FROM ( 4th row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	if (StrWriteCRLFString(SMTPS.pMsgFile, pszCommand) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());
		return (ErrorPop());
	}

	BSckSendString(hBSock, "250 OK", SMTPS.pSMTPCfg->iTimeout);

	SMTPS.iSMTPState = stateMail;

	return (0);

}

static int SMTPCheckRelayCapability(SMTPSession & SMTPS, char const *pszDestDomain)
{
///////////////////////////////////////////////////////////////////////////////
//  OK if enabled ( authentication )
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.ulFlags & SMTPF_RELAY_ENABLED)
		return (0);

///////////////////////////////////////////////////////////////////////////////
//  OK if destination domain is a custom-handled domain
///////////////////////////////////////////////////////////////////////////////
	if (USmlCustomizedDomain(pszDestDomain) == 0)
		return (0);

///////////////////////////////////////////////////////////////////////////////
//  IP based relay check
///////////////////////////////////////////////////////////////////////////////
	return (USmtpIsAllowedRelay(SMTPS.PeerInfo, SMTPS.hSvrConfig));

}

static int SMTPCheckForwardPath(char **ppszFwdDomains, SMTPSession & SMTPS,
				char *&pszRealRcpt, char *&pszSMTPError)
{

	int iDomainCount = StrStringsCount(ppszFwdDomains);

	if (iDomainCount == 0) {
		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom, "", "RCPT=ESYNTAX", 0);

		pszSMTPError = SysStrDup("501 Syntax error in forward path");

		ErrSetErrorCode(ERR_BAD_FORWARD_PATH);
		return (ERR_BAD_FORWARD_PATH);
	}

	char szDestUser[MAX_ADDR_NAME] = "";
	char szDestDomain[MAX_ADDR_NAME] = "";
	char szRealUser[MAX_ADDR_NAME] = "";

	if (USmtpSplitEmailAddr(ppszFwdDomains[iDomainCount - 1], szDestUser, szDestDomain) < 0) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom, ppszFwdDomains[0], "RCPT=ESYNTAX",
				       0);

		pszSMTPError = SysStrDup("501 Syntax error in forward path");

		return (ErrorPop());
	}

	if (iDomainCount == 1) {
		if (USmlIsCmdAliasAccount(szDestDomain, szDestUser) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  The recipient is handled with cmdaliases
///////////////////////////////////////////////////////////////////////////////

		} else if (MDomIsHandledDomain(szDestDomain) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Check user existance
///////////////////////////////////////////////////////////////////////////////
			UserInfo *pUI = UsrGetUserByNameOrAlias(szDestDomain, szDestUser);

			if (pUI != NULL) {
///////////////////////////////////////////////////////////////////////////////
//  Check if the account is enabled for receiving
///////////////////////////////////////////////////////////////////////////////
				if (!UsrGetUserInfoVarInt(pUI, "ReceiveEnable", 1)) {
					UsrFreeUserInfo(pUI);

					if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
						SMTPLogSession(SMTPS, SMTPS.pszFrom,
							       ppszFwdDomains[0], "RCPT=EDSBL",
							       0);

					pszSMTPError = StrSprint("550 Account disabled <%s@%s>",
								 szDestUser, szDestDomain);

					ErrSetErrorCode(ERR_USER_DISABLED);
					return (ERR_USER_DISABLED);
				}

				if (UsrGetUserType(pUI) == usrTypeUser) {
///////////////////////////////////////////////////////////////////////////////
//  Target is a normal user
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Check user mailbox size
///////////////////////////////////////////////////////////////////////////////
					if (UPopCheckMailboxSize(pUI) < 0) {
						ErrorPush();
						UsrFreeUserInfo(pUI);

						if (SMTPLogEnabled
						    (SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
							SMTPLogSession(SMTPS, SMTPS.pszFrom,
								       ppszFwdDomains[0],
								       "RCPT=EFULL", 0);

						pszSMTPError =
							StrSprint
							("552 Requested mail action aborted: exceeded storage allocation - <%s@%s>",
							 szDestUser, szDestDomain);

						return (ErrorPop());
					}
				} else {
///////////////////////////////////////////////////////////////////////////////
//  Target is a mailing list
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Check if client can post to this mailing list
///////////////////////////////////////////////////////////////////////////////
					if (UsrMLCheckUserPost(pUI, SMTPS.pszFrom,
							       IsEmptyString(SMTPS.
									     szLogonUser) ? NULL :
							       SMTPS.szLogonUser) < 0) {
						ErrorPush();
						UsrFreeUserInfo(pUI);

						if (SMTPLogEnabled
						    (SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
							SMTPLogSession(SMTPS, SMTPS.pszFrom,
								       ppszFwdDomains[0],
								       "RCPT=EACCESS", 0);

						pszSMTPError =
							StrSprint
							("557 Access denied <%s@%s> for user <%s>",
							 szDestUser, szDestDomain, SMTPS.pszFrom);

						return (ErrorPop());
					}
				}
///////////////////////////////////////////////////////////////////////////////
//  Extract the real user address
///////////////////////////////////////////////////////////////////////////////
				UsrGetAddress(pUI, szRealUser);

				UsrFreeUserInfo(pUI);
			} else {
///////////////////////////////////////////////////////////////////////////////
//  Recipient domain is local but no account is found inside the standard
//  users/aliases database and the account is not handled with cmdaliases.
//  It's pretty much time to report a recipient error
///////////////////////////////////////////////////////////////////////////////
				if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
					SMTPLogSession(SMTPS, SMTPS.pszFrom, ppszFwdDomains[0],
						       "RCPT=EAVAIL", 0);

				pszSMTPError = StrSprint("550 Mailbox unavailable <%s@%s>",
							 szDestUser, szDestDomain);

				ErrSetErrorCode(ERR_USER_NOT_LOCAL);
				return (ERR_USER_NOT_LOCAL);
			}
		} else {
///////////////////////////////////////////////////////////////////////////////
//  Check relay permission
///////////////////////////////////////////////////////////////////////////////
			if (SMTPCheckRelayCapability(SMTPS, szDestDomain) < 0) {
				ErrorPush();

				if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
					SMTPLogSession(SMTPS, SMTPS.pszFrom, ppszFwdDomains[0],
						       "RCPT=ERELAY", 0);

				pszSMTPError = SysStrDup("550 Relay denied");

				return (ErrorPop());
			}
		}
	} else {
///////////////////////////////////////////////////////////////////////////////
//  Check relay permission
///////////////////////////////////////////////////////////////////////////////
		if (SMTPCheckRelayCapability(SMTPS, szDestDomain) < 0) {
			ErrorPush();

			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, SMTPS.pszFrom, ppszFwdDomains[0],
					       "RCPT=ERELAY", 0);

			pszSMTPError = SysStrDup("550 Relay denied");

			return (ErrorPop());
		}
	}

///////////////////////////////////////////////////////////////////////////////
//  Retrieve destination domain
///////////////////////////////////////////////////////////////////////////////
	if (USmtpSplitEmailAddr(ppszFwdDomains[0], NULL, SMTPS.szDestDomain) < 0) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom, ppszFwdDomains[0], "RCPT=ESYNTAX",
				       0);

		pszSMTPError = SysStrDup("501 Syntax error in forward path");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Setup SendRcpt string ( it'll be used to build "RCPT TO:<>" line into
//  the message file
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.pszSendRcpt != NULL)
		SysFree(SMTPS.pszSendRcpt);

	if ((SMTPS.pszSendRcpt = USmtpBuildRcptPath(ppszFwdDomains, SMTPS.hSvrConfig)) == NULL) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom, ppszFwdDomains[0], "RCPT=ESYNTAX",
				       0);

		pszSMTPError = SysStrDup("501 Syntax error in forward path");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Setup the Real Rcpt string
///////////////////////////////////////////////////////////////////////////////
	if (!IsEmptyString(szRealUser))
		pszRealRcpt = SysStrDup(szRealUser);
///////////////////////////////////////////////////////////////////////////////
//  Setup Rcpt string
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.pszRcpt != NULL)
		SysFree(SMTPS.pszRcpt);

	SMTPS.pszRcpt = SysStrDup(ppszFwdDomains[0]);

	return (0);

}

static int SMTPHandleCmd_RCPT(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	if ((SMTPS.iSMTPState != stateMail) && (SMTPS.iSMTPState != stateRcpt)) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "503 Bad sequence of commands");

		ErrSetErrorCode(ERR_SMTP_BAD_CMD_SEQUENCE);
		return (ERR_SMTP_BAD_CMD_SEQUENCE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Check recipients count
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.iRcptCount >= SMTPS.pSMTPCfg->iMaxRcpts) {
		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom, "", "RCPT=ENBR", 0);

		SMTPSendError(hBSock, SMTPS, "552 Too many recipients");

		ErrSetErrorCode(ERR_SMTP_TOO_MANY_RECIPIENTS);
		return (ERR_SMTP_TOO_MANY_RECIPIENTS);
	}

	char **ppszFwdDomains = USmtpGetPathStrings(pszCommand);

	if (ppszFwdDomains == NULL) {
		ErrorPush();

		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, SMTPS.pszFrom, "", "RCPT=ESYNTAX", 0);

		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "501 Syntax error in parameters or arguments: (%d)",
			      ErrorFetch());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check FORWARD PATH
///////////////////////////////////////////////////////////////////////////////
	char *pszRealRcpt = NULL;
	char *pszSMTPError = NULL;

	if (SMTPCheckForwardPath(ppszFwdDomains, SMTPS, pszRealRcpt,
				 pszSMTPError) < 0) {
		ErrorPush();
		StrFreeStrings(ppszFwdDomains);

		SMTPSendError(hBSock, SMTPS, "%s", pszSMTPError);
		SysFree(pszSMTPError);
		return (ErrorPop());
	}

	StrFreeStrings(ppszFwdDomains);

///////////////////////////////////////////////////////////////////////////////
//  Log SMTP session
///////////////////////////////////////////////////////////////////////////////
	if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
		SMTPLogSession(SMTPS, SMTPS.pszFrom, SMTPS.pszRcpt, "RCPT=OK", 0);

///////////////////////////////////////////////////////////////////////////////
//  Write RCPT TO ( 5th[,...] row(s) of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	fprintf(SMTPS.pMsgFile, "RCPT TO:<%s> {ra=%s}\r\n", SMTPS.pszSendRcpt,
		(pszRealRcpt != NULL) ? pszRealRcpt: SMTPS.pszSendRcpt);

	if (pszRealRcpt != NULL)
		SysFree(pszRealRcpt);

	BSckSendString(hBSock, "250 OK", SMTPS.pSMTPCfg->iTimeout);

	++SMTPS.iRcptCount;

	SMTPS.iSMTPState = stateRcpt;

	return (0);

}

static int SMTPGetFilterFile(const char *pszFiltID, char *pszFileName, int iMaxName)
{

	char szMailRoot[SYS_MAX_PATH] = "";

	CfgGetRootPath(szMailRoot, sizeof(szMailRoot));
	SysSNPrintf(pszFileName, iMaxName - 1, "%sfilters.%s.tab", szMailRoot, pszFiltID);

	return (SysExistFile(pszFileName));

}

static int SMTPFilterMacroSubstitutes(char **ppszCmdTokens, SMTPSession & SMTPS)
{

	for (int ii = 0; ppszCmdTokens[ii] != NULL; ii++) {
		if (strcmp(ppszCmdTokens[ii], "@@FILE") == 0) {
			char *pszNewValue = SysStrDup(SMTPS.szMsgFile);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@USERAUTH") == 0) {
			char *pszNewValue = SysStrDup(IsEmptyString(SMTPS.szLogonUser) ?
						      "-": SMTPS.szLogonUser);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@REMOTEADDR") == 0) {
			char szIP[128] = "???.???.???.???";
			char *pszNewValue = SysStrDup(SysInetNToA(SMTPS.PeerInfo, szIP));

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@LOCALADDR") == 0) {
			char szIP[128] = "???.???.???.???";
			char *pszNewValue = SysStrDup(SysInetNToA(SMTPS.SockInfo, szIP));

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		}
	}

	return (0);
}

static char *SMTPGetFilterRejMessage(char const *pszMsgFilePath)
{

	FILE *pFile;
	char szRejFilePath[SYS_MAX_PATH] = "";
	char szRejMsg[512] = "";

	SysSNPrintf(szRejFilePath, sizeof(szRejFilePath) - 1, "%s.rej", pszMsgFilePath);
	if ((pFile = fopen(szRejFilePath, "rb")) == NULL)
		return NULL;

	MscFGets(szRejMsg, sizeof(szRejMsg) - 1, pFile);

	fclose(pFile);
	SysRemove(szRejFilePath);

	return (SysStrDup(szRejMsg));

}

static int SMTPLogFilter(SMTPSession & SMTPS, char const * const *ppszExec, int iExecResult,
			 int iExitCode, char const *pszType, char const *pszInfo)
{

	FilterLogInfo FLI;

	FLI.pszSender = SMTPS.pszFrom != NULL ? SMTPS.pszFrom: "";
	FLI.pszRecipient = SMTPS.pszRcpt != NULL ? (SMTPS.iRcptCount == 1 ? SMTPS.pszRcpt: "*"): "";
	FLI.LocalAddr = SMTPS.SockInfo;
	FLI.RemoteAddr = SMTPS.PeerInfo;
	FLI.ppszExec = ppszExec;
	FLI.iExecResult = iExecResult;
	FLI.iExitCode = iExitCode;
	FLI.pszType = pszType;
	FLI.pszInfo = pszInfo != NULL ? pszInfo: "";

	return (FilLogFilter(&FLI));

}

static int SMTPPreFilterExec(SMTPSession & SMTPS, FilterTokens *pToks, char **ppszPEError)
{

	FilterExecCtx FCtx;

	FCtx.pToks = pToks;
	FCtx.pszAuthName = SMTPS.szLogonUser;

	return (FilExecPreParse(&FCtx, ppszPEError));

}

static int SMTPRunFilters(SMTPSession & SMTPS, char const *pszFilterPath, char const *pszType,
			  char *&pszError)
{
///////////////////////////////////////////////////////////////////////////////
//  Share lock the filter file
///////////////////////////////////////////////////////////////////////////////
	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockSH(CfgGetBasedPath(pszFilterPath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  This should not happen but if it happens we let the message pass through
///////////////////////////////////////////////////////////////////////////////
	FILE *pFiltFile = fopen(pszFilterPath, "rt");

	if (pFiltFile == NULL) {
		RLckUnlockSH(hResLock);
		return (0);
	}

///////////////////////////////////////////////////////////////////////////////
//  Filter this message
///////////////////////////////////////////////////////////////////////////////
	int iFieldsCount, iExitCode, iExitFlags, iExecResult;
	char szFiltLine[1024] = "";

	while (MscGetConfigLine(szFiltLine, sizeof(szFiltLine) - 1, pFiltFile) != NULL) {
		char **ppszCmdTokens = StrGetTabLineStrings(szFiltLine);

		if (ppszCmdTokens == NULL)
			continue;

		if ((iFieldsCount = StrStringsCount(ppszCmdTokens)) < 1) {
			StrFreeStrings(ppszCmdTokens);
			continue;
		}

///////////////////////////////////////////////////////////////////////////////
//  Perform pre-exec filtering (like exec exclude if authenticated, ...)
///////////////////////////////////////////////////////////////////////////////
		char *pszPEError = NULL;
		FilterTokens Toks;

		Toks.ppszCmdTokens = ppszCmdTokens;
		Toks.iTokenCount = iFieldsCount;

		if (SMTPPreFilterExec(SMTPS, &Toks, &pszPEError) < 0) {
			if (bFilterLogEnabled)
				SMTPLogFilter(SMTPS, Toks.ppszCmdTokens, -1,
					      -1, pszType, pszPEError);
			if (pszPEError != NULL)
				SysFree(pszPEError);
			StrFreeStrings(ppszCmdTokens);
			continue;
		}

///////////////////////////////////////////////////////////////////////////////
//  Do filter line macro substitution
///////////////////////////////////////////////////////////////////////////////
		SMTPFilterMacroSubstitutes(Toks.ppszCmdTokens, SMTPS);

		iExitCode = -1;
		iExitFlags = 0;
		iExecResult = SysExec(Toks.ppszCmdTokens[0], &Toks.ppszCmdTokens[0],
				      iFilterTimeout, SYS_PRIORITY_NORMAL, &iExitCode);

///////////////////////////////////////////////////////////////////////////////
//  Log filter execution, if enabled
///////////////////////////////////////////////////////////////////////////////
		if (bFilterLogEnabled)
			SMTPLogFilter(SMTPS, Toks.ppszCmdTokens, iExecResult,
				      iExitCode, pszType, NULL);

		if (iExecResult == 0) {
			SysLogMessage(LOG_LEV_MESSAGE,
				      "SMTP filter run: Filter = \"%s\" Retcode = %d\n",
				      Toks.ppszCmdTokens[0], iExitCode);

			iExitFlags = iExitCode & SMTP_FILTER_FL_MASK;
			iExitCode &= ~SMTP_FILTER_FL_MASK;

			if (iExitCode == SMTP_FILTER_REJECT_CODE) {
				StrFreeStrings(ppszCmdTokens);
				fclose(pFiltFile);
				RLckUnlockSH(hResLock);

				if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
					SMTPLogSession(SMTPS, SMTPS.pszFrom, SMTPS.pszRcpt,
						       "DATA=EFILTER", 0);

				pszError = SMTPGetFilterRejMessage(SMTPS.szMsgFile);

				ErrSetErrorCode(ERR_FILTERED_MESSAGE);
				return (ERR_FILTERED_MESSAGE);
			}
		} else {
			SysLogMessage(LOG_LEV_ERROR,
				      "SMTP filter error (%d): Filter = \"%s\"\n",
				      iExecResult, Toks.ppszCmdTokens[0]);
		}

		StrFreeStrings(ppszCmdTokens);

		if (iExitFlags & SMTP_FILTER_FL_BREAK)
			break;
	}

	fclose(pFiltFile);
	RLckUnlockSH(hResLock);

	return (0);
}

static int SMTPFilterMessage(SMTPSession & SMTPS, const char *pszFiltID, char *&pszError)
{

	int iReOpen = 0;
	char szFilterFile[SYS_MAX_PATH] = "";

	if (!SMTPGetFilterFile(pszFiltID, szFilterFile, sizeof(szFilterFile) - 1))
		return (0);

	if (SMTPS.pMsgFile != NULL) {
		if (fflush(SMTPS.pMsgFile)) {
			ErrSetErrorCode(ERR_FILE_WRITE, SMTPS.szMsgFile);
			return (ERR_FILE_WRITE);
		}
		if (fclose(SMTPS.pMsgFile)) {
			SMTPS.pMsgFile = NULL;
			ErrSetErrorCode(ERR_FILE_WRITE, SMTPS.szMsgFile);
			return (ERR_FILE_WRITE);
		}
		SMTPS.pMsgFile = NULL;
		iReOpen++;
	}

	if (SMTPRunFilters(SMTPS, szFilterFile, pszFiltID, pszError) < 0)
		return (ErrGetErrorCode());

	if (iReOpen) {
		if ((SMTPS.pMsgFile = fopen(SMTPS.szMsgFile, "r+b")) == NULL) {
			ErrSetErrorCode(ERR_FILE_OPEN, SMTPS.szMsgFile);
			return (ERR_FILE_OPEN);
		}
		fseek(SMTPS.pMsgFile, 0, SEEK_END);
	}

	return (0);

}

static int SMTPHandleCmd_DATA(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{
	char *pszError;

	if (SMTPS.iSMTPState != stateRcpt) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "503 Bad sequence of commands");

		ErrSetErrorCode(ERR_SMTP_BAD_CMD_SEQUENCE);
		return (ERR_SMTP_BAD_CMD_SEQUENCE);
	}

///////////////////////////////////////////////////////////////////////////////
//  Run the pre-DATA filter
///////////////////////////////////////////////////////////////////////////////
	pszError = NULL;
	if (SMTPFilterMessage(SMTPS, SMTP_PRE_DATA_FILTER, pszError) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		if (pszError != NULL) {
			SMTPSendError(hBSock, SMTPS, "%s", pszError);
			SysFree(pszError);
		} else
			BSckSendString(hBSock, "554 Transaction failed",
				       SMTPS.pSMTPCfg->iTimeout);
		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Write data begin marker
///////////////////////////////////////////////////////////////////////////////
	if (StrWriteCRLFString(SMTPS.pMsgFile, SPOOL_FILE_DATA_START) < 0) {
		ErrorPush();
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());
		return (ErrorPop());
	}

	BSckSendString(hBSock, "354 Start mail input; end with <CRLF>.<CRLF>",
		       SMTPS.pSMTPCfg->iTimeout);

///////////////////////////////////////////////////////////////////////////////
//  Write data
///////////////////////////////////////////////////////////////////////////////
	int iErrorCode = 0;
	int iLineLength;
	int iGotNL;
	int iGotNLPrev = 1;
	unsigned long ulMessageSize = 0;
	unsigned long ulMaxMsgSize = SMTPS.ulMaxMsgSize;
	char const *pszSmtpError = NULL;
	char szBuffer[SMTP_MAX_LINE_SIZE + 4];

	for (;;) {
		if (BSckGetString
		    (hBSock, szBuffer, sizeof(szBuffer) - 3, SMTPS.pSMTPCfg->iTimeout,
		     &iLineLength, &iGotNL) == NULL) {
			ErrorPush();
			SMTPResetSession(SMTPS);
			return (ErrorPop());
		}
///////////////////////////////////////////////////////////////////////////////
//  Check end of data condition
///////////////////////////////////////////////////////////////////////////////
		if (iGotNL && iGotNLPrev && (strcmp(szBuffer, ".") == 0))
			break;

///////////////////////////////////////////////////////////////////////////////
//  Correctly terminate the line
///////////////////////////////////////////////////////////////////////////////
		if (iGotNL)
			memcpy(szBuffer + iLineLength, "\r\n", 3), iLineLength += 2;

		if (iErrorCode == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Write data on disk
///////////////////////////////////////////////////////////////////////////////
			if (!fwrite(szBuffer, iLineLength, 1, SMTPS.pMsgFile)) {
				ErrSetErrorCode(iErrorCode = ERR_FILE_WRITE, SMTPS.szMsgFile);

			}

		}

		ulMessageSize += (unsigned long) iLineLength;

///////////////////////////////////////////////////////////////////////////////
//  Check the message size
///////////////////////////////////////////////////////////////////////////////
		if ((ulMaxMsgSize != 0) && (ulMaxMsgSize < ulMessageSize)) {
			pszSmtpError = "552 Message exceeds fixed maximum message size";

			ErrSetErrorCode(iErrorCode = ERR_MESSAGE_SIZE);
		}

		if (SvrInShutdown()) {
			SMTPResetSession(SMTPS);

			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);
			return (ERR_SERVER_SHUTDOWN);
		}

		iGotNLPrev = iGotNL;
	}

///////////////////////////////////////////////////////////////////////////////
//  Check fclose() return value coz data might be buffered and fail to flush
///////////////////////////////////////////////////////////////////////////////
	if (fclose(SMTPS.pMsgFile))
		ErrSetErrorCode(iErrorCode = ERR_FILE_WRITE, SMTPS.szMsgFile);

	SMTPS.pMsgFile = NULL;

	if (iErrorCode == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Run the post-DATA filter
///////////////////////////////////////////////////////////////////////////////
		pszError = NULL;
		if (SMTPFilterMessage(SMTPS, SMTP_POST_DATA_FILTER, pszError) < 0) {
			ErrorPush();
			SMTPResetSession(SMTPS);

			if (pszError != NULL) {
				SMTPSendError(hBSock, SMTPS, "%s", pszError);
				SysFree(pszError);
			} else
				BSckSendString(hBSock, "554 Transaction failed",
					       SMTPS.pSMTPCfg->iTimeout);
			return (ErrorPop());
		}

///////////////////////////////////////////////////////////////////////////////
//  Transfer spool file
///////////////////////////////////////////////////////////////////////////////
		if (SMTPSubmitPackedFile(SMTPS, SMTPS.szMsgFile) < 0)
			SMTPSendError(hBSock, SMTPS,
				      "451 Requested action aborted: (%d) local error in processing",
				      ErrGetErrorCode());
		else {
///////////////////////////////////////////////////////////////////////////////
//  Log the message receive
///////////////////////////////////////////////////////////////////////////////
			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, SMTPS.pszFrom, SMTPS.pszRcpt, "RECV=OK",
					       ulMessageSize);

///////////////////////////////////////////////////////////////////////////////
//  Send the ack only when everything is OK
///////////////////////////////////////////////////////////////////////////////
			BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout, "250 OK <%s>",
					SMTPS.szMessageID);

		}
	} else {
///////////////////////////////////////////////////////////////////////////////
//  Notify the client the error condition
///////////////////////////////////////////////////////////////////////////////
		if (pszSmtpError == NULL)
			SMTPSendError(hBSock, SMTPS,
				      "451 Requested action aborted: (%d) local error in processing",
				      ErrGetErrorCode());
		else
			SMTPSendError(hBSock, SMTPS, "%s", pszSmtpError);

	}

	SMTPResetSession(SMTPS);

	return (iErrorCode);

}

static int SMTPAddReceived(int iType, char const *pszAuth, char const *const *ppszMsgInfo,
			   char const *pszMailFrom, char const *pszRcptTo,
			   char const *pszMessageID, FILE * pMailFile)
{

	char *pszReceived = USmtpGetReceived(iType, pszAuth, ppszMsgInfo, pszMailFrom, pszRcptTo,
					     pszMessageID);

	if (pszReceived == NULL)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Write "Received:" tag
///////////////////////////////////////////////////////////////////////////////
	fputs(pszReceived, pMailFile);

	SysFree(pszReceived);

	return (0);

}

static char *SMTPTrimRcptLine(char *pszRcptLn)
{

	char *pszTrim = strchr(pszRcptLn, '>');

	if (pszTrim != NULL)
		pszTrim[1] = '\0';

	return (pszRcptLn);

}

static int SMTPSubmitPackedFile(SMTPSession & SMTPS, const char *pszPkgFile)
{

	FILE *pPkgFile = fopen(pszPkgFile, "rb");

	if (pPkgFile == NULL) {
		ErrSetErrorCode(ERR_FILE_OPEN);
		return (ERR_FILE_OPEN);
	}

	char szSpoolLine[MAX_SPOOL_LINE] = "";

	while ((MscGetString(pPkgFile, szSpoolLine, sizeof(szSpoolLine) - 1) != NULL) &&
	       (strncmp(szSpoolLine, SPOOL_FILE_DATA_START, CStringSize(SPOOL_FILE_DATA_START)) !=
		0));

	if (strncmp(szSpoolLine, SPOOL_FILE_DATA_START, CStringSize(SPOOL_FILE_DATA_START)) != 0) {
		fclose(pPkgFile);
		ErrSetErrorCode(ERR_INVALID_SPOOL_FILE);
		return (ERR_INVALID_SPOOL_FILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Get the offset at which the message data begin and rewind the file
///////////////////////////////////////////////////////////////////////////////
	unsigned long ulMsgOffset = (unsigned long) ftell(pPkgFile);

	rewind(pPkgFile);

///////////////////////////////////////////////////////////////////////////////
//  Read SMTP message info ( 1st row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	char **ppszMsgInfo = NULL;

	if ((MscGetString(pPkgFile, szSpoolLine, sizeof(szSpoolLine) - 1) == NULL) ||
	    ((ppszMsgInfo = StrTokenize(szSpoolLine, ";")) == NULL) ||
	    (StrStringsCount(ppszMsgInfo) < smsgiMax)) {
		if (ppszMsgInfo != NULL)
			StrFreeStrings(ppszMsgInfo);
		fclose(pPkgFile);
		ErrSetErrorCode(ERR_INVALID_SPOOL_FILE);
		return (ERR_INVALID_SPOOL_FILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Read SMTP domain ( 2nd row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	char szSMTPDomain[256] = "";

	if (MscGetString(pPkgFile, szSMTPDomain, sizeof(szSMTPDomain) - 1) == NULL) {
		StrFreeStrings(ppszMsgInfo);
		fclose(pPkgFile);
		ErrSetErrorCode(ERR_INVALID_SPOOL_FILE);
		return (ERR_INVALID_SPOOL_FILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Read message ID ( 3th row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	char szMessageID[128] = "";

	if (MscGetString(pPkgFile, szMessageID, sizeof(szMessageID) - 1) == NULL) {
		StrFreeStrings(ppszMsgInfo);
		fclose(pPkgFile);
		ErrSetErrorCode(ERR_INVALID_SPOOL_FILE);
		return (ERR_INVALID_SPOOL_FILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Read "MAIL FROM:" ( 4th row of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	char szMailFrom[MAX_SPOOL_LINE] = "";

	if ((MscGetString(pPkgFile, szMailFrom, sizeof(szMailFrom) - 1) == NULL) ||
	    (StrINComp(szMailFrom, MAIL_FROM_STR) != 0)) {
		StrFreeStrings(ppszMsgInfo);
		fclose(pPkgFile);
		ErrSetErrorCode(ERR_INVALID_SPOOL_FILE);
		return (ERR_INVALID_SPOOL_FILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Get the Received: header type to emit
///////////////////////////////////////////////////////////////////////////////
	int iReceivedType = SvrGetConfigInt("ReceivedHdrType", RECEIVED_TYPE_STD,
					    SMTPS.hSvrConfig);

///////////////////////////////////////////////////////////////////////////////
//  Read "RCPT TO:" ( 5th[,...] row(s) of the smtp-mail file )
///////////////////////////////////////////////////////////////////////////////
	while ((MscGetString(pPkgFile, szSpoolLine, sizeof(szSpoolLine) - 1) != NULL) &&
	       (StrINComp(szSpoolLine, RCPT_TO_STR) == 0)) {
///////////////////////////////////////////////////////////////////////////////
//  Cleanup the RCPT line from extra info
///////////////////////////////////////////////////////////////////////////////
		SMTPTrimRcptLine(szSpoolLine);

///////////////////////////////////////////////////////////////////////////////
//  Get message handle
///////////////////////////////////////////////////////////////////////////////
		QMSG_HANDLE hMessage = QueCreateMessage(hSpoolQueue);

		if (hMessage == INVALID_QMSG_HANDLE) {
			ErrorPush();
			StrFreeStrings(ppszMsgInfo);
			fclose(pPkgFile);
			return (ErrorPop());
		}

		char szQueueFilePath[SYS_MAX_PATH] = "";

		QueGetFilePath(hSpoolQueue, hMessage, szQueueFilePath);

		FILE *pSpoolFile = fopen(szQueueFilePath, "wb");

		if (pSpoolFile == NULL) {
			QueCleanupMessage(hSpoolQueue, hMessage);
			QueCloseMessage(hSpoolQueue, hMessage);
			StrFreeStrings(ppszMsgInfo);
			fclose(pPkgFile);
			ErrSetErrorCode(ERR_FILE_CREATE);
			return (ERR_FILE_CREATE);
		}
///////////////////////////////////////////////////////////////////////////////
//  Write info line
///////////////////////////////////////////////////////////////////////////////
		USmtpWriteInfoLine(pSpoolFile, ppszMsgInfo[smsgiClientAddr],
				   ppszMsgInfo[smsgiServerAddr], ppszMsgInfo[smsgiTime]);

///////////////////////////////////////////////////////////////////////////////
//  Write SMTP domain
///////////////////////////////////////////////////////////////////////////////
		fprintf(pSpoolFile, "%s\r\n", szSMTPDomain);

///////////////////////////////////////////////////////////////////////////////
//  Write message ID
///////////////////////////////////////////////////////////////////////////////
		fprintf(pSpoolFile, "%s\r\n", szMessageID);

///////////////////////////////////////////////////////////////////////////////
//  Write "MAIL FROM:"
///////////////////////////////////////////////////////////////////////////////
		fprintf(pSpoolFile, "%s\r\n", szMailFrom);

///////////////////////////////////////////////////////////////////////////////
//  Write "RCPT TO:"
///////////////////////////////////////////////////////////////////////////////
		fprintf(pSpoolFile, "%s\r\n", szSpoolLine);

///////////////////////////////////////////////////////////////////////////////
//  Write SPOOL_FILE_DATA_START
///////////////////////////////////////////////////////////////////////////////
		fprintf(pSpoolFile, "%s\r\n", SPOOL_FILE_DATA_START);

///////////////////////////////////////////////////////////////////////////////
//  Write "X-AuthUser:" tag
///////////////////////////////////////////////////////////////////////////////
		if (!IsEmptyString(SMTPS.szLogonUser) && !(SMTPS.ulFlags & SMTPF_NOEMIT_AUTH))
			fprintf(pSpoolFile, "X-AuthUser: %s\r\n", SMTPS.szLogonUser);

///////////////////////////////////////////////////////////////////////////////
//  Write "Received:" tag
///////////////////////////////////////////////////////////////////////////////
		SMTPAddReceived(iReceivedType,
				IsEmptyString(SMTPS.szLogonUser) ? NULL : SMTPS.szLogonUser,
				ppszMsgInfo, szMailFrom, szSpoolLine, szMessageID, pSpoolFile);

///////////////////////////////////////////////////////////////////////////////
//  Write mail data, saving and restoring the current file pointer
///////////////////////////////////////////////////////////////////////////////
		unsigned long ulCurrOffset = (unsigned long) ftell(pPkgFile);

		if (MscCopyFile(pSpoolFile, pPkgFile, ulMsgOffset, (unsigned long) -1) < 0) {
			ErrorPush();
			fclose(pSpoolFile);
			QueCleanupMessage(hSpoolQueue, hMessage);
			QueCloseMessage(hSpoolQueue, hMessage);
			StrFreeStrings(ppszMsgInfo);
			fclose(pPkgFile);
			return (ErrorPop());
		}

		if (SysFileSync(pSpoolFile) < 0) {
			ErrorPush();
			fclose(pSpoolFile);
			QueCleanupMessage(hSpoolQueue, hMessage);
			QueCloseMessage(hSpoolQueue, hMessage);
			StrFreeStrings(ppszMsgInfo);
			fclose(pPkgFile);
			return (ErrorPop());
		}

		if (fclose(pSpoolFile)) {
			QueCleanupMessage(hSpoolQueue, hMessage);
			QueCloseMessage(hSpoolQueue, hMessage);
			StrFreeStrings(ppszMsgInfo);
			fclose(pPkgFile);
			ErrSetErrorCode(ERR_FILE_WRITE, szQueueFilePath);
			return (ERR_FILE_WRITE);
		}

		fseek(pPkgFile, ulCurrOffset, SEEK_SET);

///////////////////////////////////////////////////////////////////////////////
//  Transfer file to the spool
///////////////////////////////////////////////////////////////////////////////
		if (QueCommitMessage(hSpoolQueue, hMessage) < 0) {
			ErrorPush();
			QueCleanupMessage(hSpoolQueue, hMessage);
			QueCloseMessage(hSpoolQueue, hMessage);
			StrFreeStrings(ppszMsgInfo);
			fclose(pPkgFile);
			return (ErrorPop());
		}
	}

	StrFreeStrings(ppszMsgInfo);
	fclose(pPkgFile);

	return (0);

}

static int SMTPHandleCmd_HELO(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	if ((SMTPS.iSMTPState != stateInit) && (SMTPS.iSMTPState != stateHelo)) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "503 Bad sequence of commands");

		ErrSetErrorCode(ERR_SMTP_BAD_CMD_SEQUENCE);
		return (ERR_SMTP_BAD_CMD_SEQUENCE);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");
		return (-1);
	}

	StrSNCpy(SMTPS.szClientDomain, ppszTokens[1]);

	StrFreeStrings(ppszTokens);

	char *pszDomain = SvrGetConfigVar(SMTPS.hSvrConfig, "RootDomain");

	if (pszDomain == NULL) {
		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ERR_NO_ROOT_DOMAIN_VAR);

		ErrSetErrorCode(ERR_NO_ROOT_DOMAIN_VAR);
		return (ERR_NO_ROOT_DOMAIN_VAR);
	}

	BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout, "250 %s", pszDomain);

	SysFree(pszDomain);

	SMTPS.iSMTPState = stateHelo;

	return (0);

}

static int SMTPHandleCmd_EHLO(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	if ((SMTPS.iSMTPState != stateInit) && (SMTPS.iSMTPState != stateHelo)) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "503 Bad sequence of commands");

		ErrSetErrorCode(ERR_SMTP_BAD_CMD_SEQUENCE);
		return (ERR_SMTP_BAD_CMD_SEQUENCE);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");
		return (-1);
	}

	StrSNCpy(SMTPS.szClientDomain, ppszTokens[1]);

	StrFreeStrings(ppszTokens);

///////////////////////////////////////////////////////////////////////////////
//  Create response file
///////////////////////////////////////////////////////////////////////////////
	char szRespFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szRespFile);

	FILE *pRespFile = fopen(szRespFile, "w+b");

	if (pRespFile == NULL) {
		CheckRemoveFile(szRespFile);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ERR_FILE_CREATE);

		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Get root domain ( "RootDomain" )
///////////////////////////////////////////////////////////////////////////////
	char *pszDomain = SvrGetConfigVar(SMTPS.hSvrConfig, "RootDomain");

	if (pszDomain == NULL) {
		fclose(pRespFile);
		SysRemove(szRespFile);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ERR_NO_ROOT_DOMAIN_VAR);

		ErrSetErrorCode(ERR_NO_ROOT_DOMAIN_VAR);
		return (ERR_NO_ROOT_DOMAIN_VAR);
	}
///////////////////////////////////////////////////////////////////////////////
//  Build EHLO response file ( domain + auths )
///////////////////////////////////////////////////////////////////////////////
	fprintf(pRespFile, "250 %s\r\n", pszDomain);

	SysFree(pszDomain);

///////////////////////////////////////////////////////////////////////////////
//  Emit extended SMTP command and internal auths
///////////////////////////////////////////////////////////////////////////////
	fprintf(pRespFile,
		"250 VRFY\r\n"
		"250 ETRN\r\n"
		"250 8BITMIME\r\n" "250 PIPELINING\r\n" "250 AUTH LOGIN PLAIN CRAM-MD5");

///////////////////////////////////////////////////////////////////////////////
//  Emit external authentication methods
///////////////////////////////////////////////////////////////////////////////
	SMTPListExtAuths(pRespFile, SMTPS);

	fprintf(pRespFile, "\r\n");

///////////////////////////////////////////////////////////////////////////////
//  Emit maximum message size ( if set )
///////////////////////////////////////////////////////////////////////////////
	if (SMTPS.ulMaxMsgSize != 0)
		fprintf(pRespFile, "250 SIZE %lu\r\n", SMTPS.ulMaxMsgSize);
	else
		fprintf(pRespFile, "250 SIZE\r\n");

///////////////////////////////////////////////////////////////////////////////
//  Send EHLO response file
///////////////////////////////////////////////////////////////////////////////
	if (SMTPSendMultilineResponse(hBSock, SMTPS.pSMTPCfg->iTimeout, pRespFile) < 0) {
		ErrorPush();
		fclose(pRespFile);
		SysRemove(szRespFile);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());

		return (ErrorPop());
	}

	fclose(pRespFile);
	SysRemove(szRespFile);

	SMTPS.iSMTPState = stateHelo;

	return (0);

}

static int SMTPListExtAuths(FILE * pRespFile, SMTPSession & SMTPS)
{

	char szExtAuthFilePath[SYS_MAX_PATH] = "";

	SMTPGetExtAuthFilePath(szExtAuthFilePath, sizeof(szExtAuthFilePath));

	FILE *pExtAuthFile = fopen(szExtAuthFilePath, "rt");

	if (pExtAuthFile != NULL) {
		char szExtAuthLine[SVR_SMTP_EXTAUTH_LINE_MAX] = "";

		while (MscGetConfigLine(szExtAuthLine, sizeof(szExtAuthLine) - 1, pExtAuthFile) !=
		       NULL) {
			char **ppszStrings = StrGetTabLineStrings(szExtAuthLine);

			if (ppszStrings == NULL)
				continue;

			if (StrStringsCount(ppszStrings) > 0)
				fprintf(pRespFile, " %s", ppszStrings[0]);

			StrFreeStrings(ppszStrings);
		}

		fclose(pExtAuthFile);
	}

	return (0);

}

static int SMTPExternalAuthSubstitute(char **ppszAuthTokens, char const *pszChallenge,
				      char const *pszDigest, char const *pszSecretsFile)
{

	for (int ii = 0; ppszAuthTokens[ii] != NULL; ii++) {
		if (strcmp(ppszAuthTokens[ii], "@@FSECRT") == 0) {
			char *pszNewValue = SysStrDup(pszSecretsFile);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszAuthTokens[ii]);

			ppszAuthTokens[ii] = pszNewValue;
		} else if (strcmp(ppszAuthTokens[ii], "@@CHALL") == 0) {
			char *pszNewValue = SysStrDup(pszChallenge);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszAuthTokens[ii]);

			ppszAuthTokens[ii] = pszNewValue;
		} else if (strcmp(ppszAuthTokens[ii], "@@DGEST") == 0) {
			char *pszNewValue = SysStrDup(pszDigest);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszAuthTokens[ii]);

			ppszAuthTokens[ii] = pszNewValue;
		}
	}

	return (0);

}

static int SMTPCreateSecretsFile(char const *pszSecretsFile)
{

	char szAuthFilePath[SYS_MAX_PATH] = "";

	SMTPGetAuthFilePath(szAuthFilePath, sizeof(szAuthFilePath));

	FILE *pAuthFile = fopen(szAuthFilePath, "rt");

	if (pAuthFile == NULL) {
		ErrSetErrorCode(ERR_FILE_OPEN, szAuthFilePath);
		return (ERR_FILE_OPEN);
	}

	FILE *pSecretFile = fopen(pszSecretsFile, "wt");

	if (pSecretFile == NULL) {
		fclose(pAuthFile);

		ErrSetErrorCode(ERR_FILE_CREATE, pszSecretsFile);
		return (ERR_FILE_CREATE);
	}

	char szAuthLine[SVR_SMTPAUTH_LINE_MAX] = "";

	while (MscGetConfigLine(szAuthLine, sizeof(szAuthLine) - 1, pAuthFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szAuthLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if (iFieldsCount >= smtpaMax)
			fprintf(pSecretFile, "%s:%s\n", ppszStrings[smtpaUsername],
				ppszStrings[smtpaPassword]);

		StrFreeStrings(ppszStrings);
	}

	fclose(pSecretFile);
	fclose(pAuthFile);

	return (0);

}

static int SMTPExternalAuthenticate(BSOCK_HANDLE hBSock, SMTPSession & SMTPS,
				    char **ppszAuthTokens)
{
///////////////////////////////////////////////////////////////////////////////
//  Emit encoded ( base64 ) challenge ( param1 + ':' + timestamp )
//  and get client response
///////////////////////////////////////////////////////////////////////////////
	unsigned int uEnc64Length = 0;
	char szChallenge[1024] = "";
	char szDigest[1024] = "";

	SysSNPrintf(szDigest, sizeof(szDigest) - 1, "%s:%s", ppszAuthTokens[1],
		    SMTPS.szTimeStamp);
	encode64(szDigest, strlen(szDigest), szChallenge, sizeof(szChallenge) - 1, &uEnc64Length);

	if ((BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout, "334 %s", szChallenge) < 0) ||
	    (BSckGetString(hBSock, szChallenge, sizeof(szChallenge) - 1, SMTPS.pSMTPCfg->iTimeout)
	     == NULL))
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Decode ( base64 ) client response
///////////////////////////////////////////////////////////////////////////////
	unsigned int uDec64Length = 0;

	if (decode64(szChallenge, strlen(szChallenge), szDigest, &uDec64Length) != 0) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}

	SysSNPrintf(szChallenge, sizeof(szChallenge) - 1, "%s:%s", ppszAuthTokens[1],
		    SMTPS.szTimeStamp);

///////////////////////////////////////////////////////////////////////////////
//  Create secrets file ( username + ':' + password )
///////////////////////////////////////////////////////////////////////////////
	char szSecretsFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szSecretsFile);

	if (SMTPCreateSecretsFile(szSecretsFile) < 0) {
		ErrorPush();
		CheckRemoveFile(szSecretsFile);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Do macro substitution
///////////////////////////////////////////////////////////////////////////////
	SMTPExternalAuthSubstitute(ppszAuthTokens, szChallenge, szDigest, szSecretsFile);

///////////////////////////////////////////////////////////////////////////////
//  Call external program to compute the response
///////////////////////////////////////////////////////////////////////////////
	int iExitCode = -1;

	if (SysExec(ppszAuthTokens[2], &ppszAuthTokens[2], SVR_SMTP_EXTAUTH_TIMEOUT,
		    SVR_SMTP_EXTAUTH_PRIORITY, &iExitCode) < 0) {
		ErrorPush();
		SysRemove(szSecretsFile);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());

		return (ErrorPop());
	}

	if (iExitCode != SVR_SMTP_EXTAUTH_SUCCESS) {
		SysRemove(szSecretsFile);

		SMTPSendError(hBSock, SMTPS, "503 Authentication failed");

		ErrSetErrorCode(ERR_BAD_EXTRNPRG_EXITCODE);
		return (ERR_BAD_EXTRNPRG_EXITCODE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Load response file containing the matching secret
///////////////////////////////////////////////////////////////////////////////
	unsigned int uRespSize = 0;
	char *pMatchSecret = (char *) MscLoadFile(szSecretsFile, uRespSize);

	CheckRemoveFile(szSecretsFile);

	if (pMatchSecret == NULL) {
		ErrorPush();

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());

		return (ErrorPop());
	}

	while ((uRespSize > 0) &&
	       ((pMatchSecret[uRespSize - 1] == '\r') || (pMatchSecret[uRespSize - 1] == '\n')))
		--uRespSize;

	pMatchSecret[uRespSize] = '\0';

///////////////////////////////////////////////////////////////////////////////
//  Try to extract username and password tokens ( username + ':' + password )
///////////////////////////////////////////////////////////////////////////////
	char **ppszTokens = StrTokenize(pMatchSecret, ":");

	SysFree(pMatchSecret);

	if (StrStringsCount(ppszTokens) != 2) {
		StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ERR_BAD_SMTP_EXTAUTH_RESPONSE_FILE);

		ErrSetErrorCode(ERR_BAD_SMTP_EXTAUTH_RESPONSE_FILE);
		return (ERR_BAD_SMTP_EXTAUTH_RESPONSE_FILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Lookup smtp auth file
///////////////////////////////////////////////////////////////////////////////
	if (SMTPTryApplyUsrPwdAuth(SMTPS, ppszTokens[0], ppszTokens[1]) < 0) {
		ErrorPush();
		StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "503 Authentication failed");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Set the logon user
///////////////////////////////////////////////////////////////////////////////
	StrSNCpy(SMTPS.szLogonUser, ppszTokens[0]);

	StrFreeStrings(ppszTokens);

	SMTPS.ulFlags |= SMTPF_AUTHENTICATED;
	SMTPS.iSMTPState = stateAuthenticated;

	BSckSendString(hBSock, "235 Authentication successful", SMTPS.pSMTPCfg->iTimeout);

	return (0);

}

static int SMTPDoAuthExternal(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthType)
{

	char szExtAuthFilePath[SYS_MAX_PATH] = "";

	SMTPGetExtAuthFilePath(szExtAuthFilePath, sizeof(szExtAuthFilePath));

	FILE *pExtAuthFile = fopen(szExtAuthFilePath, "rt");

	if (pExtAuthFile == NULL) {
		ErrSetErrorCode(ERR_FILE_OPEN, szExtAuthFilePath);
		return (ERR_FILE_OPEN);
	}

	char szExtAuthLine[SVR_SMTP_EXTAUTH_LINE_MAX] = "";

	while (MscGetConfigLine(szExtAuthLine, sizeof(szExtAuthLine) - 1, pExtAuthFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szExtAuthLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount > 5) && (stricmp(ppszStrings[0], pszAuthType) == 0)) {
			fclose(pExtAuthFile);

			int iAuthResult = SMTPExternalAuthenticate(hBSock, SMTPS, ppszStrings);

			StrFreeStrings(ppszStrings);

			return (iAuthResult);
		}

		StrFreeStrings(ppszStrings);
	}

	fclose(pExtAuthFile);

	SMTPSendError(hBSock, SMTPS, "504 Unknown authentication");

	ErrSetErrorCode(ERR_UNKNOWN_SMTP_AUTH, pszAuthType);
	return (ERR_UNKNOWN_SMTP_AUTH);

}

static int SMTPDoAuthPlain(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthParam)
{
///////////////////////////////////////////////////////////////////////////////
//  Parameter validation
///////////////////////////////////////////////////////////////////////////////
	if ((pszAuthParam == NULL) || (strlen(pszAuthParam) == 0)) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}
///////////////////////////////////////////////////////////////////////////////
//  Decode ( base64 ) auth parameter
///////////////////////////////////////////////////////////////////////////////
	unsigned int uDec64Length = 0;
	char szClientAuth[PLAIN_AUTH_PARAM_SIZE] = "";

	ZeroData(szClientAuth);

	if (decode64(pszAuthParam, strlen(pszAuthParam), szClientAuth, &uDec64Length) != 0) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}
///////////////////////////////////////////////////////////////////////////////
//  Extract plain auth params ( unused + 0 + username + 0 + password )
///////////////////////////////////////////////////////////////////////////////
	char *pszUsername = szClientAuth + strlen(szClientAuth) + 1;
	char *pszPassword = pszUsername + strlen(pszUsername) + 1;

///////////////////////////////////////////////////////////////////////////////
//  Validate client response
///////////////////////////////////////////////////////////////////////////////
	if ((SMTPTryApplyLocalAuth(SMTPS, pszUsername, pszPassword) < 0) &&
	    (SMTPTryApplyUsrPwdAuth(SMTPS, pszUsername, pszPassword) < 0)) {
		ErrorPush();

		SMTPSendError(hBSock, SMTPS, "503 Authentication failed");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Set the logon user
///////////////////////////////////////////////////////////////////////////////
	StrSNCpy(SMTPS.szLogonUser, pszUsername);

	SMTPS.ulFlags |= SMTPF_AUTHENTICATED;
	SMTPS.iSMTPState = stateAuthenticated;

	BSckSendString(hBSock, "235 Authentication successful", SMTPS.pSMTPCfg->iTimeout);

	return (0);

}

static int SMTPDoAuthLogin(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthParam)
{
///////////////////////////////////////////////////////////////////////////////
//  Emit encoded64 username request
///////////////////////////////////////////////////////////////////////////////
	unsigned int uEnc64Length = 0;
	char szUsername[512] = "";

	encode64(LOGIN_AUTH_USERNAME, strlen(LOGIN_AUTH_USERNAME), szUsername,
		 sizeof(szUsername), &uEnc64Length);

	if ((BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout, "334 %s", szUsername) < 0) ||
	    (BSckGetString(hBSock, szUsername, sizeof(szUsername) - 1, SMTPS.pSMTPCfg->iTimeout)
	     == NULL))
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Emit encoded64 password request
///////////////////////////////////////////////////////////////////////////////
	char szPassword[512] = "";

	encode64(LOGIN_AUTH_PASSWORD, strlen(LOGIN_AUTH_PASSWORD), szPassword,
		 sizeof(szPassword), &uEnc64Length);

	if ((BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout, "334 %s", szPassword) < 0) ||
	    (BSckGetString(hBSock, szPassword, sizeof(szPassword) - 1, SMTPS.pSMTPCfg->iTimeout)
	     == NULL))
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Decode ( base64 ) username
///////////////////////////////////////////////////////////////////////////////
	unsigned int uDec64Length = 0;
	char szDecodeBuffer[512] = "";

	if (decode64(szUsername, strlen(szUsername), szDecodeBuffer, &uDec64Length) != 0) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}

	StrSNCpy(szUsername, szDecodeBuffer);

///////////////////////////////////////////////////////////////////////////////
//  Decode ( base64 ) password
///////////////////////////////////////////////////////////////////////////////
	if (decode64(szPassword, strlen(szPassword), szDecodeBuffer, &uDec64Length) != 0) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}

	StrSNCpy(szPassword, szDecodeBuffer);

///////////////////////////////////////////////////////////////////////////////
//  Validate client response
///////////////////////////////////////////////////////////////////////////////
	if ((SMTPTryApplyLocalAuth(SMTPS, szUsername, szPassword) < 0) &&
	    (SMTPTryApplyUsrPwdAuth(SMTPS, szUsername, szPassword) < 0)) {
		ErrorPush();

		SMTPSendError(hBSock, SMTPS, "503 Authentication failed");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Set the logon user
///////////////////////////////////////////////////////////////////////////////
	StrSNCpy(SMTPS.szLogonUser, szUsername);

	SMTPS.ulFlags |= SMTPF_AUTHENTICATED;
	SMTPS.iSMTPState = stateAuthenticated;

	BSckSendString(hBSock, "235 Authentication successful", SMTPS.pSMTPCfg->iTimeout);

	return (0);

}

static char *SMTPGetAuthFilePath(char *pszFilePath, int iMaxPath)
{

	CfgGetRootPath(pszFilePath, iMaxPath);

	StrNCat(pszFilePath, SVR_SMTP_AUTH_FILE, iMaxPath);

	return (pszFilePath);

}

static char *SMTPGetExtAuthFilePath(char *pszFilePath, int iMaxPath)
{

	CfgGetRootPath(pszFilePath, iMaxPath);

	StrNCat(pszFilePath, SVR_SMTP_EXTAUTH_FILE, iMaxPath);

	return (pszFilePath);

}

static int SMTPTryApplyLocalAuth(SMTPSession & SMTPS, char const *pszUsername,
				 char const *pszPassword)
{
///////////////////////////////////////////////////////////////////////////////
//  First try to lookup  mailusers.tab
///////////////////////////////////////////////////////////////////////////////
	char szAccountUser[MAX_ADDR_NAME] = "";
	char szAccountDomain[MAX_HOST_NAME] = "";

	if (StrSplitString(pszUsername, POP3_USER_SPLITTERS, szAccountUser, sizeof(szAccountUser),
			   szAccountDomain, sizeof(szAccountDomain)) < 0)
		return (ErrGetErrorCode());

	UserInfo *pUI = UsrGetUserByName(szAccountDomain, szAccountUser);

	if (pUI != NULL) {
		if (strcmp(pUI->pszPassword, pszPassword) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Apply user configuration
///////////////////////////////////////////////////////////////////////////////
			if (SMTPApplyUserConfig(SMTPS, pUI) < 0) {
				ErrorPush();
				UsrFreeUserInfo(pUI);
				return (ErrorPop());
			}

			UsrFreeUserInfo(pUI);

			return (0);
		}

		UsrFreeUserInfo(pUI);

	}

	ErrSetErrorCode(ERR_SMTP_AUTH_FAILED);
	return (ERR_SMTP_AUTH_FAILED);

}

static int SMTPGetUserSmtpPerms(UserInfo * pUI, SVRCFG_HANDLE hSvrConfig, char *pszPerms,
				int iMaxPerms)
{

	char *pszUserPerms = UsrGetUserInfoVar(pUI, "SmtpPerms");

	if (pszUserPerms != NULL) {
		StrNCpy(pszPerms, pszUserPerms, iMaxPerms);

		SysFree(pszUserPerms);
	} else {
///////////////////////////////////////////////////////////////////////////////
//  Match found, get the default permissions
///////////////////////////////////////////////////////////////////////////////
		char *pszDefultPerms = SvrGetConfigVar(hSvrConfig,
						       "DefaultSmtpPerms", "MR");

		if (pszDefultPerms != NULL) {
			StrNCpy(pszPerms, pszDefultPerms, iMaxPerms);

			SysFree(pszDefultPerms);
		} else
			SetEmptyString(pszPerms);

	}

	return (0);

}

static int SMTPTryApplyLocalCMD5Auth(SMTPSession & SMTPS, char const *pszChallenge,
				     char const *pszUsername, char const *pszDigest)
{
///////////////////////////////////////////////////////////////////////////////
//  First try to lookup  mailusers.tab
///////////////////////////////////////////////////////////////////////////////
	char szAccountUser[MAX_ADDR_NAME] = "";
	char szAccountDomain[MAX_HOST_NAME] = "";

	if (StrSplitString(pszUsername, POP3_USER_SPLITTERS, szAccountUser, sizeof(szAccountUser),
			   szAccountDomain, sizeof(szAccountDomain)) < 0)
		return (ErrGetErrorCode());

	UserInfo *pUI = UsrGetUserByName(szAccountDomain, szAccountUser);

	if (pUI != NULL) {
///////////////////////////////////////////////////////////////////////////////
//  Compute MD5 response ( secret , challenge , digest )
///////////////////////////////////////////////////////////////////////////////
		char szCurrDigest[512] = "";

		if (MscCramMD5(pUI->pszPassword, pszChallenge, szCurrDigest) < 0) {
			UsrFreeUserInfo(pUI);

			return (ErrGetErrorCode());
		}

		if (stricmp(szCurrDigest, pszDigest) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Apply user configuration
///////////////////////////////////////////////////////////////////////////////
			if (SMTPApplyUserConfig(SMTPS, pUI) < 0) {
				ErrorPush();
				UsrFreeUserInfo(pUI);
				return (ErrorPop());
			}

			UsrFreeUserInfo(pUI);

			return (0);
		}

		UsrFreeUserInfo(pUI);

	}

	ErrSetErrorCode(ERR_SMTP_AUTH_FAILED);
	return (ERR_SMTP_AUTH_FAILED);

}

static int SMTPTryApplyUsrPwdAuth(SMTPSession & SMTPS, char const *pszUsername,
				  char const *pszPassword)
{

	char szAuthFilePath[SYS_MAX_PATH] = "";

	SMTPGetAuthFilePath(szAuthFilePath, sizeof(szAuthFilePath));

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockSH(CfgGetBasedPath(szAuthFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	FILE *pAuthFile = fopen(szAuthFilePath, "rt");

	if (pAuthFile == NULL) {
		RLckUnlockSH(hResLock);
		ErrSetErrorCode(ERR_FILE_OPEN, szAuthFilePath);
		return (ERR_FILE_OPEN);
	}

	char szAuthLine[SVR_SMTPAUTH_LINE_MAX] = "";

	while (MscGetConfigLine(szAuthLine, sizeof(szAuthLine) - 1, pAuthFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szAuthLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= smtpaMax) &&
		    (strcmp(ppszStrings[smtpaUsername], pszUsername) == 0) &&
		    (strcmp(ppszStrings[smtpaPassword], pszPassword) == 0)) {
///////////////////////////////////////////////////////////////////////////////
//  Apply user perms to SMTP config
///////////////////////////////////////////////////////////////////////////////
			SMTPApplyPerms(SMTPS, ppszStrings[smtpaPerms]);

			StrFreeStrings(ppszStrings);
			fclose(pAuthFile);
			RLckUnlockSH(hResLock);

			return (0);
		}

		StrFreeStrings(ppszStrings);
	}

	fclose(pAuthFile);

	RLckUnlockSH(hResLock);

	ErrSetErrorCode(ERR_SMTP_AUTH_FAILED);
	return (ERR_SMTP_AUTH_FAILED);

}

static int SMTPTryApplyCMD5Auth(SMTPSession & SMTPS, char const *pszChallenge,
				char const *pszUsername, char const *pszDigest)
{

	char szAuthFilePath[SYS_MAX_PATH] = "";

	SMTPGetAuthFilePath(szAuthFilePath, sizeof(szAuthFilePath));

	FILE *pAuthFile = fopen(szAuthFilePath, "rt");

	if (pAuthFile == NULL) {
		ErrSetErrorCode(ERR_FILE_OPEN, szAuthFilePath);
		return (ERR_FILE_OPEN);
	}

	char szAuthLine[SVR_SMTPAUTH_LINE_MAX] = "";

	while (MscGetConfigLine(szAuthLine, sizeof(szAuthLine) - 1, pAuthFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szAuthLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= smtpaMax) &&
		    (strcmp(ppszStrings[smtpaUsername], pszUsername) == 0)) {
			char szCurrDigest[512] = "";

///////////////////////////////////////////////////////////////////////////////
//  Compute MD5 response ( secret , challenge , digest )
///////////////////////////////////////////////////////////////////////////////
			if (MscCramMD5(ppszStrings[smtpaPassword], pszChallenge, szCurrDigest) <
			    0) {
				StrFreeStrings(ppszStrings);
				fclose(pAuthFile);

				return (ErrGetErrorCode());
			}

			if (stricmp(szCurrDigest, pszDigest) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Apply user perms to SMTP config
///////////////////////////////////////////////////////////////////////////////
				SMTPApplyPerms(SMTPS, ppszStrings[smtpaPerms]);

				StrFreeStrings(ppszStrings);
				fclose(pAuthFile);

				return (0);
			}
		}

		StrFreeStrings(ppszStrings);
	}

	fclose(pAuthFile);

	ErrSetErrorCode(ERR_SMTP_AUTH_FAILED);
	return (ERR_SMTP_AUTH_FAILED);

}

static int SMTPDoAuthCramMD5(BSOCK_HANDLE hBSock, SMTPSession & SMTPS, char const *pszAuthParam)
{
///////////////////////////////////////////////////////////////////////////////
//  Emit encoded64 challenge and get client response
///////////////////////////////////////////////////////////////////////////////
	unsigned int uEnc64Length = 0;
	char szChallenge[512] = "";

	encode64(SMTPS.szTimeStamp, strlen(SMTPS.szTimeStamp), szChallenge,
		 sizeof(szChallenge), &uEnc64Length);

	if ((BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout, "334 %s", szChallenge) < 0) ||
	    (BSckGetString(hBSock, szChallenge, sizeof(szChallenge) - 1, SMTPS.pSMTPCfg->iTimeout)
	     == NULL))
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Decode ( base64 ) client response
///////////////////////////////////////////////////////////////////////////////
	unsigned int uDec64Length = 0;
	char szClientResp[512] = "";

	if (decode64(szChallenge, strlen(szChallenge), szClientResp, &uDec64Length) != 0) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}
///////////////////////////////////////////////////////////////////////////////
//  Extract the username and client digest
///////////////////////////////////////////////////////////////////////////////
	char *pszUsername = szClientResp;
	char *pszDigest = strchr(szClientResp, ' ');

	if (pszDigest == NULL) {
		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");

		ErrSetErrorCode(ERR_BAD_SMTP_CMD_SYNTAX);
		return (ERR_BAD_SMTP_CMD_SYNTAX);
	}

	*pszDigest++ = '\0';

///////////////////////////////////////////////////////////////////////////////
//  Validate client response
///////////////////////////////////////////////////////////////////////////////
	if ((SMTPTryApplyLocalCMD5Auth(SMTPS, SMTPS.szTimeStamp, pszUsername,
				       pszDigest) < 0) &&
	    (SMTPTryApplyCMD5Auth(SMTPS, SMTPS.szTimeStamp, pszUsername, pszDigest) < 0)) {
		ErrorPush();

		SMTPSendError(hBSock, SMTPS, "503 Authentication failed");

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Set the logon user
///////////////////////////////////////////////////////////////////////////////
	StrSNCpy(SMTPS.szLogonUser, pszUsername);

	SMTPS.ulFlags |= SMTPF_AUTHENTICATED;
	SMTPS.iSMTPState = stateAuthenticated;

	BSckSendString(hBSock, "235 Authentication successful", SMTPS.pSMTPCfg->iTimeout);

	return (0);

}

static int SMTPHandleCmd_AUTH(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	if (SMTPS.iSMTPState != stateHelo) {
		SMTPResetSession(SMTPS);

		SMTPSendError(hBSock, SMTPS, "503 Bad sequence of commands");

		ErrSetErrorCode(ERR_SMTP_BAD_CMD_SEQUENCE);
		return (ERR_SMTP_BAD_CMD_SEQUENCE);
	}

	int iTokensCount;
	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || ((iTokensCount = StrStringsCount(ppszTokens)) < 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");
		return (-1);
	}
///////////////////////////////////////////////////////////////////////////////
//  Decode AUTH command params
///////////////////////////////////////////////////////////////////////////////
	char szAuthType[128] = "";
	char szAuthParam[PLAIN_AUTH_PARAM_SIZE] = "";

	StrSNCpy(szAuthType, ppszTokens[1]);

	if (iTokensCount > 2)
		StrSNCpy(szAuthParam, ppszTokens[2]);

	StrFreeStrings(ppszTokens);

///////////////////////////////////////////////////////////////////////////////
//  Handle authentication methods
///////////////////////////////////////////////////////////////////////////////
	if (stricmp(szAuthType, "plain") == 0) {

		if (SMTPDoAuthPlain(hBSock, SMTPS, szAuthParam) < 0) {
			ErrorPush();

			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, "", "", "AUTH=EFAIL:TYPE=PLAIN", 0);

			return (ErrorPop());
		}

	} else if (stricmp(szAuthType, "login") == 0) {

		if (SMTPDoAuthLogin(hBSock, SMTPS, szAuthParam) < 0) {
			ErrorPush();

			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, "", "", "AUTH=EFAIL:TYPE=LOGIN", 0);

			return (ErrorPop());
		}

	} else if (stricmp(szAuthType, "cram-md5") == 0) {

		if (SMTPDoAuthCramMD5(hBSock, SMTPS, szAuthParam) < 0) {
			ErrorPush();

			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, "", "", "AUTH=EFAIL:TYPE=CRAM-MD5", 0);

			return (ErrorPop());
		}

	} else {
///////////////////////////////////////////////////////////////////////////////
//  Handle external authentication methods
///////////////////////////////////////////////////////////////////////////////
		if (SMTPDoAuthExternal(hBSock, SMTPS, szAuthType) < 0) {
			ErrorPush();

			if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
				SMTPLogSession(SMTPS, "", "", "AUTH=EFAIL:TYPE=EXTRN", 0);

			return (ErrorPop());
		}

	}

	return (0);

}

static int SMTPSendMultilineResponse(BSOCK_HANDLE hBSock, int iTimeout, FILE * pRespFile)
{

	rewind(pRespFile);

	char szCurrLine[1024] = "";
	char szPrevLine[1024] = "";

	if (MscGetString(pRespFile, szPrevLine, sizeof(szPrevLine) - 1) != NULL) {
		while (MscGetString(pRespFile, szCurrLine, sizeof(szCurrLine) - 1) != NULL) {
			szPrevLine[3] = '-';

			if (BSckSendString(hBSock, szPrevLine, iTimeout) < 0)
				return (ErrGetErrorCode());

			StrSNCpy(szPrevLine, szCurrLine);
		}

		if (BSckSendString(hBSock, szPrevLine, iTimeout) < 0)
			return (ErrGetErrorCode());
	}

	return (0);

}

static int SMTPHandleCmd_RSET(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	SMTPResetSession(SMTPS);

	BSckSendString(hBSock, "250 OK", SMTPS.pSMTPCfg->iTimeout);

	return (0);

}

static int SMTPHandleCmd_NOOP(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	BSckSendString(hBSock, "250 OK", SMTPS.pSMTPCfg->iTimeout);

	return (0);

}

static int SMTPHandleCmd_HELP(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
			"250-HELO EHLO MAIL RCPT DATA AUTH\r\n"
			"250-RSET VRFY ETRN NOOP HELP QUIT\r\n"
			"250 For more information please visit : %s", APP_URL);

	return (0);

}

static int SMTPHandleCmd_QUIT(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

	SMTPS.iSMTPState = stateExit;

	BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
			"221 %s service closing transmission channel", SMTP_SERVER_NAME);

	return (0);

}

static int SMTPHandleCmd_VRFY(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

///////////////////////////////////////////////////////////////////////////////
//  Check if VRFY is enabled
///////////////////////////////////////////////////////////////////////////////
	if (((SMTPS.ulFlags & SMTPF_VRFY_ENABLED) == 0) &&
	    !SvrTestConfigFlag("AllowSmtpVRFY", false, SMTPS.hSvrConfig)) {
		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, "", "", "VRFY=EACCESS", 0);

		SMTPSendError(hBSock, SMTPS, "252 Argument not checked");
		return (-1);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");
		return (-1);
	}

	char szVrfyUser[MAX_ADDR_NAME] = "";
	char szVrfyDomain[MAX_ADDR_NAME] = "";

	if (USmtpSplitEmailAddr(ppszTokens[1], szVrfyUser, szVrfyDomain) < 0) {
		ErrorPush();
		StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");
		return (ErrorPop());
	}

	StrFreeStrings(ppszTokens);

	UserInfo *pUI = UsrGetUserByNameOrAlias(szVrfyDomain, szVrfyUser);

	if (pUI != NULL) {
		char *pszRealName = UsrGetUserInfoVar(pUI, "RealName", "Unknown");

		BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
				"250 %s <%s@%s>", pszRealName, pUI->pszName, pUI->pszDomain);

		SysFree(pszRealName);

		UsrFreeUserInfo(pUI);
	} else {
		if (USmlIsCmdAliasAccount(szVrfyDomain, szVrfyUser) < 0) {
			SMTPSendError(hBSock, SMTPS, "550 String does not match anything");

			ErrSetErrorCode(ERR_USER_NOT_LOCAL);
			return (ERR_USER_NOT_LOCAL);
		}

		BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
				"250 Local account <%s@%s>", szVrfyUser, szVrfyDomain);

	}

	return (0);

}

static int SMTPHandleCmd_ETRN(const char *pszCommand, BSOCK_HANDLE hBSock, SMTPSession & SMTPS)
{

///////////////////////////////////////////////////////////////////////////////
//  Check if ETRN is enabled
///////////////////////////////////////////////////////////////////////////////
	if (((SMTPS.ulFlags & SMTPF_ETRN_ENABLED) == 0) &&
	    !SvrTestConfigFlag("AllowSmtpETRN", false, SMTPS.hSvrConfig)) {
		if (SMTPLogEnabled(SMTPS.hShbSMTP, SMTPS.pSMTPCfg))
			SMTPLogSession(SMTPS, "", "", "ETRN=EACCESS", 0);

		SMTPSendError(hBSock, SMTPS, "501 Command not accepted");
		return (-1);
	}

	char **ppszTokens = StrTokenize(pszCommand, " ");

	if ((ppszTokens == NULL) || (StrStringsCount(ppszTokens) != 2)) {
		if (ppszTokens != NULL)
			StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS, "501 Syntax error in parameters or arguments");
		return (-1);
	}
///////////////////////////////////////////////////////////////////////////////
//  Do a matched flush of the rsnd arena
///////////////////////////////////////////////////////////////////////////////
	if (QueFlushRsndArena(hSpoolQueue, ppszTokens[1]) < 0) {
		ErrorPush();
		StrFreeStrings(ppszTokens);

		SMTPSendError(hBSock, SMTPS,
			      "451 Requested action aborted: (%d) local error in processing",
			      ErrorFetch());

		return (ErrorPop());
	}

	BSckVSendString(hBSock, SMTPS.pSMTPCfg->iTimeout,
			"250 Queueing for '%s' has been started", ppszTokens[1]);

	StrFreeStrings(ppszTokens);

	return (0);

}
