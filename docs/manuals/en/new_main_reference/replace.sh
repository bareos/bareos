#!/bin/bash

WORKING_DIR="./source/"

cd $WORKING_DIR

echo '============================'
echo 'REPLACING RAW-LATEX WITH RST'
echo '============================'

###############################################################################
#
# Substitutions
#
###############################################################################

#
# DROP SOME STUFF WHICH IS NO LONGER NEEDED
#

sed -i 's|:raw-latex:`\\footnotesize`||g' *.rst
sed -i 's|:raw-latex:`\\normalsize`||g' *.rst
sed -i 's|:raw-latex:`\\hfill`||g' *.rst

#
# General substitutions
#

sed -i 's#:raw-latex:`\\bareosFd`# \|bareosFd\| #g' *.rst
sed -i 's#:raw-latex:`\\bareosSd`# \|bareosSd\| #g' *.rst
sed -i 's#:raw-latex:`\\bareosDir`# \|bareosDir\| #g' *.rst
sed -i 's#:raw-latex:`\\bareosTraymonitor`# \|bareosTraymonitor\| #g' *.rst
sed -i 's#:raw-latex:`\\bareosWebui`# \|bareosWebui\| #g' *.rst

sed -i 's#:raw-latex:`\\mysql`# \|mysql\| #g' *.rst
sed -i 's#:raw-latex:`\\postgresql`# \|postgresql\| #g' *.rst
sed -i 's#:raw-latex:`\\sqlite`# \|sqlite\| #g' *.rst
sed -i 's#:raw-latex:`\\vmware`# \|vmware\| #g' *.rst
sed -i 's#:raw-latex:`\\vSphere`# \|vsphere\| #g' *.rst

#
# Bareos path and filenames
#

sed -i 's#:raw-latex:`\\fileStoragePath`#*/var/lib/bareos/storage/*#g' *.rst
sed -i 's#:raw-latex:`\\scriptPathUnix`#*/usr/lib/bareos/scripts/*#g' *.rst
sed -i 's#:raw-latex:`\\configPathUnix`#*/etc/bareos/*#g' *.rst
sed -i 's#:raw-latex:`\\configFileDirUnix`#*/etc/bareos/bareos-dir.conf*#g' *.rst
sed -i 's#:raw-latex:`\\configFileSdUnix`#*/etc/bareos/bareos-sd.conf*#g' *.rst
sed -i 's#:raw-latex:`\\configFileFdUnix}`#*/etc/bareos/bareos-fd.conf*#g' *.rst
sed -i 's#:raw-latex:`\\configFileBconsoleUnix}`#*/etc/bareos/bconsole.conf*#g' *.rst
sed -i 's#:raw-latex:`\\configFileTrayMonitorUnix`#*/etc/bareos/tray-monitor.conf*#g' *.rst
sed -i 's#:raw-latex:`\\configFileBatUnix`#*/etc/bareos/bat.conf*#g' *.rst
sed -i 's#:raw-latex:`\\configDirectoryDirUnix`#*/etc/bareos/bareos-dir.d/*#g' *.rst
sed -i 's#:raw-latex:`\\configDirectorySdUnix`#*/etc/bareos/bareos-sd.d/*#g' *.rst
sed -i 's#:raw-latex:`\\configDirectoryFdUnix`#*/etc/bareos/bareos-fd.d/*#g' *.rst
sed -i 's#:raw-latex:`\\configDirectoryBconsoleUnix`#*/etc/bareos/bconsole.d/*#g' *.rst
sed -i 's#:raw-latex:`\\configDirectoryTrayMonitorUnix`#*/etc/bareos/tray-monitor.d/*#g' *.rst
sed -i 's#:raw-latex:`\\logfileUnix`#*/var/log/bareos/bareos.log*#g' *.rst
sed -i 's#:raw-latex:`\\yeno`#yes\\|no#g' *.rst

#
# General
#

sed -i 's|:raw-latex:`\\command{bconsole}`|:command:`bconsole`|g' *.rst
sed -i -E 's|:raw-latex:`\\command\{(.*)\}`|:command:`\1`|g' *.rst
sed -i -E 's|:raw-latex:`\\bcommand\{(.*)\}\{(.*)\}`|:command:`\1 \2`|g' *.rst

sed -i -E 's|:raw-latex:`\\file\{(.*)\}`|*\1*|g' *.rst
sed -i -E 's|:raw-latex:`\\directory\{(.*)\}`|*\1*|g' *.rst
#sed -i -E 's#:raw-latex:`\\path\|(.*)\|`#*\1*#g' *.rst

sed -i 's/:raw-latex:`\\textbar`/ \\| /g' *.rst
sed -i 's/:raw-latex:`\\textbar{}`/ \\| /g' *.rst

sed -i -E 's|:raw-latex:`\\user\{(.*)\}`|**\1**|g' *.rst
sed -i -E 's|:raw-latex:`\\group\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\parameter\{(.*)\}`|**\1**|g' *.rst
sed -i -E 's|:raw-latex:`\\argument\{(.*)\}`|**\1**|g' *.rst
sed -i -E 's|:raw-latex:`\\variable\{(.*)\}`|**\1**|g' *.rst


sed -i -E 's|:raw-latex:`\\sinceVersion\{(.*)\}\{(.*)\}\{(.*)\}`|\3|g' *.rst

sed -i -E 's|:raw-latex:`\\job\{(.*)\}`|**\1**|g' *.rst
sed -i -E 's|:raw-latex:`\\fileset\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\name\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\host\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\email\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\package\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\pool\{(.*)\}`|**\1**|g' *.rst
sed -i -E 's|:raw-latex:`\\volumestatus\{(.*)\}`|**\1**|g' *.rst

sed -i -E 's|:raw-latex:`\\trademark\{(.*)\}`|*\1*|g' *.rst

#
# INDEX
#

sed -i -E 's|:raw-latex:`\\index\[general\]\{(.*)!(.*)\}`|\n.. index:: \n   triple: General; \1; \2|g' *.rst
sed -i -E 's|:raw-latex:`\\index\[general\]\{(.*)\}`|.. index:: General; \1 |g' *.rst
sed -i -E 's|:raw-latex:`\\index\[sd\]\{(.*)\}`|.. index:: SD; \1 |g' *.rst

#
# LABEL AND REFERENCES
#

sed -i -E 's|:raw-latex:`\\label\{(.*)\}`|.. _\1: \1|g' *.rst

sed -i -E 's|:raw-latex:`\\nameref\{(.*)\}`|:ref:`\1`|g' *.rst

sed -i -E 's|:raw-latex:`\\elink\{(.*)\}\{(.*)\}`|:ref:`\1 \2`|g' *.rst

sed -i -E 's|:raw-latex:`\\ilink\{(.*)\}\{(.*)\}`|:ref:`\2 \1`|g' *.rst

#
# Warnings
#

sed -i -E 's|:raw-latex:`\\warning\{(.*)\}`|\n.. warning:: \n  \1|g' *.rst

#
# IMAGES (JUST A FEW, SO MAYBE MANUALLY)
#

echo "DONE!!"

exit
