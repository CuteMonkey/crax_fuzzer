/* HTGET (C) J Whitham 1998-99
** based on snarf, the Simple Non-interactive All-purpose Resource Fetcher
** snarf is copyright (C) 1995, 1996 Zachary Beane
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILIY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** The author of this program may be reached via email at
** jwhitham@globalnet.co.uk
*/

/* htget version 0.93 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define MAXLEN 		256
#define BIG_BUFFER_SIZE 4096
#define BASE64_END 	'='

const char base64_table[64]= "ABCDEFGHIJKLMNOPQRSTUVWXYZ" 
                             "abcdefghijklmnopqrstuvwxyz"
                             "0123456789+/";
			     
#define HEADER_INDICATOR	"TTP"
#define	USER_PASS		"anonymous:altrn@"
#define MY_NAME			"alternate 1"
#define CONTENT_LENGTH		"Content-Length:"

#define MAX_URLS		32

#define OPTIONS			15

/* The HTGet function can return these errors: */
#define ERROR_TIMEOUT			1
#define ERROR_WRITE				2
#define ERROR_ABORT				3
#define ERROR_RESUME_IMPOSSIBLE	4
#define ERROR_HTTP				5
#define ERROR_NOT_NEEDED		6	/* file is already complete */
#define ERROR_TCP				7
#define DOWNLOAD_OK				8
#define ERROR_PARSE				9
#define ERROR_LIST				10	
#define ERROR_FOUR_OH_FOUR		11	

	
char	ShowHelp ;
long	Timeout ;
long	Redial ;
long	Resume ;
char	Verbose ;
long	MinRate ;
char	ShowBytesRemaining ;
char	ShowTimeRemaining ;
char	ShowPercentage ;
char	ShowBytesPerSecond ;
char	DownloadsDir [ 128 ] ;
char	UserAgent [ 128 ] ;
char	ShowBanner ;




char * MakeHTTPRequest ( char * Hostname , char * Filename ) ;
int TCPConnect ( char * Hostname , unsigned int Port ) ;
int ReadLine ( int Socket , char * ReceiveBuffer ) ;
char * ToBase64(unsigned char *bin, int len ) ;
int ProcessURL ( char * TheURL , char * Hostname , char * Filename , char * ActualFilename , unsigned * Port ) ;
int GetHeaderForRequest ( unsigned socket , char * request , char * Header ) ;
int GetHeaderData(const char *Header, const char *FieldName, char *Record);
int HTGet(char *RetrieveURL, int AllowResume);
void PrintReport(long ArrivedSize, long RangeSize,
                 const struct timeval *StartTime, int TimeoutTime);
void tick(int signal);
			

