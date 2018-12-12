#!/bin/bash

DESTFILE=$1

PERL="perl -i.bak -pe"

# links
# ${PERL} 's#':raw-latex:`\\elink\{fork\}\{http://www.bareos.org/en/faq/items/why_fork.html\}`#`\1 \2>`_#g'

${PERL} 's#\\bconfigInput\{(.*?)\}#.. literalinclude:: ../../main/\1#g' ${DESTFILE}

${PERL} 's#:raw-latex:`\\bareosWhitepaperTapeSpeedTuning`#\\elink{Bareos Whitepaper Tape Speed Tuning}{http://www.bareos.org/en/Whitepapers/articles/Speed_Tuning_of_Tape_Drives.html}#g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosMigrateConfigSh `#\\elink{bareos-migrate-config.sh}{https://github.com/bareos/bareos-contrib/blob/master/misc/bareos-migrate-config/bareos-migrate-config.sh} #g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosTlsConfigurationExample`#\\elink{Bareos Regression Testing Base Configuration}{https://github.com/bareos/bareos-regress/tree/master/configs/BASE/}#g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\externalReferenceDroplet`#\\url{https://github.com/scality/Droplet}#g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\externalReferenceIsilonNdmpEnvironmentVariables`#}\elink{Isilon OneFS 7.2.0 CLI Administration Guide}{https://www.emc.com/collateral/TechnicalDocument/docu56048.pdf}, section \bquote{NDMP environment variables}#g'  ${DESTFILE}



${PERL} 's#:raw-latex:`\\linkResourceDirective\*\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\linkResourceDirectiveValue\{(.*?)\}\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ = **\4**#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\linkResourceDirective\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\linkResourceDirective\*\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}


${PERL} 's#:raw-latex:`\\resourceDirectiveValue\{(.*?)\}\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ = **\4**#g' ${DESTFILE}


${PERL} 's#:raw-latex:`\\bquote\{(.*?)\}`#:emphasis:`\1`#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\pluginevent\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\textrightarrow`#->#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\textrightarrow `#->#g' ${DESTFILE}

#``URL:http://download.bareos.org/bareos/release/latest/``
${PERL} 's#``URL:(.*?)``#`\1 <\1>`_#g'   ${DESTFILE}
${PERL} 's#``\\url\{(.*?)\}#`\1 \1>`_#g'   ${DESTFILE}

${PERL} 's#:raw-latex:`\\ref\{(.*?)\}`#`<\1>`_#g' ${DESTFILE}

${PERL} 's#``path:(.*)``#:file:`\1`#g'   ${DESTFILE}


# abbreviations
${PERL} 's#:raw-latex:`\\bareosDir(\s*)`#\|bareosDir\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosFd(\s*)`#\|bareosFd\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosSd(\s*)`#\|bareosSd\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosTrayMonitor(\s*)`#\|bareosTrayMonitor\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosWebui(\s*)`#\|bareosWebui\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\mysql(\s*)`# \|mysql\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\postgresql(\s*)`# \|postgresql\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\sqlite(\s*)`#\|sqlite\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\vmware(\s*)`#\|vmware\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\vSphere(\s*)`#\|vsphere\|\1#g'   ${DESTFILE}

# link targets
${PERL} 's#:raw-latex:`\\ilink\{(.*?)\}\{(.*?)\}`#:ref:`\1 <\2>`#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\nameref\{(.*?)\}`#:ref:`\1`#g' ${DESTFILE}
${PERL} 's#\\nameref\{(.*?)\}#:ref:`\1`#g' ${DESTFILE}



${PERL} 's#:raw-latex:`\\elink\{(.*?)\}\{(.*?)\}`(\s*)#`\1 <\2>`_\3#g' ${DESTFILE}
${PERL} 's#\\elink\{(.*?)\}\{(.*?)\}(\s*)#`\1 <\2>`_\3#g' ${DESTFILE}




${PERL} 's#:raw-latex:`\\label\{(.*)\}`#\n\n.. _`\1`: \1#g'   ${DESTFILE}

#${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}\{(.+?)}`#:strong:`\1 \2`#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\bcommand\{(\w+)\}\{([\w/ ]+)}`#:strong:`\1 \2`#g' ${DESTFILE}

