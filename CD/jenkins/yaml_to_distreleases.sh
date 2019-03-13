#!/bin/bash
MATRIX_FILE=../matrix.yml

DISTS=$(yq '.OS | (keys)' ${MATRIX_FILE} -c | tr '[],\"' '  ')

for dist in ${DISTS}; do
  for version in $(yq ".OS.${dist} | (keys)" ${MATRIX_FILE} -c | tr '[],\"' '  '); do
    for arch in $(yq -c ".OS.${dist}.\"${version}\".ARCH " ${MATRIX_FILE}  | tr '[],\"' '  '); do
      echo "${dist}-${version}-${arch}"
    done
  done
done
