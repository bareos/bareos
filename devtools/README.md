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

This script does most of the hard work when releasing Bareos.
It is used to prepare and tag the release-commit and a following base-commit for the ongoing work on the branch.

Make sure that
- You are on the branch you want to release

> For a major release you should be on the release-branch, not the master
branch. While you can move around branch pointers later, it is a lot
easier to branch first.
- You have no uncommited changes

Running `prepare-release.sh [<version>]` will do the following:

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

Verify the documentation external links by calling:
* sphinx-build -M linkcheck docs/manuals/source out/ -j2
or (outside the docbuild container):
* make docs-check-urls

If you decide not to push, nothing will be released.

#### Update Changelog

---

### Build

#### Build and Test with sanitize

#### Build RPM

#### Build Tarball

## `prepare-release.sh`


## `update-changelog-links.sh`

Script to find common link references in CHANGELOG.md (i.e. versions and bugs) and add the correct reference to the end of the file. Should be called after adding a link reference to CHANGELOG.md, but will not hurt if called too often.

## `new-changelog-release.sh`

Update the changelog when a release is made. Will either add a new "Unreleased" chapter in the changelog or replace an existing one with a provided version number and release-date.

## tools in pip-tools

There are several tools in the pip-tools directory that have frontends installed here. See the pip-tools directory for more details.