int main ( int argc , char * argv [] )
{
	char		Temp [ 130 ] ;
	int			I , URLs , Ok , CurrentURL , Success ;
	FILE		* ResourceFile ;
	char		* Home ;
	char		RetrieveURL [ MAX_URLS ][ 128 ] ;

	/* open the resource file. */
	/* first search cmd line params to see if one has been specified. */
	ResourceFile = NULL ;
	for ( I = 1 ; I < argc ; I ++ )
	{
		if ( strncasecmp ( "--rcfile=" , argv [ I ] , 9 ) == 0 )
		{
			ResourceFile = fopen ( & argv [ I ][ 9 ] , "rt" ) ;
			if ( ResourceFile == NULL )
			{
				fprintf ( stderr , "%s: couldn't open rc file %s\n" , argv [ 0 ] , & argv [ I ][ 9 ] ) ;
				exit ( 1 ) ;
			}
		}
	}
	if ( ResourceFile == NULL )
	{
		/* try a few other locations */
		ResourceFile = fopen ( "htgetrc" , "rt" ) ;
		if ( ResourceFile == NULL )
		{
			Home = (char *) getenv ( "HOME" ) ;
			if ( Home == NULL )
			{
				strcpy ( Temp , ".htgetrc" ) ;
			} else {
				strcpy ( Temp , Home ) ;
				strcat ( Temp , "/.htgetrc" ) ;
			}
			ResourceFile = fopen ( Temp , "rt" ) ;
			if ( ResourceFile == NULL )
			{
				ResourceFile = fopen ( "/etc/htget.conf" , "rt" ) ;
				if ( ResourceFile == NULL )
				{
					fprintf ( stderr , "%s: couldn't find a htget resource file in the current\n"
							"directory or your home directory. Please read\n"
							"the documentation that came with htget for more details\n" ,
							argv [ 0 ] ) ;
				}
			}
		}
	}

	/* set up some default values */
	Timeout = 30 ;
	Redial = 10 ;
	Resume = 1 ;
	MinRate = 1000 ;
	Verbose = 0 ;
	ShowBytesRemaining = 1 ;
	ShowTimeRemaining = 1 ;
	ShowPercentage = 1 ;
	ShowBytesPerSecond = 1 ;
	strcpy ( DownloadsDir , "./" ) ;
	strcpy ( UserAgent , "linux htget 0.93" ) ;
	ShowBanner = 1 ;

	if ( ResourceFile != NULL )
	{
		while ( ! feof ( ResourceFile ) )
		{
			fgets ( Temp , 128 , ResourceFile ) ;
			/* Remove terminating newline/junk */
			for ( I = 0 ; I < (signed int) strlen ( Temp ) ; I ++ )
			{
				if ( ! isprint ( Temp [ I ] ))
				{
					Temp [ I ] = '\0' ;
					break ;
				}
			}
			if ( Temp [ 0 ] != '#' )
			{
				if ( strncasecmp ( Temp , "Timeout=" , 8 ) == 0 )
				{
					Timeout = atoi ( & Temp [ 8 ] ) ;
					if ( Timeout < 0 )
					{
						fprintf ( stderr , "Timeout value cannot be negative!\n" ) ;
						return ( 1 ) ;
					}
				}
				if ( strncasecmp ( Temp , "Redial=" , 7 ) == 0 )
				{
					Redial = atoi ( & Temp [ 7 ] ) ;
					if ( Redial < 0 )
					{
						fprintf ( stderr , "Redial delay value cannot be negative!\n" ) ;
						return ( 1 ) ;
					}
				}
				if ( strncasecmp ( Temp , "MinRate=" , 8 ) == 0 )
				{
					MinRate = atoi ( & Temp [ 8 ] ) ;
					if ( MinRate < 0 )
					{
						fprintf ( stderr , "MinRate value cannot be negative!\n" ) ;
						return ( 1 ) ;
					}
				}
				if ( strncasecmp ( Temp , "Resume=" , 7 ) == 0 )
				{
					Resume = ( atoi ( & Temp [ 7 ] ) == 1 ) ;
				}
				if ( strncasecmp ( Temp , "Verbose=" , 8 ) == 0 )
				{
					Verbose = ( atoi ( & Temp [ 8 ] ) == 1 ) ;
				}
				if ( strncasecmp ( Temp , "ShowBytesRemaining=" , 19 ) == 0 )
				{
					ShowBytesRemaining = ( atoi ( & Temp [ 19 ] ) == 1 ) ;
				}
				if ( strncasecmp ( Temp , "ShowPercentage=" , 15 ) == 0 )
				{
					ShowPercentage = ( atoi ( & Temp [ 15 ] ) == 1 ) ;
				}
				if ( strncasecmp ( Temp , "ShowBytesPerSecond=" , 19 ) == 0 )
				{
					ShowBytesPerSecond = ( atoi ( & Temp [ 19 ] ) == 1 ) ;
				}
				if ( strncasecmp ( Temp , "ShowBanner=" , 11 ) == 0 )
				{
					ShowBanner = ( atoi ( & Temp [ 11 ] ) == 1 ) ;
				}
				if ( strncasecmp ( Temp , "DownloadsDir=" , 13 ) == 0 )
				{
					strcpy ( DownloadsDir , & Temp [ 13 ] ) ;
				}
				if ( strncasecmp ( Temp , "UserAgent=" , 10 ) == 0 )
				{
					strcpy ( UserAgent , & Temp [ 10 ] ) ;
				}
			}
		}
		fclose ( ResourceFile ) ;
	}

	if ( ShowBanner )
	{
		fprintf ( stderr , "HTGET (c) J Whitham 1998-99  <jwhitham@globalnet.co.uk> version 0.93\n" 
			"See README. This program comes with NO WARRANTY of any kind.\n\n" ) ;
	}

	/* parse cmd line */
	URLs = 0 ;
	for ( I = 1 ; I < argc ; I ++ )
	{
		if ( argv [ I ][ 0 ] == '-' )
		{
			if (( strncasecmp ( argv [ I ] , "--help" , 6 ) == 0 )
			|| ( strncasecmp ( argv [ I ] , "-h" , 2 ) == 0 )
			|| ( strncasecmp ( argv [ I ] , "-?" , 2 ) == 0 ))
			{
				ShowHelp = 1 ;
			}
			else if ( strncasecmp ( argv [ I ] , "--verbose" , 9 ) == 0 )
			{
				Verbose = 1 ;
			}
			else if ( strncasecmp ( argv [ I ] , "--noverbose" , 11 ) == 0 )
			{
				Verbose = 0 ;
			}
			else if ( strncasecmp ( argv [ I ] , "--resume" , 8 ) == 0 )
			{
				Resume = 1 ;
			}
			else if (( strncasecmp ( argv [ I ] , "--noresume" , 10 ) == 0 )
			|| ( strncasecmp ( argv [ I ] , "-n" , 2 ) == 0 ))
			{
				Resume = 0 ;
			}
			else if ( strncasecmp ( argv [ I ] , "--downloadsdir=" , 15 ) == 0 )
			{
				strcpy ( DownloadsDir , & argv [ I ][ 15 ] ) ;
			}
			else if ( strncasecmp ( argv [ I ] , "--currentdir" , 12 ) == 0 )
			{
				strcpy ( DownloadsDir , "./" ) ;
			}
			else if ( strncasecmp ( argv [ I ] , "--timeout=" , 10 ) == 0 )
			{
				Timeout = atoi ( & argv [ I ][ 10 ] ) ;
				if ( Timeout < 0 )
				{
					fprintf ( stderr , "Timeout must be a positive integer\n" ) ;
					return ( 1 ) ;
				}
			}
			else if ( strncasecmp ( argv [ I ] , "--redial=" , 9 ) == 0 )
			{
				Redial = atoi ( & argv [ I ][ 9 ] ) ;
				if ( Redial < 0 )
				{
					fprintf ( stderr , "Redial must be a positive integer\n" ) ;
					return ( 1 ) ;
				}
			}
			else if ( strncasecmp ( argv [ I ] , "--minrate=" , 10 ) == 0 )
			{
				MinRate = atoi ( & argv [ I ][ 10 ] ) ;
				if ( MinRate < 0 )
				{
					fprintf ( stderr , "MinRate must be a positive integer\n" ) ;
					return ( 1 ) ;
				}
			}
			else if ( strncasecmp ( argv [ I ] , "--rcfile=" , 9 ) == 0 )
			{
				/* already dealt with this one */
			} else {
				fprintf ( stderr , "Unrecognised parameter: %s\n" , argv [ I ] ) ;
			}
		} else {
			if ( strlen ( argv [ I ] ) > 0 )
			{
				strcpy ( RetrieveURL [ URLs ] , argv [ I ] ) ;
				URLs ++ ; 
				if ( URLs > MAX_URLS )
				{
					fprintf ( stderr , "The maximum number of URLs that can be specified at the command line is %d.\n" , MAX_URLS ) ;
					return ( 0 ) ;
				}
			}
		}
	}
	if ( ( URLs < 1 ) || ShowHelp )  
	{
		fprintf ( stderr , "Usage: htget <urls..> [options]\n\n"
			"Options include:\n"
			"--help      -h  This text\n"
			"--verbose       Verbose download mode on\n"
			"--noverbose     Verbose download mode off\n"
			"--resume        D/load resuming on\n"
			"--noresume      D/load resuming off\n"
			"--timeout=t     Set timeout to t seconds\n"
			"--rcfile=rc     Specify alternative resource file rc\n"
			"\n\n"
			"For more information, see the htget README file\n"
			"or enter: man htget\n" ) ;
		return ( 1 ) ;
	}

	Ok = 1 ;
	CurrentURL = 0 ;
	Success = 0 ;
	while (( Ok ) && ( CurrentURL < URLs ))
	{
		fprintf ( stderr , "\n" ) ;
		Ok = 1 ;
		switch ( HTGet ( RetrieveURL [ CurrentURL ] , Resume ) )
		{
			case ERROR_WRITE :	
						fprintf ( stderr , "\nError writing to local file!\n" ) ;
						CurrentURL ++ ;
						break ;
			case ERROR_NOT_NEEDED :	
						CurrentURL ++ ;
						break ;
					/* All of these are unrecoverable errors */
			case ERROR_PARSE :	CurrentURL ++ ;	/* next please */
						break ;
			case DOWNLOAD_OK :	Success ++ ;
						CurrentURL ++ ;	
						break ;
			case ERROR_RESUME_IMPOSSIBLE :	/* I hate this! */
						break ;
			case ERROR_TCP :
			case ERROR_HTTP :
						if ( Redial == 0 )
						{
							CurrentURL ++ ;
						} else {
							fprintf ( stderr , "\nRedial: delaying for %ld seconds before retry.." , Redial ) ;
							sleep ( Redial ) ; 
							fprintf ( stderr , "\n" ) ;
						}
						break ;
			case ERROR_FOUR_OH_FOUR :
						fprintf ( stderr , "\nFile not found on server.\n" ) ;
						CurrentURL ++ ;	
						break ;
			case ERROR_ABORT :	
						Ok = 0 ;
						break ;
			case ERROR_TIMEOUT :	/* just try again */
						if ( Redial > 0 )
						{
							fprintf ( stderr , "\nRedial: delaying for %ld seconds before retry.." , Redial ) ;
							sleep ( Redial ) ; 
							fprintf ( stderr , "\n" ) ;
						}
						break ;
		}				
	}
	
	fprintf ( stderr , "\n\nhtget: Successfully downloaded %d of %d URLs\n" , Success , URLs ) ;
	return ( 0 ) ;
}

