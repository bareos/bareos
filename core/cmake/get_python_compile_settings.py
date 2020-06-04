import sys
import sysconfig
for var in ("CC","CFLAGS","CCSHARED","INCLUDEPY","BLDSHARED","LDFLAGS","EXT_SUFFIX"):
    print ("set(Python{}_{} '{}')" . format(sys.version_info.major, var, sysconfig.get_config_var(var)))
