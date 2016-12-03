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

#define INVCHAR             '^'
#define LoChar(c)           ((((c) >= 'A') && ((c) <= 'Z')) ? ((c) - 'A' + 'a'): (c))
#define MIN_DYNSTR_INCR     256

int StrCmdLineToken(char const *&pszCmdLine, char *pszToken)
{

	char const *pszCurr = pszCmdLine;

	for (; (*pszCurr == ' ') || (*pszCurr == '\t'); pszCurr++);

	if (*pszCurr == '\0')
		return (ERR_NO_MORE_TOKENS);

	if (*pszCurr == '"') {
		++pszCurr;

		for (; *pszCurr != '\0';) {
			if (*pszCurr == '"') {
				++pszCurr;

				if (*pszCurr != '"')
					break;

				*pszToken++ = *pszCurr++;
			} else
				*pszToken++ = *pszCurr++;
		}

		*pszToken = '\0';
	} else {
		for (; (*pszCurr != ' ') && (*pszCurr != '\t') && (*pszCurr != '\0');)
			*pszToken++ = *pszCurr++;

		*pszToken = '\0';
	}

	pszCmdLine = pszCurr;

	return (0);

}

char **StrGetArgs(char const *pszCmdLine, int &iArgsCount)
{

	char const *pszCLine = pszCmdLine;
	char szToken[1024] = "";

	for (iArgsCount = 0; StrCmdLineToken(pszCLine, szToken) == 0; iArgsCount++);

	char **ppszArgs = (char **) SysAlloc((iArgsCount + 1) * sizeof(char *));

	if (ppszArgs == NULL)
		return (NULL);

	int ii = 0;

	for (pszCLine = pszCmdLine;
	     (ii < iArgsCount) && (StrCmdLineToken(pszCLine, szToken) == 0); ii++)
		ppszArgs[ii] = SysStrDup(szToken);

	ppszArgs[ii] = NULL;

	return (ppszArgs);

}

char *StrLower(char *pszString)
{

	char *pszCurr = pszString;

	for (; *pszCurr != '\0'; pszCurr++)
		if ((*pszCurr >= 'A') && (*pszCurr <= 'Z'))
			*pszCurr = 'a' + (*pszCurr - 'A');

	return (pszString);

}

char *StrUpper(char *pszString)
{

	char *pszCurr = pszString;

	for (; *pszCurr != '\0'; pszCurr++)
		if ((*pszCurr >= 'a') && (*pszCurr <= 'z'))
			*pszCurr = 'A' + (*pszCurr - 'a');

	return (pszString);

}

char *StrCrypt(char const *pszString, char *pszCrypt)
{

	SetEmptyString(pszCrypt);

	for (int ii = 0; pszString[ii] != '\0'; ii++) {
		unsigned int uChar = (unsigned int) pszString[ii];
		char szByte[32] = "";

		sprintf(szByte, "%02x", (uChar ^ 101) & 0xff);

		strcat(pszCrypt, szByte);
	}

	return (pszCrypt);

}

char *StrDeCrypt(char const *pszString, char *pszDeCrypt)
{

	int iStrLength = strlen(pszString);

	SetEmptyString(pszDeCrypt);

	if ((iStrLength % 2) != 0)
		return (NULL);

	int ii;

	for (ii = 0; ii < iStrLength; ii += 2) {
		char szByte[8] = "";

		szByte[0] = pszString[ii];
		szByte[1] = pszString[ii + 1];
		szByte[2] = '\0';

		unsigned int uChar = 0;

		if (sscanf(szByte, "%x", &uChar) != 1)
			return (NULL);

		pszDeCrypt[ii >> 1] = (char) ((uChar ^ 101) & 0xff);
	}

	pszDeCrypt[ii >> 1] = '\0';

	return (pszDeCrypt);

}

char **StrBuildList(char const *pszString, ...)
{

	int iNumString = 1;
	char const *pszArg = NULL;
	va_list Args;

	va_start(Args, pszString);

	while ((pszArg = va_arg(Args, char *)) != NULL)
		++iNumString;

	va_end(Args);

	int iStrCurr = 0;
	char **ppszStrings = (char **) SysAlloc((iNumString + 1) * sizeof(char *));

	if (ppszStrings == NULL)
		return (NULL);

	ppszStrings[iStrCurr++] = SysStrDup(pszString);

	va_start(Args, pszString);

	while ((pszArg = va_arg(Args, char *)) != NULL)
		 ppszStrings[iStrCurr++] = SysStrDup(pszArg);

	va_end(Args);

	ppszStrings[iStrCurr] = NULL;

	return (ppszStrings);

}

