OSTYPE=Linux
ARCHTYPE=i386

# From OS/Makefile-Default
MAKE_SHELL=/bin/sh
BASENAME_COMMAND=/usr/bin/basename
CHOWN_COMMAND=/usr/bin/chown
CHGRP_COMMAND=/usr/bin/chgrp
MV_COMMAND=/bin/mv
RM_COMMAND=/bin/rm
AR=ar cq
PERL_COMMAND=/usr/bin/perl
CC=gcc
EXIWHAT_PS_CMD=/bin/ps
EXIWHAT_PS_ARG=-e
EXIWHAT_EGREP_ARG=' exim( |$$|-)'
EXIWHAT_KILL_SIGNAL=-USR1
HOSTNAME_COMMAND=/bin/hostname
RANLIB=ranlib
EXIM_CHMOD=@true
LOCAL_SCAN_SOURCE=src/local_scan.c
X11=/usr/X11R6
XINCLUDE=-I$(X11)/include
XLFLAGS=-L$(X11)/lib
X11_LD_LIB=$(X11)/lib
EXIMON_TEXTPOP=em_TextPop.o
# End of OS/Makefile-Default

# From OS/Makefile-Linux
HAVE_ICONV=yes
BASENAME_COMMAND=look_for_it
CHOWN_COMMAND=look_for_it
CHGRP_COMMAND=look_for_it
CFLAGS=-O -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
DBMLIB = -ldb
USE_DB = yes
LIBS = -lnsl -lcrypt
LIBRESOLV = -lresolv
X11=/usr/X11R6
XINCLUDE=-I$(X11)/include
XLFLAGS=-L$(X11)/lib
X11_LD_LIB=$(X11)/lib
EXIWHAT_PS_ARG=ax
EXIWHAT_EGREP_ARG='/exim( |$$)'
EXIWHAT_MULTIKILL_CMD=killall
EXIWHAT_MULTIKILL_ARG=exim
EXIWHAT_KILL_SIGNAL=-USR1
# End of OS/Makefile-Linux

# From Local/Makefile
BIN_DIRECTORY=/usr/exim/bin
CONFIGURE_FILE=/usr/exim/configure
EXIM_USER=kvchung
SPOOL_DIRECTORY=/var/spool/exim
ROUTER_ACCEPT=yes
ROUTER_DNSLOOKUP=yes
ROUTER_IPLITERAL=yes
ROUTER_MANUALROUTE=yes
ROUTER_QUERYPROGRAM=yes
ROUTER_REDIRECT=yes
TRANSPORT_APPENDFILE=yes
TRANSPORT_AUTOREPLY=yes
TRANSPORT_PIPE=yes
TRANSPORT_SMTP=yes
LOOKUP_DBM=yes
LOOKUP_LSEARCH=yes
EXIM_MONITOR=eximon.bin
FIXED_NEVER_USERS=root
HEADERS_CHARSET="ISO-8859-1"
SYSLOG_LOG_PID=yes
EXICYCLOG_MAX=10
COMPRESS_COMMAND=/usr/bin/gzip
COMPRESS_SUFFIX=gz
ZCAT_COMMAND=/usr/bin/zcat
SYSTEM_ALIASES_FILE=/etc/aliases
TMPDIR="/tmp"
# End of Local/Makefile

build=

# This file is the basis of the main makefile for Exim and friends. The
# makefile at the top level arranges to build the main makefile by calling
# scripts/Configure-Makefile from within the build directory. This
# concatentates the configuration settings from Local/Makefile and other,
# optional, Local/* files at the front of this file, to create Makefile in the
# build directory.

SHELL      = $(MAKE_SHELL)
SCRIPTS    = ../scripts
EDITME     = ../Local/Makefile
EXIMON_EDITME = ../Local/eximon.conf

# The compiler used for linking is normally the same as the compiler used for
# compiling. However, by giving it a different name, we can override it from
# the command line, and this is helpful for certain types of testing.

LNCC = $(CC)


# The default target double-checks the existence of $(EDITME) and then arranges
# to touch it if it exists and any of the optional configuration files, which
# depend on the os or the architecture, have been altered. The same sub-target
# does the same thing for the eximon configuration file if it exists. Then
# there is a check that the Makefile (the one built from this file) is
# up-to-date. Then the os-specific source files and the C configuration file
# are set up, and finally it goes to the main Exim target.

all:    $(EDITME) checklocalmake Makefile os.h os.c config.h allexim

