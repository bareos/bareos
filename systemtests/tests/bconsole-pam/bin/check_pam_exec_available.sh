#!/bin/sh

#
# Verify that our test pam configuration works.
# It uses
# * pam_wrapper  to redirect PAM to our test environemnt, using a specific service file
# * pamtester    to test PAM without the need to run Bareos
# * pam_exec.so  is defined in the bareos PAM service file.
#                It is configured to accept all logins where USERNAME = PASSWORD.
#

set -e
set -u

export PAM_WRAPPER=1
export PAM_WRAPPER_SERVICE_DIR=etc/pam.d/bareos

if ! [ -e "${PAM_WRAPPER_SERVICE_DIR}" ]; then
    echo "PAM service file ${PAM_WRAPPER_SERVICE_DIR} not found"
    exit 1
fi

# DEBUG
#export PAM_WRAPPER_DEBUGLEVEL=4

# PAM_WRAPPER creates extra environments in /tmp/pam.*/

# PAM_WRAPPER_LIBRARIES will be set my cmake
USERNAME="user"
PASSWORD="user"
echo "$PASSWORD" | LD_PRELOAD=${PAM_WRAPPER_LIBRARIES} pamtester bareos "$USERNAME" authenticate

exit $?
