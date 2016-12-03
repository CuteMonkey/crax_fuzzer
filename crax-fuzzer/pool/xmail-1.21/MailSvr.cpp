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
#include "MailConfig.h"
#include "MiscUtils.h"
#include "ResLocks.h"
#include "POP3Svr.h"
#include "SMTPSvr.h"
#include "SMAILSvr.h"
#include "PSYNCSvr.h"
#include "DNS.h"
#include "DNSCache.h"
#include "UsrUtils.h"
#include "SvrUtils.h"
#include "MessQueue.h"
#include "SMAILUtils.h"
#include "QueueUtils.h"
#include "ExtAliases.h"
#include "AliasDomain.h"
#include "MailDomains.h"
#include "POP3GwLink.h"
#include "CTRLSvr.h"
#include "FINGSvr.h"
#include "LMAILSvr.h"
#include "DynDNS.h"
#include "AppDefines.h"
#include "MailSvr.h"

#define ENV_MAIN_PATH               "MAIL_ROOT"
#define ENV_CMD_LINE                "MAIL_CMD_LINE"
#define SVR_SHUTDOWN_FILE           ".shutdown"
#define STD_SMAIL_THREADS           16
#define MAX_SMAIL_THREADS           256
#define STD_SMAIL_RETRY_TIMEOUT     480
#define STD_SMAIL_RETRY_INCR_RATIO  16
#define STD_SMAIL_MAX_RETRY         32
#define STD_PSYNC_INTERVAL          120
#define STD_PSYNC_NUM_THREADS       8
#define MAX_PSYNC_NUM_THREADS       32
#define STD_POP3_BADLOGIN_WAIT      5
#define MAX_POP3_THREADS            1024
#define MAX_SMTP_THREADS            1024
#define STD_SMTP_MAX_RCPTS          100
#define MAX_CTRL_THREADS            512
#define STD_LMAIL_THREADS           3
#define MAX_LMAIL_THREADS           17
#define STD_LMAILTHREAD_SLEEP_TIME  2
#define SVR_EXIT_WAIT               480
#define STD_SERVER_SESSION_TIMEOUT  90
#define MAX_CLIENTS_WAIT            300
#define CTRL_SERVER_SESSION_TIMEOUT 120
#define SERVER_SLEEP_TIMESLICE      2
#define SHUTDOWN_CHECK_TIME         2
#define STD_POP3AUTH_EXPIRE_TIME    (15 * 60)
#define FILTER_TIMEOUT              90

static void SvrShutdownCleanup(void);
static int SvrSetShutdown(void);
static int SvrSetupCTRL(int iArgCount, char *pszArgs[]);
static void SvrCleanupCTRL(void);
static int SvrSetupFING(int iArgCount, char *pszArgs[]);
static void SvrCleanupFING(void);
static int SvrSetupPOP3(int iArgCount, char *pszArgs[]);
static void SvrCleanupPOP3(void);
static int SvrSetupSMTP(int iArgCount, char *pszArgs[]);
static void SvrCleanupSMTP(void);
static int SvrSetupSMAIL(int iArgCount, char *pszArgs[]);
static void SvrCleanupSMAIL(void);
static int SvrSetupPSYNC(int iArgCount, char *pszArgs[]);
static void SvrCleanupPSYNC(void);
static int SvrSetupLMAIL(int iArgCount, char *pszArgs[]);
static void SvrCleanupLMAIL(void);
static int SvrSetup(int iArgCount, char *pszArgs[]);
static void SvrCleanup(void);
static void SvrBreakHandler(void);
static char **SvrMergeArgs(int iArgs, char *pszArgs[], int &iArgsCount);

///////////////////////////////////////////////////////////////////////////////
//  External visible variabiles
///////////////////////////////////////////////////////////////////////////////
SHB_HANDLE hShbFING;
SHB_HANDLE hShbCTRL;
SHB_HANDLE hShbPOP3;
SHB_HANDLE hShbSMTP;
SHB_HANDLE hShbSMAIL;
SHB_HANDLE hShbPSYNC;
SHB_HANDLE hShbLMAIL;
char szMailPath[SYS_MAX_PATH];
QUEUE_HANDLE hSpoolQueue;
SYS_SEMAPHORE hSyncSem;
bool bServerDebug;
int iFilterTimeout = FILTER_TIMEOUT;
bool bFilterLogEnabled = false;
int iLogRotateHours = LOG_ROTATE_HOURS;
int iQueueSplitLevel = STD_QUEUEFS_DIRS_X_LEVEL;

#ifdef __UNIX__
int iMailboxType = XMAIL_MAILDIR;
#else
int iMailboxType = XMAIL_MAILBOX;
#endif

