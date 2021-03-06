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
#include "MessQueue.h"
#include "MailSvr.h"
#include "MiscUtils.h"
#include "SvrUtils.h"
#include "UsrUtils.h"
#include "POP3GwLink.h"

#define SVR_LINKS_FILE              "pop3links.tab"
#define SVR_LINKS_ENABLE_DIR        "pop3links"
#define SVR_POP3LOCKS_DIR           "pop3linklocks"
#define LINKS_TABLE_LINE_MAX        2048

enum LinksFileds {
	lnkDomain = 0,
	lnkName,
	lnkRmtDomain,
	lnkRmtName,
	lnkRmtPassword,
	lnkAuthType,

	lnkMax
};

struct GwLkDBScanData {
	char szTmpDBFile[SYS_MAX_PATH];
	FILE *pDBFile;
};

static char *GwLkGetTableFilePath(char *pszLnkFilePath, int iMaxPath);
static char *GwLkEnableDir(char *pszEnableDir, int iMaxPath);
static char *GwLkGetLocksDir(char *pszLocksDir, int iMaxPath);
static POP3Link *GwLkGetLinkFromStrings(char **ppszStrings);
static int GwLkWriteLink(FILE * pLnkFile, POP3Link * pPopLnk);
static char *GwLkSanitizeFileName(char const *pszInName, char *pszOutName, int iOutSize);
static char *GwLkGetLockFileName(POP3Link const *pPopLnk, char *pszLockFile);
static int GwLkGetDisableFilePath(POP3Link const *pPopLnk, char *pszEnableFile);

static char *GwLkGetTableFilePath(char *pszLnkFilePath, int iMaxPath)
{

	CfgGetRootPath(pszLnkFilePath, iMaxPath);

	StrNCat(pszLnkFilePath, SVR_LINKS_FILE, iMaxPath);

	return (pszLnkFilePath);

}

static char *GwLkEnableDir(char *pszEnableDir, int iMaxPath)
{

	CfgGetRootPath(pszEnableDir, iMaxPath);

	StrNCat(pszEnableDir, SVR_LINKS_ENABLE_DIR, iMaxPath);

	return (pszEnableDir);

}

static char *GwLkGetLocksDir(char *pszLocksDir, int iMaxPath)
{

	CfgGetRootPath(pszLocksDir, iMaxPath);

	StrNCat(pszLocksDir, SVR_POP3LOCKS_DIR, iMaxPath);

	return (pszLocksDir);

}

static POP3Link *GwLkGetLinkFromStrings(char **ppszStrings)
{

	int iFieldsCount = StrStringsCount(ppszStrings);

	if (iFieldsCount < lnkMax)
		return (NULL);

	char szPassword[512] = "";

	if (StrDeCrypt(ppszStrings[lnkRmtPassword], szPassword) == NULL)
		return (NULL);

	POP3Link *pPopLnk = (POP3Link *) SysAlloc(sizeof(POP3Link));

	if (pPopLnk == NULL)
		return (NULL);

	pPopLnk->pszDomain = SysStrDup(ppszStrings[lnkDomain]);
	pPopLnk->pszName = SysStrDup(ppszStrings[lnkName]);
	pPopLnk->pszRmtDomain = SysStrDup(ppszStrings[lnkRmtDomain]);
	pPopLnk->pszRmtName = SysStrDup(ppszStrings[lnkRmtName]);
	pPopLnk->pszRmtPassword = SysStrDup(szPassword);
	pPopLnk->pszAuthType = SysStrDup(ppszStrings[lnkAuthType]);

	return (pPopLnk);

}

