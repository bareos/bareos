#!/bin/bash

# include set_config_param()
. scripts/bareos-config-lib.sh

create_config_resource_template()
{
   local COMPONENT="$1"
   local RESOURCE="$2"
   local NAME="$3"
   local PARAM="$4"
   local VALUE="$5"

   RESPATH="${1}.d/${2}/${3}.conf"

   DEST="$DEST_DIR/etc/bareos/${RESPATH}"
   if ! [ -e "$DEST" ]; then
      SOURCE="src/defaultconfigs/${RESPATH}"
      mkdir -p `dirname $DEST`
      printf '%s\n' '@%@UCRWARNING=# @%@' > $DEST
      cat $SOURCE >> $DEST
   fi
   set_config_param "$DEST" "$RESOURCE" "$NAME" "$PARAM" "$VALUE"
}



#
# main
#

DEST_DIR=$1

#
# bareos-sd
#
create_config_resource_template bareos-sd  device  FileStorage      ArchiveDevice      '@%@bareos/filestorage@%@'

#
# bareos-dir
#
create_config_resource_template bareos-dir jobdefs DefaultJob       FileSet            "\"LinuxAll\""
create_config_resource_template bareos-dir job     backup-bareos-fd Enabled            '@%@bareos/backup_myself@%@'
create_config_resource_template bareos-dir pool    Full             MaximumVolumeBytes '@%@bareos/max_full_volume_bytes@%@ G'
create_config_resource_template bareos-dir pool    Full             MaximumVolumes     '@%@bareos/max_full_volumes@%@'
create_config_resource_template bareos-dir pool    Differential     MaximumVolumeBytes '@%@bareos/max_diff_volume_bytes@%@ G'
create_config_resource_template bareos-dir pool    Differential     MaximumVolumes     '@%@bareos/max_diff_volumes@%@'
create_config_resource_template bareos-dir pool    Incremental      MaximumVolumeBytes '@%@bareos/max_incr_volume_bytes@%@ G'
create_config_resource_template bareos-dir pool    Incremental      MaximumVolumes     '@%@bareos/max_incr_volumes@%@'

find $DEST_DIR -type f | sort

