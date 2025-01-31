# Bareos Developer Tools

This directory contains tools to work with the Bareos sourcecode. As a user, you will probably never need them.

## Tool Installation

### Pip Tools
1. Navigate to `devtools/`
2. Run `install-pip-tools.sh` (you probably need root privileges!)
3. Yeah! You should now be able to run the pip-tools from every directory
4. Test the correct installation with `bareos-check-sources` from a directory outside of `devtools/`
---

### PHP CS Fixer (for WebUI)

## Tool Usage

### Check Sources
Make sure that
- Pip-Tools are properly installed
- You are inside a git repo

Run `bareos-check-sources` to check your uncommited files for the following requirements:
* clang-format
* cmake-format
* python black
* trailing spaces
* trailing newlines
* dos line-endings
* missing copyright notices
* copyright notices with wrong end-year
* merge conflicts
* executables have a shebang
* python executables have a sane shebang
* check C/C++ header guards
* check/fix comments wasting space

Pass the following options
- `--since-merge` to check all commited and uncommited changes since last merged
- `--diff` to  print the difference of your source and the requested formatting
- `--modify` to automatically apply all requested changes to the checked files

#### Bareos Check Sources

#### Format PHP source (for WebUI)

---

### PR Tool
The PR Tool provides various workflows to simplify PR development, merging or backporting.

#### Check
Check which merge requirements of your PR are met.

Make sure that
- Pip-Tools are properly installed
- You are inside a git repo
- Your local branchs upstream branch is part of a Pull Request

```shell
pr-tool check 
```
checks the following requirements:
- PR state is OPEN
- Local and PR head commit match
- PR is mergeable
- PR is not a draft
- All Changes requested during review have been addressed
- All checkboxes in the PR description are ticked
- Repository has no dirty files (uncommitted changes)
- Commit format checks pass
- Bareos Check Sources (`--since-merge`) suceeds


#### Add Changelog

#### Update-license

#### Merge

#### Dump

#### Backport
Create a backport branch and PR based of an existing PR.

Make sure that
- Pip-Tools are properly installed
- You are inside a git repo
- Your local branchs upstream branch is part of a Pull Request

Doing a backport:
1. Checkout your git to the branch you want to backport to (e.g. `bareos-24`)
2. `pr-tool backport create <pr-number>` the PR you want to backport
3. Check if everything is fine with the new created backport branch
4. `pr-tool backport publish` to create a PR with your newly created backport branch

---

### Release

#### Prepare Release

#### Update Changelog

---

### Build

#### Build and Test with sanitize

#### Build RPM

#### Build Tarball

## `prepare-release.sh`

This script does most of the hard work when releasing Bareos. It is used to prepare and tag the release-commit and a following base-commit for the ongoing work on the branch.

## `update-changelog-links.sh`

Script to find common link references in CHANGELOG.md (i.e. versions and bugs) and add the correct reference to the end of the file. Should be called after adding a link reference to CHANGELOG.md, but will not hurt if called too often.

## `new-changelog-release.sh`

Update the changelog when a release is made. Will either add a new "Unreleased" chapter in the changelog or replace an existing one with a provided version number and release-date.

## tools in pip-tools

There are several tools in the pip-tools directory that have frontends installed here. See the pip-tools directory for more details.