POP3Link *GwLkAllocLink(char const *pszDomain, char const *pszName,
			char const *pszRmtDomain, char const *pszRmtName,
			char const *pszRmtPassword, char const *pszAuthType)
{

	POP3Link *pPopLnk = (POP3Link *) SysAlloc(sizeof(POP3Link));

	if (pPopLnk == NULL)
		return (NULL);

	pPopLnk->pszDomain = (pszDomain != NULL) ? SysStrDup(pszDomain) : NULL;
	pPopLnk->pszName = (pszName != NULL) ? SysStrDup(pszName) : NULL;
	pPopLnk->pszRmtDomain = (pszRmtDomain != NULL) ? SysStrDup(pszRmtDomain) : NULL;
	pPopLnk->pszRmtName = (pszRmtName != NULL) ? SysStrDup(pszRmtName) : NULL;
	pPopLnk->pszRmtPassword = (pszRmtPassword != NULL) ? SysStrDup(pszRmtPassword) : NULL;
	pPopLnk->pszAuthType = (pszAuthType != NULL) ? SysStrDup(pszAuthType) : NULL;

	return (pPopLnk);

}

void GwLkFreePOP3Link(POP3Link * pPopLnk)
{

	if (pPopLnk->pszDomain != NULL)
		SysFree(pPopLnk->pszDomain);

	if (pPopLnk->pszName != NULL)
		SysFree(pPopLnk->pszName);

	if (pPopLnk->pszRmtDomain != NULL)
		SysFree(pPopLnk->pszRmtDomain);

	if (pPopLnk->pszRmtName != NULL)
		SysFree(pPopLnk->pszRmtName);

	if (pPopLnk->pszRmtPassword != NULL)
		SysFree(pPopLnk->pszRmtPassword);

	if (pPopLnk->pszAuthType != NULL)
		SysFree(pPopLnk->pszAuthType);

	SysFree(pPopLnk);

}

static int GwLkWriteLink(FILE * pLnkFile, POP3Link * pPopLnk)
{

///////////////////////////////////////////////////////////////////////////////
//  Domain
///////////////////////////////////////////////////////////////////////////////
	char *pszQuoted = StrQuote(pPopLnk->pszDomain, '"');

	if (pszQuoted == NULL)
		return (ErrGetErrorCode());

	fprintf(pLnkFile, "%s\t", pszQuoted);

	SysFree(pszQuoted);

///////////////////////////////////////////////////////////////////////////////
//  Local user
///////////////////////////////////////////////////////////////////////////////
	pszQuoted = StrQuote(pPopLnk->pszName, '"');

	if (pszQuoted == NULL)
		return (ErrGetErrorCode());

	fprintf(pLnkFile, "%s\t", pszQuoted);

	SysFree(pszQuoted);

///////////////////////////////////////////////////////////////////////////////
//  Remote domain
///////////////////////////////////////////////////////////////////////////////
	pszQuoted = StrQuote(pPopLnk->pszRmtDomain, '"');

	if (pszQuoted == NULL)
		return (ErrGetErrorCode());

	fprintf(pLnkFile, "%s\t", pszQuoted);

	SysFree(pszQuoted);

///////////////////////////////////////////////////////////////////////////////
//  Remote user
///////////////////////////////////////////////////////////////////////////////
	pszQuoted = StrQuote(pPopLnk->pszRmtName, '"');

	if (pszQuoted == NULL)
		return (ErrGetErrorCode());

	fprintf(pLnkFile, "%s\t", pszQuoted);

	SysFree(pszQuoted);

///////////////////////////////////////////////////////////////////////////////
//  Remote password
///////////////////////////////////////////////////////////////////////////////
	char szPassword[512] = "";

	StrCrypt(pPopLnk->pszRmtPassword, szPassword);

	pszQuoted = StrQuote(szPassword, '"');

	if (pszQuoted == NULL)
		return (ErrGetErrorCode());

	fprintf(pLnkFile, "%s\t", pszQuoted);

	SysFree(pszQuoted);

///////////////////////////////////////////////////////////////////////////////
//  Authentication type
///////////////////////////////////////////////////////////////////////////////
	pszQuoted = StrQuote(pPopLnk->pszAuthType, '"');

	if (pszQuoted == NULL)
		return (ErrGetErrorCode());

	fprintf(pLnkFile, "%s\n", pszQuoted);

	SysFree(pszQuoted);

	return (0);

}

