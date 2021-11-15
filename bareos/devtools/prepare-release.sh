#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2021 Bareos GmbH & Co. KG
#
#   This program is Free Software; you can redistribute it and/or
#   modify it under the terms of version three of the GNU Affero General Public
#   License as published by the Free Software Foundation and included
#   in the file LICENSE.
#
#   This program is distributed in the hope that it will be useful, but
#   WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#   Affero General Public License for more details.
#
#   You should have received a copy of the GNU Affero General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
#   02110-1301, USA.

set -e
set -u

# this will match a version or pre-release version e.g.
# "19.2.4" or "19.2.4~pre13.xyz"
version_regexp='([[:digit:]]+\.){2}[[:digit:]]+(~[[:alnum:]]+)?'

# this will only match a stable version e.g.
# "18.2.7"
stable_version_regexp='([[:digit:]]+\.){2}[[:digit:]]+'

topdir="$(realpath "$(dirname "$0")/..")"
prog="$(basename "$0")"

git="${GIT:-$(type -p git)}"
cmake="${CMAKE:-$(type -p cmake)}"

pushd "$topdir" >/dev/null

log_info() {
  echo -e "\e[36m$prog: INFO:  $*\e[0m" >&2
}

log_warn() {
  echo -e "\e[33m$prog: WARN:  $*\e[0m" >&2
}

log_fatal() {
  echo -e "\e[31m$prog: FATAL: $*\e[0m" >&2
  exit 1
}

# wrap stdin in a nice blue box
print_block() {
  local s='\e[44m\e[97m' e='\e[0m'
  printf "\n%b┏" "$s"; printf -- "━%.0s" {1..77}; printf "┓%b\n" "$e"
  printf "%b┃  %-73s  ┃%b\n" "$s" '' "$e"
  while read -r line; do
    printf "%b┃  %-73s  ┃%b\n" "$s" "$line" "$e"
  done
  printf "%b┃  %-73s  ┃%b\n" "$s" '' "$e"
  printf "%b┗" "$s"; printf -- "━%.0s" {1..77}; printf "┛%b\n\n" "$e"
}

join_by() {
  local IFS="$1"
  shift
  echo "$*"
}

