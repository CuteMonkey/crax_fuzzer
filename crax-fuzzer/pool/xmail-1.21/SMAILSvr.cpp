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
#include "StrUtils.h"
#include "BuffSock.h"
#include "MiscUtils.h"
#include "ResLocks.h"
#include "BuffSock.h"
#include "SvrUtils.h"
#include "UsrUtils.h"
#include "MessQueue.h"
#include "SMAILUtils.h"
#include "QueueUtils.h"
#include "MailDomains.h"
#include "SMTPUtils.h"
#include "ExtAliases.h"
#include "UsrMailList.h"
#include "Filter.h"
#include "MailConfig.h"
#include "SMAILSvr.h"
#include "AppDefines.h"
#include "MailSvr.h"

#define SMAIL_WAITMSG_TIMEOUT       2
#define CUSTOM_PROC_LINE_MAX        1024
#define SMAIL_EXTERNAL_EXIT_BREAK   16
#define SMAIL_STOP_PROCESSING       3111965L

static SMAILConfig *SMAILGetConfigCopy(SHB_HANDLE hShbSMAIL);
static int SMAILThreadCountAdd(long lCount, SHB_HANDLE hShbSMAIL, SMAILConfig * pSMAILCfg = NULL);
static int SMAILLogEnabled(SHB_HANDLE hShbSMAIL, SMAILConfig * pSMAILCfg = NULL);
static int SMAILHandleResendNotify(SVRCFG_HANDLE hSvrConfig, QUEUE_HANDLE hQueue,
				   QMSG_HANDLE hMessage, SPLF_HANDLE hFSpool);
static int SMAILTryProcessMessage(SVRCFG_HANDLE hSvrConfig, QUEUE_HANDLE hQueue,
				  QMSG_HANDLE hMessage, SHB_HANDLE hShbSMAIL,
				  SMAILConfig * pSMAILCfg);
static int SMAILTryProcessSpool(SHB_HANDLE hShbSMAIL);
static int SMAILProcessFile(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			    SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);
static int SMAILMailingListExplode(UserInfo * pUI, SPLF_HANDLE hFSpool);
static int SMAILRemoteMsgSMTPSend(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
				  SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage,
				  char const *pszDestDomain, SMTPError * pSMTPE = NULL);
static int SMAILHandleRemoteUserMessage(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
					SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue,
					QMSG_HANDLE hMessage, char const *pszDestDomain,
					char const *pszDestUser);
static int SMAILCustomProcessMessage(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
				     SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue,
				     QMSG_HANDLE hMessage, char const *pszDestDomain,
				     char const *pszDestUser, char const *pszCustFilePath);
static int SMAILCmdMacroSubstitutes(char **ppszCmdTokens, SPLF_HANDLE hFSpool);
static int SMAILCmd_external(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			     char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			     SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);
static int SMAILCmd_filter(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			   char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			   SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);
static int SMAILCmd_smtp(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			 char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			 SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);
static int SMAILCmd_smtprelay(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			      char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			      SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);
static int SMAILCmd_redirect(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			     char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			     SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);
static int SMAILCmd_lredirect(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			      char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			      SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage);

static SMAILConfig *SMAILGetConfigCopy(SHB_HANDLE hShbSMAIL)
{

	SMAILConfig *pLMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL);

	if (pLMAILCfg == NULL)
		return (NULL);

	SMAILConfig *pNewLMAILCfg = (SMAILConfig *) SysAlloc(sizeof(SMAILConfig));

	if (pNewLMAILCfg != NULL)
		memcpy(pNewLMAILCfg, pLMAILCfg, sizeof(SMAILConfig));

	ShbUnlock(hShbSMAIL);

	return (pNewLMAILCfg);

}