///////////////////////////////////////////////////////////////////////////////
//  Local visible variabiles
///////////////////////////////////////////////////////////////////////////////
static char szShutdownFile[SYS_MAX_PATH];
static bool bServerShutdown = false;
static int iNumSMAILThreads;
static int iNumLMAILThreads;
static SYS_THREAD hCTRLThread;
static SYS_THREAD hFINGThread;
static SYS_THREAD hPOP3Thread;
static SYS_THREAD hSMTPThread;
static SYS_THREAD hSMAILThreads[MAX_SMAIL_THREADS];
static SYS_THREAD hLMAILThreads[MAX_LMAIL_THREADS];
static SYS_THREAD hPSYNCThread;

static void SvrShutdownCleanup(void)
{

	CheckRemoveFile(szShutdownFile);

	bServerShutdown = false;

}

static int SvrSetShutdown(void)
{
///////////////////////////////////////////////////////////////////////////////
//  Set the shutdown flag and shutdown the library
///////////////////////////////////////////////////////////////////////////////
	bServerShutdown = true;

	SysShutdownLibrary();

	FILE *pFile = fopen(szShutdownFile, "wt");

	if (pFile == NULL) {
		ErrSetErrorCode(ERR_FILE_CREATE, szShutdownFile);
		return (ERR_FILE_CREATE);
	}

	char szShutdownTimeStr[256] = "";

	MscGetTimeStr(szShutdownTimeStr, sizeof(szShutdownTimeStr));

	fprintf(pFile, "%s\n", szShutdownTimeStr);

	fclose(pFile);

	return (0);

}

static int SvrSetupCTRL(int iArgCount, char *pszArgs[])
{

	int iPort = STD_CTRL_PORT;
	int iSessionTimeout = CTRL_SERVER_SESSION_TIMEOUT;
	int iNumAddr = 0;
	long lMaxThreads = MAX_CTRL_THREADS;
	unsigned long ulFlags = 0;
	SYS_INET_ADDR SvrAddr[MAX_CTRL_ACCEPT_ADDRESSES];

	for (int ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'C'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('p'):
			if (++ii < iArgCount)
				iPort = atoi(pszArgs[ii]);
			break;

		case ('t'):
			if (++ii < iArgCount)
				iSessionTimeout = atoi(pszArgs[ii]);
			break;

		case ('l'):
			ulFlags |= CTRLF_LOG_ENABLED;
			break;

		case ('I'):
			if ((++ii < iArgCount) &&
			    (MscGetServerAddress(pszArgs[ii], SvrAddr[iNumAddr]) == 0))
				++iNumAddr;
			break;

		case ('X'):
			if (++ii < iArgCount)
				lMaxThreads = atol(pszArgs[ii]);
			break;
		}
	}

	if ((hShbCTRL = ShbCreateBlock(sizeof(CTRLConfig))) == SHB_INVALID_HANDLE)
		return (ErrGetErrorCode());

	CTRLConfig *pCTRLCfg = (CTRLConfig *) ShbLock(hShbCTRL);

	if (pCTRLCfg == NULL) {
		ErrorPush();
		ShbCloseBlock(hShbCTRL);
		return (ErrorPop());
	}

	pCTRLCfg->iPort = iPort;
	pCTRLCfg->ulFlags = ulFlags;
	pCTRLCfg->lThreadCount = 0;
	pCTRLCfg->lMaxThreads = lMaxThreads;
	pCTRLCfg->iSessionTimeout = iSessionTimeout;
	pCTRLCfg->iTimeout = STD_SERVER_TIMEOUT;
	pCTRLCfg->iNumAddr = iNumAddr;

	for (int nn = 0; nn < iNumAddr; nn++)
		pCTRLCfg->SvrAddr[nn] = SvrAddr[nn];

	ShbUnlock(hShbCTRL);

	if ((hCTRLThread = SysCreateThread(CTRLThreadProc, NULL)) == SYS_INVALID_THREAD) {
		ShbCloseBlock(hShbCTRL);
		return (ErrGetErrorCode());
	}

	return (0);

}

