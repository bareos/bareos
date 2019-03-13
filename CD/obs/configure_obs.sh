#!/bin/bash
OSC=osc

GIT_URL="git://gitmirror/pstorz-bareos"


if [ "${GIT_BRANCH}" = "master" ]; then
  VERSION_OR_VERSIONPREFIX="versionprefix"
else
  VERSION_OR_VERSIONPREFIX="version"
fi

# get version from version.h
BAREOS_VERSION_NUMBER=`cat ../../core/src/include/version.h | \
                grep "#define VERSION" | \
                cut -b 17- | \
                sed 's/\"//g'`

rm -Rvf jenkins:${GIT_BRANCH}
# update project config and create project if it does not exist:

#cat prj.xml.in  | sed 's#@BASEPROJECT@#jenkins#g' | sed "s#@BRANCH@#${GIT_BRANCH}#" | $OSC meta prj jenkins:${GIT_BRANCH} -F -

./create_obs_project_from_yaml.py | sed 's#@BASEPROJECT@#jenkins#g' | sed "s#@BRANCH@#${GIT_BRANCH}#" | $OSC meta prj jenkins:${GIT_BRANCH} -F -

# set project config
$OSC meta prjconf jenkins:${GIT_BRANCH} -F  prjconf


# for every package, create it via setting the package meta info:

for pkg in $(cat packages); do
cat "${pkg}"/_meta.in  | sed 's#@BASEPROJECT@#jenkins#g' | sed "s#@BRANCH@#${GIT_BRANCH}#" | $OSC meta pkg jenkins:${GIT_BRANCH} "${pkg}" -F -
done

# for every package, add files that belong to this package, especially service files

for pkg in $(cat packages); do
  $OSC co "jenkins:${GIT_BRANCH}/${pkg}";
  cat "${pkg}"/_service.in  |\
    sed "s#@VERSION_OR_VERSIONPREFIX@#${VERSION_OR_VERSIONPREFIX}#g" |\
    sed "s#@VERSION_NUMBER@#${BAREOS_VERSION_NUMBER}#g" |\
    sed "s#@GIT_URL@#${GIT_URL}#g" |\
    sed "s#@REVISION@#${GIT_COMMIT}#"  > "${pkg}"/_service
  ls "${pkg}"
  cp -v "${pkg}"/* jenkins:${GIT_BRANCH}/"${pkg}"/

  cd jenkins:${GIT_BRANCH}/"${pkg}"/ || exit
  rm _meta.in
  rm _service.in
  osc add ./*
  # run with timeout so dont wait for service to complete
  timeout 5 osc commit -m "import"
  cd - || exit
done
rm -Rvf jenkins:${GIT_BRANCH}


