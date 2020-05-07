#!/bin/bash
IFS='
'

exec >capi_1.inc

echo "/* C API functions */"
NUM=0
for i in $(cat bareosfd_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
echo "
/* static $i */
#define Bareosfd_${funcname}_NUM $NUM
#define Bareosfd_${funcname}_RETURN $retval
#define Bareosfd_${funcname}_PROTO (${prot})"

((NUM=NUM+1))
done
echo "
/*Total Number of C API function pointers */
#define Bareosfd_API_pointers $NUM"

exec >capi_2.inc

NUM=0
for i in $(cat bareosfd_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo "#define Bareosfd_${funcname} (*(Bareosfd_${funcname}_RETURN(*)Bareosfd_${funcname}_PROTO) Bareosfd_API[Bareosfd_${funcname}_NUM])
"
((NUM=NUM+1))
done

exec >capi_3.inc

NUM=0
for i in $(cat bareosfd_api_funcs.txt); do
  retval=$(echo $i | sed 's/ .*//g')
  funcname=$(echo $i | cut -b 5- | sed s/\(.*//g)
  prot=$(echo $i | sed s/.*\(//g | sed 's/);//g')
  echo " Bareosfd_API[Bareosfd_${funcname}_NUM] = (void*)${funcname};"
((NUM=NUM+1))
done