static void SvrCleanupCTRL(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop CTRL Thread
///////////////////////////////////////////////////////////////////////////////
	CTRLConfig *pCTRLCfg = (CTRLConfig *) ShbLock(hShbCTRL);

	pCTRLCfg->ulFlags |= CTRLF_STOP_SERVER;

	ShbUnlock(hShbCTRL);

///////////////////////////////////////////////////////////////////////////////
//  Wait CTRL
///////////////////////////////////////////////////////////////////////////////
	SysWaitThread(hCTRLThread, SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close CTRL Thread
///////////////////////////////////////////////////////////////////////////////
	SysCloseThread(hCTRLThread, 1);

	ShbCloseBlock(hShbCTRL);

}

static int SvrSetupFING(int iArgCount, char *pszArgs[])
{

	int iPort = STD_FINGER_PORT;
	int iNumAddr = 0;
	unsigned long ulFlags = 0;
	SYS_INET_ADDR SvrAddr[MAX_FING_ACCEPT_ADDRESSES];

	for (int ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'F'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('p'):
			if (++ii < iArgCount)
				iPort = atoi(pszArgs[ii]);
			break;

		case ('l'):
			ulFlags |= FINGF_LOG_ENABLED;
			break;

		case ('I'):
			if ((++ii < iArgCount) &&
			    (MscGetServerAddress(pszArgs[ii], SvrAddr[iNumAddr]) == 0))
				++iNumAddr;
			break;
		}
	}

	if ((hShbFING = ShbCreateBlock(sizeof(FINGConfig))) == SHB_INVALID_HANDLE)
		return (ErrGetErrorCode());

	FINGConfig *pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

	if (pFINGCfg == NULL) {
		ShbCloseBlock(hShbFING);
		return (ErrGetErrorCode());
	}

	pFINGCfg->iPort = iPort;
	pFINGCfg->ulFlags = ulFlags;
	pFINGCfg->lThreadCount = 0;
	pFINGCfg->iTimeout = STD_SERVER_TIMEOUT;
	pFINGCfg->iNumAddr = iNumAddr;

	for (int nn = 0; nn < iNumAddr; nn++)
		pFINGCfg->SvrAddr[nn] = SvrAddr[nn];

	ShbUnlock(hShbFING);

	if ((hFINGThread = SysCreateThread(FINGThreadProc, NULL)) == SYS_INVALID_THREAD) {
		ShbCloseBlock(hShbFING);
		return (ErrGetErrorCode());
	}

	return (0);

}

static void SvrCleanupFING(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop FING Thread
///////////////////////////////////////////////////////////////////////////////
	FINGConfig *pFINGCfg = (FINGConfig *) ShbLock(hShbFING);

	pFINGCfg->ulFlags |= FINGF_STOP_SERVER;

	ShbUnlock(hShbFING);

///////////////////////////////////////////////////////////////////////////////
//  Wait FINGER
///////////////////////////////////////////////////////////////////////////////
	SysWaitThread(hFINGThread, SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close FINGER Thread
///////////////////////////////////////////////////////////////////////////////
	SysCloseThread(hFINGThread, 1);

	ShbCloseBlock(hShbFING);

}

static int SvrSetupPOP3(int iArgCount, char *pszArgs[])
{

	int iPort = STD_POP3_PORT;
	int iSessionTimeout = STD_SERVER_SESSION_TIMEOUT;
	int iBadLoginWait = STD_POP3_BADLOGIN_WAIT;
	int iNumAddr = 0;
	long lMaxThreads = MAX_POP3_THREADS;
	unsigned long ulFlags = 0;
	SYS_INET_ADDR SvrAddr[MAX_POP3_ACCEPT_ADDRESSES];

	for (int ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'P'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('p'):
			if (++ii < iArgCount)
				iPort = atoi(pszArgs[ii]);
			break;

		case ('t'):
			if (++ii < iArgCount)
				iSessionTimeout = atoi(pszArgs[ii]);
			break;

		case ('w'):
			if (++ii < iArgCount)
				iBadLoginWait = atoi(pszArgs[ii]);
			break;

		case ('l'):
			ulFlags |= POP3F_LOG_ENABLED;
			break;

		case ('h'):
			ulFlags |= POP3F_HANG_ON_BADLOGIN;
			break;

		case ('I'):
			if ((++ii < iArgCount) &&
			    (MscGetServerAddress(pszArgs[ii], SvrAddr[iNumAddr]) == 0))
				++iNumAddr;
			break;

		case ('X'):
			if (++ii < iArgCount)
				lMaxThreads = atol(pszArgs[ii]);
			break;
		}
	}

	if ((hShbPOP3 = ShbCreateBlock(sizeof(POP3Config))) == SHB_INVALID_HANDLE)
		return (ErrGetErrorCode());

	POP3Config *pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3);

	if (pPOP3Cfg == NULL) {
		ShbCloseBlock(hShbPOP3);
		return (ErrGetErrorCode());
	}

	pPOP3Cfg->iPort = iPort;
	pPOP3Cfg->ulFlags = ulFlags;
	pPOP3Cfg->lThreadCount = 0;
	pPOP3Cfg->lMaxThreads = lMaxThreads;
	pPOP3Cfg->iSessionTimeout = iSessionTimeout;
	pPOP3Cfg->iTimeout = STD_SERVER_TIMEOUT;
	pPOP3Cfg->iBadLoginWait = iBadLoginWait;
	pPOP3Cfg->iNumAddr = iNumAddr;

	for (int nn = 0; nn < iNumAddr; nn++)
		pPOP3Cfg->SvrAddr[nn] = SvrAddr[nn];

	ShbUnlock(hShbPOP3);

///////////////////////////////////////////////////////////////////////////////
//  Remove POP3 lock files
///////////////////////////////////////////////////////////////////////////////
	UsrClearPop3LocksDir();

	if ((hPOP3Thread = SysCreateThread(POP3ThreadProc, NULL)) == SYS_INVALID_THREAD) {
		ShbCloseBlock(hShbPOP3);
		return (ErrGetErrorCode());
	}

	return (0);

}

