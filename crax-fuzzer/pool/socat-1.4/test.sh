#! /bin/bash
# $Id: test.sh,v 1.81 2004/06/21 20:42:19 gerhard Exp $
# Copyright Gerhard Rieger 2001-2004
# Published under the GNU General Public License V.2, see file COPYING

# perform lots of tests on socat

# this script uses functions; you need a shell that supports them

#set -vx

withroot=0
#PATH=$PATH:/opt/freeware/bin
#PATH=$PATH:/usr/local/ssl/bin
#OPENSSL_RAND="-rand /dev/egd-pool"
#SOCAT_EGD="egd=/dev/egd-pool"
#MISCDELAY=1
[ -z "$SOCAT" ] && SOCAT="./socat"
PROCAN=./procan
opts="-t0.1 $OPTS"
export SOCAT_OPTS="$opts"
#debug="1"
debug=
TESTS="$@"
INTERFACE=eth0;
#LOCALHOST=192.168.58.1
LOCALHOST=localhost
PORT=12002
SOURCEPORT=2002
# on HP-UX, comment out the following line (hangs tests 14, 15)
PTYOPTS="echo=0,opost=0"
PTYOPTS2="raw,echo=0"
CAT=cat
OD_C="od -c"
# time in microseconds to wait in some situations
MICROS=100000
if ! type usleep >/dev/null 2>&1; then
    usleep () {
	sleep $((($1+999999)/1000000))
    }
fi
#USLEEP=usleep

UNAME=`uname`
case "$UNAME" in
HP-UX|OSF1)
    echo "$SOCAT -u stdin stdout" >cat.sh
    chmod a+x cat.sh
    CAT=./cat.sh
    ;;
*)
    CAT=cat
    ;;
esac

case "$UNAME" in
HP-UX)
    PTYOPTS=
    PTYOPTS2=
    ;;
*)
    PTYOPTS="echo=0,opost=0"
    PTYOPTS2="raw,echo=0"
    ;;
esac

TRUE=$(which true)
#E=-e	# Linux
if   [ $(echo "x\c") = "x" ]; then E=""
elif [ $(echo -e "x\c") = "x" ]; then E="-e"
else
    echo "cannot suppress trailing newline on echo" >&2
    exit 1
fi
ECHO="echo $E"

case "$TERM" in
vt100|xterm)	RED="\23331m"
	GREEN="\23332m"
	YELLOW="\23333m"
	NORMAL="\23339m"
	OK="${GREEN}OK${NORMAL}"
	FAILED="${RED}FAILED${NORMAL}"
	NO_RESULT="${YELLOW}NO RESULT${NORMAL}"
	;;
*)	OK="OK"
	FAILED="FAILED"
	NO_RESULT="NO RESULT"
	;;
esac


if [ -x /usr/xpg4/bin/id ]; then
    # SunOS has rather useless tools in its default path
    PATH="/usr/xpg4/bin:$PATH"
fi

[ -z "$TESTS" ] && TESTS="CONSISTENCY FUNCTIONS"

[ -z "$USER" ] && USER="$LOGNAME"	# HP-UX
TD="/tmp/$USER/$$"; td="$TD"
rm -rf "$TD" || (echo "cannot rm $TD" >&2; exit 1)
mkdir -p "$TD"
#trap "rm -r $TD" 0 3

echo "using temp directory $TD"

case "$TESTS" in
*CONSISTENCY*)
# test if addresses are sorted alphabetically:
$ECHO "testing if address array is sorted...\c"
TF="$TD/socat-q"
IFS="$($ECHO ' \n\t')"
$SOCAT -? |sed '1,/address-head:/ d' |egrep 'groups=' |while IFS="$IFS:" read x y; do echo "$x"; done >"$TF"
$SOCAT -? |sed '1,/address-head:/ d' |egrep 'groups=' |while IFS="$IFS:" read x y; do echo "$x"; done |LC_ALL=C sort |diff "$TF" - >"$TF-diff"
if [ -s "$TF-diff" ]; then
    $ECHO "\n*** address array is not sorted. Wrong entries:" >&2
    cat "$TD/socat-q-diff" >&2
else
    echo " ok"
fi
#/bin/rm "$TF"
#/bin/rm "$TF-diff"
esac

case "$TESTS" in
*CONSISTENCY*)
# test if address options array ("optionnames") is sorted alphabetically:
$ECHO "testing if address options are sorted...\c"
TF="$TD/socat-qq"
$SOCAT -??? |sed '1,/opt:/ d' |egrep 'groups=' |awk '{print($1);}' >"$TF"
$SOCAT -??? |sed '1,/opt:/ d' |egrep 'groups=' |awk '{print($1);}' |LC_ALL=C sort |diff "$TF" - >"$TF-diff"
if [ -s "$TF-diff" ]; then
    $ECHO "\n*** option array is not sorted. Wrong entries:" >&2
    cat "$TD/socat-qq-diff" >&2
else
    echo " ok"
fi
/bin/rm "$TF"
/bin/rm "$TF-diff"
esac

#==============================================================================
case "$TESTS" in
*OPTIONS*)

# inquire which options are available
OPTS_ANY=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*ANY' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_BLK=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*BLK' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_CHILD=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*CHILD' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_CHR=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*CHR' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_DEVICE=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*DEVICE' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_EXEC=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*EXEC' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_FD=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*FD' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_FIFO=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*FIFO' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_FORK=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*FORK' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_LISTEN=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*LISTEN' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_NAMED=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*NAMED' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_OPEN=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*OPEN[^S]' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_RETRY=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*RETRY' |awk '{print($1);}' |grep -v forever|xargs echo |tr ' ' ',')
OPTS_RANGE=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*RANGE' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_FILE=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*REG' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_UNIX=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*UNIX' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_SOCKET=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*SOCKET' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_TERMIOS=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*TERMIOS' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_IP4=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*IP4' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_IP6=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*IP6' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_TCP=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*TCP' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_UDP=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*UDP' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_SOCKS4=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*SOCKS4' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_PROCESS=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*PROCESS' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_OPENSSL=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*OPENSSL' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_PTY=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*PTY' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_HTTP=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*HTTP' |awk '{print($1);}' |xargs echo |tr ' ' ',')
OPTS_APPL=$($SOCAT -?? |sed '1,/opt:/ d' |egrep 'groups=([A-Z]+,)*APPL' |awk '{print($1);}' |xargs echo |tr ' ' ',')

# find user ids to setown to; non-root only can setown to itself
if [ $(id -u) = 0 ]; then
  # up to now, it is not a big problem when these do not exist
  _UID=nobody
  _GID=staff
else
  _UID=$(id -u)
  _GID=$(id -g)
fi

# some options require values; here we try to replace these bare options with
#    valid forms.
filloptionvalues() {
    local OPTS=",$1,"
    case "$OPTS" in
    *,umask,*) OPTS=$(echo "$OPTS" |sed "s/,umask,/,umask=0026,/g");;
    esac
    case "$OPTS" in
    *,user,*) OPTS=$(echo "$OPTS" |sed "s/,user,/,user=$_UID,/g");;
    esac
    case "$OPTS" in
    *,user-early,*) OPTS=$(echo "$OPTS" |sed "s/,user-early,/,user-early=$_UID,/g");;
    esac
    case "$OPTS" in
    *,user-late,*) OPTS=$(echo "$OPTS" |sed "s/,user-late,/,user-late=$_UID,/g");;
    esac
    case "$OPTS" in
    *,owner,*) OPTS=$(echo "$OPTS" |sed "s/,owner,/,owner=$_UID,/g");;
    esac
    case "$OPTS" in
    *,uid,*) OPTS=$(echo "$OPTS" |sed "s/,uid,/,uid=$_UID,/g");;
    esac
    case "$OPTS" in
    *,uid-l,*) OPTS=$(echo "$OPTS" |sed "s/,uid-l,/,uid-l=$_UID,/g");;
    esac
    case "$OPTS" in
    *,setuid,*) OPTS=$(echo "$OPTS" |sed "s/,setuid,/,setuid=$_UID,/g");;
    esac
    case "$OPTS" in
    *,group,*) OPTS=$(echo "$OPTS" |sed "s/,group,/,group=$_GID,/g");;
    esac
    case "$OPTS" in
    *,group-early,*) OPTS=$(echo "$OPTS" |sed "s/,group-early,/,group-early=$_GID,/g");;
    esac
    case "$OPTS" in
    *,group-late,*) OPTS=$(echo "$OPTS" |sed "s/,group-late,/,group-late=$_GID,/g");;
    esac
    case "$OPTS" in
    *,gid,*) OPTS=$(echo "$OPTS" |sed "s/,gid,/,gid=$_GID,/g");;
    esac
    case "$OPTS" in
    *,gid-l,*) OPTS=$(echo "$OPTS" |sed "s/,gid-l,/,gid-l=$_GID,/g");;
    esac
    case "$OPTS" in
    *,setgid,*) OPTS=$(echo "$OPTS" |sed "s/,setgid,/,setgid=$_GID,/g");;
    esac
    case "$OPTS" in
    *,mode,*) OPTS=$(echo "$OPTS" |sed "s/,mode,/,mode=0700,/g");;
    esac
    case "$OPTS" in
    *,perm,*) OPTS=$(echo "$OPTS" |sed "s/,perm,/,perm=0700,/g");;
    esac
    case "$OPTS" in
    *,perm-early,*) OPTS=$(echo "$OPTS" |sed "s/,perm-early,/,perm-early=0700,/g");;
    esac
    case "$OPTS" in
    *,perm-late,*) OPTS=$(echo "$OPTS" |sed "s/,perm-late,/,perm-late=0700,/g");;
    esac
    case "$OPTS" in
    *,path,*) OPTS=$(echo "$OPTS" |sed "s/,path,/,path=.,/g");;
    esac
    case "$OPTS" in
    *,bind,*) OPTS=$(echo "$OPTS" |sed "s/,bind,/,bind=:,/g");;
    esac
    case "$OPTS" in
    *,linger,*) OPTS=$(echo "$OPTS" |sed "s/,linger,/,linger=2,/g");;
    esac
    case "$OPTS" in
    *,rcvtimeo,*) OPTS=$(echo "$OPTS" |sed "s/,rcvtimeo,/,rcvtimeo=1,/g");;
    esac
    case "$OPTS" in
    *,sndtimeo,*) OPTS=$(echo "$OPTS" |sed "s/,sndtimeo,/,sndtimeo=1,/g");;
    esac
    case "$OPTS" in
    *,ipoptions,*) OPTS=$(echo "$OPTS" |sed "s|,ipoptions,|,ipoptions=x01,|g");;
    esac
    case "$OPTS" in
    *,range,*) OPTS=$(echo "$OPTS" |sed "s|,range,|,range=127.0.0.1/32,|g");;
    esac
    case "$OPTS" in
    *,if,*) OPTS=$(echo "$OPTS" |sed "s/,if,/,if=$INTERFACE,/g");;
    esac
    case "$OPTS" in
    *,history,*) OPTS=$(echo "$OPTS" |sed "s/,history,/,history=.history,/g");;
    esac
    case "$OPTS" in
    *,sp,*) OPTS=$(echo "$OPTS" |sed "s/,sp,/,sp=$SOURCEPORT,/g");;
    esac
    case "$OPTS" in
    *,ciphers,*) OPTS=$(echo "$OPTS" |sed "s/,ciphers,/,ciphers=NULL,/g");;
    esac
    case "$OPTS" in
    *,method,*) OPTS=$(echo "$OPTS" |sed "s/,method,/,method=SSLv3,/g");;
    esac
    case "$OPTS" in
    *,proxyauth,*) OPTS=$(echo "$OPTS" |sed "s/,proxyauth,/,proxyauth=user:pass,/g");;
    esac
    case "$OPTS" in
    *,proxyport,*) OPTS=$(echo "$OPTS" |sed "s/,proxyport,/,proxyport=3128,/g");;
    esac
    case "$OPTS" in
    *,link,*) OPTS=$(echo "$OPTS" |sed "s/,link,/,link=testlink,/g");;
    esac
    echo $OPTS >&2
    expr "$OPTS" : ',\(.*\),'
}
# OPTS_FIFO: nothing yet

