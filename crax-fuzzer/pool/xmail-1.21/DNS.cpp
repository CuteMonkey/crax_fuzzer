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
#include "BuffSock.h"
#include "SList.h"
#include "MailConfig.h"
#include "MessQueue.h"
#include "MailSvr.h"
#include "MiscUtils.h"
#include "SvrUtils.h"
#include "DNS.h"

#define DNS_PORTNO              53
#define DNS_SOCKET_TIMEOUT      16
#define DNS_QUERY_EXTRA         512
#define DNS_MAX_RESP_PACKET     1024
#define DNS_SEND_RETRIES        3
#define DNS_MAX_RR_DATA         256
#define DNS_RESPDATA_EXTRA      (2 * sizeof(int))

#if defined(BIG_ENDIAN_CPU)
#define DNS_LABEL_LEN_MASK      0x3fff
#else				// #if defined(BIG_ENDIAN_CPU)
#define DNS_LABEL_LEN_MASK      0xff3f
#endif				// #if defined(BIG_ENDIAN_CPU)

#define DNS_LABEL_LEN_INVMASK   0xc0

#define ROOTS_FILE              "dnsroots"

struct DNSQuery {
	DNS_HEADER DNSH;
	SYS_UINT8 QueryData[DNS_QUERY_EXTRA];
};

struct DNSResourceRecord {
	char szName[MAX_HOST_NAME];
	SYS_UINT16 Type;
	SYS_UINT16 Class;
	SYS_UINT32 TTL;
	SYS_UINT16 Lenght;
	SYS_UINT8 const *pRespData;
};

struct DNSNameNode {
	LISTLINK LL;
	char *pszServer;
	char *pszQuery;
};

static SYS_UINT8 *DNS_AllocRespData(int iSize);
static int DNS_FreeRespData(SYS_UINT8 * pRespData);
static int DNS_RespDataSize(SYS_UINT8 const *pRespData);
static DNSNameNode *DNS_AllocNameNode(char const *pszServer, char const *pszQuery);
static void DNS_FreeNameNode(DNSNameNode * pDNSNN);
static void DNS_FreeNameList(HSLIST & hNameList);
static DNSNameNode *DNS_GetNameNode(HSLIST & hNameList, char const *pszServer,
				    char const *pszQuery);
static int DNS_AddNameNode(HSLIST & hNameList, char const *pszServer, char const *pszQuery);
static int DNS_GetResourceRecord(SYS_UINT8 const *pBaseData, SYS_UINT8 const *pRespData,
				 DNSResourceRecord * pRR = NULL, int *piRRLength = NULL);
static int DNS_GetName(SYS_UINT8 const *pBaseData, SYS_UINT8 const *pRespData,
		       char *pszInetName = NULL, int *piRRLength = NULL);
static int DNS_GetQuery(SYS_UINT8 const *pBaseData, SYS_UINT8 const *pRespData,
			char *pszInetName = NULL, SYS_UINT16 * pType = NULL,
			SYS_UINT16 * pClass = NULL, int *piRRLength = NULL);
static int DNS_NameCopy(SYS_UINT8 * pDNSQName, char const *pszInetName);
static SYS_UINT16 DNS_GetUniqueQueryId(void);
static int DNS_RequestSetup(DNSQuery & DNSQ, unsigned int uOpCode,
			    unsigned int uQType, char const *pszInetName,
			    int &iQueryLenght, bool bQueryRecursion = false);
static SYS_UINT8 *DNS_QueryExec(char const *pszDNSServer, int iPortNo, int iTimeout,
				unsigned int uOpCode, unsigned int uQType,
				char const *pszInetName, bool bQueryRecursion = false);
static SYS_UINT8 *DNS_QuerySendStream(char const *pszDNSServer, int iPortNo, int iTimeout,
				      DNSQuery const &DNSQ, int iQueryLenght);
static SYS_UINT8 *DNS_QuerySendDGram(char const *pszDNSServer, int iPortNo, int iTimeout,
				     DNSQuery const &DNSQ, int iQueryLenght, bool & bTruncated);
static int DNS_DecodeResponseMX(SYS_UINT8 * pRespData, char const *pszDomain,
				HSLIST & hNameList, char const *pszRespFile,
				char *pszCName, SYS_UINT32 * pTTL = NULL);
static int DNS_DecodeDirResponseMX(SYS_UINT8 * pRespData, char const *pszRespFile,
				   SYS_UINT32 * pTTL = NULL);
static int DNS_DecodeResponseNS(SYS_UINT8 * pRespData, char const *pszRespFile,
				bool & bAuth, char *pszCName, SYS_UINT32 * pTTL = NULL);
static int DNS_FindDomainMX(char const *pszDNSServer, char const *pszDomain,
			    HSLIST & hNameList, char const *pszRespFile, char *pszCName,
			    SYS_UINT32 * pTTL = NULL);
static int DNS_QueryDomainMX(char const *pszDNSServer, char const *pszDomain,
			     char *&pszMXDomains, char *pszCName, SYS_UINT32 * pTTL = NULL);
static int DNS_GetNameServersLL(char const *pszDNSServer, char const *pszDomain,
				char const *pszRespFile, HSLIST & hNameList,
				char *pszCName, SYS_UINT32 * pTTL = NULL);
static char *DNS_GetRootsFile(char *pszRootsFilePath, int iMaxPath);

static SYS_UINT8 *DNS_AllocRespData(int iSize)
{

	int iSizeExtra = DNS_RESPDATA_EXTRA;
	SYS_UINT8 *pBaseData = (SYS_UINT8 *) SysAlloc(iSize + iSizeExtra);

	if (pBaseData == NULL)
		return (NULL);

	((int *) pBaseData)[0] = iSize;

	return (pBaseData + iSizeExtra);

}

static int DNS_FreeRespData(SYS_UINT8 * pRespData)
{

	SYS_UINT8 *pBaseData = pRespData - DNS_RESPDATA_EXTRA;

	SysFree(pBaseData);

	return (0);

}

static int DNS_RespDataSize(SYS_UINT8 const *pRespData)
{

	SYS_UINT8 const *pBaseData = pRespData - DNS_RESPDATA_EXTRA;

	return (((int *) pBaseData)[0]);

}

static DNSNameNode *DNS_AllocNameNode(char const *pszServer, char const *pszQuery)
{

	DNSNameNode *pDNSNN = (DNSNameNode *) SysAlloc(sizeof(DNSNameNode));

	if (pDNSNN == NULL)
		return (NULL);

	ListLinkInit(pDNSNN);

	if ((pDNSNN->pszServer = SysStrDup(pszServer)) == NULL) {
		SysFree(pDNSNN);
		return (NULL);
	}

	if ((pDNSNN->pszQuery = SysStrDup(pszQuery)) == NULL) {
		SysFree(pDNSNN->pszServer);
		SysFree(pDNSNN);
		return (NULL);
	}

	return (pDNSNN);

}

