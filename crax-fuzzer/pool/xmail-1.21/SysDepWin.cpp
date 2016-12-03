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
#include "AppDefines.h"

#define MAX_TLS_KEYS                    64
#define UNUSED_TLS_KEY_PROC             ((void (*)(void *)) -1)

#define SOCK_VERSION_REQUESTED          MAKEWORD(2, 0)
#define SHUTDOWN_RECV_TIMEOUT           2

///////////////////////////////////////////////////////////////////////////////
//  Under certain circumstances ( M$ Proxy installed ?! ) a waiting operation
//  may be unlocked even if the IO terminal is not ready to perform following
//  IO request ( an error code WSAEWOULDBLOCK is returned ). The only solution
//  I see is to sleep a bit to prevent processor time wasting :(
///////////////////////////////////////////////////////////////////////////////
#define BLOCKED_SNDRCV_MSSLEEP          200
#define HANDLE_SOCKS_SUCKS()            do { Sleep(BLOCKED_SNDRCV_MSSLEEP); } while (0)

#define SAIN_Addr(s)                    (s).sin_addr.S_un.S_addr

#define MIN_TCP_SEND_SIZE               (1024 * 8)
#define MAX_TCP_SEND_SIZE               (1024 * 128)
#define K_IO_TIME_RATIO                 8

#define MAX_STACK_SHIFT                 2048
#define STACK_ALIGN_BYTES               sizeof(int)

struct TlsKeyEntry {
	void (*pFreeProc) (void *);
};

struct TlsKeyData {
	void *pData;
};

struct FileFindData {
	char szFindPath[SYS_MAX_PATH];
	HANDLE hFind;
	WIN32_FIND_DATA WFD;
};

struct ThreadRunner {
	unsigned int (*pThreadProc) (void *);
	void *pThreadData;
};

static void SysInitTlsKeyEntries(void);
static void SysInitTlsKeys(void);
static void SysCleanupTlsKeys(void);
static char const *SysGetLastError(void);
static int SysSetServerName(void);
static int SysBlockSocket(SYS_SOCKET SockFD, int OnOff);
static int SysSetSocketsOptions(SYS_SOCKET SockFD);
static int SysRecvLL(SYS_SOCKET SockFD, char *pszBuffer, int iBufferSize);
static int SysSendLL(SYS_SOCKET SockFD, char const *pszBuffer, int iBufferSize);
static unsigned int SysThreadRunner(void *pRunData);
static int SysThreadSetup(void);
static int SysThreadCleanup(void);
static BOOL WINAPI SysBreakHandlerRoutine(DWORD dwCtrlType);
static void SysTimetToFileTime(time_t tTime, LPFILETIME pFT);
static time_t SysFileTimeToTimet(LPFILETIME pFT);
static unsigned int SysStkCall(unsigned int (*pProc) (void *), void *pData);

static char szServerName[SYS_MAX_PATH];
static CRITICAL_SECTION csTLS;
static bool bSetupEntries = true;
static TlsKeyEntry TlsKeyEntries[MAX_TLS_KEYS];
static __declspec(thread)
TlsKeyData TlsKeys[MAX_TLS_KEYS];
static time_t tSysStart;
static SYS_INT64 PCFreq;
static SYS_INT64 PCSysStart;
static unsigned int uSRandBase;
static int iSndBufSize = -1, iRcvBufSize = -1;
static CRITICAL_SECTION csLog;
static void (*SysBreakHandler) (void) = NULL;
static HANDLE hShutdownEvent = NULL;

static void SysInitTlsKeyEntries(void)
{

	if (bSetupEntries) {
		for (int ii = 0; ii < MAX_TLS_KEYS; ii++)
			TlsKeyEntries[ii].pFreeProc = UNUSED_TLS_KEY_PROC;

		bSetupEntries = false;
	}

}

static void SysInitTlsKeys(void)
{

	for (int ii = 0; ii < MAX_TLS_KEYS; ii++)
		TlsKeys[ii].pData = NULL;

}

static void SysCleanupTlsKeys(void)
{

	EnterCriticalSection(&csTLS);

	for (int ii = 0; ii < MAX_TLS_KEYS; ii++) {
		if ((TlsKeyEntries[ii].pFreeProc != UNUSED_TLS_KEY_PROC) &&
		    (TlsKeyEntries[ii].pFreeProc != NULL))
			TlsKeyEntries[ii].pFreeProc(TlsKeys[ii].pData);

		TlsKeys[ii].pData = NULL;
	}

	LeaveCriticalSection(&csTLS);

}

static char const *SysGetLastError(void)
{

	char *pszMessage = NULL;
	DWORD dwError = GetLastError();
	DWORD dwRet =
	    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			  FORMAT_MESSAGE_ARGUMENT_ARRAY,
			  NULL,
			  dwError,
			  LANG_NEUTRAL,
			  (LPTSTR) & pszMessage,
			  0,
			  NULL);

	static char szMessage[1024] = "";

	if (dwRet == 0)
		SysSNPrintf(szMessage, sizeof(szMessage) - 1, "(0x%lX) Unknown error",
			    (unsigned long) dwError);
	else
		SysSNPrintf(szMessage, sizeof(szMessage) - 1, "(0x%lX) %s",
			    (unsigned long) dwError, pszMessage);

	if (pszMessage != NULL)
		LocalFree((HLOCAL) pszMessage);

	return (szMessage);

}

static int SysSetServerName(void)
{

	int iSize;
	char *pszSlash;
	char *pszDot;
	char szPath[SYS_MAX_PATH] = APP_NAME_STR;

	GetModuleFileName(NULL, szPath, CountOf(szPath));

	if ((pszSlash = strrchr(szPath, '\\')) == NULL)
		pszSlash = szPath;
	else
		pszSlash++;

	if ((pszDot = strchr(pszSlash, '.')) == NULL)
		pszDot = pszSlash + strlen(pszSlash);

	iSize = Min(sizeof(szServerName) - 1, (int) (pszDot - pszSlash));
	Cpy2Sz(szServerName, pszSlash, iSize);

	return (0);

}

int SysInitLibrary(void)
{
///////////////////////////////////////////////////////////////////////////////
//  Setup timers
///////////////////////////////////////////////////////////////////////////////
	LARGE_INTEGER PerfCntFreq;
	LARGE_INTEGER PerfCntCurr;

	QueryPerformanceFrequency(&PerfCntFreq);
	QueryPerformanceCounter(&PerfCntCurr);

	PCFreq = *(SYS_INT64 *) & PerfCntFreq;
	PCFreq /= 1000;
	PCSysStart = *(SYS_INT64 *) & PerfCntCurr;

	_tzset();
	time(&tSysStart);
	uSRandBase = (unsigned int) tSysStart;

///////////////////////////////////////////////////////////////////////////////
//  Set the server name
///////////////////////////////////////////////////////////////////////////////
	SysSetServerName();

///////////////////////////////////////////////////////////////////////////////
//  Setup sockets
///////////////////////////////////////////////////////////////////////////////
	WSADATA WSD;

	ZeroData(WSD);

	if (WSAStartup(SOCK_VERSION_REQUESTED, &WSD)) {
		ErrSetErrorCode(ERR_NETWORK);
		return (ERR_NETWORK);
	}

	InitializeCriticalSection(&csTLS);
	SysInitTlsKeyEntries();

	InitializeCriticalSection(&csLog);

	if ((hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) == NULL) {
		DeleteCriticalSection(&csLog);
		DeleteCriticalSection(&csTLS);
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (ERR_CREATEEVENT);
	}

	if (SysThreadSetup() < 0) {
		ErrorPush();
		CloseHandle(hShutdownEvent);
		DeleteCriticalSection(&csLog);
		DeleteCriticalSection(&csTLS);
		return (ErrorPop());
	}

	return (0);

}

void SysCleanupLibrary(void)
{

	SysThreadCleanup();

	DeleteCriticalSection(&csLog);

	DeleteCriticalSection(&csTLS);

	CloseHandle(hShutdownEvent);

	WSACleanup();

}

