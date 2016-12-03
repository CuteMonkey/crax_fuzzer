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
#include "MD5.h"
#include "Base64Enc.h"
#include "BuffSock.h"
#include "MailConfig.h"
#include "UsrUtils.h"
#include "SvrUtils.h"
#include "MessQueue.h"
#include "MailSvr.h"
#include "MiscUtils.h"
#include "Maildir.h"

int MdirCreateStructure(char const *pszBasePath)
{
///////////////////////////////////////////////////////////////////////////////
//  Create Maildir directory
///////////////////////////////////////////////////////////////////////////////
	char szMaildirPath[SYS_MAX_PATH] = "";

	StrSNCpy(szMaildirPath, pszBasePath);

	AppendSlash(szMaildirPath);
	StrSNCat(szMaildirPath, MAILDIR_DIRECTORY);

	if (SysMakeDir(szMaildirPath) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Create Maildir/tmp directory
///////////////////////////////////////////////////////////////////////////////
	char szSubPath[SYS_MAX_PATH] = "";

	sprintf(szSubPath, "%s" SYS_SLASH_STR "tmp", szMaildirPath);

	if (SysMakeDir(szSubPath) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Create Maildir/new directory
///////////////////////////////////////////////////////////////////////////////
	sprintf(szSubPath, "%s" SYS_SLASH_STR "new", szMaildirPath);

	if (SysMakeDir(szSubPath) < 0)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Create Maildir/cur directory
///////////////////////////////////////////////////////////////////////////////
	sprintf(szSubPath, "%s" SYS_SLASH_STR "cur", szMaildirPath);

	if (SysMakeDir(szSubPath) < 0)
		return (ErrGetErrorCode());

	return (0);

}

int MdirGetTmpMaildirEntry(char const *pszMaildirPath, char *pszFilePath)
{

	char szTmpPath[SYS_MAX_PATH] = "";

	sprintf(szTmpPath, "%s" SYS_SLASH_STR "tmp", pszMaildirPath);

	return (MscUniqueFile(szTmpPath, pszFilePath));

}

int MdirMoveTmpEntryInNew(char const *pszTmpEntryPath)
{
///////////////////////////////////////////////////////////////////////////////
//  Lookup Maildir/tmp/ subpath
///////////////////////////////////////////////////////////////////////////////
	char const *pszTmpDir = MAILDIR_DIRECTORY SYS_SLASH_STR "tmp" SYS_SLASH_STR;
	char const *pszLookup = strstr(pszTmpEntryPath, pszTmpDir);

	if (pszLookup == NULL) {
		ErrSetErrorCode(ERR_INVALID_MAILDIR_SUBPATH);
		return (ERR_INVALID_MAILDIR_SUBPATH);
	}
///////////////////////////////////////////////////////////////////////////////
//  Build Maildir/new file path
///////////////////////////////////////////////////////////////////////////////
	int iBaseLength = (int) (pszLookup - pszTmpEntryPath);
	char const *pszNewDir = MAILDIR_DIRECTORY SYS_SLASH_STR "new" SYS_SLASH_STR;
	char const *pszSlash = strrchr(pszTmpEntryPath, SYS_SLASH_CHAR);
	char szNewEntryPath[SYS_MAX_PATH] = "";

	StrSNCpy(szNewEntryPath, pszTmpEntryPath);
	StrNCpy(szNewEntryPath + iBaseLength, pszNewDir, sizeof(szNewEntryPath) - iBaseLength);
	StrNCat(szNewEntryPath, pszSlash + 1, sizeof(szNewEntryPath));

///////////////////////////////////////////////////////////////////////////////
//  Move to Maildir/new
///////////////////////////////////////////////////////////////////////////////
	if (SysMoveFile(pszTmpEntryPath, szNewEntryPath) < 0)
		return (ErrGetErrorCode());

	return (0);

}

int MdirMoveMessage(char const *pszMaildirPath, const char *pszFileName, char const *pszMessageID)
{
///////////////////////////////////////////////////////////////////////////////
//  Allocate a Maildir/tmp entry
///////////////////////////////////////////////////////////////////////////////
	char szTmpEntryPath[SYS_MAX_PATH] = "";

	if (pszMessageID == NULL) {
		if (MdirGetTmpMaildirEntry(pszMaildirPath, szTmpEntryPath) < 0)
			return (ErrGetErrorCode());
	} else
		sprintf(szTmpEntryPath, "%s" SYS_SLASH_STR "tmp" SYS_SLASH_STR "%s",
			pszMaildirPath, pszMessageID);

///////////////////////////////////////////////////////////////////////////////
//  This perform a copy&delete to Maildir/tmp
///////////////////////////////////////////////////////////////////////////////
	if (MscMoveFile(pszFileName, szTmpEntryPath) < 0) {
		ErrorPush();
		CheckRemoveFile(szTmpEntryPath);
		return (ErrorPop());
	}
///////////////////////////////////////////////////////////////////////////////
//  This perform a fast system move from Maildir/tmp to Maildir/new
///////////////////////////////////////////////////////////////////////////////
	if (MdirMoveTmpEntryInNew(szTmpEntryPath) < 0) {
		ErrorPush();
		SysRemove(szTmpEntryPath);
		return (ErrorPop());
	}

	return (0);

}
