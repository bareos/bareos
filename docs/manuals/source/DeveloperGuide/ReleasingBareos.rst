Releasing Bareos
================

This chapter describes how to release a new version of Bareos.
The documentation is limited to the changes that need to be done to the sources and the git repository.
Building and distributing source and binary packages is not the scope of this chapter.

.. note::

   Consider this a step-by-step manual. The sections are meant to be carried out in that order.

Deciding for a version number
-----------------------------
When you release you will usually need at least two version numbers: the version you're going to release and the version that will follow after that version.
For details on the numbering-scheme, take a look at :ref:`section-version-numbers`.

Usually, you will have an existing work-in-progress tag that generates pre-release version numbers for your branch.
So the version you're going to release is probably the final version for that pre-release.

Preparing the release notes
---------------------------
In :sinceVersion:`20.0.0: CHANGELOG.md` release notes will be kept in :file:`CHANGELOG.md`.
The "Unreleased" section in that file will automatically become the release notes and a new "Unreleased" section will be added afterwards.

How does it work in detail?
~~~~~~~~~~~~~~~~~~~~~~~~~~~
When you run :file:`devtools/prepare-release.sh` the "Unreleased" section will be renamed to the current release by :file:`devtools/new-changelog-release.sh`.
All links to pull requests, issues, etc. will be automatically updated by :file:`devtools/update-changelog-links.sh`.
After the release has been made :file:`devtools/new-changelog-release.sh` will be called again to add the new "Unreleased" section that will be added to the commit for the work-in-progress tag.

Update version-dependent files
------------------------------
There are version-dependent files in the Bareos sources that might need your attention.

version.map.in
~~~~~~~~~~~~~~
The file :file:`core/src/cats/ddl/version.map.in` must be updated
whenever the database schema version changes.
Usually the developer who did a schema change should have done this.
However, please double-check and add or update the version mapping if needed.

Prepare the git commits and tags
--------------------------------
There is a helper script :file:`devtools/prepare-release.sh` that will help with the process.
You can just call the script and it will handle the version-dependent things that need to happen for the release.
The script will also prepare all git commits that are required to release the new version and set the correct git tags.

.. important::

   All changes are only done to you local git repository. For pushing these changes see :ref:`publishing-the-release`.


Special considerations for major versions
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Whenever you release a new major version you will be releasing more or less the master branch.

Before branching, add the new major release to :file:`SECURITY.md`.
You should also mark no longer supported versions as such.

To allow the splitting of the development (i.e. continue working on master, but allowing to work on the newly released version, too) you will have to carry out a few additional steps.

.. important::
   Do not release from the master branch.

Before running :file:`devtools/prepare-release.sh` you should create the new release-branch and switch to it using ``git checkout -b bareos-X.Y``.
When you now run :file:`prepare-release.sh` it will only generate a new WIP-tag for your branch and nothing for the master branch.
Also the release of the major version itself will not be visible on the master branch.

To make the release visible on the master branch, you can just forward the branch pointer to the parent of the new WIP-tag.
You can do so using ``git checkout master`` followed by ``git reset --soft <wip-tag>~`` (notice the ~).

In :sinceVersion:`20.0.0: CHANGELOG.md` release notes are only per major-release.
All sections referencing a previous release should be removed from :file:`CHANGELOG.md` before continuing work on the master branch.

In :file:`docs/manuals/source/conf.py` on the master branch the setting for ``scv_root_ref`` and ``scv_banner_main_ref`` should point to the latest (i.e. your newly created) release-branch, so users will see the latest released documentation by default.
You may also need to change ``scv_whitelist_branches`` so the release-branch will be picked up when building the documentation.

The commit that contains the above changes should then be tagged as the new WIP-tag on the master branch.
For example ``git add CHANGELOG.md docs/manuals/source/conf.py`` followed by ``git commit -m 'Start development of X.Y.Z'``.
That commit can now be tagged with a new WIP-tag using ``git tag WIP/X.Y.Z-pre``.

.. _publishing-the-release:

Publishing the release
----------------------
To actually publish the release you push the commits and tags created earlier to GitHub.
After you have reviewed the commits and tags that have been set in the previous release and made sure all branch pointers point to the right places (please double- and triple-check this) and you're on the correct branch, you can push the changes to GitHub.

**For the release branch:** First push the branch ``git push <remote>``, then push the release-tag ``git push <remote> <release-tag>`` and if applicable push the WIP-tag ``git push <remote> <WIP-tag>``.

**For the master:** If this is a new major release you also need to push master and the new WIP-tag for master.

Updating GitHub Release
-----------------------
Pushing a tag to GitHub will implicitly create a release on the `project's list of releases <https://github.com/bareos/bareos/releases/>`_.
The release information there is incomplete and should be updated.

Go to the list described above, select your release-tag and press "Edit tag".
In the form enter "Release X.Y.Z" for "Release title" and add the URL of the release notes to "Describe this release".
If you're releasing a pre-release (anything with a tilde in the version number) check the "This is a pre-release" box.
Apply the changes by pressing "Save".
