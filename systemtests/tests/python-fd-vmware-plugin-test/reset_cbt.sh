#!/bin/bash
set -e
#set -u

. environment
"${PROJECT_SOURCE_DIR}"/../core/src/vmware/vmware_cbt_tool/vmware_cbt_tool.py  \
  -s "@vmware_server@" \
  -u "@vmware_user@" \
  -p "@vmware_password@" \
  -f "@vmware_folder@" \
  -d "@vmware_datacenter@" \
  -v "@vmware_vm_name@" \
  --resetcbt
