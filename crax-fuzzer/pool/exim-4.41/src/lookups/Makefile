# Make file for building a library containing all the available lookups and
# calling it lookups.a. This is called from the main make file, after cd'ing
# to the lookups subdirectory. When the relevant LOOKUP_ macros are not
# defined, dummy modules get compiled.

OBJ = cdb.o dbmdb.o dnsdb.o dsearch.o ibase.o ldap.o lsearch.o mysql.o nis.o \
      nisplus.o oracle.o passwd.o pgsql.o testdb.o whoson.o lf_check_file.o

lookups.a:       $(OBJ)
		 /bin/rm -f lookups.a
		 $(AR) lookups.a $(OBJ)
		 $(RANLIB) $@
		 /bin/rm -rf ../drtables.o

.SUFFIXES:       .o .c
.c.o:;           $(CC) -c $(CFLAGS) $(INCLUDE) $*.c

lf_check_file.o: $(HDRS) lf_check_file.c  lf_functions.h

cdb.o:           $(HDRS) cdb.c       cdb.h
dbmdb.o:         $(HDRS) dbmdb.c     dbmdb.h
dnsdb.o:         $(HDRS) dnsdb.c     dnsdb.h
dsearch.o:       $(HDRS) dsearch.c   dsearch.h
ibase.o:         $(HDRS) ibase.c     ibase.h
ldap.o:          $(HDRS) ldap.c      ldap.h
lsearch.o:       $(HDRS) lsearch.c   lsearch.h
mysql.o:         $(HDRS) mysql.c     mysql.h
nis.o:           $(HDRS) nis.c       nis.h
nisplus.o:       $(HDRS) nisplus.c   nisplus.h
oracle.o:        $(HDRS) oracle.c    oracle.h
passwd.o:        $(HDRS) passwd.c    passwd.h
pgsql.o:         $(HDRS) pgsql.c     pgsql.h
testdb.o:        $(HDRS) testdb.c    testdb.h
whoson.o:        $(HDRS) whoson.c    whoson.h

# End