static void SvrCleanupPOP3(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop POP3 Thread
///////////////////////////////////////////////////////////////////////////////
	POP3Config *pPOP3Cfg = (POP3Config *) ShbLock(hShbPOP3);

	pPOP3Cfg->ulFlags |= POP3F_STOP_SERVER;

	ShbUnlock(hShbPOP3);

///////////////////////////////////////////////////////////////////////////////
//  Wait POP3 Thread
///////////////////////////////////////////////////////////////////////////////
	SysWaitThread(hPOP3Thread, SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close POP3 Thread
///////////////////////////////////////////////////////////////////////////////
	SysCloseThread(hPOP3Thread, 1);

	ShbCloseBlock(hShbPOP3);

}

static int SvrSetupSMTP(int iArgCount, char *pszArgs[])
{

	int iPort = STD_SMTP_PORT;
	int iSessionTimeout = STD_SERVER_SESSION_TIMEOUT;
	int iMaxRcpts = STD_SMTP_MAX_RCPTS;
	int iNumAddr = 0;
	unsigned int uPopAuthExpireTime = STD_POP3AUTH_EXPIRE_TIME;
	long lMaxThreads = MAX_SMTP_THREADS;
	unsigned long ulFlags = 0;
	SYS_INET_ADDR SvrAddr[MAX_SMTP_ACCEPT_ADDRESSES];

	for (int ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'S'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('p'):
			if (++ii < iArgCount)
				iPort = atoi(pszArgs[ii]);
			break;

		case ('t'):
			if (++ii < iArgCount)
				iSessionTimeout = atoi(pszArgs[ii]);
			break;

		case ('l'):
			ulFlags |= SMTPF_LOG_ENABLED;
			break;

		case ('I'):
			if ((++ii < iArgCount) &&
			    (MscGetServerAddress(pszArgs[ii], SvrAddr[iNumAddr]) == 0))
				++iNumAddr;
			break;

		case ('X'):
			if (++ii < iArgCount)
				lMaxThreads = atol(pszArgs[ii]);
			break;

		case ('r'):
			if (++ii < iArgCount)
				iMaxRcpts = atoi(pszArgs[ii]);
			break;

		case ('e'):
			if (++ii < iArgCount)
				uPopAuthExpireTime = (unsigned int) atol(pszArgs[ii]);
			break;

		}
	}

	if ((hShbSMTP = ShbCreateBlock(sizeof(SMTPConfig))) == SHB_INVALID_HANDLE)
		return (ErrGetErrorCode());

	SMTPConfig *pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP);

	if (pSMTPCfg == NULL) {
		ShbCloseBlock(hShbSMTP);
		return (ErrGetErrorCode());
	}

	pSMTPCfg->iPort = iPort;
	pSMTPCfg->ulFlags = ulFlags;
	pSMTPCfg->lThreadCount = 0;
	pSMTPCfg->lMaxThreads = lMaxThreads;
	pSMTPCfg->iSessionTimeout = iSessionTimeout;
	pSMTPCfg->iTimeout = STD_SERVER_TIMEOUT;
	pSMTPCfg->iMaxRcpts = iMaxRcpts;
	pSMTPCfg->uPopAuthExpireTime = uPopAuthExpireTime;
	pSMTPCfg->iNumAddr = iNumAddr;

	for (int nn = 0; nn < iNumAddr; nn++)
		pSMTPCfg->SvrAddr[nn] = SvrAddr[nn];

	ShbUnlock(hShbSMTP);

	if ((hSMTPThread = SysCreateThread(SMTPThreadProc, NULL)) == SYS_INVALID_THREAD) {
		ShbCloseBlock(hShbSMTP);
		return (ErrGetErrorCode());
	}

	return (0);

}

