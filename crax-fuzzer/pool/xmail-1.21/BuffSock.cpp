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
#include "StrUtils.h"
#include "BuffSock.h"

#define BSOCK_EOF                   INT_MIN

struct BuffSocketData {
	SYS_SOCKET SockFD;
	int iBufferSize;
	char *pszBuffer;
	int iBytesInBuffer;
	int iReadIndex;
};

static int BSckFetchData(BuffSocketData * pBSD, int iTimeout);

BSOCK_HANDLE BSckAttach(SYS_SOCKET SockFD, int iBufferSize)
{

	BuffSocketData *pBSD = (BuffSocketData *) SysAlloc(sizeof(BuffSocketData));

	if (pBSD == NULL)
		return (INVALID_BSOCK_HANDLE);

	char *pszBuffer = (char *) SysAlloc(iBufferSize);

	if (pszBuffer == NULL) {
		SysFree(pBSD);
		return (INVALID_BSOCK_HANDLE);
	}

	pBSD->SockFD = SockFD;
	pBSD->iBufferSize = iBufferSize;
	pBSD->pszBuffer = pszBuffer;
	pBSD->iBytesInBuffer = 0;
	pBSD->iReadIndex = 0;

	return ((BSOCK_HANDLE) pBSD);

}

SYS_SOCKET BSckDetach(BSOCK_HANDLE hBSock, int iCloseSocket)
{

	BuffSocketData *pBSD = (BuffSocketData *) hBSock;
	SYS_SOCKET SockFD = pBSD->SockFD;

	SysFree(pBSD->pszBuffer);

	SysFree(pBSD);

	if (iCloseSocket) {
		SysCloseSocket(SockFD);
		return (SYS_INVALID_SOCKET);
	}

	return (SockFD);

}

static int BSckFetchData(BuffSocketData * pBSD, int iTimeout)
{

	int iReadedBytes;

	pBSD->iReadIndex = 0;

	if ((iReadedBytes = SysRecvData(pBSD->SockFD, pBSD->pszBuffer,
					pBSD->iBufferSize, iTimeout)) <= 0) {
		ErrSetErrorCode(ERR_SOCK_NOMORE_DATA);
		return (iReadedBytes);
	}

	pBSD->iBytesInBuffer = iReadedBytes;

	return (iReadedBytes);

}

int BSckGetChar(BSOCK_HANDLE hBSock, int iTimeout)
{

	BuffSocketData *pBSD = (BuffSocketData *) hBSock;

	if ((pBSD->iBytesInBuffer == 0) && (BSckFetchData(pBSD, iTimeout) <= 0))
		return (BSOCK_EOF);

	int iChar = (int) pBSD->pszBuffer[pBSD->iReadIndex];

	pBSD->iReadIndex = INext(pBSD->iReadIndex, pBSD->iBufferSize);

	--pBSD->iBytesInBuffer;

	return (iChar);

}

char *BSckChGetString(BSOCK_HANDLE hBSock, char *pszBuffer, int iMaxChars, int iTimeout,
		      int *pLineLength, int *piGotNL)
{

	int ii;

	for (ii = 0, iMaxChars--; ii < iMaxChars; ii++) {
		int iChar = BSckGetChar(hBSock, iTimeout);

		if (iChar == BSOCK_EOF)
			return (NULL);

		if (iChar == '\n') {
			for (; (ii > 0) && (pszBuffer[ii - 1] == '\r'); ii--);

			pszBuffer[ii] = '\0';

			if (pLineLength != NULL)
				*pLineLength = ii;

			if (piGotNL != NULL)
				*piGotNL = 1;

			return (pszBuffer);
		} else
			pszBuffer[ii] = (char) iChar;

	}

	pszBuffer[ii] = '\0';

	if (pLineLength != NULL)
		*pLineLength = ii;

	if (piGotNL != NULL) {
		*piGotNL = 0;
		return (pszBuffer);
	}

	ErrSetErrorCode(ERR_LINE_TOO_LONG);

	return (NULL);

}