# OPTS_CHR: nothing yet

# OPTS_BLK: nothing yet

# OPTS_REG: nothing yet

OPTS_SOCKET=",$OPTS_SOCKET,"
OPTS_SOCKET=$(expr "$OPTS_SOCKET" : ',\(.*\),')

N=1
#------------------------------------------------------------------------------

#method=open
#METHOD=$(echo "$method" |tr a-z A-Z)
#TEST="$METHOD on file accepts all its options"
#    echo "### $TEST"
#TF=$TD/file$N
#DA="$(date)"
#OPTGROUPS=$($SOCAT -? |fgrep " $method:" |sed 's/.*=//')
#for g in $(echo $OPTGROUPS |tr ',' ' '); do
#    eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
#    OPTS="$OPTS,$OPTG";
#done
##echo $OPTS
#
#for o in $(filloptionvalues $OPTS|tr ',' ' '); do
#    echo testing if $METHOD accepts option $o
#    touch $TF
#    $SOCAT $opts -!!$method:$TF,$o /dev/null,ignoreof </dev/null
#    rm -f $TF
#done

#------------------------------------------------------------------------------

# test openssl connect

#set -vx
if true; then
#if false; then
#opts="-s -d -d -d -d"
pid=$!
for addr in openssl; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
#    echo OPTGROUPS=$OPTGROUPS
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    echo $OPTS
	openssl s_server -www -accept $PORT || echo "cannot start s_server" >&2 &
	pid=$!
	sleep 1
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
#	echo $SOCAT $opts /dev/null $addr:$LOCALHOST:$PORT,$o
	$SOCAT $opts /dev/null $addr:$LOCALHOST:$PORT,$o
    done
	kill $pid
done
kill $pid 2>/dev/null
opts=
	PORT=$((PORT+1))
fi

#------------------------------------------------------------------------------

# test proxy connect

#set -vx
if true; then
#if false; then
#opts="-s -d -d -d -d"
pid=$!
for addr in proxy; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
#    echo OPTGROUPS=$OPTGROUPS
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    echo $OPTS
    # prepare dummy server
    $SOCAT tcp-l:$PORT,reuseaddr,crlf exec:"bash proxyecho.sh" || echo "cannot start proxyecho.sh" >&2 &
	pid=$!
	sleep 1
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
#	echo $SOCAT $opts /dev/null $addr:$LOCALHOST:127.0.0.1:$PORT,$o
	$SOCAT $opts /dev/null $addr:$LOCALHOST:127.0.0.1:$PORT,$o
    done
	kill $pid
done
kill $pid 2>/dev/null
opts=
	PORT=$((PORT+1))
fi

#------------------------------------------------------------------------------

# test tcp4

#set -vx
if true; then
#if false; then
#opts="-s -d -d -d -d"
$SOCAT $opts tcp4-listen:$PORT,reuseaddr,fork,$o echo </dev/null &
pid=$!
for addr in tcp4; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
#    echo OPTGROUPS=$OPTGROUPS
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts /dev/null $addr:$LOCALHOST:$PORT,$o
    done
done
kill $pid 2>/dev/null
opts=
PORT=$((PORT+1))
fi

#------------------------------------------------------------------------------

# test udp4

#set -vx
if true; then
#if false; then
#opts="-s -d -d -d -d"
$SOCAT $opts udp4-listen:$PORT,fork,$o echo </dev/null &
pid=$!
for addr in udp4; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
#    echo OPTGROUPS=$OPTGROUPS
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts /dev/null $addr:$LOCALHOST:$PORT,$o
    done
done
kill $pid 2>/dev/null
opts=
PORT=$((PORT+1))
fi

#------------------------------------------------------------------------------

# test tcp4-listen

#set -vx
if true; then
#if false; then
#opts="-s -d -d -d -d"
for addr in tcp4-listen; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts $ADDR:$PORT,reuseaddr,$o echo </dev/null &
	pid=$!
	$SOCAT /dev/null tcp4:$LOCALHOST:$PORT 2>/dev/null
	kill $pid 2>/dev/null
    done
done
opts=
PORT=$((PORT+1))
fi

#------------------------------------------------------------------------------

# test udp4-listen

#set -vx
if true; then
#if false; then
#opts="-s -d -d -d -d"
for addr in udp4-listen; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts $ADDR:$PORT,reuseaddr,$o echo </dev/null &
	pid=$!
	$SOCAT /dev/null udp4:$LOCALHOST:$PORT 2>/dev/null
	kill $pid 2>/dev/null
    done
done
opts=
PORT=$((PORT+1))
fi

#------------------------------------------------------------------------------

# test READLINE

if true; then
#if false; then
#opts="-s -d -d -d -d"
for addr in readline; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    TS=$TD/script$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr	" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
#    for o in bs0; do
	echo "testing if $ADDR accepts option $o"
	echo "$SOCAT $opts readline,$o /dev/null" >$TS
	chmod u+x $TS
	$SOCAT /dev/null,ignoreeof exec:$TS,pty
	#stty sane
    done
    #reset 1>&0 2>&0
done
opts=
fi

#------------------------------------------------------------------------------

# unnamed pipe
#if false; then
if true; then
for addr in pipe; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="unnamed $ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |egrep " $addr[^:]" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS

    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if unnamed $ADDR accepts option $o
	$SOCAT $opts $addr,$o /dev/null </dev/null
    done
done
fi

#------------------------------------------------------------------------------

# test addresses on files

N=1
#if false; then
if true; then
for addr in create; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR on new file accepts all its options"
    echo "### $TEST"
    TF=$TD/file$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS

    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR accepts option $o
	rm -f $TF
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
	rm -f $TF
    done
done
fi
#------------------------------------------------------------------------------

#if false; then
if true; then
for addr in exec system; do
    ADDR=$(echo "$addr" |tr a-z A-Z)

    TEST="$ADDR with socketpair accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,FIFO,/,/g' -e 's/,TERMIOS,/,/g' -e '/,PTY,/,/g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    OPTS=$(echo $OPTS|sed -e 's/,pipes,/,/g' -e 's/,pty,/,/g' -e 's/,openpty,/,/g' -e 's/,ptmx,/,/g' -e 's/,nofork,/,/g')
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR with socketpair accepts option $o
	$SOCAT $opts $addr:$TRUE,$o /dev/null,ignoreof </dev/null
    done

    TEST="$ADDR with pipes accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,TERMIOS,/,/g' -e '/,PTY,/,/g' -e 's/,SOCKET,/,/g' -e 's/,UNIX//g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    # flock tends to hang, so dont test it
    OPTS=$(echo $OPTS|sed -e 's/,pipes,/,/g' -e 's/,pty,/,/g' -e 's/,openpty,/,/g' -e 's/,ptmx,/,/g' -e 's/,nofork,/,/g' -e 's/,flock,/,/g')
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR with pipes accepts option $o
	$SOCAT $opts $addr:$TRUE,pipes,$o /dev/null,ignoreof </dev/null
    done

    TEST="$ADDR with pty accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,FIFO,/,/g' -e 's/,SOCKET,/,/g' -e 's/,UNIX//g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    OPTS=$(echo $OPTS|sed -e 's/,pipes,/,/g' -e 's/,pty,/,/g' -e 's/,openpty,/,/g' -e 's/,ptmx,/,/g' -e 's/,nofork,/,/g')
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR with pty accepts option $o
	$SOCAT $opts $addr:$TRUE,pty,$o /dev/null,ignoreof </dev/null
    done

    TEST="$ADDR with nofork accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,FIFO,/,/g' -e 's/,PTY,/,/g' -e 's/,TERMIOS,/,/g' -e 's/,SOCKET,/,/g' -e 's/,UNIX//g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    OPTS=$(echo $OPTS|sed -e 's/,pipes,/,/g' -e 's/,pty,/,/g' -e 's/,openpty,/,/g' -e 's/,ptmx,/,/g' -e 's/,nofork,/,/g')
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR with nofork accepts option $o
	$SOCAT /dev/null $opts $addr:$TRUE,nofork,$o </dev/null
    done

done
fi

#------------------------------------------------------------------------------

#if false; then
if true; then
for addr in fd; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    TF=$TD/file$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR (to file) accepts option $o"
	rm -f $TF
	$SOCAT $opts -u /dev/null $addr:3,$o 3>$TF
    done
done
fi

#------------------------------------------------------------------------------

# test OPEN address

#! test it on pipe, device, new file

N=1
#if false; then
if true; then
for addr in open; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR on file accepts all its options"
    echo "### $TEST"
    TF=$TD/file$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on file accepts option $o
	touch $TF
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
	rm -f $TF
    done
done
fi

#------------------------------------------------------------------------------

# test GOPEN address on files, sockets, pipes, devices