int HTGet ( char * RetrieveURL , int AllowResume ) 
{
	/* request-header */
	char		Header [ BIG_BUFFER_SIZE ] ;
	char		Request [ BIG_BUFFER_SIZE ] ;
	/* tcp/ip */
	unsigned int	socket ;
	/* URL */
	int		I , Port ;
	char		Hostname [ MAXLEN ] ;
	char		Filename [ MAXLEN ] ;
	char		ActualFilename [ MAXLEN ] ;
	int		Resuming = 0 ;
	/* output */
	FILE		* File ;
	long		ActualSize;
 	char 		recv_buf [ BIG_BUFFER_SIZE ] ;
	long		CurrentSize ;
	long		RangeStart = 0 ;
	/* authenticate */
	char		UserPass [ MAXLEN ] ;
	char		* base64str ;

	/* import status globals */
  extern int ArrivedSize;
  extern int RangeSize;
  extern struct timeval StartTime;
  /* extern int ticks; */

  RangeSize = 0;

	strcpy ( UserPass , USER_PASS ) ;
	
	if ( ! ProcessURL ( RetrieveURL , Hostname , Filename , ActualFilename , & Port ) )
	{
		fprintf ( stderr , "Couldn't parse URL %s\n" , RetrieveURL ) ;
		return ( ERROR_PARSE ) ;
	}

	/* apply alternate directory stuff */
	for ( I = strlen ( DownloadsDir ) - 1 ; I >= 0 ; I -- )
	{
		if ( DownloadsDir [ I ] == '/' )
		{
			DownloadsDir [ I ] = '\0' ;
		} else {
			break ;
		}
	}
	strcat ( DownloadsDir , "/" ) ;
	strcpy ( recv_buf , ActualFilename ) ;
	strcpy ( ActualFilename , DownloadsDir ) ;
	strcat ( ActualFilename , recv_buf ) ;

	CurrentSize = 0 ;
	/* read/resume check */
	File = fopen ( ActualFilename , "rb" ) ;
	if ( File != NULL )
	{
		fseek ( File , 0 , SEEK_END ) ;
		CurrentSize = ftell ( File ) ;
		fclose ( File ) ;
	}
	/* write check */
	File = fopen ( ActualFilename , "ab" ) ;
	if ( File == NULL )
	{
		fprintf ( stderr , "Error: attempt to open file %s for writing failed.\n" , ActualFilename ) ;
		return ( ERROR_WRITE ) ;
	}
	fclose ( File ) ;
	ActualSize = -1 ;
	if (( CurrentSize > 0 ) && AllowResume )
	{
		/* See if resume is possible by finding out the file length,
		time and date */
		socket = TCPConnect ( Hostname , Port ) ;
		if ( socket == 0 )
		{
			return ( ERROR_TCP ) ; /* error messages already generated by TCPConnect */
		}
	
		fprintf ( stderr , "Getting header for %s.." , RetrieveURL ) ;
		base64str = ToBase64 ( UserPass , strlen ( UserPass ) ) ;
		sprintf ( Request , "HEAD %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"User-agent: %s\r\n"
			"Authorization: Basic %s\r\n\r\n" ,
			    Filename , Hostname , UserAgent , base64str ) ;
		free ( base64str ) ;

		if ( Verbose )
		{
			fprintf ( stderr , "\n** Sent: %s" , Request ) ;
		}
		I = GetHeaderForRequest ( socket , Request , Header ) ;
		if ( I > 0 )
		{
			if ( I == 404 )
			{
				return ( ERROR_FOUR_OH_FOUR ) ;
			} else {
				return ( ERROR_HTTP ) ;
			}
		}
		if ( Verbose )
		{
			fprintf ( stderr , "\n** Received: %s" , Header ) ;
		}
		/* get some data from the header..
		 * the actual size for file resumes */
		Resuming = 0 ;
		if ( GetHeaderData ( Header , "Content-Length:" , recv_buf ) )
		{
			ActualSize = atol ( recv_buf ) ;
			if ( CurrentSize > 0 )
			{
				if ( CurrentSize < ActualSize )
				{
					Resuming = 1 ;
					fprintf ( stderr , "Resuming download from %ld bytes\n" , CurrentSize ) ;
				}
				if ( CurrentSize == ActualSize )
				{
					fprintf ( stderr , "Remote and local size is the same!\n" ) ;
					return ( ERROR_NOT_NEEDED ) ;
				}
			}
		}
		/* next bit: is it really necessary to reopen the socket?? */
	} else {
		CurrentSize = 0 ;
		/* Avoid bugs in the next bit */
	}
		
	socket = TCPConnect ( Hostname , Port ) ;
	if ( socket == 0 )
	{
		return ( ERROR_TCP ) ;
	}
	fprintf ( stderr , "Getting data for %s.." , RetrieveURL ) ;
	if ( Resuming )
	{
		base64str = ToBase64 ( UserPass , strlen ( UserPass ) ) ;
		sprintf ( Request , "GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"User-agent: %s\r\n" 
			"Range: bytes=%ld-%ld\r\n"
			"Authorization: Basic %s\r\n\r\n" ,
				Filename , Hostname , UserAgent ,
				CurrentSize , ActualSize , base64str ) ;
		free ( base64str ) ;
	} else {
		base64str = ToBase64 ( UserPass , strlen ( UserPass ) ) ;
		sprintf ( Request , "GET %s HTTP/1.0\r\n"
			"Host: %s\r\n"
			"User-agent: %s\r\n" 
			"Authorization: Basic %s\r\n\r\n" ,
				Filename , Hostname , UserAgent , base64str ) ;
		free ( base64str ) ;
	}
	if ( Verbose )
	{
		fprintf ( stderr , "\n** Sent: %s" , Request ) ;
	}
	I = GetHeaderForRequest ( socket , Request , Header );
	if ( I > 0 )
	{	
		if ( I == 404 )
		{
			return ( ERROR_FOUR_OH_FOUR ) ;
		} else {
			return ( ERROR_HTTP ) ;
		}
	}

	if ( Verbose )
	{
		fprintf ( stderr , "\n** Received: %s" , Header ) ;
	}
	if ( Resuming )
	{
		RangeSize = ActualSize - CurrentSize ;
	} else {
		RangeSize = ActualSize ;
	}
	if ( GetHeaderData ( Header , "Content-Length:" , recv_buf ) )
	{
		RangeSize = atol ( recv_buf ) ;
		if ( ActualSize < 0 )
		{
			ActualSize = RangeSize ;
		}
	}
	RangeStart = 0 ;
	if ( GetHeaderData ( Header , "Content-Range:" , recv_buf ) )
	{
		/* example from RFC:
		 * Content-Range: bytes 21010-47021/47022
		 * add 6 bytes */
		RangeStart = atol ( & recv_buf [ 6 ] ) ;
	}
	if (( Resuming )
	&& (( RangeStart > CurrentSize ) || ( RangeStart < 128 )) )
	{
		fprintf ( stderr , "This server does not support resume\n" ) ;
		return ( ERROR_RESUME_IMPOSSIBLE ) ;
	}

	if (( CurrentSize > 0 ) && ( Resuming ) )
	{
		File = fopen ( ActualFilename , "ab" ) ;
	} else {
		File = fopen ( ActualFilename , "wb" ) ;
	}
	if ( File == NULL )
	{
		return ( ERROR_WRITE ) ;
	}
	if ( RangeStart > CurrentSize )
	{
		fprintf ( stderr , "The HTTP range is past the end of file\n" ) ;
		return ( ERROR_RESUME_IMPOSSIBLE ) ;
			/* well actually it isn't but this will force the
			gizmo to try again from 0 bytes */
	}
	fseek ( File , RangeStart , SEEK_SET ) ;
 	
  fprintf(stderr, "\nReceiving HTTP data..");
  fprintf(stderr, "..\nTotal size: %ld bytes\n", ActualSize);

  gettimeofday(&StartTime, NULL);

  {
  struct itimerval timer;
  struct sigaction action;
  extern int ticks;

  /* setup a signal handler */
  memset(&action, 0, sizeof(action));
  action.sa_handler = tick;
  sigaction(SIGALRM, &action, NULL);

  /* start the timer ticking */
  timer.it_interval.tv_sec = 1;
  timer.it_interval.tv_usec = 0;
  timer.it_value.tv_sec = 1;
  timer.it_value.tv_usec = 0;
  setitimer(ITIMER_REAL, &timer, NULL);

  ticks = Timeout;
  ArrivedSize = 0;

  while ((I = read(socket, recv_buf, sizeof(recv_buf))))
  {
    if (I > 0)
    {
      ticks = Timeout;

      if (fwrite(recv_buf, I, 1, File) != 1)
      {
        setitimer(ITIMER_REAL, NULL, NULL);
        fprintf(stderr, "\nError writing to file %s\n", ActualFilename);
        fclose(File);
        return (ERROR_WRITE);
      }

      if ((ArrivedSize += I) == RangeSize)
      {
        setitimer(ITIMER_REAL, NULL, NULL);
        fclose(File);
        fprintf(stderr, "\nFinished downloading.\n");
        return (DOWNLOAD_OK);
      }
    }
    else if (errno != EINTR)
    {
      setitimer(ITIMER_REAL, NULL, NULL);
      fprintf(stderr, "\nError reading from web server\n");
      fclose(File);
      return (ERROR_HTTP);
    }

    if (ticks <= 0)
    {
      setitimer(ITIMER_REAL, NULL, NULL);
      fclose(File);
      return (ERROR_TIMEOUT);
    }
  }
  }
  /* stop the timer */
  setitimer(ITIMER_REAL, NULL, NULL);
  fclose(File);
  fprintf(stderr, "\nFinished downloading.\n");
  return (DOWNLOAD_OK);
}


