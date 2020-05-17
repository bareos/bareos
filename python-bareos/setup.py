#!/usr/bin/python

import os
import re
from setuptools import find_packages, setup


def get_version():
    base_dir = os.path.abspath(os.path.dirname(__file__))

    try:
        with open(
            os.path.join(base_dir, "bareos", "VERSION.txt")
        ) as version_file:
            # read version
            # and adapt it according to
            # https://www.python.org/dev/peps/pep-0440/.
            fullversion = version_file.read().strip()
            __version__ = re.compile(r'~pre([0-9]+).*').sub(r'.dev\1', fullversion)
    except IOError:
        # Fallback version.
        # First protocol implemented
        # has been introduced with this version.
        __version__ = "18.2.5"

    return __version__


setup(
    name="python-bareos",
    version=get_version(),
    license="AGPLv3",
    author="Bareos Team",
    author_email="packager@bareos.com",
    packages=find_packages(),
    scripts=["bin/bconsole.py", "bin/bconsole-json.py", "bin/bareos-fd-connect.py"],
    package_data={"bareos": ["VERSION.txt"]},
    url="https://github.com/bareos/bareos/",
    # What does your project relate to?
    keywords="bareos",
    description="Client library and tools for Bareos console access.",
    long_description=open("README.rst").read(),
    long_description_content_type='text/x-rst',
    # Python 2.6 is used by RHEL/Centos 6.
    # When RHEL/Centos 6 is no longer supported (End of 2020),
    # Python 2.6 will no longer be supported by python-bareos.
    python_requires=">=2.6",
    extras_require={"TLS-PSK": ["sslpsk"]},
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "License :: OSI Approved :: GNU Affero General Public License v3",
        "Operating System :: OS Independent",
        "Programming Language :: Python",
        "Topic :: System :: Archiving :: Backup",
    ],
)