static void DNS_FreeNameNode(DNSNameNode * pDNSNN)
{

	SysFree(pDNSNN->pszQuery);

	SysFree(pDNSNN->pszServer);

	SysFree(pDNSNN);

}

static void DNS_FreeNameList(HSLIST & hNameList)
{

	DNSNameNode *pDNSNN;

	while ((pDNSNN = (DNSNameNode *) ListRemove(hNameList)) != INVALID_SLIST_PTR)
		DNS_FreeNameNode(pDNSNN);

}

static DNSNameNode *DNS_GetNameNode(HSLIST & hNameList, char const *pszServer,
				    char const *pszQuery)
{

	DNSNameNode *pDNSNN = (DNSNameNode *) ListFirst(hNameList);

	for (; pDNSNN != INVALID_SLIST_PTR; pDNSNN = (DNSNameNode *)
	     ListNext(hNameList, (PLISTLINK) pDNSNN))
		if ((stricmp(pDNSNN->pszServer, pszServer) == 0) &&
		    (stricmp(pDNSNN->pszQuery, pszQuery) == 0))
			return (pDNSNN);

	return (NULL);

}

static int DNS_AddNameNode(HSLIST & hNameList, char const *pszServer, char const *pszQuery)
{

	DNSNameNode *pDNSNN = DNS_AllocNameNode(pszServer, pszQuery);

	if (pDNSNN == NULL)
		return (ErrGetErrorCode());

	ListAddTail(hNameList, (PLISTLINK) pDNSNN);

	return (0);

}

static int DNS_GetResourceRecord(SYS_UINT8 const *pBaseData, SYS_UINT8 const *pRespData,
				 DNSResourceRecord * pRR, int *piRRLength)
{

	if (pRR != NULL)
		ZeroData(*pRR);

///////////////////////////////////////////////////////////////////////////////
//  Read name field
///////////////////////////////////////////////////////////////////////////////
	int iRRLen = 0;
	char *pszName = (pRR != NULL) ? pRR->szName : NULL;

	if (DNS_GetName(pBaseData, pRespData, pszName, &iRRLen) < 0)
		return (ErrGetErrorCode());

	pRespData += iRRLen;

///////////////////////////////////////////////////////////////////////////////
//  Read type field
///////////////////////////////////////////////////////////////////////////////
	if (pRR != NULL)
		pRR->Type = ntohs(MscReadUint16(pRespData));

	pRespData += sizeof(SYS_UINT16);
	iRRLen += sizeof(SYS_UINT16);

///////////////////////////////////////////////////////////////////////////////
//  Read class field
///////////////////////////////////////////////////////////////////////////////
	if (pRR != NULL)
		pRR->Class = ntohs(MscReadUint16(pRespData));

	pRespData += sizeof(SYS_UINT16);
	iRRLen += sizeof(SYS_UINT16);

///////////////////////////////////////////////////////////////////////////////
//  Read TTL field
///////////////////////////////////////////////////////////////////////////////
	if (pRR != NULL)
		pRR->TTL = ntohl(MscReadUint32(pRespData));

	pRespData += sizeof(SYS_UINT32);
	iRRLen += sizeof(SYS_UINT32);

///////////////////////////////////////////////////////////////////////////////
//  Read lenght field
///////////////////////////////////////////////////////////////////////////////
	SYS_UINT16 Lenght = ntohs(MscReadUint16(pRespData));

	if (pRR != NULL)
		pRR->Lenght = Lenght;

	pRespData += sizeof(SYS_UINT16);
	iRRLen += sizeof(SYS_UINT16);

///////////////////////////////////////////////////////////////////////////////
//  Read RR data
///////////////////////////////////////////////////////////////////////////////
	if (pRR != NULL)
		pRR->pRespData = pRespData;

	iRRLen += (int) Lenght;

	if (piRRLength != NULL)
		*piRRLength = iRRLen;

	return (0);

}

static int DNS_GetName(SYS_UINT8 const *pBaseData, SYS_UINT8 const *pRespData,
		       char *pszInetName, int *piRRLength)
{

	char szNameBuffer[MAX_HOST_NAME] = "";

	if (pszInetName == NULL)
		pszInetName = szNameBuffer;

	int iBaseLength = DNS_RespDataSize(pBaseData);
	int iCurrOffset = (int) (pRespData - pBaseData);
	int iDataLength = 0;
	int iNameLength = 0;
	int iBackLink = 0;

	while (*pRespData != 0) {
		if (*pRespData & DNS_LABEL_LEN_INVMASK) {
///////////////////////////////////////////////////////////////////////////////
//  Got displacement from base-data ( boundary checked )
///////////////////////////////////////////////////////////////////////////////
			if ((iCurrOffset = (int)
			     ntohs(MscReadUint16(pRespData) & DNS_LABEL_LEN_MASK)) >= iBaseLength) {
				ErrSetErrorCode(ERR_BAD_DNS_NAME_RECORD);
				return (ERR_BAD_DNS_NAME_RECORD);
			}

			pRespData = pBaseData + iCurrOffset;

			if (!iBackLink)
				iDataLength += sizeof(SYS_UINT16), ++iBackLink;

			continue;
		}
///////////////////////////////////////////////////////////////////////////////
//  Extract label length ( boundary checked )
///////////////////////////////////////////////////////////////////////////////
		int iLabelLength = (int) *pRespData;

		if (((iNameLength + iLabelLength + 2) >= MAX_HOST_NAME) ||
		    ((iCurrOffset + iLabelLength + 1) >= iBaseLength)) {
			ErrSetErrorCode(ERR_BAD_DNS_NAME_RECORD);
			return (ERR_BAD_DNS_NAME_RECORD);
		}
///////////////////////////////////////////////////////////////////////////////
//  Append to name and update pointers
///////////////////////////////////////////////////////////////////////////////
		memcpy(pszInetName, pRespData + 1, iLabelLength);
		pszInetName[iLabelLength] = '.';

		iNameLength += iLabelLength + 1;
		pszInetName += iLabelLength + 1;

///////////////////////////////////////////////////////////////////////////////
//  If we've not got a back-jump, update the data length
///////////////////////////////////////////////////////////////////////////////
		if (!iBackLink)
			iDataLength += iLabelLength + 1;

///////////////////////////////////////////////////////////////////////////////
//  Move pointers
///////////////////////////////////////////////////////////////////////////////
		iCurrOffset += iLabelLength + 1;
		pRespData += iLabelLength + 1;
	}

	*pszInetName = '\0';

	if (piRRLength != NULL)
		*piRRLength = (!iBackLink) ? iDataLength + 1 : iDataLength;

	return (0);

}

