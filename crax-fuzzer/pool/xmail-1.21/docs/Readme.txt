NAME

    XMail - Internet/Intranet mail server.

    [top]

LICENSE

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation (<http://www.gnu.org>); either version 2 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
    Public License for more details.

    you should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    [top]

OVERVIEW

    XMail is an Internet and Intranet mail server featuring an SMTP server,
    POP3 server, finger server, multiple domains, no need for users to have
    a real system account, SMTP relay checking, RBL/RSS/ORBS/DUL and custom
    (IP based and address based) spam protection, SMTP authentication (PLAIN
    LOGIN CRAM-MD5 POP3-before-SMTP and custom), a POP3 account synchronizer
    with external POP3 accounts, account aliases, domain aliases, custom
    mail processing, direct mail files delivery, custom mail filters,
    mailing lists, remote administration, custom mail exchangers, logging,
    and multi-platform code.

    XMail sources compile under GNU/Linux, FreeBSD, OpenBSD, NetBSD, OSX,
    Solaris and NT/2K.

    This server born due to the need of having a free and stable Mail Server
    to be used inside my old company, which used a Windows Network. I don't
    like to reinvent the wheel but the need of some special features drive
    me to start a new project. Probably if I could use a Linux server on my
    net, I would be able to satisfy my needs without write code, but this is
    not my case. It should be also portable to other OSs, like Linux and
    other Unixes.

    Another reason that drove me to write XMail is the presence of the same
    steps in setting up a typical mail server, ie:

     sendmail + qpopper + fetchmail

    if one needs SMTP, POP3 and external synchronization, or:

     sendmail + qpopper

    for only SMTP and POP3 (I've quoted sendmail, qpopper and fetchmail, but
    there are many other packages you can use to reach these needs). With
    XMail you get an all-in-one package with a central administration that
    can simplify the above common steps.

    The first code of XMail Server is started on Windows NT and Linux, and
    now, the FreeBSD and Solaris version ready. The compilers supported are
    gcc for Linux, FreeBSD, OpenBSD and Solaris and M$ Visual C++ for NT/2K.

    [top]

VERSION

  current

    1.21

  release type

    Gnu Public License <http://www.gnu.org>

  release date

    Jan 9, 2005

  project by

    Davide Libenzi <davidel@xmailserver.org> <http://www.xmailserver.org/>

  credits

    Michael Hartle <mhartle@hartle-klug.com>

    Shawn Anderson <sanderson@eye-catcher.com>

    Dick van der Kaaden <dick@netrex.nl>

    Beau E, Cox <beau@beaucox.com>

  warning

     ************************************************************
     *                     <<WARNING>>                          *
     *  If you're upgrading an existing version of XMail it's   *
     *  strongly suggested that you read all the ChangeLog.txt  *
     *  notes that range from existing version to the new one.  *
     ************************************************************

    [top]

DOCUMENTATION CONVENTIONS

    This document contains various examples of entries you must make to the
    XMail configuration tables. These examples are written in a

     mono-spaced font like this.

    The prototype statement is shown with explicit '[TAB]' and '[NEWLINE]'
    characters:

     "aliasdomain"[TAB]"realdomain"[NEWLINE]

    while examples omit these characters:

     "simpson.org"   "simpson.com"
     "*.homer.net"   "homer.net"

    'YOU MUST ALWAYS ENTER THE DATA EXACTLY AS SHOWN IN THE PROTOTYPE.'

    When a protype or example statement is too long to easily be shown on
    the screen or printed, the line is split into multiple lines by showing
    '=>' at the end of continued lines and indenting the continuation
    line(s):

     "domain"[TAB]"account"[TAB]"enc-passwd"[TAB]"account-id"[TAB]"account-dir"[TAB]=>
       "account-type"[NEWLINE]

    'DO NOT ENTER THE => CHARACTERS. ENTER THE ENTIRE ENTRY AS ONE LINE.'

    [top]

FEATURES

    *   ESMTP server

    *   POP3 server

    *   Finger server

    *   Multiple domains

    *   Users don't need a real system account

    *   SMTP relay checking

    *   SMTP RBL maps check (rbl.maps.vix.com)

    *   SMTP RSS maps check (relays.mail-abuse.org)

    *   SMTP ORBS relay check (relays.orbs.org)

    *   SMTP DUL map check (dialups.mail-abuse.org)

    *   SMTP protection over spammers (IP based and address based)

    *   SMTP authentication (PLAIN LOGIN CRAM-MD5 POP3/SMTP and custom)

    *   SMTP ETRN command support

    *   POP3 account synchronizer with external POP3 accounts

    *   Account aliasing

    *   Domain aliasing

    *   Mailing lists

    *   Custom mail processing

    *   Locally generated mail files delivery

    *   Remote administration

    *   Custom mail exchangers

    *   Logging

    *   Multi platform

    *   Domain message filters

    *   Custom (external) POP3 authentication

    [top]

PORTING STATUS

    Right now the Linux and NT ports are stable, while the Solaris, FreeBSD
    and OpenBSD ones have not been tested as well as the previous OSs.

    [top]

REQUIREMENTS

    *   Any version of Linux that has glibc.

    *   Windows NT with ws2_32.dll correctly installed.

    *   A working DNS and gateway to the Internet (if you plan to use it).

    *   To build from source for Linux you need any version of gcc and glibc
        installed.

    *   To build from source for Windows you need MS Visual C++ (project
        included).

    *   -or- any other working compiler that provides support for the Win32
        SDK.

    [top]

OBTAINING THE SOURCE

    Always get the latest sources at the XMail home page
    <http://www.xmailserver.org/> because otherwise you may be using an old
    version.

    Use the correct distribution for your system and don't mix Unix files
    with Windows ones because this is one of the most common cause of XMail
    bad behavior.

    When you unzip (or untar) the package you've to check that the MailRoot
    directory contained inside the package itself is complete (look at the
    directory tree listed below) because some unzippers don't restore empty
    directories.

    [top]

BUILD

    For Windows, Visual C++ project files are supplied, while for *nixes,
    the following options are given:

    [Linux]

      # make -f Makefile.lnx

    [FreeBSD]

      # setenv OSTYPE FreeBSD
      # gmake -f Makefile.bsd

    or (depending on the shell):

      # OSTYPE=FreeBSD gmake -f Makefile.bsd

    [OpenBSD]

      # setenv OSTYPE OpenBSD
      # gmake -f Makefile.bsd

    or (depending on the shell):

      # OSTYPE=OpenBSD gmake -f Makefile.bsd

    [NetBSD]

      # setenv OSTYPE NetBSD
      # gmake -f Makefile.bsd

    or (depending on the shell):

      # OSTYPE=NetBSD gmake -f Makefile.bsd

    [OSX]

      # OSTYPE=Darwin make -f Makefile.bsd

    or (depending on the shell):

      # setenv OSTYPE Darwin
      # make -f Makefile.bsd

    [Solaris]

      # make -f Makefile.sso

    Under Linux an init.d startup script is supplied (xmail) to allow you to
    run XMail as a standard rc? daemon. You must put it into /etc/init.d (it
    depends on which distro you're using) directory and then create K??xmail
    - S??xmail links into the proper directories.

    Under Windows NT/2000/XP the XMail's executable is a Win32 service by
    default and if you want to have it built like a standard executable
    you've to comment the statement:

     "#define SERVICE" in MainWin.cpp

    When it's built as a service (default) you can run:

     XMail --install

    to install XMail as a manual startup service or:

     XMail --install-auto

    to install XMail as an automatic startup service.

    If you run '--install' and you want XMail to run at NT boot, you must go
    in ControlPanel->Services and edit the startup options of XMail. Once
    you have the service version of XMail you can run it in a 'normal' way
    by executing:

     XMail --debug [options]

    [top]

CONFIGURATION

  Linux/Solaris/FreeBSD/OpenBSD

    1.  Build XMail.

    2.  Log as root.

    3.  Copy the supplied MailRoot directory where you want it to reside
        (normally /var).

    4.  Do a # chmod 700 /var/MailRoot to setup MailRoot directory access
        rights.

    5.  Strip XMail executables if you want to reduce their sizes (strip
        filename).

    6.  Copy XMail executables to /var/MailRoot/bin.

    7.  If you have 'inetd' installed, comment out the lines of
        '/etc/inetd.conf' that involve SMTP, POP3, and Finger. Restart
        'inetd' (kill -HUP ...).

    8.  Since XMail use syslog to log messages, enable syslogd if it's not
        running.

    9.  Setup the 'SERVER.TAB' configuration option (after reading the rest
        of this document well).

    10. Add your users and domains (after reading the rest of this document
        well).

    11. Change or comment out (#) the example account in 'ctrlaccounts.tab'
        by using non-trivial username and password.

    12. Copy the xmail startup script to your init.d directory (it's
        position depends on your distro). If you've setup XMail to work in a
        subdirectory other than '/var/MailRoot' you must edit xmail startup
        script to customize its boot.

    13. Use the 'sysv_inst.sh' shell script (from root user) to create SysV
        boot script - unless your distro has other tools to do this.

    14. To start XMail without reboot you can run (from root):
        /etc/rc.d/init.d/xmail start otherwise reboot your machine.

    15. Setup the file 'smtprelay.tab' if you want to extend mail relaying
        to IPs out of the internet's private IP blocks (or you want to deny
        even those - that comes enabled by default with XMail).

    [configuration] [top]

  NT/Win2K

    1.  Build XMail.

    2.  Copy the supplied MailRoot directory where you want it to reside
        (normally 'C:\MailRoot').

    3.  Setup the MailRoot directory (and subdirectories and file)
        permissions to allow access only to System and Administrators. Doing
        this you can run XMail as a console startup only if you're
        Administrator (service startup as System).

    4.  Copy XMail executables to 'C:\MailRoot\bin'.

    5.  With 'regedit', create 'GNU' key inside
        'HKEY_LOCAL_MACHINE\SOFTWARE\' and then 'XMail' key inside
        'HKEY_LOCAL_MACHINE\SOFTWARE\GNU'.

    6.  Create a new string value named 'MAIL_ROOT' inside
        'HKEY_LOCAL_MACHINE\SOFTWARE\GNU\XMail\' with value 'C:\MailRoot'.

    7.  Optionally create a new string value named 'MAIL_CMD_LINE' inside
        'HKEY_LOCAL_MACHINE\SOFTWARE\GNU\XMail\' to store your command line
        options (read well the rest of this document).

    8.  Open an NT console (command prompt).

    9.  Go inside 'C:\MailRoot\bin' and run: XMail --install for a manual
        startup, or: XMail --install-auto for an automatic startup.

    10. If you have other services that give the same functionality of
        XMail, that is SMTP, POP3, or Finger servers, you must stop these
        services.

    11. Setup the 'SERVER.TAB' configuration option after reading the rest
        of this document well.

    12. Add your users and domains (after reading to the rest of this
        document well).

    13. Setup file permissions of the 'C:\MailRoot' directory to grant
        access only to 'SYSTEM' and 'Domain Admins'.

    14. Change or comment out (#) the example account in ctrlaccounts.tab by
        using a non-trivial username and password.

    15. To start XMail without reboot you can go to: ControlPanel ->
        Services -> XMail server and start the service, otherwise reboot
        your machine.

    16. Setup the file 'smtprelay.tab' if you want to extend mail relaying
        to IPs out of the internet's private IP blocks (or you want to deny
        even those - that comes enabled by default with XMail).

    [configuration] [top]

  Environment variables

    [MAIL_ROOT]
        If you want to start XMail as a simple test you must setup an
        environment variable MAIL_ROOT that point to the XMail Server root
        directory.

        Linux/etc.:

         export MAIL_ROOT=/var/XMailRoot

        Windows:

         set MAIL_ROOT=C:\MailRoot

    [MAIL_CMD_LINE]
        Let the user to specify extra command line parameters (they will be
        appended to the ones specified in the command line).

    [XMAIL_PID_DIR]
        Let the user to specify the PID directory (Unix only ports). The
        specified directory must NOT have the final slash (/) appended to
        the path.

    [configuration] [top]

  MailRoot structure

    Mail root directory contain these files:

      aliases.tab <file>
      aliasdomain.tab <file>
      domains.tab <file>
      dnsroots    <file>
      extaliases.tab  <file>
      mailusers.tab   <file>
      message.id  <file>
      pop3links.tab   <file>
      server.tab  <file>
      smtpgw.tab  <file>
      smtpfwd.tab <file>
      smtprelay.tab   <file>
      smtpauth.tab    <file>
      smtpextauth.tab <file>
      userdef.tab <file>
      ctrlaccounts.tab    <file>
      spammers.tab    <file>
      spam-address.tab    <file>
      pop3.ipmap.tab  <file>
      smtp.ipmap.tab  <file>
      ctrl.ipmap.tab  <file>
      finger.ipmap.tab    <file>
      filters.in.tab  <file>
      filters.out.tab <file>
      filters.pre-data.tab <file>
      filters.post-data.tab <file>
      smtp.ipprop.tab <file>

    and these directories:

      bin     <dir>
      cmdaliases  <dir>
      tabindex    <dir>
      dnscache    <dir>
        mx  <dir>
        ns  <dir>
      custdomains <dir>
      filters     <dir>
      logs        <dir>
      pop3locks   <dir>
      pop3linklocks   <dir>
      pop3links   <dir>
      spool       <dir>
        local       <dir>
        temp        <dir>
        0           <dir>
          0           <dir>
            mess        <dir>
            rsnd        <dir>
            info        <dir>
            temp        <dir>
            slog        <dir>
            lock        <dir>
            cust        <dir>
            froz        <dir>
          ...
        ...
      userauth    <dir>
        pop3    <dir>
        smtp    <dir>
      domains     <dir>

    and for each domain DOMAIN handled a directory (inside domains):

        DOMAIN      <dir>
        userdef.tab <file>
        mailproc.tab    <file>  [ optional ]

    inside of which reside, for each account ACCOUNT:

          ACCOUNT         <dir>
            user.tab    <file>
            mlusers.tab <file>  [ mailing list case ]
            mailproc.tab    <file>  [ optional ]
            pop3.ipmap.tab  <file>  [ optional ]

    and

            mailbox     <dir>

    for mailbox structure, while:

            Maildir     <dir>
              tmp <dir>
              new <dir>
              cur <dir>

    for Maildir structure.

    [configuration] [top]

  Configuration tables

    TAB ('something.tab') files are text files (in the sense meant by the OS
    in use: <CR><LF> for NT and <CR> for Linux) with this format:

     "value1"[TAB]"value2"[TAB]...[NEWLINE]

    The following sections explain each file's structure and use.

    "ALIASES.TAB"
    "ALIASDOMAIN.TAB"
    "DOMAINS.TAB"
    "DNSROOTS"
    "EXTALIASES.TAB"
    "MAILUSERS.TAB"
    "MESSAGE.ID"
    "POP3LINKS.TAB"
    "SERVER.TAB"
    "SMTPGW.TAB"
    "SMTPFWD.TAB"
    "SMTPRELAY.TAB"
    "SMTPAUTH.TAB"
    "SMTPEXTAUTH.TAB"
    "USERDEF.TAB"
    "CTRLACCOUNTS.TAB"
    "SPAMMERS.TAB"
    "SPAM-ADDRESS.TAB"
    "POP3.IPMAP.TAB"
    "SMTP.IPMAP.TAB"
    "CTRL.IPMAP.TAB"
    "FINGER.IPMAP.TAB"
    "USER.TAB"
    "MLUSERS.TAB"
    "MAILPROC.TAB"
    "SMTP.IPPROP.TAB"
    "FILTERS.IN.TAB"
    "FILTERS.OUT.TAB"
    "FILTERS.PRE-DATA.TAB"
    "FILTERS.POST-DATA.TAB"

    [configuration] [top]

   ALIASES.TAB

     "domain"[TAB]"alias"[TAB]"realaccount"[NEWLINE]

    Example:

     "home.bogus"    "davidel"   "dlibenzi"

    define 'davidel' as alias for 'dlibenzi' in 'home.bogus' domain.

     "home.bogus"    "foo*bog"   "homer@internal-domain.org"

    define an alias for all users whose name starts with 'foo' and ends with
    'bog' that point to the locally handled account
    'homer@internal-domain.org'.

     "home.bogus"    "??trips"   "travels"

    define an alias for all users whose names start with any two chars and
    end with 'trips'. You can have widcard even in the domain field, as:

     "*" "postmaster"    "postmaster@domain.net"

    You 'CANNOT' edit this file while XMail is running because it is an
    indexed file.

    [table index] [configuration] [top]

   ALIASDOMAIN.TAB

     "aliasdomain"[TAB]"realdomain"[NEWLINE]

    where 'aliasdomain' can use wildcards:

     "simpson.org"   "simpson.com"
     "*.homer.net"   "homer.net"

    The first line defines 'simpson.org' as an alias of 'simpson.com' while
    the second remaps all subdomains of 'homer.net' to 'homer.net'.

    You 'CANNOT' edit this file while XMail is running because it is an
    indexed file.

    [table index] [configuration] [top]

   DOMAINS.TAB

     "domain"[NEWLINE]

    defines domains handled by the server.

    [table index] [configuration] [top]

   DNSROOTS

     host

    This is a file that lists a root name server in each line (this is not a
    TAB file). This can be created from a query via nslookup for type=ns and
    host = '.'.

    [table index] [configuration] [top]

   EXTALIASES.TAB

     "external-domain"[TAB]"external-account"[TAB]"local-domain"[TAB]"local-user"[NEWLINE]

    Example:

     "xmailserver.org"   "dlibenzi"  "home.bogus"    "dlibenzi"

    This file is used in configurations in which the server does not run
    directly on Internet (like my case) but acts as internal mail exchanger
    and external mail gateway. This file defines 'Return-Path: <...>'
    mapping for internal mail delivery. If you are using a Mail client like
    Outlook, Eudora, KMail ... you have to configure your email address with
    the external account say 'dlibenzi@xmailserver.org'. When you post an
    internal message to 'foo@home.bogus' the mail client puts your external
    email address ('dlibenzi@xmailserver.org') in the 'MAIL FROM: <...>'
    SMTP request. Now if the user 'foo' replies to this message, it replies
    to 'dlibenzi@xmailserver.org', and then is sent to the external mail
    server. With the entry above in 'EXTALIASES.TAB' file the 'Return-Path:
    <...>' field is filled with 'dlibenzi@home.bogus' that leads to an
    internal mail reply.

    You 'CANNOT' edit this file while XMail is running because it is an
    indexed file.

    [table index] [configuration] [top]

   MAILUSERS.TAB

     "domain"[TAB]"account"[TAB]"enc-passwd"[TAB]"account-id"[TAB]"account-dir"[TAB]=>
       "account-type"[NEWLINE]

    (remember, enter as one line.) Example:

     "home.bogus"    "dlibenzi"  "XYZ..."    1   "dlibenzi"  "U"

    defines an account 'dlibenzi' in domain 'home.bogus' with the encrypted
    password 'XYZ...', user id '1' and mail directory 'dlibenzi' inside
    '$MAIL_ROOT/domains/home.bogus'. To allow multiple domain handling the
    POP3 client must use the entire email address for the POP3 user account;
    for example. if a user has email user@domain it must supply:

     user@domain

    as POP3 account login.

    The directory 'account-dir' 'must' case match with the field
    'account-dir' of this file. Note that user id 'must' be unique for all
    users (duplicate user ids are not allowed). The user id 0 is reserved by
    XMail and cannot be used.

    The last field 'U' is the account type:

     "U" = User account
     "M" = Mailing list account

    The encrypted password is generated by 'XMCrypt' whose source is
    'XMCrypt.cpp'. Even if external authentication is used (see "External
    Authentication") this file 'must' contain an entry for each user handled
    by XMail.

    You 'CANNOT' edit this file while XMail is running because it is an
    indexed file.

    [table index] [configuration] [top]

   MESSAGE.ID

    A file storing a sequential message number. Set it at 1 when you install
    the server and leave it be handled by the software.

    [table index] [configuration] [top]

   POP3LINKS.TAB

     "local-domain"[TAB]"local-account"[TAB]"external-domain"[TAB]=>
       "external-account"[TAB]"external-crypted-password"[TAB]"authtype"[NEWLINE]

    (remember, enter as one line) where:

    'authtype' = authentication method ('CLR' = USER/PASS auth, 'APOP' =
    APOP auth).

    Examples;

     "home.bogus"    "dlibenzi"  "xmailserver.org"   "dlibenzi" "XYZ..."=>
       "APOP"

    This entry is used to synchronize the external account
    'dlibenzi@xmailserver.org' with encrypted password 'XYZ...' with the
    local account 'dlibenzi@home.bogus' using 'APOP' authentication. It
    connect with the 'xmailserver.org' POP3 server and download all messages
    for 'dlibenzi@xmailserver.org' into the local account
    'dlibenzi@home.bogus'. The remote server must support 'APOP'
    authentication to specify 'APOP' as authtype. Even if using APOP
    authentication is more secure because clear usernames and password does
    not travel on the network, if you're not sure about it, specify 'CLR' as
    authtype. For non local POP3 sync you've to specify a line like this one
    (@ as the first domain char):

     "@home.bogus.com"   "dlibenzi"  "xmailserver.org:110"   "dlibenzi" "XYZ..."=>
       "CLR"

    This entry is used to synchronize the external account
    'dlibenzi@xmailserver.org' with encrypted password 'XYZ...' with the
    account 'dlibenzi@home.bogus.com' using 'CLR' authentication. The
    message is pushed into the spool having as destination
    dlibenzi@home.bogus.com , so you've to have some kind of processing for
    that user or domain in your XMail configuration (for example custom
    domain processing). you can also have the option to setup a line like
    this one:

     "?home.bogus.com,felins.net,pets.org"   "dlibenzi"  "xmailserver.org"=>
       "dlibenzi"  "XYZ..."    "CLR"

    and messages are dropped inside the spool by following these rules:

    1.  XMail parse the message headers by searching for To:, Cc: and Bcc:
        addresses.

    2.  Each address's domain is compared with the list of valid domains
        (felins.net, pets.org).

    3.  For each valid address the username part is taken and joined with
        the '@' and the masquerade domain name (the name following '?').

    4.  The message is spooled with the above built destination address.

    Obviously the masquerade domain ('home.bogus.com') MUST be handled by
    the server or MUST be a valid external mail domain. So if a message
    having as To: address graycat@felins.net is fetched by the previous line
    a message is pushed into the spool with address graycat@home.bogus.com.
    Particular attention is to be taken about at not creating mail loops.
    Another option is:

     "&.local,felins.net,pets.org"   "dlibenzi"  "xmailserver.org" "dlibenzi"=>
       "XYZ..."    "CLR"

    where a fetched message whose To: address is graycat@felins.net is
    replaced with graycat@felins.net.local. You can avoid the matching
    domain list after the masquerading domain but, in that case, you may
    have bad destination addresses inside the spool. The list MUST be comma
    separated WITHOUT spaces. XMail starts PSYNC session with a delay that
    you can specify with the -Yi nsec command line parameter (default 120).
    XMail also checks for the presence (inside MAIL_ROOT) of a file named
    '.psync-trigger' and, when this file is found, a PSYNC session starts
    and that file is removed.

    [table index] [configuration] [top]

   SERVER.TAB

     "varname"[TAB]"varvalue"[NEWLINE]

    This file contains server configuration variables. See "SERVER.TAB
    variables" below for details.

    [table index] [configuration] [top]

   SMTPGW.TAB

     "domain"[TAB]"smtp-gateway"[NEWLINE]

    Examples:

     "foo.example.com"   "@xmailserver.org"

    sends all mail for 'foo.example.com' through the 'xmailserver.org' SMTP
    server, while:

     "*.dummy.net"   "@relay.xmailserver.org"

    sends all mail for "*'*.dummy.net' through 'relay.xmailserver.org'.

    The 'smtp-gateway' can be a complex routing also, for example:

     "*.dummy.net"   "@relay.xmailserver.org,@mail.nowhere.org"

    sends all mail for "*'*.dummy.net' through
    '@relay.xmailserver.org,@mail.nowhere.org', in this way:
    relay.xmailserver.org --> mail.nowhere.org --> @DESTINATION.

    [table index] [configuration] [top]

   SMTPFWD.TAB

     "domain"[TAB]"smtp-mx-list"[NEWLINE]

    Examples:

     "foo.example.com"   "mail.xmailserver.org:7001,192.168.1.1:6123,mx.xmailserver.org"

    sends all mail for 'foo.example.com' using the provided list of mail
    exchangers, while:

     "*.dummy.net"   "mail.xmailserver.org,192.168.1.1,mx.xmailserver.org:6423"

    sends all mail for "*'*.dummy.net' through the provided list of mail
    exchangers. If the port (:nn) is not specified the default SMTP port
    (25) is assumed. you can also enable XMail to random-select the order of
    the gateway list by specifying:

     "*.dummy.net"   "#mail.xmailserver.org,192.168.1.1,mx.xmailserver.org:6423"

    using the character '#' as the first char of the gateway list.

    [table index] [configuration] [top]

   SMTPRELAY.TAB

     "ipaddr"[TAB]"netmask"[NEWLINE]

    Example:

     "212.131.173.0"   "255.255.255.0"

    allows all hosts of the class 'C' network '212.131.173.XXX' to use the
    server as relay.

    [table index] [configuration] [top]

   SMTPAUTH.TAB

     "username"[TAB]"password"[TAB]"permissions"[NEWLINE]

    is used to permit SMTP clients authentication with protocols PLAIN,
    LOGIN, CRAM-MD5 and custom. With custom authentication a file containing
    all secrets (username + ':' + password) is passed as parameter to the
    custom authentication program which tests all secrets to find the one
    matching (if exist). For this reason it's better to keep the number of
    entries in this file as low as possible. Permissions are a string that
    can contain:

    M   open mailing features

    R   open relay features (bypass all other relay blocking traps)

    V   VRFY command enabled (bypass SERVER.TAB variable)

    T   ETRN command enabled (bypass SERVER.TAB variable)

    Z   disable mail size checking (bypass SERVER.TAB variable)

    When PLAIN, LOGIN or CRAM-MD5 authentication mode are used, first a
    lookup in 'MAILUSERS.TAB' accounts is performed to avoid duplicating
    information with 'SMTPAUTH.TAB'. Therefore when using these
    authentication modes a user must use as username the full email address
    (the : separator is permitted instead of @) and as password his POP3
    password. If the lookup succeed the 'SERVER.TAB' variable
    'DefaultSmtpPerms' is used to assign user SMTP permissions (default MR).
    If the lookup fails then 'SMTPAUTH.TAB' lookup is done.

    [table index] [configuration] [top]

   SMTPEXTAUTH.TAB

    Besides internal SMTP authentication methods a user (XMail
    administrator) can define custom authentication procedures by setting up
    this file properly. The section "SMTP Client Authentication" explains
    the client part of custom authentication when we put an 'external' line
    inside the configuration file. The file 'SMTPEXTAUTH.TAB' is the server
    part of the custom authentication which has the given format:

     "auth-name"[TAB]"base-challenge"[TAB]"program-path"[TAB]"arg-or-macro"...[NEWLINE]

    This file can contain multiple lines whose 'auth-name' are listed during
    the EHLO command response. Where 'arg-or-macro' can be:

    @@CHALL
        server challenge given by base-challenge + ':' + server-timestamp

    @@DGEST
        client response to server challenge (@CHALL)

    @@FSECRT
        a file containing all the lines (username + ':' + password) of
        SMTPAUTH.TAB

    Example:

     "RSA-AUTH" "foochallenge" "/usr/bin/myrsa-authenticate"=>
       "-c" "@@CHALL" "-f" "@@FSECRT" "-d" "@@DGEST"

    The external program must test all lines of '@@FSECRT' to find the one
    (if it exists) that matches the client digest (@@DGEST). If it finds a
    match, it must return zero and overwrite '@@FSECRT' with the matching
    secret (username + ':' + password). If a match is not found, the program
    must return a value other than zero.

    [table index] [configuration] [top]

   USERDEF.TAB

     "varname"[TAB]"varvalue"[NEWLINE]

    Example:

     "RealName"  "??"
     "HomePage"  "??"
     "Address"   "??"
     "Telephone" "??"
     "MaxMBSize" "10000"

    contains user default values for new users that are not set during the
    new account creation. This file is looked up in two different places,
    first in '$MAIL_ROOT/domains/DOMAIN' then in '$MAIL_ROOT', where
    'DOMAIN' is the name of the domain where We're going to create the new
    user.

    For each 'domain' handled by the server we'll create a directory
    'domain' inside $MAIL_ROOT. Inside $MAIL_ROOT/'domain' reside
    'domain'->'account' directories ($MAIL_ROOT/'domain'/'account'). This
    folder contains a sub folder named 'mailbox' (or
    'Maildir/(tmp,new,cur)') that stores all 'account' messages. It also
    contains a file named 'USER.TAB' that stores"account" variabiles,
    example:

     "RealName"  "Davide Libenzi"
     "HomePage"  "http://www.xmailserver.org/davide.html"
     "MaxMBSize" "30000"

    [table index] [configuration] [top]

   CTRLACCOUNTS.TAB

     "username"[TAB]"password"[NEWLINE]

    This file contains the accounts that are enable to remote administer
    XMail. The password is encrypted with the 'XMCrypt' program supplied
    with the source distro.

    'REMEMBER THAT THIS HOLDS ADMIN ACCOUNTS, SO PLEASE CHOOSE COMPLEX
    USERNAMES AND PASSWORDS AND USE CTRL.IPMAP.TAB TO RESTRICT IP ACCESS!
    REMEMBER TO REMOVE THE EXAMPLE ACCOUNT FROM THIS FILE!'

    [table index] [configuration] [top]

   SPAMMERS.TAB

     "ipaddr"[TAB]"netmask"[NEWLINE]

    or:

     "ipaddr"[TAB]"netmask"[TAB]"params"[NEWLINE]

    or:

     "ipaddr/bits"[NEWLINE]

    or:

     "ipaddr/bits"[TAB]"params"[NEWLINE]

    Example:

     "212.131.173.0"  "255.255.255.0"
     "212.131.173.0/24"

    register all hosts of the class 'C' network '212.131.173.XXX' as
    spammers, and block them the use of XMail SMTP server. If a match is
    found on one of those records, XMail will reject the incoming SMTP
    connection at early stages. It is possible to specify optional
    parameters to tell XMail which behaviour it should assume in case of
    match. An example of such setup is:

     "212.131.173.0/24"  "code=0"

    In this case a code=0 tells XMail to flag the connection as possible
    spammer, but wait later SMTP session stages to reject the connection
    itself. In this case an authenticated SMTP session can override the
    SPAMMERS.TAB match. The optional "params" field lists parameters
    associated with the record, separated by a comma:

     "param1=value1,param2=value2,...,paramN=valueN"

    Currently supported parameters are:

    code
        Specify the rejection code for the record. If the value is greater
        than zero, the connection is rejected soon, and the remote SMTP
        client is disconnected. If the value is zero, the connection is
        flagged as spammer but wait later stages for rejection, by allowing
        authenticated SMTP connections to bypass the SPAMMERS.TAB match. If
        the value is less than zero, XMail will insert an "absolute value"
        seconds delay between SMTP commands.

    [table index] [configuration] [top]

   SPAM-ADDRESS.TAB

     "spam-address"[NEWLINE]

    Example:

     "*@rude.net"
     "*-admin@even.more.rude.net"

    blocks mails coming from the entire domain 'rude.net' and coming from
    all addresses that end with '-admin@'even.more.rude.net.

    [table index] [configuration] [top]

   POP3.IPMAP.TAB

     "ipaddr"[TAB]"netmask"[TAB]"permission"[TAB]"precedence"[NEWLINE]

    This file controls the global IP access permission to the POP3 server if
    located in the MAIL_ROOT path, and user IP access to its POP3 mailbox if
    located inside the user directory.

    Example:

     "0.0.0.0"  "0.0.0.0"  "DENY"  "1"
     "212.131.173.0"  "255.255.255.0"  "ALLOW"  "2"

    This configuration denies access to all IPs except the ones of the class
    'C' network '212.131.173.XXX'.

    Higher precedences win over lower ones.

    [table index] [configuration] [top]

   SMTP.IPMAP.TAB

     "ipaddr"[TAB]"netmask"[TAB]"permission"[TAB]"precedence"[NEWLINE]

    This file controls IP access permission to SMTP server.

    Example:

     "0.0.0.0"  "0.0.0.0"  "DENY"  "1"
     "212.131.173.0"  "255.255.255.0"  "ALLOW"  "2"

    This configuration denies access to all IPs except the ones of the class
    'C' network '212.131.173.XXX'.

    Higher precedences win over lower ones.

    [table index] [configuration] [top]

   CTRL.IPMAP.TAB

     "ipaddr"[TAB]"netmask"[TAB]"permission"[TAB]"precedence"[NEWLINE]

    This file control IP access permission to CTRL server. Example:

     "0.0.0.0"  "0.0.0.0"  "DENY"  "1"
     "212.131.173.0"  "255.255.255.0"  "ALLOW"  "2"

    This configuration deny access to all IPs except the ones of the class
    'C' network '212.131.173.XXX'. Higher precedences win over lower ones.

    [table index] [configuration] [top]

   FINGER.IPMAP.TAB

     "ipaddr"[TAB]"netmask"[TAB]"permission"[TAB]"precedence"[NEWLINE]

    This file controls IP access permission to FINGER server. Example:

     "0.0.0.0"  "0.0.0.0"  "DENY"  "1"
     "212.131.173.0"  "255.255.255.0"  "ALLOW"  "2"

    This configuration denies access to all IPs except the ones of the class
    'C' network '212.131.173.XXX'. Higher precedences win over lower ones.

    [table index] [configuration] [top]

   USER.TAB

     "variable"[TAB]"value"[NEWLINE]

    store user information such as:

     "RealName"  "Davide Libenzi"
     "HomePage"  "http://www.xmailserver.org/davide.html"
     "MaxMBSize" "30000"
     "ClosedML"  "0"

    Please refer to "USER.TAB variables" below.

    [table index] [configuration] [top]

   MLUSERS.TAB

    If the user is a mailing list this file must exist inside the user
    account subdirectory and contain a list of users subscribed to this
    list. The file format is:

     "user"[TAB]"perms"[NEWLINE]

    where:

    user
        subscriber email address.

    perms
        subscriber permissions:

        R       read.

        W       write (check done using the 'MAIL FROM:<...>' SMTP return
                path).

        A       write (check done using the email address used for SMTP
                authentication).

    Example:

     "davidel@xmailserver.org"   "RW"
     "ghostuser@nightmare.net"   "R"
     "meawmeaw@kitty.cat"        "RA"

    If the 'USER.TAB' file defines the 'ClosedML' variable as '1' then a
    client can post to this mailing list only if it's listed in
    'MLUSERS.TAB' with RW permissions.

    [table index] [configuration] [top]

   MAILPROC.TAB

     "command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    stores commands (internals or externals) that have to be executed on a
    message file. The presence of this file is optional ans if it does not
    exist the default processing is to store the message in user mailbox.
    The 'MAILPROC.TAB' file can be either per user or per domain, depending
    where the file is stored. If stored inside the user directory it applies
    only to the user whose directory hosts the 'MAILPROC.TAB', while if
    stored inside the domain directory it applies to all users of such
    domain. Each argument can be a macro also:

    @@FROM
        is substituted for the sender of the message

    @@RCPT
        is substituted for the recipient of the message

    @@RRCPT
        is substituted for the real recipient (@@RCPT could be an alias) of
        the message

    @@FILE
        is substituted for the message file path (the external command
        _must_ only read the file)

    @@MSGID
        is substituted for the (XMail unique) message id

    @@MSGREF
        is substituted for the reference SMTP message id

    @@TMPFILE
        creates a copy of the message file to a temporary one. It can be
        used with 'external' command but in this case it's external program
        responsibility to delete the temporary file. Do not use it with
        'filter' commands since the filter will have no way to modify the
        real spool file

    @@USERAUTH
        name of the SMTP authenticated user, or "-" if no authentication has
        been supplied

    Supported commands:

    [EXTERNAL]

     "external"[TAB]"priority"[TAB]"wait-timeout"[TAB]"command-path"[TAB]=>
       "arg-or-macro"[TAB]...[NEWLINE]

    where:

    external
        command keyword

    priority
        process priority: 0 = normal -1 = below normal +1 = above normal

    wait-timeout
        wait timeout for process execution in seconds: 0 = nowait

        Be carefull if using @@FILE to give the external command enough
        timeout to complete, otherwise the file will be removed by XMail
        while the command is processing. This is because such file is a
        temporary one that is deleted when XMail exits from 'MAILPROC.TAB'
        file processing. In case the external command exit code will be
        '16', the command processing will stop and all the following
        commands listed inside the file will be skipped.

    [FILTER]

     "filter"[TAB]"priority"[TAB]"wait-timeout"[TAB]"command-path"[TAB]=>
       "arg-or-macro"[TAB]...[NEWLINE]

    where:

    filter
        command keyword

    priority
        process priority: 0 = normal -1 = below normal +1 = above normal

    wait-timeout
        wait timeout for process execution in seconds: 0 = nowait

        With filters, it is not suggested to use @@TMPFILE, since the filter
        will never have the ability to change the message content in that
        way. Also, to avoid problems very difficult to troubleshoot, it is
        suggested to give the filter 'ENOUGH' timeout to complete (90
        seconds or more). See [MESSAGE FILTERS] for detailed information
        about return codes. In the filter command, the "Stop Filter
        Processing" return flag will make XMail to stop the execution of the
        current custom processing file.

    The 'filter' command will pass the message file to a custom external
    filter, that after inspecting it, has the option to accept, reject or
    modify it. Care should be taken to properly re-format the message after
    changing it, to avoid message corruption. The 'filter' command 'CANNOT'
    successfully change the private XMail's header part of the spool
    message.

    [MAILBOX]

     "mailbox"[NEWLINE]

    With this command the message is pushed into local user mailbox.

    [REDIRECT]

     "redirect"[TAB]"domain-or-emailaddress"[TAB]...[NEWLINE]

    Redirect message to internal or external domain or email address. If the
    message was for foo-user@custdomain.net and the file custdomain.net.tab
    contains a line:

     "redirect"  "target-domain.org"

    the message is delivered to 'foo-user@target-domain.org'.

    While the line:

     "redirect"  "user@target-domain.org"

    redirects the message to user@target-domain.org.

    [LREDIRECT]

     "lredirect"[TAB]"domain-or-emailaddress"[TAB]...[NEWLINE]

    Redirect the message to internal or external domain (or email address)
    impersonating local domain during messages delivery. If the message was
    for foo-user@custdomain.net and the file custdomain.net.tab contains a
    line:

     "redirect"  "target-domain.org"

    the message is delivered to 'foo-user@target-domain.org'.

    While the line:

     "redirect"  "user@target-domain.org"

    redirects the message to 'user@target-domain.org'. The difference
    between "redirect" and "lredirect" is the following. Suppose A@B sends a
    message to C@D, that has a redirect to E@F. With "redirect" E@F will see
    A@B has sender while with "lredirect" he will see C@D.

    [SMTPRELAY]

     "smtprelay"[TAB]"server[:port],server[:port],..."[NEWLINE]

    Send mail to the specified SMTP server list by trying the first, if
    fails the second and so on. Otherwise You can use this syntax:

     "smtprelay"[TAB]"#server[:port],server[:port],..."[NEWLINE]

    to have XMail random-select the order the specified relays.

    [table index] [configuration] [top]

   SMTP.IPPROP.TAB

     "ip-addr"[TAB]"var0=value0"...[TAB]"varN=valueN"[NEWLINE]

    Example:

     "192.168.0.7/32"   "WhiteList=1"

    Address selection mask are formed by an IP address (network) plus the
    number of valid bits inside the network mask. No space are allowed
    between the variable name and the '=' sign and between the '=' sign and
    the value. These are the currently defined variables:

    WhiteList
        If set to 1 and if the peer IP matches the address mask, all peer IP
        based checks will be skipped.

    [table index] [configuration] [top]

   FILTERS.IN.TAB

    See [MESSAGE FILTERS]

    [table index] [configuration] [top]

   FILTERS.OUT.TAB

    See [MESSAGE FILTERS]

    [table index] [configuration] [top]

   FILTERS.PRE-DATA.TAB

    See [SMTP MESSAGE FILTERS]

    [table index] [configuration] [top]

   FILTERS.POST-DATA.TAB

    See [SMTP MESSAGE FILTERS]

    [table index] [configuration] [top]

EXTERNAL AUTHENTICATION

    You can use external modules (executables) to perform user
    authentication instead of using XMail 'mailusers.tab' lookups. Inside
    the userauth directory you'll find one directory for each service whose
    authentication can be handled externally (for now only POP3). Suppose We
    must authenticate 'USERNAME' inside 'DOMAIN', XMail first tries to
    lookup (inside userauth/pop3) a file named:

    'DOMAIN.tab'

    else:

    '.tab'

    If one of these files is found, XMail authenticates 'USERNAME' -
    'DOMAIN' using that file. The authentication file is a TAB file (see at
    the proper section in this document) which has the given structure:

     "auth-action"[TAB]"command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    Each argument can be a macro also:

    @@USER
        the USERNAME to authenticate

    @@DOMAIN
        the DOMAIN to authenticate

    @@PASSWD
        the user password

    @@PATH
        user path

    The values for 'auth-action' can be one of:

        item userauth

        executed when user authentication is required

    useradd
        executed when a user need to be added

    useredit
        executed when a user change is required

    userdel
        executed when a user deletion is required

    domaindrop
        executed when all domain users need to be deleted

    The first line that stores the handling command for the requested action
    is executed as:

     command arg0 ... argN

    that must return zero if successful. Any other exit code is interpreted
    as authentication operation failure, that. in 'userauth' case, means
    such user is not authenticated.

    If the execution of the command fails for system reasons (command not
    found, access denied, etc ...) then the user is not authenticated.

    If none of this file's id are found, then usual authentication is
    performed ('mailusers.tab'). The use of external authentication does not
    avoid the presence of the user entry in 'mailusers.tab'.

    [top]

SMTP CLIENT AUTHENTICATION

    When a message is to be sent through an SMTP server that requires
    authentication, XMail provides a way to handle this task by if the
    'userauth/smtp' subdirectory is set up properly.

    Suppose a mail is to be sent through the SMTP server 'mail.foo.net',
    this makes XMail to search for a file named (inside userauth/smtp):

    'mail.foo.net.tab'

    then:

    'foo.net.tab'

    then:

    'net.tab'

    If one of these files is found its content is used to authenticate the
    SMTP client session. The structure of this file, as the extension says,
    is the TAB one used for most of the configuration files inside XMail.
    Only the first valid line (uncommented #) is used to choose the
    authentication method and lines has this format:

     "auth-type"[TAB]"param1"...[TAB]"paramN"[NEWLINE]

    Valid lines are:

     "plain" "username"  "password"

    or

     "login" "username"  "password"

    or

     "cram-md5"  "username"  "password"

    or

     "external"  "auth-name" "secret"    "prog-path" "arg-or-macro"  ...

    Where 'auth-name' can be any symbolic name and 'arg-or-macro' can be a
    program argument or one of these macros:

    @@CHALL
        server challenge string

    @@SECRT
        authentication secret

    @@RFILE
        output response file path

    For example:

     "external" "RSA-AUTH" "mysecret" "/usr/bin/myrsa-auth" "-c" "@@CHALL" "-s"=>
       "@@SECRT" "-f" "@@RFILE"

    XMail sends a line like:

     AUTH RSA-AUTH

    to the SMTP server, and wait for a line like:

     3?? base64-challenge

    Then XMail decodes 'base64-challenge' and invokes the external program
    to get the response to send to the SMTP server. The external program
    must return zero upon success and must put the response into the file
    @@RFILE (without new line termination).

    [top]

CUSTOM DOMAIN MAIL PROCESSING

    If a message that has as target domain of 'sub1.sub2.domain.net' arrives
    at the XMail server, 'AND' XMail does not have a real domain
    'sub1.sub2.domain.net' inside its domain list, XMail decides if this
    domain gets a custom domain processing by trying to lookup:

     sub1.sub2.domain.net.tab
     .sub2.domain.net.tab
     .domain.net.tab
     .net.tab
     .tab

    inside the 'custdomains' directory.

    If one of these files is found the incoming mail gets custom domain
    processing by executing commands that are stored in such a file.

    The format is:

     "command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    These tables store commands (internals or externals) that have to be
    executed on the message file. The presence of one of these files is
    optional and if none exist the default processing is applied to the
    message via SMTP.

    Each argument can be a macro also:

    @@FROM
        the sender of the message

    @@RCPT
        the target of the message

    @@FILE
        the message file path (the external command 'must only read' the
        file)

    @@MSGID
        the (XMail unique) message id

    @@MSGREF
        the reference SMTP message id

    @@TMPFILE
        creates a copy of the message file to a temporary one. It can be
        used with 'external' command but in this case it's external
        program's responsibility to delete the temporary file

    @@USERAUTH
        name of the SMTP authenticated user, or "-" if no authentication has
        been supplied

    Supported commands:

    [EXTERNAL]
         "external"[TAB]"priority"[TAB]"wait-timeout"[TAB]"command-path"[TAB]=>
           "arg-or-macro"[TAB]...[NEWLINE]

        where:

        external
                command keyword

        priority
                process priority: 0 = normal -1 = below normal +1 = above
                normal

        wait-timeout
                wait timeout for process execution in seconds: 0 = nowait

                Be carefull if using @@FILE to give the external command
                enough timeout to complete, otherwise the file will be
                removed by XMail while the command is processing. This is
                because such file is a temporary one that is deleted when
                XMail exits from file processing. In case the external
                command exit code will be '16', the command processing will
                stop and all the following commands listed inside the file
                will be skipped.

    [FILTER]
         "filter"[TAB]"priority"[TAB]"wait-timeout"[TAB]"command-path"[TAB]=>
           "arg-or-macro"[TAB]...[NEWLINE]

        where:

        filter
            command keyword

        priority
            process priority: 0 = normal -1 = below normal +1 = above normal

        wait-timeout
            wait timeout for process execution in seconds: 0 = nowait

            With filters, it is not suggested to use @@TMPFILE, since the
            filter will never have the ability to change the message content
            in that way. Also, to avoid problems very difficult to
            troubleshoot, it is suggested to give the filter 'ENOUGH'
            timeout to complete (90 seconds or more). See [MESSAGE FILTERS]
            for detailed information about return codes. In the filter
            command, the "Stop Filter Processing" return flag will make
            XMail to stop the execution of the current custom processing
            file.

        The 'filter' command will pass the message file to a custom external
        filter, that after inspecting it, has the option to accept, reject
        or modify it. Care should be taken to properly re-format the message
        after changing it, to avoid message corruption. The 'filter' command
        'CANNOT' successfully change the private XMail's header part of the
        spool message.

    [REDIRECT]
         "redirect"[TAB]"domain-or-emailaddress"[TAB]...[NEWLINE]

        Redirect message to internal or external domain or email address. If
        the message was for foo-user@custdomain.net and the file
        custdomain.net.tab contains a line:

         "redirect"  "target-domain.org"

        the message is delivered to 'foo-user@target-domain.org'.

        While the line:

         "redirect"  "user@target-domain.org"

        redirects the message to user@target-domain.org.

    [LREDIRECT]
         "lredirect"[TAB]"domain-or-emailaddress"[TAB]...[NEWLINE]

        Redirect the message to internal or external domain (or email
        address) impersonating local domain during messages delivery. If the
        message was for foo-user@custdomain.net and the file
        custdomain.net.tab contains a line:

         "redirect"  "target-domain.org"

        the message is delivered to 'foo-user@target-domain.org'.

        While the line:

         "redirect"  "user@target-domain.org"

        redirects the message to 'user@target-domain.org'. The difference
        between "redirect" and "lredirect" is the following. Suppose A@B
        sends a message to C@D, that has a redirect to E@F. With "redirect"
        E@F will see A@B has sender while with "lredirect" he will see C@D.

    [SMTPRELAY]
         "smtprelay"[TAB]"server[:port],server[:port],..."[NEWLINE]

        Send mail to the specified SMTP server list by trying the first, if
        that fails, the second and so on.

        Otherwise you can use this syntax:

         "smtprelay"[TAB]"#server[:port],server[:port],..."[NEWLINE]

        To have XMail random-select the order the specified relays.

    [SMTP]
         "smtp"[NEWLINE]

        Do SMTP delivery.

    [top]

CMD ALIASES

    CmdAliases implement aliases that are handled only through commands and
    can be thought of as a user level implementation of custom domain
    processing commands. The command set is the same of the one that is
    described above ("Custom domain mail processing") and won't be explained
    again here.

    For every handled domain (listed inside 'domains.tab') a directory with
    the same domain name is created inside the 'cmdaliases' subdirectory.
    This directory is automatically created and removed when you add/remove
    domains through the CTRL protocol (or 'CtrlClnt').

    When a mail for 'USER@DOMAIN' is received by the server, the domain
    'DOMAIN' is to be handled locally, and the standard users/aliases lookup
    fails, a file named 'USER.tab' is searched inside
    '$MAIL_ROOT/cmdaliases/DOMAIN'. If such file is found, commands listed
    inside the file (whose format must follow the one described in the
    previous section) are executed by the server as a matter of mail message
    processing. An important thing to remember is that all domain and user
    names, when applied to the file system, must be lower case.

    The use of the command '[SMTP]' must be implemented with great care
    because it could create mail loops within the server.

    [top]

SERVER.TAB VARIABLES

    The following variables are for use int the "SERVER.TAB" configuration
    file.

    [RootDomain]
        Indicate the primary domain for the server.

    [SmtpServerDomain]
        If set, forces the domain name XMail uses inside the ESMTP banner
        used to support CRAM-MD5 ESMTP authentication.

    [POP3Domain]
        Set the default domain for POP3 client connections.

    [PostMaster]
        Set the postmaster address.

    [ErrorsAdmin]
        The email address that receives notification messages for every
        message that has had delivery errors. If it is empty (allowed), the
        notification message is sent to the sender only.

    [TempErrorsAdmin]
        The email address that receives notification for temporary delivery
        failures. In case it's empty the notification message is sent to the
        sender only.

    [DefaultSMTPGateways]
        A comma separated list of SMTP servers XMail 'must' use to send its
        mails. This has the precedence over MX records.

    [HeloDomain]
        If this variable is specified and is not empty, its content is sent
        as HELO domain. Otherwise the reverse lookup of the local IP is sent
        as HELO domain. This helps to deal with remote SMTP servers that are
        set to check the reverse lookup of the incoming IP.

    [CheckMailerDomain]
        Enable validation of the sender domain ('MAIL FROM:<...@xxx>') by
        looking up DNS/MX entries.

    [RemoveSpoolErrors]
        Indicate if mail has to be removed or stored in 'froz' directory
        after a failure in delivery or filtering.

    [NotifyMsgLinesExtra]
        Number of lines of the bounced message that have to be listed inside
        the notify message (lines after the headers section). Default is
        zero.

    [NotifySendLogToSender]
        Enable/Disable sending the message log file inside the notify
        message to the sender. Default is off (zero).

    [NotifyTryPattern]
        List of delivery attempts that require the system to send a
        notification to the sender (and eventually to 'TempErrorsAdmin').
        The list is a comma separated list of numbers (with no extra spaces)
        as in:

         "1,4,9"

        Default is empty which means no notification is sent upon a delivery
        attempt failure.

    [AllowNullSender]
        Enable null sender ('MAIL FROM:<>') messages to be accepted by
        XMail.

    [NoSenderBounce]
        When building bounce messages, use the null SMTP sender ('MAIL
        FROM:<>') instead of the 'PostMaster' address. This will affect only
        the SMTP sender, while the message RFC822 headers will still contain
        the correct From: header.

    [MaxMTAOps]
        Set the maximum number of MTA relay steps before to declare the
        message as looped (default 16).

    [ReceivedHdrType]
        Set the verbosity of the Received: message headers tag.

        '0'     Standard (client IP shown , server IP not). Default.

        '1'     Verbose (client IP shown , server IP shown)

        '2'     Strict (no IP shown)

        '3'     Same as 0 but the client IP is not shown if the client
                authenticate itself.

        '4'     Same as 1 but the client IP is not shown if the client
                authenticate itself.

    [FetchHdrTags]
        Set the list of headers tags to be used to extract addresses from
        POP3 fetched messages ("POP3LINKS.TAB"). This is a comma delimited
        list (no extra space or TABs must be included inside the list) as
        in:

         "+X-Deliver-To,To,Cc"

        Tags preceded by a '+' character make XMail stop scanning when an
        address is found inside the header tag.

        Tags preceded by a '+' character must be listed before other tags.

        The string "+X-Deliver-To,To,Cc" is the default if nothing is
        specified.

    [SMTP-MaxErrors]
        Set the maximum number of errors allowed in a single SMTP session.
        When the maximum number of allowed errors is exceeded, the
        connection will be automatically dropped. If such variable is not
        set, or it is set to zero, the maximum number of errors will be
        unlimited.

    [SmtpMsgIPBanSpammers]
        Used to set the message that is sent to the SMTP client when the
        client IP is listed inside the file SPAMMER.TAB.

    [SmtpMsgIPBanSpamAddress]
        Used to set the message that is sent to the SMTP client when the
        client IP is listed inside the file SPAM-ADDRESS.TAB.

    [SmtpMsgIPBanMaps]
        Used to set the message that is sent to the SMTP client when the
        client IP is listed inside one of the "CustMapsList".

    [CustomSMTPMessage]
        Set this to the message that you want to follow the standard SMTP
        error response sent by XMail, as in (one line, remember the =>):

         "Please open http://www.xmailserver.test/smtp_errors.html to get=>
            more information about this error"

        Please be aware the RFC821 fix the maximum reply line length to 512
        bytes.

    [AllowSmtpVRFY]
        Enable the use of VRFY SMTP command. This flag may be forced by SMTP
        authentication.

    [AllowSmtpETRN]
        Enable the use of ETRN SMTP command. This flag may be forced by SMTP
        authentication.

    [SmtpMinDiskSpace]
        Minimum disk space (in Kb) that is requested before accepting an
        SMTP connection.

    [SmtpMinVirtMemSpace]
        Minimum virtual memory (in Kb) that is requested before accepting an
        SMTP connection.

    [Pop3MinVirtMemSpace]
        Minimum virtual memory (in Kb) that is requested before accepting a
        POP3 connection.

    [Pop3SyncErrorAccount]
        This defines the email account (MUST be handled locally) that
        receives all fetched email that XMail has not been able to deliver.

    [EnableAuthSMTP-POP3]
        Enable SMTP after POP3 authentication (default on).

    [MaxMessageSize]
        Set the maximum message size in Kb that is possible to send through
        the server.

    [DefaultSmtpPerms]
        This list SMTP permissions assigned to users looked up inside
        "MAILUSERS.TAB" during SMTP authentication. It also defines the
        permissions for users authenticated with SMTP after POP3.

    [CustMapsList]
        This is a list a user can use to set custom maps checking. The list
        has the given (strict) format:

        maps-root:code,maps-root:code...

        Where maps-root is the root for the dns query (ie.
        dialups.mail-abuse.org.) and the code can be:

        '1'     the connection is drooped soon

        "0"     the connection is kept alive but only authenticated users
                can send mail

        '-S'    the peer can send messages but a delay of S seconds is
                introduced between commands

    [SMTP-RDNSCheck]
        Indicate if XMail must do an RDNS lookup before accepting a incoming
        SMTP connection. If 0, the check is not performed; if 1 and the
        check fails, the user receives a 'server use forbidden' at MAIL_FROM
        time; if -S (S > 0) and the check fails, a delay of S seconds
        between SMTP commands is used to prevent massive spamming.

        SMTP authentication overrides the denial set by this option by
        giving authenticated users the ability to access the server from
        'mapped' IPs.

    [SmartDNSHost]
        Setup a list of smart DNS hosts to which are directed DNS queries
        with recursion bit set to true. Such DNS hosts must support DNS
        recursion in queries. The format is:

         dns.home.bogus.net:tcp,192.168.1.1:udp,...

    [DisableEmitAuthUser]
        Enable/disable the emission the the 'X-AuthUser:' mail header for
        authenticated users. Valid values are "0" or '1', default is "0"
        (emission enabled).

    [DynDnsSetup]
        Give the possibility to handle dynamic IP domain registration to
        dynamic IP servers. One of these service providers is
        'www.dyndns.org' whose site you can watch for registrations and more
        info.

        The string has the format:

         server,port,HTTP-GET-String[,username,password]

        For Example:

        members.dyndns.org,80,/nic/dyndns?action=edit&started=1&hostname=YES
        &host_id=yourhost.ourdomain.ext&myip=%s&wildcard=OFF&mx=mail.exchang
        er.ext&backmx=NO,foouser,foopasswd

        or

        www.dns4ever.com,80,/sys/u.cgi?d=DOMAIN&u=USERNAME&p=PASSWORD&i=%s

        where:

        DOMAIN  the domain you've registered

        USERNAME
                the username you get from service provider

        PASSWORD
                the password you get from service provider

        The %s in HTTP-GET-String is replaced with the IP address to
        register.

    [SmtpConfig]
        Default SMTP server config loaded if specific server IP config is
        not found.

    [SmtpConfig-XXX.YYY.ZZZ.WWW]
        Specific IP SMTP server config. The variable value is a comma
        separated sequence of configuration tokens whose meaning is:

        mail-auth
                authentication required to send mail to the server. Please
                note that by setting this value everything requires
                authentication, even for sending to local domains, and this
                is probably not what you want.

    [top]

MESSAGE FILTERS

    This feature offers a way to filter messages by providing the ability to
    execute external programs, such as scripts or real executables. These
    'filters' may examine and/or modify messages and inform XMail of their
    actions with a return value.

    This feature offers the ability to inspect and modify messages, giving a
    way to reject messages based on content, alter messages (address
    rewriting) and so on.

    If this filters returns '4, 5 or 6' the message is rejected and is
    stopped in its travel. If the filter modifies the message it must return
    '7'.

    Additional flags are allowed to be returned to XMail as a result of
    filter processing by adding the flags value to the exits code above
    listed. The currently defined flags are :

    '16'
        Stop selected filter list processing.

    Filter flags are additive and if more than one flag need to be
    specified, their values must be added together. If a filter "raw" exit
    code is RC and the filter needs to return extra flags FILTER-SUM, the
    final return code FRC must be :

    FRC = RC + FILTER-SUM

    Example. Suppose a filter modified the message and hence needs to return
    7 as return code. Suppose also that a filter wants to block the filter
    selection list processing by specifying a flags value of 16, the value
    to be returned will be :

    FRC = 7 + 16 = 23

    Filter selection is driven by two files 'FILTERS.IN.TAB' and
    'FILTERS.OUT.TAB' located inside the $MAIL_ROOT/ directory and that have
    the following format:

     "sender"[TAB]"recipient"[TAB]"remote-addr"[TAB]"local-addr"[TAB]"filename"[NEWLINE]

    For example:

     "*@bad-domain.com" "*" "0.0.0.0/0" "0.0.0.0/0" "av-filter.tab"
     "*" "clean@purified.net" "0.0.0.0/0" "0.0.0.0/0" "spam-block.tab"
     "*" "*" "192.168.1.0/24" "0.0.0.0/0" "archive.tab"

    where the file "av-filter.tab" must be present inside the
    $MAIL_ROOT/filters directory. The "sender" and the "recipient" are
    resolved to the real account when possible. Address selection mask are
    formed by an IP address (network) plus the number of valid bits inside
    the network mask. The file 'FILTERS.IN.TAB' lists filters that have to
    be applied to inbound messages (going to local mailboxes) while the file
    'FILTERS.OUT.TAB' lists filters that have to be applied to outbound
    messages (delivered remotely). All four
    (sender+recipient+remote-addr+local-addr) selection fields must have a
    match in order "filename" to be evaluated. The syntax of the filter file
    is:

     "command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    or:

     "!flags"[TAB]"command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    Each file may contain multiple commands, that will be executed in
    strictly sequential order. The first command that will trigger a
    rejection code will make the filtering process to end. The 'flags'
    parameter is a comma-separated list of flags that drives the filter
    execution. The syntax of each flag is either FLAG or FLAG=VAL. Currently
    supported flags are:

    aex exclude filter execution in case of authenticated sender

    Each argument can be a macro also:

    @@FROM
        the sender of the message

    @@RFROM
        the sender of the message resolved to the real account, when
        possible (alias resolution)

    @@RCPT
        the target of the message

    @@RRCPT
        the target of the message resolved to the real account, when
        possible (alias resolution)

    @@REMOTEADDR
        remote IP address and port of the sender

    @@LOCALADDR
        local IP address and port where the message has been accepted

    @@FILE
        the message file path (the external command may modify the file if
        it returns '7' as command exit value.)

    @@MSGID
        with the (XMail unique) message id

    @@MSGREF
        the reference SMTP message id

    @@USERAUTH
        name of the SMTP authenticated user, or "-" if no authentication has
        been supplied

    Here 'command' is the name of an external program that processes the
    message and returns its processing result. If it returns '6' the message
    is rejected and a notification message is sent to the sender. By
    returning '5' the message is rejected without notification. While
    returning '4' the message is rejected without notification and without
    being frozen (a '5' response could lead to a frozen message if the
    "SERVER.TAB" configuration enables this). If all filters return values
    different from '6, 5 and 4' the message continues its trip. The filter
    command may also modify the file (AV scanning, content filter, message
    rewriting, etc) by returning '7'. The filter 'MUST' return '7' in case
    it modifies the message. If the filter changes the message file it
    'MUST' keep the message structure and it 'MUST' terminate all line with
    <CR><LF>. The filter has also the ability to return a one-line custom
    return message by creating a file named @@FILE.rej holding the message
    in the very first line. This file should be created 'ONLY' when the
    filter returns a rejection code ('6, 5 and 4')and 'NEVER' in case of
    passthru code ('7') or modify code.

    The spool files has this structure:

     Info Data           [ 1th line ]
     SmtpDomain          [ 2nd line ]
     SmtpMessageID       [ 3rd line ]
     MAIL FROM:<...>     [ 4th line ]
     RCPT TO:<...>       [ 5th line ]
     <<MAIL-DATA>>       [ 6th line ]
     ...

    After the '<<MAIL-DATA>>' tag (5th line) the message follows. The
    message is composed of a headers section and, after the first empty
    line, the message body. The format of the "Info Data" line is:

     ClientDomain;ClientIP;ClientPort;ServerDomain;ServerIP;ServerPort;Time;Logo

    'EXTREME' care must be used when modifying the message because the
    filter will be working on the real message, and a badly reformatted file
    will lead to message loss. The spool file header (any data before
    <<MAIL-DATA>>) 'MUST' be preserved as is by the filter in case of
    message rewrite happens.

    [top]

SMTP MESSAGE FILTERS

    Besides having the ability to perform off-line message filtering, XMail
    gives the user the power to run filters during the SMTP session. Two
    files drive the SMTP on-line filtering, and these are
    'FILTERS.PRE-DATA.TAB' and 'FILTERS.POST-DATA.TAB'. The file
    'FILTERS.PRE-DATA.TAB' contains one or more commands to be executed
    after the remote SMTP client sends the DATA command, and before XMail
    sends the response to the command. Using such filters, the user can tell
    XMail if or if not accept the following DATA transaction and, in case of
    rejection, the user is also allowed to specify a custom message to be
    sent to the remote SMTP client. The file 'FILTERS.POST-DATA.TAB'
    contains one or more commands to be executed after XMail received the
    whole client DATA, and before XMail sends the final response to the DATA
    command (final messages ack). The files 'FILTERS.PRE-DATA.TAB' and
    'FILTERS.POST-DATA.TAB' conatins zero or more lines with the following
    format:

     "command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    or:

     "!flags"[TAB]"command"[TAB]"arg-or-macro"[TAB]...[NEWLINE]

    Each file may contain multiple commands, that will be executed in
    strictly sequential order. The first command that will trigger a
    rejection code will make the filtering process to end. The 'flags'
    parameter is a comma-separated list of flags that drives the filter
    execution. The syntax of each flag is either FLAG or FLAG=VAL. Currently
    supported flags are:

    aex exclude filter execution in case of authenticated sender

    Each argument can be a macro also:

    @@FILE
        message file path

    @@USERAUTH
        name of the SMTP authenticated user, or "-" if no authentication has
        been supplied

    @@REMOTEADDR
        remote IP address and port of the sender

    @@LOCALADDR
        local IP address and port where the message has been accepted

    Filter commands have the ability to inspect and modify the content of
    the message (or info) file. The exit code of commands executed by XMail
    are used to tell XMail the action that has to be performed as a
    cosequence of the filter. The exit code is composed by a raw exit code
    and additional flags. Currently defined flags are:

    '16'
        Stop selected filter list processing.

    Currently defined raw exit codes are:

    '3' Reject the message.

    Any other exit codes will make XMail to accept the message, and can be
    used also when changing the content of the @@FILE file. 'EXTREME' care
    must be used when changing the @@FILE file, since XMail expect the file
    format to be correct. Also, it is important to preserve the <CR><LF>
    line termination of the file itself. When rejecting the message, the
    filter command has the ability to specify the SMTP status code that
    XMail will send to the remote SMTP client, by creating a file named
    @@FILE.rej containing the message in the very first line. Such file will
    be automatically removed by XMail. The data passed to filter commands
    inside @@FILE varies depending if the command is listed inside
    'FILTERS.PRE-DATA.TAB' or inside 'FILTERS.POST-DATA.TAB'. Commands
    listed inside 'FILTERS.PRE-DATA.TAB' will receive the following data
    stored inside @@FILE:

     Info Data           [ 1th line ]
     SmtpDomain          [ 2nd line ]
     SmtpMessageID       [ 3rd line ]
     MAIL FROM:<...>     [ 4th line ]
     RCPT TO:<...> {...} [ 5th line ]
     ...

    The file can have one or more "RCPT TO" lines. The format of the "Info
    Data" line is:

     ClientDomain;ClientIP;ClientPort;ServerDomain;ServerIP;ServerPort;Time;Logo

    Commands listed inside 'FILTERS.POST-DATA.TAB' will receive the
    following data stored inside @@FILE:

     Info Data           [ 1th line ]
     SmtpDomain          [ 2nd line ]
     SmtpMessageID       [ 3rd line ]
     MAIL FROM:<...>     [ 4th line ]
     RCPT TO:<...> {...} [ 5th line ]
     ...
     <<MAIL-DATA>>
     ...

    After the '<<MAIL-DATA>>' tag the message follows. The message is
    composed of a headers section and, after the first empty line, the
    message body. The format of the RCPT line is:

     RCPT TO:<address> {ra=real-address}

    where "real-address" is the "address" after it has been translated (if
    aliases applies) to the real local address. Otherwise it holds the same
    value of "address".

    [top]

USER.TAB VARIABLES

    The following variables are for use in the "USER.TAB" configuration
    file.

    [RealName]
        Full user name, ie.:

         "RealName"  "Davide Libenzi"

    [HomePage]
        User home page, ie.:

         "HomePage"  "http://www.xmailserver.org/davide.html"

    [MaxMBSize]
        Max user mailbox size in Kb, ie.:

         "MaxMBSize" "30000"

    [ClosedML]
        Specify if the mailing list is closed only to subscribed users, ie.:

         "ClosedML"  "1"

    [ListSender]
        Specify the mailing list sender or administrator:

         "ListSender"    "ml-admin@xmailserver.org"

        This variable should be set to avoid delivery error notifications to
        reach the original message senders.

    [SmtpPerms]
        User SMTP permissions (see SMTPAUTH.TAB for info).

    [ReceiveEnable]
        Set to '1' if the account can receive email, '0' if you want to
        disable the account from receiving messages.

    [PopEnable]
        Set to '1' if you want to enable the account to fetch POP3 messages,
        '0' otherwise.

    [UseReplyTo]
        Enable/Disable the emission of the Reply-To: header for mailing
        list's messages (default 1).

    [MaxMessageSize]
        Set the maximum message size (in Kb) that the user is able to send
        through the server. Overrides the SERVER.TAB variable.

    [DisableEmitAuthUser]
        Enable/disable the emission the the 'X-AuthUser:' mail header for
        authenticated users. Valid values are '0' or '1', default is '0'
        (emission enabled). This variable overrides the SERVER.TAB one when
        present.

    [top]

MAIL ROUTING THROUGH ADDRESSES

    A full implementation of SMTP protocol allows the ability to perform
    mail routing bypassing DNS MX records by means of setting, in a ruled
    way, the 'RCPT TO: <>' request. A mail from 'xuser@hostz' directed to
    '@hosta,@hostb:foouser@hostc' is received by '@hosta' then sent to
    '@hostb' using 'MAIL FROM: <@hosta:xuser@hostz>' and 'RCPT TO:
    <@hostb:foouser@hostc>'. The message is then sent to '@'hostc using
    'MAIL FROM: <@hostb,@hosta:xuser@hostz>' and 'RCPT TO: <foouser@hostc>'.

    [top]

XMAIL SPOOL DESIGN

    The new spool fs tree format has been designed to enable XMail to handle
    very large queues. Instead of having a single spool directory (like
    versions older than 0.61) a two layer deep splitting has been introduced
    so that its structure is:

     0   <dir>
       0   <dir>
         mess    <dir>
         rsnd    <dir>
         info    <dir>
         temp    <dir>
         slog    <dir>
         cust    <dir>
         froz    <dir>
       ...
     ...

    When XMail needs to create a new spool file a spool path is chosen in a
    random way and a new file with the format:

     mstime.tid.seq.hostname

    is created inside the 'temp' subdirectory. When the spool file is ready
    to be committed, it's moved into the 'mess' subdirectory that holds
    newer spool files. If XMail fails sending a new message (the ones in
    mess subdirectory) it creates a log file (with the same name of the
    message file) inside the 'slog' subdirectory and move the file from
    'mess' to 'rsnd'. During the message sending the message itself is
    locked by creating a file inside the 'lock' subdirectory (with the same
    name of the message file). If the message has permanent delivery errors
    or is expired and if the option 'RemoveSpoolErrors' of the 'SERVER.TAB'
    file is off, the message file is moved into the 'froz' subdirectory.

    [top]

SMTP COMMANDS

    These are commands understood by ESMTP server:

    MAIL FROM:<>
    RCPT TO:<>
    DATA
    HELO
    EHLO
    AUTH
    RSET
    VRFY
    ETRN
    NOOP
    HELP
    QUIT

    [top]

POP3 COMMANDS

    These are commands understood by POP3 server:

    USER
    PASS
    APOP
    STAT
    LIST
    UIDL
    QUIT
    RETR
    TOP
    DELE
    NOOP
    LAST
    RSET

    [top]

COMMAND LINE

    Most of XMail configuration settings are command line tunables. These
    are command line switches organized by server.

    [XMAIL]

        -Ms pathname
                Mail root path (also settable with MAIL_ROOT environment).

        -Md     Activate debug (verbose) mode.

        -Mr hours
                Set log rotate hours step.

        -Mx split-level
                Set the queue split level. The value you set here is rounded
                to the lower prime number higher or equal than the value
                you've set.

        -MR bytes
                Set the size of the socket's receive buffer in bytes
                (rounded up to 1024).

        -MS bytes
                Set the size of the socket's send buffer in bytes (rounded
                up to 1024).

        -MM     Setup XMail to use 'Maildir' delivery (default on Unix).

        -Mm     Setup XMail to use 'mailbox' delivery (default on Windows).

        -MD ndirs
                Set the number of subdirectories allocated for the DNS cache
                files storage ( default 101 ).

    [POP3]

        -Pp port
                Set POP3 server port (if you change this you must know what
                you're doing).

        -Pt timeout
                Set POP3 session timeout (seconds) after which the server
                closes. the connection if it does not receive any commands.

        -Pl     Enable POP3 logging.

        -Pw timeout
                Set the delay timeout in response to a bad POP3 login. Such
                time is doubled at the next bad login.

        -Ph     Hang the connection in bad login response.

        -PI ip[:port]
                Bind server to the specified ip address and (optional) port
                (can be multiple).

        -PX nthreads
                Set the maximum number of threads for POP3 server.

    [SMTP]

        -Sp port
                Set SMTP server port (if you change this you must know what
                you're doing).

        -St timeout
                Set SMTP session timeout (seconds) after which the server
                closes the connection if no commands are received.

        -Sl     Enable SMTP logging.

        -SI ip[:port]
                Bind server to the specified ip address and (optional) port
                (can be multiple).

        -SX nthreads
                Set the maximum number of threads for SMTP server.

        -Sr maxrcpts
                Set the maximu number of recipients for a single SMTP
                message (default 100).

        -Se nsecs
                Set the expire timeout for a POP3 authentication IP (default
                900).

    [SMAIL]

        -Qn nthreads
                Set the number of mailer threads.

        -Qt timeout
                Set the time to be wait for a next try after send failure.
                Default 480.

        -Qi ratio
                Set the increment ratio of the reschedule time in sending a
                messages. At every failure in delivery a message, reschedule
                time T is incremented by (T / ratio), therefore :

                 T(i) = T(i-1) + T(i-1)/ratio.

                If you set this ratio to zero, T remain unchanged over
                delivery tentatives. Default 16.

        -Qr nretries
                Set the maximum number of times to try to send the message.
                Default 32.

        -Ql     Enable SMAIL logging.

        -QT timeout
                Timeout value for filters commands in seconds. Default 90.

        -Qg     Enable filter logging.

    [PSYNC]

        -Yi interval
                Set external POP3 accounts sync interval. Setting this to
                zero will disable the PSYNC task. Default 120.

        -Yt nthreads
                Set the number of POP3 sync threads.

        -Yl     Enable PSYNC logging.

    [FINGER]

        -Fp port
                Set FINGER server port (if you change this you must know
                what you're doing).

        -Fl     Enable FINGER logging.

        -FI ip[:port]
                Bind server to the specified ip address and (optional) port
                (can be multiple).

    [CTRL]

        -Cp port
                Set CTRL server port (if you change this you must know what
                you're doing).

        -Ct timeout
                Set CTRL session timeout (seconds) after which the server
                closes the connection if no commands are received.

        -Cl     Enable CTRL logging.

        -CI ip[:port]
                Bind server to the specified ip address and (optional) port
                (can be multiple).

        -CX nthreads
                Set the maximum number of threads for CTRL server.

    [LMAIL]

        -Ln nthreads
                Set the number of local mailer threads.

        -Lt timeout
                Set the sleep timeout for LMAIL threads (in seconds, default
                2).

        -Ll     Enable local mail logging.

    [top]

XMAIL ADMIN PROTOCOL

    It's possible to remote admin XMail due to the existence of a
    'controller server' that runs with XMail and waits for TCP/IP
    connections on a port (6017 or tunable via a '-Cp nport') command line
    option.

    Admin protocol details:

    "Description"
    "Adding a user"
    "Deleting a user"
    "Changing a user's password"
    "Authenticate user"
    "Retrieve user statistics"
    "Adding an alias"
    "Deleting an alias"
    "Listing aliases"
    "Listing user vars"
    "Setting user vars"
    "Listing users"
    "Getting mailproc.tab file"
    "Setting mailproc.tab file"
    "Adding a mailing list user"
    "Deleting a mailing list user"
    "Listing mailing list users"
    "Adding a domain"
    "Deleting a domain"
    "Listing handled domains"
    "Adding a domain alias"
    "Deleting a domain alias"
    "Listing alias domains"
    "Getting custom domain file"
    "Setting custom domain file"
    "Listing custom domains"
    "Adding a POP3 external link"
    "Deleting a POP3 external link"
    "Listing POP3 external links"
    "Enabling a POP3 external link"
    "Listing files"
    "Getting configuration file"
    "Setting configuration file"
    "Listing frozen messages"
    "Rescheduling frozen message"
    "Deleting frozen message"
    "Getting frozen message log file"
    "Getting frozen message"
    "Starting a queue flush"
    "Do nothing command"
    "Quit the connection"
    "Do you want...?"

  Description

    The XMail admin server 'speaks' a given protocol that can be used by
    external GUI utilities written with the more disparate scripting
    languages, to remote administer the mail server. The protocol is based
    on sending formatted command and waiting for formatted server responses
    and error codes. All the lines, commands, and responses are delimited by
    a <CR><LF> pair. The error code string (I'll call it RESSTRING) has the
    given format:

     "+DDDDD OK"<CR><LF>

    if the command execution is successful while:

     "-DDDDD ErrorString"<CR><LF>

    if the command failed.

    The " character is not included in responses. DDDDD is a numeric error
    code while ErrorString is a description of the error. If DDDDD equals
    00100, a lines list, terminated by a line with a single point
    (<CR><LF>.<CR><LF>), follows the response.

    The input format for commands is similar to the one used in TAB files:

     "cmdstring"[TAB]"param1"[TAB]..."paramN"<CR><LF>

    where 'cmdstring' is the command string identifying the action to be
    performed, and param1,... are the parameters of the command.

    Immediately after the connection with XMail controller server is
    established the client receives a RESSTRING that is:

     +00000 <TimeStamp> XMail ...

    if the server is ready, while:

     -DDDDD ...

    (where DDDDDD is an error code) if not.

    The TimeStamp string has the format:

     currtime.pid@ipaddress

    and is used in MD5 authentication procedure.

    As the first action immediately after the connection the client must
    send an authentication string with this format:

     "user"[TAB]"password"<CR><LF>

    where user must be enabled to remote admin XMail. Clear text
    authentication should not be used due server security. Using MD5
    authentication instead, the client must perform an MD5 checksum on the
    string composed by (<> included):

     <TimeStamp>password

    and then send to the server:

     "user"[TAB]"#md5chksum"<CR><LF>

    where md5chksum is the MD5 checksum (note '#' as first char of sent
    digest). The result of the authentication send is a RESSTRING. If the
    user does not receive a positive authentication response, the connection
    is closed by the server.

    [admin protocol] [top]

  Adding a user

     "useradd"[TAB]"domain"[TAB]"username"[TAB]"password"[TAB]"usertype"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    username
        username to add.

    password
        user password.

    usertype
        'U' for normal user and 'M' for mailing list.

    The result is a RESSTRING.

    [admin protocol] [top]

  Deleting a user

     "userdel"[TAB]"domain"[TAB]"username"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    username
        username to delete.

    The result is a RESSTRING.

    [admin protocol] [top]

  Changing a user's password

     "userpasswd"[TAB]"domain"[TAB]"username"[TAB]"password"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    username
        username (must exist).

    password
        new password.

    The result is a RESSTRING.

    [admin protocol] [top]

  Authenticate user

     "userauth"[TAB]"domain"[TAB]"username"[TAB]"password"<CR><LF>

    where:

    domain
        domain name.

    username
        username.

    password
        password.

    The result is a RESSTRING.

    [admin protocol] [top]

  Retrieve user statistics

     "userstat"[TAB]"domain"[TAB]"username"<CR><LF>

    where:

    domain
        domain name.

    username
        username/alias.

    The result is a RESSTRING. If successful (00100), a formatted matching
    users list follows terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). This is the format of the listing:

     "variable"[TAB]"value"<CR><LF>

    Where valid variables are:

    RealAddress
        real address (maybe different is the supplied username is an alias).

    MailboxSize
        total size of the mailbox in bytes.

    MailboxMessages
        total number of messages.

    LastLoginIP
        last user login IP address.

    LastLoginTimeDate
        time of the last login.

    [admin protocol] [top]

  Adding an alias

     "aliasadd"[TAB]"domain"[TAB]"alias"[TAB]"account"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    alias
        alias to add.

    account
        real email account (locally handled). This can be a fully qualified
        email address or a username inside the same domain.

    The result is a RESSTRING.

    [admin protocol] [top]

  Deleting an alias

     "aliasdel"[TAB]"domain"[TAB]"alias"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    alias
        alias to delete.

    The result is a RESSTRING.

    [admin protocol] [top]

  Listing aliases

     "aliaslist"[TAB]"domain"[TAB]"alias"[TAB]"account"<CR><LF>

    or

     "aliaslist"[TAB]"domain"[TAB]"alias"<CR><LF>

    or

     "aliaslist"[TAB]"domain"<CR><LF>

    or

     "aliaslist"<CR><LF>

    where:

    domain
        domain name, optional (can contain wild cards).

    alias
        alias name, optional (can contain wildcards).

    account
        account, optional (can contain wildcards).

    Example:

     "aliaslist"[TAB]"foo.bar"[TAB]"*"[TAB]"mickey"<CR><LF>

    lists all aliases of user 'mickey' in domain 'foo.bar'.

    The result is a RESSTRING. In successful cases (00100) a formatted
    matching users list follows, terminated by a line containing a single
    dot (<CR><LF>.<CR><LF>). This is the format of the listing:

     "domain"[TAB]"alias"[TAB]"username"<CR><LF>

    [admin protocol] [top]

  Adding an external alias

     "exaliasadd"[TAB]"local-address"[TAB]"remote-address"<CR><LF>

    where:

    local-address
        local email address.

    remote-address
        remote email address.

    For example, the following command string:

     "exaliasadd"[TAB]"dlibenzi@home.bogus"[TAB]"dlibenzi@xmailserver.org"<CR><LF>

    will link the external email address 'dlibenzi@xmailserver.org' with the
    local email address 'dlibenzi@home.bogus'. The result is a RESSTRING.

    [admin protocol] [top]

  Deleting an external alias

     "exaliasdel"[TAB]"remote-address"<CR><LF>

    where:

    remote-address
        remote email address.

    The result is a RESSTRING.

    [admin protocol] [top]

  Listing external aliases

     "exaliaslist"[TAB]"local-address"[TAB]"remote-address"<CR><LF>

    or

     "exaliaslist"[TAB]"local-address"<CR><LF>

    or

     "exaliaslist"<CR><LF>

    where:

    local-address
        local email address. This can contain wildcard characters.

    remote-address
        remote email address. This can contain wildcard characters.

    Example:

     "exaliaslist"[TAB]"*@home.bogus"<CR><LF>

    lists all the external aliases linked to local accounts in domain
    'home.bogus'.

    The result is a RESSTRING. In successful cases (00100) a formatted
    matching users list follows, terminated by a line containing a single
    dot (<CR><LF>.<CR><LF>). This is the format of the listing:

     "rmt-domain"[TAB]"rmt-name"[TAB]"loc-domain"[TAB]"loc-name"<CR><LF>

    [admin protocol] [top]

  Listing user vars

     "uservars"[TAB]"domain"[TAB]"username"<CR><LF>

    where:

    domain
        domain name.

    username
        username.

    The result is a RESSTRING. In successfully cases (00100) a formatted
    list of user vars follow, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). This is the format of the listing:

     "varname"[TAB]"varvalue"<CR><LF>

    [admin protocol] [top]

  Setting user vars

     "uservarsset"[TAB]"domain"[TAB]"username"[TAB]"varname"[TAB]"varvalue" ... <CR><LF>

    where:

    domain
        domain name.

    username
        username.

    varname
        variable name.

    varvalue
        variable value.

    There can be multiple variable assignments with a single call. If
    'varvalue' is the string '.|rm' the variable 'varname' is deleted. The
    result is a RESSTRING.

    [admin protocol] [top]

  Listing users

     "userlist"[TAB]"domain"[TAB]"username"<CR><LF>

    or

     "userlist"[TAB]"domain"<CR><LF>

    or

     "userlist"<CR><LF>

    where:

    domain
        domain name, optional (can contain wild cards).

    username
        username, optional (can contain wild cards).

    Example:

     "userlist"[TAB]"spacejam.foo"[TAB]"*admin"<CR><LF>

    lists all users of domain 'spacejam.foo' that end with the word 'admin'.

    The result are a RESSTRING. If successful (00100), a formatted matching
    users list follows terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). This is the format of the listing:

     "domain"[TAB]"username"[TAB]"password"[TAB]"usertype"<CR><LF>

    [admin protocol] [top]

  Getting mailproc.tab file

     "usergetmproc"[TAB]"domain"[TAB]"username"<CR><LF>

    where:

    domain
        domain name.

    username
        username.

    Example:

     "usergetmproc"[TAB]"spacejam.foo"[TAB]"admin"<CR><LF>

    gets mailproc.tab file for user 'admin' in domain 'spacejam.foo'.

    The result is a RESSTRING. In successful cases (00100) the mailproc.tab
    file is listed line by line, terminated by a line containing a single
    dot (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Setting mailproc.tab file

     "usersetmproc"[TAB]"domain"[TAB]"username"<CR><LF>

    where:

    domain
        domain name.

    username
        username.

    Example:

     "usersetmproc"[TAB]"spacejam.foo"[TAB]"admin"<CR><LF>

    sets mailproc.tab file for user 'admin' in domain 'spacejam.foo'.

    The result is a RESSTRING. If successful (00101), the client must list
    the mailproc.tab file line by line, ending with a line containing a
    single dot (<CR><LF>.<CR><LF>). If a line of the file begins with a dot,
    another dot must be added at the beginning of the line. If the file has
    zero length the mailproc.tab is deleted. The client then gets another
    RESSTRING indicating the final command result.

    [admin protocol] [top]

  Adding a mailing list user

     "mluseradd"[TAB]"domain"[TAB]"mlusername"[TAB]"mailaddress"[TAB]"perms"<CR><LF>

    or

     "mluseradd"[TAB]"domain"[TAB]"mlusername"[TAB]"mailaddress"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    mlusername
        mailing list username.

    mailaddress
        mail address to add to the mailing list 'mlusername@domain'.

    perms
        user permissions (R or RW or RA). When 'perms' is not specified the
        default is RW.

    The result is a RESSTRING.

    [admin protocol] [top]

  Deleting a mailing list user

     "mluserdel"[TAB]"domain"[TAB]"mlusername"[TAB]"mailaddress"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    mlusername
        mailing list username.

    mailaddress
        mail address to delete from the mailing list 'mlusername@domain'.

    The result is a RESSTRING.

    [admin protocol] [top]

  Listing mailing list users

     "mluserlist"[TAB]"domain"[TAB]"mlusername"<CR><LF>

    where:

    domain
        domain name (must be handled by the server).

    mlusername
        mailing list username.

    The result is a RESSTRING. If successful (00100), a formatted list of
    mailing list users follows terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Adding a domain

     "domainadd"[TAB]"domain"<CR><LF>

    where:

    domain
        domain name to add.

    The result is a RESSTRING.

    [admin protocol] [top]

  Deleting a domain

     "domaindel"[TAB]"domain"<CR><LF>

    where:

    domain
        domain name to delete.

    The result is a RESSTRING. This is not always a safe operation.

    [admin protocol] [top]

  Listing handled domains

     "domainlist"<CR><LF>

    or:

     "domainlist"[TAB]"wildmatch0"[TAB]...[TAB]"wildmatchN"<CR><LF>

    The result is a RESSTRING. The wild match versions simply returns a
    filtered list of domains. If successful (00100), a formatted list of
    handled domains follows, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Adding a domain alias

     "aliasdomainadd"[TAB]"realdomain"[TAB]"aliasdomain"<CR><LF>

    Example:

     "aliasdomainadd"[TAB]"xmailserver.org"[TAB]"xmailserver.com"<CR><LF>

    defines 'xmailserver.com' as an alias of 'xmailserver.org', or:

     "aliasdomainadd"[TAB]"xmailserver.org"[TAB]"*.xmailserver.org"<CR><LF>

    defines all subdomains of 'xmailserver.org' as alises of
    'xmailserver.org'.

    [admin protocol] [top]

  Deleting a domain alias

     "aliasdomaindel"[TAB]"aliasdomain"<CR><LF>

    Example:

     "aliasdomaindel"[TAB]"*.xmailserver.org"<CR><LF>

    removes the '*.xmailserver.org' domain alias.

    [admin protocol] [top]

  Listing alias domains

     "aliasdomainlist"<CR><LF>

    or:

     "aliasdomainlist"[TAB]"wild-dom-match"<CR><LF>

    or:

     "aliasdomainlist"[TAB]"wild-dom-match"[TAB]"wild-adom-match"<CR><LF>

    The result is a RESSTRING. The wild match version simply returns a
    filtered list of alias domains. If successful (00100), a formatted list
    of alias domains follows, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). The output format is:

     "real-domain"[TAB]"alias-domain"<CR><LF>

    [admin protocol] [top]

  Getting custom domain file

     "custdomget"[TAB]"domain"<CR><LF>

    where:

    domain
        domain name.

    Example:

     "custdomget"[TAB]"spacejam.foo"<CR><LF>

    gets the custom domain file for domain 'spacejam.foo'.

    The result is a RESSTRING. If successful (00100), the custom domain file
    is listed line by line terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Setting custom domain file

     "custdomset"[TAB]"domain"<CR><LF>

    where:

    domain
        domain name.

    Example:

     "custdomset"[TAB]"spacejam.foo"<CR><LF>

    sets custom domain file for domain 'spacejam.foo'.

    The result is a RESSTRING. If successful (00101), the client must list
    the custom domain file line by line, ending with a line containing a
    single dot (<CR><LF>.<CR><LF>). If a line of the file begins with a dot,
    another dot must be added at the begin of the line. If the file has zero
    length the custom domain file is deleted. The client then gets another
    RESSTRING indicating the final command result.

    [admin protocol] [top]

  Listing custom domains

     "custdomlist"<CR><LF>

    The result is a RESSTRING. If successful (00100), a formatted list of
    custom domains follows, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Adding a POP3 external link

     "poplnkadd"[TAB]"loc-domain"[TAB]"loc-username"[TAB]"extrn-domain"=>
       [TAB]"extrn-username"[TAB]"extrn-password"[TAB]"authtype"<CR><LF>

    where:

    loc-domain
        local domain name (must be handled by the server).

    loc-username
        local username which receives mails.

    extrn-domain
        external domain.

    extrn-username
        external username.

    extrn-password
        external user password.

    authtype
        authentication method ('CLR' = USER/PASS auth 'APOP' = APOP auth).

    The remote server must support 'APOP' authentication to specify APOP as
    authtype. Using APOP authentication is more secure because clear
    usernames and passwords do not travel on the network; if you're not sure
    about it, specify 'CLR' as authtype.

    The result is a RESSTRING.

    [admin protocol] [top]

  Deleting a POP3 external link

     "poplnkdel"[TAB]"loc-domain"[TAB]"loc-username"[TAB]"extrn-domain"=>
       [TAB]"extrn-username"<CR><LF>

    where:

    loc-domain
        local domain name (must be handled by the server).

    loc-username
        local username which receives mails.

    extrn-domain
        external domain.

    extrn-username
        external username.

    The result is a RESSTRING.

    [admin protocol] [top]

  Listing POP3 external links

     "poplnklist"[TAB]"loc-domain"[TAB]"loc-username"<CR><LF>

    or

     "poplnklist"[TAB]"loc-domain"<CR><LF>

    or

     "poplnklist"<CR><LF>

    The result is a RESSTRING. If successful (00100), a formatted list of
    handled domains follows, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). The format of the listing is:

     "loc-domain"[TAB]"loc-username"[TAB]"extrn-domain"[TAB]"extrn-username"=>
       [TAB]"extrn-password"[TAB]"authtype"[TAB]"on-off"<CR><LF>

    [admin protocol] [top]

  Enabling a POP3 external link

     "poplnkenable"[TAB]"enable"[TAB]"loc-domain"[TAB]"loc-username"=>
       [TAB]"extrn-domain"[TAB]"extrn-username"<CR><LF>

    or

     "poplnkenable"[TAB]"enable"[TAB]"loc-domain"[TAB]"loc-username"<CR><LF>

    where:

    enable
        1 for enabling - 0 for disabling.

    loc-domain
        local domain name.

    loc-username
        local username which receives mails.

    extrn-domain
        external domain.

    extrn-username
        external username.

    In the second format all users, links are affected by the enable
    operation.

    The result is a RESSTRING.

    [admin protocol] [top]

  Listing files

     "filelist"[TAB]"relative-dir-path"[TAB]"match-string"<CR><LF>

    where:

    relative-dir-path
        path relative to MAIL_ROOT path.

    match-string
        wild card match string for file list selection.

    The result is a RESSTRING. If successful (00100), the directory is
    listed line by line, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). The listing format is:

    "filename"[TAB]"filesize"<CR><LF>

    [admin protocol] [top]

  Getting configuration file

     "cfgfileget"[TAB]"relative-file-path"<CR><LF>

    where:

    relative-file-path
        path relative to MAIL_ROOT path.

    Example:

     "cfgfileget"[TAB]"ctrlaccounts.tab"<CR><LF>

    The result is a RESSTRING. If successful (00100), the file is listed
    line by line, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). You CANNOT use this command with indexed files !

    [admin protocol] [top]

  Setting configuration file

     "cfgfileset"[TAB]"relative-file-path"<CR><LF>

    where:

    relative-file-path
        path relative to MAIL_ROOT path.

    Example:

     "cfgfileset"[TAB]"ctrlaccounts.tab"<CR><LF>

    The result is a RESSTRING. IF successful (00101), the client must list
    the configuration file line by line, ending with a line containing a
    single dot (<CR><LF>.<CR><LF>). If a line of the file begins with a dot,
    another dot must be added at the beginning of the line. If the file has
    zero length the configuration file is deleted. The client then gets
    another RESSTRING indicating the final command result. Remember that
    configuration files have a strict syntax and that pushing a incorrect
    one can make XMail not work properly. You CANNOT use this command with
    indexed files!

    [admin protocol] [top]

  Listing frozen messages

     "frozlist"<CR><LF>

    The result is a RESSTRING. If successful (00100), a formatted list of
    frozen messages follows, terminated by a line containing a single dot
    (<CR><LF>.<CR><LF>). The format of the listing is:

     "msgfile"[tab]"lev0"[TAB]"lev1"[TAB]"from"[TAB]"to"[TAB]"time"[TAB]"size"<CR><LF>

    Where:

    msgfile
        message name or id.

    lev0
        queue fs level 0 (first level directory index).

    lev1
        queue fs level 1 (second level directory index).

    from
        message sender.

    to  message destination.

    time
        message time ("YYYY-MM-DD HH:MM:SS").

    size
        message size in bytes.

    [admin protocol] [top]

  Rescheduling frozen message

     "frozsubmit"[TAB]"lev0"[TAB]"lev1"[TAB]"msgfile"<CR><LF>

    where:

    msgfile
        message name or id.

    lev0
        queue fs level 0 (first level directory index).

    lev1
        queue fs level 1 (second level directory index).

    You can get this information from the frozlist command. After a message
    has been successfully rescheduled it is deleted from the frozen fs path.
    The result is a RESSTRING.

    [admin protocol] [top]

  Deleting frozen message

     "frozdel"[TAB]"lev0"[TAB]"lev1"[TAB]"msgfile"<CR><LF>

    where:

    msgfile
        message name or id.

    lev0
        queue fs level 0 (first level directory index).

    lev1
        queue fs level 1 (second level directory index).

    You can get this information from the frozlist command. The result is a
    RESSTRING.

    [admin protocol] [top]

  Getting frozen message log file

     "frozgetlog"[TAB]"lev0"[TAB]"lev1"[TAB]"msgfile"<CR><LF>

    where:

    msgfile
        message name or id.

    lev0
        queue fs level 0 (first level directory index).

    lev1
        queue fs level 1 (second level directory index).

    You can get this information from the frozlist command. The result is a
    RESSTRING. If successful (00100), the frozen message log file follows,
    terminated by a line containing a single dot (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Getting frozen message

     "frozgetmsg"[TAB]"lev0"[TAB]"lev1"[TAB]"msgfile"<CR><LF>

    where:

    msgfile
        message name or id.

    lev0
        queue fs level 0 (first level directory index).

    lev1
        queue fs level 1 (second level directory index).

    You can get this information from the frozlist command. The result is a
    RESSTRING. If successful (00100), the frozen message file follows,
    terminated by a line containing a single dot (<CR><LF>.<CR><LF>).

    [admin protocol] [top]

  Starting a queue flush

     "etrn"[TAB]"email-match0"...<CR><LF>

    where:

    email-match0
        wild card email matching for destination address.

    Example:

     "etrn"  "*@*.mydomain.com"  "your-domain.org"

    starts queueing all messages with a matching destination address.

    [admin protocol] [top]

  Do nothing command

    "noop"<CR><LF>

    The result is a RESSTRING.

    [admin protocol] [top]

  Quit the connection

     "quit"<CR><LF>

    The result is a RESSTRING.

    [admin protocol] [top]

  Do you want...?

    Do you want to build GUI configuration tools using common scripting
    languages (Java, TCL/Tk, etc) and XMail controller protocol? Do you want
    to build Web configuration tools? Please let me know
    <davidel@xmailserver.org>.

    [admin protocol] [top]

XMAIL LOCAL MAILER

    XMail has the ability to deliver locally prepared mail files that if
    finds in the 'spool/local' directory. The format of these files is
    strict:

     mail from:<...>[CR][LF]
     rcpt to:<...>[CR][LF]
     ...
     [CR][LF]
     message text in RFC822 format with [CR][LF] line termination

    All lines must be [CR][LF] terminated, with one mail-from statement, one
    or more rcpt-to statements, an empty line and the message text. Mail
    files must not be created directly inside the '/spool/local' directory
    but instead inside '/spool/temp' directory. When the file is prepared it
    has to be moved into '/spool/local'. The file name format is:

     stime-seqnr.pid.hostname

    where:

    stime
        system time in sec from 01/01/1970.

    seqnr
        sequence number for the current file.

    pid process or thread id.

    hostname
        creator process host name.

    Example:

     97456928-001.7892.home.bogus

    XMail has a number of LMAIL threads that periodically scan the
    '/spool/local' directory watching for locally generated mail files. You
    can tune this number of threads with the '-Ln nthreads' command line
    option. The suggested number ranges from three to seven.

    [top]

CtrlClnt (XMAIL ADMINISTRATION)

    You can use CtrlClnt to send administration commands to XMail. These
    commands are defined in the previous section ("XMAIL ADMIN PROTOCOL").
    The syntax of CtrlClnt is:

     CtrlClnt  [-snuptf]  ...

    where:

    -s server
        set server address.

    -n port
        set server port [6017].

    -u user
        set username.

    -p pass
        set password.

    -t timeout
        set timeout [60].

    -f filename
        set dump filename [stdout].

    With the command and parameters that follow adhering to the command
    syntax, ie:

     CtrlClnt  -s mail.foo.org -u davide.libenzi -p ciao=>
       useradd home.bogus foouser foopasswd U

    executes the command useradd with parameters 'home.bogus foouser
    foopasswd U'.

    CtrlClnt returns 0 if the command is successful and != 0 if not. If the
    command is a query, then the result is printed to stdout.

    [top]

SERVER SHUTDOWN

    [Linux]
        Under Linux, XMail creates a file named XMail.pid in '/var/run' that
        contains the PID of the main XMail thread. By issuing a:

         kill -INT `cat /var/run/XMail.pid`

        a system administrator can initiate the shutdown process (this can
        take several seconds). You can use the supplied 'xmail' startup
        script to start / stop / restart XMail:

         xmail start / stop / restart

    [NT as console service]
        Under NT console service (XMail --debug ...) you can hit Ctrl-C to
        initiate the shutdown process.

    [NT as service]
        Using [Control Panel]->[Services] you can start and stop XMail as
        you wish.

    [All]
        XMail detects a shutdown condition by checking the presence of a
        file named '.shutdown' in its main directory (MAIL_ROOT). You can
        initiate XMail shutdown process by creating (or copying) a file with
        that name into MAIL_ROOT.

    [top]

MkUsers

    This command line utility enables you to create the user accounts
    structure by giving it a formatted list of users parameters (or a
    formatted text file). The syntax of the list (or file) is:

     domain;username;password;real-name;homepage[NEWLINE]

    where a line whose first character is '#' is treated as a comment. This
    utility can also be used to create a random number users (useful for me
    to test server performance).

    These are MkUsers command line parameters:

    -a numusers
        number of users to create in auto-mode.

    -d domain
        domain name in auto-mode.

    -f inputFile
        input file name {stdin}.

    -u username
        radix user name in auto-mode.

    -r rootdir
        mail root path {./}.

    -s mboxsize
        mailbox maximum size {10000}.

    -i useridbase
        base user id {1};

    -m  create Maildir boxes.

    -h  show this message.

    MkUsers creates, under the specified root directory, the given
    structure:

     rootdir            <dir>
       mailusers.tab    <file>
       domains          <dir>
         domainXXX      <dir>
           userXXX      <dir>
             user.tab   <file>
             mailbox    <dir>
           ...
        ...

    for the mailbox structure, while:

     rootdir            <dir>
       mailusers.tab    <file>
       domains          <dir>
         domainXXX      <dir>
           userXXX      <dir>
             user.tab   <file>
             Maildir    <dir>
               tmp      <dir>
               new      <dir>
               cur      <dir>
           ...
         ...

    for the Maildir structure.

    If the file 'mailusers.tab' already exist in the mail root path, MkUsers
    exits without overwriting the existing copy. This protect you from
    accidentally overwriting your file when playing inside the real
    MAIL_ROOT directory. If you want to setup the root directory (-r ...) as
    MAIL_ROOT, you must delete by hand the existing file (you must know what
    you're doing). If you setup the root directory (-r ...) as MAIL_ROOT you
    MUST have XMail stopped before running MkUsers. Existing files and
    directories are not overwritten by MkUsers so you can keep your users db
    in the formatted text file (or generate it by a database dump for
    example) and run MkUsers to create the structure. Remember that you have
    to add new domains in the 'domains.tab' file by hand. MkUsers is
    intended as a bulk-mode utility, not to create single user; for this
    CtrlClnt (or other GUI/Web configuration utilities) is better suited.

    [top]

sendmail

    When building XMail, an executable called 'sendmail' is created. This is
    a replacement of the sendmail program used mostly on Unix systems; it
    uses the local mail delivery of XMail to send email generated onto the
    server machine. These sendmail options are supported (other options are
    simply ignored):

    -f{mail from}
        Set the sender of the email.

    -F{ext mail from}
        Set the extended sender of the email.

    -t  Extract recipients from the 'To:'/'Cc:'/'Bcc:' header tags.

    The syntax is:

     sendmail [-t] [-f...] [-F...] [--input-file fname] [--xinput-file fname]=>
       [--rcpt-file fname] [--] recipient ...

    The message content is read from the standard input and must be RFC
    compliant.

    The following parameters are XMail extensions meant to be used with
    mailing lists managers (using sendmail as a mail list exploder):

    --input-file fname
        take the message from the specified file instead from stdin (RFC
        format).

    --xinput-file fname
        take the message from the specified file instead from stdin (XMail
        format).

    --rcpt-file fname
        add recipients listed inside the specified file (list exploder).

    To be RFC compliant means that the message MUST have the format:

     [Headers]
     NewLine
     Body

    Suppose you have your message in the file 'msg.txt', you're
    'xmailuser@smartdomain', and you want to send the message to
    'user1@dom1' and 'user2@dom2'. The syntax is:

     sendmail -fxmailuser@smartdomain user1@dom1 user2@dom2 < msg.txt

    or

     sendmail -fxmailuser@smartdomain --input-file msg.txt user1@dom1 user2@dom2

    [top]

MISCELLANEOUS

    1.  To handle multiple POP3 domains, the server makes a reverse lookup
        of the IP address upon which it receives the connection. Suppose the
        reverse lookup results in 'xxxx.yyyy.zzzz'. XMail checks if
        'xxxx.yyyy.zzzz' is handled, then it checks 'yyyy.zzzz', and then
        'zzzz'. The first resolved (in the given order) is the POP3 domain.
        To avoid the above behavior, it's sufficient that the POP3 client
        supply the entire email address as POP3 login username:

         foo@foodomain.net   ==> foo@foodomain.net

        and not:

         foo@foodomain.net   ==> foo

        This enables XMail to handle multiple domains in cases where more
        nic-names are mapped over a single IP address.

        To run finger queries you must specify:

         foo@foodomain.net@foodomain.net

        or as general rule:

         username@pop3domain@hostname

        You can use the optional configuration variable 'POP3Domain' (see
        "SERVER.TAB VARIABLES" above) to set the default domain for POP3
        clients connections. This means that users of 'POP3Domain' can use
        only the name part of their email address as POP3 login, while users
        of other hosted domains must use their entire email as POP3 login.

    2.  Important!

        *       'REMEMBER TO REMOVE THE EXAMPLE ACCOUNT FROM
                CTRLACCOUNTS.TAB FILE!'

        *       Use ctrl.ipmap.tab to restrict CTRL server access.

        *       Use a long password (mixed upper/lower case with digits) for
                ctrlaccounts.tab.

    3.  The main cause of bugs with XMail is due a bad line termination of
        configuration files, so check that these files being correctly line
        terminated for your OS. Linux uses the standard <CR> while M$ uses
        <CR><LF>.

    4.  If you get a bind error in Linux,you must comment pop3, smtp and
        finger entries in your /etc/inetd.conf.

    5.  Remember to compile the file CTRL.IPMAP.TAB to restrict the access
        to the IPs you use to remote administer XMail server.

    6.  If you have an heavily loaded server remember to setup the best
        number of XMAIL threads by specifying the '-Qn nthreads' option (you
        must do some tentatives to find the best value for your needs). Also
        you can limit the number of SMTP, POP3 and CTRL service threads by
        specifying the options '-SX maxthreads', '-PX maxthreads' and '-CX
        maxthreads'.

    7.  If you have enabled logging, remember to setup the '-Mr hours'
        option depending on the traffic you get in your server. This avoids
        XMail having to work with very large log files and can improve
        server performance.

    8.  If you are unable to start XMail (even if you followed this
        document's instructions), check the MailRoot directory with the one
        listed above. More than one unzipper does not restore empty
        directories by default.

    Please report XMail errors and errors in this document. If you
    successfully build and run XMail please let me know at
    davidel@xmailserver.org, I don't want money ;)

    [top]

KNOWN BUGS

    Version 0.1 (Alpha-1):

    0.1-001 Linux (FIXED)
        SMail threads don't wake up upon a release semaphore by SMTP
        threads.

    0.27-001 Windows
        Using XMail inside a net that use MS Proxy Server 2.0 cause XMail to
        fail in sending UDP packets for DNS MX queries (due to a bug of
        WS2_32.DLL linked to MS Proxy 2.0). I don't know if more recent
        versions of MS Proxy fixes this bug. To makes XMail work in such
        environment you can use 'DefaultSMTPGateways' option in 'SERVER.TAB'
        (see "SERVER.TAB VARIABLES" above) to use smart SMTP hosts. Or
        better still, strip away MS Proxy server and setup a cheap PC
        running Linux + IP-Masquerading that cost exactly 0.0 $ and works
        great. Or use 'SmartDNSHost' configuration to redirect recursion
        queries to a DNS smart host that support TCP and recursion.

    [top]

THANKS

    My mother Adelisa, for giving me the light.

    My cat Grace, for her patience waiting for food while I'm coding.

    All of the free source community, for giving me code and knowledge.

    [top]

