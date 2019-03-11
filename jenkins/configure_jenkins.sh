#!/bin/bash

OBS_SERVER="http://obs2018"

# Variables to replace in xml
#GIT_BRANCH   name of the current branch, e.g. master, bareos-18.2
#REPOURL      url of the packages on obs server, e.g. http://obs2018.dass-it/bareos:/master/
#DISTRELEASES list of distreleases as xml string list like <string>CentOS-6-x86_64</string><string>CentOS-7-x86_64</string>

DISTRELEASES="CentOS-6-x86_64 CentOS-7-x86_64 Fedora-29-x86_64"

DISTRELEASES_XML=""
for DR in ${DISTRELEASES}; do
  DISTRELEASES_XML="${DISTRELEASES_XML}<string>${DR}</string> "
done

cat bareos.xml.in |\
  sed "s#@GIT_BRANCH@#${GIT_BRANCH}#g" |\
  sed "s#@DISTRELEASES_XML@#${DISTRELEASES_XML}#g" |\
  sed "s#@REPOURL@#${OBS_SERVER}/bareos:/${GIT_BRANCH}/#"

