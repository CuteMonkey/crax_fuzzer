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

#ifndef _MISCUTILS_H
#define _MISCUTILS_H

#define LOCK_FILE_WAITSTEP          1

#define INVALID_FSCAN_HANDLE        ((FSCAN_HANDLE) 0)

#define HASH_INIT_VALUE             5381

typedef struct FSCAN_HANDLE_struct {
} *FSCAN_HANDLE;

struct AddressFilter {
	SYS_UINT8 Addr[sizeof(NET_ADDRESS)];
	SYS_UINT8 Mask[sizeof(NET_ADDRESS)];
};

int MscUniqueFile(char const *pszDir, char *pszFilePath);
int MscRecvTextFile(const char *pszFileName, BSOCK_HANDLE hBSock, int iTimeout,
		    int (*pStopProc) (void *) = NULL, void *pParam = NULL);
int MscSendTextFile(const char *pszFileName, BSOCK_HANDLE hBSock, int iTimeout,
		    int (*pStopProc) (void *) = NULL, void *pParam = NULL);
char *MscTranslatePath(char *pszPath);
void *MscLoadFile(char const *pszFilePath, unsigned int &uFileSize);
int MscLockFile(const char *pszFileName, int iMaxWait, int iWaitStep = LOCK_FILE_WAITSTEP);
int MscGetTimeNbrString(char *pszTimeStr, int iStringSize, time_t tTime = 0);
int MscGetTime(struct tm &tmLocal, int &iDiffHours, int &iDiffMins, time_t tCurr = 0);
char *MscStrftime(struct tm const *ptmTime, char *pszDateStr, int iSize);
int MscGetTimeStr(char *pszTimeStr, int iStringSize, time_t tCurr = 0);
int MscGetDirectorySize(char const *pszPath, bool bRecurse, unsigned long &ulDirSize,
			unsigned long &ulNumFiles, int (*pFNValidate) (char const *) = NULL);
FSCAN_HANDLE MscFirstFile(char const *pszPath, int iListDirs, char *pszFileName);
int MscNextFile(FSCAN_HANDLE hFileScan, char *pszFileName);
void MscCloseFindFile(FSCAN_HANDLE hFileScan);
int MscGetFileList(char const *pszPath, const char *pszListFile, int iListDirs = 1);
int MscCreateEmptyFile(const char *pszFileName);
int MscClearDirectory(const char *pszPath, int iRecurseSubs = 1);
int MscCopyFile(const char *pszCopyTo, const char *pszCopyFrom);
int MscAppendFile(const char *pszCopyTo, const char *pszCopyFrom);
int MscCopyFile(FILE * pFileOut, FILE * pFileIn, unsigned long ulBaseOffset,
		unsigned long ulCopySize);
int MscMoveFile(char const *pszOldName, char const *pszNewName);
char *MscGetString(FILE * pFile, char *pszBuffer, int iMaxChars);
char *MscFGets(char *pszLine, int iLineSize, FILE * pFile);
char *MscGetConfigLine(char *pszLine, int iLineSize, FILE * pFile, bool bSkipComments = true);
int MscGetPeerHost(SYS_SOCKET SockFD, char *pszFQDN);
int MscGetSockHost(SYS_SOCKET SockFD, char *pszFQDN);
int MscGetServerAddress(char const *pszServer, SYS_INET_ADDR & SvrAddr, int iPortNo = 0);
int MscSplitFQDN(const char *pszFQDN, char *pszHost, char *pszDomain);
char *MscLogFilePath(char const *pszLogFile, char *pszLogFilePath);
int MscFileLog(char const *pszLogFile, char const *pszFormat, ...);
int MscSplitPath(char const *pszFilePath, char *pszDir, char *pszFName, char *pszExt);
int MscGetFileName(char const *pszFilePath, char *pszFileName);
int MscCreateClientSocket(char const *pszServer, int iPortNo, int iSockType,
			  SYS_SOCKET * pSockFD, SYS_INET_ADDR * pSvrAddr,
			  SYS_INET_ADDR * pSockAddr, int iTimeout);
int MscCreateServerSockets(int iNumAddr, SYS_INET_ADDR const *pSvrAddr, int iPortNo,
			   int iListenSize, SYS_SOCKET * pSockFDs, int &iNumSockFDs);
int MscGetMaxSockFD(SYS_SOCKET const *pSockFDs, int iNumSockFDs);
int MscAcceptServerConnection(SYS_SOCKET const *pSockFDs, int iNumSockFDs,
			      SYS_SOCKET * pConnSockFD, int &iNumConnSockFD, int iTimeout);
int MscLoadAddressFilter(char const *const *ppszFilter, int iNumTokens, AddressFilter & AF);
bool MscAddressMatch(AddressFilter const &AF, NET_ADDRESS const &TestAddr);
int MscCheckAllowedIP(char const *pszMapFile, const SYS_INET_ADDR & PeerInfo, bool bDefault);
char **MscGetIPProperties(char const *pszFileName, const SYS_INET_ADDR & PeerInfo);
int MscMD5Authenticate(const char *pszPassword, const char *pszTimeStamp, const char *pszDigest);
char *MscExtractServerTimeStamp(char const *pszResponse, char *pszTimeStamp, int iMaxTimeStamp);
int MscBase64FileEncode(char const *pszBoundary, char const *pszFilePath, FILE * pFileOut);
int MscRootedName(char const *pszHostName);
int MscCramMD5(char const *pszSecret, char const *pszChallenge, char *pszDigest);
SYS_UINT32 MscHashString(char const *pszBuffer, int iLength,
			 SYS_UINT32 uHashInit = HASH_INIT_VALUE);
int MscSplitAddressPort(char const *pszConnSpec, char *pszAddress, int &iPortNo, int iDefPortNo);
SYS_UINT16 MscReadUint16(void const *pData);
SYS_UINT32 MscReadUint32(void const *pData);
SYS_UINT64 MscReadUint64(void const *pData);
void *MscWriteUint16(void *pData, SYS_UINT16 uValue);
void *MscWriteUint32(void *pData, SYS_UINT32 uValue);
void *MscWriteUint64(void *pData, SYS_UINT64 uValue);
int MscCmdStringCheck(char const *pszString);
int MscGetSectionSize(FileSection const *pFS, unsigned long *pulSize);

#endif
