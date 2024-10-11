#!/bin/bash
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2024-2024 Bareos GmbH & Co. KG
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

topdir="$(realpath "$(dirname "$0")/..")"
prog="$(basename "$0")"
: "${install_dir:=$HOME/.local/lib/bareos-tools}"

git="${GIT:-$(command -v git)}"

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

confirm() {
  echo -ne "  \e[44m\e[97m $* (y/n) \e[0m"
  read -n 1 -r
  echo -e "\n"
  [[ $REPLY =~ ^[Yy]$ ]]
}

get_symlink_hash() {
  echo -n "$*" | git hash-object --stdin
}

find_wrapper_links() {
  local objectname
  objectname="$(get_symlink_hash pip-tools/pipenv-wrapper.sh)"
  "${git}" ls-files \
    --format '%(objectmode) %(objectname) %(path)' \
    -- \
    devtools \
  | awk "/^120000 ${objectname} / { print \$3 }"
}

while [ "${1:-}" ]; do
  case "$1" in
    --bindir)
      shift
      bin_dir="$1"
      ;;
    --python)
      shift
      python_version="$1"
      ;;
    *)
      log_fatal "Unknown parameter: $1"
      ;;
    esac
    shift
done

if [ -z "$git" ] || [ ! -x "$git" ]; then
  log_fatal "git not found."
elif [ ! -e .git ]; then
  log_fatal "This is not a git working copy."
fi

# split up $PATH into $path array
IFS=: read -r -a path <<< "$PATH"

if [ ! "${bin_dir:+x}" ]; then
  # find first writable directory in $path
  for p in "${path[@]}"; do
    if [ -d "$p" ] && [ -w "$p" ]; then
      bin_dir="$p"
      break
    fi
  done
  if [ "${bin_dir:+x}" ]; then
    log_info "Will place symlinks into ${bin_dir}"
  else
    log_fatal "Could not discover a writable directory in \$PATH.\n" \
      "Consider the --bindir parameter."
  fi
fi

if [ ! -d "${bin_dir}" ]; then
  log_fatal "${bin_dir} is not a directory" >&2
elif [ ! -w "${bin_dir}" ]; then
  log_fatal "Directory ${bin_dir} is not writable."
fi


# find symlinks to wrapper in devtools
tools=()
for link in $(find_wrapper_links); do
  tools+=("$(basename "$link")")
done

if [ -e "${install_dir}" ]; then
  if [ -r "${install_dir}/Pipfile" ]; then
    log_info "Cleaning up old installation"
    pushd "${install_dir}" >/dev/null
    pipenv --bare --rm || :
    popd >/dev/null
    find "${install_dir}" -mindepth 1 -delete
  else
    log_fatal "${install_dir} exists, but does not contain Pipfile."
  fi
else
  mkdir -p "${install_dir}"
fi

log_info "Installing into ${install_dir}"
"${git}" archive --format=tar HEAD:devtools/pip-tools \
  | tar xCf "${install_dir}" -
pushd "${install_dir}" >/dev/null
pipenv_opts=(--bare)
if [ "${python_version:+x}" ]; then
  pipenv_opts+=("--python=${python_version}")
fi
pipenv "${pipenv_opts[@]}" sync
popd >/dev/null

for tool in "${tools[@]}"; do
  if [ -L "${bin_dir}/${tool}" ]; then
    if [ "$(readlink "${bin_dir}/${tool}")" = "${install_dir}/pipenv-wrapper.sh" ]; then
      log_info "Symlink for ${tool} is up to date."
      continue
    else
      log_fatal "Non-matching symlink for ${tool} in ${bin_dir}"
    fi
  elif [ -e "${bin_dir}/${tool}" ]; then
    log_fatal "Non-symlink ${tool} in ${bin_dir}"
  fi
  log_info "Installing symlink for ${tool}"
  ln --symbolic "${install_dir}/pipenv-wrapper.sh" "${bin_dir}/${tool}"
done
