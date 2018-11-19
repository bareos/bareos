#!/bin/bash

ORIGIN_DIR="../main/"
TARGET_DIR="./source/"

echo '==========================='
echo 'Converting tex files to rst'
echo '==========================='

###############################################################################
#
# Prepare tex files to avoid pandoc errors
#
###############################################################################

# todo (maybe)

###############################################################################
#
# CONVERT
#
###############################################################################

### INTRODUCTION AND TUTORIAL

echo 'general.tex'
pandoc --verbose -f latex+raw_tex -t rst ${ORIGIN_DIR}general.tex -o ${TARGET_DIR}general.rst

echo 'state.tex'
pandoc --verbose -f latex+raw_tex -t rst ${ORIGIN_DIR}state.tex -o ${TARGET_DIR}state.rst

echo 'install.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}install.tex -o ${TARGET_DIR}install.rst

echo 'webui.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}webui.tex -o ${TARGET_DIR}webui.rst

echo 'update.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}update.tex -o ${TARGET_DIR}update.rst

echo 'quickstart.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}quickstart.tex -o ${TARGET_DIR}quickstart.rst

echo 'tutorial.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}tutorial.tex -o ${TARGET_DIR}tutorial.rst

echo 'critical.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}critical.tex -o ${TARGET_DIR}critical.rst

### CONFIGURATION

echo 'configure.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}configure.tex -o ${TARGET_DIR}configure.rst

echo 'dirdconf.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}dirdconf.tex -o ${TARGET_DIR}dirdconf.rst

echo 'storedconf.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}storedconf.tex -o ${TARGET_DIR}storedconf.rst

echo 'filedconf.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}filedconf.tex -o ${TARGET_DIR}filedconf.rst

echo 'messagesres.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}messagesres.tex -o ${TARGET_DIR}messagesres.rst

echo 'consoleconf.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}consoleconf.tex -o ${TARGET_DIR}consoleconf.rst

echo 'monitorconf.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}monitorconf.tex -o ${TARGET_DIR}monitorconf.rst

### TASKS AND CONCEPTS

echo 'bconsole.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}bconsole.tex -o ${TARGET_DIR}bconsole.rst

echo 'restore.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}restore.tex -o ${TARGET_DIR}restore.rst

### VOLUME MANAGEMENT

echo 'disk.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}disk.tex -o ${TARGET_DIR}disk.rst

echo 'recycling.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}recycling.tex -o ${TARGET_DIR}recycling.rst

echo 'pools.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}pools.tex -o ${TARGET_DIR}pools.rst

echo 'autochangers.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}autochangers.tex -o ${TARGET_DIR}autochangers.rst

echo 'tape-without-autochanger.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}tape-without-autochanger.tex -o ${TARGET_DIR}tape-without-autochanger.rst

echo 'spooling.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}spooling.tex -o ${TARGET_DIR}spooling.rst

echo 'migration.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}migration.tex -o ${TARGET_DIR}migration.rst

echo 'always-incremental.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}always-incremental.tex -o ${TARGET_DIR}always-incremental.rst

echo 'basejob.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}basejob.tex -o ${TARGET_DIR}basejob.rst

echo 'plugin.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}plugins.tex -o ${TARGET_DIR}plugins.rst

echo 'win32.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}win32.tex -o ${TARGET_DIR}win32.rst

### NETWORK SETUP

echo 'client-initiated-connection.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}client-initiated-connection.tex -o ${TARGET_DIR}client-initiated-connection.rst

echo 'passiveclient.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}passiveclient.tex -o ${TARGET_DIR}passiveclient.rst

echo 'lanaddress.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}lanaddress.tex -o ${TARGET_DIR}lanaddress.rst

echo 'tls.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}tls.tex -o ${TARGET_DIR}tls.rst

echo 'dataencryption.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}dataencryption.tex -o ${TARGET_DIR}dataencryption.rst

### NDMP BACKUPS WITH BAREOS

echo 'ndmp.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}ndmp.tex -o ${TARGET_DIR}ndmp.rst

echo 'catmaintenance.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}catmaintenance.tex -o ${TARGET_DIR}catmaintenance.rst

echo 'security.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}security.tex -o ${TARGET_DIR}security.rst

### APPENDIX

# A
echo 'requirements.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}requirements.tex -o ${TARGET_DIR}requirements.rst

# B
echo 'supportedoses.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}supportedoses.tex -o ${TARGET_DIR}supportedoses.rst

# C
echo 'programs.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}programs.tex -o ${TARGET_DIR}programs.rst

# D
echo 'bootstrap.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}bootstrap.tex -o ${TARGET_DIR}bootstrap.rst

# E
echo 'verify.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}verify.tex -o ${TARGET_DIR}verify.rst

# F
echo 'backward-compability.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}backward-compability.tex -o ${TARGET_DIR}backward-compatibility.rst

# G
echo 'tables.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}tables.tex -o ${TARGET_DIR}tables.rst

# H
echo 'howto.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}howto.tex -o ${TARGET_DIR}howto.rst

# I
echo 'rescue.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}rescue.tex -o ${TARGET_DIR}rescue.rst

# J
echo 'troubleshooting.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}troubleshooting.tex -o ${TARGET_DIR}troubleshooting.rst

# K
echo 'debug.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}debug.tex -o ${TARGET_DIR}debug.rst

# L
echo 'releasenotes.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}releasenotes.tex -o ${TARGET_DIR}releasenotes.rst

# M
echo 'license.tex'
pandoc --verbose  -f latex+raw_tex -t rst ${ORIGIN_DIR}license.tex -o ${TARGET_DIR}license.rst

exit