/* TCPConnect
 * Gets a socket number for the host after finding it's address
 * [ZB] */
int TCPConnect ( char * Hostname , unsigned int Port )
{
	struct hostent		* host ;
	struct sockaddr_in	sin ;
	int			sock_fd ;

	if( ( host = gethostbyname ( Hostname ) ) == NULL ) 
	{
		fprintf ( stderr , "Error finding host %s\n" , Hostname ) ;
		return ( 0 ) ;
	}

/* get the socket */
	if( ( sock_fd = socket ( AF_INET , SOCK_STREAM , 0 ) ) < 0 )
	{
		fprintf ( stderr , "Error creating TCP/IP socket\n" ) ;
		return ( 0 ) ;
	}

  /* connect the socket, filling in the important stuff */
	sin . sin_family = AF_INET ;
	sin . sin_port = htons ( Port ) ;
	memcpy ( & sin . sin_addr , host -> h_addr , host -> h_length ) ;
  
	if ( connect ( sock_fd , ( struct sockaddr * ) & sin , sizeof ( sin ) ) < 0 )
	{
		fprintf ( stderr , "Error connecting to %s\n" , Hostname ) ;
		return ( 0 ) ;
	}

	return ( sock_fd ) ;
}

/* ReadLine
 * Reads a \n terminated line (includes the \n)
 * [ZB] heavily modified by jw.*/
 