char **StrTokenize(const char *pszString, const char *pszTokenizer)
{

	char *pszBuffer = SysStrDup(pszString);

	if (pszBuffer == NULL)
		return (NULL);

	int iTokenCount = 0;
	char *pszToken = NULL;
	char *pszSavePtr = NULL;

	pszToken = SysStrTok(pszBuffer, pszTokenizer, &pszSavePtr);

	while (pszToken != NULL) {
		++iTokenCount;

		pszToken = SysStrTok(NULL, pszTokenizer, &pszSavePtr);
	}

	char **ppszTokens = (char **) SysAlloc((iTokenCount + 1) * sizeof(char *));

	if (ppszTokens == NULL) {
		SysFree(pszBuffer);
		return (NULL);
	}

	strcpy(pszBuffer, pszString);

	iTokenCount = 0;
	pszToken = SysStrTok(pszBuffer, pszTokenizer, &pszSavePtr);

	while (pszToken != NULL) {
		ppszTokens[iTokenCount++] = SysStrDup(pszToken);

		pszToken = SysStrTok(NULL, pszTokenizer, &pszSavePtr);
	}

	ppszTokens[iTokenCount] = NULL;

	SysFree(pszBuffer);

	return (ppszTokens);

}

void StrFreeStrings(char **ppszStrings)
{

	for (int ii = 0; ppszStrings[ii] != NULL; ii++)
		SysFree(ppszStrings[ii]);

	SysFree(ppszStrings);

}

int StrStringsCount(char const *const *ppszStrings)
{

	int ii;

	for (ii = 0; ppszStrings[ii] != NULL; ii++);

	return (ii);

}

bool StrStringsMatch(char const *const *ppszStrings, char const *pszMatch)
{

	int ii;

	for (ii = 0; ppszStrings[ii] != NULL; ii++)
		if (strcmp(ppszStrings[ii], pszMatch) == 0)
			return (true);

	return (false);

}

bool StrStringsIMatch(char const *const *ppszStrings, char const *pszMatch)
{

	int ii;

	for (ii = 0; ppszStrings[ii] != NULL; ii++)
		if (stricmp(ppszStrings[ii], pszMatch) == 0)
			return (true);

	return (false);

}

bool StrStringsRIWMatch(char const *const *pszMatches, char const *pszString)
{

	int ii;

	for (ii = 0; pszMatches[ii] != NULL; ii++)
		if (StrIWildMatch(pszString, pszMatches[ii]))
			return (true);

	return (false);

}

char *StrConcat(char const *const *ppszStrings, char const *pszCStr)
{

	int ii;
	int iStrCount = StrStringsCount(ppszStrings);
	int iCStrLength = strlen(pszCStr);
	int iSumLength = 0;

	for (ii = 0; ii < iStrCount; ii++)
		iSumLength += strlen(ppszStrings[ii]) + iCStrLength;

	char *pszConcat = (char *) SysAlloc(iSumLength + 1);

	if (pszConcat == NULL)
		return (NULL);

	SetEmptyString(pszConcat);

	for (ii = 0; ii < iStrCount; ii++) {
		if (ii > 0)
			strcat(pszConcat, pszCStr);

		strcat(pszConcat, ppszStrings[ii]);
	}

	return (pszConcat);

}

char *StrDeQuote(char *pszString, int iChar)
{

	if (*pszString == iChar) {
		int ii;

		for (ii = 1; (pszString[ii] != '\0') && (pszString[ii] != iChar); ii++)
			pszString[ii - 1] = pszString[ii];

		pszString[ii - 1] = '\0';
	}

	return (pszString);

}

char *StrQuote(const char *pszString, int iChar)
{

	int iStrLen = strlen(pszString);
	char *pszBuffer = (char *) SysAlloc(3 + iStrLen);

	if (pszBuffer == NULL)
		return (NULL);

	pszBuffer[0] = (char) iChar;
	memcpy(pszBuffer + 1, pszString, iStrLen);
	pszBuffer[iStrLen + 1] = (char) iChar;
	pszBuffer[iStrLen + 2] = '\0';

	return (pszBuffer);

}

char **StrGetTabLineStrings(const char *pszUsrLine)
{

	char **ppszStrings = StrTokenize(pszUsrLine, "\t");

	if (ppszStrings == NULL)
		return (NULL);

	for (int ii = 0; ppszStrings[ii] != NULL; ii++)
		StrDeQuote(ppszStrings[ii], '"');

	return (ppszStrings);

}