checklocalmake:
	@if $(SHELL) $(SCRIPTS)/newer $(EDITME)-$(OSTYPE) $(EDITME) || \
	  $(SHELL) $(SCRIPTS)/newer $(EDITME)-$(ARCHTYPE) $(EDITME) || \
	  $(SHELL) $(SCRIPTS)/newer $(EDITME)-$(OSTYPE)-$(ARCHTYPE) $(EDITME); \
	then \
	  touch $(EDITME); \
	fi
	@if $(SHELL) $(SCRIPTS)/newer $(EXIMON_EDITME)-$(OSTYPE) $(EXIMON_EDITME) || \
	  $(SHELL) $(SCRIPTS)/newer $(EXIMON_EDITME)-$(ARCHTYPE) $(EXIMON_EDITME) || \
	  $(SHELL) $(SCRIPTS)/newer $(EXIMON_EDITME)-$(OSTYPE)-$(ARCHTYPE) $(EXIMON_EDITME); \
	then \
	  if [ -f $(EXIMON_EDITME) ]; then touch $(EXIMON_EDITME); fi \
	fi

$(EDITME):
	@echo " "
	@echo "*** Please create Local/Makefile by copying src/EDITME and making"
	@echo "*** appropriate changes for your site."
	@echo " "
	@false

$(EXIMON_EDITME):
	@echo " "
	@echo "*** Please create Local/eximon.conf by copying exim_monitor/EDITME and making"
	@echo "*** appropriate changes for your site."
	@echo " "
	@test ! -d ../Local && mkdir ../Local
	@false

# Check that the local Makefile is up-to-date

Makefile: ../OS/Makefile-Base ../OS/Makefile-Default \
          $(SCRIPTS)/Configure $(SCRIPTS)/Configure-Makefile $(EDITME)
	@echo " "
	@echo "*** Makefile needs rebuilding"
	@echo "*** Please run \"make makefile\" at top level"
	@echo " "
	@false

# Build (link) the os.h file

os.h:
	$(SHELL) $(SCRIPTS)/Configure-os.h

# Build the os.c file

os.c:   ../src/os.c
	$(SHELL) $(SCRIPTS)/Configure-os.c

# Build the config.h file.

config.h: Makefile buildconfig ../src/config.h.defaults $(EDITME)
	$(SHELL) $(SCRIPTS)/Configure-config.h


# This target is recognized specially by GNU make. It records those targets
# that do not correspond to files that are being built and which should
# therefore always be run, even if the files exist. This shouldn't in fact be a
# problem, but it does no harm. Other make programs will just ignore this.

.PHONY: all allexim buildauths buildlookups buildpcre buildrouters \
        buildtransports checklocalmake clean


# This is the real default target for all the various exim binaries and
# scripts, once the configuring stuff is done.

allexim: config.h buildpcre $(EXIM_MONITOR) exicyclog exinext exiwhat \
        exigrep eximstats exipick exiqgrep exiqsumm \
        transport-filter.pl convert4r3 convert4r4 \
        exim_checkaccess \
        exim_dbmbuild exim_dumpdb exim_fixdb exim_tidydb exim_lock \
        buildlookups buildrouters buildtransports \
        buildauths exim


# Targets for special-purpose configuration header builders
buildconfig: buildconfig.c
	$(CC) $(CFLAGS) $(INCLUDE) -o buildconfig buildconfig.c $(LIBS)


# Target for the exicyclog utility script
exicyclog: Makefile config.h ../src/exicyclog.src
	@rm -f exicyclog
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?CONFIGURE_FILE_USE_NODE?$(CONFIGURE_FILE_USE_NODE)?" \
	  -e "s?CONFIGURE_FILE_USE_EUID?$(CONFIGURE_FILE_USE_EUID)?" \
	  -e "s?CONFIGURE_FILE?$(CONFIGURE_FILE)?" \
	  -e "s?BIN_DIRECTORY?$(BIN_DIRECTORY)?" \
	  -e "s?EXICYCLOG_MAX?$(EXICYCLOG_MAX)?" \
	  -e "s?COMPRESS_COMMAND?$(COMPRESS_COMMAND)?" \
	  -e "s?COMPRESS_SUFFIX?$(COMPRESS_SUFFIX)?" \
	  -e "s?CHOWN_COMMAND?$(CHOWN_COMMAND)?" \
	  -e "s?CHGRP_COMMAND?$(CHGRP_COMMAND)?" \
	  -e "s?MV_COMMAND?$(MV_COMMAND)?" \
	  -e "s?RM_COMMAND?$(RM_COMMAND)?" \
	  ../src/exicyclog.src > exicyclog-t
	@mv exicyclog-t exicyclog
	@chmod a+x exicyclog
	@echo ">>> exicyclog script built"; echo ""

