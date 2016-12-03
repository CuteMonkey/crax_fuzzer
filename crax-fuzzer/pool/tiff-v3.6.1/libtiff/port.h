/*
 * Warning, this file was automatically created by the TIFF configure script
 * VERSION:	 v3.6.1
 * RELEASE:   
 * DATE:	 Sat Sep 14 17:40:15 EDT 2013
 * TARGET:	 i686-pc-linux-gnu
 * CCOMPILER:	 /usr/bin/gcc-4.4.5-8' --with-bugurl=file:///usr/share/doc/gcc-4.4/README.Bugs --enable-languages=c,c++,fortran,objc,obj-c++ --prefix=/usr --program-suffix=-4.4 --enable-shared --enable-multiarch --enable-linker-build-id --with-system-zlib --libexecdir=/usr/lib --without-included-gettext --enable-threads=posix --with-gxx-include-dir=/usr/include/c++/4.4 --libdir=/usr/lib --enable-nls --enable-clocale=gnu --enable-libstdcxx-debug --enable-objc-gc --enable-targets=all --with-arch-32=i586 --with-tune=generic --enable-checking=release --build=i486-linux-gnu --host=i486-linux-gnu --target=i486-linux-gnu 4.4.5-8)
 */
#ifndef _PORT_
#define _PORT_ 1
#ifdef __cplusplus
extern "C" {
#endif
#include <sys/types.h>
#define HOST_FILLORDER FILLORDER_MSB2LSB
#define HOST_BIGENDIAN	0
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
typedef double dblparam_t;
#ifdef __STRICT_ANSI__
#define	INLINE	__inline__
#else
#define	INLINE	inline
#endif
#define GLOBALDATA(TYPE,NAME)	extern TYPE NAME
#ifdef __cplusplus
}
#endif
#endif
