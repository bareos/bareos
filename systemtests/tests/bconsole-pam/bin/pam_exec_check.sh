#!/bin/sh

# auth optional pam_exec.so expose_authtok debug log=/tmp/pam.log /usr/bin/sc_pam_wlan.sh

# called by:
# auth    optional     pam_exec.so expose_authtok /usr/bin/sc_pam_wlan.sh

# pam_exec options:
#   debug
#   expose_authtok
#     During authentication the calling command can read the password from stdin(3).
#     (PAM_TYPE=auth only)
#   log=file
#     The output of the command is appended to file
#   type=type
#     Only run the command if the module type matches the given type.
#   stdout
#     Per default the output of the executed command is written to /dev/null. 
#     With this option, the stdout output of the executed command is redirected 
#     to the calling application. 
#     It's in the responsibility of this application what happens with the output. 
#     The log option is ignored.
#  quiet
#    Per default pam_exec.so will echo the exit status of the external command 
#    if it fails. Specifying this option will suppress the message.
#  seteuid
#    Per default pam_exec.so will execute the external command 
#    with the real user ID of the calling process. 
#    Specifying this option means the command is run with the effective user ID.

# PAM_TYPE:
#   "auth"
#   ...
#   "open_session"
#   "close_session"

RC_OK=0
RC_SKIP=1
RC_NOK=2

echo "$0"
#echo "current user: $USER ($UID)"

if [ "$PAM_TYPE" != "auth" ]; then
    echo "only pam type auth supported, not $PAM_TYPE"
    exit $RC_SKIP
fi

echo "PAM settings:"
echo "User:             $PAM_USER"
echo "Ruser:            $PAM_RUSER"
echo "Rhost:            $PAM_RHOST"
echo "Service:          $PAM_SERVICE"
echo "TTY:              $PAM_TTY"

USERNAME="$PAM_USER"
# This does not work in PAM environment
# if [ -z "$PAM_USER" ]; then
#   read -p "PE Login: " USERNAME
# fi


read -p "PE Passwort: " PASSWORD


if [ "$USERNAME" = "$PASSWORD" ]; then
  echo "grant access for $USERNAME"
  RC=$RC_OK
else
  echo "deny access for $USERNAME"
  RC=$RC_NOK
fi

exit $RC