${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}\{}`#:strong:`\1`#g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\bcommand\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}

${PERL} 's#\\bcommand\{(.*?)\}\{}#:strong:`\1`#g'  ${DESTFILE}

${PERL} 's#:raw-latex:`\\command\{(.*?)\}`#:program:`\1`#g'  ${DESTFILE}

${PERL} 's#:raw-latex:`\\configline\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\configdirective\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}
${PERL} 's#:raw-latex:`\\configresource\{(.*?)\}`#:strong:`\1`#g'  ${DESTFILE}



# abbreviations
${PERL} 's#:raw-latex:`\\bareosDir\(\s*\)`#\|bareosDir\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosFd\(\s*\)`#\|bareosFd\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosSd\(\s*\)`#\|bareosSd\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosTrayMonitor\(\s*\)`#\|bareosTrayMonitor\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\bareosWebui\(\s*\)`#\|bareosWebui\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\mysql\(\s*\)`# \|mysql\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\postgresql\(\s*\)`# \|postgresql\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\sqlite\(\s*\)`#\|sqlite\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\vmware\(\s*\)`#\|vmware\|\1#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\vSphere\(\s*\)`#\|vsphere\|\1#g'   ${DESTFILE}





# rename sec: to section- as : has special meaning in sphinx
${PERL} 's#sec:#section-#g'   ${DESTFILE}
${PERL} 's#item:#item-#g'   ${DESTFILE}


# \package
${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\package\{(.*?)\}`#**\1**#g'   ${DESTFILE}

${PERL} 's#:raw-latex:`\\volumestatus\{(.*?)\}`#**\1**#g'   ${DESTFILE}


${PERL} 's#:raw-latex:`\\volumeparameter\{(.*?)\}\{(.*?)\}`#\1 = **\2**#g' ${DESTFILE}

# remove nonsense lines
${PERL} 's#   .. raw:: latex##g'   ${DESTFILE}

# \parameter
${PERL} 's#(\s+):raw-latex:`\\parameter\{(.*?)\}(\s*)`#\1               :option:`\2`\3#g' ${DESTFILE}
${PERL} 's#(\s+)\\parameter\{(.*?)\}(\s*)#\1                  :option:`\2`\3#g' ${DESTFILE}



# sinceversion
#              raw-latex:`\sinceVersion{dir}{requires!jansson}{15.2.0}`
#sed  's#:raw-latex:`\\sinceVersion\{(.*)\}\{(.*)\}\{(.*)\}`#\n\n.. versionadded:: \3\n   \1 \2\n#g'   ${DESTFILE}
${PERL} 's|:raw-latex:`\\sinceVersion\{(.*)\}\{(.*)\}\{(.*)\}`|\3|g'   ${DESTFILE}



# file
${PERL} 's#:raw-latex:`\\file\{(.*?)\}`#:file:`\1`#g'   ${DESTFILE}
${PERL} 's#\\file\{(.*?)\}#:file:`\1`#g'   ${DESTFILE}

${PERL} 's#:raw-latex:`\\directory\{(.*?)\}`#:file:`\1`#g'   ${DESTFILE}

${PERL} 's#:raw-latex:`\\argument\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}

#\dt
${PERL} 's#:raw-latex:`\\dt\{([A-Za-z ]*)\}`#:strong:`\1`\ #g' ${DESTFILE}

${PERL} 's#:raw-latex:`\\dtYesNo`#:strong:`Yes|No`#g'  ${DESTFILE}

#${PERL} 's#:raw-latex:`\\multicolumn\{[^}]*\}\{[^}]+\}\{([^}]*)\}`#:strong:`\1`\ #g' ${DESTFILE}


#\os
${PERL} 's#\\os\{(.*?)\}\{}#:strong:`\1`#g' ${DESTFILE}
${PERL} 's#\\os\{(.*?)\}\{(.*?)}#:strong:`\1 \2`#g' ${DESTFILE}




#code blocks
#     {commands}{get the process ID of a running Bareos daemon}
#     command> /command>parameter>ps fax | grep bareos-dir/parameter>
#      2103 ?        S      0:00 /usr/sbin/bareos-dir

#.. code-block:: c
#   :caption: get the process ID of a running Bareos daemon

${PERL} 's#begin\{verbatim\}##g' ${DESTFILE}
${PERL} 's#end\{verbatim\}##g' ${DESTFILE}

