#!/bin/bash
set -e
set -o pipefail
set -u

#shellcheck source=../environment.in
. ./environment

#shellcheck source=../scripts/functions
. "${rscripts}"/functions
"${rscripts}"/cleanup
"${rscripts}"/setup

bin/bareos start

# make sure, director is up and running.
print_debug "$(bin/bconsole <<< "status dir")"