N=1
#if false; then
if true; then
for addr in gopen; do
    ADDR=$(echo "$addr" |tr a-z A-Z)

    TEST="$ADDR on new file accepts all its options"
    echo "### $TEST"
    TF=$TD/file$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,SOCKET,/,/g' -e 's/,UNIX//g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on new file accepts option $o
	rm -f $TF
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
	rm -f $TF
    done

    TEST="$ADDR on existing file accepts all its options"
    echo "### $TEST"
    TF=$TD/file$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,SOCKET,/,/g' -e 's/,UNIX//g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on existing file accepts option $o
	rm -f $TF; touch $TF
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
	rm -f $TF
    done

    TEST="$ADDR on existing pipe accepts all its options"
    echo "### $TEST"
    TF=$TD/pipe$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,REG,/,/g' -e 's/,SOCKET,/,/g' -e 's/,UNIX//g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on named pipe accepts option $o
	rm -f $TF; mkfifo $TF
	$SOCAT $opts $addr:$TF,$o,nonblock /dev/null </dev/null
	rm -f $TF
    done

    TEST="$ADDR on existing socket accepts all its options"
    echo "### $TEST"
    TF=$TD/sock$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,REG,/,/g' -e 's/,OPEN,/,/g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on socket accepts option $o
	rm -f $TF; $SOCAT - UNIX-L:$TF & pid=$!
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
	kill $pid 2>/dev/null
	rm -f $TF
    done

  if [ $(id -u) -eq 0 ]; then
    TEST="$ADDR on existing device accepts all its options"
    echo "### $TEST"
    TF=$TD/null
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,REG,/,/g' -e 's/,OPEN,/,/g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on existing device accepts option $o
	rm -f $TF; mknod $TF c 1 3
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
    done
  else
    TEST="$ADDR on existing device accepts all its options"
    echo "### $TEST"
    TF=/dev/null
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTGROUPS=$(echo $OPTGROUPS |sed -e 's/,REG,/,/g' -e 's/,OPEN,/,/g')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if $ADDR on existing device accepts option $o
	$SOCAT $opts -!!$addr:$TF,$o /dev/null,ignoreof </dev/null
    done
  fi

done
fi

#------------------------------------------------------------------------------

# test named pipe

N=1
#if false; then
if true; then
for addr in pipe; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR on file accepts all its options"
    echo "### $TEST"
    TF=$TD/pipe$N
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS

    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo testing if named $ADDR accepts option $o
	rm -f $TF
	# blocks with rdonly, wronly
	case "$o" in rdonly|wronly) o="$o,nonblock" ;; esac
	$SOCAT $opts $addr:$TF,$o /dev/null </dev/null
	rm -f $TF
    done
done
fi
#------------------------------------------------------------------------------

# test STDIO

#! test different stream types

#if false; then
if true; then
for addr in stdio; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS

    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR (/dev/null, stdout) accepts option $o"
	$SOCAT $opts $addr,$o /dev/null,ignoreof </dev/null
    done
done
fi

#------------------------------------------------------------------------------

# test STDIN

#if false; then
if true; then
for addr in stdin; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr	" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS

    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR (/dev/null) accepts option $o"
	$SOCAT $opts -u $addr,$o /dev/null </dev/null
    done
done
fi

#------------------------------------------------------------------------------

# test STDOUT, STDERR

if true; then
#if false; then
for addr in stdout stderr; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr	" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts -u /dev/null $addr,$o
    done
done
fi

#------------------------------------------------------------------------------
# REQUIRES ROOT

if [ "$withroot" ]; then
for addr in ip4; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts $addr:127.0.0.1:200 /dev/null,ignoreof </dev/null
    done
done
fi

#------------------------------------------------------------------------------
# REQUIRES ROOT

if [ "$withroot" ]; then
for addr in ip6; do
    ADDR=$(echo "$addr" |tr a-z A-Z)
    TEST="$ADDR accepts all its options"
    echo "### $TEST"
    OPTGROUPS=$($SOCAT -? |fgrep " $addr:" |sed 's/.*=//')
    OPTS=
    for g in $(echo $OPTGROUPS |tr ',' ' '); do
	eval "OPTG=\$OPTS_$(echo $g |tr a-z A-Z)";
	OPTS="$OPTS,$OPTG";
    done
    #echo $OPTS
    for o in $(filloptionvalues $OPTS|tr ',' ' '); do
	echo "testing if $ADDR accepts option $o"
	$SOCAT $opts $addr:::1:200 /dev/null,ignoreof </dev/null
    done
done
fi

#==============================================================================

#TEST="stdio accepts all options of GROUP_ANY"
#echo "### $TEST"
#CMD="$SOCAT $opts -,$OPTS_ANY /dev/null"
#$CMD
#if [ $? = 0 ]; then
#    echo "... test $N ($TEST) succeeded"
##    echo "CMD=$CMD"
#else
#    echo "*** test $N ($TEST) FAILED"
#    echo "CMD=$CMD"
#fi
#
#N=$((N+1))
##------------------------------------------------------------------------------
#
#TEST="exec accepts all options of GROUP_ANY and GROUP_SOCKET"
#echo "### $TEST"
#CMD="$SOCAT $opts exec:$TRUE,$OPTS_ANY,$OPTS_SOCKET /dev/null"
#$CMD
#if [ $? = 0 ]; then
#    echo "... test $N ($TEST) succeeded"
##    echo "CMD=$CMD"
#else
#    echo "*** test $N ($TEST) FAILED"
#    echo "CMD=$CMD"
#fi

#------------------------------------------------------------------------------

esac

#==============================================================================

N=1
#==============================================================================
# test if selected socat features work ("FUNCTIONS")

testecho () {
    local num="$1"
    local title="$2"
    local arg1="$3";	[ -z "$arg1" ] && arg1="-"
    local arg2="$4";	[ -z "$arg2" ] && arg2="echo"
    local opts="$5"
    local T="$6";	[ -z "$T" ] && T=0
    local tf="$td/test$N.stdout"
    local te="$td/test$N.stderr"
    local tdiff="$td/test$N.diff"
    local da="$(date)"
    #local cmd="$SOCAT $opts $arg1 $arg2"
    #$ECHO "testing $title (test $num)... \c"
    printf "test %2d %s... " $num "$title"
    #echo "$da" |$cmd >"$tf" 2>"$te"
    (echo "$da"; sleep $T) |$SOCAT $opts "$arg1" "$arg2" >"$tf" 2>"$te"
    if [ "$?" != 0 ]; then
	$ECHO "$FAILED: $SOCAT:"
	echo "$SOCAT $opts $arg1 $arg2"
	cat "$te"
    elif echo "$da" |diff - "$tf" >"$tdiff" 2>&1; then
	$ECHO "$OK"
	if [ -n "$debug" ]; then cat $te; fi
    else
	$ECHO "$FAILED: diff:"
	echo "$SOCAT $opts $arg1 $arg2"
	cat "$te"
	cat "$tdiff"
    fi
}

# test if call to od and throughput of data works - with graceful shutdown and
# flush of od buffers
testod () {
    local num="$1"
    local title="$2"
    local arg1="$3";	[ -z "$arg1" ] && arg1="-"
    local arg2="$4";	[ -z "$arg2" ] && arg2="echo"
    local opts="$5"
    local T="$6";	[ -z "$T" ] && T=0
    local tf="$td/test$N.stdout"
    local te="$td/test$N.stderr"
    local tdiff="$td/test$N.diff"
    local dain="$(date)"
    local daout="$(echo "$dain" |od -c)"
    printf "test %2d %s... " $num "$title"
    (echo "$dain"; sleep $T) |$SOCAT $opts "$arg1" "$arg2" >"$tf" 2>"$te"
    if [ "$?" != 0 ]; then
	$ECHO "$FAILED: $SOCAT:"
	echo "$SOCAT $opts $arg1 $arg2"
	cat "$te"
    elif echo "$daout" |diff - "$tf" >"$tdiff" 2>&1; then
	$ECHO "$OK"
	if [ -n "$debug" ]; then cat $te; fi
    else
	$ECHO "$FAILED: diff:"
	echo "$SOCAT $opts $arg1 $arg2"
	cat "$te"
	cat "$tdiff"
    fi
}

# test if the socat executable has these address types compiled in
# print the first missing address type
testaddrs () {
    local a A;
    for a in $@; do
	A=$(echo "$a" |tr 'a-z' 'A-Z')
	if $SOCAT -V |grep "#define WITH_$A 1\$" >/dev/null; then
	    shift
	    continue
	fi
	echo "$a"
	return -1
    done
    return 0
}

# check if an IP6 loopback interface exists
runsip6 () {
    local l
    case $(uname) in
    AIX)   l=$(/usr/sbin/ifconfig lo0 |grep 'inet6 ::1/0') ;;
    HP-UX) l=$(/usr/sbin/ifconfig lo0 |grep ' inet6 ') ;;
    Linux) l=$(/sbin/ifconfig |grep 'inet6 addr: ::1/') ;;
    NetBSD)l=$(/sbin/ifconfig -a |grep ' inet6 ::1 ');;
    OSF1)  l=$(/sbin/ifconfig -a |grep ' inet6 ') ;;
    SunOS) l=$(ifconfig -a |grep ' inet6 ') ;;
    *)     l=$(/sbin/ifconfig -a |grep ' ::1[^:0-9A-Fa-f]') ;;
    esac
    [ -n "$l" ];
}

# wait until a TCP4 listen port is ready
waittcp4port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case $(uname) in
	Linux)   l=$(netstat -an |grep '^tcp .* .*[0-9*]:'$port' .* LISTEN') ;;
	FreeBSD) l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
	Darwin)  l=$(netstat -an |grep '^tcp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
	AIX)	 l=$(netstat -an |grep '^tcp[^6]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	SunOS)   l=$(netstat -an -f inet |grep '.*[1-9*]\.'$port' .*\*                0 .* LISTEN') ;;
	HP-UX)   l=$(netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' .* LISTEN$') ;;
	OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') ;;
 	*)       l=$(netstat -an |grep -i 'tcp .*[0-9*][:.]'$port' .* listen') ;;
	esac
	[ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	  \( \( $logic -eq 0 \) -a -z "$l" \) ] && return 0
	sleep 1
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    return 1
}

# wait until a UDP4 port is ready
waitudp4port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case $(uname) in
	Linux)   l=$(netstat -an |grep '^udp .* .*[0-9*]:'$port' [ ]*0\.0\.0\.0:\*') ;;
	FreeBSD) l=$(netstat -an |grep '^udp4[6 ] .*[0-9*]\.'$port' .* \*\.\*') ;;
	NetBSD)  l=$(netstat -an |grep '^udp .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	OpenBSD) l=$(netstat -an |grep '^udp .*[0-9*]\.'$port' [ ]* \*\.\*') ;;
	Darwin)  l=$(netstat -an |grep '^udp4.* .*[0-9*]\.'$port' .* \*\.\* .* LISTEN') ;;
	AIX)	 l=$(netstat -an |grep '^udp[4 ]       0      0 .*[*0-9]\.'$port' .* \*\.\*[ ]*$') ;;
	SunOS)   l=$(netstat -an -f inet |grep '.*[1-9*]\.'$port' [ ]*Idle') ;;
	HP-UX)   l=$(netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' .* \*\.\* ') ;;
	OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' [ ]*\*\.\*') ;;
 	*)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' ') ;;
	esac
	[ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	  \( \( $logic -eq 0 \) -a -z "$l" \) ] && return 0
	sleep 1
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    return 1
}