${PERL} 's#\{bareosConfigResource\}\{(.*?)\}\{(.*?)\}\{(.*?)\}#\n.. code-block:: sh\n    :caption: \1 \2 \3\n#g'   ${DESTFILE}
#${PERL} 's#\{bareosConfigResource\}\{(.*?)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}

${PERL} 's#\{commands\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
${PERL} 's#\{logging\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
${PERL} 's#\{config\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
${PERL} 's#\{bconfig\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
${PERL} 's#\{commandOut\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}
${PERL} 's#\{bconsole\}\{(.*)\}#\n.. code-block:: sh\n    :caption: \1\n#g'   ${DESTFILE}


perl -i -pe 's#\{bareosConfigResource\}\{(.*)\}\{(.*)\}\{(.*)\}#\n.. code-block:: sh\n    :caption: daemon:\1 resource:\2 name:\3\n#g' ${DESTFILE}

#greater or equal sign
${PERL} 's#\$\\ge\$#≥#g'   ${DESTFILE}
${PERL} 's#\$\\geq\$#≥#g'   ${DESTFILE}
${PERL} 's#\$\$##g'   ${DESTFILE}
${PERL} 's#\$=\$#≤#g'   ${DESTFILE}

# \user
${PERL} 's#:raw-latex:`\\user\{(.*)\}`#**\1**#g'   ${DESTFILE}
${PERL} 's#\\user\{(.*)\}#**\1**#g'   ${DESTFILE}



# remove xml stuff

${PERL} 's#\<input\>##g'  ${DESTFILE}
${PERL} 's#\</input\>##g'  ${DESTFILE}
${PERL} 's#\<command\>##g'  ${DESTFILE}
${PERL} 's#\</command\>##g'  ${DESTFILE}





# resourcetype

${PERL} 's#:raw-latex:`\\resourcetype\{(.*?)\}\{(.*?)\}`#:sup:`\1`\ :strong:`\2`#g'   ${DESTFILE}

# \name


# \dbtable
${PERL} 's#\\dbtable\{(.*)\}#**\1**#g'   ${DESTFILE}

# resourcename

${PERL} 's#:raw-latex:`\\resourcename\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\resourcename\*\{(.*?)\}\{(.*?)\}\{(.*?)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}





${PERL} 's#:raw-latex:`\\job\{(.*?)\}`#**\1**:sup:`Dir`:sub:`job`\ #g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\pool\{(.*?)\}`#**\1**:sup:`Dir`:sub:`pool`\ #g'   ${DESTFILE}

${PERL} 's#:raw-latex:`\\textbar`#|#g'   ${DESTFILE}







#\dt



#\os



# TODO: add link
${PERL} 's#:raw-latex:`\\linkResourceDirective\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\linkResourceDirective\*\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}`#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}


${PERL} 's#\\linkResourceDirective\{([ \w]+)\}\{([ \w]+)\}\{([ \w]+)\}#**\3**:sup:`\1`:sub:`\2`\ #g'   ${DESTFILE}









# TODO: check if |xxxx| replace can be used
${PERL} 's#\\configFileDirUnix#:file:`/etc/bareos/bareos-dir.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\fileStoragePath`#:file:`/var/lib/bareos/storage/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\scriptPathUnix`#:file:`/usr/lib/bareos/scripts/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configPathUnix`#:file:`/etc/bareos/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configFileDirUnix`#:file:`/etc/bareos/bareos-dir.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configFileSdUnix`#:file:`/etc/bareos/bareos-sd.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configFileFdUnix`#:file:`/etc/bareos/bareos-fd.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configFileBconsoleUnix`#:file:`/etc/bareos/bconsole.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configFileTrayMonitorUnix`#:file:`/etc/bareos/tray-monitor.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configFileBatUnix`#:file:`/etc/bareos/bat.conf`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configDirectoryDirUnix`#:file:`/etc/bareos/bareos-dir.d/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configDirectorySdUnix`#:file:`/etc/bareos/bareos-sd.d/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configDirectoryFdUnix`#:file:`/etc/bareos/bareos-fd.d/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configDirectoryBconsoleUnix`#:file:`/etc/bareos/bconsole.d/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\configDirectoryTrayMonitorUnix`#:file:`/etc/bareos/tray-monitor.d/`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\logfileUnix`#:file:`/var/log/bareos/bareos.log`#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\yesno`#yes\\|no#g'   ${DESTFILE}
${PERL} 's#:raw-latex:`\\NdmpBareos`#NDMP_BAREOS#g'   ${DESTFILE}