# Target for the exinext utility script
exinext: Makefile config.h ../src/exinext.src
	@rm -f exinext
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?CONFIGURE_FILE_USE_NODE?$(CONFIGURE_FILE_USE_NODE)?" \
	  -e "s?CONFIGURE_FILE?$(CONFIGURE_FILE)?" \
	  -e "s?BIN_DIRECTORY?$(BIN_DIRECTORY)?" \
	  ../src/exinext.src > exinext-t
	@mv exinext-t exinext
	@chmod a+x exinext
	@echo ">>> exinext script built"; echo ""

# Target for the exiwhat utility script
exiwhat: Makefile config.h ../src/exiwhat.src
	@rm -f exiwhat
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?CONFIGURE_FILE_USE_NODE?$(CONFIGURE_FILE_USE_NODE)?" \
	  -e "s?CONFIGURE_FILE?$(CONFIGURE_FILE)?" \
	  -e "s?BIN_DIRECTORY?$(BIN_DIRECTORY)?" \
	  -e "s?EXIWHAT_PS_CMD?$(EXIWHAT_PS_CMD)?" \
	  -e "s?EXIWHAT_PS_ARG?$(EXIWHAT_PS_ARG)?" \
	  -e "s?EXIWHAT_KILL_SIGNAL?$(EXIWHAT_KILL_SIGNAL)?" \
	  -e "s?EXIWHAT_EGREP_ARG?$(EXIWHAT_EGREP_ARG)?" \
	  -e "s?EXIWHAT_MULTIKILL_CMD?$(EXIWHAT_MULTIKILL_CMD)?" \
	  -e "s?EXIWHAT_MULTIKILL_ARG?$(EXIWHAT_MULTIKILL_ARG)?" \
	  ../src/exiwhat.src > exiwhat-t
	@mv exiwhat-t exiwhat
	@chmod a+x exiwhat
	@echo ">>> exiwhat script built"; echo ""

# Target for the exim_checkaccess utility script
exim_checkaccess: Makefile config.h ../src/exim_checkaccess.src
	@rm -f exim_checkaccess
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?CONFIGURE_FILE_USE_NODE?$(CONFIGURE_FILE_USE_NODE)?" \
	  -e "s?CONFIGURE_FILE?$(CONFIGURE_FILE)?" \
	  -e "s?BIN_DIRECTORY?$(BIN_DIRECTORY)?" \
	  -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/exim_checkaccess.src > exim_checkaccess-t
	@mv exim_checkaccess-t exim_checkaccess
	@chmod a+x exim_checkaccess
	@echo ">>> exim_checkaccess script built"; echo ""

# Target for the Exim monitor start-up script
eximon: Makefile config.h ../src/eximon.src ../OS/eximon.conf-Default \
          ../Local/eximon.conf
	@rm -f eximon
	$(SHELL) $(SCRIPTS)/Configure-eximon
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?CONFIGURE_FILE_USE_NODE?$(CONFIGURE_FILE_USE_NODE)?" \
	  -e "s?CONFIGURE_FILE?$(CONFIGURE_FILE)?" \
	  -e "s?BIN_DIRECTORY?$(BIN_DIRECTORY)?" \
	  -e "s?BASENAME_COMMAND?$(BASENAME_COMMAND)?" \
	  -e "s?HOSTNAME_COMMAND?$(HOSTNAME_COMMAND)?" \
	  -e "s?X11_LD_LIBRARY?$(X11_LD_LIB)?" \
	  ../src/eximon.src >> eximon
	@echo ">>> eximon script built"; echo ""

# Targets for utilities; these are all Perl scripts that have to get the
# location of Perl put in them. A few need other things as well.

exigrep: Makefile ../src/exigrep.src
	@rm -f exigrep
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  -e "s?ZCAT_COMMAND?$(ZCAT_COMMAND)?" \
          -e "s?COMPRESS_SUFFIX?$(COMPRESS_SUFFIX)?" \
	  ../src/exigrep.src > exigrep-t
	@mv exigrep-t exigrep
	@chmod a+x exigrep
	@echo ">>> exigrep script built"; echo ""

