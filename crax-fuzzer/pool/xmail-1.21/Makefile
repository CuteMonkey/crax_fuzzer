#
#  XMail by Davide Libenzi ( Intranet and Internet mail server )
#  Copyright (C) 1999,..,2004  Davide Libenzi
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#
#  Davide Libenzi <davidel@xmailserver.org>
#
SYSTYPE = linux
O = o
A = a
CPP = cpp
CC = g++
LD = g++

ifeq ($(XMAIL_DEBUG), 1)
STRIP = touch
CFLAGS = -g -rdynamic
else
STRIP = strip
CFLAGS = -O2
endif

CFLAGS := $(CFLAGS) -I. -D__UNIX__ -D__LINUX__ -D_REENTRANT=1 -D_THREAD_SAFE=1 -DHAS_SYSMACHINE

LDFLAGS = -ldl -lpthread

OUTDIR = bin
MKMACHDEPINC = SysMachine.h
MKMACHDEP = ${OUTDIR}/MkMachDep
MAILSVR = ${OUTDIR}/XMail
CRTLCLNT = ${OUTDIR}/CtrlClnt
XMCRYPT = ${OUTDIR}/XMCrypt
MKUSERS = ${OUTDIR}/MkUsers
SENDMAIL = ${OUTDIR}/sendmail

MKMACHDEPSRCS = MkMachDep.${CPP}
MKMACHDEPOBJS = MkMachDep.${O}

SVRSRCS = BuffSock.${CPP} CTRLSvr.${CPP} DynDNS.${CPP} DNS.${CPP} DNSCache.${CPP} Errors.${CPP} ExtAliases.${CPP} FINGSvr.${CPP} \
	MailConfig.${CPP} MailSvr.${CPP} Maildir.${CPP} MailDomains.${CPP} MD5.${CPP} MiscUtils.${CPP} LMAILSvr.${CPP} \
	AliasDomain.${CPP} POP3GwLink.${CPP} POP3Svr.${CPP} POP3Utils.${CPP} PSYNCSvr.${CPP} ResLocks.${CPP} SList.${CPP} SMAILSvr.${CPP} \
	TabIndex.${CPP} SMAILUtils.${CPP} SMTPSvr.${CPP} SMTPUtils.${CPP} ShBlocks.${CPP} StrUtils.${CPP} MessQueue.${CPP} \
	QueueUtils.${CPP} SvrUtils.${CPP} SysDep.${CPP} UsrMailList.${CPP} UsrAuth.${CPP} UsrUtils.${CPP} Main.${CPP} Base64Enc.${CPP} \
	Filter.${CPP}

SVROBJS = BuffSock.${O} CTRLSvr.${O} DynDNS.${O} DNS.${O} DNSCache.${O} Errors.${O} ExtAliases.${O} FINGSvr.${O} \
	MailConfig.${O} MailSvr.${O} Maildir.${O} MailDomains.${O} MD5.${O} MiscUtils.${O} LMAILSvr.${O} \
	AliasDomain.${O} POP3GwLink.${O} POP3Svr.${O} POP3Utils.${O} PSYNCSvr.${O} ResLocks.${O} SList.${O} SMAILSvr.${O} \
	TabIndex.${O} SMAILUtils.${O} SMTPSvr.${O} SMTPUtils.${O} ShBlocks.${O} StrUtils.${O} MessQueue.${O} \
	QueueUtils.${O} SvrUtils.${O} SysDep.${O} UsrMailList.${O} UsrAuth.${O} UsrUtils.${O} Main.${O} Base64Enc.${O} \
	Filter.${O}

CCLNSRCS = Base64Enc.${CPP} BuffSock.${CPP} SysDep.${CPP} StrUtils.${CPP} MD5.${CPP} MiscUtils.${CPP} CTRLClient.${CPP} Errors.${CPP}

CCLNOBJS = Base64Enc.${O} BuffSock.${O} SysDep.${O} StrUtils.${O} MD5.${O} MiscUtils.${O} CTRLClient.${O} Errors.${O}

XMCSRCS = XMCrypt.${CPP}

XMCOBJS = XMCrypt.${O}

MKUSRCS = MkUsers.${CPP}

MKUOBJS = MkUsers.${O}

SENDMAILSRC = SendMail.${CPP}

SENDMAILOBJS = SendMail.${O}

%.${O} : %.${CPP}
	${CC} ${CPPFLAGS} ${CFLAGS} -c $*.${CPP}

all: ${OUTDIR} ${MKMACHDEPINC} ${MAILSVR} ${CRTLCLNT} ${XMCRYPT} ${MKUSERS} ${SENDMAIL}

${OUTDIR}:
	@mkdir ${OUTDIR}

${MKMACHDEPINC}: ${MKMACHDEP}
	./${MKMACHDEP} > ${MKMACHDEPINC}

${MKMACHDEP}: ${MKMACHDEPOBJS}
	${LD} -o ${MKMACHDEP} ${MKMACHDEPOBJS} ${LDFLAGS}
	${STRIP} ${MKMACHDEP}

${MAILSVR}: ${SVROBJS}
	${LD} -o ${MAILSVR} ${SVROBJS} ${LDFLAGS}
	${STRIP} ${MAILSVR}

${CRTLCLNT}: ${CCLNOBJS}
	${LD} -o ${CRTLCLNT} ${CCLNOBJS} ${LDFLAGS}
	${STRIP} ${CRTLCLNT}

${XMCRYPT}: ${XMCOBJS}
	${LD} -o ${XMCRYPT} ${XMCOBJS} ${LDFLAGS}
	${STRIP} ${XMCRYPT}

${MKUSERS}: ${MKUOBJS}
	${LD} -o ${MKUSERS} ${MKUOBJS} ${LDFLAGS}
	${STRIP} ${MKUSERS}

${SENDMAIL}: ${SENDMAILOBJS}
	${LD} -o ${SENDMAIL} ${SENDMAILOBJS} ${LDFLAGS}
	${STRIP} ${SENDMAIL}

distclean: clean

clean:
	rm -f .depend a.out core ${MAILSVR} ${CRTLCLNT} ${XMCRYPT} ${MKUSERS} ${SENDMAIL}
	rm -f *.${O} *~ ${MKMACHDEPINC} ${MKMACHDEP}
	rm -rf ${OUTDIR}

