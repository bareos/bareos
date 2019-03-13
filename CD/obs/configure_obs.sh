#!/bin/bash
OSC=osc

GIT_URL="git://gitmirror/pstorz-bareos"


if [ "${GIT_BRANCH}" = "master" ]; then
  VERSION_OR_VERSIONPREFIX="versionprefix"
else
  VERSION_OR_VERSIONPREFIX="version"
fi

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

export ORIGINAL_BRANCH



# get version from version.h
BAREOS_VERSION_NUMBER=`cat ../../core/src/include/version.h | \
                grep "#define VERSION" | \
                cut -b 17- | \
                sed 's/\"//g'`

rm -Rvf ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}
# update project config and create project if it does not exist:

./create_obs_project_from_yaml.py | \
  sed 's#@BASEPROJECT@#jenkins#g' | \
  sed "s#@SUBPROJECTNAME@#${SUBPROJECT_NAME}#g" | \
  sed "s#@ORIGINALBRANCH@#${ORIGINAL_BRANCH}#g" |\
  $OSC meta prj ${BASEPROJECT_NAME}:${SUBPROJECT_NAME} -F -

# set project config
$OSC meta prjconf ${BASEPROJECT_NAME}:${SUBPROJECT_NAME} -F  prjconf


# for every package, create it via setting the package meta info:

for pkg in $(cat packages); do
cat "${pkg}"/_meta.in  | \
  sed 's#@BASEPROJECT@#jenkins#g' | \
  sed "s#@SUBPROJECTNAME@#${SUBPROJECT_NAME}#g" | \
  sed "s#@ORIGINALBRANCH@#${ORIGINAL_BRANCH}#g" | \
  $OSC meta pkg ${BASEPROJECT_NAME}:${SUBPROJECT_NAME} "${pkg}" -F -
done

# for every package, add files that belong to this package, especially service files

for pkg in $(cat packages); do
  $OSC co "${BASEPROJECT_NAME}:${SUBPROJECT_NAME}/${pkg}";
  if [ -f "${pkg}/_service.in" ]; then
  cat "${pkg}/_service.in" | \
    sed "s#@VERSION_OR_VERSIONPREFIX@#${VERSION_OR_VERSIONPREFIX}#g" | \
    sed "s#@VERSION_NUMBER@#${BAREOS_VERSION_NUMBER}#g" | \
    sed "s#@SUBPROJECTNAME@#${SUBPROJECT_NAME}#g" | \
    sed "s#@GIT_URL@#${GIT_URL}#g" | \
    sed "s#@REVISION@#${GIT_COMMIT}#"  > "${pkg}"/_service
  fi
  ls "${pkg}"
  cp -v "${pkg}"/* ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}/"${pkg}"/
  cd ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}/"${pkg}"/ || exit
  rm _meta.in
  rm _service.in
  osc add ./*
  # run with timeout so dont wait for service to complete
  timeout 5 osc commit -m "import"
  cd - || exit
done

#rm -Rvf ${BASEPROJECT_NAME}:${SUBPROJECT_NAME}