eximstats: Makefile ../src/eximstats.src
	@rm -f eximstats
	sed \
	  -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/eximstats.src > eximstats-t
	@mv eximstats-t eximstats
	@chmod a+x eximstats
	@echo ">>> eximstats script built"; echo ""

exiqgrep: Makefile ../src/exiqgrep.src
	@rm -f exiqgrep
	sed \
	  -e "s?PROCESSED_FLAG?This file has been so processed.?"\
	  -e "/^# /p" \
	  -e "/^# /d" \
	  -e "s?BIN_DIRECTORY?$(BIN_DIRECTORY)?" \
	  -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/exiqgrep.src > exiqgrep-t
	@mv exiqgrep-t exiqgrep
	@chmod a+x exiqgrep
	@echo ">>> exiqgrep script built"; echo ""

exiqsumm: Makefile ../src/exiqsumm.src
	@rm -f exiqsumm
	sed -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/exiqsumm.src > exiqsumm-t
	@mv exiqsumm-t exiqsumm
	@chmod a+x exiqsumm
	@echo ">>> exiqsumm script built"; echo ""

exipick: Makefile ../src/exipick.src
	@rm -f exipick
	sed -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  -e "s?SPOOL_DIRECTORY?$(SPOOL_DIRECTORY)?" \
	  ../src/exipick.src > exipick-t
	@mv exipick-t exipick
	@chmod a+x exipick
	@echo ">>> exipick script built"; echo ""

transport-filter.pl: Makefile ../src/transport-filter.src
	@rm -f transport-filter.pl
	sed -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/transport-filter.src > transport-filter.pl-t
	@mv transport-filter.pl-t transport-filter.pl
	@chmod a+x transport-filter.pl
	@echo ">>> transport-filter.pl script built"; echo ""

convert4r3: Makefile ../src/convert4r3.src
	@rm -f convert4r3
	sed -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/convert4r3.src > convert4r3-t
	@mv convert4r3-t convert4r3
	@chmod a+x convert4r3
	@echo ">>> convert4r3 script built"; echo ""

convert4r4: Makefile ../src/convert4r4.src
	@rm -f convert4r4
	sed -e "s?PERL_COMMAND?$(PERL_COMMAND)?" \
	  ../src/convert4r4.src > convert4r4-t
	@mv convert4r4-t convert4r4
	@chmod a+x convert4r4
	@echo ">>> convert4r4 script built"; echo ""


# Targets for final binaries; the main one has a build number which is
# updated each time. We don't bother with that for the auxiliaries.

OBJ_EXIM = acl.o child.o crypt16.o daemon.o dbfn.o debug.o deliver.o \
        directory.o dns.o drtables.o enq.o exim.o expand.o filter.o \
        filtertest.o globals.o \
        header.o host.o ip.o log.o lss.o match.o moan.o \
        os.o parse.o queue.o \
        rda.o readconf.o receive.o retry.o rewrite.o rfc2047.o \
        route.o search.o sieve.o smtp_in.o smtp_out.o spool_in.o spool_out.o \
        store.o string.o tls.o tod.o transport.o tree.o verify.o \
        local_scan.o $(EXIM_PERL)

exim:   pcre/libpcre.a lookups/lookups.a auths/auths.a \
        routers/routers.a transports/transports.a \
        $(OBJ_EXIM) version.c
	@echo " "
	awk '{ print ($$1+1) }' cnumber.h > cnumber.temp
	rm -f cnumber.h; mv cnumber.temp cnumber.h
	$(CC) -c $(CFLAGS) $(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE) version.c
	rm -f exim
	$(PURIFY) $(LNCC) -o exim $(LFLAGS) $(OBJ_EXIM) version.o \
	  pcre/libpcre.a \
	  routers/routers.a transports/transports.a lookups/lookups.a \
	  auths/auths.a \
	  $(LIBRESOLV) $(LIBS) $(LIBS_EXIM) $(IPV6_LIBS) $(EXTRALIBS) \
	  $(EXTRALIBS_EXIM) $(DBMLIB) $(LOOKUP_LIBS) $(PERL_LIBS) $(TLS_LIBS)
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) exim; \
	  $(STRIP_COMMAND) exim; \
	fi
	$(EXIM_CHMOD)
	@echo " "
	@echo ">>> exim binary built"
	@echo " "

# The utility for dumping the contents of an exim database

OBJ_DUMPDB = exim_dumpdb.o util-os.o util-store.o

