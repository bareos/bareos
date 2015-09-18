# cleanup old
rm -r python-bareos-?.?

# creates 
# dist/python-bareos-*.tar.gz
python setup.py sdist

# debian:
# see https://wiki.debian.org/Python/Packaging
cd dist
py2dsc python-bareos-*.tar.gz

# copy to obs directiory
OBS=...
cp -a python-bareos_*.debian.tar.gz  python-bareos_*.orig.tar.gz $OBS
cp -a python-bareos_0.2-1.dsc $OBS/python-bareos.dsc
cp -a ../packaging/*.spec $OBS
cp -a ../packaging/python-bareos.changes $OBS

# transfered to OBS _service file, to build directly from github.com
