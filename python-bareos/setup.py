#!/usr/bin/python

from setuptools import find_packages, setup

setup(
    name='python-bareos',
    version='0.3',
    author='Joerg Steffens',
    author_email='joerg.steffens@bareos.com',
    packages=find_packages(),
    scripts=['bin/bconsole.py', 'bin/bconsole-json.py', 'bin/bareos-fd-connect.py'],
    url='https://github.com/bareos/python-bareos/',
    # What does your project relate to?
    keywords='bareos',
    description='Network socket connection to the Bareos backup system.',
    long_description=open('README.rst').read(),
    install_requires=[
        #'hmac',
        #'socket',
        'python-dateutil',
    ]
)