int SysShutdownLibrary(int iMode)
{

	SetEvent(hShutdownEvent);

	return (0);

}

int SysSetupSocketBuffers(int *piSndBufSize, int *piRcvBufSize)
{

	if (piSndBufSize != NULL)
		iSndBufSize = *piSndBufSize;

	if (piRcvBufSize != NULL)
		iRcvBufSize = *piRcvBufSize;

	return (0);

}

SYS_SOCKET SysCreateSocket(int iAddressFamily, int iType, int iProtocol)
{

	SOCKET SockFD = WSASocket(iAddressFamily, iType, iProtocol, NULL, 0,
				  WSA_FLAG_OVERLAPPED);

	if (SockFD == INVALID_SOCKET) {
		ErrSetErrorCode(ERR_SOCKET_CREATE);
		return (SYS_INVALID_SOCKET);
	}

	if (SysSetSocketsOptions((SYS_SOCKET) SockFD) < 0) {
		SysCloseSocket((SYS_SOCKET) SockFD);

		return (SYS_INVALID_SOCKET);
	}

	return ((SYS_SOCKET) SockFD);

}

static int SysBlockSocket(SYS_SOCKET SockFD, int OnOff)
{

	u_long IoctlLong = (OnOff) ? 0 : 1;

	if (ioctlsocket(SockFD, FIONBIO, &IoctlLong) == SOCKET_ERROR) {
		ErrSetErrorCode(ERR_NETWORK);
		return (ERR_NETWORK);
	}

	return (0);

}

static int SysSetSocketsOptions(SYS_SOCKET SockFD)
{
///////////////////////////////////////////////////////////////////////////////
//  Set socket buffer sizes
///////////////////////////////////////////////////////////////////////////////
	if (iSndBufSize > 0) {
		int iSize = iSndBufSize;

		setsockopt((int) SockFD, SOL_SOCKET, SO_SNDBUF, (const char *) &iSize,
			   sizeof(iSize));
	}

	if (iRcvBufSize > 0) {
		int iSize = iRcvBufSize;

		setsockopt((int) SockFD, SOL_SOCKET, SO_RCVBUF, (const char *) &iSize,
			   sizeof(iSize));
	}

	int iActivate = 1;

	if (setsockopt(SockFD, SOL_SOCKET, SO_REUSEADDR, (const char *) &iActivate,
		       sizeof(iActivate)) != 0) {
		ErrSetErrorCode(ERR_SETSOCKOPT);
		return (ERR_SETSOCKOPT);
	}
///////////////////////////////////////////////////////////////////////////////
//  Disable linger
///////////////////////////////////////////////////////////////////////////////
	struct linger Ling;

	ZeroData(Ling);
	Ling.l_onoff = 0;
	Ling.l_linger = 0;

	setsockopt(SockFD, SOL_SOCKET, SO_LINGER, (const char *) &Ling, sizeof(Ling));

///////////////////////////////////////////////////////////////////////////////
//  Set KEEPALIVE if supported
///////////////////////////////////////////////////////////////////////////////
	setsockopt(SockFD, SOL_SOCKET, SO_KEEPALIVE, (const char *) &iActivate,
		   sizeof(iActivate));

	return (0);

}

void SysCloseSocket(SYS_SOCKET SockFD)
{

	closesocket(SockFD);

}

int SysBindSocket(SYS_SOCKET SockFD, const struct sockaddr *SockName, int iNameLen)
{

	if (bind(SockFD, SockName, iNameLen) == SOCKET_ERROR) {
		ErrSetErrorCode(ERR_SOCKET_BIND);
		return (ERR_SOCKET_BIND);
	}

	return (0);

}

void SysListenSocket(SYS_SOCKET SockFD, int iConnections)
{

	listen(SockFD, iConnections);

}

static int SysRecvLL(SYS_SOCKET SockFD, char *pszBuffer, int iBufferSize)
{

	DWORD dwRtxBytes = 0;
	DWORD dwRtxFlags = 0;
	WSABUF WSABuff;

	ZeroData(WSABuff);
	WSABuff.len = iBufferSize;
	WSABuff.buf = pszBuffer;

	return ((WSARecv(SockFD, &WSABuff, 1, &dwRtxBytes, &dwRtxFlags,
			 NULL, NULL) == 0) ? (int) dwRtxBytes : -WSAGetLastError());

}

static int SysSendLL(SYS_SOCKET SockFD, char const *pszBuffer, int iBufferSize)
{

	DWORD dwRtxBytes = 0;
	WSABUF WSABuff;

	ZeroData(WSABuff);
	WSABuff.len = iBufferSize;
	WSABuff.buf = (char *) pszBuffer;

	return ((WSASend(SockFD, &WSABuff, 1, &dwRtxBytes, 0,
			 NULL, NULL) == 0) ? (int) dwRtxBytes : -WSAGetLastError());

}

int SysRecvData(SYS_SOCKET SockFD, char *pszBuffer, int iBufferSize, int iTimeout)
{

	HANDLE hReadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (hReadEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (ERR_CREATEEVENT);
	}

	int iRecvBytes = 0;
	HANDLE hWaitEvents[2] = { hReadEvent, hShutdownEvent };

	for (;;) {
		WSAEventSelect(SockFD, (WSAEVENT) hReadEvent, FD_READ | FD_CLOSE);

		DWORD dwWaitResult = WSAWaitForMultipleEvents(2, hWaitEvents, FALSE,
							      (DWORD) (iTimeout * 1000), TRUE);

		WSAEventSelect(SockFD, (WSAEVENT) hReadEvent, 0);

		if (dwWaitResult == (WSA_WAIT_EVENT_0 + 1)) {
			CloseHandle(hReadEvent);
			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);
			return (ERR_SERVER_SHUTDOWN);
		} else if (dwWaitResult != WSA_WAIT_EVENT_0) {
			CloseHandle(hReadEvent);
			ErrSetErrorCode(ERR_TIMEOUT);
			return (ERR_TIMEOUT);
		}

		if ((iRecvBytes = SysRecvLL(SockFD, pszBuffer, iBufferSize)) >= 0)
			break;

		int iErrorCode = -iRecvBytes;

		if (iErrorCode != WSAEWOULDBLOCK) {
			CloseHandle(hReadEvent);
			ErrSetErrorCode(ERR_NETWORK);
			return (ERR_NETWORK);
		}
///////////////////////////////////////////////////////////////////////////////
//  You should never get here if Win32 API worked fine
///////////////////////////////////////////////////////////////////////////////
		HANDLE_SOCKS_SUCKS();
	}

	CloseHandle(hReadEvent);

	return (iRecvBytes);

}

int SysRecv(SYS_SOCKET SockFD, char *pszBuffer, int iBufferSize, int iTimeout)
{

	int iRtxBytes = 0;

	while (iRtxBytes < iBufferSize) {
		int iRtxCurrent = SysRecvData(SockFD, pszBuffer + iRtxBytes,
					      iBufferSize - iRtxBytes, iTimeout);

		if (iRtxCurrent <= 0)
			return (iRtxBytes);

		iRtxBytes += iRtxCurrent;
	}

	return (iRtxBytes);

}

int SysRecvDataFrom(SYS_SOCKET SockFD, struct sockaddr *pFrom, int iFromlen,
		    char *pszBuffer, int iBufferSize, int iTimeout)
{

	HANDLE hReadEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (hReadEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (ERR_CREATEEVENT);
	}

	DWORD dwRtxBytes = 0;
	WSABUF WSABuff;
	HANDLE hWaitEvents[2] = { hReadEvent, hShutdownEvent };

	ZeroData(WSABuff);
	WSABuff.len = iBufferSize;
	WSABuff.buf = pszBuffer;

	for (;;) {
		WSAEventSelect(SockFD, (WSAEVENT) hReadEvent, FD_READ | FD_CLOSE);

		DWORD dwWaitResult = WSAWaitForMultipleEvents(2, hWaitEvents, FALSE,
							      (DWORD) (iTimeout * 1000), TRUE);

		WSAEventSelect(SockFD, (WSAEVENT) hReadEvent, 0);

		if (dwWaitResult == (WSA_WAIT_EVENT_0 + 1)) {
			CloseHandle(hReadEvent);
			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);
			return (ERR_SERVER_SHUTDOWN);
		} else if (dwWaitResult != WSA_WAIT_EVENT_0) {
			CloseHandle(hReadEvent);
			ErrSetErrorCode(ERR_TIMEOUT);
			return (ERR_TIMEOUT);
		}

		INT FromLen = (INT) iFromlen;
		DWORD dwRtxFlags = 0;

		if (WSARecvFrom(SockFD, &WSABuff, 1, &dwRtxBytes, &dwRtxFlags,
				pFrom, &FromLen, NULL, NULL) == 0)
			break;

		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			CloseHandle(hReadEvent);
			ErrSetErrorCode(ERR_NETWORK);
			return (ERR_NETWORK);
		}