int ReadLine ( int Socket , char * ReceiveBuffer )
{
	int	rc = 0 ;
	char	ch ;
	int	I = 0 ;

	rc = read ( Socket , & ch , 1 ) ;
	while ( rc == 1 )
	{
		ReceiveBuffer [ I ] = ch ;
		I ++ ;
		if ( ch == '\n' )
		{
			break ;
		}
		if ( I > ( BIG_BUFFER_SIZE - 4 ))
		{
			break ;
		}
		rc = read ( Socket , & ch , 1 ) ;
	}
	if ( rc == 0 )
	{
		if ( I == 0 )
		{
			/* nothing read.. */
			return ( 0 ) ;
		}
		/* unexpected EOF */
	}
	if (( rc != 0 ) && ( rc != 1 ))
	{
		/* something has gone very wrong!? */
		return ( -1 ) ;
	}
	ReceiveBuffer [ I ] = '\0' ;
	return ( I ) ;
}
			
	
/* ToBase64 [ZB]
 * lauri alanko, i thank you */
char * ToBase64(unsigned char *bin, int len)
{
	char *buf= (char *)malloc((len+2)/3*4+1);
	int i=0, j=0;

	if ( buf == NULL )
	{
		puts ( "ToBase64: error allocating memory for output" ) ;
		exit ( 1 ) ;
	}
	
	while(j<len-2){
		buf[i++]=base64_table[bin[j]>>2];
		buf[i++]=base64_table[((bin[j]&3)<<4)|(bin[j+1]>>4)];
		buf[i++]=base64_table[((bin[j+1]&15)<<2)|(bin[j+2]>>6)];
		buf[i++]=base64_table[bin[j+2]&63];
		j+=3;
	}
	switch(len-j){
	    case 1:
		buf[i++]=base64_table[bin[j]>>2];
		buf[i++]=base64_table[(bin[j]&3)<<4];
		buf[i++]=BASE64_END;
		buf[i++]=BASE64_END;
		break;
	    case 2:
		buf[i++]=base64_table[bin[j]>>2];
		buf[i++]=base64_table[((bin[j]&3)<<4)|(bin[j+1]>>4)];
		buf[i++]=base64_table[(bin[j+1]&15)<<2];
		buf[i++]=BASE64_END;
		break;
	}
	buf[i]='\0';
	return buf;
}