# wait until a tcp6 listen port is ready
waittcp6port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case $(uname) in
	Linux)   l=$(netstat -an |grep '^tcp .* [0-9a-f:]*:'$port' .* LISTEN') ;;
	AIX)	 l=$(netstat -an |grep '^tcp[6 ]       0      0 .*[*0-9]\.'$port' .* LISTEN$') ;;
	#SunOS)   l=$(netstat -an -f inet6 |grep '.*[1-9*]\.'$port' .*\*                0 .* LISTEN') ;;
	#OSF1)    l=$(/usr/sbin/netstat -an |grep '^tcp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\* [ ]*LISTEN') /*?*/;;
 	*)       l=$(netstat -an |grep -i 'tcp6 .*:'$port' .* listen') ;;
	esac
	[ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	  \( \( $logic -eq 0 \) -a -z "$l" \) ] && return 0
	sleep 1
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    return 1
}

# wait until a UDP6 port is ready
waitudp6port () {
    local port="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    local l
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	case $(uname) in
	Linux)   l=$(netstat -an |grep '^udp .* .*[0-9*:]:'$port' [ ]*:::\*') ;;
	#FreeBSD) l=$(netstat -an |grep '^udp*[6] .*[0-9*]\.'$port' .* \*\.\*') ;;
	#Darwin)  l=$(netstat -an |grep '^udp6.* .*[0-9*]\.'$port' .* \*\.\* ') ;;
	AIX)	 l=$(netstat -an |grep '^udp[6 ]       0      0 .*[*0-9]\.'$port' .* \*\.\*[ ]*$') ;;
	#SunOS)   l=$(netstat -an -f inet6 |grep '.*[1-9*]\.'$port' [ ]*Idle') ;;
	#HP-UX)   l=$(netstat -an |grep '^udp        0      0  .*[0-9*]\.'$port' ') ;;
	#OSF1)    l=$(/usr/sbin/netstat -an |grep '^udp6       0      0  .*[0-9*]\.'$port' [ ]*\*\.\*') ;;
 	*)       l=$(netstat -an |grep -i 'udp .*[0-9*][:.]'$port' ') ;;
	esac
	[ \( \( $logic -ne 0 \) -a -n "$l" \) -o \
	  \( \( $logic -eq 0 \) -a -z "$l" \) ] && return 0
	sleep 1
	timeout=$((timeout-1))
    done

    $ECHO "!port $port timed out! \c" >&2
    return 1
}

# wait until a filesystem entry exists
waitfile () {
    local file="$1"
    local logic="$2"	# 0..wait until free; 1..wait until listening
    local timeout="$3"
    [ "$logic" ] || logic=1
    [ "$timeout" ] || timeout=5
    while [ $timeout -gt 0 ]; do
	[ \( \( $logic -ne 0 \) -a -e "$file" \) -o \
	  \( \( $logic -eq 0 \) -a ! -e "$file" \) ] && return 0
	sleep 1
	timeout=$((timeout-1))
    done

    echo "file $file timed out" >&2
    return 1
}

# generate a test certificate and key
gentestcert () {
    local name="$1"
    if [ -f $name.key -a -f $name.crt -a -f $name.pem ]; then return; fi
    openssl genrsa $OPENSSL_RAND -out $name.key 768 >/dev/null 2>&1
    openssl req -new -config testcert.conf -key $name.key -x509 -out $name.crt >/dev/null 2>&1
    cat $name.key $name.crt >$name.pem
}

TESTS="%$(echo "$TESTS" |tr ' ' '%')%"

NAME=UNISTDIO
case "$TESTS " in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: unidirectional throughput from stdin to stdout"
testecho "$N" "$TEST" "stdin" "stdout" "$opts -u"
esac
N=$((N+1))


NAME=UNPIPESTDIO
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: stdio with simple echo via internal pipe"
testecho "$N" "$TEST" "stdio" "pipe" "$opts"
esac
N=$((N+1))


NAME=UNPIPESHORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: short form of stdio ('-') with simple echo via internal pipe"
testecho "$N" "$TEST" "-" "pipe" "$opts"
esac
N=$((N+1))


NAME=DUALSTDIO
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: splitted form of stdio ('stdin!!stdout') with simple echo via internal pipe"
testecho "$N" "$TEST" "stdin!!stdout" "pipe" "$opts"
esac
N=$((N+1))


NAME=DUALSHORTSTDIO
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: short splitted form of stdio ('-!!-') with simple echo via internal pipe"
testecho "$N" "$TEST" "-!!-" "pipe" "$opts"
esac
N=$((N+1))


NAME=DUALFDS
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: file descriptors with simple echo via internal pipe"
testecho "$N" "$TEST" "0!!1" "pipe" "$opts"
esac
N=$((N+1))


NAME=NAMEDPIPE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via named pipe"
# with MacOS, this test hangs if nonblock is not used. Is an OS bug.
tp="$td/pipe$N"
# note: the nonblock is required by MacOS 10.1(?), otherwise it hangs (OS bug?)
testecho "$N" "$TEST" "" "pipe:$tp,nonblock" "$opts"
esac
N=$((N+1))


NAME=DUALPIPE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via named pipe, specified twice"
tp="$td/pipe$N"
testecho "$N" "$TEST" "" "pipe:$tp,nonblock!!pipe:$tp" "$opts"
esac
N=$((N+1))


NAME=FILE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via file"
tf="$td/file$N"
testecho "$N" "$TEST" "" "$tf,ignoreeof!!$tf" "$opts"
esac
N=$((N+1))


NAME=EXECSOCKET
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via exec of cat with socketpair"
testecho "$N" "$TEST" "" "exec:$CAT" "$opts"
esac
N=$((N+1))


NAME=SYSTEMSOCKET
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via system() of cat with socketpair"
testecho "$N" "$TEST" "" "system:$CAT" "$opts"
esac
N=$((N+1))


NAME=EXECPIPES
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via exec of cat with pipes"
testecho "$N" "$TEST" "" "exec:$CAT,pipes" "$opts"
esac
N=$((N+1))


NAME=SYSTEMPIPES
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via system() of cat with pipes"
testecho "$N" "$TEST" "" "system:$CAT,pipes" "$opts"
esac
N=$((N+1))


NAME=EXECPTY
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via exec of cat with pseudo terminal"
if ! testaddrs pty >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
else
testecho "$N" "$TEST" "" "exec:$CAT,pty,$PTYOPTS" "$opts"
fi
esac
N=$((N+1))


NAME=SYSTEMPTY
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via system() of cat with pseudo terminal"
if ! testaddrs pty >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
else
testecho "$N" "$TEST" "" "system:$CAT,pty,$PTYOPTS" "$opts"
fi
esac
N=$((N+1))


NAME=SYSTEMPIPESFDS
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via system() of cat with pipes, non stdio"
testecho "$N" "$TEST" "" "system:$CAT>&9 <&8,pipes,fdin=8,fdout=9" "$opts"
esac
N=$((N+1))


NAME=DUALSYSTEMFDS
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: echo via dual system() of cat"
testecho "$N" "$TEST" "system:$CAT>&6,fdout=6!!system:$CAT<&7,fdin=7" "" "$opts"
esac
N=$((N+1))


NAME=EXECSOCKETFLUSH
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: call to od via exec with socketpair"
testod "$N" "$TEST" "" "exec:$OD_C" "$opts"
esac
N=$((N+1))


NAME=SYSTEMSOCKETFLUSH
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: call to od via system() with socketpair"
testod "$N" "$TEST" "" "system:$OD_C" "$opts"
esac
N=$((N+1))


NAME=EXECPIPESFLUSH
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: call to od via exec with pipes"
testod "$N" "$TEST" "" "exec:$OD_C,pipes" "$opts"
esac
N=$((N+1))


## LATER:
#NAME=SYSTEMPIPESFLUSH
#case "$TESTS" in
#*%$NAME%*|*%FUNCTIONS%*)
#TEST="$NAME: call to od via system() with pipes"
#testod "$N" "$TEST" "" "system:$OD_C,pipes" "$opts"
#esac
#N=$((N+1))


## LATER:
#NAME=EXECPTYFLUSH
#case "$TESTS" in
#*%$NAME%*|*%FUNCTIONS%*)
#TEST="$NAME: call to od via exec with pseudo terminal"
#if ! testaddrs pty >/dev/null; then
#    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
#else
#testod "$N" "$TEST" "" "exec:$OD_C,pty,$PTYOPTS" "$opts"
#fi
#esac
#N=$((N+1))


## LATER:
#NAME=SYSTEMPTYFLUSH
#case "$TESTS" in
#*%$NAME%*|*%FUNCTIONS%*)
#TEST="$NAME: call to od via system() with pseudo terminal"
#if ! testaddrs pty >/dev/null; then
#    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
#else
#testod "$N" "$TEST" "" "system:$OD_C,pty,$PTYOPTS" "$opts"
#fi
#esac
#N=$((N+1))


## LATER:
#NAME=SYSTEMPIPESFDSFLUSH
#case "$TESTS" in
#*%$NAME%*|*%FUNCTIONS%*)
#TEST="$NAME: call to od via system() with pipes, non stdio"
#testod "$N" "$TEST" "" "system:$OD_C>&9 <&8,pipes,fdin=8,fdout=9" "$opts"
#esac
#N=$((N+1))


## LATER:
#NAME=DUALSYSTEMFDSFLUSH
#case "$TESTS" in
#*%$NAME%*|*%FUNCTIONS%*)
#TEST="$NAME: call to od via dual system()"
#testecho "$N" "$TEST" "system:$OD_C>&6,fdout=6!!system:$CAT<&7,fdin=7" "" "$opts"
#esac
#N=$((N+1))


NAME=RAWIP
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via raw IP protocol"
if [ $(id -u) -ne 0 ]; then
    printf "test %2s $TEST... ${YELLOW}must be root${NORMAL}\n" $N
else
    testecho "$N" "$TEST" "" "ip:127.0.0.1:254" "$opts"
fi
esac
N=$((N+1))


