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

#ifndef _STRUTILS_H
#define _STRUTILS_H

#define STRSPRINTF(r, l, f) \
do \
{ \
    int             iCurrSize = 256; \
    int             iPSize; \
    va_list         Args; \
    for (;;) \
    { \
        r = (char *) SysAlloc(iCurrSize); \
        if (r == NULL) \
            break; \
        va_start(Args, l); \
        if (((iPSize = SysVSNPrintf(r, iCurrSize - 1, f, Args)) >= 0) && \
            iPSize < iCurrSize) \
        { \
            va_end(Args); \
            break; \
        } \
        va_end(Args); \
        if (iPSize > 0) \
            iCurrSize = (4 * iPSize) / 3 + 2; \
        else \
            iCurrSize *= 2; \
        SysFree(r); \
    } \
} while (0)

struct DynString {
	char *pszBuffer;
	int iStringSize;
	int iBufferSize;
};

int StrCmdLineToken(char const *&pszCmdLine, char *pszToken);
char **StrGetArgs(char const *pszCmdLine, int &iArgsCount);
char *StrLower(char *pszString);
char *StrUpper(char *pszString);
char *StrCrypt(char const *pszString, char *pszCrypt);
char *StrDeCrypt(char const *pszString, char *pszDeCrypt);
char **StrBuildList(char const *pszString, ...);
char **StrTokenize(const char *pszString, const char *pszTokenizer);
void StrFreeStrings(char **ppszStrings);
int StrStringsCount(char const *const *ppszStrings);
bool StrStringsMatch(char const *const *ppszStrings, char const *pszMatch);
bool StrStringsIMatch(char const *const *ppszStrings, char const *pszMatch);
bool StrStringsRIWMatch(char const *const *pszMatches, char const *pszString);
char *StrConcat(char const *const *ppszStrings, char const *pszCStr);
char *StrDeQuote(char *pszString, int iChar);
char *StrQuote(const char *pszString, int iChar);
char **StrGetTabLineStrings(const char *pszUsrLine);
int StrWriteCRLFString(FILE * pFile, const char *pszString);
int StrWildMatch(char const *pszString, char const *pszMatch);
int StrIWildMatch(char const *pszString, char const *pszMatch);
char *StrLoadFile(FILE * pFile);
char *StrSprint(char const *pszFormat, ...);
int StrSplitString(char const *pszString, char const *pszSplitters,
		   char *pszStrLeft, int iSizeLeft, char *pszStrRight, int iSizeRight);
char *StrLTrim(char *pszString, char const *pszTrimChars);
char *StrRTrim(char *pszString, char const *pszTrimChars);
char *StrTrim(char *pszString, char const *pszTrimChars);
char *StrEOLTrim(char *pszString);
char *StrIStr(char const *pszBuffer, char const *pszMatch);
int StrDynInit(DynString * pDS);
int StrDynFree(DynString * pDS);
int StrDynTruncate(DynString * pDS);
char const *StrDynGet(DynString * pDS);
int StrDynSize(DynString * pDS);
int StrDynAdd(DynString * pDS, char const *pszBuffer, int iStringSize = -1);
int StrParamGet(char const *pszBuffer, char const *pszName, char *pszVal, int iMaxVal);

#endif
