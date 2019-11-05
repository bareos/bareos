#!/usr/bin/python

import os
from setuptools import find_packages, setup


def get_version():
    base_dir = os.path.abspath(os.path.dirname(__file__))

    try:
        with open(os.path.join(base_dir, 'python-bareos', 'VERSION.txt')) as version_file:
            __version__ = version_file.read().strip()
    except IOError:
        # Fallback version.
        # First protocol implemented
        # has been introduced with this version.
        __version__ = '18.2.5'

    return __version__


setup(
    name='python-bareos',
    version=get_version(),
    author='Bareos Team',
    author_email='packager@bareos.com',
    packages=find_packages(),
    scripts=['bin/bconsole.py', 'bin/bconsole-json.py', 'bin/bareos-fd-connect.py'],
    package_data={'bareos': ['VERSION.txt']},
    url='https://github.com/bareos/python-bareos/',
    # What does your project relate to?
    keywords='bareos',
    description='Network socket connection to the Bareos backup system.',
    long_description=open('README.rst').read(),
    # Python 2.6 is used by RHEL/Centos 6.
    # When RHEL/Centos 6 is no longer supported (End of 2020),
    # Python 2.6 will no longer be supported by python-bareos.
    python_requires='>=2.6',
    install_requires=[
        #'hmac',
        #'socket',
        'python-dateutil',
    ],
    extras_require={
        'TLS-PSK':  ["sslpsk"],
    }
)
