#!/bin/bash

set -e
set -u

backuplevel="${1:-}"

REAR_OUTPUT_PATH=/var/lib/rear/output/

if [ "$backuplevel" = "Full" ]; then
    echo "backup level = $backuplevel: recreating rescue image"
else
    days=7
    current_rescue_images=( $( find "$REAR_OUTPUT_PATH" -name "rear-*" -mtime -$days ) )
    if  [ ${#current_rescue_images[@]} -gt 0 ]; then
        echo "SKIPPING 'rear mkrescue', as images newer than $days days exists: ${current_rescue_images[@]}"
        exit 0
    else
        echo "no current rescue image found. recreating it"
    fi
fi

rear mkrescue
exit $?
