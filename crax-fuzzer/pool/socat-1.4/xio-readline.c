/* $Id: xio-readline.c,v 1.9 2004/06/20 21:28:36 gerhard Exp $ */
/* Copyright Gerhard Rieger 2002-2004 */
/* Published under the GNU General Public License V.2, see file COPYING */

/* this file contains the source for opening the readline address */

#include "xiosysincludes.h"
#include "xioopen.h"

#include "xio-termios.h"
#include "xio-readline.h"


#if WITH_READLINE

/*
options: history file
	prompt
	mode=vi?
	inputrc=?

uses stdin!!
*/

/* length of buffer for dynamic prompt */
#define READLINE_MAXPROMPT 512

static int xioopen_readline(char *arg, int rw, xiofile_t *xfd, unsigned groups,
			    int dummy1, int dummy2, void *dummyp1);


const struct addrdesc addr_readline = {
   "readline", 3, xioopen_readline, GROUP_FD|GROUP_TERMIOS|GROUP_READLINE, 0, 0, NULL HELP(NULL) };

const struct optdesc opt_history_file = { "history-file", "history", OPT_HISTORY_FILE, GROUP_READLINE, PH_LATE, TYPE_STRING, OFUNC_OFFSET, (int)&((xiofile_t *)0)->stream.para.readline.history_file };
const struct optdesc opt_prompt       = { "prompt",       NULL,      OPT_PROMPT,       GROUP_READLINE, PH_LATE, TYPE_STRING, OFUNC_OFFSET, (int)&((xiofile_t *)0)->stream.para.readline.prompt };
const struct optdesc opt_noprompt     = { "noprompt",     NULL,      OPT_NOPROMPT,     GROUP_READLINE, PH_LATE, TYPE_BOOL,   OFUNC_SPEC,   0 };
const struct optdesc opt_noecho       = { "noecho",       NULL,      OPT_NOECHO,       GROUP_READLINE, PH_LATE, TYPE_STRING, OFUNC_SPEC,   0 };

static int xioopen_readline(char *arg, int rw, xiofile_t *xfd, unsigned groups,
			    int dummy1, int dummy2, void *dummyp1) {
   char msgbuf[256], *cp = msgbuf;
   struct opt *opts = NULL;
   bool noprompt = false;
   char *noecho = NULL;

   strcpy(cp, "using "); cp = strchr(cp, '\0');
   if ((rw+1)&1) {
      strcpy(cp, "readline on stdin for reading"); cp = strchr(cp, '\0');

      if ((rw+1)&2)
      strcpy(cp, " and ");  cp = strchr(cp, '\0');
   }
   if ((rw+1)&2) {
      strcpy(cp, "stdio for writing"); cp = strchr(cp, '\0');
   }
   Notice(msgbuf);

#if 0
   if (*arg != '\0' && *arg != ',') {
      Error("readline takes no parameters");
      return -1;
   }
#endif

   xfd->stream.fd = 0;	/* stdin */
   xfd->stream.howtoend = END_NONE;
   xfd->stream.dtype    = DATA_READLINE;

#if WITH_TERMIOS
   if (Isatty(xfd->stream.fd)) {
      if (Tcgetattr(xfd->stream.fd, &xfd->stream.savetty) < 0) {
	 Warn2("cannot query current terminal settings on fd %d. %s",
	       xfd->stream.fd, strerror(errno));
      } else {
	 xfd->stream.ttyvalid = true;
      }
   }
#endif /* WITH_TERMIOS */

   if (parseopts(arg, groups|groupbits(xfd->stream.fd), &opts) < 0)
      return STAT_NORETRY;
   applyopts(-1, opts, PH_INIT);

   applyopts2(xfd->stream.fd, opts, PH_INIT, PH_FD);

   Using_history();
   applyopts_offset(xfd, opts);
   retropt_bool(opts, OPT_NOPROMPT, &noprompt);
   if (!noprompt && !xfd->stream.para.readline.prompt) {
      xfd->stream.para.readline.dynbytes = READLINE_MAXPROMPT;
      xfd->stream.para.readline.dynprompt =
	 Malloc(xfd->stream.para.readline.dynbytes+1);
      xfd->stream.para.readline.dynend =
	 xfd->stream.para.readline.dynprompt;
   }

#if HAVE_REGEX_H
   retropt_string(opts, OPT_NOECHO, &noecho);
   if (noecho) {
      int errcode;
      char errbuf[128];
      if ((errcode = regcomp(&xfd->stream.para.readline.noecho, noecho,
			     REG_EXTENDED|REG_NOSUB))
	  != 0) {
	 regerror(errcode, &xfd->stream.para.readline.noecho,
		  errbuf, sizeof(errbuf));
	 Error3("regcomp(%p, \"%s\", REG_EXTENDED|REG_NOSUB): %s",
		&xfd->stream.para.readline.noecho, noecho, errbuf);
	 return -1;
      }
      xfd->stream.para.readline.hasnoecho = true;
   }
#endif /* HAVE_REGEX_H */
   if (xfd->stream.para.readline.history_file) {
      Read_history(xfd->stream.para.readline.history_file);
   }
   xiotermios_clrflag(xfd->stream.fd, 3, ICANON);
   xiotermios_clrflag(xfd->stream.fd, 3, ECHO);
   return _xio_openlate(&xfd->stream, opts);
}


