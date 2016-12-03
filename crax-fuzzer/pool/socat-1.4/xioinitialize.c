/* $Id: xioinitialize.c,v 1.11 2004/06/20 21:14:51 gerhard Exp $ */
/* Copyright Gerhard Rieger 2001-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for the initialize function */

#include "xiosysincludes.h"

#include "xioopen.h"


static int xioinitialized;

int xioinitialize(void) {
   if (xioinitialized)  return 0;

   /* configure and .h's cannot guarantee this */
   assert(sizeof(uint8_t)==1);
   assert(sizeof(uint16_t)==2);
   assert(sizeof(uint32_t)==4);

   /* assertions regarding O_ flags - important for XIO_READABLE() etc. */
   assert(O_RDONLY==0);
   assert(O_WRONLY==1);
   assert(O_RDWR==2);

   /* some assertions about termios */
#if WITH_TERMIOS
#ifdef CRDLY
   assert(3 << opt_crdly.arg3  == CRDLY);
#endif
#ifdef TABDLY
   assert(3 << opt_tabdly.arg3 == TABDLY);
#endif
   assert(3 << opt_csize.arg3  == CSIZE);
   {
      union {
	 struct termios termarg;
	 tcflag_t flags[4];
#if HAVE_TERMIOS_ISPEED
	 speed_t speeds[sizeof(struct termios)/sizeof(speed_t)];
#endif
      } tdata;
      tdata.termarg.c_iflag = 0x12345678;
      tdata.termarg.c_oflag = 0x23456789;
      tdata.termarg.c_cflag = 0x3456789a;
      tdata.termarg.c_lflag = 0x456789ab;
      assert(tdata.termarg.c_iflag == tdata.flags[0]);
      assert(tdata.termarg.c_oflag == tdata.flags[1]);
      assert(tdata.termarg.c_cflag == tdata.flags[2]);
      assert(tdata.termarg.c_lflag == tdata.flags[3]);
#if HAVE_TERMIOS_ISPEED
      tdata.termarg.c_ispeed = 0x56789abc;
      tdata.termarg.c_ospeed = 0x6789abcd;
      assert(tdata.termarg.c_ispeed == tdata.speeds[ISPEED_OFFSET]);
      assert(tdata.termarg.c_ospeed == tdata.speeds[OSPEED_OFFSET]);
#endif
   }
#endif

   /* these dependencies required in applyopts() for OFUNC_FCNTL */
   assert(F_GETFD == F_SETFD-1);
   assert(F_GETFL == F_SETFL-1);

   if (Atexit(xioexit) < 0) {
      Error("atexit(xioexit) failed");
      return -1;
   }
   xioinitialized = 1;
   return 0;
}