exim_dumpdb: $(OBJ_DUMPDB)
	$(LNCC) $(CFLAGS) $(INCLUDE) -o exim_dumpdb $(LFLAGS) $(OBJ_DUMPDB) \
	  $(LIBS) $(EXTRALIBS) $(DBMLIB)
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) exim_dumpdb; \
	  $(STRIP_COMMAND) exim_dumpdb; \
	fi
	@echo " "
	@echo ">>> exim_dumpdb utility built"
	@echo " "

# The utility for interrogating/fixing the contents of an exim database

OBJ_FIXDB = exim_fixdb.o util-os.o util-store.o

exim_fixdb:  $(OBJ_FIXDB)
	$(LNCC) $(CFLAGS) $(INCLUDE) -o exim_fixdb $(LFLAGS) $(OBJ_FIXDB) \
	  $(LIBS) $(EXTRALIBS) $(DBMLIB)
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) exim_fixdb; \
	  $(STRIP_COMMAND) exim_fixdb; \
	fi
	@echo " "
	@echo ">>> exim_fixdb utility built"
	@echo " "

# The utility for tidying the contents of an exim database

OBJ_TIDYDB = exim_tidydb.o util-os.o util-store.o

exim_tidydb: $(OBJ_TIDYDB)
	$(LNCC) $(CFLAGS) $(INCLUDE) -o exim_tidydb $(LFLAGS) $(OBJ_TIDYDB) \
	  $(LIBS) $(EXTRALIBS) $(DBMLIB)
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) exim_tidydb; \
	  $(STRIP_COMMAND) exim_tidydb; \
	fi
	@echo " "
	@echo ">>> exim_tidydb utility built"
	@echo " "

# The utility for building dbm files

exim_dbmbuild: exim_dbmbuild.o
	$(LNCC) -o exim_dbmbuild $(LFLAGS) exim_dbmbuild.o \
	  $(LIBS) $(EXTRALIBS) $(DBMLIB)
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) exim_dbmbuild; \
	  $(STRIP_COMMAND) exim_dbmbuild; \
	fi
	@echo " "
	@echo ">>> exim_dbmbuild utility built"
	@echo " "

# The utility for locking a mailbox while messing around with it

exim_lock: exim_lock.c
	$(CC) -c $(CFLAGS) $(INCLUDE) exim_lock.c
	$(LNCC) -o exim_lock $(LFLAGS) exim_lock.o  \
	  $(LIBS) $(EXTRALIBS)
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) exim_lock; \
	  $(STRIP_COMMAND) exim_lock; \
	fi
	@echo " "
	@echo ">>> exim_lock utility built"
	@echo " "

# The X-based Exim monitor program's binary part. There's a macro for cutting
# out the modified TextPop module, because some antique link editors cannot
# handle the fact that it is redefining things that are found later in the
# Xaw library.

# Object modules that are the unique Eximon modules

MONBIN = em_StripChart.o $(EXIMON_TEXTPOP) em_globals.o em_init.o \
	em_log.o em_main.o em_menu.o em_queue.o em_strip.o \
	em_text.o em_xs.o

# The complete modules list also includes some specially compiled versions of
# code from the main Exim source tree.

OBJ_MONBIN = util-spool_in.o util-store.o util-string.o tod.o tree.o $(MONBIN)

eximon.bin: $(EXIMON_EDITME) eximon $(OBJ_MONBIN) pcre/libpcre.a \
            ../exim_monitor/em_version.c
	$(CC) -o em_version.o -c \
	  $(CFLAGS) $(XINCLUDE) -I. ../exim_monitor/em_version.c
	$(PURIFY) $(LNCC) -o eximon.bin em_version.o $(LFLAGS) $(XLFLAGS) \
	$(OBJ_MONBIN) -lXaw -lXmu -lXt -lXext -lX11 pcre/libpcre.a \
	  $(LIBS) $(LIBS_EXIMON) $(EXTRALIBS) $(EXTRALIBS_EXIMON) -lc
	@if [ x"$(STRIP_COMMAND)" != x"" ]; then \
	  echo $(STRIP_COMMAND) eximon.bin; \
	  $(STRIP_COMMAND) eximon.bin; \
	fi
	@echo " "
	@echo ">>> exim monitor binary built"
	@echo " "


