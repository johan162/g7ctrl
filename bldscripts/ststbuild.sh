#! /bin/sh
# Build the server and install in a local system test directory "systest" and setup a suitable
# config file based on the argument to this script. 
# $Id

smtp_srv=
smtp_uid=
smtp_pwd=
smtp_from=
smtp_to=
api_key=

while getopts "s:u:p:f:t:k:" o; do  
   case "$o" in
    s)      smtp_srv="$OPTARG"
            ;;
    u)      smtp_uid="$OPTARG"
    	    ;;
    p)      smtp_pwd="$OPTARG"
	    ;;
    f)      smtp_from="$OPTARG"
	    ;;
    t)      smtp_to="$OPTARG"
	    ;;    
    k)      api_key="$OPTARG"
	    ;;    
    [?])    printf >&2 "Usage; $0 -s smtp_server -u smtp_user -p smtp_password -f sender_from -t sender_to -k API_key\n"
            exit 1
            ;;
    esac
done

if test -z "$smtp_srv" -o -z "$smtp_uid" -o -z "$smtp_pwd" -o -z "$smtp_from" -o -z "$smtp_to"; then
    printf >&2 "Usage; $0 -s smtp_server -u smtp_user -p smtp_password -f sender_from -t sender_to\n"
    exit 1
fi 

rm -rf systest
mkdir -p systest/var/run
mkdir -p systest/var/lib

autoreconf && ./configure --disable-pie --enable-stacktrace && make clean &&  make CFLAGS="-O0 -g" -j8
make DESTDIR=`pwd`/systest install

cwd=$(pwd  | sed 's/\//\\\//g') && cu=$(whoami) && \
sed -e "s/#run_as_user=g7ctrl/run_as_user=$cu/" \
-e "s/#verbose_log=1/verbose_log=3/" \
-e "s/#include_minimap=no/include_minimap=yes/" \
-e "s/#use_address_lookup=no/use_address_lookup=yes/" \
-e "s/#enable_mail=no/enable_mail=yes/" \
-e "s/#sendmail_address=/sendmail_address=$smtp_to/" \
-e "s/#daemon_email_from=/daemon_email_from=$smtp_from/" \
-e "s/#smtp_use=no/smtp_use=yes/" \
-e "s/#use_short_devid=no/use_short_devid=yes/" \
-e "s/#smtp_server=/smtp_server=$smtp_srv/" \
-e "s/#smtp_user=/smtp_user=$smtp_uid/" \
-e "s/#smtp_pwd=/smtp_pwd=$smtp_pwd/" \
-e "s/#google_api_key=/google_api_key=$api_key/" \
systest/etc/g7ctrl/g7ctrl.conf.template > systest/g7ctrl.conf