static int DNS_GetQuery(SYS_UINT8 const *pBaseData, SYS_UINT8 const *pRespData,
			char *pszInetName, SYS_UINT16 * pType, SYS_UINT16 * pClass,
			int *piRRLength)
{

///////////////////////////////////////////////////////////////////////////////
//  Read name field
///////////////////////////////////////////////////////////////////////////////
	int iQueryLen = 0;

	if (DNS_GetName(pBaseData, pRespData, pszInetName, &iQueryLen) < 0)
		return (ErrGetErrorCode());

	pRespData += iQueryLen;

///////////////////////////////////////////////////////////////////////////////
//  Read type field
///////////////////////////////////////////////////////////////////////////////
	if (pType != NULL)
		*pType = ntohs(MscReadUint16(pRespData));

	pRespData += sizeof(SYS_UINT16);
	iQueryLen += sizeof(SYS_UINT16);

///////////////////////////////////////////////////////////////////////////////
//  Read class field
///////////////////////////////////////////////////////////////////////////////
	if (pClass != NULL)
		*pClass = ntohs(MscReadUint16(pRespData));

	pRespData += sizeof(SYS_UINT16);
	iQueryLen += sizeof(SYS_UINT16);

	if (piRRLength != NULL)
		*piRRLength = iQueryLen;

	return (0);

}

static int DNS_NameCopy(SYS_UINT8 * pDNSQName, char const *pszInetName)
{

	char *pszNameCopy = SysStrDup(pszInetName);

	if (pszNameCopy == NULL)
		return (ErrGetErrorCode());

	int iNameLen = 0;
	char *pszToken = NULL;
	char *pszSavePtr = NULL;

	pszToken = SysStrTok(pszNameCopy, ".", &pszSavePtr);

	while (pszToken != NULL) {
		int iTokLen = strlen(pszToken);

		*pDNSQName = (SYS_UINT8) iTokLen;

		strcpy((char *) pDNSQName + 1, pszToken);

		pDNSQName += iTokLen + 1;
		iNameLen += iTokLen + 1;

		pszToken = SysStrTok(NULL, ".", &pszSavePtr);
	}

	*pDNSQName = 0;

	SysFree(pszNameCopy);

	return (iNameLen + 1);

}

static SYS_UINT16 DNS_GetUniqueQueryId(void)
{

	static SYS_UINT16 uDnsQueryId = 0;

	return ((SYS_UINT16) (++uDnsQueryId * SysGetCurrentThreadId()));

}

static int DNS_RequestSetup(DNSQuery & DNSQ, unsigned int uOpCode,
			    unsigned int uQType, char const *pszInetName,
			    int &iQueryLenght, bool bQueryRecursion)
{

///////////////////////////////////////////////////////////////////////////////
//  Setup query header
///////////////////////////////////////////////////////////////////////////////
	ZeroData(DNSQ);
	DNSQ.DNSH.Id = DNS_GetUniqueQueryId();
	DNSQ.DNSH.QR = 0;
	DNSQ.DNSH.RD = (bQueryRecursion) ? 1 : 0;
	DNSQ.DNSH.OpCode = uOpCode;
	DNSQ.DNSH.QDCount = htons(1);
	DNSQ.DNSH.ANCount = htons(0);
	DNSQ.DNSH.NSCount = htons(0);
	DNSQ.DNSH.ARCount = htons(0);

	SYS_UINT8 *pQueryData = DNSQ.QueryData;

///////////////////////////////////////////////////////////////////////////////
//  Copy name to query
///////////////////////////////////////////////////////////////////////////////
	int iNameLen = DNS_NameCopy(pQueryData, pszInetName);

	if (iNameLen < 0)
		return (ErrGetErrorCode());

	pQueryData += iNameLen;

///////////////////////////////////////////////////////////////////////////////
//  Set query type
///////////////////////////////////////////////////////////////////////////////
	MscWriteUint16(pQueryData, (SYS_UINT16) htons(uQType));
	pQueryData += sizeof(SYS_UINT16);

///////////////////////////////////////////////////////////////////////////////
//  Set query class
///////////////////////////////////////////////////////////////////////////////
	MscWriteUint16(pQueryData, (SYS_UINT16) htons(QCLASS_IN));
	pQueryData += sizeof(SYS_UINT16);

	iQueryLenght = (int) (pQueryData - (SYS_UINT8 *) & DNSQ);

	return (0);

}

static SYS_UINT8 *DNS_QueryExec(char const *pszDNSServer, int iPortNo, int iTimeout,
				unsigned int uOpCode, unsigned int uQType,
				char const *pszInetName, bool bQueryRecursion)
{

	int iQueryLenght;
	DNSQuery DNSQ;

	if (DNS_RequestSetup(DNSQ, uOpCode, uQType, pszInetName, iQueryLenght,
			     bQueryRecursion) < 0)
		return (NULL);

	bool bTruncated = false;

	SYS_UINT8 *pRespData = DNS_QuerySendDGram(pszDNSServer, iPortNo, iTimeout,
						  DNSQ, iQueryLenght, bTruncated);

	if ((pRespData == NULL) && bTruncated)
		pRespData = DNS_QuerySendStream(pszDNSServer, iPortNo, iTimeout,
						DNSQ, iQueryLenght);

	return (pRespData);

}

static SYS_UINT8 *DNS_QuerySendStream(char const *pszDNSServer, int iPortNo, int iTimeout,
				      DNSQuery const &DNSQ, int iQueryLenght)
{

///////////////////////////////////////////////////////////////////////////////
//  Open DNS server socket
///////////////////////////////////////////////////////////////////////////////
	SYS_SOCKET SockFD;
	SYS_INET_ADDR SvrAddr;
	SYS_INET_ADDR SockAddr;

	if (MscCreateClientSocket(pszDNSServer, iPortNo, SOCK_STREAM, &SockFD, &SvrAddr,
				  &SockAddr, iTimeout) < 0)
		return (NULL);

	SYS_UINT16 QLenght = (SYS_UINT16) htons(iQueryLenght);

///////////////////////////////////////////////////////////////////////////////
//  Send packet lenght
///////////////////////////////////////////////////////////////////////////////
	if (SysSend(SockFD, (char *) &QLenght, sizeof(QLenght), iTimeout) != sizeof(QLenght)) {
		ErrorPush();
		SysCloseSocket(SockFD);
		ErrSetErrorCode(ErrorFetch());
		return (NULL);
	}
///////////////////////////////////////////////////////////////////////////////
//  Send packet
///////////////////////////////////////////////////////////////////////////////
	if (SysSend(SockFD, (char const *) &DNSQ, iQueryLenght, iTimeout) != iQueryLenght) {
		ErrorPush();
		SysCloseSocket(SockFD);
		ErrSetErrorCode(ErrorFetch());
		return (NULL);
	}
///////////////////////////////////////////////////////////////////////////////
//  Receive packet lenght
///////////////////////////////////////////////////////////////////////////////
	if (SysRecv(SockFD, (char *) &QLenght, sizeof(QLenght), iTimeout) != sizeof(QLenght)) {
		ErrorPush();
		SysCloseSocket(SockFD);
		ErrSetErrorCode(ErrorFetch());
		return (NULL);
	}

	int iPacketLenght = (int) ntohs(QLenght);

	SYS_UINT8 *pRespData = DNS_AllocRespData(iPacketLenght + 1);

	if (pRespData == NULL) {
		ErrorPush();
		SysCloseSocket(SockFD);
		ErrSetErrorCode(ErrorFetch());
		return (NULL);
	}
///////////////////////////////////////////////////////////////////////////////
//  Receive packet
///////////////////////////////////////////////////////////////////////////////
	if (SysRecv(SockFD, (char *) pRespData, iPacketLenght, iTimeout) != iPacketLenght) {
		ErrorPush();
		DNS_FreeRespData(pRespData);
		SysCloseSocket(SockFD);
		ErrSetErrorCode(ErrorFetch());
		return (NULL);
	}

	SysCloseSocket(SockFD);

	DNS_HEADER *pDNSH = (DNS_HEADER *) pRespData;

	if (DNSQ.DNSH.RD && !pDNSH->RA) {
		DNS_FreeRespData(pRespData);
		ErrSetErrorCode(ERR_DNS_RECURSION_NOT_AVAILABLE);
		return (NULL);
	}

	return (pRespData);

}