///////////////////////////////////////////////////////////////////////////////
//  You should never get here if Win32 API worked fine
///////////////////////////////////////////////////////////////////////////////
		HANDLE_SOCKS_SUCKS();
	}

	CloseHandle(hReadEvent);

	return ((int) dwRtxBytes);

}

int SysSendData(SYS_SOCKET SockFD, char const *pszBuffer, int iBufferSize, int iTimeout)
{

	HANDLE hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (hWriteEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (ERR_CREATEEVENT);
	}

	int iSendBytes = 0;
	HANDLE hWaitEvents[2] = { hWriteEvent, hShutdownEvent };

	for (;;) {
		WSAEventSelect(SockFD, (WSAEVENT) hWriteEvent, FD_WRITE | FD_CLOSE);

		DWORD dwWaitResult = WSAWaitForMultipleEvents(2, hWaitEvents, FALSE,
							      (DWORD) (iTimeout * 1000), TRUE);

		WSAEventSelect(SockFD, (WSAEVENT) hWriteEvent, 0);

		if (dwWaitResult == (WSA_WAIT_EVENT_0 + 1)) {
			CloseHandle(hWriteEvent);
			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);
			return (ERR_SERVER_SHUTDOWN);
		} else if (dwWaitResult != WSA_WAIT_EVENT_0) {
			CloseHandle(hWriteEvent);
			ErrSetErrorCode(ERR_TIMEOUT);
			return (ERR_TIMEOUT);
		}

		if ((iSendBytes = SysSendLL(SockFD, pszBuffer, iBufferSize)) >= 0)
			break;

		int iErrorCode = -iSendBytes;

		if (iErrorCode != WSAEWOULDBLOCK) {
			CloseHandle(hWriteEvent);
			ErrSetErrorCode(ERR_NETWORK);
			return (ERR_NETWORK);
		}
///////////////////////////////////////////////////////////////////////////////
//  You should never get here if Win32 API worked fine
///////////////////////////////////////////////////////////////////////////////
		HANDLE_SOCKS_SUCKS();
	}

	CloseHandle(hWriteEvent);

	return (iSendBytes);

}

int SysSend(SYS_SOCKET SockFD, char const *pszBuffer, int iBufferSize, int iTimeout)
{

	int iRtxBytes = 0;

	while (iRtxBytes < iBufferSize) {
		int iRtxCurrent = SysSendData(SockFD, pszBuffer + iRtxBytes,
					      iBufferSize - iRtxBytes, iTimeout);

		if (iRtxCurrent <= 0)
			return (iRtxBytes);

		iRtxBytes += iRtxCurrent;
	}

	return (iRtxBytes);

}

int SysSendDataTo(SYS_SOCKET SockFD, const struct sockaddr *pTo,
		  int iToLen, char const *pszBuffer, int iBufferSize, int iTimeout)
{

	HANDLE hWriteEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (hWriteEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (ERR_CREATEEVENT);
	}

	DWORD dwRtxBytes = 0;
	WSABUF WSABuff;
	HANDLE hWaitEvents[2] = { hWriteEvent, hShutdownEvent };

	ZeroData(WSABuff);
	WSABuff.len = iBufferSize;
	WSABuff.buf = (char *) pszBuffer;

	for (;;) {
		WSAEventSelect(SockFD, (WSAEVENT) hWriteEvent, FD_WRITE | FD_CLOSE);

		DWORD dwWaitResult = WSAWaitForMultipleEvents(2, hWaitEvents, FALSE,
							      (DWORD) (iTimeout * 1000), TRUE);

		WSAEventSelect(SockFD, (WSAEVENT) hWriteEvent, 0);

		if (dwWaitResult == (WSA_WAIT_EVENT_0 + 1)) {
			CloseHandle(hWriteEvent);
			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);
			return (ERR_SERVER_SHUTDOWN);
		} else if (dwWaitResult != WSA_WAIT_EVENT_0) {
			CloseHandle(hWriteEvent);
			ErrSetErrorCode(ERR_TIMEOUT);
			return (ERR_TIMEOUT);
		}

		DWORD dwRtxFlags = 0;

		if (WSASendTo(SockFD, &WSABuff, 1, &dwRtxBytes, dwRtxFlags,
			      pTo, iToLen, NULL, NULL) == 0)
			break;

		if (WSAGetLastError() != WSAEWOULDBLOCK) {
			CloseHandle(hWriteEvent);
			ErrSetErrorCode(ERR_NETWORK);
			return (ERR_NETWORK);
		}
///////////////////////////////////////////////////////////////////////////////
//  You should never get here if Win32 API worked fine
///////////////////////////////////////////////////////////////////////////////
		HANDLE_SOCKS_SUCKS();
	}

	CloseHandle(hWriteEvent);

	return ((int) dwRtxBytes);

}

int SysConnect(SYS_SOCKET SockFD, const SYS_INET_ADDR * pSockName, int iNameLen, int iTimeout)
{

	HANDLE hConnectEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (hConnectEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (ERR_CREATEEVENT);
	}

	WSAEventSelect(SockFD, (WSAEVENT) hConnectEvent, FD_CONNECT);

	int iConnectResult = WSAConnect(SockFD, (const struct sockaddr *) &pSockName->Addr,
					iNameLen, NULL, NULL, NULL, NULL);
	int iConnectError = WSAGetLastError();

	if ((iConnectResult != 0) && (iConnectError == WSAEWOULDBLOCK)) {
		HANDLE hWaitEvents[2] = { hConnectEvent, hShutdownEvent };
		DWORD dwWaitResult = WSAWaitForMultipleEvents(2, hWaitEvents, FALSE,
							      (DWORD) (iTimeout * 1000), TRUE);

		if (dwWaitResult == WSA_WAIT_EVENT_0) {
			WSANETWORKEVENTS NetEvents;

			if ((WSAEnumNetworkEvents(SockFD, hConnectEvent, &NetEvents) != 0) ||
			    (NetEvents.iErrorCode[FD_CONNECT_BIT] != 0)) {
				ErrSetErrorCode(ERR_CONNECT);

				iConnectResult = ERR_CONNECT;
			} else
				iConnectResult = 0;
		} else if (dwWaitResult == (WSA_WAIT_EVENT_0 + 1)) {
			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);

			iConnectResult = ERR_SERVER_SHUTDOWN;
		} else {
			ErrSetErrorCode(ERR_TIMEOUT);

			iConnectResult = ERR_TIMEOUT;
		}
	}

	WSAEventSelect(SockFD, (WSAEVENT) hConnectEvent, 0);
	CloseHandle(hConnectEvent);

	return (iConnectResult);

}