NAME=TCPSELF
case "$TESTS" in
*%LINUX%*)
TEST="$NAME: echo via self connection of TCP IP4 socket (works due to bug in Linux?)"
ts="127.0.0.1:$tsl"
testecho "$N" "$TEST" "" "tcp:127.100.0.1:$PORT,sp=$PORT,bind=127.100.0.1,reuseaddr" "$opts"
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=UDPSELF
case "$TESTS" in
*%LINUX%*)
TEST="$NAME: echo via self connection of UDP IP4 socket"
testecho "$N" "$TEST" "" "udp:127.100.0.1:$PORT,sp=$PORT,bind=127.100.0.1" "$opts"
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=UDP6SELF
case "$TESTS" in
*%LINUX%*)
TEST="$NAME: echo via self connection of UDP IP6 socket"
if ! testaddrs udp ip6 || ! runsip6 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}UDP6 not available${NORMAL}\n" $N
else
    tf="$td/file$N"
    testecho "$N" "$TEST" "" "udp6:0:0:0:0:0:0:0:1:$PORT,sp=$PORT,bind=0:0:0:0:0:0:0:1" "$opts"
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=DUALUDPSELF
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: echo via two unidirectional UDP IP4 sockets"
tf="$td/file$N"
p1=$PORT
p2=$((PORT+1))
testecho "$N" "$TEST" "" "udp:127.0.0.1:$p2,sp=$p1!!udp:127.0.0.1:$p1,sp=$p2" "$opts"
esac
PORT=$((PORT+2))
N=$((N+1))


#function testdual {
#    local
#}


NAME=UNIXSOCKET
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: echo via connection to UNIX domain socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ts="$td/socket$N"
tdiff="$td/test$N.diff"
da=$(date)
CMD1="$SOCAT $opts UNIX-LISTEN:$ts PIPE"
CMD2="$SOCAT $opts -!!- UNIX:$ts"
printf "test %2d $TEST... " $N
$CMD1 </dev/null >$tf 2>"$te" &
bg=$!	# background process id
waitfile "$ts"
echo "$da" |$CMD2 >>"$tf" 2>>"$te"
if [ $? -ne 0 ]; then
   $ECHO "$FAILED: $SOCAT:"
   echo "$CMD1 &"
   echo "$CMD2"
   cat $te
elif ! echo "$da" |diff - "$tf" >$tdiff; then
   $ECHO "$FAILED: diff:"
   cat $tdiff
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
kill $bg 2>/dev/null
esac
N=$((N+1))


NAME=TCP4
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: echo via connection to TCP V4 socket"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tsl=$PORT
ts="127.0.0.1:$tsl"
da=$(date)
CMD1="$SOCAT $opts TCP-listen:$tsl,reuseaddr PIPE"
CMD2="$SOCAT $opts stdin!!stdout TCP:$ts"
printf "test %2d $TEST... " $N
$CMD1 >"$tf" 2>"$te" &
waittcp4port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"$te"
if [ $? -ne 0 ]; then
   $ECHO "$FAILED: $SOCAT:"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "$te"
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $ECHO "$FAILED"
   cat $tdiff
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi ;;
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=TCP6
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: echo via connection to TCP V6 socket"
if ! testaddrs tcp ip6 || ! runsip6 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tsl=$PORT
ts="::1:$tsl"
da=$(date)
CMD1="$SOCAT $opts TCP6-listen:$tsl,reuseaddr PIPE"
CMD2="$SOCAT $opts stdin!!stdout TCP6:$ts"
printf "test %2d $TEST... " $N
$CMD1 >"$tf" 2>"$te" &
pid=$!	# background process id
waittcp6port $tsl 1
echo "$da" |$CMD2 >>"$tf" 2>>"$te"
if [ $? -ne 0 ]; then
   $ECHO "$FAILED: $SOCAT:"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "$te"
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $ECHO "$FAILED: diff:"
   cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=GOPENFILE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: file opening with gopen"
tf1="$td/test$N.1.stdout"
tf2="$td/test$N.2.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
echo "$da" >$tf1
CMD="$SOCAT $opts $tf1!!/dev/null /dev/null,ignoreeof!!-"
printf "test %2d $TEST... " $N
$CMD >"$tf2" 2>"$te"
if [ $? -ne 0 ]; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD"
    cat "$te"
elif ! diff "$tf1" "$tf2" >"$tdiff"; then
    $ECHO "$FAILED: diff:"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
esac
N=$((N+1))


NAME=GOPENPIPE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: pipe opening with gopen for reading"
tp="$td/pipe$N"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
CMD="$SOCAT $opts $tp!!/dev/null /dev/null,ignoreeof!!$tf"
printf "test %2d $TEST... " $N
#mknod $tp p	# no mknod p on FreeBSD
mkfifo $tp
#$CMD >$tf 2>"$te" &
($CMD >$tf 2>"$te" || rm -f "$tp") &
bg=$!	# background process id
usleep $MICROS
echo "$da" >"$tp"
# Solaris needs more time:
sleep 1
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    if [ -s "$te" ]; then
	$ECHO "$FAILED: $SOCAT:"
	echo "$CMD"
	cat "$te"
    else
	$ECHO "$FAILED: diff:"
	cat "$tdiff"
    fi
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
esac
N=$((N+1))


NAME=EXECIGNOREEOF
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: exec against address with ignoreeof"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$SOCAT $opts EXEC:$TRUE /dev/null,ignoreeof"
printf "test %2d $TEST... " $N
$CMD >$tf 2>"$te"
if [ -s "$te" ]; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD"
    cat "$te"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
esac
N=$((N+1))


NAME=FAKEPTY
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: generation of pty for other processes"
if ! testaddrs pty >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
else
tt="$td/pty$N"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
CMD1="$SOCAT $opts pty,link=$tt pipe"
CMD2="$SOCAT $opts - $tt,$PTYOPTS2"
printf "test %2d $TEST... " $N
$CMD1 2>${te}1 &
pid=$!	# background process id
waitfile "$tt"
echo "$da" |$CMD2 >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD1 &"
    echo "$CMD2"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
fi
kill $pid 2>/dev/null
fi
esac
N=$((N+1))


NAME=O_TRUNC
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: option o-trunc"
ff="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
CMD="$SOCAT -u $opts - open:$ff,append,o-trunc"
printf "test %2d $TEST... " $N
rm -f $ff; $ECHO "prefix-\c" >$ff
if ! echo "$da" |$CMD >$tf 2>"$te" ||
    ! echo "$da" |diff - $ff >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
esac
N=$((N+1))


NAME=FTRUNCATE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: option ftruncate"
ff="$td/test$N.file"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
CMD="$SOCAT -u $opts - open:$ff,append,ftruncate=0"
printf "test %2d $TEST... " $N
rm -f $ff; $ECHO "prefix-\c" >$ff
if ! echo "$da" |$CMD >$tf 2>"$te" ||
    ! echo "$da" |diff - $ff >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
esac
N=$((N+1))


NAME=RIGHTTOLEFT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: unidirectional throughput from stdin to stdout, right to left"
testecho "$N" "$TEST" "stdout" "stdin" "$opts -U"
esac
N=$((N+1))


NAME=CHILDDEFAULT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: child process default properties"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$SOCAT $opts -u exec:$PROCAN -"
printf "test %2d $TEST... " $N
$CMD >$tf 2>$te
MYPID=`expr "\`grep "process id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
MYPPID=`expr "\`grep "process parent id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
MYPGID=`expr "\`grep "process group id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
MYSID=`expr "\`grep "process session id =" $tf\`" : '[^0-9]*\([0-9]*\).*'`
#echo "PID=$MYPID, PPID=$MYPPID, PGID=$MYPGID, SID=$MYSID"
if [ "$MYPID" = "$MYPPID" -o "$MYPID" = "$MYPGID" -o "$MYPID" = "$MYSID" -o \
     "$MYPPID" = "$MYPGID" -o "$MYPPID" = "$MYSID" -o "$MYPGID" = "$MYSID" ];
then
    $ECHO "$FAILED:"
    echo "$CMD"
else
    $ECHO "$OK"
fi
esac
N=$((N+1))


NAME=CHILDSETSID
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: child process with setsid"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$SOCAT $opts -u exec:$PROCAN,setsid -"
printf "test %2d $TEST... " $N
$CMD >$tf 2>$te
MYPID=`grep "process id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPPID=`grep "process parent id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPGID=`grep "process group id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYSID=`grep "process session id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
#$ECHO "\nPID=$MYPID, PPID=$MYPPID, PGID=$MYPGID, SID=$MYSID"
# PID, PGID, and  SID must be the same
if [ "$MYPID" = "$MYPPID" -o \
     "$MYPID" != "$MYPGID" -o "$MYPID" != "$MYSID" ];
then
    $ECHO "$FAILED"
    echo "$CMD"
else
    $ECHO "$OK"
fi
esac
N=$((N+1))


NAME=MAINSETSID
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: main process with setsid"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
CMD="$SOCAT $opts -U -,setsid exec:$PROCAN"
printf "test %2d $TEST... " $N
$CMD >$tf 2>$te
MYPID=`grep "process id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPPID=`grep "process parent id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYPGID=`grep "process group id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
MYSID=`grep "process session id =" $tf |(expr "\`cat\`" : '[^0-9]*\([0-9]*\).*')`
#$ECHO "\nPID=$MYPID, PPID=$MYPPID, PGID=$MYPGID, SID=$MYSID"
# PPID, PGID, and  SID must be the same
if [ "$MYPID" = "$MYPPID" -o \
     "$MYPPID" != "$MYPGID" -o "$MYPPID" != "$MYSID" ];
then
    $ECHO "$FAILED"
    echo "$CMD"
else
    $ECHO "$OK"
fi
esac
N=$((N+1))