# Compile step for most of the exim modules. HDRS is a list of headers
# which cause everthing to be rebuilt. PHDRS is the same, for the use
# of routers, transports, and authenticators. I can't find a way of doing this
# in one. This list is overkill, but it doesn't really take much time to
# rebuild Exim on a modern computer.

HDRS  =    config.h    dbfunctions.h    dbstuff.h    exim.h    functions.h    globals.h    local_scan.h    macros.h    mytypes.h    structs.h
PHDRS = ../config.h ../dbfunctions.h ../dbstuff.h ../exim.h ../functions.h ../globals.h ../local_scan.h ../macros.h ../mytypes.h ../structs.h

.SUFFIXES: .o .c
.c.o:; $(CC) -c $(CFLAGS) -I. $(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE) $*.c

# This is the dummy module for use by test compiles of individual modules. It
# contains functions such as log_write() that may be called from bits of Exim
# in the tested code.

dummies.o:       dummies.c

# Compile instructions for perl.o for when EXIM_PERL is set

perl.o:          $(HDRS) perl.c
	$(PERL_CC) $(PERL_CCOPTS) $(INCLUDE) -c perl.c

# Compile instructions for the database utility modules

exim_dumpdb.o:   $(HDRS) exim_dbutil.c
	$(CC) -c $(CFLAGS) $(INCLUDE) \
				      -DCOMPILE_UTILITY \
				      -DEXIM_DUMPDB \
				      -o exim_dumpdb.o exim_dbutil.c

exim_fixdb.o:    $(HDRS) exim_dbutil.c
	$(CC) -c $(CFLAGS) $(INCLUDE) \
				      -DCOMPILE_UTILITY \
				      -DEXIM_FIXDB \
				      -o exim_fixdb.o exim_dbutil.c

exim_tidydb.o:   $(HDRS) exim_dbutil.c
	$(CC) -c $(CFLAGS) $(INCLUDE) \
				      -DCOMPILE_UTILITY \
				      -DEXIM_TIDYDB \
				      -o exim_tidydb.o exim_dbutil.c

# Compile instructions for exim_dbmbuild

exim_dbmbuild.o: $(HDRS) exim_dbmbuild.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -o exim_dbmbuild.o exim_dbmbuild.c

# Utilities use special versions of some modules - typically with debugging
# calls cut out.

util-spool_in.o: $(HDRS) spool_in.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -DCOMPILE_UTILITY -o util-spool_in.o spool_in.c

util-store.o:    $(HDRS) store.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -DCOMPILE_UTILITY -o util-store.o store.c

util-string.o:   $(HDRS) string.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -DCOMPILE_UTILITY -o util-string.o string.c

util-os.o:       $(HDRS) os.c
	$(CC) -c $(CFLAGS) $(INCLUDE) \
				      -DCOMPILE_UTILITY \
				      -DOS_LOAD_AVERAGE \
				      -DFIND_RUNNING_INTERFACES \
				      -o util-os.o os.c

# The local scan module depends only on its own special header, and is compiled
# from a source whose location is set by configuration.

local_scan.o:    Makefile local_scan.h ../$(LOCAL_SCAN_SOURCE)
	$(CC) -c $(CFLAGS) -I. $(INCLUDE) -o local_scan.o ../$(LOCAL_SCAN_SOURCE)

# Dependencies for the "ordinary" exim modules

acl.o:           $(HDRS) acl.c
child.o:         $(HDRS) child.c
crypt16.o:       $(HDRS) crypt16.c
daemon.o:        $(HDRS) daemon.c
dbfn.o:          $(HDRS) dbfn.c
debug.o:         $(HDRS) debug.c
deliver.o:       $(HDRS) deliver.c
directory.o:     $(HDRS) directory.c
dns.o:           $(HDRS) dns.c
enq.o:           $(HDRS) enq.c
exim.o:          $(HDRS) exim.c
expand.o:        $(HDRS) expand.c
filter.o:        $(HDRS) filter.c
filtertest.o:    $(HDRS) filtertest.c
globals.o:       $(HDRS) globals.c
header.o:        $(HDRS) header.c
host.o:          $(HDRS) host.c
ip.o:            $(HDRS) ip.c
log.o:           $(HDRS) log.c
lss.o:           $(HDRS) lss.c
match.o:         $(HDRS) match.c
moan.o:          $(HDRS) moan.c
os.o:            $(HDRS) os.c
parse.o:         $(HDRS) parse.c
queue.o:         $(HDRS) queue.c
rda.o:           $(HDRS) rda.c
readconf.o:      $(HDRS) readconf.c
receive.o:       $(HDRS) receive.c
retry.o:         $(HDRS) retry.c
rewrite.o:       $(HDRS) rewrite.c
rfc2047.o:       $(HDRS) rfc2047.c
route.o:         $(HDRS) route.c
search.o:        $(HDRS) search.c
sieve.o:         $(HDRS) sieve.c
smtp_in.o:       $(HDRS) smtp_in.c
smtp_out.o:      $(HDRS) smtp_out.c
spool_in.o:      $(HDRS) spool_in.c
spool_out.o:     $(HDRS) spool_out.c
store.o:         $(HDRS) store.c
string.o:        $(HDRS) string.c
tls.o:           $(HDRS) tls.c tls-gnu.c tls-openssl.c
tod.o:           $(HDRS) tod.c
transport.o:     $(HDRS) transport.c
tree.o:          $(HDRS) tree.c
verify.o:        $(HDRS) verify.c