SYS_SOCKET SysAccept(SYS_SOCKET SockFD, SYS_INET_ADDR * pSockName, int *iNameLen, int iTimeout)
{

	HANDLE hAcceptEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

	if (hAcceptEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (SYS_INVALID_SOCKET);
	}

	WSAEventSelect(SockFD, (WSAEVENT) hAcceptEvent, FD_ACCEPT);

	SOCKET SockFDAccept =
	    WSAAccept(SockFD, (struct sockaddr *) &pSockName->Addr, iNameLen, NULL, 0);

	int iConnectError = WSAGetLastError();

	if ((SockFDAccept == INVALID_SOCKET) && (iConnectError == WSAEWOULDBLOCK)) {
		HANDLE hWaitEvents[2] = { hAcceptEvent, hShutdownEvent };
		DWORD dwWaitResult = WSAWaitForMultipleEvents(2, hWaitEvents, FALSE,
							      (DWORD) (iTimeout * 1000), TRUE);

		if (dwWaitResult == WSA_WAIT_EVENT_0)
			SockFDAccept =
			    WSAAccept(SockFD, (struct sockaddr *) &pSockName->Addr, iNameLen,
				      NULL, 0);
		else if (dwWaitResult == (WSA_WAIT_EVENT_0 + 1))
			ErrSetErrorCode(ERR_SERVER_SHUTDOWN);
		else
			ErrSetErrorCode(ERR_TIMEOUT);
	}

	WSAEventSelect(SockFD, (WSAEVENT) hAcceptEvent, 0);
	CloseHandle(hAcceptEvent);

	if (SockFDAccept != INVALID_SOCKET) {
		if ((SysBlockSocket(SockFDAccept, 0) < 0) ||
		    (SysSetSocketsOptions(SockFDAccept) < 0)) {
			SysCloseSocket(SockFDAccept);
			return (SYS_INVALID_SOCKET);
		}
	}

	return (SockFDAccept);

}

