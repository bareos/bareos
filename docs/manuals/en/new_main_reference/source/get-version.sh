#!/bin/bash

VERSION_H=../../../../../core/src/include/version.h
if [ -f ${VERSION_H}  ]; then
  grep "^#define VERSION" ${VERSION_H} | cut -b 17-  | sed 's/"//g'
else
  cd ../../../../ && dpkg-parsechangelog  | grep Version | cut -b 10-
fi