# The module containing tables of available lookups, routers, auths, and
# transports must be rebuilt if any of them are. However, because the makefiles
# for the drivers are always run, we don't actually put the dependencies here,
# because if we do, some version of "make" (e.g. IRIX) insist on rebuilding
# drtables.o even though the .a files haven't in fact been updated. Instead
# it is arranged that the lower-level makefiles remove drtables.o when they
# rebuild the .a files.

drtables.o:      $(HDRS) drtables.c


# The exim monitor's private modules - the sources live in a private
# subdirectory. The final binary combines the private modules with some
# modules from the main exim binary.

em_StripChart.o: ../exim_monitor/em_StripChart.c
em_TextPop.o:    ../exim_monitor/em_TextPop.c
em_globals.o:    ../exim_monitor/em_globals.c ../exim_monitor/em_hdr.h
em_init.o:       ../exim_monitor/em_init.c    ../exim_monitor/em_hdr.h
em_log.o:        ../exim_monitor/em_log.c     ../exim_monitor/em_hdr.h
em_main.o:       ../exim_monitor/em_main.c    ../exim_monitor/em_hdr.h
em_menu.o:       ../exim_monitor/em_menu.c    ../exim_monitor/em_hdr.h
em_queue.o:      ../exim_monitor/em_queue.c   ../exim_monitor/em_hdr.h
em_strip.o:      ../exim_monitor/em_strip.c   ../exim_monitor/em_hdr.h
em_text.o:       ../exim_monitor/em_text.c    ../exim_monitor/em_hdr.h
em_xs.o:         ../exim_monitor/em_xs.c      ../exim_monitor/em_hdr.h
em_version.o:    ../exim_monitor/em_version.c ../exim_monitor/em_hdr.h
$(MONBIN): $(HDRS)
		 $(CC) -o $@ -c $(CFLAGS) -I. -I../exim_monitor $(INCLUDE) $(XINCLUDE) \
		   ../exim_monitor/`echo $@ | sed 's/o$$/c/'`


# Targets for the various libraries that Exim uses. This coding is tedious,
# because different versions of "make" behave in different ways with regard
# to rebuilding. If these target names are of the form pcre/libpcre.a, for
# example, then a forcing mechanism is required to get them obeyed each time.
# That's fine on Solaris and other systems; the rebuilding of the exim target
# happens only if the libraries are actually rebuilt. However, on IRIX, if
# the target is forced, the exim target gets unnecessarily rebuilt even if
# the .a file is not. Contrariwise, if we use dummy names, they don't interact
# with the building of exim (and eximon.bin), but for libpcre Exim doesn't get
# rebuilt when it should. (For the others it does, because they remove
# drtables.o when they rebuild.) To get round this, we forcibly remove the
# binary when it needs to be rebuilt.

# The PCRE regex library. Move the pcretest program to the util directory. Some
# "clever" versions of make notice that there are two successive shell
# commands, and they run them in the same shell. This means that we have to
# take care to encapsulate change of directory in parentheses, so that it
# reverts when it should.

buildpcre:
	 @(cd pcre; $(MAKE) SHELL=$(SHELL) AR="$(AR)" $(MFLAGS) CC="$(CC)" \
	   CFLAGS="$(CFLAGS) $(PCRE_CFLAGS)" \
	   RANLIB="$(RANLIB)" HDRS="$(PHDRS)" \
	   INCLUDE="$(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE)")
	 @if $(SHELL) $(SCRIPTS)/newer pcre/libpcre.a exim; then \
	   rm -f exim eximon.bin; fi

