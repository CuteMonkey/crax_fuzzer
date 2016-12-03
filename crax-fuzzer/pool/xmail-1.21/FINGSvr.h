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

#ifndef _FINGSVR_H
#define _FINGSVR_H

#define STD_FINGER_PORT             79
#define MAX_FING_ACCEPT_ADDRESSES   32

#define FINGF_STOP_SERVER           (1 << 0)
#define FINGF_LOG_ENABLED           (1 << 1)

struct FINGConfig {
	int iPort;
	unsigned long ulFlags;
	long lThreadCount;
	int iTimeout;
	int iNumAddr;
	SYS_INET_ADDR SvrAddr[MAX_FING_ACCEPT_ADDRESSES];

};

unsigned int FINGThreadProc(void *pThreadData);

#endif
