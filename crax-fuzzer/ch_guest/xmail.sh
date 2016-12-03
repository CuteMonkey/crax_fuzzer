#!/bin/bash

xmail_root=/home/chengte/xmail-1.21

export MAIL_ROOT=${xmail_root}/MailRoot

./symstdin `cat ${xmail_root}/ch_mail` ${xmail_root}/bin/sendmail -t