# get_next_version accepts a stable version as its only parameter.
# it will then return the patch-version that follows after the provided version.
get_next_version() {
  local in_parts out_parts last
  IFS=. read -ra in_parts <<< "$1" # split into an array
  ((last=${#in_parts[@]} - 1 )) # get index of last element
  out_parts=()
  # concatenate up to, but not including last element
  for (( i=0; i<last; i++ )); do
    out_parts+=("${in_parts[i]}")
  done
  out_parts+=($(( in_parts[i]+1 ))) # add last element incremented by one
  join_by . "${out_parts[@]}" # join array to string
}

confirm() {
  echo -ne "  \e[44m\e[97m $* (y/n) \e[0m"
  read -n 1 -r
  echo -e "\n"
  [[ $REPLY =~ ^[Yy]$ ]]
}

if [ -z "$git" ] || [ ! -x "$git" ]; then
  log_fatal "git not found."
elif [ -z "$cmake" ] || [ ! -x "$cmake" ]; then
  log_fatal "cmake not found."
elif [ ! -e .git ]; then
  log_fatal "This is not a git working copy."
elif [ -n "$("$git" status --short)" ]; then
  log_fatal "This script must be run in a clean working copy."
fi

# get the git remote name for the upstream GitHub repository
git_remote="$("$git" remote -v | \
  awk '/git@github.com:\/?bareos\/bareos.git \(push\)/ { print $1 }')"
if [ -z "$git_remote" ]; then
  # warn if not found and set a default for sane messages"
  log_warn "Could not find the upstream repository in your git remotes."
  git_remote="<remote>"
fi


# we try to detect the version that should be released.
# for this we retrieve the version of the source-tree we're running in and strip
# any prerelease suffix.
autodetect_version=$("$cmake" -P get_version.cmake \
                      | sed --expression='s/^-- //' --expression='s/~.*$//')
if [ -n "${1:-}" ]; then
    log_info "Using provided version $1."
    version="$1"
elif [ -n "$autodetect_version" ]; then
  log_info "Autodetected version $autodetect_version based on WIP-tag."
  version="$autodetect_version"
else
  log_fatal Cannot autodetect version and no version passed on cmdline
fi

if ! grep --quiet --extended-regexp "^$version_regexp\$" <<< "$version"; then
  log_fatal "Version $1 does not match the expected pattern"
fi

git_branch="$(git rev-parse --abbrev-ref=strict HEAD)"
if ! grep --quiet --fixed-strings "$git_branch" <<< "bareos-$version"; then
  log_warn "Git branch $git_branch is not the release branch for $version."
fi

release_tag="Release/${version/\~/-}"
release_ts="$(date --utc --iso-8601=seconds | sed --expression='s/T/ /;s/+.*$//')"
release_date="$(sed --expression='s/ .*$//' <<< "$release_ts")"
release_message="Release $version"

# Only if we are preparing a stable release
if grep --quiet --extended-regexp "^$stable_version_regexp\$" <<< "$version"; then
  wip_enable=1
  wip_version="$(get_next_version "$version")"
  wip_tag="WIP/$wip_version-pre"
  wip_message="Start development of $wip_version"
else
  log_info "Will not generate a new WIP tag for a pre-release"
  wip_enable=0
  wip_version="(none)"
  wip_tag="(none)"
  wip_message="(none)"
fi

print_block << EOT
You are preparing a release of Bareos. As soon as you push the results of
this script to the official git repository, the new version is released.
If you decide not to push, nothing will be released.

The release you are preparing will have the following settings:
   Version:      "$version"
   Release tag:  "$release_tag"
   Release time: "$release_ts"
   Release msg:  "$release_message"

If this is not a pre-release a new work-in-progress tag will be created:
   Next version: "$wip_version"
   WIP tag:      "$wip_tag"
   WIP message:  "$wip_message"


This script will do the following:

  1. Create release commit
  1a. create empty commit with version timestamp
  1b. generate and add cmake/BareosVersion.cmake
  1c. update CHANGELOG.md to reflect new release
  1d. amend commit from a. with changes from b. and c.

  2. Clean up after release commit
  2a. remove cmake/BareosVersion.cmake
  2b. commit the removed files

  3. Set up a new base for future work (not for pre-releases)
  3a. add a new "unreleased" section to CHANGELOG.md
  3b. commit updated CHANGELOG.md

  4. Set tags
  4a. add the release-tag pointing to the commit from 1d.
  4b. add WIP tag pointing to the commit form 3b. if applicable

Please make sure you're on the right branch before continuing and review
the commits, tags and branch pointers before you push!

For a major release you should be on the release-branch, not the master
branch. While you can move around branch pointers later, it is a lot
easier to branch first.
EOT

if ! confirm "Do you want to proceed?"; then
  log_info "Exiting due to user request."
  exit 0
fi

original_commit="$(git rev-parse HEAD)"
log_info "if you want to rollback the commits" \
  "you can run 'git reset --soft $original_commit'"

# we start with a temporary commit so write_version_files.cmake will then see
# the timestamp of that commit. This makes sure we have $release_ts written to
# the BareosVersion.cmake file
"$git" commit \
  --quiet \
  --allow-empty \
  --date="$release_ts" \
  -m "TEMPORARY COMMIT MESSAGE"

"$cmake" -DVERSION_STRING="$version" -P write_version_files.cmake

if [ "$wip_enable" -eq 1 ]; then
  ./devtools/new-changelog-release.sh "$version" "$release_date"
  ./devtools/update-changelog-links.sh
fi

"$git" add \
  --no-all \
  --force \
  -- \
  ./cmake/BareosVersion.cmake \
  ./CHANGELOG.md

"$git" commit \
  --quiet \
  --amend \
  --only \
  --date="$release_ts" \
  -m "$release_message" \
  -- \
  ./cmake/BareosVersion.cmake \
  ./CHANGELOG.md

release_commit="$(git rev-parse HEAD)"
log_info "commit for release tag will be $release_commit"

"$git" rm \
  -- \
  ./cmake/BareosVersion.cmake

"$git" commit \
  --quiet \
  --date="$release_ts" \
  -m "Remove cmake/BareosVersion.cmake after release"

if [ "$wip_enable" -eq 1 ]; then
  ./devtools/new-changelog-release.sh unreleased
  ./devtools/update-changelog-links.sh

  "$git" add \
    --no-all \
    -- \
    ./CHANGELOG.md

  "$git" commit \
    --quiet \
    --allow-empty \
    --date="$release_ts" \
    -m "$wip_message"

  wip_commit="$(git rev-parse HEAD)"
  log_info "commit for WIP tag will be $wip_commit"
fi


echo
echo The log for the new commits is:
echo

"$git" log \
  --decorate \
  --graph \
  "${original_commit}..HEAD"

echo

log_info "Creating release tag for $release_commit named $release_tag"
"$git" tag "$release_tag" "$release_commit"

if [ -n "${wip_commit:-}" ]; then
  log_info "Creating WIP tag for $wip_commit named $wip_tag"
  "$git" tag "$wip_tag" "$wip_commit"
fi

(
  cat <<EOT
To publish your new release, you need to do the follwing git push steps:
  git push $git_remote HEAD
  git push $git_remote $release_tag
EOT
  if [ -n "${wip_commit:-}" ]; then
    echo "git push $git_remote $wip_tag"
  fi
) | print_block