static void SvrCleanupSMTP(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop SMTP Thread
///////////////////////////////////////////////////////////////////////////////
	SMTPConfig *pSMTPCfg = (SMTPConfig *) ShbLock(hShbSMTP);

	pSMTPCfg->ulFlags |= SMTPF_STOP_SERVER;

	ShbUnlock(hShbSMTP);

///////////////////////////////////////////////////////////////////////////////
//  Wait SMTP Thread
///////////////////////////////////////////////////////////////////////////////
	SysWaitThread(hSMTPThread, SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close SMTP Thread
///////////////////////////////////////////////////////////////////////////////
	SysCloseThread(hSMTPThread, 1);

	ShbCloseBlock(hShbSMTP);

}

static int SvrSetupSMAIL(int iArgCount, char *pszArgs[])
{

	int ii;
	int iRetryTimeout = STD_SMAIL_RETRY_TIMEOUT;
	int iRetryIncrRatio = STD_SMAIL_RETRY_INCR_RATIO;
	int iMaxRetry = STD_SMAIL_MAX_RETRY;
	unsigned long ulFlags = 0;

	iNumSMAILThreads = STD_SMAIL_THREADS;

	for (ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'Q'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('n'):
			if (++ii < iArgCount)
				iNumSMAILThreads = atoi(pszArgs[ii]);

			iNumSMAILThreads = Min(MAX_SMAIL_THREADS, Max(1, iNumSMAILThreads));
			break;

		case ('t'):
			if (++ii < iArgCount)
				iRetryTimeout = atoi(pszArgs[ii]);
			break;

		case ('i'):
			if (++ii < iArgCount)
				iRetryIncrRatio = atoi(pszArgs[ii]);
			break;

		case ('r'):
			if (++ii < iArgCount)
				iMaxRetry = atoi(pszArgs[ii]);
			break;

		case ('l'):
			ulFlags |= SMAILF_LOG_ENABLED;
			break;

		case ('T'):
			if (++ii < iArgCount)
				iFilterTimeout = atoi(pszArgs[ii]);
			break;

		case ('g'):
			bFilterLogEnabled = true;
			break;
		}
	}

	if ((hShbSMAIL = ShbCreateBlock(sizeof(SMAILConfig))) == SHB_INVALID_HANDLE)
		return (ErrGetErrorCode());

	SMAILConfig *pSMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL);

	if (pSMAILCfg == NULL) {
		ErrorPush();
		ShbCloseBlock(hShbSMAIL);

		return (ErrorPop());
	}

	pSMAILCfg->ulFlags = ulFlags;
	pSMAILCfg->lThreadCount = 0;

	ShbUnlock(hShbSMAIL);

///////////////////////////////////////////////////////////////////////////////
//  Initialize queue fs
///////////////////////////////////////////////////////////////////////////////
	char szSpoolDir[SYS_MAX_PATH] = "";

	SvrGetSpoolDir(szSpoolDir, sizeof(szSpoolDir));

	if ((hSpoolQueue = QueOpen(szSpoolDir, iMaxRetry, iRetryTimeout, iRetryIncrRatio,
				   iQueueSplitLevel)) == INVALID_QUEUE_HANDLE) {
		ErrorPush();
		ShbCloseBlock(hShbSMAIL);

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Create mailer threads
///////////////////////////////////////////////////////////////////////////////
	for (ii = 0; ii < iNumSMAILThreads; ii++)
		hSMAILThreads[ii] = SysCreateThread(SMAILThreadProc, NULL);

	return (0);

}

static void SvrCleanupSMAIL(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop SMAIL Thread ( and unlock threads wait with "SysReleaseSemaphore" )
///////////////////////////////////////////////////////////////////////////////
	SMAILConfig *pSMAILCfg = (SMAILConfig *) ShbLock(hShbSMAIL);

	pSMAILCfg->ulFlags |= SMAILF_STOP_SERVER;

	ShbUnlock(hShbSMAIL);

///////////////////////////////////////////////////////////////////////////////
//  Wait SMAIL Threads
///////////////////////////////////////////////////////////////////////////////
	int tt;

	for (tt = 0; tt < iNumSMAILThreads; tt++)
		SysWaitThread(hSMAILThreads[tt], SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close SMAIL Threads
///////////////////////////////////////////////////////////////////////////////
	for (tt = 0; tt < iNumSMAILThreads; tt++)
		SysCloseThread(hSMAILThreads[tt], 1);

	ShbCloseBlock(hShbSMAIL);

///////////////////////////////////////////////////////////////////////////////
//  Close the mail queue
///////////////////////////////////////////////////////////////////////////////
	QueClose(hSpoolQueue);

}

static int SvrSetupPSYNC(int iArgCount, char *pszArgs[])
{

	int iSyncInterval = STD_PSYNC_INTERVAL;
	int iNumSyncThreads = STD_PSYNC_NUM_THREADS;
	unsigned long ulFlags = 0;

	for (int ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'Y'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('i'):
			if (++ii < iArgCount)
				iSyncInterval = atoi(pszArgs[ii]);
			break;

		case ('t'):
			if (++ii < iArgCount)
				iNumSyncThreads = atoi(pszArgs[ii]);

			iNumSyncThreads = Min(MAX_PSYNC_NUM_THREADS, Max(1, iNumSyncThreads));
			break;

		case ('l'):
			ulFlags |= PSYNCF_LOG_ENABLED;
			break;
		}
	}

	if ((hSyncSem =
	     SysCreateSemaphore(iNumSyncThreads, SYS_DEFAULT_MAXCOUNT)) == SYS_INVALID_SEMAPHORE)
		return (ErrGetErrorCode());

	if ((hShbPSYNC = ShbCreateBlock(sizeof(PSYNCConfig))) == SHB_INVALID_HANDLE) {
		ErrorPush();
		SysCloseSemaphore(hSyncSem);
		return (ErrorPop());
	}

	PSYNCConfig *pPSYNCCfg = (PSYNCConfig *) ShbLock(hShbPSYNC);

	if (pPSYNCCfg == NULL) {
		ErrorPush();
		ShbCloseBlock(hShbPSYNC);
		SysCloseSemaphore(hSyncSem);
		return (ErrorPop());
	}

	pPSYNCCfg->ulFlags = ulFlags;
	pPSYNCCfg->lThreadCount = 0;
	pPSYNCCfg->iTimeout = STD_SERVER_TIMEOUT;
	pPSYNCCfg->iSyncInterval = iSyncInterval;
	pPSYNCCfg->iNumSyncThreads = iNumSyncThreads;

	ShbUnlock(hShbPSYNC);

///////////////////////////////////////////////////////////////////////////////
//  Remove POP3 links lock files
///////////////////////////////////////////////////////////////////////////////
	GwLkClearLinkLocksDir();

	if ((hPSYNCThread = SysCreateThread(PSYNCThreadProc, NULL)) == SYS_INVALID_THREAD) {
		ShbCloseBlock(hShbPSYNC);
		SysCloseSemaphore(hSyncSem);

		return (ErrGetErrorCode());
	}

	return (0);

}