static SYS_UINT8 *DNS_QuerySendDGram(char const *pszDNSServer, int iPortNo, int iTimeout,
				     DNSQuery const &DNSQ, int iQueryLenght, bool & bTruncated)
{

	bTruncated = false;

///////////////////////////////////////////////////////////////////////////////
//  Open DNS server socket
///////////////////////////////////////////////////////////////////////////////
	SYS_SOCKET SockFD;
	SYS_INET_ADDR SvrAddr;
	SYS_INET_ADDR SockAddr;

	if (MscCreateClientSocket(pszDNSServer, iPortNo, SOCK_DGRAM, &SockFD, &SvrAddr,
				  &SockAddr, iTimeout) < 0)
		return (NULL);

///////////////////////////////////////////////////////////////////////////////
//  Query loop
///////////////////////////////////////////////////////////////////////////////
	for (int iSendLoops = 0; iSendLoops < DNS_SEND_RETRIES; iSendLoops++) {
///////////////////////////////////////////////////////////////////////////////
//  Send packet
///////////////////////////////////////////////////////////////////////////////
		if (SysSendData(SockFD, (char const *) &DNSQ, iQueryLenght, iTimeout) !=
		    iQueryLenght)
			continue;

///////////////////////////////////////////////////////////////////////////////
//  Receive packet lenght
///////////////////////////////////////////////////////////////////////////////
		SYS_INET_ADDR RecvAddr;
		SYS_UINT8 RespBuffer[1024];

		ZeroData(RecvAddr);

		int iPacketLenght =
		    SysRecvDataFrom(SockFD, (struct sockaddr *) &RecvAddr, sizeof(RecvAddr),
				    (char *) RespBuffer, sizeof(RespBuffer), iTimeout);

		if ((iPacketLenght < 0) || (iPacketLenght < sizeof(DNS_HEADER)))
			continue;

		DNS_HEADER *pDNSH = (DNS_HEADER *) RespBuffer;

		if (pDNSH->Id != DNSQ.DNSH.Id)
			continue;

		if (DNSQ.DNSH.RD && !pDNSH->RA) {
			SysCloseSocket(SockFD);
			ErrSetErrorCode(ERR_DNS_RECURSION_NOT_AVAILABLE);
			return (NULL);
		}

		if (pDNSH->TC) {
			bTruncated = true;

			SysCloseSocket(SockFD);
			ErrSetErrorCode(ERR_TRUNCATED_DGRAM_DNS_RESPONSE);
			return (NULL);
		}

		SYS_UINT8 *pRespData = DNS_AllocRespData(iPacketLenght + 1);

		if (pRespData == NULL) {
			ErrorPush();
			SysCloseSocket(SockFD);
			ErrSetErrorCode(ErrorFetch());
			return (NULL);
		}

		memcpy(pRespData, RespBuffer, iPacketLenght);

		SysCloseSocket(SockFD);

		return (pRespData);
	}

	SysCloseSocket(SockFD);

	ErrSetErrorCode(ERR_NO_DGRAM_DNS_RESPONSE);

	return (NULL);

}

static int DNS_DecodeResponseMX(SYS_UINT8 * pRespData, char const *pszDomain,
				HSLIST & hNameList, char const *pszRespFile,
				char *pszCName, SYS_UINT32 * pTTL)
{

	SYS_UINT8 *pBaseData = pRespData;
	DNSQuery *pDNSQ = (DNSQuery *) pRespData;

	if (pDNSQ->DNSH.RCode != 0) {
		int iErrorCode = ERR_BAD_DNS_RESPONSE;

		if (pDNSQ->DNSH.RCode == RCODE_NXDOMAIN)
			iErrorCode = ERR_DNS_NXDOMAIN;

		ErrSetErrorCode(iErrorCode);
		return (iErrorCode);
	}

	pDNSQ->DNSH.QDCount = ntohs(pDNSQ->DNSH.QDCount);
	pDNSQ->DNSH.ANCount = ntohs(pDNSQ->DNSH.ANCount);
	pDNSQ->DNSH.NSCount = ntohs(pDNSQ->DNSH.NSCount);
	pDNSQ->DNSH.ARCount = ntohs(pDNSQ->DNSH.ARCount);

	FILE *pMXFile = fopen(pszRespFile, "wb");

	if (pMXFile == NULL) {
		ErrSetErrorCode(ERR_FILE_CREATE, pszRespFile);
		return (ERR_FILE_CREATE);
	}

	pRespData = pDNSQ->QueryData;

///////////////////////////////////////////////////////////////////////////////
//  Scan query data
///////////////////////////////////////////////////////////////////////////////
	int ii;

	for (ii = 0; ii < (int) pDNSQ->DNSH.QDCount; ii++) {
		int iQLenght = 0;
		SYS_UINT16 Type = 0;
		SYS_UINT16 Class = 0;
		char szInetName[MAX_HOST_NAME] = "";

		if (DNS_GetQuery(pBaseData, pRespData, szInetName, &Type, &Class, &iQLenght) < 0) {
			ErrorPush();
			fclose(pMXFile);
			SysRemove(pszRespFile);
			return (ErrorPop());
		}

		pRespData += iQLenght;
	}

///////////////////////////////////////////////////////////////////////////////
//  Scan answer data
///////////////////////////////////////////////////////////////////////////////
	int iMXRecords = 0;
	SYS_UINT32 TTL = 0;

	for (ii = 0; ii < (int) pDNSQ->DNSH.ANCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0) {
			ErrorPush();
			fclose(pMXFile);
			SysRemove(pszRespFile);
			return (ErrorPop());
		}

		pRespData += iRRLenght;

		SYS_UINT8 const *pMXData = RR.pRespData;
		SYS_UINT16 Preference = ntohs(MscReadUint16(pMXData));

		pMXData += sizeof(SYS_UINT16);

		char szRRName[MAX_HOST_NAME] = "";

		if (DNS_GetName(pBaseData, pMXData, szRRName) < 0) {
			ErrorPush();
			fclose(pMXFile);
			SysRemove(pszRespFile);
			return (ErrorPop());
		}

		if (RR.Type == QTYPE_MX) {
			if (ii == 0)
				fprintf(pMXFile, "%d:%s", (int) Preference, szRRName);
			else
				fprintf(pMXFile, ",%d:%s", (int) Preference, szRRName);

			if ((TTL == 0) || (RR.TTL < TTL))
				TTL = RR.TTL;

			++iMXRecords;
		} else if (RR.Type == QTYPE_CNAME) {
			if (pszCName != NULL)
				strcpy(pszCName, szRRName);
			fclose(pMXFile);
			SysRemove(pszRespFile);

			ErrSetErrorCode(ERR_DNS_IS_CNAME);
			return (ERR_DNS_IS_CNAME);
		}
	}

	fclose(pMXFile);

