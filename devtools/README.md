# Bareos Developer Tools

This directory contains tools to work with the Bareos sourcecode. As a user, you will probably never need them.

## `prepare-release.sh`

This script does most of the hard work when releasing Bareos. It is used to prepare and tag the release-commit and a following base-commit for the ongoing work on the branch.

## `update-changelog-links.sh`

Script to find common link references in CHANGELOG.md (i.e. versions and bugs) and add the correct reference to the end of the file. Should be called after adding a link reference to CHANGELOG.md, but will not hurt if called too often.

## `new-changelog-release.sh`

Update the changelog when a release is made. Will either add a new "Unreleased" chapter in the changelog or replace an existing one with a provided version number and release-date.