${PERL} 's|:raw-latex:`\\fileset\{(.*)\}`|**\1**|g'   ${DESTFILE}

${PERL} 's#:raw-latex:`\\cmlink\{([^}]*)\}`#                  **\1**#g'   ${DESTFILE}
${PERL} 's#\\cmlink\{(.*)\}#**\1**#g'   ${DESTFILE}

# remove stuff that has no meaning anymore
${PERL} 's#\\footnotesize##g'   ${DESTFILE}
${PERL} 's#\\normalsize##g'   ${DESTFILE}
${PERL} 's#\\hfill##g'   ${DESTFILE}


${PERL} 's#:math:`\ldots`#...#g'   ${DESTFILE}

PERL0="perl -0 -i.bak -pe"
# warning
${PERL0} 's#:raw-latex:`\\warning\{(.*?)\}`#\n.. warning:: \n  \1#ims' ${DESTFILE}
${PERL0} 's#\\warning\{(.*?)\}#\n.. warning:: \n  \1#ims'  ${DESTFILE}






# clean empty :: at the start of line
${PERL} 's/\s*::$//g' ${DESTFILE}



#######
# index
#######



# index
#:raw-latex:`\index[general]{Version numbers}`
# ->
#:index:`Version numbers`
${PERL} 's#^(\s*):raw-latex:`\\index\[(.*?)\]\{([^!]*)\}`#.. index::\n\1   single: \3\n#g'   ${DESTFILE}
#${PERL} 's#(\s*):raw-latex:`\\index\[(.*?)\]\{(.*?)\}`#\1:index:`\3 <single: \3>`#g'    ${DESTFILE}

#:raw-latex:`\index[general]{Problem!Windows}`
# ->
#:index:`pair: Problem;Windows>`
${PERL} 's#^(\s*):raw-latex:`\\index\[(.*?)\]\{([^!]*)!([^!}]*)\}`#.. index::\n\1   pair: \3; \4\n#g'   ${DESTFILE}
${PERL} 's#^(\s*):raw-latex:`\\index\[(.*?)\]\{([^!]*)!([^!}]*)\}`#.. index::\n\1   pair: \3; \4\n#g'   ${DESTFILE}
${PERL} 's#^(\s*):raw-latex:`\\index\[(.*?)\]\{([^!]*)!([^!}]*)\}`#.. index::\n\1   pair: \3; \4\n#g'   ${DESTFILE}
#${PERL} 's#(\s*):raw-latex:`\\index\[(.*?)\]\{(.*?)!(.*?)\}`#\1:index:`\4 <pair: \3;\4>`#g'    ${DESTFILE}



#:raw-latex:`\index[general]{Windows!File Daemon!Command Line Options}`
# ->
#:index:`triple: <Windows!File Daemon!Command Line Options>`
${PERL} 's#^(\s*):raw-latex:`\\index\[([^]]*)\]\{([^!]*)!([^!]*)!([^!}]*)\}`#\n.. index::\n\1   triple: \3; \4; \5;\n#g'   ${DESTFILE}
#${PERL} 's#(\s*):raw-latex:`\\index\[(.*?)\]\{(.*?)!(.*?)!(.*?)\}`#\1:index:`\5 <triple: \3;\4;\5>`#g'    ${DESTFILE}




${PERL} 's#:raw-latex:`\\name\{(.*?)\}`#**\1**#g' ${DESTFILE}
${PERL} 's#:raw-latex:`\\host\{(.*?)\}`#:strong:`\1`#g' ${DESTFILE}



# add rst comment that this file was automatically converted

mv  ${DESTFILE}  ${DESTFILE}.tmp
echo ".. ATTENTION do not edit this file manually." > ${DESTFILE}
echo "   It was automatically converted from the corresponding .tex file" >> ${DESTFILE}
echo "" >> ${DESTFILE}
cat ${DESTFILE}.tmp >> ${DESTFILE}
rm ${DESTFILE}.tmp

diff  --color -ruN   ${DESTFILE}.bak ${DESTFILE}

exit 0
