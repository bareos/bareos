MATRIX_FILE=../matrix.yml

#   <repository name="xUbuntu_18.04">
#     <path project="openSUSE.org:Ubuntu:18.04" repository="universe"/>
#     <path project="bareos:@BRANCH@:common" repository="xUbuntu_18.04"/>
#     <arch>x86_64</arch>
#   </repository>

# get all indices
# yq  '.OS.xUbuntu."16.04".PROJECTPATH | (keys)' ../matrix.yml
# [
#   0,
#   1,
#   2,
#   3
# ]

# get individual listentry
# yq  '.OS.xUbuntu."16.04".PROJECTPATH[0] | (keys)' ../matrix.yml
# [
#   "PATH",
#   "REPOSITORY"
# ]



DISTS=$(yq '.OS | (keys)' ${MATRIX_FILE} -c | tr '[],\"' ' ')

for dist in ${DISTS}; do
  for version in $(yq ".OS.${dist} | (keys)" ${MATRIX_FILE} -c | tr '[],\"' ' '); do
    echo "   <repository name=\"${dist}_${version}\""
    for projectpathindex in $(yq -c ".OS.${dist}.\"${version}\".PROJECTPATH | (keys)" ${MATRIX_FILE}  | tr '[],\"' ' '); do
      projectpath=$(yq -c ".OS.${dist}.\"${version}\".PROJECTPATH[${projectpathindex}].PATH" ${MATRIX_FILE}  | tr '[],' ' ' | tr -d '\"');
      projectrepo=$(yq -c ".OS.${dist}.\"${version}\".PROJECTPATH[${projectpathindex}].REPOSITORY" ${MATRIX_FILE}  | tr '[],' ' ' | tr -d '\"' );
      echo "      <path project=\"${projectpath}\" repository=\"${projectrepo}\"/>"
    done
    for arch in $(yq -c ".OS.${dist}.\"${version}\".ARCH " ${MATRIX_FILE}  | tr '[],\"' '  '); do
      echo "      <arch>${arch}</arch>"
    done
  done
echo "   </repository>"
done