NAME=OPENSSL
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: openssl connect"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
elif ! type openssl >/dev/null 2>&1; then
    printf "test %2d $TEST... ${YELLOW}openssl executable not available${NORMAL}\n" $N
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
CMD2="$SOCAT $opts exec:'openssl s_server -accept "$PORT" -quiet -cert testsrv.pem' pipe"
#! CMD="$SOCAT $opts - openssl:$LOCALHOST:$PORT"
CMD="$SOCAT $opts - openssl:$LOCALHOST:$PORT,verify=0,$SOCAT_EGD"
printf "test %2d $TEST... " $N
eval "$CMD2 2>${te}1 &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
fi
kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=OPENSSLLISTEN
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: openssl listen"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
else
gentestcert testsrv
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
CMD2="$SOCAT $opts OPENSSL-LISTEN:$PORT,reuseaddr,$SOCAT_EGD,cert=testsrv.crt,key=testsrv.key pipe"
CMD="$SOCAT $opts - openssl:$LOCALHOST:$PORT,verify=0,$SOCAT_EGD"
printf "test %2d $TEST... " $N
eval "$CMD2 2>${te}1 &"
pid=$!	# background process id
waittcp4port $PORT
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
fi
kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=SOCKS4CONNECT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: socks4 connect"
if ! testaddrs socks4 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}SOCKS4 not available${NORMAL}\n" $N
else
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="$(date)" da="$da$($ECHO '\r')"
# we have a normal tcp echo listening - so the socks header must appear in answer
CMD2="$SOCAT tcp-l:$PORT,reuseaddr exec:\"./socks4echo.sh\""
CMD="$SOCAT $opts - socks4:$LOCALHOST:32.98.76.54:32109,socksport=$PORT",socksuser="nobody"
printf "test %2d $TEST... " $N
eval "$CMD2 2>${te}1 &"
pid=$!	# background process id
waittcp4port $PORT 1
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
fi
kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=PROXYCONNECT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: proxy connect"
if ! testaddrs proxy >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PROXY not available${NORMAL}\n" $N
else
ts="$td/test$N.sh"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="$(date)" da="$da$($ECHO '\r')"
#CMD2="$SOCAT tcp-l:$PORT,crlf system:\"read; read; $ECHO \\\"HTTP/1.0 200 OK\n\\\"; cat\""
CMD2="$SOCAT tcp-l:$PORT,reuseaddr,crlf exec:\"bash proxyecho.sh\""
CMD="$SOCAT $opts - proxy:$LOCALHOST:127.0.0.1:1000,proxyport=$PORT"
printf "test %2d $TEST... " $N
eval "$CMD2 2>${te}1 &"
pid=$!	# background process id
waittcp4port $PORT 1
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
fi
kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=TCP4NOFORK
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: echo via connection to TCP V4 socket with nofork'ed exec"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
tsl=$PORT
ts="127.0.0.1:$tsl"
da=$(date)
CMD1="$SOCAT $opts TCP-listen:$tsl,reuseaddr exec:$CAT,nofork"
CMD2="$SOCAT $opts stdin!!stdout TCP:$ts"
printf "test %2d $TEST... " $N
$CMD1 >"$tf" 2>"$te" &
waittcp4port $tsl
#usleep $MICROS
echo "$da" |$CMD2 >>"$tf" 2>>"$te"
if [ $? -ne 0 ]; then
   $ECHO "$FAILED: $SOCAT:"
   echo "$CMD1 &"
   echo "$CMD2"
   cat "$te"
elif ! echo "$da" |diff - "$tf" >"$tdiff"; then
   $ECHO "$FAILED"
   cat $tdiff
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=EXECCATNOFORK
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via exec of cat with nofork"
testecho "$N" "$TEST" "" "exec:$CAT,nofork" "$opts"
esac
N=$((N+1))


NAME=SYSTEMCATNOFORK
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: simple echo via system() of cat with nofork"
testecho "$N" "$TEST" "" "system:$CAT,nofork" "$opts"
esac
N=$((N+1))

#==============================================================================
#TEST="$NAME: echo via 'connection' to UDP V4 socket"
#tf="$td/file$N"
#tsl=65534
#ts="127.0.0.1:$tsl"
#da=$(date)
#$SOCAT UDP-listen:$tsl PIPE &
#sleep 2
#echo "$da" |$SOCAT stdin!!stdout UDP:$ts >"$tf"
#if [ $? -eq 0 ] && echo "$da" |diff "$tf" -; then
#   $ECHO "... test $N succeeded"
#else
#   $ECHO "*** test $N $FAILED"
#fi
#N=$((N+1))
#==============================================================================
# TEST 4 - simple echo via new file
#N=4
#tf="$td/file$N"
#tp="$td/pipe$N"
#da=$(date)
#rm -f "$tf.tmp"
#echo "$da" |$SOCAT - FILE:$tf.tmp,ignoreeof >"$tf"
#if [ $? -eq 0 ] && echo "$da" |diff "$tf" -; then
#   $ECHO "... test $N succeeded"
#else
#   $ECHO "*** test $N $FAILED"
#fi

#==============================================================================

NAME=PROXY2SPACES
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: proxy connect accepts status with multiple spaces"
if ! testaddrs proxy >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PROXY not available${NORMAL}\n" $N
else
ts="$td/test$N.sh"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da="$(date)" da="$da$($ECHO '\r')"
#CMD2="$SOCAT tcp-l:$PORT,crlf system:\"read; read; $ECHO \\\"HTTP/1.0 200 OK\n\\\"; cat\""
CMD2="$SOCAT tcp-l:$PORT,reuseaddr,crlf exec:\"bash proxyecho.sh -w 2\""
CMD="$SOCAT $opts - proxy:$LOCALHOST:127.0.0.1:1000,proxyport=$PORT"
printf "test %2d $TEST... " $N
eval "$CMD2 2>${te}1 &"
pid=$!	# background process id
waittcp4port $PORT 1
echo "$da" |$CMD >$tf 2>"${te}2"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD2 &"
    echo "$CMD"
    cat "${te}1"
    cat "${te}2"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat ${te}1 ${te}2; fi
fi
kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=BUG-UNISTDIO
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: for bug with address options on both stdin/out in unidirectional mode"
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
ff="$td/file$N"
printf "test %2d $TEST... " $N
>"$ff"
$SOCAT $opts -u /dev/null -,setlk <"$ff"  2>"$te"
if [ "$?" -eq 0 ]; then
    $ECHO "$OK"
else
    $ECHO "$FAILED"
    cat "$te"
fi
esac
N=$((N+1))


NAME=SINGLEEXECOUTSOCKETPAIR
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: inheritance of stdout to single exec with socketpair"
testecho "$N" "$TEST" "-!!exec:cat" "" "$opts" 1
esac
N=$((N+1))

NAME=SINGLEEXECOUTPIPE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: inheritance of stdout to single exec with pipe"
testecho "$N" "$TEST" "-!!exec:cat,pipes" "" "$opts" 1
esac
N=$((N+1))

NAME=SINGLEEXECOUTPTY
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: inheritance of stdout to single exec with pty"
if ! testaddrs pty >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
else
testecho "$N" "$TEST" "-!!exec:cat,pty,raw" "" "$opts" 1
fi
esac
N=$((N+1))

NAME=SINGLEEXECINSOCKETPAIR
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: inheritance of stdin to single exec with socketpair"
testecho "$N" "$TEST" "exec:cat!!-" "" "$opts"
esac
N=$((N+1))

NAME=SINGLEEXECINPIPE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: inheritance of stdin to single exec with pipe"
testecho "$N" "$TEST" "exec:cat,pipes!!-" "" "$opts"
esac
N=$((N+1))

NAME=SINGLEEXECINPTY
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: inheritance of stdin to single exec with pty"
if ! testaddrs pty >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}PTY not available${NORMAL}\n" $N
else
testecho "$N" "$TEST" "exec:cat,pty,raw!!-" "" "$opts" $MISCDELAY
fi
esac
N=$((N+1))


NAME=READLINE
#set -vx
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: readline with password and sigint"
if ! feat=$(testaddrs readline pty); then
    printf "test %2d $TEST... ${YELLOW}$(echo $feat| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
else
ts="$td/test$N.sh"
to="$td/test$N.stdout"
tpi="$td/test$N.inpipe"
tpo="$td/test$N.outpipe"
te="$td/test$N.stderr"
tr="$td/test$N.ref"
tdiff="$td/test$N.diff"
da="$(date)" da="$da$($ECHO '\r')"
# the feature that we really want to test is in the readline.sh script:
CMD="$SOCAT $opts open:$tpi,nonblock,ignoreeof!!open:$tpo exec:\"./readline.sh -nh ./readline-test.sh\",pty,ctty,setsid,raw,echo=0,isig"
#echo "$CMD" >"$ts"
#chmod a+x "$ts"
printf "test %2d $TEST... " $N
rm -f "$tpi" "$tpo"
mkfifo "$tpi"
touch "$tpo"
#
# during development of this test, the following command line succeeded:
# (sleep 1; $ECHO "user\n\c"; sleep 1; $ECHO "password\c"; sleep 1; $ECHO "\n\c"; sleep 1; $ECHO "test 1\n\c"; sleep 1; $ECHO "\003\c"; sleep 1; $ECHO "test 2\n\c"; sleep 1; $ECHO "exit\n\c"; sleep 1) |./socat -d -d -d -d -lf/tmp/gerhard/debug1 -v -x - exec:'./readline.sh ./readline-test.sh',pty,ctty,setsid,raw,echo=0,isig
#
PATH=${SOCAT%socat}:$PATH eval "$CMD 2>$te &"
pid=$!	# background process id
usleep $MICROS

(
usleep $((3*MICROS))
$ECHO "user\n\c"
usleep $MICROS
$ECHO "password\c"
usleep $MICROS
$ECHO "\n\c"
usleep $MICROS
$ECHO "test 1\n\c"
usleep $MICROS
$ECHO "\003\c"
usleep $MICROS
$ECHO "test 2\n\c"
usleep $MICROS
$ECHO "exit\n\c"
usleep $MICROS
) >"$tpi"

#cat >$tr <<EOF
#readline feature test program
#Authentication required
#Username: Username: user
#Password: 
#prog> prog> test 1
#executing test 1
#prog> ./readline-test.sh got SIGINT
#test 2
#executing test 2
#prog> prog> exit
#EOF
cat >$tr <<EOF
readline feature test program
Authentication required
Username: user
Password: 
prog> test 1
executing test 1
prog> ./readline-test.sh got SIGINT
test 2
executing test 2
prog> exit
EOF

#0 if ! sed 's/.*\r//g' "$tpo" |diff -q "$tr" - >/dev/null 2>&1; then
#0 if ! sed 's/.*'"$($ECHO '\r\c')"'//g' "$tpo" |diff -q "$tr" - >/dev/null 2>&1; then
if ! tr "$($ECHO '\r \c')" "% " <$tpo |sed 's/%$//g' |sed 's/.*%//g' |diff "$tr" - >"$tdiff" 2>&1; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD"
    cat "$te"
    cat "$tdiff"
else
   $ECHO "$OK"
   if [ -n "$debug" ]; then cat $te; fi
fi
#kill $pid 2>/dev/null
fi
esac
PORT=$((PORT+1))
N=$((N+1))


NAME=GENDERCHANGER
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: TCP4 \"gender changer\""
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
# this is the server in the protected network that we want to reach
CMD1="$SOCAT -lpserver $opts tcp-l:$PORT,reuseaddr,bind=$LOCALHOST echo"
# this is the double client in the protected network
CMD2="$SOCAT -lp2client $opts tcp:$LOCALHOST:$((PORT+1)),retry=10,intervall=1 tcp:$LOCALHOST:$PORT"
# this is the double server in the outside network
CMD3="$SOCAT -lp2server $opts tcp-l:$((PORT+2)),reuseaddr,bind=$LOCALHOST tcp-l:$((PORT+1)),reuseaddr,bind=$LOCALHOST"
# this is the outside client that wants to use the protected server
CMD4="$SOCAT -lpclient $opts -t1 - tcp:$LOCALHOST:$((PORT+2))"
printf "test %2d $TEST... " $N
eval "$CMD1 2>${te}1 &"
pid1=$!
eval "$CMD2 2>${te}2 &"
pid2=$!
eval "$CMD3 2>${te}3 &"
pid3=$!
waittcp4port $PORT 1 &&
waittcp4port $((PORT+2)) 1 || $ECHO "$FAILED"
sleep 1
echo "$da" |$CMD4 >$tf 2>"${te}4"
if ! echo "$da" |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD1 &"
    echo "$CMD2 &"
    echo "$CMD3 &"
    echo "$CMD4"
    cat "${te}1" "${te}2" "${te}3" "${te}4"
