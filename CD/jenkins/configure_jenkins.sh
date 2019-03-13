#!/bin/bash

OBS_SERVER="http://obs2018.dass-it"

BASEPROJECT_NAME=jenkins
ORIGINAL_BRANCH=${GIT_BRANCH}
SUBPROJECT_NAME=${GIT_BRANCH}
DEV=$(echo ${GIT_BRANCH} | cut -d '/' -f1)
if [ "${DEV}" = "dev" ]; then
  VERSION_OR_VERSIONPREFIX="version"
  DEVELOPER=$(echo ${GIT_BRANCH} | cut -d '/' -f2)
  ORIGINAL_BRANCH=$(echo ${GIT_BRANCH} | cut -d '/' -f3) #OBS common project?
  DEVELOPER_BRANCH=$(echo ${GIT_BRANCH} | cut -d '/' -f4)
  SUBPROJECT_NAME="${DEVELOPER}:${ORIGINAL_BRANCH}:${DEVELOPER_BRANCH}"
fi

JENKINS_JOB_NAME=$(echo "${BASEPROJECT_NAME}-${SUBPROJECT_NAME}" | tr ':' '-')


# Variables to replace in xml
#GIT_BRANCH   name of the current branch, e.g. master, bareos-18.2
#REPOURL      url of the packages on obs server, e.g. http://obs2018.dass-it/bareos:/master/
#DISTRELEASES list of distreleases as xml string list like <string>CentOS-6-x86_64</string><string>CentOS-7-x86_64</string>

#DISTRELEASES="CentOS-7-x86_64"

/usr/local/bin/jenkins-cli.sh delete-job "${JENKINS_JOB_NAME}"


DISTRELEASES_XML=$(./create_jenkins_project_from_yaml.py)


cat bareos.xml.in |\
  sed "s#@GIT_BRANCH@#${GIT_BRANCH}#g" |\
  sed "s#@DISTRELEASES_XML@#${DISTRELEASES_XML}#g" |\
  sed "s#@REPOURL@#${OBS_SERVER}/${BASEPROJECT_NAME}:/${GIT_BRANCH}/#"  |\
  /usr/local/bin/jenkins-cli.sh create-job "${JENKINS_JOB_NAME}"


# run the job, and wait until complete and show output
/usr/local/bin/jenkins-cli.sh build "${JENKINS_JOB_NAME}" -s -v