static void SvrCleanupPSYNC(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop PSYNC Thread
///////////////////////////////////////////////////////////////////////////////
	PSYNCConfig *pPSYNCCfg = (PSYNCConfig *) ShbLock(hShbPSYNC);

	pPSYNCCfg->ulFlags |= PSYNCF_STOP_SERVER;

	ShbUnlock(hShbPSYNC);

///////////////////////////////////////////////////////////////////////////////
//  Wait PSYNC Thread
///////////////////////////////////////////////////////////////////////////////
	SysWaitThread(hPSYNCThread, SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close PSYNC Thread
///////////////////////////////////////////////////////////////////////////////
	SysCloseThread(hPSYNCThread, 1);

	ShbCloseBlock(hShbPSYNC);

	SysCloseSemaphore(hSyncSem);

}

static int SvrSetupLMAIL(int iArgCount, char *pszArgs[])
{

	int ii;
	int iSleepTimeout = STD_LMAILTHREAD_SLEEP_TIME;
	unsigned long ulFlags = 0;

	iNumLMAILThreads = STD_LMAIL_THREADS;

	for (ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'L'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('n'):
			if (++ii < iArgCount)
				iNumLMAILThreads = atoi(pszArgs[ii]);

			iNumLMAILThreads = Min(MAX_LMAIL_THREADS, Max(1, iNumLMAILThreads));
			break;

		case ('l'):
			ulFlags |= LMAILF_LOG_ENABLED;
			break;

		case ('t'):
			if (++ii < iArgCount)
				iSleepTimeout = atoi(pszArgs[ii]);
			break;
		}
	}

	if ((hShbLMAIL = ShbCreateBlock(sizeof(LMAILConfig))) == SHB_INVALID_HANDLE)
		return (ErrGetErrorCode());

	LMAILConfig *pLMAILCfg = (LMAILConfig *) ShbLock(hShbLMAIL);

	if (pLMAILCfg == NULL) {
		ErrorPush();
		ShbCloseBlock(hShbLMAIL);

		return (ErrorPop());
	}

	pLMAILCfg->ulFlags = ulFlags;
	pLMAILCfg->iSleepTimeout = iSleepTimeout;
	pLMAILCfg->lNumThreads = iNumLMAILThreads;
	pLMAILCfg->lThreadCount = 0;

	ShbUnlock(hShbLMAIL);

///////////////////////////////////////////////////////////////////////////////
//  Create mailer threads
///////////////////////////////////////////////////////////////////////////////
	for (ii = 0; ii < iNumLMAILThreads; ii++)
		hLMAILThreads[ii] = SysCreateThread(LMAILThreadProc, NULL);

	return (0);

}

static void SvrCleanupLMAIL(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Stop LMAIL Thread ( and unlock threads wait with "SysReleaseSemaphore" )
///////////////////////////////////////////////////////////////////////////////
	LMAILConfig *pLMAILCfg = (LMAILConfig *) ShbLock(hShbLMAIL);

	pLMAILCfg->ulFlags |= LMAILF_STOP_SERVER;

	ShbUnlock(hShbLMAIL);

///////////////////////////////////////////////////////////////////////////////
//  Wait LMAIL Threads
///////////////////////////////////////////////////////////////////////////////
	int tt;

	for (tt = 0; tt < iNumLMAILThreads; tt++)
		SysWaitThread(hLMAILThreads[tt], SVR_EXIT_WAIT);

///////////////////////////////////////////////////////////////////////////////
//  Close LMAIL Threads
///////////////////////////////////////////////////////////////////////////////
	for (tt = 0; tt < iNumLMAILThreads; tt++)
		SysCloseThread(hLMAILThreads[tt], 1);

	ShbCloseBlock(hShbLMAIL);

}

