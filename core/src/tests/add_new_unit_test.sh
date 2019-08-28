#!/bin/sh

# create and add an empty unittest file

if [ $# -ne 1 ]; then
  echo "usage: $0 <testname>"
  exit 1
fi
testname="$1"

if [ -f "$testname.cc" ]; then
  echo "Filename for test already exists: $testname.cc"
  exit 1
fi

if grep -qw "$testname.cc" CMakeLists.txt; then
  echo "$testname.cc already exists in CMakeLists.txt"
  exit 1
fi

if ! sed -e "s/@year@/$(date +%Y)/" test_template.cc.in > "$testname.cc"; then
  echo "File creation failed: $testname.cc"
  exit 1
fi

if ! sed -e "s/@testname@/$testname/g" test_cmakelists_template.txt.in >> CMakeLists.txt; then
  echo "Adding to CMakeLists.txt failed"
  exit 1
fi

echo "Don't forget to add your new file to the git index with"
echo "     git add $testname.cc"

exit 0

