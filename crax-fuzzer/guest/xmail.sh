#!/bin/sh
export MAIL_ROOT=/home/littlepunpun/crax-fuzzer/pool/xmail-1.21/MailRoot/
./symstdin `cat ../pool/xmail-1.21/demo` ../pool/xmail-1.21/bin/sendmail -t
