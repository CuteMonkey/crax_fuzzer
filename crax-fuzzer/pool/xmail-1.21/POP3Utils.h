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

#ifndef _POP3UTILS_H
#define _POP3UTILS_H

#define INVALID_POP3_HANDLE         ((POP3_HANDLE) 0)

#define AUTH_TYPE_CLEAR             "CLR"
#define AUTH_TYPE_APOP              "APOP"

#define POP3_USER_SPLITTERS         "@:"

typedef struct POP3_HANDLE_struct {
} *POP3_HANDLE;

struct PopSyncReport {
	int iMsgSync;
	int iMsgErr;
	unsigned long ulSizeSync;
	unsigned long ulSizeErr;
};

struct PopLastLoginInfo {
	SYS_INET_ADDR Address;
	time_t LTime;
};

int UPopGetMailboxSize(UserInfo * pUI, unsigned long &ulMBSize, unsigned long &ulNumMessages);
int UPopCheckMailboxSize(UserInfo * pUI, unsigned long *pulAvailSpace = NULL);
int UPopAuthenticateAPOP(const char *pszDomain, const char *pszUsrName,
			 const char *pszTimeStamp, const char *pszDigest);
POP3_HANDLE UPopBuildSession(const char *pszDomain, const char *pszUsrName,
			     const char *pszUsrPass, SYS_INET_ADDR const *pPeerInfo);
void UPopReleaseSession(POP3_HANDLE hPOPSession, int iUpdate = 1);
char *UPopGetUserInfoVar(POP3_HANDLE hPOPSession, const char *pszName,
			 const char *pszDefault = NULL);
int UPopGetSessionMsgCurrent(POP3_HANDLE hPOPSession);
int UPopGetSessionMsgTotal(POP3_HANDLE hPOPSession);
unsigned long UPopGetSessionMBSize(POP3_HANDLE hPOPSession);
int UPopGetSessionLastAccessed(POP3_HANDLE hPOPSession);
int UPopGetMessageSize(POP3_HANDLE hPOPSession, int iMsgIndex, unsigned long &ulMessageSize);
int UPopGetMessageUIDL(POP3_HANDLE hPOPSession, int iMsgIndex, char *pszMessageUIDL);
int UPopDeleteMessage(POP3_HANDLE hPOPSession, int iMsgIndex);
int UPopResetSession(POP3_HANDLE hPOPSession);
int UPopSendErrorResponse(BSOCK_HANDLE hBSock, int iErrorCode, int iTimeout);
int UPopSessionSendMsg(POP3_HANDLE hPOPSession, int iMsgIndex, BSOCK_HANDLE hBSock);
int UPopSessionTopMsg(POP3_HANDLE hPOPSession, int iMsgIndex, int iNumLines, BSOCK_HANDLE hBSock);
int UPopSaveUserIP(POP3_HANDLE hPOPSession);
int UPopSyncRemoteLink(const char *pszSyncAddr, const char *pszRmtServer,
		       const char *pszRmtName, const char *pszRmtPassword,
		       PopSyncReport * pSRep,
		       const char *pszFetchHdrTags = "+X-Deliver-To,To,Cc",
		       const char *pszAuthType = AUTH_TYPE_CLEAR,
		       const char *pszErrorAccount = NULL);
int UPopUserIpCheck(UserInfo * pUI, SYS_INET_ADDR const *pPeerInfo, unsigned int uExpireTime);
int UPopGetLastLoginInfo(UserInfo *pUI, PopLastLoginInfo *pInfo);

#endif
