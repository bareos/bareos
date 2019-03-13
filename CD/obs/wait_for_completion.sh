#!/bin/bash


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

OSC="osc -A https://obs2018.dass-it"

$OSC prjresults ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}

date
$OSC prjresults ${BASEPROJECT_NAME}:${SUBPROJECT_NAME} --watch --xml
date

$OSC prjresults ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}

# rebuild failed jobs
$OSC rebuild -f ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}

date
$OSC prjresults ${BASEPROJECT_NAME}:${SUBPROJECT_NAME} --watch --xml
date

# rebuild failed jobs
$OSC rebuild -f ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}

date
$OSC prjresults ${BASEPROJECT_NAME}:${SUBPROJECT_NAME} --watch --xml
date
