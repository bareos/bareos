#!/bin/bash
IFS='
'

exec >capi_1.inc

echo "/* C API functions */"
NUM=0
for i in $(cat bareossd_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
echo "
/* static $i */
#define Bareossd_${funcname}_NUM $NUM
#define Bareossd_${funcname}_RETURN $retval
#define Bareossd_${funcname}_PROTO (${prot})"

((NUM=NUM+1))
done
echo "
/*Total Number of C API function pointers */
#define Bareossd_API_pointers $NUM"

exec >capi_2.inc

NUM=0
for i in $(cat bareossd_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo "#define Bareossd_${funcname} (*(Bareossd_${funcname}_RETURN(*)Bareossd_${funcname}_PROTO) Bareossd_API[Bareossd_${funcname}_NUM])
"
((NUM=NUM+1))
done

exec >capi_3.inc

NUM=0
for i in $(cat bareossd_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo " Bareossd_API[Bareossd_${funcname}_NUM] = (void*)${funcname};"
((NUM=NUM+1))
done
