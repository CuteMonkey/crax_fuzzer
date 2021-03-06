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

#ifndef _SVRUTILS_H
#define _SVRUTILS_H

#define SVR_LOGS_DIR                "logs"

#define INVALID_SVRCFG_HANDLE       ((SVRCFG_HANDLE) 0)

typedef struct SVRCFG_HANDLE_struct {
} *SVRCFG_HANDLE;

SVRCFG_HANDLE SvrGetConfigHandle(int iWriteLock = 0);
void SvrReleaseConfigHandle(SVRCFG_HANDLE hSvrConfig);
char *SvrGetConfigVar(SVRCFG_HANDLE hSvrConfig, const char *pszName,
		      const char *pszDefault = NULL);
bool SvrTestConfigFlag(char const *pszName, bool bDefault,
		       SVRCFG_HANDLE hSvrConfig = INVALID_SVRCFG_HANDLE);
int SvrGetConfigInt(char const *pszName, int iDefault,
		    SVRCFG_HANDLE hSvrConfig = INVALID_SVRCFG_HANDLE);
int SysFlushConfig(SVRCFG_HANDLE hSvrConfig);
int SvrGetMessageID(SYS_UINT64 * pullMessageID);
char *SvrGetLogsDir(char *pszLogsPath, int iMaxPath);
char *SvrGetSpoolDir(char *pszSpoolPath, int iMaxPath);
int SvrConfigVar(char const *pszVarName, char *pszVarValue, int iMaxVarValue,
		 SVRCFG_HANDLE hSvrConfig = INVALID_SVRCFG_HANDLE, char const *pszDefault = NULL);
int SvrCheckDiskSpace(unsigned long ulMinSpace);
int SvrCheckVirtMemSpace(unsigned long ulMinSpace);

#endif