int GwLkAddLink(POP3Link * pPopLnk)
{

	char szLnkFilePath[SYS_MAX_PATH] = "";

	GwLkGetTableFilePath(szLnkFilePath, sizeof(szLnkFilePath));

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockEX(CfgGetBasedPath(szLnkFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	FILE *pLnkFile = fopen(szLnkFilePath, "r+t");

	if (pLnkFile == NULL) {
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_LINKS_FILE_NOT_FOUND);
		return (ERR_LINKS_FILE_NOT_FOUND);
	}

	char szLnkLine[LINKS_TABLE_LINE_MAX] = "";

	while (MscGetConfigLine(szLnkLine, sizeof(szLnkLine) - 1, pLnkFile) != NULL) {
		char **ppszStrings = StrGetTabLineStrings(szLnkLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= lnkMax) &&
		    (stricmp(pPopLnk->pszDomain, ppszStrings[lnkDomain]) == 0) &&
		    (stricmp(pPopLnk->pszName, ppszStrings[lnkName]) == 0) &&
		    (stricmp(pPopLnk->pszRmtDomain, ppszStrings[lnkRmtDomain]) == 0) &&
		    (stricmp(pPopLnk->pszRmtName, ppszStrings[lnkRmtName]) == 0)) {
			StrFreeStrings(ppszStrings);
			fclose(pLnkFile);
			RLckUnlockEX(hResLock);

			ErrSetErrorCode(ERR_LINK_EXIST);
			return (ERR_LINK_EXIST);
		}

		StrFreeStrings(ppszStrings);
	}

	fseek(pLnkFile, 0, SEEK_END);

	if (GwLkWriteLink(pLnkFile, pPopLnk) < 0) {
		fclose(pLnkFile);
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_WRITE_LINKS_FILE);
		return (ERR_WRITE_LINKS_FILE);
	}

	fclose(pLnkFile);

	RLckUnlockEX(hResLock);

	return (0);

}

