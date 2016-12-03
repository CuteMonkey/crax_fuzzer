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

#ifndef _SMTPSVR_H
#define _SMTPSVR_H

#define STD_SMTP_PORT               25
#define MAX_SMTP_ACCEPT_ADDRESSES   32

#define SMTPF_STOP_SERVER           (1 << 0)
#define SMTPF_LOG_ENABLED           (1 << 1)

struct SMTPConfig {
	int iPort;
	unsigned long ulFlags;
	long lThreadCount;
	long lMaxThreads;
	int iSessionTimeout;
	int iTimeout;
	int iMaxRcpts;
	unsigned int uPopAuthExpireTime;
	int iNumAddr;
	SYS_INET_ADDR SvrAddr[MAX_SMTP_ACCEPT_ADDRESSES];

};

unsigned int SMTPThreadProc(void *pThreadData);

#endif
