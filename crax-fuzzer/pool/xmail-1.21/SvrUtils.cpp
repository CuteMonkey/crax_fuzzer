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
#include "ResLocks.h"
#include "StrUtils.h"
#include "SList.h"
#include "BuffSock.h"
#include "MailConfig.h"
#include "UsrUtils.h"
#include "SvrUtils.h"
#include "MessQueue.h"
#include "SMAILUtils.h"
#include "QueueUtils.h"
#include "SMAILSvr.h"
#include "AppDefines.h"
#include "MailSvr.h"
#include "MiscUtils.h"

#define SVR_PROFILE_FILE            "server.tab"
#define MESSAGEID_FILE              "message.id"
#define SMTP_SPOOL_DIR              "spool"
#define MAX_MSG_FILENAME_LENGTH     80
#define SVR_PROFILE_LINE_MAX        2048
#define SYS_RES_CHECK_INTERVAL      8

struct ServerInfoVar {
	LISTLINK LL;
	char *pszName;
	char *pszValue;
};

struct ServerConfigData {
	RLCK_HANDLE hResLock;
	int iWriteLock;
	HSLIST hConfigList;
};

static char *SvrGetProfileFilePath(char *pszFilePath, int iMaxPath);
static ServerInfoVar *SvrAllocVar(const char *pszName, const char *pszValue);
static void SvrFreeVar(ServerInfoVar * pSIV);
static void SvrFreeInfoList(HSLIST & hConfigList);
static ServerInfoVar *SvrGetUserVar(HSLIST & hConfigList, const char *pszName);
static int SvrWriteInfoList(HSLIST & hConfigList, FILE * pProfileFile);
static int SvrLoadServerConfig(HSLIST & hConfigList, const char *pszFilePath);

static char *SvrGetProfileFilePath(char *pszFilePath, int iMaxPath)
{

	CfgGetRootPath(pszFilePath, iMaxPath);

	StrNCat(pszFilePath, SVR_PROFILE_FILE, iMaxPath);

	return (pszFilePath);

}

SVRCFG_HANDLE SvrGetConfigHandle(int iWriteLock)
{

	char szProfilePath[SYS_MAX_PATH] = "";

	SvrGetProfileFilePath(szProfilePath, sizeof(szProfilePath));

///////////////////////////////////////////////////////////////////////////////
//  Lock the profile resource
///////////////////////////////////////////////////////////////////////////////
	RLCK_HANDLE hResLock = INVALID_RLCK_HANDLE;
	char szResLock[SYS_MAX_PATH] = "";

	if (iWriteLock) {

		if ((hResLock = RLckLockEX(CfgGetBasedPath(szProfilePath, szResLock,
							   sizeof(szResLock)))) ==
		    INVALID_RLCK_HANDLE)
			return (INVALID_SVRCFG_HANDLE);

	} else {

		if ((hResLock = RLckLockSH(CfgGetBasedPath(szProfilePath, szResLock,
							   sizeof(szResLock)))) ==
		    INVALID_RLCK_HANDLE)
			return (INVALID_SVRCFG_HANDLE);

	}

	ServerConfigData *pSCD = (ServerConfigData *) SysAlloc(sizeof(ServerConfigData));

	if (pSCD == NULL) {
		if (iWriteLock)
			RLckUnlockEX(hResLock);
		else
			RLckUnlockSH(hResLock);

		return (INVALID_SVRCFG_HANDLE);
	}

	pSCD->hResLock = hResLock;

	pSCD->iWriteLock = iWriteLock;

	ListInit(pSCD->hConfigList);

	if (SvrLoadServerConfig(pSCD->hConfigList, szProfilePath) < 0) {
		if (iWriteLock)
			RLckUnlockEX(hResLock);
		else
			RLckUnlockSH(hResLock);

		SysFree(pSCD);
		return (INVALID_SVRCFG_HANDLE);
	}

	return ((SVRCFG_HANDLE) pSCD);

}