///////////////////////////////////////////////////////////////////////////////
//  Got answers ?
///////////////////////////////////////////////////////////////////////////////
	if (iMXRecords) {
		if (pTTL != NULL)
			*pTTL = TTL;
		return (0);
	}

	SysRemove(pszRespFile);

///////////////////////////////////////////////////////////////////////////////
//  Scan name servers data
///////////////////////////////////////////////////////////////////////////////
	for (ii = 0; ii < (int) pDNSQ->DNSH.NSCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0)
			return (ErrGetErrorCode());

		pRespData += iRRLenght;

		SYS_UINT8 const *pNSData = RR.pRespData;

		char szNSName[MAX_HOST_NAME] = "";

		if (DNS_GetName(pBaseData, pNSData, szNSName) < 0)
			return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Recursively try authority name servers
///////////////////////////////////////////////////////////////////////////////
		if (DNS_GetNameNode(hNameList, szNSName, pszDomain) == NULL) {
			int iFindResult = DNS_FindDomainMX(szNSName, pszDomain,
							   hNameList, pszRespFile,
							   pszCName, pTTL);

			if ((iFindResult == 0) || (iFindResult == ERR_DNS_NXDOMAIN) ||
			    (iFindResult == ERR_DNS_IS_CNAME))
				return (iFindResult);
		}
	}

///////////////////////////////////////////////////////////////////////////////
//  Scan additional records data
///////////////////////////////////////////////////////////////////////////////
	for (ii = 0; ii < (int) pDNSQ->DNSH.ARCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0)
			return (ErrGetErrorCode());

		pRespData += iRRLenght;

	}

	ErrSetErrorCode(ERR_BAD_DNS_RESPONSE);
	return (ERR_BAD_DNS_RESPONSE);

}

static int DNS_DecodeDirResponseMX(SYS_UINT8 * pRespData, char const *pszRespFile,
				   SYS_UINT32 * pTTL)
{

	SYS_UINT8 *pBaseData = pRespData;
	DNSQuery *pDNSQ = (DNSQuery *) pRespData;

	if (pDNSQ->DNSH.RCode != 0) {
		int iErrorCode = ERR_BAD_DNS_RESPONSE;

		if (pDNSQ->DNSH.RCode == RCODE_NXDOMAIN)
			iErrorCode = ERR_DNS_NXDOMAIN;

		ErrSetErrorCode(iErrorCode);
		return (iErrorCode);
	}

	pDNSQ->DNSH.QDCount = ntohs(pDNSQ->DNSH.QDCount);
	pDNSQ->DNSH.ANCount = ntohs(pDNSQ->DNSH.ANCount);
	pDNSQ->DNSH.NSCount = ntohs(pDNSQ->DNSH.NSCount);
	pDNSQ->DNSH.ARCount = ntohs(pDNSQ->DNSH.ARCount);

	if (pDNSQ->DNSH.ANCount == 0) {
		ErrSetErrorCode(ERR_EMPTY_DNS_RESPONSE);
		return (ERR_EMPTY_DNS_RESPONSE);
	}

	FILE *pMXFile = fopen(pszRespFile, "wb");

	if (pMXFile == NULL) {
		ErrSetErrorCode(ERR_FILE_CREATE);
		return (ERR_FILE_CREATE);
	}

	pRespData = pDNSQ->QueryData;

///////////////////////////////////////////////////////////////////////////////
//  Scan query data
///////////////////////////////////////////////////////////////////////////////
	int ii;

	for (ii = 0; ii < (int) pDNSQ->DNSH.QDCount; ii++) {
		int iQLenght = 0;
		SYS_UINT16 Type = 0;
		SYS_UINT16 Class = 0;
		char szInetName[MAX_HOST_NAME] = "";

		if (DNS_GetQuery(pBaseData, pRespData, szInetName, &Type, &Class, &iQLenght) < 0) {
			ErrorPush();
			fclose(pMXFile);
			return (ErrorPop());
		}

		pRespData += iQLenght;
	}

///////////////////////////////////////////////////////////////////////////////
//  Scan answer data
///////////////////////////////////////////////////////////////////////////////
	int iMXRecords = 0;
	SYS_UINT32 TTL = 0;

	for (ii = 0; ii < (int) pDNSQ->DNSH.ANCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0) {
			ErrorPush();
			fclose(pMXFile);
			return (ErrorPop());
		}

		pRespData += iRRLenght;

		SYS_UINT8 const *pMXData = RR.pRespData;
		SYS_UINT16 Preference = ntohs(MscReadUint16(pMXData));

		pMXData += sizeof(SYS_UINT16);

		char szMXDomain[MAX_HOST_NAME] = "";

		if (DNS_GetName(pBaseData, pMXData, szMXDomain) < 0) {
			ErrorPush();
			fclose(pMXFile);
			return (ErrorPop());
		}

		if (RR.Type == QTYPE_MX) {
			if (ii == 0)
				fprintf(pMXFile, "%d:%s", (int) Preference, szMXDomain);
			else
				fprintf(pMXFile, ",%d:%s", (int) Preference, szMXDomain);

			if ((TTL == 0) || (RR.TTL < TTL))
				TTL = RR.TTL;

			++iMXRecords;
		}
	}

	fclose(pMXFile);

	if (iMXRecords == 0) {
		SysRemove(pszRespFile);
		ErrSetErrorCode(ERR_EMPTY_DNS_RESPONSE);
		return (ERR_EMPTY_DNS_RESPONSE);
	}

	if (pTTL != NULL)
		*pTTL = TTL;

	return (0);

}

