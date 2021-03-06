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

#ifndef _MAILDIR_H
#define _MAILDIR_H

#define MAILDIR_DIRECTORY           "Maildir"

int MdirCreateStructure(char const *pszBasePath);
int MdirGetTmpMaildirEntry(char const *pszMaildirPath, char *pszFilePath);
int MdirMoveTmpEntryInNew(char const *pszTmpEntryPath);
int MdirMoveMessage(char const *pszMaildirPath, const char *pszFileName,
		    char const *pszMessageID = NULL);

#endif