static int SMAILThreadCountAdd(long lCount, SHB_HANDLE hShbSMAIL, SMAILConfig * pSMAILCfg)
{

	int iDoUnlock = 0;

	if (pSMAILCfg == NULL) {
		if ((pSMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	pSMAILCfg->lThreadCount += lCount;

	if (iDoUnlock)
		ShbUnlock(hShbSMAIL);

	return (0);

}

static int SMAILLogEnabled(SHB_HANDLE hShbSMAIL, SMAILConfig * pSMAILCfg)
{

	int iDoUnlock = 0;

	if (pSMAILCfg == NULL) {
		if ((pSMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL)) == NULL)
			return (ErrGetErrorCode());

		++iDoUnlock;
	}

	unsigned long ulFlags = pSMAILCfg->ulFlags;

	if (iDoUnlock)
		ShbUnlock(hShbSMAIL);

	return ((ulFlags & SMAILF_LOG_ENABLED) ? 1 : 0);

}

unsigned int SMAILThreadProc(void *pThreadData)
{

	SMAILConfig *pSMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL);

	if (pSMAILCfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Get thread id
///////////////////////////////////////////////////////////////////////////////
	long lThreadId = pSMAILCfg->lThreadCount;

///////////////////////////////////////////////////////////////////////////////
//  Increase thread count
///////////////////////////////////////////////////////////////////////////////
	SMAILThreadCountAdd(+1, hShbSMAIL, pSMAILCfg);

	ShbUnlock(hShbSMAIL);

	SysLogMessage(LOG_LEV_MESSAGE, "SMAIL thread [%02ld] started\n", lThreadId);

	for (;;) {
///////////////////////////////////////////////////////////////////////////////
//  Check shutdown condition
///////////////////////////////////////////////////////////////////////////////
		pSMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL);

		if ((pSMAILCfg == NULL) || (pSMAILCfg->ulFlags & SMAILF_STOP_SERVER)) {
			SysLogMessage(LOG_LEV_MESSAGE, "SMAIL thread [%02ld] exiting\n",
				      lThreadId);

			if (pSMAILCfg != NULL)
				ShbUnlock(hShbSMAIL);
			break;
		}

		ShbUnlock(hShbSMAIL);

///////////////////////////////////////////////////////////////////////////////
//  Process spool files
///////////////////////////////////////////////////////////////////////////////
		SMAILTryProcessSpool(hShbSMAIL);

	}

///////////////////////////////////////////////////////////////////////////////
//  Decrease thread count
///////////////////////////////////////////////////////////////////////////////
	SMAILThreadCountAdd(-1, hShbSMAIL);

	SysLogMessage(LOG_LEV_MESSAGE, "SMAIL thread [%02ld] stopped\n", lThreadId);

	return (0);

}

static int SMAILHandleResendNotify(SVRCFG_HANDLE hSvrConfig, QUEUE_HANDLE hQueue,
				   QMSG_HANDLE hMessage, SPLF_HANDLE hFSpool)
{
///////////////////////////////////////////////////////////////////////////////
//  Check if it's time to notify about a failed delivery attempt
///////////////////////////////////////////////////////////////////////////////
	int iRetryCount = QueGetTryCount(hMessage);
	char szNotifyPattern[128] = "";

	SvrConfigVar("NotifyTryPattern", szNotifyPattern, sizeof(szNotifyPattern) - 1, hSvrConfig,
		     "");

	for (char *pszTry = szNotifyPattern; pszTry != NULL; ++pszTry) {
		if (isdigit(*pszTry) && (atoi(pszTry) == iRetryCount))
			break;

		if ((pszTry = strchr(pszTry, ',')) == NULL)
			return (0);
	}

///////////////////////////////////////////////////////////////////////////////
//  Build the notification text and send the message
///////////////////////////////////////////////////////////////////////////////
	time_t tLastTry = QueGetLastTryTime(hMessage);
	time_t tNextTry = QueGetMessageNextOp(hQueue, hMessage);
	char szTimeLast[128] = "";
	char szTimeNext[128] = "";

	MscGetTimeStr(szTimeLast, sizeof(szTimeLast) - 1, tLastTry);
	MscGetTimeStr(szTimeNext, sizeof(szTimeNext) - 1, tNextTry);

	char *pszText =
	    StrSprint("** This is a temporary error and you do not have to resend the message\r\n"
		      "** The system tried to send the message at      : %s\r\n"
		      "** The current number of delivery attempts is   : %d\r\n"
		      "** The system will try to resend the message at : %s\r\n",
		      szTimeLast, iRetryCount, szTimeNext);

	if (pszText == NULL)
		return (ErrGetErrorCode());

	int iNotifyResult = QueUtNotifyTempErrDelivery(hQueue, hMessage, hFSpool,
						       NULL, pszText, NULL);

	SysFree(pszText);

	return (iNotifyResult);

}

static int SMAILTryProcessMessage(SVRCFG_HANDLE hSvrConfig, QUEUE_HANDLE hQueue,
				  QMSG_HANDLE hMessage, SHB_HANDLE hShbSMAIL,
				  SMAILConfig * pSMAILCfg)
{
///////////////////////////////////////////////////////////////////////////////
//  Create the handle to manage the queue file
///////////////////////////////////////////////////////////////////////////////
	char szMessFilePath[SYS_MAX_PATH] = "";

	QueGetFilePath(hQueue, hMessage, szMessFilePath);

	SPLF_HANDLE hFSpool = USmlCreateHandle(szMessFilePath);

	if (hFSpool == INVALID_SPLF_HANDLE) {
		ErrorPush();
		ErrLogMessage(LOG_LEV_ERROR,
			      "Unable to load spool file \"%s\"\n"
			      "%s = \"%s\"\n",
			      szMessFilePath, SMTP_ERROR_VARNAME, "554 Error loading spool file");
		QueUtErrLogMessage(hQueue, hMessage,
				   "Unable to load spool file \"%s\"\n"
				   "%s = \"%s\"\n",
				   szMessFilePath, SMTP_ERROR_VARNAME,
				   "554 Error loading spool file");

		QueUtCleanupNotifyRoot(hQueue, hMessage, INVALID_SPLF_HANDLE,
				       ErrGetErrorString(ErrorFetch()));
		QueCloseMessage(hQueue, hMessage);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Check for mail loops
///////////////////////////////////////////////////////////////////////////////
	if (USmlMailLoopCheck(hFSpool, hSvrConfig) < 0) {
		ErrorPush();

///////////////////////////////////////////////////////////////////////////////
//  Notify root and remove the message
///////////////////////////////////////////////////////////////////////////////
		char const *pszSmtpMessageID = USmlGetSmtpMessageID(hFSpool);

		ErrLogMessage(LOG_LEV_ERROR,
			      "Message <%s> blocked by mail loop check !\n"
			      "%s = \"%s\"\n",
			      pszSmtpMessageID, SMTP_ERROR_VARNAME,
			      "554 Message blocked by mail loop check");
		QueUtErrLogMessage(hQueue, hMessage,
				   "Message <%s> blocked by mail loop check !\n"
				   "%s = \"%s\"\n",
				   pszSmtpMessageID, SMTP_ERROR_VARNAME,
				   "554 Message blocked by mail loop check");

		QueUtCleanupNotifyRoot(hQueue, hMessage, hFSpool,
				       ErrGetErrorString(ErrorFetch()));
		USmlCloseHandle(hFSpool);
		QueCloseMessage(hQueue, hMessage);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Process queue file
///////////////////////////////////////////////////////////////////////////////
	if (SMAILProcessFile(hSvrConfig, hShbSMAIL, hFSpool, hQueue, hMessage) < 0) {
		ErrorPush();

///////////////////////////////////////////////////////////////////////////////
//  Resend the message if it's not been cleaned up
///////////////////////////////////////////////////////////////////////////////
		if (QueCheckMessage(hQueue, hMessage) == 0) {
			USmlSyncChanges(hFSpool);

///////////////////////////////////////////////////////////////////////////////
//  Handle resend notifications
///////////////////////////////////////////////////////////////////////////////
			SMAILHandleResendNotify(hSvrConfig, hQueue, hMessage, hFSpool);

///////////////////////////////////////////////////////////////////////////////
//  Resend the message
///////////////////////////////////////////////////////////////////////////////
			QueUtResendMessage(hQueue, hMessage, hFSpool);

		} else
			QueCloseMessage(hQueue, hMessage);

		USmlCloseHandle(hFSpool);

		return (ErrorPop());
	}

	USmlCloseHandle(hFSpool);

///////////////////////////////////////////////////////////////////////////////
//  Cleanup message
///////////////////////////////////////////////////////////////////////////////
	QueCleanupMessage(hQueue, hMessage);

	QueCloseMessage(hQueue, hMessage);

	return (0);

}

static int SMAILTryProcessSpool(SHB_HANDLE hShbSMAIL)
{

	SMAILConfig *pSMAILCfg = SMAILGetConfigCopy(hShbSMAIL);

	if (pSMAILCfg == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Get queue file to process
///////////////////////////////////////////////////////////////////////////////
	QMSG_HANDLE hMessage = QueExtractMessage(hSpoolQueue, SMAIL_WAITMSG_TIMEOUT);

	if (hMessage != INVALID_QMSG_HANDLE) {
///////////////////////////////////////////////////////////////////////////////
//  Get configuration handle
///////////////////////////////////////////////////////////////////////////////
		SVRCFG_HANDLE hSvrConfig = SvrGetConfigHandle();

		if (hSvrConfig == INVALID_SVRCFG_HANDLE) {
			ErrorPush();
			ErrLogMessage(LOG_LEV_ERROR,
				      "Unable to load server configuration file\n"
				      "%s = \"%s\"\n", SMTP_ERROR_VARNAME,
				      "417 Unable to load server configuration file");
			QueUtErrLogMessage(hSpoolQueue, hMessage,
					   "Unable to load server configuration file\n"
					   "%s = \"%s\"\n", SMTP_ERROR_VARNAME,
					   "417 Unable to load server configuration file");

			QueUtResendMessage(hSpoolQueue, hMessage, NULL);

			SysFree(pSMAILCfg);
			return (ErrorPop());
		}
///////////////////////////////////////////////////////////////////////////////
//  Process queue file
///////////////////////////////////////////////////////////////////////////////
		SMAILTryProcessMessage(hSvrConfig, hSpoolQueue, hMessage, hShbSMAIL, pSMAILCfg);

		SvrReleaseConfigHandle(hSvrConfig);
	}

	SysFree(pSMAILCfg);

	return (0);

}

static int SMAILProcessFile(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			    SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{

	char const *const *ppszRcpt = USmlGetRcptTo(hFSpool);
	int iRcptDomains = StrStringsCount(ppszRcpt);

	char szDestUser[MAX_ADDR_NAME] = "";
	char szDestDomain[MAX_ADDR_NAME] = "";
	char szAliasFilePath[SYS_MAX_PATH] = "";

	if (iRcptDomains < 1) {
		ErrSetErrorCode(ERR_BAD_EMAIL_ADDR);
		return (ERR_BAD_EMAIL_ADDR);
	}
///////////////////////////////////////////////////////////////////////////////
//  We can have two cases here. The recipient is a simple one, or it has an
//  explicit routing (@dom1,@dom2:usr@dom).
///////////////////////////////////////////////////////////////////////////////
	if (iRcptDomains == 1) {
		if (USmtpSplitEmailAddr(ppszRcpt[0], szDestUser, szDestDomain) < 0)
			return (ErrGetErrorCode());
	} else if (USmtpSplitEmailAddr(ppszRcpt[0], NULL, szDestDomain) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Is the target handled with cmdalias ?
///////////////////////////////////////////////////////////////////////////////
	if (USmlGetCmdAliasCustomFile(hFSpool, hQueue, hMessage, szDestDomain,
				      szDestUser, szAliasFilePath) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Do cmd alias processing
///////////////////////////////////////////////////////////////////////////////
		if (SMAILCustomProcessMessage(hSvrConfig, hShbSMAIL, hFSpool, hQueue, hMessage,
					      szDestDomain, szDestUser, szAliasFilePath) < 0)
			return (ErrGetErrorCode());

	}
///////////////////////////////////////////////////////////////////////////////
//  Check if we are at home
///////////////////////////////////////////////////////////////////////////////
	else if (MDomIsHandledDomain(szDestDomain) == 0) {
		UserInfo *pUI = UsrGetUserByNameOrAlias(szDestDomain, szDestUser);

		if (pUI != NULL) {
			SysLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL local SMTP = \"%s\" From = <%s> To = <%s>\n",
				      USmlGetSMTPDomain(hFSpool), USmlMailFrom(hFSpool),
				      ppszRcpt[0]);

			if (UsrGetUserType(pUI) == usrTypeUser) {
///////////////////////////////////////////////////////////////////////////////
//  Local user case
///////////////////////////////////////////////////////////////////////////////
				LocalMailProcConfig LMPC;

				ZeroData(LMPC);
				LMPC.ulFlags =
					(SMAILLogEnabled(hShbSMAIL)) ? LMPCF_LOG_ENABLED : 0;

				if (USmlProcessLocalUserMessage
				    (hSvrConfig, pUI, hFSpool, hQueue, hMessage, LMPC) < 0) {
					ErrorPush();
					UsrFreeUserInfo(pUI);
					return (ErrorPop());
				}
			} else {
///////////////////////////////////////////////////////////////////////////////
//  Apply filters ...
///////////////////////////////////////////////////////////////////////////////
				if (FilFilterMessage
				    (hFSpool, hQueue, hMessage, FILTER_MODE_INBOUND) < 0) {
					ErrorPush();
					UsrFreeUserInfo(pUI);
					return (ErrorPop());
				}
///////////////////////////////////////////////////////////////////////////////
//  Local mailing list case
///////////////////////////////////////////////////////////////////////////////
				if (SMAILMailingListExplode(pUI, hFSpool) < 0) {
					ErrorPush();
					UsrFreeUserInfo(pUI);
					return (ErrorPop());
				}
			}

			UsrFreeUserInfo(pUI);
		} else {
			ErrorPush();
///////////////////////////////////////////////////////////////////////////////
//  No account inside the handled domain
///////////////////////////////////////////////////////////////////////////////
			char szBounceMsg[512] = "";

			SysSNPrintf(szBounceMsg, sizeof(szBounceMsg) - 1,
				    "Unknown user \"%s\" in domain \"%s\"", szDestUser,
				    szDestDomain);

			QueUtErrLogMessage(hQueue, hMessage, "%s\n", szBounceMsg);

			QueUtNotifyPermErrDelivery(hQueue, hMessage, hFSpool, szBounceMsg, NULL,
						   true);

			return (ErrorPop());
		}
	} else {
///////////////////////////////////////////////////////////////////////////////
//  Remote user case ( or custom domain user )
///////////////////////////////////////////////////////////////////////////////
		if (SMAILHandleRemoteUserMessage(hSvrConfig, hShbSMAIL, hFSpool,
						 hQueue, hMessage, szDestDomain, szDestUser) < 0)
			return (ErrGetErrorCode());

	}

	return (0);

}

static int SMAILMailingListExplode(UserInfo * pUI, SPLF_HANDLE hFSpool)
{

	char const *const *ppszFrom = USmlGetMailFrom(hFSpool);
	char const *const *ppszRcpt = USmlGetRcptTo(hFSpool);

	char szDestUser[MAX_ADDR_NAME] = "";
	char szDestDomain[MAX_ADDR_NAME] = "";

	if (USmtpSplitEmailAddr(ppszRcpt[0], szDestUser, szDestDomain) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Get Mailing List Sender address. If this account variable does not exist
//  the sender will be the "real" message sender
///////////////////////////////////////////////////////////////////////////////
	char *pszMLSender = UsrGetUserInfoVar(pUI, "ListSender");

///////////////////////////////////////////////////////////////////////////////
//  Check if the use of the Reply-To: is requested
///////////////////////////////////////////////////////////////////////////////
	int iUseReplyTo = UsrGetUserInfoVarInt(pUI, "UseReplyTo", 1);

	USRML_HANDLE hUsersDB = UsrMLOpenDB(pUI);

	if (hUsersDB == INVALID_USRML_HANDLE) {
		ErrorPush();
		SysFreeCheck(pszMLSender);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Mailing list scan
///////////////////////////////////////////////////////////////////////////////
	MLUserInfo *pMLUI = UsrMLGetFirstUser(hUsersDB);

	for (; pMLUI != NULL; pMLUI = UsrMLGetNextUser(hUsersDB)) {
		if (strchr(pMLUI->pszPerms, 'R') != NULL) {
///////////////////////////////////////////////////////////////////////////////
//  Get message handle
///////////////////////////////////////////////////////////////////////////////
			QMSG_HANDLE hMessage = QueCreateMessage(hSpoolQueue);

			if (hMessage == INVALID_QMSG_HANDLE) {
				ErrorPush();
				SysFreeCheck(pszMLSender);
				UsrMLFreeUser(pMLUI);
				UsrMLCloseDB(hUsersDB);
				return (ErrorPop());
			}

			char szQueueFilePath[SYS_MAX_PATH] = "";

			QueGetFilePath(hSpoolQueue, hMessage, szQueueFilePath);

///////////////////////////////////////////////////////////////////////////////
//  Create spool file. If "pszMLSender" is NULL the original sender is kept
///////////////////////////////////////////////////////////////////////////////
			if (USmlCreateSpoolFile
			    (hFSpool, pszMLSender, pMLUI->pszAddress, szQueueFilePath,
			     (iUseReplyTo != 0) ? "Reply-To" : "", ppszRcpt[0], NULL) < 0) {
				ErrorPush();
				QueCleanupMessage(hSpoolQueue, hMessage);
				QueCloseMessage(hSpoolQueue, hMessage);
				SysFreeCheck(pszMLSender);
				UsrMLFreeUser(pMLUI);
				UsrMLCloseDB(hUsersDB);
				return (ErrorPop());
			}
///////////////////////////////////////////////////////////////////////////////
//  Transfer file to the spool
///////////////////////////////////////////////////////////////////////////////
			if (QueCommitMessage(hSpoolQueue, hMessage) < 0) {
				ErrorPush();
				QueCleanupMessage(hSpoolQueue, hMessage);
				QueCloseMessage(hSpoolQueue, hMessage);
				SysFreeCheck(pszMLSender);
				UsrMLFreeUser(pMLUI);
				UsrMLCloseDB(hUsersDB);
				return (ErrorPop());
			}
		}

		UsrMLFreeUser(pMLUI);
	}

	UsrMLCloseDB(hUsersDB);

	SysFreeCheck(pszMLSender);

	return (0);

}

static int SMAILRemoteMsgSMTPSend(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
				  SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage,
				  char const *pszDestDomain, SMTPError * pSMTPE)
{
///////////////////////////////////////////////////////////////////////////////
//  Apply filters ...
///////////////////////////////////////////////////////////////////////////////
	if (FilFilterMessage(hFSpool, hQueue, hMessage, FILTER_MODE_OUTBOUND) < 0)
		return (ErrGetErrorCode());

	char const *pszSMTPDomain = USmlGetSMTPDomain(hFSpool);
	char const *pszSmtpMessageID = USmlGetSmtpMessageID(hFSpool);
	char const *pszSpoolFile = USmlGetSpoolFile(hFSpool);
	char const *pszSpoolFilePath = USmlGetSpoolFilePath(hFSpool);
	char const *pszMailFrom = USmlMailFrom(hFSpool);
	char const *pszSendMailFrom = USmlSendMailFrom(hFSpool);
	char const *pszRcptTo = USmlRcptTo(hFSpool);
	char const *pszSendRcptTo = USmlSendRcptTo(hFSpool);
	char const *pszRelayDomain = USmlGetRelayDomain(hFSpool);
	FileSection FS;

///////////////////////////////////////////////////////////////////////////////
//  This function retrieve the spool file message section and sync the content.
//  This is necessary before sending the file
///////////////////////////////////////////////////////////////////////////////
	if (USmlGetMsgFileSection(hFSpool, FS) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Get HELO domain
///////////////////////////////////////////////////////////////////////////////
	char szHeloDomain[MAX_HOST_NAME] = "";

	SvrConfigVar("HeloDomain", szHeloDomain, sizeof(szHeloDomain) - 1, hSvrConfig, "");

	char const *pszHeloDomain = IsEmptyString(szHeloDomain) ? NULL : szHeloDomain;

///////////////////////////////////////////////////////////////////////////////
//  If it's a relayed message use the associated relay
///////////////////////////////////////////////////////////////////////////////
	if (pszRelayDomain != NULL) {
		SysLogMessage(LOG_LEV_MESSAGE,
			      "SMAIL SMTP-Send CMX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\"\n",
			      pszRelayDomain, pszSMTPDomain, pszMailFrom, pszRcptTo);

		if (pSMTPE != NULL)
			USmtpCleanupError(pSMTPE);

		if (USmtpSendMail(pszRelayDomain, pszHeloDomain, pszSendMailFrom, pszSendRcptTo,
				  &FS, pSMTPE) < 0) {
			ErrorPush();

			char szSmtpError[512] = "";

			USmtpGetSMTPError(pSMTPE, szSmtpError, sizeof(szSmtpError));

			ErrLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL SMTP-Send CMX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
				      "%s = \"%s\"\n"
				      "%s = \"%s\"\n", pszRelayDomain, pszSMTPDomain, pszMailFrom,
				      pszRcptTo, SMTP_ERROR_VARNAME, szSmtpError,
				      SMTP_SERVER_VARNAME, USmtpGetErrorServer(pSMTPE));

			QueUtErrLogMessage(hQueue, hMessage,
					   "SMAIL SMTP-Send CMX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
					   "%s = \"%s\"\n"
					   "%s = \"%s\"\n", pszRelayDomain, pszSMTPDomain,
					   pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
					   szSmtpError, SMTP_SERVER_VARNAME,
					   USmtpGetErrorServer(pSMTPE));

			return (ErrorPop());
		}

		if (SMAILLogEnabled(hShbSMAIL))
			USmlLogMessage(hFSpool, "SMTP", pszRelayDomain);

		return (0);
	}
///////////////////////////////////////////////////////////////////////////////
//  Check the existance of direct SMTP forwarders
///////////////////////////////////////////////////////////////////////////////
	char **ppszFwdGws = USmtpGetFwdGateways(hSvrConfig, pszDestDomain);

	if (ppszFwdGws != NULL) {
///////////////////////////////////////////////////////////////////////////////
//  By initializing this to zero makes XMail to discharge all mail for domains
//  that have an empty forwarders list
///////////////////////////////////////////////////////////////////////////////
		int iSendErrorCode = 0;

		for (int ss = 0; ppszFwdGws[ss] != NULL; ss++) {
			SysLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL SMTP-Send FWD = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\"\n",
				      ppszFwdGws[ss], pszSMTPDomain, pszMailFrom, pszRcptTo);

			if (pSMTPE != NULL)
				USmtpCleanupError(pSMTPE);

			if ((iSendErrorCode = USmtpSendMail(ppszFwdGws[ss], pszHeloDomain,
							    pszSendMailFrom, pszSendRcptTo, &FS,
							    pSMTPE)) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
				if (SMAILLogEnabled(hShbSMAIL))
					USmlLogMessage(hFSpool, "FWD", ppszFwdGws[ss]);

				break;
			}

			char szSmtpError[512] = "";

			USmtpGetSMTPError(pSMTPE, szSmtpError, sizeof(szSmtpError));

			ErrLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL SMTP-Send FWD = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
				      "%s = \"%s\"\n"
				      "%s = \"%s\"\n", ppszFwdGws[ss], pszSMTPDomain, pszMailFrom,
				      pszRcptTo, SMTP_ERROR_VARNAME, szSmtpError,
				      SMTP_SERVER_VARNAME, USmtpGetErrorServer(pSMTPE));

			QueUtErrLogMessage(hQueue, hMessage,
					   "SMAIL SMTP-Send FWD = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
					   "%s = \"%s\"\n"
					   "%s = \"%s\"\n", ppszFwdGws[ss], pszSMTPDomain,
					   pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
					   szSmtpError, SMTP_SERVER_VARNAME,
					   USmtpGetErrorServer(pSMTPE));

			if ((pSMTPE != NULL) && USmtpIsFatalError(pSMTPE))
				break;
		}

		StrFreeStrings(ppszFwdGws);

		return (iSendErrorCode);
	}
///////////////////////////////////////////////////////////////////////////////
//  Try to get custom mail exchangers or DNS mail exchangers and if both tests
//  fails try direct
///////////////////////////////////////////////////////////////////////////////
	char **ppszMXGWs = USmtpGetMailExchangers(hSvrConfig, pszDestDomain);

	if (ppszMXGWs == NULL) {
		char szDomainMXHost[256] = "";
		MXS_HANDLE hMXSHandle = USmtpGetMXFirst(hSvrConfig, pszDestDomain,
							szDomainMXHost);

		if (hMXSHandle != INVALID_MXS_HANDLE) {
			int iSendErrorCode = 0;

			do {
				SysLogMessage(LOG_LEV_MESSAGE,
					      "SMAIL SMTP-Send MX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\"\n",
					      szDomainMXHost, pszSMTPDomain, pszMailFrom,
					      pszRcptTo);

				if (pSMTPE != NULL)
					USmtpCleanupError(pSMTPE);

				if ((iSendErrorCode = USmtpSendMail(szDomainMXHost, pszHeloDomain,
								    pszSendMailFrom,
								    pszSendRcptTo, &FS,
								    pSMTPE)) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
					if (SMAILLogEnabled(hShbSMAIL))
						USmlLogMessage(hFSpool, "SMTP", szDomainMXHost);

					break;
				}

				char szSmtpError[512] = "";

				USmtpGetSMTPError(pSMTPE, szSmtpError, sizeof(szSmtpError));

				ErrLogMessage(LOG_LEV_MESSAGE,
					      "SMAIL SMTP-Send MX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
					      "%s = \"%s\"\n"
					      "%s = \"%s\"\n", szDomainMXHost, pszSMTPDomain,
					      pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
					      szSmtpError, SMTP_SERVER_VARNAME,
					      USmtpGetErrorServer(pSMTPE));

				QueUtErrLogMessage(hQueue, hMessage,
						   "SMAIL SMTP-Send MX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
						   "%s = \"%s\"\n"
						   "%s = \"%s\"\n", szDomainMXHost, pszSMTPDomain,
						   pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
						   szSmtpError, SMTP_SERVER_VARNAME,
						   USmtpGetErrorServer(pSMTPE));

				if ((pSMTPE != NULL) && USmtpIsFatalError(pSMTPE))
					break;

			} while (USmtpGetMXNext(hMXSHandle, szDomainMXHost) == 0);

			USmtpMXSClose(hMXSHandle);

			if (iSendErrorCode < 0)
				return (iSendErrorCode);

		} else {
///////////////////////////////////////////////////////////////////////////////
//  MX records for destination domain not found, try direct !
///////////////////////////////////////////////////////////////////////////////
			SysLogMessage(LOG_LEV_MESSAGE,
				      "MX records for domain \"%s\" not found, trying direct.\n",
				      pszDestDomain);

			SysLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL SMTP-Send FF = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\"\n",
				      pszDestDomain, pszSMTPDomain, pszMailFrom, pszRcptTo);

			if (pSMTPE != NULL)
				USmtpCleanupError(pSMTPE);

			if (USmtpSendMail
			    (pszDestDomain, pszHeloDomain, pszSendMailFrom, pszSendRcptTo, &FS,
			     pSMTPE) < 0) {
				ErrorPush();

				char szSmtpError[512] = "";

				USmtpGetSMTPError(pSMTPE, szSmtpError, sizeof(szSmtpError));

				ErrLogMessage(LOG_LEV_MESSAGE,
					      "SMAIL SMTP-Send FF = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
					      "%s = \"%s\"\n"
					      "%s = \"%s\"\n", pszDestDomain, pszSMTPDomain,
					      pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
					      szSmtpError, SMTP_SERVER_VARNAME,
					      USmtpGetErrorServer(pSMTPE));

				QueUtErrLogMessage(hQueue, hMessage,
						   "SMAIL SMTP-Send FF = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
						   "%s = \"%s\"\n"
						   "%s = \"%s\"\n", pszDestDomain, pszSMTPDomain,
						   pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
						   szSmtpError, SMTP_SERVER_VARNAME,
						   USmtpGetErrorServer(pSMTPE));

				return (ErrorPop());
			}
///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
			if (SMAILLogEnabled(hShbSMAIL))
				USmlLogMessage(hFSpool, "SMTP", pszDestDomain);
		}
	} else {
		int iSendErrorCode = 0;

		for (int ss = 0; ppszMXGWs[ss] != NULL; ss++) {
			SysLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL SMTP-Send MX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\"\n",
				      ppszMXGWs[ss], pszSMTPDomain, pszMailFrom, pszRcptTo);

			if (pSMTPE != NULL)
				USmtpCleanupError(pSMTPE);

			if ((iSendErrorCode = USmtpSendMail(ppszMXGWs[ss], pszHeloDomain,
							    pszSendMailFrom, pszSendRcptTo, &FS,
							    pSMTPE)) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
				if (SMAILLogEnabled(hShbSMAIL))
					USmlLogMessage(hFSpool, "SMTP", ppszMXGWs[ss]);

				break;
			}

			char szSmtpError[512] = "";

			USmtpGetSMTPError(pSMTPE, szSmtpError, sizeof(szSmtpError));

			ErrLogMessage(LOG_LEV_MESSAGE,
				      "SMAIL SMTP-Send MX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
				      "%s = \"%s\"\n"
				      "%s = \"%s\"\n", ppszMXGWs[ss], pszSMTPDomain, pszMailFrom,
				      pszRcptTo, SMTP_ERROR_VARNAME, szSmtpError,
				      SMTP_SERVER_VARNAME, USmtpGetErrorServer(pSMTPE));

			QueUtErrLogMessage(hQueue, hMessage,
					   "SMAIL SMTP-Send MX = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
					   "%s = \"%s\"\n"
					   "%s = \"%s\"\n", ppszMXGWs[ss], pszSMTPDomain,
					   pszMailFrom, pszRcptTo, SMTP_ERROR_VARNAME,
					   szSmtpError, SMTP_SERVER_VARNAME,
					   USmtpGetErrorServer(pSMTPE));

			if ((pSMTPE != NULL) && USmtpIsFatalError(pSMTPE))
				break;
		}

		StrFreeStrings(ppszMXGWs);

		if (iSendErrorCode < 0)
			return (iSendErrorCode);
	}

	return (0);

}

static int SMAILHandleRemoteUserMessage(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
					SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue,
					QMSG_HANDLE hMessage, char const *pszDestDomain,
					char const *pszDestUser)
{

///////////////////////////////////////////////////////////////////////////////
//  Try domain custom processing
///////////////////////////////////////////////////////////////////////////////
	char szCustFilePath[SYS_MAX_PATH] = "";

	if (USmlGetDomainMsgCustomFile(hFSpool, hQueue, hMessage, pszDestDomain,
				       szCustFilePath) == 0)
		return (SMAILCustomProcessMessage
			(hSvrConfig, hShbSMAIL, hFSpool, hQueue, hMessage, pszDestDomain,
			 pszDestUser, szCustFilePath));

///////////////////////////////////////////////////////////////////////////////
//  Fall down to use standard SMTP delivery
///////////////////////////////////////////////////////////////////////////////
	SMTPError SMTPE;

	USmtpInitError(&SMTPE);

	if (SMAILRemoteMsgSMTPSend(hSvrConfig, hShbSMAIL, hFSpool, hQueue, hMessage,
				   pszDestDomain, &SMTPE) < 0) {
		ErrorPush();
///////////////////////////////////////////////////////////////////////////////
//  If a permanent SMTP error has been detected, then notify the message
//  sender and remove the spool file
///////////////////////////////////////////////////////////////////////////////
		if (USmtpIsFatalError(&SMTPE))
			QueUtNotifyPermErrDelivery(hQueue, hMessage, hFSpool,
						   USmtpGetErrorMessage(&SMTPE),
						   USmtpGetErrorServer(&SMTPE), true);

		USmtpCleanupError(&SMTPE);

		return (ErrorPop());
	}

	USmtpCleanupError(&SMTPE);

	return (0);

}

static int SMAILCustomProcessMessage(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
				     SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue,
				     QMSG_HANDLE hMessage, char const *pszDestDomain,
				     char const *pszDestUser, char const *pszCustFilePath)
{

	FILE *pCPFile = fopen(pszCustFilePath, "rt");

	if (pCPFile == NULL) {
		ErrSetErrorCode(ERR_FILE_OPEN, pszCustFilePath);
		return (ERR_FILE_OPEN);
	}
///////////////////////////////////////////////////////////////////////////////
//  Create pushback command file
///////////////////////////////////////////////////////////////////////////////
	char szTmpFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szTmpFile);

	FILE *pPushBFile = fopen(szTmpFile, "wt");

	if (pPushBFile == NULL) {
		fclose(pCPFile);

		ErrSetErrorCode(ERR_FILE_CREATE, szTmpFile);
		return (ERR_FILE_CREATE);
	}

	int iPushBackCmds = 0;
	char szCmdLine[CUSTOM_PROC_LINE_MAX] = "";

	while (MscGetConfigLine(szCmdLine, sizeof(szCmdLine) - 1, pCPFile) != NULL) {
		char **ppszCmdTokens = StrGetTabLineStrings(szCmdLine);

		if (ppszCmdTokens == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszCmdTokens);

		if (iFieldsCount > 0) {
///////////////////////////////////////////////////////////////////////////////
//  Do command line macro substitution
///////////////////////////////////////////////////////////////////////////////
			SMAILCmdMacroSubstitutes(ppszCmdTokens, hFSpool);

			int iCmdResult = 0;

			if (stricmp(ppszCmdTokens[0], "external") == 0)
				iCmdResult =
					SMAILCmd_external(hSvrConfig, hShbSMAIL, pszDestDomain,
							  ppszCmdTokens, iFieldsCount, hFSpool,
							  hQueue, hMessage);
			else if (stricmp(ppszCmdTokens[0], "filter") == 0)
				iCmdResult = SMAILCmd_filter(hSvrConfig, hShbSMAIL, pszDestDomain,
							     ppszCmdTokens, iFieldsCount, hFSpool,
							     hQueue, hMessage);
			else if (stricmp(ppszCmdTokens[0], "smtp") == 0)
				iCmdResult = SMAILCmd_smtp(hSvrConfig, hShbSMAIL, pszDestDomain,
							   ppszCmdTokens, iFieldsCount, hFSpool,
							   hQueue, hMessage);
			else if (stricmp(ppszCmdTokens[0], "smtprelay") == 0)
				iCmdResult =
					SMAILCmd_smtprelay(hSvrConfig, hShbSMAIL, pszDestDomain,
							   ppszCmdTokens, iFieldsCount, hFSpool,
							   hQueue, hMessage);
			else if (stricmp(ppszCmdTokens[0], "redirect") == 0)
				iCmdResult =
					SMAILCmd_redirect(hSvrConfig, hShbSMAIL, pszDestDomain,
							  ppszCmdTokens, iFieldsCount, hFSpool,
							  hQueue, hMessage);
			else if (stricmp(ppszCmdTokens[0], "lredirect") == 0)
				iCmdResult =
					SMAILCmd_lredirect(hSvrConfig, hShbSMAIL, pszDestDomain,
							   ppszCmdTokens, iFieldsCount, hFSpool,
							   hQueue, hMessage);
			else {
				SysLogMessage(LOG_LEV_ERROR,
					      "Invalid command \"%s\" in file \"%s\"\n",
					      ppszCmdTokens[0], pszCustFilePath);

			}

///////////////////////////////////////////////////////////////////////////////
//  Check for the stop-processing error code
///////////////////////////////////////////////////////////////////////////////
			if (iCmdResult == SMAIL_STOP_PROCESSING) {
				StrFreeStrings(ppszCmdTokens);
				break;
			}
///////////////////////////////////////////////////////////////////////////////
//  Test if we must save a failed command
//  <0 = Error ; ==0 = Success ; >0 = Transient error ( save the command )
///////////////////////////////////////////////////////////////////////////////
			if (iCmdResult > 0) {
				fprintf(pPushBFile, "%s\n", szCmdLine);

				++iPushBackCmds;
			}
///////////////////////////////////////////////////////////////////////////////
//  An error code might result if filters blocked the message. If this is the
//  case QueCheckMessage() will return error and we MUST stop processing
///////////////////////////////////////////////////////////////////////////////
			if ((iCmdResult < 0) && (QueCheckMessage(hQueue, hMessage) < 0)) {
				ErrorPush();
				StrFreeStrings(ppszCmdTokens);
				fclose(pPushBFile);
				fclose(pCPFile);
				SysRemove(szTmpFile);
				return (ErrorPop());
			}
		}

		StrFreeStrings(ppszCmdTokens);
	}

	fclose(pPushBFile);
	fclose(pCPFile);

	SysRemove(pszCustFilePath);

	if (iPushBackCmds > 0) {
///////////////////////////////////////////////////////////////////////////////
//  If commands left out of processing, push them into the custom file
///////////////////////////////////////////////////////////////////////////////
		if (MscMoveFile(szTmpFile, pszCustFilePath) < 0)
			return (ErrGetErrorCode());

		ErrSetErrorCode(ERR_INCOMPLETE_PROCESSING);
		return (ERR_INCOMPLETE_PROCESSING);
	}

	SysRemove(szTmpFile);

	return (0);

}

static int SMAILCmdMacroSubstitutes(char **ppszCmdTokens, SPLF_HANDLE hFSpool)
{

	char const *pszSMTPDomain = USmlGetSMTPDomain(hFSpool);
	char const *const *ppszFrom = USmlGetMailFrom(hFSpool);
	char const *const *ppszRcpt = USmlGetRcptTo(hFSpool);
	char const *pszSmtpMessageID = USmlGetSmtpMessageID(hFSpool);
	char const *pszMessageID = USmlGetSpoolFile(hFSpool);
	int iFromDomains = StrStringsCount(ppszFrom);
	int iRcptDomains = StrStringsCount(ppszRcpt);
	FileSection FS;

///////////////////////////////////////////////////////////////////////////////
//  This function retrieve the spool file message section and sync the content.
//  This is necessary before passing the file name to external programs
///////////////////////////////////////////////////////////////////////////////
	if (USmlGetMsgFileSection(hFSpool, FS) < 0)
		return (ErrGetErrorCode());

	for (int ii = 0; ppszCmdTokens[ii] != NULL; ii++) {
		if (strcmp(ppszCmdTokens[ii], "@@FROM") == 0) {
			char *pszNewValue =
				SysStrDup((iFromDomains > 0) ? ppszFrom[iFromDomains - 1] : "");

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@RCPT") == 0) {
			char *pszNewValue =
				SysStrDup((iRcptDomains > 0) ? ppszRcpt[iRcptDomains - 1] : "");

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@FILE") == 0) {
			char *pszNewValue = SysStrDup(FS.szFilePath);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@MSGID") == 0) {
			char *pszNewValue = SysStrDup(pszMessageID);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@MSGREF") == 0) {
			char *pszNewValue = SysStrDup(pszSmtpMessageID);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@TMPFILE") == 0) {
			char szTmpFile[SYS_MAX_PATH] = "";

			SysGetTmpFile(szTmpFile);

			if (MscCopyFile(szTmpFile, FS.szFilePath) < 0) {
				ErrorPush();
				CheckRemoveFile(szTmpFile);
				return (ErrorPop());
			}

			char *pszNewValue = SysStrDup(szTmpFile);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		} else if (strcmp(ppszCmdTokens[ii], "@@USERAUTH") == 0) {
			char szAuthName[MAX_ADDR_NAME] = "-";

			USmlMessageAuth(hFSpool, szAuthName, sizeof(szAuthName) - 1);

			char *pszNewValue = SysStrDup(szAuthName);

			if (pszNewValue == NULL)
				return (ErrGetErrorCode());

			SysFree(ppszCmdTokens[ii]);

			ppszCmdTokens[ii] = pszNewValue;
		}

	}

	return (0);

}

static int SMAILCmd_external(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			     char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			     SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{
///////////////////////////////////////////////////////////////////////////////
//  Apply filters ...
///////////////////////////////////////////////////////////////////////////////
	if (FilFilterMessage(hFSpool, hQueue, hMessage, FILTER_MODE_INBOUND) < 0)
		return (ErrGetErrorCode());

	if (iNumTokens < 5) {
		ErrSetErrorCode(ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
		return (ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
	}

	int iPriority = atoi(ppszCmdTokens[1]);
	int iWaitTimeout = atoi(ppszCmdTokens[2]);
	int iExitStatus = 0;

	if (SysExec(ppszCmdTokens[3], &ppszCmdTokens[3], iWaitTimeout, iPriority,
		    &iExitStatus) < 0) {
		ErrorPush();

		char const *pszMailFrom = USmlMailFrom(hFSpool);
		char const *pszRcptTo = USmlRcptTo(hFSpool);

		ErrLogMessage(LOG_LEV_MESSAGE,
			      "SMAIL EXTRN-Send Prg = \"%s\" Domain = \"%s\" From = \"%s\" To = \"%s\" Failed !\n",
			      ppszCmdTokens[3], pszDestDomain, pszMailFrom, pszRcptTo);

		QueUtErrLogMessage(hQueue, hMessage,
				   "SMAIL EXTRN-Send Prg = \"%s\" Domain = \"%s\" From = \"%s\" To = \"%s\" Failed !\n",
				   ppszCmdTokens[3], pszDestDomain, pszMailFrom, pszRcptTo);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
	if (SMAILLogEnabled(hShbSMAIL))
		USmlLogMessage(hFSpool, "EXTRN", ppszCmdTokens[3]);

	return ((iExitStatus == SMAIL_EXTERNAL_EXIT_BREAK) ? SMAIL_STOP_PROCESSING: 0);

}

static int SMAILCmd_filter(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			   char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			   SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{

	if (iNumTokens < 5) {
		ErrSetErrorCode(ERR_BAD_MAILPROC_CMD_SYNTAX);
		return (ERR_BAD_MAILPROC_CMD_SYNTAX);
	}

	int iPriority = atoi(ppszCmdTokens[1]);
	int iWaitTimeout = atoi(ppszCmdTokens[2]);
	int iExitStatus = 0;

	if (SysExec(ppszCmdTokens[3], &ppszCmdTokens[3], iWaitTimeout, iPriority,
		    &iExitStatus) < 0) {
		ErrorPush();

		char const *pszMailFrom = USmlMailFrom(hFSpool);
		char const *pszRcptTo = USmlRcptTo(hFSpool);

		ErrLogMessage(LOG_LEV_MESSAGE,
			      "USMAIL FILTER Prg = \"%s\" From = \"%s\" To = \"%s\" Failed !\n",
			      ppszCmdTokens[3], pszMailFrom, pszRcptTo);

		QueUtErrLogMessage(hQueue, hMessage,
				   "USMAIL FILTER Prg = \"%s\" From = \"%s\" To = \"%s\" Failed !\n",
				   ppszCmdTokens[3], pszMailFrom, pszRcptTo);

		return (ErrorPop());
	}

///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
	if (SMAILLogEnabled(hShbSMAIL))
		USmlLogMessage(hFSpool, "FILTER", ppszCmdTokens[3]);

///////////////////////////////////////////////////////////////////////////////
//  Separate code from flags
///////////////////////////////////////////////////////////////////////////////
	int iExitFlags = iExitStatus & FILTER_FLAGS_MASK;

	iExitStatus &= ~FILTER_FLAGS_MASK;

	if ((iExitStatus == FILTER_OUT_EXITCODE) ||
	    (iExitStatus == FILTER_OUT_NN_EXITCODE) ||
	    (iExitStatus == FILTER_OUT_NNF_EXITCODE)) {
///////////////////////////////////////////////////////////////////////////////
//  Filter out message
///////////////////////////////////////////////////////////////////////////////
		char *pszRejMsg = FilGetFilterRejMessage(USmlGetSpoolFilePath(hFSpool));

		if (iExitStatus == FILTER_OUT_EXITCODE)
			QueUtNotifyPermErrDelivery(hQueue, hMessage, NULL,
						   (pszRejMsg != NULL) ? pszRejMsg :
						   ErrGetErrorString(ERR_FILTERED_MESSAGE),
						   NULL, true);
		else if (iExitStatus == FILTER_OUT_NN_EXITCODE)
			QueCleanupMessage(hQueue, hMessage,
					  !QueUtRemoveSpoolErrors());
		else
			QueCleanupMessage(hQueue, hMessage, false);

		if (pszRejMsg != NULL)
			SysFree(pszRejMsg);

		ErrSetErrorCode(ERR_FILTERED_MESSAGE);
		return (ERR_FILTERED_MESSAGE);
	} else if (iExitStatus == FILTER_MODIFY_EXITCODE) {
///////////////////////////////////////////////////////////////////////////////
//  Filter modified the message, we need to reload the spool handle
///////////////////////////////////////////////////////////////////////////////
		if (USmlReloadHandle(hFSpool) < 0) {
			ErrorPush();

			char const *pszMailFrom = USmlMailFrom(hFSpool);
			char const *pszRcptTo = USmlRcptTo(hFSpool);

			SysLogMessage(LOG_LEV_MESSAGE,
				      "Filter error [ Modified message corrupted ]: Sender = \"%s\" Recipient = \"%s\" (%s)\n",
				      pszMailFrom, pszRcptTo,
				      ppszCmdTokens[3]);

			QueUtErrLogMessage(hQueue, hMessage,
					   "Filter error [ Modified message corrupted ]: Sender = \"%s\" Recipient = \"%s\" (%s)\n",
					   pszMailFrom, pszRcptTo,
					   ppszCmdTokens[3]);

			QueCleanupMessage(hQueue, hMessage, true);

			return (ErrorPop());
		}
	}

	return ((iExitFlags & FILTER_FLAGS_BREAK) ? SMAIL_STOP_PROCESSING: 0);

}

static int SMAILCmd_smtp(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			 char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			 SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{

	if (iNumTokens != 1) {
		ErrSetErrorCode(ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
		return (ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
	}

	SMTPError SMTPE;

	USmtpInitError(&SMTPE);

	if (SMAILRemoteMsgSMTPSend(hSvrConfig, hShbSMAIL, hFSpool, hQueue, hMessage,
				   pszDestDomain, &SMTPE) < 0) {
///////////////////////////////////////////////////////////////////////////////
//  If we get an SMTP fatal error We must return <0 , otherwise >0 to give
//  XMail to ability to resume the command
///////////////////////////////////////////////////////////////////////////////
		int iReturnCode = USmtpIsFatalError(&SMTPE) ? ErrGetErrorCode() :
		    -ErrGetErrorCode();

///////////////////////////////////////////////////////////////////////////////
//  If a permanent SMTP error has been detected, then notify the message sender
///////////////////////////////////////////////////////////////////////////////
		if (USmtpIsFatalError(&SMTPE))
			QueUtNotifyPermErrDelivery(hQueue, hMessage, hFSpool,
						   USmtpGetErrorMessage(&SMTPE),
						   USmtpGetErrorServer(&SMTPE), false);

		USmtpCleanupError(&SMTPE);

		return (iReturnCode);
	}

	USmtpCleanupError(&SMTPE);

	return (0);

}

static int SMAILCmd_smtprelay(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			      char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			      SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{
///////////////////////////////////////////////////////////////////////////////
//  Apply filters ...
///////////////////////////////////////////////////////////////////////////////
	if (FilFilterMessage(hFSpool, hQueue, hMessage, FILTER_MODE_OUTBOUND) < 0)
		return (ErrGetErrorCode());

	if (iNumTokens != 2) {
		ErrSetErrorCode(ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
		return (ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
	}

	char **ppszRelays = NULL;

	if (ppszCmdTokens[1][0] == '#') {
		if ((ppszRelays = StrTokenize(ppszCmdTokens[1] + 1, ",")) != NULL) {
			int iRelayCount = StrStringsCount(ppszRelays);

			srand((unsigned int) time(NULL));

			for (int ii = 0; ii < (iRelayCount / 2); ii++) {
				int iSwap1 = rand() % iRelayCount;
				int iSwap2 = rand() % iRelayCount;
				char *pszRly1 = ppszRelays[iSwap1];
				char *pszRly2 = ppszRelays[iSwap2];

				ppszRelays[iSwap1] = pszRly2;
				ppszRelays[iSwap2] = pszRly1;
			}
		}
	} else
		ppszRelays = StrTokenize(ppszCmdTokens[1], ",");

	if (ppszRelays == NULL)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  This function retrieve the spool file message section and sync the content.
//  This is necessary before sending the file
///////////////////////////////////////////////////////////////////////////////
	FileSection FS;

	if (USmlGetMsgFileSection(hFSpool, FS) < 0) {
		ErrorPush();
		StrFreeStrings(ppszRelays);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Get spool file infos
///////////////////////////////////////////////////////////////////////////////
	char const *pszSMTPDomain = USmlGetSMTPDomain(hFSpool);
	char const *pszMailFrom = USmlMailFrom(hFSpool);
	char const *pszRcptTo = USmlRcptTo(hFSpool);
	char const *pszSendMailFrom = USmlSendMailFrom(hFSpool);
	char const *pszSendRcptTo = USmlSendRcptTo(hFSpool);
	char const *pszSpoolFilePath = USmlGetSpoolFilePath(hFSpool);

///////////////////////////////////////////////////////////////////////////////
//  Get HELO domain
///////////////////////////////////////////////////////////////////////////////
	char szHeloDomain[MAX_HOST_NAME] = "";

	SvrConfigVar("HeloDomain", szHeloDomain, sizeof(szHeloDomain) - 1, hSvrConfig, "");

	char const *pszHeloDomain = IsEmptyString(szHeloDomain) ? NULL : szHeloDomain;

	SMTPError SMTPE;

	USmtpInitError(&SMTPE);

///////////////////////////////////////////////////////////////////////////////
//  By initializing this to zero makes XMail to discharge all mail for domains
//  that have an empty relay list
///////////////////////////////////////////////////////////////////////////////
	int iReturnCode = 0;

	for (int ss = 0; ppszRelays[ss] != NULL; ss++) {
		SysLogMessage(LOG_LEV_MESSAGE,
			      "SMAIL SMTP-Send RLYS = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\"\n",
			      ppszRelays[ss], pszSMTPDomain, pszMailFrom, pszRcptTo);

		USmtpCleanupError(&SMTPE);

		if (USmtpSendMail(ppszRelays[ss], pszHeloDomain, pszSendMailFrom, pszSendRcptTo,
				  &FS, &SMTPE) == 0) {
///////////////////////////////////////////////////////////////////////////////
//  Log Mailer operation
///////////////////////////////////////////////////////////////////////////////
			if (SMAILLogEnabled(hShbSMAIL))
				USmlLogMessage(hFSpool, "RLYS", ppszRelays[ss]);

			USmtpCleanupError(&SMTPE);

			StrFreeStrings(ppszRelays);

			return (0);
		}

		int iErrorCode = ErrGetErrorCode();
		char szSmtpError[512] = "";

		USmtpGetSMTPError(&SMTPE, szSmtpError, sizeof(szSmtpError));

		ErrLogMessage(LOG_LEV_MESSAGE,
			      "SMAIL SMTP-Send RLYS = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
			      "%s = \"%s\"\n"
			      "%s = \"%s\"\n", ppszRelays[ss], pszSMTPDomain, pszMailFrom,
			      pszRcptTo, SMTP_ERROR_VARNAME, szSmtpError, SMTP_SERVER_VARNAME,
			      USmtpGetErrorServer(&SMTPE));

		QueUtErrLogMessage(hQueue, hMessage,
				   "SMAIL SMTP-Send RLYS = \"%s\" SMTP = \"%s\" From = \"%s\" To = \"%s\" Failed !\n"
				   "%s = \"%s\"\n"
				   "%s = \"%s\"\n", ppszRelays[ss], pszSMTPDomain, pszMailFrom,
				   pszRcptTo, SMTP_ERROR_VARNAME, szSmtpError,
				   SMTP_SERVER_VARNAME, USmtpGetErrorServer(&SMTPE));

///////////////////////////////////////////////////////////////////////////////
//  If a permanent SMTP error has been detected, then notify the message sender
///////////////////////////////////////////////////////////////////////////////
		if (USmtpIsFatalError(&SMTPE))
			QueUtNotifyPermErrDelivery(hQueue, hMessage, hFSpool,
						   USmtpGetErrorMessage(&SMTPE),
						   USmtpGetErrorServer(&SMTPE), false);

		iReturnCode = USmtpIsFatalError(&SMTPE) ? iErrorCode : -iErrorCode;
	}

	USmtpCleanupError(&SMTPE);

	StrFreeStrings(ppszRelays);

	return (iReturnCode);

}

static int SMAILCmd_redirect(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			     char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			     SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{

	if (iNumTokens < 2) {
		ErrSetErrorCode(ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
		return (ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
	}

	char const *pszSMTPDomain = USmlGetSMTPDomain(hFSpool);
	char const *const *ppszFrom = USmlGetMailFrom(hFSpool);
	char const *const *ppszRcpt = USmlGetRcptTo(hFSpool);

	int iFromDomains = StrStringsCount(ppszFrom);
	int iRcptDomains = StrStringsCount(ppszRcpt);

	char szLocalUser[MAX_ADDR_NAME] = "";
	char szLocalDomain[MAX_ADDR_NAME] = "";

	if ((iRcptDomains < 1) ||
	    (USmtpSplitEmailAddr(ppszRcpt[iRcptDomains - 1], szLocalUser, szLocalDomain) < 0))
		return (ErrGetErrorCode());

	for (int ii = 1; ppszCmdTokens[ii] != NULL; ii++) {
///////////////////////////////////////////////////////////////////////////////
//  Get message handle
///////////////////////////////////////////////////////////////////////////////
		QMSG_HANDLE hRedirMessage = QueCreateMessage(hSpoolQueue);

		if (hRedirMessage == INVALID_QMSG_HANDLE)
			return (ErrGetErrorCode());

		char szQueueFilePath[SYS_MAX_PATH] = "";

		QueGetFilePath(hSpoolQueue, hRedirMessage, szQueueFilePath);

		char szAliasAddr[MAX_ADDR_NAME] = "";

		if (strchr(ppszCmdTokens[ii], '@') == NULL)
			SysSNPrintf(szAliasAddr, sizeof(szAliasAddr) - 1, "%s@%s",
				    szLocalUser, ppszCmdTokens[ii]);
		else
			StrSNCpy(szAliasAddr, ppszCmdTokens[ii]);

		if (USmlCreateSpoolFile(hFSpool, NULL, szAliasAddr, szQueueFilePath, NULL) < 0) {
			ErrorPush();
			QueCleanupMessage(hSpoolQueue, hRedirMessage);
			QueCloseMessage(hSpoolQueue, hRedirMessage);
			return (ErrorPop());
		}
///////////////////////////////////////////////////////////////////////////////
//  Transfer file to the spool
///////////////////////////////////////////////////////////////////////////////
		if (QueCommitMessage(hSpoolQueue, hRedirMessage) < 0) {
			ErrorPush();
			QueCleanupMessage(hSpoolQueue, hRedirMessage);
			QueCloseMessage(hSpoolQueue, hRedirMessage);
			return (ErrorPop());
		}
	}

	return (0);

}

static int SMAILCmd_lredirect(SVRCFG_HANDLE hSvrConfig, SHB_HANDLE hShbSMAIL,
			      char const *pszDestDomain, char **ppszCmdTokens, int iNumTokens,
			      SPLF_HANDLE hFSpool, QUEUE_HANDLE hQueue, QMSG_HANDLE hMessage)
{

	if (iNumTokens < 2) {
		ErrSetErrorCode(ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
		return (ERR_BAD_DOMAIN_PROC_CMD_SYNTAX);
	}

	char const *const *ppszFrom = USmlGetMailFrom(hFSpool);
	char const *const *ppszRcpt = USmlGetRcptTo(hFSpool);

	int iFromDomains = StrStringsCount(ppszFrom);
	int iRcptDomains = StrStringsCount(ppszRcpt);

	char szLocalUser[MAX_ADDR_NAME] = "";
	char szLocalDomain[MAX_ADDR_NAME] = "";

	if ((iRcptDomains < 1) ||
	    (USmtpSplitEmailAddr(ppszRcpt[iRcptDomains - 1], szLocalUser, szLocalDomain) < 0))
		return (ErrGetErrorCode());

	for (int ii = 1; ppszCmdTokens[ii] != NULL; ii++) {
///////////////////////////////////////////////////////////////////////////////
//  Get message handle
///////////////////////////////////////////////////////////////////////////////
		QMSG_HANDLE hRedirMessage = QueCreateMessage(hSpoolQueue);

		if (hRedirMessage == INVALID_QMSG_HANDLE)
			return (ErrGetErrorCode());

		char szQueueFilePath[SYS_MAX_PATH] = "";

		QueGetFilePath(hSpoolQueue, hRedirMessage, szQueueFilePath);

		char szAliasAddr[MAX_ADDR_NAME] = "";

		if (strchr(ppszCmdTokens[ii], '@') == NULL)
			SysSNPrintf(szAliasAddr, sizeof(szAliasAddr) - 1, "%s@%s",
				    szLocalUser, ppszCmdTokens[ii]);
		else
			StrSNCpy(szAliasAddr, ppszCmdTokens[ii]);

		if (USmlCreateSpoolFile(hFSpool, ppszRcpt[iRcptDomains - 1], szAliasAddr,
					szQueueFilePath, NULL) < 0) {
			ErrorPush();
			QueCleanupMessage(hSpoolQueue, hRedirMessage);
			QueCloseMessage(hSpoolQueue, hRedirMessage);
			return (ErrorPop());
		}
///////////////////////////////////////////////////////////////////////////////
//  Transfer file to the spool
///////////////////////////////////////////////////////////////////////////////
		if (QueCommitMessage(hSpoolQueue, hRedirMessage) < 0) {
			ErrorPush();
			QueCleanupMessage(hSpoolQueue, hRedirMessage);
			QueCloseMessage(hSpoolQueue, hRedirMessage);
			return (ErrorPop());
		}
	}

	return (0);

}