/* ProcessURL
 * Converts TheURL to it's component parts. Uses defaults if necessary.
 * [JW] returns 0 if error. 1 if OK. */
int ProcessURL ( char * TheURL , char * Hostname , char * Filename , char * ActualFilename , unsigned * Port )
{
	char		BufferURL [ MAXLEN ] ;
	char		NormalURL [ MAXLEN ] ;
	int		I ;
	
	/* clear everything. Is this necessary? not sure */
	memset ( Hostname , '\0' , strlen ( TheURL ) ) ;
	memset ( Filename , '\0' , strlen ( TheURL ) ) ;
	memset ( ActualFilename , '\0' , strlen ( TheURL ) ) ;
	/* remove "http://" */
	strcpy ( BufferURL , TheURL ) ;
	/* BUGFIX: strlwr ( BufferURL )? */
	if ( strncmp ( BufferURL , "http://" , 7 ) == 0 )
	{
		strcpy ( NormalURL , & TheURL [ 7 ] ) ;
	} else {
		strcpy ( NormalURL , TheURL ) ;
	}
	if ( strlen ( NormalURL ) < 1 )
	{
		return ( 0 ) ;
	}
	/* scan for first /. */
	for ( I = 0 ; I < (signed int) strlen ( NormalURL ) ; I ++ )
	{
		if ( NormalURL [ I ] == '/' )
		{
			break ;
		}
	}
	/* whatever, I will be the end of the hostname */
	strncpy ( Hostname , NormalURL , I ) ;
	if ( I < 1 )
	{
		return ( 0 ) ; /* gone wrong -- too short */
	}
	strcpy ( BufferURL , & NormalURL [ I ] ) ;
	if ( strlen ( BufferURL ) < 1 )
	{
		/* no filename error! */
		/* Bugfix: this is still legal, but we must specify root */
		strcat ( BufferURL , "/" ) ;
	}
	(* Port) = 80 ; /* default HTTP port */
	for ( I = strlen ( BufferURL ) - 1 ; I >= 0 ; I -- )
	{
		if ( BufferURL [ I ] == ':' )
		{
			/* everything after I is a port number */
			(* Port) = (unsigned int) atol ( & BufferURL [ I + 1 ] ) ;
			BufferURL [ I ] = '\0' ;
			break ;
		}
	}
	/* that means that BufferURL is the Filename */
	strcpy ( Filename , BufferURL ) ;
	if ( strlen ( BufferURL ) < 1 )
	{
		/* no filename error, again */
		return ( 0 ) ;
	}
	/* now the ActualFilename, everything after the last / */
	for ( I = strlen ( Filename ) - 1 ; I >= 0 ; I -- )
	{
		if ( Filename [ I ] == '/' )
		{
			strcpy ( ActualFilename , & Filename [ I + 1 ] ) ;
			break ;
		}
	}
	if ( strlen ( ActualFilename ) < 1 )
	{
		/* This is probably because user asked for URL like: www.stuff.com/
		Make up a filename based on URL. */
		strcpy ( ActualFilename , Hostname ) ;
	}

	return ( 1 ) ;
}