int StrWriteCRLFString(FILE * pFile, const char *pszString)
{

	unsigned int uStrLength = strlen(pszString);

	if ((uStrLength != 0) && (fwrite(pszString, uStrLength, 1, pFile) == 0)) {
		ErrSetErrorCode(ERR_FILE_WRITE);
		return (ERR_FILE_WRITE);
	}

	fputs("\r\n", pFile);

	return (0);

}

int StrWildMatch(char const *pszString, char const *pszMatch)
{

	int iPrev;
	int iMatched;
	int iReverse;
	int iEscape;

	for (; *pszMatch; pszString++, pszMatch++) {
		switch (*pszMatch) {
		case ('\\'):
			if (!*++pszMatch)
				return (0);

		default:
			if (*pszString != *pszMatch)
				return (0);
			continue;

		case ('?'):
			if (*pszString == '\0')
				return (0);
			continue;

		case ('*'):
			while (*(++pszMatch) == '*');
			if (!*pszMatch)
				return (1);
			while (*pszString)
				if (StrWildMatch(pszString++, pszMatch))
					return (1);
			return (0);

		case ('['):
			iEscape = 0;
			iReverse = (pszMatch[1] == INVCHAR) ? 1 : 0;
			if (iReverse)
				pszMatch++;
			for (iPrev = 256, iMatched = 0; *++pszMatch &&
			     (iEscape || (*pszMatch != ']')); iPrev = iEscape ? iPrev : *pszMatch)
			{
				if (!iEscape && (iEscape = *pszMatch == '\\'))
					continue;
				if (!iEscape && (*pszMatch == '-')) {
					if (!*++pszMatch)
						return (0);
					if (*pszMatch == '\\')
						if (!*++pszMatch)
							return (0);
					iMatched = iMatched || ((*pszString <= *pszMatch) &&
								(*pszString >= iPrev));
				} else
					iMatched = iMatched || (*pszString == *pszMatch);
				iEscape = 0;
			}

			if ((iPrev == 256) || iEscape || (*pszMatch != ']') ||
			    (iMatched == iReverse))
				return (0);

			continue;
		}
	}

	return ((*pszString == '\0') ? 1 : 0);

}

int StrIWildMatch(char const *pszString, char const *pszMatch)
{

	char *pszLowString = SysStrDup(pszString);

	if (pszLowString == NULL)
		return (0);

	char *pszLowMatch = SysStrDup(pszMatch);

	if (pszLowMatch == NULL) {
		SysFree(pszLowString);
		return (0);
	}

	StrLower(pszLowString);
	StrLower(pszLowMatch);

	int iMatchResult = StrWildMatch(pszLowString, pszLowMatch);

	SysFree(pszLowMatch);
	SysFree(pszLowString);

	return (iMatchResult);

}

char *StrLoadFile(FILE * pFile)
{

	fseek(pFile, 0, SEEK_END);

	unsigned int uFileSize = (unsigned int) ftell(pFile);
	char *pszData = (char *) SysAlloc(uFileSize + 1);

	if (pszData == NULL)
		return (NULL);

	fseek(pFile, 0, SEEK_SET);

	fread(pszData, uFileSize, 1, pFile);
	pszData[uFileSize] = '\0';

	return (pszData);

}

char *StrSprint(char const *pszFormat, ...)
{

	char *pszMessage = NULL;

	STRSPRINTF(pszMessage, pszFormat, pszFormat);

	return (pszMessage);

}

int StrSplitString(char const *pszString, char const *pszSplitters,
		   char *pszStrLeft, int iSizeLeft, char *pszStrRight, int iSizeRight)
{

	char const *pszSplitChar = NULL;

	for (;
	     (*pszSplitters != '\0') &&
	     ((pszSplitChar = strchr(pszString, *pszSplitters)) == NULL); ++pszSplitters);

	if (pszSplitChar == NULL) {
		if (pszStrLeft != NULL)
			StrNCpy(pszStrLeft, pszString, iSizeLeft);

		if (pszStrRight != NULL)
			SetEmptyString(pszStrRight);
	} else {
		if (pszStrLeft != NULL) {
			int iUserLength = (int) (pszSplitChar - pszString);

			StrNCpy(pszStrLeft, pszString, Min(iUserLength + 1, iSizeLeft));
		}

		if (pszStrRight != NULL)
			StrNCpy(pszStrRight, pszSplitChar + 1, iSizeRight);
	}

	return (0);

}