static int DNS_DecodeResponseNS(SYS_UINT8 * pRespData, char const *pszRespFile,
				bool & bAuth, char *pszCName, SYS_UINT32 * pTTL)
{

	SYS_UINT8 *pBaseData = pRespData;
	DNSQuery *pDNSQ = (DNSQuery *) pRespData;

	if (pDNSQ->DNSH.RCode != 0) {
		int iErrorCode = ERR_BAD_DNS_RESPONSE;

		if (pDNSQ->DNSH.RCode == RCODE_NXDOMAIN)
			iErrorCode = ERR_DNS_NXDOMAIN;

		ErrSetErrorCode(iErrorCode);
		return (iErrorCode);
	}

	FILE *pNSFile = fopen(pszRespFile, "wt");

	if (pNSFile == NULL) {
		ErrSetErrorCode(ERR_FILE_CREATE, pszRespFile);
		return (ERR_FILE_CREATE);
	}

	pDNSQ->DNSH.QDCount = ntohs(pDNSQ->DNSH.QDCount);
	pDNSQ->DNSH.ANCount = ntohs(pDNSQ->DNSH.ANCount);
	pDNSQ->DNSH.NSCount = ntohs(pDNSQ->DNSH.NSCount);
	pDNSQ->DNSH.ARCount = ntohs(pDNSQ->DNSH.ARCount);

	pRespData = pDNSQ->QueryData;

///////////////////////////////////////////////////////////////////////////////
//  Scan query data
///////////////////////////////////////////////////////////////////////////////
	int ii;

	for (ii = 0; ii < (int) pDNSQ->DNSH.QDCount; ii++) {
		int iQLenght = 0;
		SYS_UINT16 Type = 0;
		SYS_UINT16 Class = 0;
		char szInetName[MAX_HOST_NAME] = "";

		if (DNS_GetQuery(pBaseData, pRespData, szInetName, &Type, &Class, &iQLenght) < 0) {
			ErrorPush();
			fclose(pNSFile);
			return (ErrorPop());
		}

		pRespData += iQLenght;
	}

///////////////////////////////////////////////////////////////////////////////
//  Scan answer data
///////////////////////////////////////////////////////////////////////////////
	int iNSRecords = 0;
	SYS_UINT32 TTL = 0;

	for (ii = 0; ii < (int) pDNSQ->DNSH.ANCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0) {
			ErrorPush();
			fclose(pNSFile);
			return (ErrorPop());
		}

		pRespData += iRRLenght;

		SYS_UINT8 const *pNSData = RR.pRespData;

		char szRRName[MAX_HOST_NAME] = "";

		if (DNS_GetName(pBaseData, pNSData, szRRName) < 0) {
			ErrorPush();
			fclose(pNSFile);
			return (ErrorPop());
		}

		if (RR.Type == QTYPE_NS) {
			fprintf(pNSFile, "%s\n", szRRName);

			if ((TTL == 0) || (RR.TTL < TTL))
				TTL = RR.TTL;

			++iNSRecords;
		} else if (RR.Type == QTYPE_CNAME) {
			if (pszCName != NULL)
				strcpy(pszCName, szRRName);
			fclose(pNSFile);

			ErrSetErrorCode(ERR_DNS_IS_CNAME);
			return (ERR_DNS_IS_CNAME);
		}
	}

///////////////////////////////////////////////////////////////////////////////
//  Got answers ?
///////////////////////////////////////////////////////////////////////////////
	if (iNSRecords > 0) {
		fclose(pNSFile);
		bAuth = true;
		if (pTTL != NULL)
			*pTTL = TTL;
		return (0);
	}
///////////////////////////////////////////////////////////////////////////////
//  Scan name servers data
///////////////////////////////////////////////////////////////////////////////
	for (ii = 0; ii < (int) pDNSQ->DNSH.NSCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0) {
			ErrorPush();
			fclose(pNSFile);
			return (ErrorPop());
		}

		pRespData += iRRLenght;

		SYS_UINT8 const *pNSData = RR.pRespData;

		char szRRName[MAX_HOST_NAME] = "";

		if (DNS_GetName(pBaseData, pNSData, szRRName) < 0) {
			ErrorPush();
			fclose(pNSFile);
			return (ErrorPop());
		}

		if (RR.Type == QTYPE_NS) {
			fprintf(pNSFile, "%s\n", szRRName);

			if ((TTL == 0) || (RR.TTL < TTL))
				TTL = RR.TTL;

			++iNSRecords;
		}
	}

///////////////////////////////////////////////////////////////////////////////
//  Got answers ?
///////////////////////////////////////////////////////////////////////////////
	if (iNSRecords > 0) {
		fclose(pNSFile);
		bAuth = false;
		if (pTTL != NULL)
			*pTTL = TTL;
		return (0);
	}
///////////////////////////////////////////////////////////////////////////////
//  Scan additional records data
///////////////////////////////////////////////////////////////////////////////
	for (ii = 0; ii < (int) pDNSQ->DNSH.ARCount; ii++) {
		int iRRLenght = 0;
		DNSResourceRecord RR;

		if (DNS_GetResourceRecord(pBaseData, pRespData, &RR, &iRRLenght) < 0) {
			ErrorPush();
			fclose(pNSFile);
			return (ErrorPop());
		}

		pRespData += iRRLenght;

	}

	fclose(pNSFile);

	ErrSetErrorCode(ERR_BAD_DNS_RESPONSE);
	return (ERR_BAD_DNS_RESPONSE);

}

static int DNS_FindDomainMX(char const *pszDNSServer, char const *pszDomain,
			    HSLIST & hNameList, char const *pszRespFile,
			    char *pszCName, SYS_UINT32 * pTTL)
{

///////////////////////////////////////////////////////////////////////////////
//  Send query and read result
///////////////////////////////////////////////////////////////////////////////
	SYS_UINT8 *pRespData = DNS_QueryExec(pszDNSServer, DNS_PORTNO, DNS_SOCKET_TIMEOUT,
					     0, QTYPE_MX, pszDomain);

	if (pRespData == NULL)
		return (ErrGetErrorCode());

///////////////////////////////////////////////////////////////////////////////
//  Add this server to the visited list
///////////////////////////////////////////////////////////////////////////////
	DNS_AddNameNode(hNameList, pszDNSServer, pszDomain);

///////////////////////////////////////////////////////////////////////////////
//  Decode server response ( recursive )
///////////////////////////////////////////////////////////////////////////////
	int iDecodeResult = DNS_DecodeResponseMX(pRespData, pszDomain,
						 hNameList, pszRespFile,
						 pszCName, pTTL);

	DNS_FreeRespData(pRespData);

	return (iDecodeResult);

}