static int SvrSetup(int iArgCount, char *pszArgs[])
{

	StrSNCpy(szMailPath, SYS_BASE_FS_STR);

	char *pszValue = SysGetEnv(ENV_MAIN_PATH);

	if (pszValue != NULL) {
		if (strncmp(szMailPath, pszValue, strlen(szMailPath)) == 0)
			StrSNCpy(szMailPath, pszValue);
		else
			StrSNCat(szMailPath, pszValue);
		DelFinalSlash(szMailPath);

		SysFree(pszValue);
	}

	bServerDebug = false;

	int iSndBufSize = -1;
	int iRcvBufSize = -1;
	int iDnsCacheDirs = DNS_HASH_NUM_DIRS;

	for (int ii = 0; ii < iArgCount; ii++) {
		if ((pszArgs[ii][0] != '-') || (pszArgs[ii][1] != 'M'))
			continue;

		switch (pszArgs[ii][2]) {
		case ('s'):
			if (++ii < iArgCount) {
				StrSNCpy(szMailPath, pszArgs[ii]);
				DelFinalSlash(szMailPath);
			}
			break;

		case ('d'):
			bServerDebug = true;
			break;

		case ('r'):
			if (++ii < iArgCount)
				iLogRotateHours = atoi(pszArgs[ii]);
			break;

		case ('x'):
			if (++ii < iArgCount) {
				iQueueSplitLevel = atoi(pszArgs[ii]);

				while (!IsPrimeNumber(iQueueSplitLevel))
					++iQueueSplitLevel;
			}
			break;

		case ('R'):
			if (++ii < iArgCount) {
				iRcvBufSize = atoi(pszArgs[ii]);
				iRcvBufSize = NbrCeil(iRcvBufSize, 1024);
			}
			break;

		case ('S'):
			if (++ii < iArgCount) {
				iSndBufSize = atoi(pszArgs[ii]);
				iSndBufSize = NbrCeil(iSndBufSize, 1024);
			}
			break;

		case ('M'):
			iMailboxType = XMAIL_MAILDIR;
			break;

		case ('m'):
			iMailboxType = XMAIL_MAILBOX;
			break;

		case ('D'):
			if (++ii < iArgCount)
				iDnsCacheDirs = atoi(pszArgs[ii]);
			break;
		}
	}

	if ((strlen(szMailPath) == 0) || !SysExistDir(szMailPath)) {
		ErrSetErrorCode(ERR_CONF_PATH);
		return (ERR_CONF_PATH);
	}

	AppendSlash(szMailPath);

///////////////////////////////////////////////////////////////////////////////
//  Setup library socket buffers
///////////////////////////////////////////////////////////////////////////////
	SysSetupSocketBuffers((iSndBufSize > 0) ? &iSndBufSize : NULL,
			      (iRcvBufSize > 0) ? &iRcvBufSize : NULL);

///////////////////////////////////////////////////////////////////////////////
//  Setup shutdown file name ( must be called before any shutdown function )
///////////////////////////////////////////////////////////////////////////////
	sprintf(szShutdownFile, "%s%s", szMailPath, SVR_SHUTDOWN_FILE);

///////////////////////////////////////////////////////////////////////////////
//  Setup resource lockers
///////////////////////////////////////////////////////////////////////////////
	if (RLckInitLockers() < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Check dynamic DNS setup
///////////////////////////////////////////////////////////////////////////////
	if (DynDnsSetup() < 0) {
		ErrorPush();
		RLckCleanupLockers();

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Clear shutdown condition
///////////////////////////////////////////////////////////////////////////////
	SvrShutdownCleanup();

///////////////////////////////////////////////////////////////////////////////
//  Align table indexes
///////////////////////////////////////////////////////////////////////////////
	if ((UsrCheckUsersIndexes() < 0) ||
	    (UsrCheckAliasesIndexes() < 0) ||
	    (ExAlCheckAliasIndexes() < 0) ||
	    (MDomCheckDomainsIndexes() < 0) || (ADomCheckDomainsIndexes() < 0)) {
		ErrorPush();
		RLckCleanupLockers();

		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  Initialize DNS cache
///////////////////////////////////////////////////////////////////////////////
	if (CDNS_Initialize(iDnsCacheDirs) < 0) {
		ErrorPush();
		RLckCleanupLockers();

		return (ErrorPop());
	}

	return (0);

}

static void SvrCleanup(void)
{
///////////////////////////////////////////////////////////////////////////////
//  Cleanup resource lockers
///////////////////////////////////////////////////////////////////////////////
	RLckCleanupLockers();

///////////////////////////////////////////////////////////////////////////////
//  Clear shutdown condition
///////////////////////////////////////////////////////////////////////////////
	SvrShutdownCleanup();

}

static void SvrBreakHandler(void)
{

///////////////////////////////////////////////////////////////////////////////
//  Set shutdown condition
///////////////////////////////////////////////////////////////////////////////
	SvrSetShutdown();

}

static char **SvrMergeArgs(int iArgs, char *pszArgs[], int &iArgsCount)
{

	int iCmdArgs = 0;
	char **ppszCmdArgs = NULL;
	char *pszCmdLine = SysGetEnv(ENV_CMD_LINE);

	if (pszCmdLine != NULL) {
		ppszCmdArgs = StrGetArgs(pszCmdLine, iCmdArgs);

		SysFree(pszCmdLine);
	}

	char **ppszMergeArgs = (char **) SysAlloc((iCmdArgs + iArgs + 1) * sizeof(char *));

	if (ppszMergeArgs == NULL) {
		if (ppszCmdArgs != NULL)
			StrFreeStrings(ppszCmdArgs);

		return (NULL);
	}

	iArgsCount = 0;

	for (int ii = 0; ii < iArgs; ii++, iArgsCount++)
		ppszMergeArgs[iArgsCount] = SysStrDup(pszArgs[ii]);

	for (int jj = 0; jj < iCmdArgs; jj++, iArgsCount++)
		ppszMergeArgs[iArgsCount] = SysStrDup(ppszCmdArgs[jj]);

	ppszMergeArgs[iArgsCount] = NULL;

	if (ppszCmdArgs != NULL)
		StrFreeStrings(ppszCmdArgs);

	return (ppszMergeArgs);

}

int SvrMain(int iArgCount, char *pszArgs[])
{

	if (SysInitLibrary() < 0) {
		ErrorPush();
		SysEventLog("%s\n", ErrGetErrorString());
		return (ErrorPop());
	}

	int iMergeArgsCount = 0;
	char **ppszMergeArgs = SvrMergeArgs(iArgCount, pszArgs, iMergeArgsCount);

	if (ppszMergeArgs == NULL) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetup(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		StrFreeStrings(ppszMergeArgs);
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupSMAIL(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupCTRL(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SvrCleanupSMAIL();
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupPOP3(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SvrCleanupCTRL();
		SvrCleanupSMAIL();
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupSMTP(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SvrCleanupPOP3();
		SvrCleanupCTRL();
		SvrCleanupSMAIL();
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupPSYNC(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SvrCleanupSMTP();
		SvrCleanupPOP3();
		SvrCleanupCTRL();
		SvrCleanupSMAIL();
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupFING(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SvrCleanupPSYNC();
		SvrCleanupSMTP();
		SvrCleanupPOP3();
		SvrCleanupCTRL();
		SvrCleanupSMAIL();
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	if (SvrSetupLMAIL(iMergeArgsCount, ppszMergeArgs) < 0) {
		ErrorPush();
		SysLogMessage(LOG_LEV_ERROR, "%s\n", ErrGetErrorString());
		SvrCleanupFING();
		SvrCleanupPSYNC();
		SvrCleanupSMTP();
		SvrCleanupPOP3();
		SvrCleanupCTRL();
		SvrCleanupSMAIL();
		StrFreeStrings(ppszMergeArgs);
		SvrCleanup();
		SysCleanupLibrary();
		return (ErrorPop());
	}

	StrFreeStrings(ppszMergeArgs);

///////////////////////////////////////////////////////////////////////////////
//  Set stop handler
///////////////////////////////////////////////////////////////////////////////
	SysSetBreakHandler(SvrBreakHandler);

///////////////////////////////////////////////////////////////////////////////
//  Server main loop
///////////////////////////////////////////////////////////////////////////////
	for (; !SvrInShutdown(true);) {
		SysSleep(SERVER_SLEEP_TIMESLICE);

	}

///////////////////////////////////////////////////////////////////////////////
//  Goodbye cleanups
///////////////////////////////////////////////////////////////////////////////
	SvrCleanupLMAIL();
	SvrCleanupFING();
	SvrCleanupPSYNC();
	SvrCleanupSMTP();
	SvrCleanupPOP3();
	SvrCleanupCTRL();
	SvrCleanupSMAIL();

	SvrCleanup();

	SysLogMessage(LOG_LEV_MESSAGE, APP_NAME_VERSION_STR " server stopped\n");

	SysCleanupLibrary();

	return (0);

}

int SvrStopServer(bool bWait)
{

///////////////////////////////////////////////////////////////////////////////
//  Set shutdown condition
///////////////////////////////////////////////////////////////////////////////
	SvrSetShutdown();

	if (bWait) {
		int iWaitTime = 0;

		for (; SvrInShutdown(true); iWaitTime += SERVER_SLEEP_TIMESLICE)
			SysSleep(SERVER_SLEEP_TIMESLICE);
	}

	return (0);

}

bool SvrInShutdown(bool bForceCheck)
{

	time_t tNow = time(NULL);
	static time_t tLastCheck = 0;
	static bool bShutdown = false;

	if (bForceCheck || (tNow > (tLastCheck + SHUTDOWN_CHECK_TIME))) {
		tLastCheck = tNow;

		if (bServerShutdown)
			bShutdown = true;
		else if (SysExistFile(szShutdownFile)) {
			bServerShutdown = true;

			SysShutdownLibrary();

			bShutdown = true;
		}
	}

	return (bShutdown);

}
