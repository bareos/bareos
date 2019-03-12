
DISTS=$(yq '.OS | (keys)' matrix.yml -c | tr '[],\"' '  ')

for dist in ${DISTS}; do
  for version in $(yq ".OS.${dist} | (keys)" matrix.yml -c | tr '[],\"' '  '); do
    for arch in $(yq -c ".OS.${dist}.\"${version}\"" matrix.yml | tr '[],\"' '  '); do
      echo "${dist}:${version}:${arch}"
    done
  done
done
