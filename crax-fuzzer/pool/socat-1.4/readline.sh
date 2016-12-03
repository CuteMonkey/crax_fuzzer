#! /bin/bash
# $Id: readline.sh,v 1.2 2004/06/06 17:33:22 gerhard Exp $
# Copyright Gerhard Rieger 2003-2004
# Published under the GNU General Public License V.2, see file COPYING

# this is an attempt for a socat based readline wrapper
# usage: readline.sh <command>

withhistfile=1

while true; do
    case "X$1" in
    X-nh|X-nohist*) withhistfile=; shift; continue ;;
    *) break;;
    esac
done

PROGRAM="$@"
if [ "$withhistfile" ]; then
    HISTFILE="$HOME/.$2_history"
    $HISTOPT=",history=$HISTFILE"
else
    HISTOPT=
fi
mkdir -p /tmp/$USER || exit 1
#
#

exec socat -d readline"$HISTOPT",noecho='[Pp]assword:' exec:"$PROGRAM",sigint,pty,setsid,ctty,raw,echo=0,stderr 2>/tmp/$USER/stderr2

