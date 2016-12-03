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

#ifndef _CTRLSVR_H
#define _CTRLSVR_H

#define STD_CTRL_PORT               6017
#define MAX_CTRL_ACCEPT_ADDRESSES   32

#define CTRLF_STOP_SERVER           (1 << 0)
#define CTRLF_LOG_ENABLED           (1 << 1)

struct CTRLConfig {
	unsigned long ulFlags;
	int iPort;
	long lThreadCount;
	long lMaxThreads;
	int iSessionTimeout;
	int iTimeout;
	int iNumAddr;
	SYS_INET_ADDR SvrAddr[MAX_CTRL_ACCEPT_ADDRESSES];

};

unsigned int CTRLThreadProc(void *pThreadData);

#endif