/* GetHeaderForRequest .. gets the HTTP header for
 * request @ socket into Header */
int GetHeaderForRequest ( unsigned socket , char * request , char * Header )
{
	int		Timeout , I ;
	char		line [ MAXLEN ] ;
	char		status [ MAXLEN ] ;
	char		token [ MAXLEN ] ;
	int 		in_header = 0;
	char		ch ;

	Header [ 0 ] = '\0' ;
	send ( socket , request , strlen ( request ) , 0 ) ;

	/* hmm...let's do a bit of status checking by reading the numeric
	response code from the server */

	Timeout = 0 ;

	/* I don't know how much of this waiting stuff is really needed,
	but I put it in after some problems connecting to my local server,
	there are possibly some latency issues here? */
	
	fprintf ( stderr , ".." ) ;
	ch = '\0' ;
	I = read ( socket , & ch , 1 ) ;
	while ( I == 0 )
	{
		fprintf ( stderr , "." ) ;
		sleep ( 1 ) ;	
		I = read ( socket , & ch , 1 ) ;
	}

	if ( I == -1 )
	{
		fprintf ( stderr , "\nUnknown TCP/IP error\n" ) ;
		return ( 1 ) ;
	}

	strcpy ( status , "" ) ;
	if ( ! ReadLine ( socket , status ) )
	{
		fprintf ( stderr , "no response\n" )  ;
		return ( 1 ) ;
	} else {
		fprintf ( stderr , "\n" ) ;
	}
	/* this will re-append the check character read above to the 1st line */
	line [ 0 ] = ch ;
	line [ 1 ] = '\0' ;
	strcat ( line , status ) ;
	strcpy ( status , line ) ;

	/* scan line for "HTTP" to indicate this is the header */
	for ( I = 0 ; I < (signed int) ( strlen ( line ) - strlen ( HEADER_INDICATOR ) ) ; I ++ )
	{
		if ( strncmp ( & line [ I ] , HEADER_INDICATOR , strlen ( HEADER_INDICATOR ) ) == 0 )
		{
			in_header = 1 ;
			break ;
		}
	}
	
	if ( in_header )
	{
		strtok(line, " ");
		strcpy(token, (char *)strtok(NULL, " "));
		if ( token [ 0 ] == '4' || token [ 0 ] == '5' )
		{
			fprintf ( stderr , "http error from server: %s" , status ) ;
			if ( strncmp ( token , "404" , 3 ) == 0 )
			{
				return ( 404 ) ;
			} else {
				return ( 1 ) ;
			}
		}
	}
		
	while ( in_header )
	{
		if ( ! ReadLine(socket, line) ) 
		{
			in_header = 0;
			break;
		}

		if ( line [ 0 ] == '\n' || line [ 1 ] == '\n' )
		{
			in_header = 0;

		}
		strcat ( Header , line ) ;
	}
	return ( 0 ) ;
}


