#!/usr/bin/python

from distutils.core import setup

setup(
    name='python-bareos',
    version='0.1',
    author='Joerg Steffens',
    author_email='joerg.steffens@bareos.com',
    packages=['bareos'],
    scripts=['bin/bconsole.py'],
    url='https://github.com/joergsteffens/python-bareos/',
    description='connect to Bareos backup system.',
    long_description=open('README.md').read(),
)