void SvrReleaseConfigHandle(SVRCFG_HANDLE hSvrConfig)
{

	ServerConfigData *pSCD = (ServerConfigData *) hSvrConfig;

///////////////////////////////////////////////////////////////////////////////
//  Unlock the profile resource
///////////////////////////////////////////////////////////////////////////////
	if (pSCD->iWriteLock)
		RLckUnlockEX(pSCD->hResLock);
	else
		RLckUnlockSH(pSCD->hResLock);

	SvrFreeInfoList(pSCD->hConfigList);

	SysFree(pSCD);

}

char *SvrGetConfigVar(SVRCFG_HANDLE hSvrConfig, const char *pszName, const char *pszDefault)
{

	ServerConfigData *pSCD = (ServerConfigData *) hSvrConfig;

	ServerInfoVar *pSIV = SvrGetUserVar(pSCD->hConfigList, pszName);

	if (pSIV != NULL)
		return (SysStrDup(pSIV->pszValue));

	return ((pszDefault != NULL) ? SysStrDup(pszDefault) : NULL);

}

bool SvrTestConfigFlag(char const *pszName, bool bDefault, SVRCFG_HANDLE hSvrConfig)
{

	char szValue[64] = "";

	SvrConfigVar(pszName, szValue, sizeof(szValue) - 1, hSvrConfig, (bDefault) ? "1" : "0");

	return ((atoi(szValue) != 0) ? true : false);

}

int SvrGetConfigInt(char const *pszName, int iDefault, SVRCFG_HANDLE hSvrConfig)
{

	char szValue[64] = "";

	return (((SvrConfigVar(pszName, szValue, sizeof(szValue) - 1, hSvrConfig, NULL) < 0) ||
		 IsEmptyString(szValue)) ? iDefault : atoi(szValue));

}

