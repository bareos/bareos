import sys
import os
from distutils import sysconfig

print 'PYTHON_INCDIR="-I%s"' % \
      ( sysconfig.get_config_var('INCLUDEPY'), )

libdir=sysconfig.get_config_var('LIBPL')
filename = sysconfig.get_config_var('LDLIBRARY');
lib = os.path.splitext(filename)[0];
if lib[0:3] == 'lib':
    lib = lib[3:]

print 'PYTHON_LIBS="-L%s -l%s"' % ( libdir, lib )
