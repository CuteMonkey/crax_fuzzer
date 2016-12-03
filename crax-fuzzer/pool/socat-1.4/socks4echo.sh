#! /bin/bash
# $Id: socks4echo.sh,v 1.1 2004/06/12 07:46:34 gerhard Exp $

# Copyright Gerhard Rieger 2004
# Published under the GNU General Public License V.2, see file COPYING

# perform primitive simulation of a socks4 server with echo function via stdio.
# accepts and answers correct SOCKS4 requests, but then just echoes data.
# it is required for test.sh
# for TCP, use this script as:
# socat tcp-l:1080,reuseaddr,crlf system:"proxyecho.sh"

if type socat >/dev/null 2>&1; then
    SOCAT=socat
else
    SOCAT=./socat
fi

case `uname` in
HP-UX|OSF1)
    CAT="$SOCAT -u stdin stdout"
    ;;
*)
    CAT=cat
    ;;
esac

if   [ $(echo "x\c") = "x" ]; then E=""
elif [ $(echo -e "x\c") = "x" ]; then E="-e"
else
    echo "cannot suppress trailing newline on echo" >&2
    exit 1
fi
ECHO="echo $E"

#SPACES=" "
#while [ -n "$1" ]; do
#    case "$1" in
#    -w) n="$2"; while [ "$n" -gt 0 ]; do SPACES="$SPACES "; n=$((n-1)); done
#	shift ;;
#    #-s) STAT="$2"; shift ;;
#    esac
#    shift
#done

# read and parse SOCKS4 header
read -r -n 1 vn
if [ "$vn" != $($ECHO "\4") ]; then
    $ECHO "\0\133\0\0\0\0\0\0\c"
    echo "invalid socks version requested" >&2
    exit
fi
read -r -n 1 cd
if [ "$cd" != $($ECHO "\1") ]; then
    $ECHO "\0\133\0\0\0\0\0\0\c"
    echo "invalid socks operation requested" >&2
    exit
fi
read -r -n 6 a
if [ "$a" != "$($ECHO "}m bL6")" ]; then
    $ECHO "\0\133\0\0\0\0\0\0\c"
    echo "wrong socks address or port requested" >&2
    exit
fi
read -r -n 7 u
if [ "$u" != "nobody" ]; then
    $ECHO "\0\133\0\0\0\0\0\0\c"
    echo "wrong socks user requested" >&2
    exit
fi

# send ok status
$ECHO "\0\132\0\0\0\0\0\0\c"

# perform echo function
$CAT