int SysFlushConfig(SVRCFG_HANDLE hSvrConfig)
{

	ServerConfigData *pSCD = (ServerConfigData *) hSvrConfig;

	if (!pSCD->iWriteLock) {
		ErrSetErrorCode(ERR_SVR_PRFILE_NOT_LOCKED);
		return (ERR_SVR_PRFILE_NOT_LOCKED);
	}

	char szProfilePath[SYS_MAX_PATH] = "";

	SvrGetProfileFilePath(szProfilePath, sizeof(szProfilePath));

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockEX(CfgGetBasedPath(szProfilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	FILE *pProfileFile = fopen(szProfilePath, "wt");

	if (pProfileFile == NULL) {
		RLckUnlockEX(hResLock);
		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	int iFlushResult = SvrWriteInfoList(pSCD->hConfigList, pProfileFile);

	fclose(pProfileFile);

	RLckUnlockEX(hResLock);

	return (iFlushResult);

}

static ServerInfoVar *SvrAllocVar(const char *pszName, const char *pszValue)
{

	ServerInfoVar *pSIV = (ServerInfoVar *) SysAlloc(sizeof(ServerInfoVar));

	if (pSIV == NULL)
		return (NULL);

	ListLinkInit(pSIV);
	pSIV->pszName = SysStrDup(pszName);
	pSIV->pszValue = SysStrDup(pszValue);

	return (pSIV);

}

static void SvrFreeVar(ServerInfoVar * pSIV)
{

	SysFree(pSIV->pszName);
	SysFree(pSIV->pszValue);

	SysFree(pSIV);

}

static void SvrFreeInfoList(HSLIST & hConfigList)
{

	ServerInfoVar *pSIV;

	while ((pSIV = (ServerInfoVar *) ListRemove(hConfigList)) != INVALID_SLIST_PTR)
		SvrFreeVar(pSIV);

}

static ServerInfoVar *SvrGetUserVar(HSLIST & hConfigList, const char *pszName)
{

	ServerInfoVar *pSIV = (ServerInfoVar *) ListFirst(hConfigList);

	for (; pSIV != INVALID_SLIST_PTR; pSIV = (ServerInfoVar *)
	     ListNext(hConfigList, (PLISTLINK) pSIV))
		if (strcmp(pSIV->pszName, pszName) == 0)
			return (pSIV);

	return (NULL);

}

static int SvrWriteInfoList(HSLIST & hConfigList, FILE * pProfileFile)
{

	ServerInfoVar *pSIV = (ServerInfoVar *) ListFirst(hConfigList);

	for (; pSIV != INVALID_SLIST_PTR; pSIV = (ServerInfoVar *)
	     ListNext(hConfigList, (PLISTLINK) pSIV)) {
///////////////////////////////////////////////////////////////////////////////
//  Write variabile name
///////////////////////////////////////////////////////////////////////////////
		char *pszQuoted = StrQuote(pSIV->pszName, '"');

		if (pszQuoted == NULL)
			return (ErrGetErrorCode());

		fprintf(pProfileFile, "%s\t", pszQuoted);

		SysFree(pszQuoted);

///////////////////////////////////////////////////////////////////////////////
//  Write variabile value
///////////////////////////////////////////////////////////////////////////////
		pszQuoted = StrQuote(pSIV->pszValue, '"');

		if (pszQuoted == NULL)
			return (ErrGetErrorCode());

		fprintf(pProfileFile, "%s\n", pszQuoted);

		SysFree(pszQuoted);
	}

	return (0);

}

static int SvrLoadServerConfig(HSLIST & hConfigList, const char *pszFilePath)
{

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockSH(CfgGetBasedPath(pszFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	FILE *pProfileFile = fopen(pszFilePath, "rt");

	if (pProfileFile == NULL) {
		RLckUnlockSH(hResLock);

		ErrSetErrorCode(ERR_NO_USER_PRFILE);
		return (ERR_NO_USER_PRFILE);
	}

	char szProfileLine[SVR_PROFILE_LINE_MAX] = "";

	while (MscGetConfigLine(szProfileLine, sizeof(szProfileLine) - 1, pProfileFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szProfileLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if (iFieldsCount == 2) {
			ServerInfoVar *pSIV = SvrAllocVar(ppszStrings[0], ppszStrings[1]);

			if (pSIV != NULL)
				ListAddTail(hConfigList, (PLISTLINK) pSIV);
		}

		StrFreeStrings(ppszStrings);
	}

	fclose(pProfileFile);

	RLckUnlockSH(hResLock);

	return (0);

}

int SvrGetMessageID(SYS_UINT64 * pullMessageID)
{

	char szMsgIDFile[SYS_MAX_PATH] = "";

	CfgGetRootPath(szMsgIDFile, sizeof(szMsgIDFile));
	StrNCat(szMsgIDFile, MESSAGEID_FILE, sizeof(szMsgIDFile));

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockEX(CfgGetBasedPath(szMsgIDFile, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	FILE *pMsgIDFile = fopen(szMsgIDFile, "r+b");

	if (pMsgIDFile == NULL) {
		if ((pMsgIDFile = fopen(szMsgIDFile, "wb")) == NULL) {
			RLckUnlockEX(hResLock);

			ErrSetErrorCode(ERR_FILE_CREATE, szMsgIDFile);
			return (ERR_FILE_CREATE);
		}
		*pullMessageID = 1;
	} else {
		char szMessageID[128] = "";

		if ((MscGetString(pMsgIDFile, szMessageID, sizeof(szMessageID) - 1) == NULL) ||
		    !isdigit(szMessageID[0])) {
			fclose(pMsgIDFile);
			RLckUnlockEX(hResLock);

			ErrSetErrorCode(ERR_INVALID_FILE, szMsgIDFile);
			return (ERR_INVALID_FILE);
		}
		if (sscanf(szMessageID, SYS_LLU_FMT, pullMessageID) != 1) {
			fclose(pMsgIDFile);
			RLckUnlockEX(hResLock);

			ErrSetErrorCode(ERR_INVALID_FILE, szMsgIDFile);
			return (ERR_INVALID_FILE);
		}
	}

	++*pullMessageID;

	fseek(pMsgIDFile, 0, SEEK_SET);

	fprintf(pMsgIDFile, SYS_LLU_FMT "\r\n", *pullMessageID);

	fclose(pMsgIDFile);

	RLckUnlockEX(hResLock);

	return (0);

}

char *SvrGetLogsDir(char *pszLogsPath, int iMaxPath)
{

	CfgGetRootPath(pszLogsPath, iMaxPath);
	StrNCat(pszLogsPath, SVR_LOGS_DIR, iMaxPath);

	return (pszLogsPath);

}

char *SvrGetSpoolDir(char *pszSpoolPath, int iMaxPath)
{

	CfgGetRootPath(pszSpoolPath, iMaxPath);
	StrNCat(pszSpoolPath, SMTP_SPOOL_DIR, iMaxPath);

	return (pszSpoolPath);

}

int SvrConfigVar(char const *pszVarName, char *pszVarValue, int iMaxVarValue,
		 SVRCFG_HANDLE hSvrConfig, char const *pszDefault)
{

	int iReleaseConfig = 0;

	if (hSvrConfig == INVALID_SVRCFG_HANDLE) {
		if ((hSvrConfig = SvrGetConfigHandle()) == INVALID_SVRCFG_HANDLE)
			return (ErrGetErrorCode());

		++iReleaseConfig;
	}

	char *pszValue = SvrGetConfigVar(hSvrConfig, pszVarName, pszDefault);

	if (pszValue == NULL) {
		if (iReleaseConfig)
			SvrReleaseConfigHandle(hSvrConfig);

		ErrSetErrorCode(ERR_CFG_VAR_NOT_FOUND);
		return (ERR_CFG_VAR_NOT_FOUND);
	}

	strncpy(pszVarValue, pszValue, iMaxVarValue - 1);
	pszVarValue[iMaxVarValue - 1] = '\0';

	SysFree(pszValue);

	if (iReleaseConfig)
		SvrReleaseConfigHandle(hSvrConfig);

	return (0);

}

int SvrCheckDiskSpace(unsigned long ulMinSpace)
{

	time_t tNow = time(NULL);
	static SYS_INT64 FreeSpace = 0;
	static time_t tLastCheck = 0;

	if (tNow > (tLastCheck + SYS_RES_CHECK_INTERVAL)) {
		SYS_INT64 TotalSpace;
		char szRootDir[SYS_MAX_PATH] = "";

		tLastCheck = tNow;

		CfgGetRootPath(szRootDir, sizeof(szRootDir));

		if (SysGetDiskSpace(szRootDir, &TotalSpace, &FreeSpace) < 0)
			return (ErrGetErrorCode());

	}

	if (FreeSpace < (SYS_INT64) ulMinSpace) {
		ErrSetErrorCode(ERR_LOW_DISK_SPACE);
		return (ERR_LOW_DISK_SPACE);
	}

	return (0);

}

int SvrCheckVirtMemSpace(unsigned long ulMinSpace)
{

	time_t tNow = time(NULL);
	static SYS_INT64 FreeSpace = 0;
	static time_t tLastCheck = 0;

	if (tNow > (tLastCheck + SYS_RES_CHECK_INTERVAL)) {
		tLastCheck = tNow;

		SYS_INT64 RamTotal;
		SYS_INT64 RamFree;
		SYS_INT64 VirtTotal;

		if (SysMemoryInfo(&RamTotal, &RamFree, &VirtTotal, &FreeSpace) < 0)
			return (ErrGetErrorCode());

	}

	if (FreeSpace < (SYS_INT64) ulMinSpace) {
		ErrSetErrorCode(ERR_LOW_VM_SPACE);
		return (ERR_LOW_VM_SPACE);
	}

	return (0);

}