else
    $ECHO "$OK"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3" "${te}4"; fi
fi
kill $pid1 $pid2 $pid3 $pid4 2>/dev/null
esac
PORT=$((PORT+3))
N=$((N+1))


#!
#PORT=10000
#!
NAME=OUTBOUNDIN
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: gender changer via SSL through HTTP proxy, oneshot"
if ! feat=$(testaddrs openssl proxy); then
    printf "test %2d $TEST... ${YELLOW}$(echo $feat |tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
else
gentestcert testsrv
gentestcert testcli
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da=$(date)
# this is the server in the protected network that we want to reach
CMD1="$SOCAT $opts -lpserver tcp-l:$PORT,reuseaddr,bind=$LOCALHOST echo"
# this is the proxy in the protected network that provides a way out
CMD2="$SOCAT $opts -lpproxy tcp-l:$((PORT+1)),reuseaddr,bind=$LOCALHOST,fork exec:./proxy.sh"
# this is our proxy connect wrapper in the protected network
CMD3="$SOCAT $opts -lpwrapper tcp-l:$((PORT+2)),reuseaddr,bind=$LOCALHOST,fork proxy:$LOCALHOST:$LOCALHOST:$((PORT+3)),proxyport=$((PORT+1)),resolve"
# this is our double client in the protected network using SSL
#CMD4="$SOCAT $opts -lp2client ssl:$LOCALHOST:$((PORT+2)),retry=10,intervall=1,cert=testcli.pem,cafile=testsrv.crt,$SOCAT_EGD tcp:$LOCALHOST:$PORT"
CMD4="$SOCAT $opts -lp2client ssl:$LOCALHOST:$((PORT+2)),cert=testcli.pem,cafile=testsrv.crt,$SOCAT_EGD tcp:$LOCALHOST:$PORT"
# this is the double server in the outside network
CMD5="$SOCAT $opts -lp2server -t1 tcp-l:$((PORT+4)),reuseaddr,bind=$LOCALHOST ssl-l:$((PORT+3)),reuseaddr,bind=$LOCALHOST,$SOCAT_EGD,cert=testsrv.pem,cafile=testcli.crt"
# this is the outside client that wants to use the protected server
CMD6="$SOCAT $opts -lpclient -t5 - tcp:$LOCALHOST:$((PORT+4))"
printf "test %2d $TEST... " $N
eval "$CMD1 2>${te}1 &"
pid1=$!
eval "$CMD2 2>${te}2 &"
pid2=$!
eval "$CMD3 2>${te}3 &"
pid3=$!
waittcp4port $PORT 1       || $ECHO "$FAILED: port $PORT" >&2 </dev/null
waittcp4port $((PORT+1)) 1 || $ECHO "$FAILED: port $((PORT+1))" >&2 </dev/null
waittcp4port $((PORT+2)) 1 || $ECHO "$FAILED: port $((PORT+2))" >&2 </dev/null
eval "$CMD5 2>${te}5 &"
pid5=$!
waittcp4port $((PORT+4)) 1 || $ECHO "$FAILED: port $((PORT+4))" >&2 </dev/null
echo "$da" |$CMD6 >$tf 2>"${te}6" &
pid6=$!
waittcp4port $((PORT+3)) 1 || $ECHO "$FAILED: port $((PORT+3))" >&2 </dev/null
eval "$CMD4 2>${te}4 &"
pid4=$!
wait $pid6
if ! (echo "$da"; sleep 2) |diff - "$tf" >"$tdiff"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2 &"
    cat "${te}2"
    echo "$CMD3 &"
    cat "${te}3"
    echo "$CMD5 &"
    cat "${te}5"
    echo "$CMD6"
    cat "${te}6"
    echo "$CMD4 &"
    cat "${te}4"
    cat "$tdiff"
else
    $ECHO "$OK"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3" "${te}4" "${te}5" "${te}6"; fi
fi
kill $pid1 $pid2 $pid3 $pid4 $pid5 2>/dev/null
fi
esac
PORT=$((PORT+5))
N=$((N+1))


#!
PORT=$((RANDOM+16184))
#!
NAME=INTRANETRIPPER
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*)
TEST="$NAME: gender changer via SSL through HTTP proxy, daemons"
if ! feat=$(testaddrs openssl proxy); then
    printf "test %2d $TEST... ${YELLOW}$(echo $feat| tr 'a-z' 'A-Z') not available${NORMAL}\n" $N
else
gentestcert testsrv
gentestcert testcli
tf="$td/test$N.stdout"
te="$td/test$N.stderr"
tdiff="$td/test$N.diff"
da1="test$N.1 $(date) $RANDOM"
da2="test$N.2 $(date) $RANDOM"
da3="test$N.3 $(date) $RANDOM"
# this is the server in the protected network that we want to reach
CMD1="$SOCAT $opts -lpserver -t1 tcp-l:$PORT,reuseaddr,bind=$LOCALHOST,fork echo"
# this is the proxy in the protected network that provides a way out
CMD2="$SOCAT $opts -lpproxy -t1 tcp-l:$((PORT+1)),reuseaddr,bind=$LOCALHOST,fork exec:./proxy.sh"
# this is our proxy connect wrapper in the protected network
CMD3="$SOCAT $opts -lpwrapper -t3 tcp-l:$((PORT+2)),reuseaddr,bind=$LOCALHOST,fork proxy:$LOCALHOST:$LOCALHOST:$((PORT+3)),proxyport=$((PORT+1)),resolve"
# this is our double client in the protected network using SSL
CMD4="$SOCAT $opts -lp2client -t3 ssl:$LOCALHOST:$((PORT+2)),retry=10,intervall=1,cert=testcli.pem,cafile=testsrv.crt,fork,$SOCAT_EGD tcp:$LOCALHOST:$PORT"
# this is the double server in the outside network
CMD5="$SOCAT $opts -lp2server -t4 tcp-l:$((PORT+4)),reuseaddr,bind=$LOCALHOST,fork ssl-l:$((PORT+3)),reuseaddr,bind=$LOCALHOST,$SOCAT_EGD,cert=testsrv.pem,cafile=testcli.crt,retry=10"
# this is the outside client that wants to use the protected server
CMD6="$SOCAT $opts -lpclient -t5 - tcp:$LOCALHOST:$((PORT+4)),retry=3"
printf "test %2d $TEST... " $N
# start the intranet infrastructure
eval "$CMD1 2>${te}1 &"
pid1=$!
eval "$CMD2 2>${te}2 &"
pid2=$!
waittcp4port $PORT 1       || $ECHO "$FAILED: port $PORT" >&2 </dev/null
waittcp4port $((PORT+1)) 1 || $ECHO "$FAILED: port $((PORT+1))" >&2 </dev/null
# initiate our internal measures
eval "$CMD3 2>${te}3 &"
pid3=$!
eval "$CMD4 2>${te}4 &"
pid4=$!
waittcp4port $((PORT+2)) 1 || $ECHO "$FAILED: port $((PORT+2))" >&2 </dev/null
# now we start the external daemon
eval "$CMD5 2>${te}5 &"
pid5=$!
waittcp4port $((PORT+4)) 1 || $ECHO "$FAILED: port $((PORT+4))" >&2 </dev/null
# and this is the outside client:
echo "$da1" |$CMD6 >${tf}_1 2>"${te}6_1" &
pid6_1=$!
echo "$da2" |$CMD6 >${tf}_2 2>"${te}6_2" &
pid6_2=$!
echo "$da3" |$CMD6 >${tf}_3 2>"${te}6_3" &
pid6_3=$!
wait $pid6_1 $pid6_2 $pid6_3
#
(echo "$da1"; sleep 2) |diff - "${tf}_1" >"${tdiff}1"
(echo "$da2"; sleep 2) |diff - "${tf}_2" >"${tdiff}2"
(echo "$da3"; sleep 2) |diff - "${tf}_3" >"${tdiff}3"
if test -s "${tdiff}1" -o -s "${tdiff}2" -o -s "${tdiff}3"; then
    $ECHO "$FAILED: $SOCAT:"
    echo "$CMD1 &"
    cat "${te}1"
    echo "$CMD2 &"
    cat "${te}2"
    echo "$CMD3 &"
    cat "${te}3"
    echo "$CMD4 &"
    cat "${te}4"
    echo "$CMD5 &"
    cat "${te}5"
    echo "$CMD6 &"
    cat "${te}6_1"
    cat "${tdiff}1"
    echo "$CMD6 &"
    cat "${te}6_2"
    cat "${tdiff}2"
    echo "$CMD6 &"
    cat "${te}6_3"
    cat "${tdiff}3"
else
    $ECHO "$OK"
    if [ -n "$debug" ]; then cat "${te}1" "${te}2" "${te}3" "${te}4" "${te}5" ${te}6*; fi
fi
kill $pid1 $pid2 $pid3 $pid4 $pid5 2>/dev/null
fi
esac
PORT=$((PORT+5))
N=$((N+1))


# let us test the security features with -s, retry, and fork
# method: first test without security feature if it works
#   then try with security feature, must fail