static int DNS_QueryDomainMX(char const *pszDNSServer, char const *pszDomain,
			     char *&pszMXDomains, char *pszCName, SYS_UINT32 * pTTL)
{

	HSLIST hNameList;
	char szMXFileName[SYS_MAX_PATH] = "";

	SysGetTmpFile(szMXFileName);

	ListInit(hNameList);

	if (DNS_FindDomainMX(pszDNSServer, pszDomain, hNameList,
			     szMXFileName, pszCName, pTTL) < 0) {
		ErrorPush();
		DNS_FreeNameList(hNameList);
		SysRemove(szMXFileName);
		return (ErrorPop());
	}

	DNS_FreeNameList(hNameList);

	FILE *pMXFile = fopen(szMXFileName, "rb");

	if (pMXFile == NULL) {
		SysRemove(szMXFileName);

		ErrSetErrorCode(ERR_FILE_OPEN);
		return (ERR_FILE_OPEN);
	}

	fseek(pMXFile, 0, SEEK_END);

	unsigned int uFileSize = (unsigned int) ftell(pMXFile);

	if ((pszMXDomains = (char *) SysAlloc(uFileSize + 1)) == NULL) {
		fclose(pMXFile);
		SysRemove(szMXFileName);
		return (ErrGetErrorCode());
	}

	fseek(pMXFile, 0, SEEK_SET);

	fread(pszMXDomains, uFileSize, 1, pMXFile);
	pszMXDomains[uFileSize] = '\0';

	fclose(pMXFile);

	SysRemove(szMXFileName);

	return (0);

}

int DNS_QueryNameServers(char const *pszDNSServer, char const *pszDomain,
			 char const *pszRespFile, bool & bAuth, char *pszCName, SYS_UINT32 * pTTL)
{

///////////////////////////////////////////////////////////////////////////////
//  Send query and read result
///////////////////////////////////////////////////////////////////////////////
	SYS_UINT8 *pRespData = DNS_QueryExec(pszDNSServer, DNS_PORTNO, DNS_SOCKET_TIMEOUT,
					     0, QTYPE_NS, pszDomain);

	if (pRespData == NULL)
		return (ErrGetErrorCode());

	if (DNS_DecodeResponseNS(pRespData, pszRespFile, bAuth, pszCName, pTTL) < 0) {
		ErrorPush();
		DNS_FreeRespData(pRespData);
		return (ErrorPop());
	}

	DNS_FreeRespData(pRespData);

	return (0);

}

static int DNS_GetNameServersLL(char const *pszDNSServer, char const *pszDomain,
				char const *pszRespFile, HSLIST & hNameList,
				char *pszCName, SYS_UINT32 * pTTL)
{

	char szRespFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szRespFile);

	bool bAuth = false;

	if (DNS_QueryNameServers(pszDNSServer, pszDomain, szRespFile, bAuth, pszCName, pTTL) < 0) {
		ErrorPush();
		CheckRemoveFile(szRespFile);

		return (ErrorPop());
	}

	if (bAuth) {
		if (MscCopyFile(pszRespFile, szRespFile) < 0) {
			ErrorPush();
			SysRemove(szRespFile);

			return (ErrorPop());
		}

		SysRemove(szRespFile);

		return (0);
	}
///////////////////////////////////////////////////////////////////////////////
//  Add this server to the visited list
///////////////////////////////////////////////////////////////////////////////
	DNS_AddNameNode(hNameList, pszDNSServer, pszDomain);

	FILE *pNSFile = fopen(szRespFile, "rt");

	if (pNSFile == NULL) {
		SysRemove(szRespFile);

		ErrSetErrorCode(ERR_FILE_OPEN);
		return (ERR_FILE_OPEN);
	}

	char szNS[MAX_HOST_NAME] = "";

	while (MscFGets(szNS, sizeof(szNS) - 1, pNSFile) != NULL) {
		if (DNS_GetNameNode(hNameList, szNS, pszDomain) == NULL) {
			int iQueryResult = DNS_GetNameServersLL(szNS, pszDomain,
								pszRespFile, hNameList,
								pszCName, pTTL);

			if ((iQueryResult == 0) || (iQueryResult == ERR_DNS_NXDOMAIN) ||
			    (iQueryResult == ERR_DNS_IS_CNAME)) {
				fclose(pNSFile);
				SysRemove(szRespFile);
				return (iQueryResult);
			}
		}
	}

	fclose(pNSFile);

	SysRemove(szRespFile);

	ErrSetErrorCode(ERR_NS_NOT_FOUND);
	return (ERR_NS_NOT_FOUND);

}

int DNS_GetNameServers(char const *pszDNSServer, char const *pszDomain,
		       char const *pszRespFile, char *pszCName, SYS_UINT32 * pTTL)
{

	HSLIST hNameList;

	ListInit(hNameList);

	int iQueryResult = DNS_GetNameServersLL(pszDNSServer, pszDomain,
						pszRespFile, hNameList, pszCName, pTTL);

	DNS_FreeNameList(hNameList);

	return (iQueryResult);

}