# The lookups library.

buildlookups:
	 @cd lookups; $(MAKE) SHELL=$(SHELL) AR="$(AR)" $(MFLAGS) CC="$(CC)" CFLAGS="$(CFLAGS)" \
	   RANLIB="$(RANLIB)" HDRS="$(PHDRS)" \
	   INCLUDE="$(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE) $(LOOKUP_INCLUDE)"; \
	 echo " "

# The routers library.

buildrouters:
	 @cd routers; $(MAKE) SHELL=$(SHELL) AR="$(AR)" $(MFLAGS) CC="$(CC)" CFLAGS="$(CFLAGS)" \
	   RANLIB="$(RANLIB)" HDRS="$(PHDRS)" \
	   INCLUDE="$(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE)"; \
	 echo " "

# The transports library.

buildtransports:
	 @cd transports; $(MAKE) SHELL=$(SHELL) AR="$(AR)" $(MFLAGS) CC="$(CC)" CFLAGS="$(CFLAGS)" \
	   RANLIB="$(RANLIB)" HDRS="$(PHDRS)" \
	   INCLUDE="$(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE)"; \
	 echo " "

# The library of authorization modules

buildauths:
	 @cd auths; $(MAKE) SHELL=$(SHELL) AR="$(AR)" $(MFLAGS) CC="$(CC)" CFLAGS="$(CFLAGS)" \
	   RANLIB="$(RANLIB)" HDRS="$(PHDRS)" \
	   INCLUDE="$(INCLUDE) $(IPV6_INCLUDE) $(TLS_INCLUDE)"; \
	 echo " "

# The "clean", "install", and "makefile" targets just pass themselves back to
# the main Exim makefile. These targets will be obeyed only if "make" is obeyed
# for them in the build directory.

clean install makefile:; cd ..; $(MAKE) $(MFLAGS) build=$(build) $@

# Targets for building stand-alone testing programs for basic testing of
# some of the building blocks. These are not integrated with the makefile-
# building targets. If you change something that is going to cause the
# makefile to be rebuilt, you must run "make makefile" before running one
# of these.

# The testing programs use different versions of some modules - usually
# with bits cut out that are not relevant to the test in hand. For those
# that are used by several tests, we use a different name.

sa-globals.o:    $(HDRS) globals.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE -o sa-globals.o globals.c

sa-os.o:         $(HDRS) os.c
	$(CC) -c $(CFLAGS) $(INCLUDE) \
				      -DFIND_RUNNING_INTERFACES \
				      -o sa-os.o os.c

# These are the test targets themselves

test_dbfn:   config.h dbfn.c dummies.o sa-globals.o sa-os.o store.o \
	       string.o tod.o version.o
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE dbfn.c
	$(LNCC) -o test_dbfn $(LFLAGS) dbfn.o \
	  dummies.o sa-globals.o sa-os.o store.o string.o \
	  tod.o version.o $(LIBS) $(DBMLIB)
	rm -f dbfn.o

test_host:   config.h host.c dns.c dummies.o sa-globals.o sa-os.o store.o \
	       string.o tod.o tree.o
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE -DTEST_HOST host.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE -DTEST_HOST dns.c
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE -DTEST_HOST dummies.c
	$(LNCC) -o test_host $(LFLAGS) \
	  host.o dns.o dummies.o sa-globals.o os.o store.o string.o tod.o tree.o \
	  $(LIBS) $(LIBRESOLV)
	rm -f dummies.o  host.o dns.o

test_os:     os.h os.c dummies.o sa-globals.o store.o string.o tod.o
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE os.c
	$(LNCC) -o test_os $(LFLAGS) os.o dummies.o \
	  sa-globals.o store.o string.o tod.o $(LIBS)
	rm -f os.o

test_parse:  config.h parse.c dummies.o sa-globals.o \
	     store.o string.o tod.o version.o
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE parse.c
	$(LNCC) -o test_parse $(LFLAGS) parse.o \
	  dummies.o sa-globals.o store.o string.o tod.o version.o
	rm -f parse.o

test_string: config.h string.c dummies.o sa-globals.o store.o tod.o
	$(CC) -c $(CFLAGS) $(INCLUDE) -DSTAND_ALONE string.c
	$(LNCC) -o test_string $(LFLAGS) -DSTAND_ALONE string.o \
	  dummies.o sa-globals.o store.o tod.o $(LIBS)
	rm -f string.o

# End