ssize_t xioread_readline(struct single *pipe, void *buff, size_t bufsiz) {
   /*! indent */
   ssize_t bytes;
      char *line;

#if HAVE_REGEX_H
      if (pipe->para.readline.dynprompt &&
	  pipe->para.readline.hasnoecho &&
	  !regexec(&pipe->para.readline.noecho,
		   pipe->para.readline.dynprompt, 0, NULL, 0)) {
	 struct termios saveterm, setterm;
	 *pipe->para.readline.dynend = '\0';
	 Tcgetattr(pipe->fd, &saveterm);	/*! error */
	 setterm = saveterm;
	 setterm.c_lflag |= ICANON;
	 Tcsetattr(pipe->fd, TCSANOW, &setterm);	/*!*/
	 bytes = Read(pipe->fd, buff, bufsiz);	/*!*/
	 setterm.c_lflag &= ~ICANON;
	 Tcgetattr(pipe->fd, &setterm);	/*! error */
	 Tcsetattr(pipe->fd, TCSANOW, &saveterm);	/*!*/
	 pipe->para.readline.dynend = pipe->para.readline.dynprompt;
	 /*Write(pipe->fd, "\n", 1);*/	/*!*/
	 return bytes;
      }
#endif /* HAVE_REGEX_H */

      xiotermios_setflag(pipe->fd, 3, ECHO);
      if (pipe->para.readline.prompt || pipe->para.readline.dynprompt) {
	 /* we must carriage return, because readline will first print the
	    prompt */
	 ssize_t writt;
	 do {
	    writt = Write(pipe->fd, "\r", 1);
	 } while (writt < 0 && errno == EINTR);
	 if (writt < 0) {
	    Warn2("write(%d, \"\\r\", 1): %s",
		   pipe->fd, strerror(errno));
	 } else if (writt < 1) {
	    Warn1("write() only wrote "F_Zu" of 1 byte", writt);
	 }
      }

      if (pipe->para.readline.dynprompt) {
	 *pipe->para.readline.dynend = '\0';
	 line = Readline(pipe->para.readline.dynprompt);
	 pipe->para.readline.dynend = pipe->para.readline.dynprompt;
      } else {
	 line = Readline(pipe->para.readline.prompt);
      }
      if (line == NULL) {
	 return 0;	/* EOF */
      }
      xiotermios_clrflag(pipe->fd, 3, ECHO);
      Add_history(line);
      bytes = strlen(line);
      strncpy(buff, line, bufsiz);
      free(line);
      if ((size_t)bytes < bufsiz) {
	 strcat(buff, "\n");  ++bytes;
      }
      return bytes;
}

void xioscan_readline(struct single *pipe, const void *buff, size_t bytes) {
   if (pipe->dtype == DATA_READLINE && pipe->para.readline.dynprompt) {
      /* we save the last part of the output as possible prompt */
      const void *ptr = buff;
      const void *pcr = memrchr(buff, '\r', bytes);
      const void *plf = memrchr(buff, '\n', bytes);
      size_t len;
      if (bytes > pipe->para.readline.dynbytes) {
	 ptr = (const char *)buff + bytes - pipe->para.readline.dynbytes;
      }
      if (pcr) {
	 /* forget old prompt */
	 pipe->para.readline.dynend = pipe->para.readline.dynprompt;
	 /* new prompt starts here */
	 ptr = (const char *)pcr+1;
      }
      if (plf && plf >= ptr) {
	 /* forget old prompt */
	 pipe->para.readline.dynend = pipe->para.readline.dynprompt;
	 /* new prompt starts here */
	 ptr = (const char *)plf+1;
      }
      len = (const char *)buff-(const char *)ptr+bytes;
      if (pipe->para.readline.dynend - pipe->para.readline.dynprompt + len >
	  pipe->para.readline.dynbytes) {
	 memmove(pipe->para.readline.dynprompt,
		 pipe->para.readline.dynend -
		    (pipe->para.readline.dynbytes - len),
		 pipe->para.readline.dynbytes - len);
	 pipe->para.readline.dynend =
	    pipe->para.readline.dynprompt + pipe->para.readline.dynbytes - len;
      }
      memcpy(pipe->para.readline.dynend, ptr, len);
      /*pipe->para.readline.dynend = pipe->para.readline.dynprompt + len;*/
      pipe->para.readline.dynend = pipe->para.readline.dynend + len;
   }
   return;
}

#endif /* WITH_READLINE */
