#!/bin/bash

#This is a script for automatically downloading winbareos packages from
# the repo and creating new opsi packages
#daniel.neuberger@dass-it.de, 14.02.2013

#obs repo url
url=http://obs.dass-it/winbareos/SLE_11_SP2/noarch/

#deleting of the existing .exe binarys
rm -f /home/opsiproducts/bareos/CLIENT_DATA/data/winbareos-*.*.exe

#get an index.html for listing available files
wget $url

#fetching of the needed binary names
cat index.html | cut -s -d ">" -f 3 index.html | grep -o winbareos-*.*.*-*-bit-r*.*.exe >packages.txt

#cat index.html.1 | grep -o winbareos-*.*.*-*-bit-r*.*.exe | sed 's/...................................$//'>packages.txt

#download the .exe files
for a in `cat packages.txt` ; do

wget -P /home/opsiproducts/bareos/CLIENT_DATA/data/ $url$a

done

#auto configuration for the setup.ins
cat packages.txt | sed 's/-/ /g' | while read name version arch bit release ;
        do
	   cd /home/opsiproducts/bareos/CLIENT_DATA/
           sed -i -e's/Set $ProductVer$      = "*.*.*"/Set $ProductVer$      = "'$version'"/g'  -e's/Set $ReleaseVer$      = "*.*.*"/Set $ReleaseVer$      = "'$release'"/g' setup3264.ins
done

#building opsi packages
cd /home/opsiproducts/
opsi-makeproductfile -qv bareos

opsi-package-manager -i -qv bareos_*.opsi

#deleting of helpfiles
cd
rm index.html
rm packages.txt
