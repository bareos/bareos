#!/bin/bash
set -e
set -u

# this will match a version or pre-release version e.g.
# "19.2.4" or "19.2.4~pre13.xyz"
version_regexp='([[:digit:]]+\.){2}[[:digit:]]+(~[[:alnum:]]+)?'

# this will only match a stable version e.g.
# "18.2.7"
stable_version_regexp='([[:digit:]]+\.){2}[[:digit:]]+'

topdir="$(dirname "$0")"
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
  while read line; do
    printf "%b┃  %-73s  ┃%b\n" "$s" "$line" "$e"
  done
  printf "%b┃  %-73s  ┃%b\n" "$s" '' "$e"
  printf "%b┗" "$s"; printf -- "━%.0s" {1..77}; printf "┛%b\n\n" "$e"
}

# get_next_version accepts a stable version as its only parameter.
# it will then return the patch-version that follows after the provided version.
get_next_version() {
  local in_parts out_parts last
  IFS=. in_parts=($1)  # split into an array
  ((last=${#in_parts[@]} - 1 )) # get index of last element
  out_parts=()
  # concatenate up to, but not including last element
  for (( i=0; i<last; i++ )); do
    out_parts+=("${in_parts[i]}")
  done
  out_parts+=($(( in_parts[i]+1 ))) # add last element incremented by one
  IFS=. echo "${out_parts[*]}" # join array to string
}

confirm() {
  echo -ne "  \e[44m\e[97m $* (y/n) \e[0m"
  read -n 1 -r
  echo -e "\n"
  [[ $REPLY =~ ^[Yy]$ ]]
}

replace_define_expr() {
  # generate an sed expression to replace the string value of a single-line
  # C preprocessor object macro
  # requires sed --regexp-extended
  echo "s/(#define $1[^\"]+)\"[^\"]+\"/\1\"$2\"/"
}

if [ -z "$git" -o ! -x "$git" ]; then
  log_fatal "git not found."
elif [ -z "$cmake" -o ! -x "$cmake" ]; then
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
autodetect_version=$("$git" describe --tags --match "WIP/*" \
                     | grep --extended-regexp --only-matching "$version_regexp")
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
  1. Patch version.h with the correct version and timestamps
  2. Create a commit containing the patched version.h
  3. Add an empty commit for a WIP tag (not for pre-releases)
  4. Set the release tag
  5. Set the WIP tag (not for pre-releases)

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

version_h=src/include/version.h

define_BDATE="$(LC_TIME=C date --date="$release_ts+0000" "+%d %B %Y")"
define_LSMDATE="$(LC_TIME=C date --date="$release_ts+0000" "+%d%b%y")"
define_BYEAR="$(LC_TIME=C date --date="$release_ts+0000" "+%Y")"

sed \
  --in-place \
  --regexp-extended \
  --expression="$(replace_define_expr VERSION "$version")" \
  --expression="$(replace_define_expr BDATE "$define_BDATE")" \
  --expression="$(replace_define_expr LSMDATE "$define_LSMDATE")" \
  --expression="$(replace_define_expr BYEAR "$define_BYEAR")" \
  "$version_h" 

"$git" diff -- "$version_h"

if ! confirm "Should we proceed with the above changes applied?"; then
  log_info "Exiting due to user request."
  exit 0
fi

"$git" add \
  --no-all \
  -- \
  "$version_h"

"$git" commit \
  --quiet \
  --only \
  --date="$release_ts" \
  -m "$release_message" \
  -- \
  "$version_h"

release_commit="$(git rev-parse HEAD)"
log_info "commit for release tag will be $release_commit"

if [ "$wip_enable" -eq 1 ]; then
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
