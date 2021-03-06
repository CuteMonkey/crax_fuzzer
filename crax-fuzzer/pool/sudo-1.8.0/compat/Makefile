#
# Copyright (c) 2011 Todd C. Miller <Todd.Miller@courtesan.com>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# compat/Makefile.  Generated from Makefile.in by configure.
#

#### Start of system configuration section. ####

srcdir = .
devdir = $(srcdir)
top_builddir = ..
top_srcdir = ..
incdir = $(top_srcdir)/include

# Compiler & tools to use
CC = gcc
LIBTOOL = $(SHELL) $(top_builddir)/libtool

# C preprocessor flags
CPPFLAGS = -I. -I$(top_builddir) -I$(incdir) 

# Usually -O and/or -g
CFLAGS = -rdynamic

# OS dependent defines
DEFS =  -D_GNU_SOURCE

#### End of system configuration section. ####

SHELL = /bin/bash

LIBOBJDIR = 

LTLIBOBJS =  ${LIBOBJDIR}strlcpy$U.lo ${LIBOBJDIR}strlcat$U.lo ${LIBOBJDIR}closefrom$U.lo

all: libreplace.la

Makefile: $(srcdir)/Makefile.in
	(cd $(top_builddir) && ./config.status --file compat/Makefile)

.SUFFIXES: .o .c .h .lo

.c.lo:
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $<

libreplace.la: $(LTLIBOBJS)
	$(LIBTOOL) --mode=link $(CC) -o $@ $(LTLIBOBJS) -no-install

./siglist.c: ./mksiglist
	./mksiglist > $@

./mksiglist: $(srcdir)/mksiglist.c $(srcdir)/mksiglist.h $(incdir)/missing.h $(top_builddir)/config.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/mksiglist.c -o $@

fnm_test: $(srcdir)/regress/fnmatch/fnm_test.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/regress/fnmatch/fnm_test.c -o $@

globtest: $(srcdir)/regress/glob/globtest.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/regress/glob/globtest.c -o $@

#$(srcdir)/mksiglist.h: $(srcdir)/siglist.in
#	awk 'BEGIN {print "/* public domain */\n"} /^    [A-Z]/ {printf("#ifdef SIG%s\n    if (my_sys_siglist[SIG%s] == NULL)\n\tmy_sys_siglist[SIG%s] = \"%s\";\n#endif\n", $$1, $$1, $$1, substr($$0, 13))}' < $(srcdir)/siglist.in > $@

# Dependencies
closefrom.lo: $(srcdir)/closefrom.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/closefrom.c
dlopen.lo: $(srcdir)/dlopen.c $(srcdir)/dlfcn.h $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/dlopen.c
fnmatch.lo: $(srcdir)/fnmatch.c $(srcdir)/fnmatch.h $(srcdir)/charclass.h $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/fnmatch.c
getcwd.lo: $(srcdir)/getcwd.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/getcwd.c
getline.lo: $(srcdir)/getline.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/getline.c
getprogname.lo: $(srcdir)/getprogname.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/getprogname.c
glob.lo: $(srcdir)/glob.c $(srcdir)/glob.h $(srcdir)/charclass.h $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/glob.c
isblank.lo: $(srcdir)/isblank.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/isblank.c
memrchr.lo: $(srcdir)/memrchr.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/memrchr.c
mktemp.lo: $(srcdir)/mktemp.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/mktemp.c
nanosleep.lo: $(srcdir)/nanosleep.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/nanosleep.c
siglist.lo: ./siglist.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) ./siglist.c
snprintf.lo: $(srcdir)/snprintf.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/snprintf.c
strcasecmp.lo: $(srcdir)/strcasecmp.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/strcasecmp.c
strerror.lo: $(srcdir)/strerror.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/strerror.c
strlcat.lo: $(srcdir)/strlcat.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/strlcat.c
strlcpy.lo: $(srcdir)/strlcpy.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/strlcpy.c
strsignal.lo: $(srcdir)/strsignal.c $(incdir)/missing.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/strsignal.c
utimes.lo: $(srcdir)/utimes.c $(incdir)/missing.h $(srcdir)/utime.h $(top_builddir)/config.h
	$(LIBTOOL) --mode=compile $(CC) -c -o $@ $(CPPFLAGS) $(CFLAGS) $(DEFS) $(srcdir)/utimes.c

install:

install-dirs:

install-binaries:

install-includes:

install-doc:

install-plugin:

uninstall:

check: fnm_test globtest
	@./fnm_test $(srcdir)/regress/fnmatch/fnm_test.in
	@mkdir -p `sed 's@/[^/]*$$@@' $(srcdir)/regress/glob/files | sort -u`
	@touch `cat $(srcdir)/regress/glob/files`
	@chmod 0755 `grep '/r[^/]*$$' $(srcdir)/regress/glob/files`
	@chmod 0444 `grep '/s[^/]*$$' $(srcdir)/regress/glob/files`
	@chmod 0711 `grep '/t[^/]*$$' $(srcdir)/regress/glob/files`
	@./globtest $(srcdir)/regress/glob/globtest.in
	@rm -rf fake

clean:
	-$(LIBTOOL) --mode=clean rm -f mksiglist siglist.c *.lo *.o *.la *.a stamp-* core *.core core.*

mostlyclean: clean

distclean: clean
	-rm -rf Makefile .libs

clobber: distclean

realclean: distclean
	rm -f TAGS tags

cleandir: realclean