int GwLkRemoveLink(POP3Link * pPopLnk)
{

	char szLnkFilePath[SYS_MAX_PATH] = "";

	GwLkGetTableFilePath(szLnkFilePath, sizeof(szLnkFilePath));

	char szTmpFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szTmpFile);

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockEX(CfgGetBasedPath(szLnkFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	FILE *pLnkFile = fopen(szLnkFilePath, "rt");

	if (pLnkFile == NULL) {
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_LINKS_FILE_NOT_FOUND);
		return (ERR_LINKS_FILE_NOT_FOUND);
	}

	FILE *pTmpFile = fopen(szTmpFile, "wt");

	if (pTmpFile == NULL) {
		fclose(pLnkFile);
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	int iLinksFound = 0;
	char szLnkLine[LINKS_TABLE_LINE_MAX] = "";

	while (MscGetConfigLine(szLnkLine, sizeof(szLnkLine) - 1, pLnkFile, false) != NULL) {
		if (szLnkLine[0] == TAB_COMMENT_CHAR) {
			fprintf(pTmpFile, "%s\n", szLnkLine);
			continue;
		}

		char **ppszStrings = StrGetTabLineStrings(szLnkLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= lnkMax) &&
		    (stricmp(pPopLnk->pszDomain, ppszStrings[lnkDomain]) == 0) &&
		    (stricmp(pPopLnk->pszName, ppszStrings[lnkName]) == 0) &&
		    (stricmp(pPopLnk->pszRmtDomain, ppszStrings[lnkRmtDomain]) == 0) &&
		    (stricmp(pPopLnk->pszRmtName, ppszStrings[lnkRmtName]) == 0)) {

			++iLinksFound;

		} else
			fprintf(pTmpFile, "%s\n", szLnkLine);

		StrFreeStrings(ppszStrings);
	}

	fclose(pLnkFile);
	fclose(pTmpFile);

	if (iLinksFound == 0) {
		SysRemove(szTmpFile);
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_LINK_NOT_FOUND);
		return (ERR_LINK_NOT_FOUND);
	}

	char szTmpLnkFilePath[SYS_MAX_PATH] = "";

	sprintf(szTmpLnkFilePath, "%s.tmp", szLnkFilePath);

	if (MscMoveFile(szLnkFilePath, szTmpLnkFilePath) < 0) {
		ErrorPush();
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	if (MscMoveFile(szTmpFile, szLnkFilePath) < 0) {
		ErrorPush();
		MscMoveFile(szTmpLnkFilePath, szLnkFilePath);
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	SysRemove(szTmpLnkFilePath);

///////////////////////////////////////////////////////////////////////////////
//  Remove the disable file if exist
///////////////////////////////////////////////////////////////////////////////
	char szEnableFile[SYS_MAX_PATH] = "";

	if (GwLkGetDisableFilePath(pPopLnk, szEnableFile) < 0) {
		ErrorPush();
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	CheckRemoveFile(szEnableFile);

	RLckUnlockEX(hResLock);

	return (0);

}

int GwLkRemoveUserLinks(const char *pszDomain, const char *pszName)
{

	char szLnkFilePath[SYS_MAX_PATH] = "";

	GwLkGetTableFilePath(szLnkFilePath, sizeof(szLnkFilePath));

	char szTmpFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szTmpFile);

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockEX(CfgGetBasedPath(szLnkFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE) {
		ErrorPush();
		CheckRemoveFile(szTmpFile);
		return (ErrorPop());
	}

	FILE *pLnkFile = fopen(szLnkFilePath, "rt");

	if (pLnkFile == NULL) {
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_LINKS_FILE_NOT_FOUND);
		return (ERR_LINKS_FILE_NOT_FOUND);
	}

	FILE *pTmpFile = fopen(szTmpFile, "wt");

	if (pTmpFile == NULL) {
		fclose(pLnkFile);
		RLckUnlockEX(hResLock);

		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	int iLinksFound = 0;
	char szLnkLine[LINKS_TABLE_LINE_MAX] = "";

	while (MscGetConfigLine(szLnkLine, sizeof(szLnkLine) - 1, pLnkFile, false) != NULL) {
		if (szLnkLine[0] == TAB_COMMENT_CHAR) {
			fprintf(pTmpFile, "%s\n", szLnkLine);
			continue;
		}

		char **ppszStrings = StrGetTabLineStrings(szLnkLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= lnkMax) && (stricmp(pszDomain, ppszStrings[lnkDomain]) == 0)
		    && (stricmp(pszName, ppszStrings[lnkName]) == 0)) {

			++iLinksFound;

		} else
			fprintf(pTmpFile, "%s\n", szLnkLine);

		StrFreeStrings(ppszStrings);
	}

	fclose(pLnkFile);
	fclose(pTmpFile);

	if (iLinksFound == 0) {
		SysRemove(szTmpFile);
		RLckUnlockEX(hResLock);
		return (0);
	}

	char szTmpLnkFilePath[SYS_MAX_PATH] = "";

	sprintf(szTmpLnkFilePath, "%s.tmp", szLnkFilePath);

	if (MscMoveFile(szLnkFilePath, szTmpLnkFilePath) < 0) {
		ErrorPush();
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	if (MscMoveFile(szTmpFile, szLnkFilePath) < 0) {
		ErrorPush();
		MscMoveFile(szTmpLnkFilePath, szLnkFilePath);
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	SysRemove(szTmpLnkFilePath);

	RLckUnlockEX(hResLock);

	return (0);

}

int GwLkRemoveDomainLinks(const char *pszDomain)
{

	char szLnkFilePath[SYS_MAX_PATH] = "";

	GwLkGetTableFilePath(szLnkFilePath, sizeof(szLnkFilePath));

	char szTmpFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szTmpFile);

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockEX(CfgGetBasedPath(szLnkFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE) {
		ErrorPush();
		CheckRemoveFile(szTmpFile);
		return (ErrorPop());
	}

	FILE *pLnkFile = fopen(szLnkFilePath, "rt");

	if (pLnkFile == NULL) {
		RLckUnlockEX(hResLock);
		CheckRemoveFile(szTmpFile);

		ErrSetErrorCode(ERR_LINKS_FILE_NOT_FOUND);
		return (ERR_LINKS_FILE_NOT_FOUND);
	}

	FILE *pTmpFile = fopen(szTmpFile, "wt");

	if (pTmpFile == NULL) {
		fclose(pLnkFile);
		RLckUnlockEX(hResLock);
		CheckRemoveFile(szTmpFile);

		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	int iLinksFound = 0;
	char szLnkLine[LINKS_TABLE_LINE_MAX] = "";

	while (MscGetConfigLine(szLnkLine, sizeof(szLnkLine) - 1, pLnkFile, false) != NULL) {
		if (szLnkLine[0] == TAB_COMMENT_CHAR) {
			fprintf(pTmpFile, "%s\n", szLnkLine);
			continue;
		}

		char **ppszStrings = StrGetTabLineStrings(szLnkLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if ((iFieldsCount >= lnkMax) && (stricmp(pszDomain, ppszStrings[lnkDomain]) == 0)) {

			++iLinksFound;

		} else
			fprintf(pTmpFile, "%s\n", szLnkLine);

		StrFreeStrings(ppszStrings);
	}

	fclose(pLnkFile);
	fclose(pTmpFile);

	if (iLinksFound == 0) {
		SysRemove(szTmpFile);
		RLckUnlockEX(hResLock);
		return (0);
	}

	char szTmpLnkFilePath[SYS_MAX_PATH] = "";

	sprintf(szTmpLnkFilePath, "%s.tmp", szLnkFilePath);

	if (MscMoveFile(szLnkFilePath, szTmpLnkFilePath) < 0) {
		ErrorPush();
		SysRemove(szTmpFile);
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	if (MscMoveFile(szTmpFile, szLnkFilePath) < 0) {
		ErrorPush();
		MscMoveFile(szTmpLnkFilePath, szLnkFilePath);
		SysRemove(szTmpFile);
		RLckUnlockEX(hResLock);
		return (ErrorPop());
	}

	SysRemove(szTmpLnkFilePath);

	RLckUnlockEX(hResLock);

	return (0);

}

int GwLkGetDBFileSnapShot(const char *pszFileName)
{

	char szGwLkFilePath[SYS_MAX_PATH] = "";

	GwLkGetTableFilePath(szGwLkFilePath, sizeof(szGwLkFilePath));

	char szResLock[SYS_MAX_PATH] = "";
	RLCK_HANDLE hResLock = RLckLockSH(CfgGetBasedPath(szGwLkFilePath, szResLock,
							  sizeof(szResLock)));

	if (hResLock == INVALID_RLCK_HANDLE)
		return (ErrGetErrorCode());

	if (MscCopyFile(pszFileName, szGwLkFilePath) < 0) {
		ErrorPush();
		RLckUnlockSH(hResLock);
		return (ErrorPop());
	}

	RLckUnlockSH(hResLock);

	return (0);

}

GWLKF_HANDLE GwLkOpenDB(void)
{

	GwLkDBScanData *pGLSD = (GwLkDBScanData *) SysAlloc(sizeof(GwLkDBScanData));

	if (pGLSD == NULL)
		return (INVALID_GWLKF_HANDLE);

	SysGetTmpFile(pGLSD->szTmpDBFile);

	if (GwLkGetDBFileSnapShot(pGLSD->szTmpDBFile) < 0) {
		SysFree(pGLSD);
		return (INVALID_GWLKF_HANDLE);
	}

	if ((pGLSD->pDBFile = fopen(pGLSD->szTmpDBFile, "rt")) == NULL) {
		SysRemove(pGLSD->szTmpDBFile);
		SysFree(pGLSD);

		ErrSetErrorCode(ERR_FILE_OPEN);
		return (INVALID_GWLKF_HANDLE);
	}

	return ((GWLKF_HANDLE) pGLSD);

}

void GwLkCloseDB(GWLKF_HANDLE hLinksDB)
{

	GwLkDBScanData *pGLSD = (GwLkDBScanData *) hLinksDB;

	fclose(pGLSD->pDBFile);

	SysRemove(pGLSD->szTmpDBFile);

	SysFree(pGLSD);

}

POP3Link *GwLkGetFirstUser(GWLKF_HANDLE hLinksDB)
{

	GwLkDBScanData *pGLSD = (GwLkDBScanData *) hLinksDB;

	rewind(pGLSD->pDBFile);

	POP3Link *pPopLnk = NULL;
	char szLnkLine[LINKS_TABLE_LINE_MAX] = "";

	while ((pPopLnk == NULL) &&
	       (MscGetConfigLine(szLnkLine, sizeof(szLnkLine) - 1, pGLSD->pDBFile) != NULL)) {
		char **ppszStrings = StrGetTabLineStrings(szLnkLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if (iFieldsCount >= lnkMax)
			pPopLnk = GwLkGetLinkFromStrings(ppszStrings);

		StrFreeStrings(ppszStrings);
	}

	return (pPopLnk);

}

POP3Link *GwLkGetNextUser(GWLKF_HANDLE hLinksDB)
{

	GwLkDBScanData *pGLSD = (GwLkDBScanData *) hLinksDB;

	POP3Link *pPopLnk = NULL;
	char szLnkLine[LINKS_TABLE_LINE_MAX] = "";

	while ((pPopLnk == NULL) &&
	       (MscGetConfigLine(szLnkLine, sizeof(szLnkLine) - 1, pGLSD->pDBFile) != NULL)) {
		char **ppszStrings = StrGetTabLineStrings(szLnkLine);

		if (ppszStrings == NULL)
			continue;

		int iFieldsCount = StrStringsCount(ppszStrings);

		if (iFieldsCount >= lnkMax)
			pPopLnk = GwLkGetLinkFromStrings(ppszStrings);

		StrFreeStrings(ppszStrings);
	}

	return (pPopLnk);

}

static char *GwLkSanitizeFileName(char const *pszInName, char *pszOutName, int iOutSize)
{

	int i;

	for (i = 0, iOutSize--; i < iOutSize && *pszInName != '\0'; i++, pszInName++) {
		switch (*pszInName) {
		case ':':
		case '/':
		case '\\':
			pszOutName[i] = '_';
			break;
		default:
			pszOutName[i] = *pszInName;
		}
	}
	pszOutName[i] = '\0';

	return (pszOutName);
}

static char *GwLkGetLockFileName(POP3Link const *pPopLnk, char *pszLockFile)
{

	char szLocksDir[SYS_MAX_PATH] = "";
	char szRmtDomain[MAX_HOST_NAME] = "";
	char szRmtName[MAX_HOST_NAME] = "";

	GwLkGetLocksDir(szLocksDir, sizeof(szLocksDir));

	GwLkSanitizeFileName(pPopLnk->pszRmtDomain, szRmtDomain, sizeof(szRmtDomain) - 1);
	GwLkSanitizeFileName(pPopLnk->pszRmtName, szRmtName, sizeof(szRmtName) - 1);

	sprintf(pszLockFile, "%s%s%s@%s", szLocksDir, SYS_SLASH_STR, szRmtName,
		szRmtDomain);

	return (pszLockFile);

}

int GwLkLinkLock(POP3Link const *pPopLnk)
{

	char szLockFile[SYS_MAX_PATH] = "";

	GwLkGetLockFileName(pPopLnk, szLockFile);

	return (SysLockFile(szLockFile));

}

void GwLkLinkUnlock(POP3Link const *pPopLnk)
{

	char szLockFile[SYS_MAX_PATH] = "";

	GwLkGetLockFileName(pPopLnk, szLockFile);

	SysUnlockFile(szLockFile);

}

int GwLkClearLinkLocksDir(void)
{

	char szLocksDir[SYS_MAX_PATH] = "";

	GwLkGetLocksDir(szLocksDir, sizeof(szLocksDir));

	return (MscClearDirectory(szLocksDir));

}

int GwLkLocalDomain(POP3Link const *pPopLnk)
{

	return (((pPopLnk != NULL) && (pPopLnk->pszDomain[0] != '@') &&
		 (pPopLnk->pszDomain[0] != '?') && (pPopLnk->pszDomain[0] != '&')) ? 1 : 0);

}

int GwLkMasqueradeDomain(POP3Link const *pPopLnk)
{

	return (((pPopLnk != NULL) &&
		 ((pPopLnk->pszDomain[0] == '?') || (pPopLnk->pszDomain[0] == '&'))) ? 1 : 0);

}

static int GwLkGetDisableFilePath(POP3Link const *pPopLnk, char *pszEnableFile)
{

	if (GwLkLocalDomain(pPopLnk)) {
		UserInfo *pUI = UsrGetUserByName(pPopLnk->pszDomain, pPopLnk->pszName);

		if (pUI == NULL)
			return (ErrGetErrorCode());

		char szUserPath[SYS_MAX_PATH] = "";

		if (UsrGetUserPath(pUI, szUserPath, sizeof(szUserPath), 1) == NULL) {
			ErrorPush();
			UsrFreeUserInfo(pUI);
			return (ErrorPop());
		}

		UsrFreeUserInfo(pUI);

		sprintf(pszEnableFile, "%s%s@%s.disabled", szUserPath, pPopLnk->pszRmtName,
			pPopLnk->pszRmtDomain);
	} else {
		char szEnableDir[SYS_MAX_PATH] = "";

		GwLkEnableDir(szEnableDir, sizeof(szEnableDir));

		sprintf(pszEnableFile, "%s%s%s@%s.disabled", szEnableDir, SYS_SLASH_STR,
			pPopLnk->pszRmtName, pPopLnk->pszRmtDomain);
	}

	return (0);

}

int GwLkCheckEnabled(POP3Link const *pPopLnk)
{

	char szEnableFile[SYS_MAX_PATH] = "";

	if (GwLkGetDisableFilePath(pPopLnk, szEnableFile) < 0)
		return (ErrGetErrorCode());

	if (SysExistFile(szEnableFile)) {
		ErrSetErrorCode(ERR_POP3_EXTERNAL_LINK_DISABLED);
		return (ERR_POP3_EXTERNAL_LINK_DISABLED);
	}

	return (0);

}

int GwLkEnable(POP3Link const *pPopLnk, bool bEnable)
{

	char szEnableFile[SYS_MAX_PATH] = "";

	if (GwLkGetDisableFilePath(pPopLnk, szEnableFile) < 0)
		return (ErrGetErrorCode());

	if (bEnable)
		CheckRemoveFile(szEnableFile);
	else {
		FILE *pFile = fopen(szEnableFile, "wt");

		if (pFile == NULL) {
			ErrSetErrorCode(ERR_FILE_CREATE);
			return (ERR_FILE_CREATE);
		}

		time_t tCurr;

		time(&tCurr);

		struct tm tmTime;
		char szTime[256] = "";

		SysLocalTime(&tCurr, &tmTime);

		strftime(szTime, sizeof(szTime), "%d %b %Y %H:%M:%S %Z", &tmTime);

		fprintf(pFile, "%s\n", szTime);

		fclose(pFile);
	}

	return (0);

}

int GwLkEnable(char const *pszDomain, char const *pszName,
	       char const *pszRmtDomain, char const *pszRmtName, bool bEnable)
{

	GWLKF_HANDLE hLinksDB = GwLkOpenDB();

	if (hLinksDB == INVALID_GWLKF_HANDLE)
		return (ErrGetErrorCode());

	int iMatchingUsers = 0;
	POP3Link *pPopLnk = GwLkGetFirstUser(hLinksDB);

	for (; pPopLnk != NULL; pPopLnk = GwLkGetNextUser(hLinksDB)) {
		if ((stricmp(pPopLnk->pszDomain, pszDomain) == 0) &&
		    (stricmp(pPopLnk->pszName, pszName) == 0) &&
		    ((pszRmtDomain == NULL) ||
		     (stricmp(pPopLnk->pszRmtDomain, pszRmtDomain) == 0)) && ((pszRmtName == NULL)
									      ||
									      (stricmp
									       (pPopLnk->
										pszRmtName,
										pszRmtName) ==
									       0))) {
			GwLkEnable(pPopLnk, bEnable);

			++iMatchingUsers;
		}

		GwLkFreePOP3Link(pPopLnk);
	}

	GwLkCloseDB(hLinksDB);

	if (iMatchingUsers == 0) {
		ErrSetErrorCode(ERR_LINK_NOT_FOUND);
		return (ERR_LINK_NOT_FOUND);
	}

	return (0);

}
