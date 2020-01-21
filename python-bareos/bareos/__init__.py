# __all__ = [ "bconsole" ]

import os.path

try:
    base_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    base_dir = None
else:
    try:
        with open(os.path.join(base_dir, "VERSION.txt")) as version_file:
            __version__ = version_file.read().strip()
    except IOError:
        # Fallback version.
        # First protocol implemented
        # has been introduced with this version.
        __version__ = "18.2.5"

from bareos.exceptions import *
from bareos.util.password import Password
import bareos.util
import bareos.bsock