# test the security features of a server address
testserversec () {
    local num="$1"
    local title="$2"
    local opts="$3"
    local arg1="$4"	# the server address
    local secopt0="$5"	# option without security for server, mostly empty
    local secopt1="$6"	# the security option for server, to be tested
    local arg2="$7"	# the client address
    local ipvers="$8"	# IP version, for check of listen port
    local proto="$9"	# protocol, for check of listen port
    local port="${10}"	# start client when this port is listening
    local expect="${11}"	# expected behaviour of client: 0..empty output; -1..error
    local T="${12}";	[ -z "$T" ] && T=0
    local tf="$td/test$N.stdout"
    local te="$td/test$N.stderr"
    local tdiff1="$td/test$N.diff1"
    local tdiff2="$td/test$N.diff2"
    local da="$(date)"
    local stat result

    printf "test %2d %s... " $num "$title"
#set -vx
    # first: without security
    # start server
    $SOCAT $opts "$arg1,$secopt0" echo 2>"${te}1" &
    spid=$!
    if [ "$port" ] && ! wait${proto}${ipvers}port $port 1; then
	kill $spid 2>/dev/null
	$ECHO "$NO_RESULT (ph.1 server not working):"
	echo "$SOCAT $opts \"$arg1,$secopt0\" echo &"
	cat "${te}1"
	return
    fi
    # now use client
    (echo "$da"; sleep $T) |$SOCAT $opts - "$arg2" >"$tf" 2>"${te}2"
    stat="$?"
    kill $spid 2>/dev/null
    #killall $SOCAT 2>/dev/null
    if [ "$stat" != 0 ]; then
	$ECHO "$NO_RESULT (ph.1 function fails): $SOCAT:"
	echo "$SOCAT $opts \"$arg1,$secopt0\" echo &"
	cat "${te}1"
	echo "$SOCAT $opts - \"$arg2\""
	cat "${te}2"
	return
    elif echo "$da" |diff - "$tf" >"$tdiff1" 2>&1; then
	:	# function without security is ok, go on
    else
	$ECHO "$NO_RESULT (ph.1 function fails): diff:"
	echo "$SOCAT $opts $arg1,$secopt0 echo &"
	cat "${te}1"
	echo "$SOCAT $opts - $arg2"
	cat "${te}2"
	cat "$tdiff1"
	return
    fi

    # then: with security
    if [ "$port" ] && ! wait${proto}${ipvers}port $port 0; then
	$ECHO "$NO_RESULT (ph.1 port remains in use)"
	return
    fi

    # start server
    $SOCAT $opts "$arg1,$secopt1" echo 2>"${te}3" &
    spid=$!
    if [ "$port" ] && ! wait${proto}${ipvers}port $port 1; then
	kill $spid 2>/dev/null
	$ECHO "$NO_RESULT (ph.2 server not working)"
	return
    fi
    # now use client
    (echo "$da"; sleep $T) |$SOCAT $opts - "$arg2" >"$tf" 2>"${te}4"
    stat=$?
    kill $spid 2>/dev/null
    #killall $SOCAT 2>/dev/null
    if [ "$stat" != 0 ]; then
	result=-1;	# socat had error
    elif [ ! -s "$tf" ]; then
	result=0;	# empty output
    elif echo "$da" |diff - "$tf" >"$tdiff2" 2>&1; then
	result=1;	# output is copy of input
    else
	result=2;	# output differs from input
    fi
    if [ X$result != X$expect ]; then
	case X$result in
	X-1) $ECHO "$NO_RESULT (ph.2 client error): $SOCAT:"
	    echo "$SOCAT $opts $arg1,$secopt1 echo"
	    cat "${te}3"
	    echo "$SOCAT $opts - $arg2"
	    cat "${te}4" ;;
	X0) $ECHO "$NO_RESULT (ph.2 diff failed): diff:"
	    echo "$SOCAT $opts $arg1,$secopt1 echo"
	    cat "${te}3"
	    echo "$SOCAT $opts - $arg2"
	    cat "${te}4"
	    cat "$tdiff2" ;;
	X1) $ECHO "$FAILED: SECURITY BROKEN" ;;
	X2) $ECHO "$FAILED: diff:"
	    echo "$SOCAT $opts $arg1,$secopt1 echo"
	    cat "${te}3"
	    echo "$SOCAT $opts - $arg2"
	    cat "${te}4"
	    cat "$tdiff2" ;;
	esac
    else
	$ECHO "$OK"
	if [ -n "$debug" ]; then cat $te; fi
    fi
#set +vx
}


NAME=TCP4RANGE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of TCP4-L with RANGE option"
testserversec "$N" "$TEST" "$opts -s" "tcp4-l:$PORT,reuseaddr,fork,retry=1" "" "range=127.0.0.2/32" "tcp4:127.0.0.1:$PORT" 4 tcp $PORT 0
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=TCP4SOURCEPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of TCP4-L with SOURCEPORT option"
testserversec "$N" "$TEST" "$opts -s" "tcp4-l:$PORT,reuseaddr,fork,retry=1" "" "sp=$PORT" "tcp4:127.0.0.1:$PORT" 4 tcp $PORT 0
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=TCP4LOWPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of TCP4-L with LOWPORT option"
testserversec "$N" "$TEST" "$opts -s" "tcp4-l:$PORT,reuseaddr,fork,retry=1" "" "lowport" "tcp4:127.0.0.1:$PORT" 4 tcp $PORT 0
esac
PORT=$((PORT+1))
N=$((N+1))

#!!!NAME=TCP4WRAPPERS

NAME=TCP6SOURCEPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of TCP6-L with SOURCEPORT option"
if ! feat=$(testaddrs tcp ip6) || ! runsip6 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
else
testserversec "$N" "$TEST" "$opts -s" "tcp6-l:$PORT,reuseaddr,fork,retry=1" "" "sp=$PORT" "tcp6:::1:$PORT" 6 tcp $PORT 0
fi
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=TCP6LOWPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of TCP6-L with LOWPORT option"
if ! feat=$(testaddrs tcp ip6) || ! runsip6 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
else
testserversec "$N" "$TEST" "$opts -s" "tcp6-l:$PORT,reuseaddr,fork,retry=1" "" "lowport" "tcp6:::1:$PORT" 6 tcp $PORT 0
fi
esac
PORT=$((PORT+1))
N=$((N+1))

#!!!NAME=TCP6WRAPPERS

NAME=UDP4RANGE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of UDP4-L with RANGE option"
#testserversec "$N" "$TEST" "$opts -s" "udp4-l:$PORT,reuseaddr,fork" "" "range=127.0.0.2/32" "udp4:127.0.0.1:$PORT" 4 udp $PORT 0
testserversec "$N" "$TEST" "$opts -s" "udp4-l:$PORT,reuseaddr" "" "range=127.0.0.2/32" "udp4:127.0.0.1:$PORT" 4 udp $PORT 0
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=UDP4SOURCEPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of UDP4-L with SOURCEPORT option"
testserversec "$N" "$TEST" "$opts -s" "udp4-l:$PORT,reuseaddr" "" "sp=$PORT" "udp4:127.0.0.1:$PORT" 4 udp $PORT 0
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=UDP4LOWPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of UDP4-L with LOWPORT option"
testserversec "$N" "$TEST" "$opts -s" "udp4-l:$PORT,reuseaddr" "" "lowport" "udp4:127.0.0.1:$PORT" 4 udp $PORT 0
esac
PORT=$((PORT+1))
N=$((N+1))

#!!!NAME=UDP4WRAPPERS

NAME=UDP6SOURCEPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of UDP6-L with SOURCEPORT option"
if ! feat=$(testaddrs udp ip6) || ! runsip6 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
else
testserversec "$N" "$TEST" "$opts -s" "udp6-l:$PORT,reuseaddr" "" "sp=$PORT" "udp6:::1:$PORT" 6 udp $PORT 0
fi
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=UDP6LOWPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of UDP6-L with LOWPORT option"
if ! feat=$(testaddrs udp ip6) || ! runsip6 >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}TCP6 not available${NORMAL}\n" $N
else
testserversec "$N" "$TEST" "$opts -s" "udp6-l:$PORT,reuseaddr" "" "lowport" "udp6:::1:$PORT" 6 udp $PORT 0
fi
esac
PORT=$((PORT+1))
N=$((N+1))

#!!!NAME=UDP6WRAPPERS

NAME=OPENSSLRANGE
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of SSL-L with RANGE option"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
else
gentestcert testsrv
testserversec "$N" "$TEST" "$opts -s" "ssl-l:$PORT,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "range=127.0.0.2/32" "ssl:127.0.0.1:$PORT,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=OPENSSLSOURCEPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of SSL-L with SOURCEPORT option"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
else
gentestcert testsrv
testserversec "$N" "$TEST" "$opts -s" "ssl-l:$PORT,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "sp=$PORT" "ssl:127.0.0.1:$PORT,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=OPENSSLLOWPORT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of SSL-L with LOWPORT option"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
else
gentestcert testsrv
testserversec "$N" "$TEST" "$opts -s" "ssl-l:$PORT,reuseaddr,fork,retry=1,$SOCAT_EGD,verify=0,cert=testsrv.crt,key=testsrv.key" "" "lowport" "ssl:127.0.0.1:$PORT,cafile=testsrv.crt,$SOCAT_EGD" 4 tcp $PORT -1
fi
esac
PORT=$((PORT+1))
N=$((N+1))

#!!!NAME=OPENSSLWRAPPERS

NAME=OPENSSLCERTSERVER
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of SSL-L with client certificate"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
else
gentestcert testsrv
gentestcert testcli
testserversec "$N" "$TEST" "$opts -s" "ssl-l:$PORT,reuseaddr,fork,retry=1,$SOCAT_EGD,verify,cert=testsrv.crt,key=testsrv.key" "cafile=testcli.crt" "cafile=testsrv.crt" "ssl:127.0.0.1:$PORT,cafile=testsrv.crt,cert=testcli.pem,$SOCAT_EGD" 4 tcp $PORT -1
fi
esac
PORT=$((PORT+1))
N=$((N+1))

NAME=OPENSSLCERTCLIENT
case "$TESTS" in
*%$NAME%*|*%FUNCTIONS%*|*%SECURITY%*)
TEST="$NAME: security of SSL with server certificate"
if ! testaddrs openssl >/dev/null; then
    printf "test %2d $TEST... ${YELLOW}OPENSSL not available${NORMAL}\n" $N
else
gentestcert testsrv
gentestcert testcli
testserversec "$N" "$TEST" "$opts -s -lu -d" "ssl:localhost:$PORT,fork,retry=2,verify,cert=testcli.pem,$SOCAT_EGD" "cafile=testsrv.crt" "cafile=testcli.crt" "ssl-l:$PORT,reuseaddr,$SOCAT_EGD,cafile=testcli.crt,cert=testsrv.crt,key=testsrv.key" 4 tcp "" -1
fi
esac
PORT=$((PORT+1))
N=$((N+1))

#==============================================================================

rm -f testsrv.* testcli.*

# end

# too dangerous - run as root and having a shell problem, it might purge your
# file systems 
#rm -r "$td"

exit


TEST="$NAME: transferring from one file to another with echo"
tf1="$td/file$N.input"
tf2="$td/file$N.output"
testecho "$N" "$TEST" "" "echo" "$opts"


# MANUAL TESTS

# ZOMBIES
#   have httpd on PORT/tcp
#   nice -20 $SOCAT -d tcp-l:24080,fork tcp:$LOCALHOST:PORT
#   i=0; while [ $i -lt 100 ]; do $ECHO 'GET / HTTP/1.0\n' |$SOCAT -t -,ignoreeof tcp:$LOCALHOST:24080 >/dev/null& i=$((i+1)); done