char *StrLTrim(char *pszString, char const *pszTrimChars)
{

	int ii;

	for (ii = 0; (pszString[ii] != '\0') && (strchr(pszTrimChars, pszString[ii]) != NULL);
	     ii++);

	if ((ii > 0) && (pszString[ii] != '\0')) {
		int jj;

		for (jj = ii; pszString[jj] != '\0'; jj++)
			pszString[jj - ii] = pszString[jj];

		pszString[jj - ii] = pszString[jj];
	}

	return (pszString);

}

char *StrRTrim(char *pszString, char const *pszTrimChars)
{

	int ii = strlen(pszString) - 1;

	for (; (ii >= 0) && (strchr(pszTrimChars, pszString[ii]) != NULL); ii--)
		pszString[ii] = '\0';

	return (pszString);

}

char *StrTrim(char *pszString, char const *pszTrimChars)
{

	return (StrRTrim(StrLTrim(pszString, pszTrimChars), pszTrimChars));

}

char *StrEOLTrim(char *pszString)
{

	int iPos = strlen(pszString);

	for (; (iPos > 0) && ((pszString[iPos - 1] == '\r') || (pszString[iPos - 1] == '\n'));
	     iPos--);

	pszString[iPos] = '\0';

	return (pszString);

}

char *StrIStr(char const *pszBuffer, char const *pszMatch)
{

	int iMatchLen = strlen(pszMatch);
	int iMatchPos = 0;
	int iLoMatch = LoChar(*pszMatch);

	if (iMatchLen == 0)
		return ((char *) pszBuffer);

	for (; *pszBuffer != '\0'; pszBuffer++) {
		if (LoChar(*pszBuffer) == iLoMatch) {
			if (++iMatchPos == iMatchLen)
				return ((char *) pszBuffer - iMatchLen + 1);

			iLoMatch = LoChar(pszMatch[iMatchPos]);
		} else if (iMatchPos != 0) {
			iMatchPos = 0;

			iLoMatch = LoChar(*pszMatch);
		}
	}

	return (NULL);

}

int StrDynInit(DynString * pDS)
{

	ZeroData(*pDS);
	pDS->pszBuffer = SysStrDup("");
	pDS->iStringSize = 0;
	pDS->iBufferSize = 0;

	return (0);

}

int StrDynFree(DynString * pDS)
{

	if (pDS->pszBuffer != NULL)
		SysFree(pDS->pszBuffer);

	ZeroData(*pDS);

	return (0);

}

int StrDynTruncate(DynString * pDS)
{

	if (pDS->pszBuffer != NULL)
		SetEmptyString(pDS->pszBuffer);

	pDS->iStringSize = 0;

	return (0);

}

char const *StrDynGet(DynString * pDS)
{

	return (pDS->pszBuffer);

}

int StrDynSize(DynString * pDS)
{

	return (pDS->iStringSize);

}

int StrDynAdd(DynString * pDS, char const *pszBuffer, int iStringSize)
{

	if (iStringSize < 0)
		iStringSize = strlen(pszBuffer);

	if ((pDS->iStringSize + iStringSize) >= pDS->iBufferSize) {
		int iNewSize = pDS->iBufferSize + Max(2 * iStringSize, MIN_DYNSTR_INCR);
		char *pszNewBuffer = (char *) SysAlloc(iNewSize);

		if (pszNewBuffer == NULL)
			return (ErrGetErrorCode());

		strcpy(pszNewBuffer, pDS->pszBuffer);

		SysFree(pDS->pszBuffer);

		pDS->pszBuffer = pszNewBuffer;

		pDS->iBufferSize = iNewSize;
	}

	memcpy(pDS->pszBuffer + pDS->iStringSize, pszBuffer, iStringSize);

	pDS->iStringSize += iStringSize;

	pDS->pszBuffer[pDS->iStringSize] = '\0';

	return (0);

}

int StrParamGet(char const *pszBuffer, char const *pszName, char *pszVal, int iMaxVal)
{

	int iNameLen = strlen(pszName), iParamSize;
	char const *pszTmp, *pszEnd;

	for (pszTmp = pszBuffer; (pszTmp = strstr(pszTmp, pszName)) != NULL; pszTmp++) {
		if ((pszTmp > pszBuffer) && (pszTmp[-1] != ','))
			continue;
		if (pszTmp[iNameLen] != '=')
			continue;
		pszTmp += iNameLen + 1;
		if ((pszEnd = strchr(pszTmp, ',')) == NULL)
			pszEnd = pszTmp + strlen(pszTmp);
		iParamSize = (int) (pszEnd - pszTmp);
		iParamSize = Min(iMaxVal - 1, iParamSize);

		Cpy2Sz(pszVal, pszTmp, iParamSize);

		return (1);
	}

	return (0);

}

