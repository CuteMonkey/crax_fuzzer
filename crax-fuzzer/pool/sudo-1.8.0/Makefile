#
# Copyright (c) 2010-2011 Todd C. Miller <Todd.Miller@courtesan.com>
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

srcdir = .
devdir = $(srcdir)
top_builddir = .
top_srcdir = .

# Installation paths for package building
prefix = /usr/local
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
sbindir = $(exec_prefix)/sbin
sysconfdir = /etc
libexecdir = $(exec_prefix)/libexec
includedir = $(prefix)/include
datarootdir = $(prefix)/share
localstatedir = ${prefix}/var
docdir = $(datarootdir)/doc/$(PACKAGE_TARNAME)
mandir = $(prefix)/man
timedir = /var/lib/sudo

# sudoers owner and mode for package building
sudoersdir = $(sysconfdir)
sudoers_uid = 0
sudoers_gid = 0
sudoers_mode = 0440

SUBDIRS = compat common  plugins/sudoers src include doc

SAMPLES = plugins/sample plugins/sample_group

VERSION = 1.8.0
PACKAGE_TARNAME = sudo

LIBTOOL_DEPS = ./ltmain.sh

SHELL = /bin/bash

all check: config.status
	for d in $(SUBDIRS) $(SAMPLES); do (cd $$d && $(MAKE) $@) || break; done

install-dirs install-binaries install-includes install-plugin uninstall: config.status
	for d in $(SUBDIRS); do (cd $$d && $(MAKE) $@) || break; done

install install-doc: config.status ChangeLog
	for d in $(SUBDIRS); do (cd $$d && $(MAKE) $@) || break; done

autoconf:
	autoconf -I m4

ChangeLog:
	if test -d $(srcdir)/.hg && cd $(srcdir); then \
	    if hg log --style=changelog -b default > $@.tmp; then \
		mv -f $@.tmp $@; \
	    else \
		rm -f $@.tmp; \
	    fi; \
	fi

config.status:
	@if [ ! -s config.status ]; then \
		echo "Please run configure first"; \
		exit 1; \
	fi

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

Makefile: $(srcdir)/Makefile.in
	./config.status --file Makefile

dist: ChangeLog $(srcdir)/MANIFEST
	pax -w -x ustar -s '/^/$(PACKAGE_TARNAME)-$(VERSION)\//' \
	    -f ../$(PACKAGE_TARNAME)-$(VERSION).tar \
	    `sed 's/[ 	].*//' $(srcdir)/MANIFEST`
	gzip -9f ../$(PACKAGE_TARNAME)-$(VERSION).tar
	ls -l ../$(PACKAGE_TARNAME)-$(VERSION).tar.gz

package: sudo.pp
	DESTDIR=`cd $(top_builddir) && pwd`/destdir; rm -rf $$DESTDIR; \
	$(MAKE) install DESTDIR=$$DESTDIR && \
	$(SHELL) $(srcdir)/pp $(PPFLAGS) \
	    --destdir=$$DESTDIR \
	    $(srcdir)/sudo.pp \
	    bindir=$(bindir) \
	    sbindir=$(sbindir) \
	    libexecdir=$(libexecdir) \
	    includedir=$(includedir) \
	    timedir=$(timedir) \
	    mandir=$(mandir) \
	    docdir=$(docdir) \
	    sysconfdir=$(sysconfdir) \
	    sudoersdir=$(sudoersdir) \
	    sudoers_uid=$(sudoers_uid) \
	    sudoers_gid=$(sudoers_gid) \
	    sudoers_mode=$(sudoers_mode) \
	    version=$(VERSION) $(PPVARS)

clean: config.status
	for d in $(SUBDIRS) $(SAMPLES); do (cd $$d && $(MAKE) $@); done

mostlyclean: clean

distclean: config.status
	for d in $(SUBDIRS) $(SAMPLES); do (cd $$d && $(MAKE) $@); done
	-rm -rf Makefile pathnames.h config.h config.status config.cache \
		config.log libtool stamp-* autom4te.cache

cleandir: distclean

clobber: distclean

realclean: distclean

.PHONY: ChangeLog