char *BSckGetString(BSOCK_HANDLE hBSock, char *pszBuffer, int iMaxChars, int iTimeout,
		    int *pLineLength, int *piGotNL)
{

	int ii;
	BuffSocketData *pBSD = (BuffSocketData *) hBSock;

	for (ii = 0, iMaxChars--; ii < iMaxChars;) {
///////////////////////////////////////////////////////////////////////////////
//  Verify to have something to read
///////////////////////////////////////////////////////////////////////////////
		if ((pBSD->iBytesInBuffer == 0) && (BSckFetchData(pBSD, iTimeout) <= 0))
			return (NULL);

		int iBytesLookup = Min(pBSD->iBytesInBuffer, iMaxChars - ii);

		if (iBytesLookup > 0) {
			char *pszNL = (char *) memchr(pBSD->pszBuffer + pBSD->iReadIndex, '\n',
						      iBytesLookup);

			if (pszNL != NULL) {
				int iCopySize =
				    (int) (pszNL - (pBSD->pszBuffer + pBSD->iReadIndex));

				memcpy(pszBuffer + ii, pBSD->pszBuffer + pBSD->iReadIndex,
				       iCopySize);
				ii += iCopySize;

				pBSD->iReadIndex += iCopySize + 1;

				pBSD->iBytesInBuffer -= iCopySize + 1;

///////////////////////////////////////////////////////////////////////////////
//  Line cleanup
///////////////////////////////////////////////////////////////////////////////
				for (; (ii > 0) && (pszBuffer[ii - 1] == '\r'); ii--);

				pszBuffer[ii] = '\0';

				if (pLineLength != NULL)
					*pLineLength = ii;

				if (piGotNL != NULL)
					*piGotNL = 1;

				return (pszBuffer);
			} else {
				memcpy(pszBuffer + ii, pBSD->pszBuffer + pBSD->iReadIndex,
				       iBytesLookup);
				ii += iBytesLookup;

				pBSD->iReadIndex += iBytesLookup;

				pBSD->iBytesInBuffer -= iBytesLookup;
			}
		}
	}

	pszBuffer[ii] = '\0';

	if (pLineLength != NULL)
		*pLineLength = ii;

	if (piGotNL != NULL) {
		*piGotNL = 0;
		return (pszBuffer);
	}

	ErrSetErrorCode(ERR_LINE_TOO_LONG);

	return (NULL);

}

int BSckSendString(BSOCK_HANDLE hBSock, char const *pszBuffer, int iTimeout)
{

	BuffSocketData *pBSD = (BuffSocketData *) hBSock;
	char *pszSendBuffer = (char *) SysAlloc(strlen(pszBuffer) + 3);

	if (pszSendBuffer == NULL)
		return (ErrGetErrorCode());

	sprintf(pszSendBuffer, "%s\r\n", pszBuffer);

	int iSendLength = strlen(pszSendBuffer);

	if (SysSend(pBSD->SockFD, pszSendBuffer, iSendLength, iTimeout) != iSendLength) {
		SysFree(pszSendBuffer);
		return (ErrGetErrorCode());
	}

	SysFree(pszSendBuffer);

	return (iSendLength);

}

int BSckVSendString(BSOCK_HANDLE hBSock, int iTimeout, char const *pszFormat, ...)
{

	char *pszBuffer = NULL;

	STRSPRINTF(pszBuffer, pszFormat, pszFormat);

	if (pszBuffer == NULL)
		return (ErrGetErrorCode());

	if (BSckSendString(hBSock, pszBuffer, iTimeout) < 0) {
		ErrorPush();
		SysFree(pszBuffer);
		return (ErrorPop());
	}

	SysFree(pszBuffer);

	return (0);

}

int BSckSendData(BSOCK_HANDLE hBSock, char const *pszBuffer, int iSize, int iTimeout)
{

	BuffSocketData *pBSD = (BuffSocketData *) hBSock;

	if (SysSend(pBSD->SockFD, pszBuffer, iSize, iTimeout) != iSize)
		return (ErrGetErrorCode());

	return (iSize);

}

int BSckReadData(BSOCK_HANDLE hBSock, char *pszBuffer, int iSize, int iTimeout)
{

	BuffSocketData *pBSD = (BuffSocketData *) hBSock;
	int iReadedBytes = 0;
	int iReadFromBuffer = Min(iSize, pBSD->iBytesInBuffer);

	if (iReadFromBuffer > 0) {
		memcpy(pszBuffer, pBSD->pszBuffer + pBSD->iReadIndex, iReadFromBuffer);

		pBSD->iReadIndex += iReadFromBuffer;

		pBSD->iBytesInBuffer -= iReadFromBuffer;

		iReadedBytes = iReadFromBuffer;
	}

	if ((iReadedBytes < iSize) &&
	    (SysRecv(pBSD->SockFD, pszBuffer + iReadedBytes, iSize - iReadedBytes,
		     iTimeout) != (iSize - iReadedBytes)))
		return (ErrGetErrorCode());

	return (iSize);

}

SYS_SOCKET BSckGetAttachedSocket(BSOCK_HANDLE hBSock)
{

	BuffSocketData *pBSD = (BuffSocketData *) hBSock;

	return (pBSD->SockFD);

}