int SysSelect(int iMaxFD, SYS_fd_set * pReadFDs, SYS_fd_set * pWriteFDs, SYS_fd_set * pExcptFDs,
	      int iTimeout)
{

	struct timeval TV;

	ZeroData(TV);
	TV.tv_sec = iTimeout;
	TV.tv_usec = 0;

	int iSelectResult = select(iMaxFD + 1, pReadFDs, pWriteFDs, pExcptFDs, &TV);

	if (iSelectResult < 0) {
		ErrSetErrorCode(ERR_SELECT);
		return (ERR_SELECT);
	}

	if (iSelectResult == 0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (iSelectResult);

}

int SysSendFile(SYS_SOCKET SockFD, char const *pszFileName, unsigned long ulBaseOffset,
		unsigned long ulEndOffset, int iTimeout)
{
///////////////////////////////////////////////////////////////////////////////
//  Open the source file
///////////////////////////////////////////////////////////////////////////////
	HANDLE hFile = CreateFile(pszFileName, GENERIC_READ, FILE_SHARE_READ,
				  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		ErrSetErrorCode(ERR_FILE_OPEN);
		return (ERR_FILE_OPEN);
	}
///////////////////////////////////////////////////////////////////////////////
//  Create file mapping and the file
///////////////////////////////////////////////////////////////////////////////
	DWORD dwFileSizeHi = 0;
	DWORD dwFileSizeLo = GetFileSize(hFile, &dwFileSizeHi);
	HANDLE hFileMap = CreateFileMapping(hFile, NULL, PAGE_READONLY,
					    dwFileSizeHi, dwFileSizeLo, NULL);

	if (hFileMap == NULL) {
		CloseHandle(hFile);

		ErrSetErrorCode(ERR_CREATEFILEMAPPING);
		return (ERR_CREATEFILEMAPPING);
	}

	void *pAddress = MapViewOfFile(hFileMap, FILE_MAP_READ, 0, 0, 0);

	if (pAddress == NULL) {
		CloseHandle(hFileMap);
		CloseHandle(hFile);

		ErrSetErrorCode(ERR_MAPVIEWOFFILE);
		return (ERR_MAPVIEWOFFILE);
	}
///////////////////////////////////////////////////////////////////////////////
//  Send the file
///////////////////////////////////////////////////////////////////////////////
	int iSndBuffSize = MIN_TCP_SEND_SIZE;
	SYS_UINT64 ullFileSize = (((SYS_UINT64) dwFileSizeHi) << 32) | (SYS_UINT64) dwFileSizeLo;
	SYS_UINT64 ullEndOffset = (ulEndOffset != (unsigned long) -1) ?
	    ((SYS_UINT64) ulEndOffset) : ullFileSize;
	SYS_UINT64 ullCurrOffset = (SYS_UINT64) ulBaseOffset;
	char *pszBuffer = (char *) pAddress + ulBaseOffset;
	time_t tStart;

	while (ullCurrOffset < ullEndOffset) {
		int iCurrSend = (int) Min(iSndBuffSize, ullEndOffset - ullCurrOffset);

		tStart = time(NULL);
		if ((iCurrSend = SysSendData(SockFD, pszBuffer, iCurrSend, iTimeout)) < 0) {
			ErrorPush();
			UnmapViewOfFile(pAddress);
			CloseHandle(hFileMap);
			CloseHandle(hFile);
			return (ErrorPop());
		}

		if ((((time(NULL) - tStart) * K_IO_TIME_RATIO) < iTimeout) &&
		    (iSndBuffSize < MAX_TCP_SEND_SIZE))
			iSndBuffSize = Min(iSndBuffSize * 2, MAX_TCP_SEND_SIZE);

		pszBuffer += iCurrSend;
		ullCurrOffset += (SYS_UINT64) iCurrSend;
	}

	UnmapViewOfFile(pAddress);
	CloseHandle(hFileMap);
	CloseHandle(hFile);

	return (0);

}

int SysSetupAddress(SYS_INET_ADDR & AddrInfo, int iFamily,
		    NET_ADDRESS const &NetAddr, int iPortNo)
{

	ZeroData(AddrInfo);
	AddrInfo.Addr.sin_family = iFamily;
	SAIN_Addr(AddrInfo.Addr) = NetAddr;
	AddrInfo.Addr.sin_port = htons((short) iPortNo);

	return (0);

}

int SysGetAddrAddress(SYS_INET_ADDR const &AddrInfo, NET_ADDRESS & NetAddr)
{

	NetAddr = SAIN_Addr(AddrInfo.Addr);

	return (0);

}

int SysGetAddrPort(SYS_INET_ADDR const &AddrInfo)
{

	return (ntohs(AddrInfo.Addr.sin_port));

}

int SysSetAddrAddress(SYS_INET_ADDR & AddrInfo, NET_ADDRESS const &NetAddr)
{

	SAIN_Addr(AddrInfo.Addr) = NetAddr;

	return (0);

}

int SysSetAddrPort(SYS_INET_ADDR & AddrInfo, int iPortNo)
{

	AddrInfo.Addr.sin_port = htons((short) iPortNo);

	return (0);

}

int SysGetHostByName(char const *pszName, NET_ADDRESS & NetAddr)
{

	struct hostent *pHostEnt = gethostbyname(pszName);

	if ((pHostEnt == NULL) || (pHostEnt->h_addr_list[0] == NULL)) {
		ErrSetErrorCode(ERR_BAD_SERVER_ADDR, pszName);
		return (ERR_BAD_SERVER_ADDR);
	}

	memcpy(&NetAddr, pHostEnt->h_addr_list[0], sizeof(NetAddr));

	return (0);

}

int SysGetHostByAddr(SYS_INET_ADDR const &AddrInfo, char *pszFQDN)
{

	struct hostent *pHostEnt = gethostbyaddr((const char *) &SAIN_Addr(AddrInfo.Addr),
						 sizeof(SAIN_Addr(AddrInfo.Addr)), AF_INET);

	if (pHostEnt == NULL) {
		char szIP[128] = "???.???.???.???";

		ErrSetErrorCode(ERR_GET_SOCK_HOST, SysInetNToA(AddrInfo, szIP));
		return (ERR_GET_SOCK_HOST);
	}

	strcpy(pszFQDN, pHostEnt->h_name);

	return (0);

}

int SysGetPeerInfo(SYS_SOCKET SockFD, SYS_INET_ADDR & AddrInfo)
{

	ZeroData(AddrInfo);

	socklen_t InfoSize = sizeof(AddrInfo.Addr);

	if (getpeername(SockFD, (struct sockaddr *) &AddrInfo.Addr, &InfoSize) == -1) {
		ErrSetErrorCode(ERR_GET_PEER_INFO);
		return (ERR_GET_PEER_INFO);
	}

	return (0);

}

int SysGetSockInfo(SYS_SOCKET SockFD, SYS_INET_ADDR & AddrInfo)
{

	ZeroData(AddrInfo);

	socklen_t InfoSize = sizeof(AddrInfo.Addr);

	if (getsockname(SockFD, (struct sockaddr *) &AddrInfo.Addr, &InfoSize) == -1) {
		ErrSetErrorCode(ERR_GET_SOCK_INFO);
		return (ERR_GET_SOCK_INFO);
	}

	return (0);

}

char *SysInetNToA(SYS_INET_ADDR const &AddrInfo, char *pszIP)
{

	union {
		unsigned int a;
		unsigned char b[4];
	} UAddr;

	memcpy(&UAddr, &AddrInfo.Addr.sin_addr, sizeof(UAddr));

	sprintf(pszIP, "%u.%u.%u.%u",
		(unsigned int) UAddr.b[0],
		(unsigned int) UAddr.b[1], (unsigned int) UAddr.b[2], (unsigned int) UAddr.b[3]);

	return (pszIP);

}

int SysInetAddr(char const *pszDotName, NET_ADDRESS & NetAddr)
{

	if ((NetAddr = (NET_ADDRESS) inet_addr(pszDotName)) == SYS_INVALID_NET_ADDRESS) {
		ErrSetErrorCode(ERR_BAD_SERVER_ADDR, pszDotName);
		return (ERR_BAD_SERVER_ADDR);
	}

	return (0);

}

int SysSameAddress(NET_ADDRESS const &NetAddr1, NET_ADDRESS const &NetAddr2)
{

	return (memcmp(&NetAddr1, &NetAddr2, sizeof(NET_ADDRESS)) == 0);

}

SYS_SEMAPHORE SysCreateSemaphore(int iInitCount, int iMaxCount)
{

	HANDLE hSemaphore = CreateSemaphore(NULL, iInitCount, iMaxCount, NULL);

	if (hSemaphore == NULL) {
		ErrSetErrorCode(ERR_CREATESEMAPHORE);
		return (SYS_INVALID_SEMAPHORE);
	}

	return ((SYS_SEMAPHORE) hSemaphore);

}

int SysCloseSemaphore(SYS_SEMAPHORE hSemaphore)
{

	if (!CloseHandle((HANDLE) hSemaphore)) {
		ErrSetErrorCode(ERR_CLOSEHANDLE);
		return (ERR_CLOSEHANDLE);
	}

	return (0);

}

int SysWaitSemaphore(SYS_SEMAPHORE hSemaphore, int iTimeout)
{

	if (WaitForSingleObject((HANDLE) hSemaphore, (DWORD) (iTimeout * 1000)) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

int SysReleaseSemaphore(SYS_SEMAPHORE hSemaphore, int iCount)
{

	if (!ReleaseSemaphore((HANDLE) hSemaphore, (LONG) iCount, NULL)) {
		ErrSetErrorCode(ERR_RELEASESEMAPHORE);
		return (ERR_RELEASESEMAPHORE);
	}

	return (0);

}

int SysTryWaitSemaphore(SYS_SEMAPHORE hSemaphore)
{

	if (WaitForSingleObject((HANDLE) hSemaphore, 0) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

SYS_MUTEX SysCreateMutex(void)
{

	HANDLE hMutex = CreateMutex(NULL, FALSE, NULL);

	if (hMutex == NULL) {
		ErrSetErrorCode(ERR_CREATEMUTEX);
		return (SYS_INVALID_MUTEX);
	}

	return ((SYS_MUTEX) hMutex);

}

int SysCloseMutex(SYS_MUTEX hMutex)
{

	if (!CloseHandle((HANDLE) hMutex)) {
		ErrSetErrorCode(ERR_CLOSEHANDLE);
		return (ERR_CLOSEHANDLE);
	}

	return (0);

}

int SysLockMutex(SYS_MUTEX hMutex, int iTimeout)
{

	if (WaitForSingleObject((HANDLE) hMutex, (DWORD) (iTimeout * 1000)) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

int SysUnlockMutex(SYS_MUTEX hMutex)
{

	ReleaseMutex((HANDLE) hMutex);

	return (0);

}

int SysTryLockMutex(SYS_MUTEX hMutex)
{

	if (WaitForSingleObject((HANDLE) hMutex, 0) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

SYS_EVENT SysCreateEvent(int iManualReset)
{

	HANDLE hEvent = CreateEvent(NULL, iManualReset, FALSE, NULL);

	if (hEvent == NULL) {
		ErrSetErrorCode(ERR_CREATEEVENT);
		return (SYS_INVALID_EVENT);
	}

	return ((SYS_EVENT) hEvent);

}

int SysCloseEvent(SYS_EVENT hEvent)
{

	if (!CloseHandle((HANDLE) hEvent)) {
		ErrSetErrorCode(ERR_CLOSEHANDLE);
		return (ERR_CLOSEHANDLE);
	}

	return (0);

}

int SysWaitEvent(SYS_EVENT hEvent, int iTimeout)
{

	if (WaitForSingleObject((HANDLE) hEvent, (DWORD) (iTimeout * 1000)) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

int SysSetEvent(SYS_EVENT hEvent)
{

	SetEvent((HANDLE) hEvent);

	return (0);

}

int SysResetEvent(SYS_EVENT hEvent)
{

	ResetEvent((HANDLE) hEvent);

	return (0);

}

int SysTryWaitEvent(SYS_EVENT hEvent)
{

	if (WaitForSingleObject((HANDLE) hEvent, 0) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

static unsigned int SysThreadRunner(void *pRunData)
{

	ThreadRunner *pTR = (ThreadRunner *) pRunData;

	if (SysThreadSetup() < 0) {
		SysFree(pTR);
		return (ErrGetErrorCode());
	}

	unsigned int uResultCode = SysStkCall(pTR->pThreadProc, pTR->pThreadData);

	SysThreadCleanup();

	SysFree(pTR);

	return (uResultCode);

}

SYS_THREAD SysCreateThread(unsigned int (*pThreadProc) (void *), void *pThreadData)
{
///////////////////////////////////////////////////////////////////////////////
//  Alloc thread runner data
///////////////////////////////////////////////////////////////////////////////
	ThreadRunner *pTR = (ThreadRunner *) SysAlloc(sizeof(ThreadRunner));

	if (pTR == NULL)
		return (SYS_INVALID_THREAD);

	pTR->pThreadProc = pThreadProc;
	pTR->pThreadData = pThreadData;

///////////////////////////////////////////////////////////////////////////////
//  Create the thread
///////////////////////////////////////////////////////////////////////////////
	unsigned int uThreadId = 0;
	unsigned long ulThread = _beginthreadex(NULL, 0,
						(unsigned (__stdcall *) (void *)) SysThreadRunner,
						pTR, 0, &uThreadId);

	if (ulThread == 0) {
		SysFree(pTR);
		ErrSetErrorCode(ERR_BEGINTHREADEX);
		return (SYS_INVALID_THREAD);
	}

	return ((SYS_THREAD) ulThread);

}

SYS_THREAD SysCreateServiceThread(unsigned int (*pThreadProc) (void *), SYS_SOCKET SockFD)
{

	SYS_THREAD ThreadID = SysCreateThread(pThreadProc, (void *) (unsigned int) SockFD);

	return (ThreadID);

}

static int SysThreadSetup(void)
{

	SysInitTlsKeys();

	return (0);

}

static int SysThreadCleanup(void)
{

	SysCleanupTlsKeys();

	return (0);

}

void SysCloseThread(SYS_THREAD ThreadID, int iForce)
{

	if (iForce)
		TerminateThread((HANDLE) ThreadID, (DWORD) - 1);

	CloseHandle((HANDLE) ThreadID);

}

int SysSetThreadPriority(SYS_THREAD ThreadID, int iPriority)
{

	BOOL bSetResult = FALSE;

	switch (iPriority) {
	case (SYS_PRIORITY_NORMAL):
		bSetResult = SetThreadPriority((HANDLE) ThreadID, THREAD_PRIORITY_NORMAL);
		break;

	case (SYS_PRIORITY_LOWER):
		bSetResult = SetThreadPriority((HANDLE) ThreadID, THREAD_PRIORITY_BELOW_NORMAL);
		break;

	case (SYS_PRIORITY_HIGHER):
		bSetResult = SetThreadPriority((HANDLE) ThreadID, THREAD_PRIORITY_ABOVE_NORMAL);
		break;
	}

	if (!bSetResult) {
		ErrSetErrorCode(ERR_SET_THREAD_PRIORITY);
		return (ERR_SET_THREAD_PRIORITY);
	}

	return (0);

}

int SysWaitThread(SYS_THREAD ThreadID, int iTimeout)
{

	if (WaitForSingleObject((HANDLE) ThreadID, (DWORD) (iTimeout * 1000)) != WAIT_OBJECT_0) {
		ErrSetErrorCode(ERR_TIMEOUT);
		return (ERR_TIMEOUT);
	}

	return (0);

}

unsigned long SysGetCurrentThreadId(void)
{

	return ((unsigned long) GetCurrentThreadId());

}

int SysExec(char const *pszCommand, char const *const *pszArgs, int iWaitTimeout,
	    int iPriority, int *piExitStatus)
{

	int ii;
	int iCommandLength = strlen(pszCommand) + 4;

	for (ii = 1; pszArgs[ii] != NULL; ii++)
		iCommandLength += strlen(pszArgs[ii]) + 4;

	char *pszCmdLine = (char *) SysAlloc(iCommandLength + 1);

	if (pszCmdLine == NULL)
		return (ErrGetErrorCode());

	strcpy(pszCmdLine, pszCommand);

	for (ii = 1; pszArgs[ii] != NULL; ii++)
		sprintf(StrAppend(pszCmdLine), " \"%s\"", pszArgs[ii]);

	PROCESS_INFORMATION PI;
	STARTUPINFO SI;

	ZeroData(PI);
	ZeroData(SI);
	SI.cb = sizeof(STARTUPINFO);

	BOOL bProcessCreated = CreateProcess(NULL, pszCmdLine, NULL, NULL, FALSE,
					     CREATE_NO_WINDOW | NORMAL_PRIORITY_CLASS, NULL, NULL,
					     &SI, &PI);

	SysFree(pszCmdLine);

	if (!bProcessCreated) {
		ErrSetErrorCode(ERR_PROCESS_EXECUTE);
		return (ERR_PROCESS_EXECUTE);
	}

	SysSetThreadPriority((SYS_THREAD) PI.hThread, iPriority);

	if (iWaitTimeout > 0) {
		if (WaitForSingleObject(PI.hProcess, iWaitTimeout * 1000) != WAIT_OBJECT_0) {
			CloseHandle(PI.hThread);
			CloseHandle(PI.hProcess);

			ErrSetErrorCode(ERR_TIMEOUT);
			return (ERR_TIMEOUT);
		}

		if (piExitStatus != NULL) {
			DWORD dwExitCode = 0;

			if (!GetExitCodeProcess(PI.hProcess, &dwExitCode))
				*piExitStatus = -1;
			else
				*piExitStatus = (int) dwExitCode;
		}
	} else if (piExitStatus != NULL)
		*piExitStatus = -1;

	CloseHandle(PI.hThread);
	CloseHandle(PI.hProcess);

	return (0);

}

static BOOL WINAPI SysBreakHandlerRoutine(DWORD dwCtrlType)
{

	BOOL bReturnValue = FALSE;

	switch (dwCtrlType) {
	case (CTRL_C_EVENT):
	case (CTRL_CLOSE_EVENT):
	case (CTRL_SHUTDOWN_EVENT):

		if (SysBreakHandler != NULL)
			SysBreakHandler(), bReturnValue = TRUE;

		break;

	}

	return (bReturnValue);

}

void SysSetBreakHandler(void (*BreakHandler) (void))
{

	if (SysBreakHandler == NULL)
		SetConsoleCtrlHandler(SysBreakHandlerRoutine,
				      (BreakHandler != NULL) ? TRUE : FALSE);

	SysBreakHandler = BreakHandler;

}

int SysCreateTlsKey(SYS_TLSKEY & TlsKey, void (*pFreeProc) (void *))
{

	EnterCriticalSection(&csTLS);

	for (int ii = 0; ii < MAX_TLS_KEYS; ii++) {
		if (TlsKeyEntries[ii].pFreeProc == UNUSED_TLS_KEY_PROC) {
			TlsKeyEntries[ii].pFreeProc = pFreeProc;

			LeaveCriticalSection(&csTLS);

			TlsKey = (SYS_TLSKEY) ii;

			return (0);
		}
	}

	LeaveCriticalSection(&csTLS);

	ErrSetErrorCode(ERR_NOMORE_TLSKEYS);

	return (ERR_NOMORE_TLSKEYS);

}

int SysDeleteTlsKey(SYS_TLSKEY & TlsKey)
{

	int iKey = (int) TlsKey;

	EnterCriticalSection(&csTLS);

	if ((iKey < 0) || (iKey >= MAX_TLS_KEYS) ||
	    (TlsKeyEntries[iKey].pFreeProc == UNUSED_TLS_KEY_PROC)) {
		LeaveCriticalSection(&csTLS);

		ErrSetErrorCode(ERR_INVALID_TLSKEY);
		return (ERR_INVALID_TLSKEY);
	}

	TlsKeyEntries[iKey].pFreeProc = UNUSED_TLS_KEY_PROC;

	LeaveCriticalSection(&csTLS);

	TlsKey = (SYS_TLSKEY) - 1;

	return (0);

}

int SysSetTlsKeyData(SYS_TLSKEY & TlsKey, void *pData)
{

	int iKey = (int) TlsKey;

	if ((iKey < 0) || (iKey >= MAX_TLS_KEYS) ||
	    (TlsKeyEntries[iKey].pFreeProc == UNUSED_TLS_KEY_PROC)) {
		ErrSetErrorCode(ERR_INVALID_TLSKEY);
		return (ERR_INVALID_TLSKEY);
	}

	TlsKeys[iKey].pData = pData;

	return (0);

}

void *SysGetTlsKeyData(SYS_TLSKEY & TlsKey)
{

	int iKey = (int) TlsKey;

	if ((iKey < 0) || (iKey >= MAX_TLS_KEYS) ||
	    (TlsKeyEntries[iKey].pFreeProc == UNUSED_TLS_KEY_PROC)) {
		ErrSetErrorCode(ERR_INVALID_TLSKEY);
		return (NULL);
	}

	return (TlsKeys[iKey].pData);

}

void SysThreadOnce(SYS_THREAD_ONCE * pThrOnce, void (*pOnceProc) (void))
{

	if (InterlockedExchange(&pThrOnce->lOnce, 1) == 0) {
		pOnceProc();

		pThrOnce->lDone++;
	}

	while (!pThrOnce->lDone)
		Sleep(0);

}

void *SysAlloc(unsigned int uSize)
{

	void *pData = malloc(uSize);

	if (pData != NULL)
		memset(pData, 0, uSize);
	else
		ErrSetErrorCode(ERR_MEMORY);

	return (pData);

}

void SysFree(void *pData)
{

	free(pData);

}

void *SysRealloc(void *pData, unsigned int uSize)
{

	void *pNewData = realloc(pData, uSize);

	if (pNewData == NULL)
		ErrSetErrorCode(ERR_MEMORY);

	return (pNewData);

}

int SysLockFile(const char *pszFileName, char const *pszLockExt)
{

	char szLockFile[SYS_MAX_PATH] = "";

	SysSNPrintf(szLockFile, sizeof(szLockFile) - 1, "%s%s", pszFileName, pszLockExt);

///////////////////////////////////////////////////////////////////////////////
//  Try to create lock file
///////////////////////////////////////////////////////////////////////////////
	HANDLE hFile = CreateFile(szLockFile, GENERIC_READ | GENERIC_WRITE,
				  0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		if (GetLastError() == ERROR_ALREADY_EXISTS) {
			ErrSetErrorCode(ERR_LOCKED);
			return (ERR_LOCKED);
		}

		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	DWORD dwWritten = 0;
	char szLock[128] = "";

	sprintf(szLock, "%lu", (unsigned long) GetCurrentThreadId());

	if (!WriteFile(hFile, szLock, strlen(szLock) + 1, &dwWritten, NULL)) {
		CloseHandle(hFile);

		ErrSetErrorCode(ERR_FILE_WRITE);
		return (ERR_FILE_WRITE);
	}

	CloseHandle(hFile);

	return (0);

}

int SysUnlockFile(const char *pszFileName, char const *pszLockExt)
{

	char szLockFile[SYS_MAX_PATH] = "";

	SysSNPrintf(szLockFile, sizeof(szLockFile) - 1, "%s%s", pszFileName, pszLockExt);

	if (_unlink(szLockFile) != 0) {
		ErrSetErrorCode(ERR_NOT_LOCKED);
		return (ERR_NOT_LOCKED);
	}

	return (0);

}

SYS_HANDLE SysOpenModule(char const *pszFilePath)
{

	HMODULE hModule = LoadLibrary(pszFilePath);

	if (hModule == NULL) {
		ErrSetErrorCode(ERR_LOADMODULE, pszFilePath);
		return (SYS_INVALID_HANDLE);
	}

	return ((SYS_HANDLE) hModule);

}

int SysCloseModule(SYS_HANDLE hModule)
{

	FreeLibrary((HMODULE) hModule);

	return (0);

}

void *SysGetSymbol(SYS_HANDLE hModule, char const *pszSymbol)
{

	void *pSymbol = (void *) GetProcAddress((HMODULE) hModule, pszSymbol);

	if (pSymbol == NULL) {
		ErrSetErrorCode(ERR_LOADMODULESYMBOL, pszSymbol);
		return (NULL);
	}

	return (pSymbol);

}

int SysEventLogV(char const *pszFormat, va_list Args)
{

	HANDLE hEventSource = RegisterEventSource(NULL, szServerName);

	if (hEventSource == NULL) {
		ErrSetErrorCode(ERR_REGISTER_EVENT_SOURCE);
		return (ERR_REGISTER_EVENT_SOURCE);
	}

	char *pszStrings[2];
	char szBuffer[2048] = "";

	_vsnprintf(szBuffer, sizeof(szBuffer) - 1, pszFormat, Args);
	pszStrings[0] = szBuffer;

	ReportEvent(hEventSource,
		    EVENTLOG_ERROR_TYPE, 0, 0, NULL, 1, 0, (const char **) pszStrings, NULL);

	DeregisterEventSource(hEventSource);

	return (0);

}

int SysEventLog(char const *pszFormat, ...)
{

	va_list Args;

	va_start(Args, pszFormat);

	int iLogResult = SysEventLogV(pszFormat, Args);

	va_end(Args);

	return (0);

}

int SysLogMessage(int iLogLevel, char const *pszFormat, ...)
{

	extern bool bServerDebug;

	EnterCriticalSection(&csLog);

	va_list Args;

	va_start(Args, pszFormat);

	if (bServerDebug) {
///////////////////////////////////////////////////////////////////////////////
//  Debug implementation
///////////////////////////////////////////////////////////////////////////////

		vprintf(pszFormat, Args);

	} else {
		switch (iLogLevel) {
		case (LOG_LEV_WARNING):
		case (LOG_LEV_ERROR):

			SysEventLogV(pszFormat, Args);

			break;
		}
	}

	va_end(Args);

	LeaveCriticalSection(&csLog);

	return (0);

}

void SysSleep(int iTimeout)
{

	SysMsSleep(iTimeout * 1000);

}

void SysMsSleep(int iMsTimeout)
{

	Sleep(iMsTimeout);

}

static void SysTimetToFileTime(time_t tTime, LPFILETIME pFT)
{

	LONGLONG llTime = Int32x32To64(tTime, 10000000) + 116444736000000000;

	pFT->dwLowDateTime = (DWORD) llTime;
	pFT->dwHighDateTime = (DWORD) (llTime >> 32);

}

static time_t SysFileTimeToTimet(LPFILETIME pFT)
{

	LONGLONG llTime = ((LONGLONG) pFT->dwLowDateTime) |
	    (((LONGLONG) pFT->dwHighDateTime) << 32);

	return ((time_t) ((llTime - 116444736000000000) / 10000000));

}

SYS_INT64 SysMsTime(void)
{

	LARGE_INTEGER PerfCntCurr;

	QueryPerformanceCounter(&PerfCntCurr);

	SYS_INT64 MsTicks = *(SYS_INT64 *) & PerfCntCurr;

	MsTicks -= PCSysStart;
	MsTicks /= PCFreq;
	MsTicks += (SYS_INT64) tSysStart *1000;

	return (MsTicks);

}

int SysExistFile(const char *pszFilePath)
{

	DWORD dwAttr = GetFileAttributes(pszFilePath);

	if (dwAttr == (DWORD) - 1)
		return (0);

	return ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1);

}

int SysExistDir(const char *pszDirPath)
{

	DWORD dwAttr = GetFileAttributes(pszDirPath);

	if (dwAttr == (DWORD) - 1)
		return (0);

	return ((dwAttr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0);

}

SYS_HANDLE SysFirstFile(const char *pszPath, char *pszFileName)
{

	char szMatch[SYS_MAX_PATH] = "";

	strcpy(szMatch, pszPath);
	AppendSlash(szMatch);
	strcat(szMatch, "*");

	WIN32_FIND_DATA WFD;
	HANDLE hFind = FindFirstFile(szMatch, &WFD);

	if (hFind == INVALID_HANDLE_VALUE)
		return (SYS_INVALID_HANDLE);

	FileFindData *pFFD = (FileFindData *) SysAlloc(sizeof(FileFindData));

	if (pFFD == NULL) {
		FindClose(hFind);
		return (SYS_INVALID_HANDLE);
	}

	strcpy(pFFD->szFindPath, pszPath);
	AppendSlash(pFFD->szFindPath);

	pFFD->hFind = hFind;
	pFFD->WFD = WFD;

	strcpy(pszFileName, pFFD->WFD.cFileName);

	return ((SYS_HANDLE) pFFD);

}

int SysIsDirectory(SYS_HANDLE hFind)
{

	FileFindData *pFFD = (FileFindData *) hFind;

	return ((pFFD->WFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0);

}

unsigned long SysGetSize(SYS_HANDLE hFind)
{

	FileFindData *pFFD = (FileFindData *) hFind;

	return ((unsigned long) pFFD->WFD.nFileSizeLow);

}

int SysNextFile(SYS_HANDLE hFind, char *pszFileName)
{

	FileFindData *pFFD = (FileFindData *) hFind;

	if (!FindNextFile(pFFD->hFind, &pFFD->WFD))
		return (0);

	strcpy(pszFileName, pFFD->WFD.cFileName);

	return (1);

}

void SysFindClose(SYS_HANDLE hFind)
{

	FileFindData *pFFD = (FileFindData *) hFind;

	FindClose(pFFD->hFind);

	SysFree(pFFD);

}

int SysGetFileInfo(char const *pszFileName, SYS_FILE_INFO & FI)
{

	WIN32_FIND_DATA WFD;
	HANDLE hFind = FindFirstFile(pszFileName, &WFD);

	if (hFind == INVALID_HANDLE_VALUE) {
		ErrSetErrorCode(ERR_STAT);
		return (ERR_STAT);
	}

	ZeroData(FI);
	FI.iFileType = (WFD.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? ftDirectory : ftNormal;
	FI.ulSize = (unsigned long) WFD.nFileSizeLow;
	FI.tMod = SysFileTimeToTimet(&WFD.ftLastWriteTime);

	FindClose(hFind);

	return (0);

}

int SysSetFileModTime(char const *pszFileName, time_t tMod)
{

	HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE,
				  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				  NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	if (hFile == INVALID_HANDLE_VALUE) {
		ErrSetErrorCode(ERR_SET_FILE_TIME);
		return (ERR_SET_FILE_TIME);
	}

	FILETIME MFT;

	SysTimetToFileTime(tMod, &MFT);

	if (!SetFileTime(hFile, NULL, &MFT, &MFT)) {
		CloseHandle(hFile);
		ErrSetErrorCode(ERR_SET_FILE_TIME);
		return (ERR_SET_FILE_TIME);
	}

	CloseHandle(hFile);

	return (0);

}

char *SysStrDup(const char *pszString)
{

	int iStrLength = strlen(pszString);
	char *pszBuffer = (char *) SysAlloc(iStrLength + 1);

	if (pszBuffer != NULL)
		strcpy(pszBuffer, pszString);

	return (pszBuffer);

}

char *SysGetEnv(const char *pszVarName)
{

	char szRKeyPath[256] = "";

	SysSNPrintf(szRKeyPath, sizeof(szRKeyPath) - 1, "SOFTWARE\\%s\\%s",
		    APP_PRODUCER, szServerName);

	HKEY hKey;

	if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, szRKeyPath, 0, KEY_QUERY_VALUE,
			 &hKey) == ERROR_SUCCESS) {
		char szKeyValue[2048] = "";
		DWORD dwSize = sizeof(szKeyValue);
		DWORD dwKeyType;

		if (RegQueryValueEx(hKey, pszVarName, NULL, &dwKeyType, (u_char *) szKeyValue,
				    &dwSize) == ERROR_SUCCESS) {
			RegCloseKey(hKey);

			return (SysStrDup(szKeyValue));
		}

		RegCloseKey(hKey);
	}

	const char *pszValue = getenv(pszVarName);

	return ((pszValue != NULL) ? SysStrDup(pszValue) : NULL);

}

char *SysGetTmpFile(char *pszFileName)
{

	char szTmpPath[SYS_MAX_PATH] = "";

	GetTempPath(sizeof(szTmpPath) - 1, szTmpPath);

	static unsigned int uFileSeqNr = 0;
	SYS_LONGLONG llFileID =
	    (((SYS_LONGLONG) GetCurrentThreadId()) << 32) | (SYS_LONGLONG)++ uFileSeqNr;

	SysSNPrintf(pszFileName, SYS_MAX_PATH - 1, "%smsrv%I64x.tmp", szTmpPath, llFileID);

	return (pszFileName);

}

int SysRemove(const char *pszFileName)
{

	if (!DeleteFile(pszFileName)) {
		ErrSetErrorCode(ERR_FILE_DELETE);
		return (ERR_FILE_DELETE);
	}

	return (0);

}

int SysMakeDir(const char *pszPath)
{

	if (!CreateDirectory(pszPath, NULL)) {
		ErrSetErrorCode(ERR_DIR_CREATE);
		return (ERR_DIR_CREATE);
	}

	return (0);

}

int SysRemoveDir(const char *pszPath)
{

	if (!RemoveDirectory(pszPath)) {
		ErrSetErrorCode(ERR_DIR_DELETE);
		return (ERR_DIR_DELETE);
	}

	return (0);

}

int SysMoveFile(char const *pszOldName, char const *pszNewName)
{

	if (!MoveFileEx(pszOldName, pszNewName,
			MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED)) {
		ErrSetErrorCode(ERR_FILE_MOVE);
		return (ERR_FILE_MOVE);
	}

	return (0);

}

int SysVSNPrintf(char *pszBuffer, int iSize, char const *pszFormat, va_list Args)
{

	return (_vsnprintf(pszBuffer, iSize, pszFormat, Args));

}

int SysFileSync(FILE * pFile)
{

	if (fflush(pFile) || _commit(_fileno(pFile))) {
		ErrSetErrorCode(ERR_FILE_WRITE);
		return (ERR_FILE_WRITE);
	}

	return (0);

}

char *SysStrTok(char *pszData, char const *pszDelim, char **ppszSavePtr)
{

	return (*ppszSavePtr = strtok(pszData, pszDelim));

}

char *SysCTime(time_t * pTimer, char *pszBuffer)
{

	return (strcpy(pszBuffer, ctime(pTimer)));

}

struct tm *SysLocalTime(time_t * pTimer, struct tm *pTStruct)
{

	*pTStruct = *localtime(pTimer);

	return (pTStruct);

}

struct tm *SysGMTime(time_t * pTimer, struct tm *pTStruct)
{

	*pTStruct = *gmtime(pTimer);

	return (pTStruct);

}

char *SysAscTime(struct tm *pTStruct, char *pszBuffer, int iBufferSize)
{

	strncpy(pszBuffer, asctime(pTStruct), iBufferSize);
	pszBuffer[iBufferSize - 1] = '\0';

	return (pszBuffer);

}

long SysGetTimeZone(void)
{

	return ((long) _timezone);

}

long SysGetDayLight(void)
{

	time_t tCurr = time(NULL);
	struct tm tmCurr = *localtime(&tCurr);

	return ((long) ((tmCurr.tm_isdst <= 0) ? 0 : 3600));

}

int SysGetDiskSpace(char const *pszPath, SYS_INT64 * pTotal, SYS_INT64 * pFree)
{

	ULARGE_INTEGER BytesAvail;
	ULARGE_INTEGER BytesOnDisk;
	ULARGE_INTEGER BytesFree;
	char szXPath[SYS_MAX_PATH] = "";

	StrSNCpy(szXPath, pszPath);
	AppendSlash(szXPath);

	if (!GetDiskFreeSpaceEx(szXPath, &BytesAvail, &BytesOnDisk, &BytesFree)) {
		ErrSetErrorCode(ERR_GET_DISK_SPACE_INFO);
		return (ERR_GET_DISK_SPACE_INFO);
	}

	*pTotal = *(SYS_INT64 *) & BytesOnDisk;

	*pFree = *(SYS_INT64 *) & BytesAvail;

	return (0);

}

int SysMemoryInfo(SYS_INT64 * pRamTotal, SYS_INT64 * pRamFree,
		  SYS_INT64 * pVirtTotal, SYS_INT64 * pVirtFree)
{

#if _WIN32_WINNT >= 0x0500

	MEMORYSTATUSEX MSEX;

	ZeroData(MSEX);

	if (!GlobalMemoryStatusEx(&MSEX)) {
		ErrSetErrorCode(ERR_GET_MEMORY_INFO);
		return (ERR_GET_MEMORY_INFO);
	}

	*pRamTotal = (SYS_INT64) MSEX.ullTotalPhys;

	*pRamFree = (SYS_INT64) MSEX.ullAvailPhys;

	*pVirtTotal = (SYS_INT64) MSEX.ullTotalVirtual;

	*pVirtFree = (SYS_INT64) MSEX.ullAvailVirtual;

#else				// #if defined(_WIN32_WINNT 0x0500)

	MEMORYSTATUS MS;

	ZeroData(MS);

	GlobalMemoryStatus(&MS);

	*pRamTotal = (SYS_INT64) MS.dwTotalPhys;

	*pRamFree = (SYS_INT64) MS.dwAvailPhys;

	*pVirtTotal = (SYS_INT64) MS.dwTotalVirtual;

	*pVirtFree = (SYS_INT64) MS.dwAvailVirtual;

#endif				// #if defined(_WIN32_WINNT 0x0500)

	return (0);

}

static unsigned int SysStkCall(unsigned int (*pProc) (void *), void *pData)
{

	srand(GetCurrentThreadId() * (unsigned int) time(NULL) * uSRandBase);

	unsigned int uResult;
	unsigned int uStkDisp =
	    (unsigned int) (rand() % MAX_STACK_SHIFT) & ~(STACK_ALIGN_BYTES - 1);

#if !defined(USE_ASM_STK_DISP)

	void *pStkSpace = _alloca(uStkDisp);

	uResult = pProc(pData);

#else

	__asm {
		sub esp, uStkDisp;
	}

	uResult = pProc(pData);

	__asm {
		add esp, uStkDisp;
	}

#endif

	return (uResult);

}
