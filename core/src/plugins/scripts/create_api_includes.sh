#!/bin/bash
IFS='
'

PREFIX=$1
if [ -z "$PREFIX" ]; then
  echo "API prefix required. Bareosfd, Bareossd or Bareosdir"
  exit 1
fi

exec >capi_1.inc

echo "/* C API functions */"
NUM=0
for i in $(cat api_definition.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
echo "
/* static $i */
#define ${PREFIX}_${funcname}_NUM $NUM
#define ${PREFIX}_${funcname}_RETURN $retval
#define ${PREFIX}_${funcname}_PROTO (${prot})"

((NUM=NUM+1))
done
echo "
/*Total Number of C API function pointers */
#define ${PREFIX}_API_pointers $NUM"

exec >capi_2.inc

NUM=0
for i in $(cat api_definition.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo "#define ${PREFIX}_${funcname} (*(${PREFIX}_${funcname}_RETURN(*)${PREFIX}_${funcname}_PROTO) ${PREFIX}_API[${PREFIX}_${funcname}_NUM])
"
((NUM=NUM+1))
done

exec >capi_3.inc

NUM=0
for i in $(cat api_definition.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo " ${PREFIX}_API[${PREFIX}_${funcname}_NUM] = (void*)${funcname};"
((NUM=NUM+1))
done
