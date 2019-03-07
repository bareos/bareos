#!/bin/bash
OSC=osc

# update project config and create project if it does not exist:

cat prj.xml.in  | sed 's#@BASEPROJECT@#jenkins#g' | sed 's#@BRANCH@#master#' | $OSC meta prj jenkins:master -F -

# set project config
$OSC meta prjconf jenkins:master -f  prjconf


# for every package, create it via setting the package meta info:

for pkg in `cat packages`; do
cat ${pkg}/_meta.in  | sed 's#@BASEPROJECT@#jenkins#g' | sed 's#@BRANCH@#master#' | $OSC meta pkg jenkins:master ${pkg} -F -
done
# for every package, add files that belong to this package, especially service files

for pkg in `cat packages`; do
  $OSC co jenkins:master/${pkg};
  cp -v "${pkg}/*" jenkins:master/${pkg}/;
  cd jenkins:master/${pkg}/
  rm _meta.in
  osc add *
  osc commit -m "import"
  cd -
done
rm -Rvf jenkins:master/${pkg}/