int DNS_DomainNameServers(char const *pszDomain, char const *pszRespFile,
			  char *pszCName, SYS_UINT32 * pTTL)
{

	char szRootsFile[SYS_MAX_PATH] = "";
	char szRespFile[SYS_MAX_PATH] = "";

	DNS_GetRootsFile(szRootsFile, sizeof(szRootsFile));
	StrSNCpy(szRespFile, szRootsFile);

	char **ppszDomains = StrTokenize(pszDomain, ". \t");

	if (ppszDomains == NULL)
		return (ErrGetErrorCode());

	int iSubDomains = StrStringsCount(ppszDomains);
	int iLastNSResult = ERR_DNS_NXDOMAIN;
	char szPrevDomain[MAX_HOST_NAME] = "";

	for (--iSubDomains; iSubDomains >= 0; iSubDomains--) {
		char szCurrDomain[MAX_HOST_NAME] = "";

		SysSNPrintf(szCurrDomain, sizeof(szCurrDomain) - 1, "%s.%s",
			    ppszDomains[iSubDomains], szPrevDomain);
		StrSNCpy(szPrevDomain, szCurrDomain);

		FILE *pNSFile = fopen(szRespFile, "rt");

		if (pNSFile == NULL) {
			StrFreeStrings(ppszDomains);
			if (strcmp(szRespFile, szRootsFile) != 0)
				SysRemove(szRespFile);

			ErrSetErrorCode(ERR_FILE_OPEN);
			return (ERR_FILE_OPEN);
		}

		char szRespFile2[SYS_MAX_PATH] = "";

		SysGetTmpFile(szRespFile2);

		int iNSGetResult = -1;
		char szNS[MAX_HOST_NAME] = "";

		while (MscFGets(szNS, sizeof(szNS) - 1, pNSFile) != NULL) {
			iNSGetResult = DNS_GetNameServers(szNS, szCurrDomain,
							  szRespFile2, pszCName, pTTL);

			if ((iNSGetResult == 0) || (iNSGetResult == ERR_DNS_NXDOMAIN))
				break;

			if (iNSGetResult == ERR_DNS_IS_CNAME) {
				fclose(pNSFile);
				StrFreeStrings(ppszDomains);
				if (strcmp(szRespFile, szRootsFile) != 0)
					SysRemove(szRespFile);
				ErrSetErrorCode(iNSGetResult);
				return (iNSGetResult);
			}
		}

		fclose(pNSFile);

///////////////////////////////////////////////////////////////////////////////
//  Not getting a valid response at level N does not mean that we will not
//  succeed at level N+1. It might happen that mid-level subdomains do not
//  have NS records while low-level will do
///////////////////////////////////////////////////////////////////////////////
		if ((iLastNSResult = iNSGetResult) < 0)
			SysRemove(szRespFile2);
		else {
			if (strcmp(szRespFile, szRootsFile) != 0)
				SysRemove(szRespFile);

			StrSNCpy(szRespFile, szRespFile2);
		}
	}

	StrFreeStrings(ppszDomains);

///////////////////////////////////////////////////////////////////////////////
//  If we did not get any valid response walking down the hierarchy we bounce
//  back with the latest error that we received
///////////////////////////////////////////////////////////////////////////////
	if (strcmp(szRespFile, szRootsFile) == 0) {
		ErrSetErrorCode(iLastNSResult);
		return (iLastNSResult);
	}

	if (MscCopyFile(pszRespFile, szRespFile) < 0) {
		ErrorPush();
		SysRemove(szRespFile);
		return (ErrorPop());
	}

	SysRemove(szRespFile);

	return (0);

}

static char *DNS_GetRootsFile(char *pszRootsFilePath, int iMaxPath)
{

	CfgGetRootPath(pszRootsFilePath, iMaxPath);

	StrNCat(pszRootsFilePath, ROOTS_FILE, iMaxPath);

	return (pszRootsFilePath);

}

int DNS_GetRoots(char const *pszDNSServer, char const *pszRespFile)
{

	return (DNS_GetNameServers(pszDNSServer, ".", pszRespFile, NULL));

}

int DNS_GetDomainMX(char const *pszDomain, char *&pszMXDomains, char *pszCName, SYS_UINT32 * pTTL)
{

	char szRespFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szRespFile);

	if (DNS_DomainNameServers(pszDomain, szRespFile, pszCName) < 0) {
		ErrorPush();
		SysRemove(szRespFile);

		return (ErrorPop());
	}

	FILE *pNSFile = fopen(szRespFile, "rt");

	if (pNSFile == NULL) {
		SysRemove(szRespFile);

		ErrSetErrorCode(ERR_FILE_OPEN);
		return (ERR_FILE_OPEN);
	}

	char szNS[MAX_HOST_NAME] = "";

	while (MscFGets(szNS, sizeof(szNS) - 1, pNSFile) != NULL) {
		int iQueryResult = DNS_QueryDomainMX(szNS, pszDomain, pszMXDomains,
						     pszCName, pTTL);

		if ((iQueryResult == 0) || (iQueryResult == ERR_DNS_NXDOMAIN) ||
		    (iQueryResult == ERR_DNS_IS_CNAME)) {
			fclose(pNSFile);
			SysRemove(szRespFile);

			return (iQueryResult);
		}
	}

	fclose(pNSFile);

	SysRemove(szRespFile);

	ErrSetErrorCode(ERR_NO_DEFINED_MXS_FOR_DOMAIN);
	return (ERR_NO_DEFINED_MXS_FOR_DOMAIN);

}

int DNS_GetDomainMXDirect(char const *pszDNSServer, char const *pszDomain,
			  int iQuerySockType, char *&pszMXDomains, SYS_UINT32 * pTTL)
{

///////////////////////////////////////////////////////////////////////////////
//  Setup DNS MX query with recursion requested
///////////////////////////////////////////////////////////////////////////////
	int iQueryLenght;
	DNSQuery DNSQ;

	if (DNS_RequestSetup(DNSQ, 0, QTYPE_MX, pszDomain, iQueryLenght, true) < 0)
		return (ErrGetErrorCode());

	SYS_UINT8 *pRespData = NULL;

	switch (iQuerySockType) {
	case (DNS_QUERY_TCP):
		{

			if ((pRespData =
			     DNS_QuerySendStream(pszDNSServer, DNS_PORTNO, DNS_SOCKET_TIMEOUT,
						 DNSQ, iQueryLenght)) == NULL)
				return (ErrGetErrorCode());

		}
		break;

	case (DNS_QUERY_UDP):
	default:
		{
///////////////////////////////////////////////////////////////////////////////
//  Try needed UDP query first, if it's truncated switch to TCP query
///////////////////////////////////////////////////////////////////////////////
			bool bTruncated = false;

			if (((pRespData =
			      DNS_QuerySendDGram(pszDNSServer, DNS_PORTNO, DNS_SOCKET_TIMEOUT,
						 DNSQ, iQueryLenght, bTruncated)) == NULL) &&
			    bTruncated)
				pRespData =
				    DNS_QuerySendStream(pszDNSServer, DNS_PORTNO,
							DNS_SOCKET_TIMEOUT, DNSQ, iQueryLenght);

			if (pRespData == NULL)
				return (ErrGetErrorCode());

		}
		break;
	}

	char szRespFile[SYS_MAX_PATH] = "";

	SysGetTmpFile(szRespFile);

	if (DNS_DecodeDirResponseMX(pRespData, szRespFile, pTTL) < 0) {
		ErrorPush();
		DNS_FreeRespData(pRespData);
		CheckRemoveFile(szRespFile);

		return (ErrorPop());
	}

	DNS_FreeRespData(pRespData);

	FILE *pMXFile = fopen(szRespFile, "rb");

	if (pMXFile == NULL) {
		ErrorPush();
		SysRemove(szRespFile);

		return (ErrorPop());
	}

	fseek(pMXFile, 0, SEEK_END);

	unsigned int uFileSize = (unsigned int) ftell(pMXFile);

	if ((pszMXDomains = (char *) SysAlloc(uFileSize + 1)) == NULL) {
		ErrorPush();
		fclose(pMXFile);
		SysRemove(szRespFile);

		return (ErrorPop());
	}

	fseek(pMXFile, 0, SEEK_SET);

	fread(pszMXDomains, uFileSize, 1, pMXFile);
	pszMXDomains[uFileSize] = '\0';

	fclose(pMXFile);
	SysRemove(szRespFile);

	return (0);

}
