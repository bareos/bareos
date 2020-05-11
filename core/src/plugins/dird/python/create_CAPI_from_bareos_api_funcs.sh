#!/bin/bash
IFS='
'

exec >capi_1.inc

echo "/* C API functions */"
NUM=0
for i in $(cat bareosdir_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
echo "
/* static $i */
#define Bareosdir_${funcname}_NUM $NUM
#define Bareosdir_${funcname}_RETURN $retval
#define Bareosdir_${funcname}_PROTO (${prot})"

((NUM=NUM+1))
done
echo "
/*Total Number of C API function pointers */
#define Bareosdir_API_pointers $NUM"

exec >capi_2.inc

NUM=0
for i in $(cat bareosdir_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo "#define Bareosdir_${funcname} (*(Bareosdir_${funcname}_RETURN(*)Bareosdir_${funcname}_PROTO) Bareosdir_API[Bareosdir_${funcname}_NUM])
"
((NUM=NUM+1))
done

exec >capi_3.inc

NUM=0
for i in $(cat bareosdir_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo " Bareosdir_API[Bareosdir_${funcname}_NUM] = (void*)${funcname};"
((NUM=NUM+1))
done