int NoCaseStrNCmp ( const char * String1 , const char * String2 , int Length )
{
	int	I ;

	for ( I = 0 ; I < Length ; I ++ )
	{
		if ( toupper ( String1 [ I ] ) != toupper ( String2 [ I ] ))
		{
			return ( 0 ) ;	
		}
	}
	return ( 1 ) ;
}

int GetHeaderData(const char *Header, const char *FieldName, char *Record)
{
	int	I , J ;

	for ( I = 0 ; I < (signed int) strlen ( Header ) ; I ++ )
	{
		if ( NoCaseStrNCmp ( & Header [ I ] , FieldName , strlen ( FieldName ) ) )
		{
			I += strlen ( FieldName ) ;
			if ( ( I + 2 ) < (signed int) strlen ( Header ))
			{
				strcpy ( Record , & Header [ I ] ) ;
				for ( J = 0 ; J < (signed int) strlen ( Record ) ; J ++ )
				{
					if ( Record [ J ] == '\n' )
					{
						Record [ J ] = '\0' ;
						break ;
					}
				}
			} else {
				return ( 0 ) ;
			}
			return ( 1 ) ;
		}
	}
	return ( 0 ) ;
}


void PrintReport(long ArrivedSize, long RangeSize,
                 const struct timeval *StartTime, int TimeoutTime)
{
  float now;
  float start;
  float BytesPerSecond;
  int TimeLeft;

  struct timeval CurrentTime;
  gettimeofday(&CurrentTime, NULL);

	fprintf ( stderr , "\r\033[K\r.." ) ;
	if ( ShowBytesRemaining )
  {
    if (RangeSize > 0)
      fprintf(stderr, " %ld/%ld", ArrivedSize, RangeSize);
    else
      fprintf(stderr, " %ld/unknown", ArrivedSize);
  }
  if ((ShowPercentage) && (RangeSize > 0))
    fprintf(stderr, " %.2f%%", (ArrivedSize * 100) / (float) RangeSize);

  start = StartTime->tv_sec + StartTime->tv_usec / 1000000;
  now = CurrentTime.tv_sec + CurrentTime.tv_usec / 1000000;
  if ((now - start) > 0)
  {
    BytesPerSecond = ArrivedSize / (now - start);
    if (ShowBytesPerSecond)
      fprintf(stderr, " %.2f bytes/sec", BytesPerSecond);
		if (( RangeSize > 0 ) && ShowTimeRemaining )
		{
			/* time left may be predicted. */
      TimeLeft = (RangeSize - ArrivedSize) / BytesPerSecond;
			if ( TimeLeft > 3599 )
			{
				/* hours and minutes */
				fprintf ( stderr , "%4dh %02dm" ,
					( TimeLeft / 3600 ) , (( TimeLeft / 60 ) % 60 ) ) ;
			} else {
				fprintf ( stderr , "%4dm %02ds" ,
					(( TimeLeft / 60 ) % 60 ) ,
					( TimeLeft % 60 ) ) ;
			}
		}
	}
  if (TimeoutTime < (Timeout - 1))
    fprintf(stderr, " Timeout in %ds", TimeoutTime);
}

/* this is the alarm signal handler... which decrements the timeout timer */
void tick(int signal)
{
  extern int ArrivedSize;
  extern int RangeSize;
  extern struct timeval StartTime;
  extern int ticks;

  signal = signal;
  ticks--;

  /* print out the report for each alarm */
  PrintReport(ArrivedSize, RangeSize, &StartTime, ticks);
}

/* ick globals */
int ArrivedSize;
int RangeSize;
struct timeval StartTime;
int ticks;
