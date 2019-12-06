Automatic Version Generation
============================
This section describes how version numbers for Releases and Prereleases are determined. 
Generally they are generated based on Git metadata.
It covers what prerequisites must be met for this process to work and describes ways to bypass it.

What is this about?
-------------------
When we develop Bareos we usually build and test each and every commit.
To be able to distinguish the built versions from each other, it helps if each and every of these builds has its own version number.
As we do have lots of metadata in git using this is the obvious choice.

How does it work?
-----------------
Git contains the great ``git describe`` command that can generate a unique version number based on a previous tag.
We use this command and some regexp parsing to generate a version number for each commit.
Whenever you run ``cmake`` to configure your source tree for building there is a CMake module that will try to determine the Bareos version and fail if it cannot determine the version information.
There are three methods that will be tried in order to determine the version number (VERSION_STRING) and the timestamp of the commit (VERSION_TIMESTAMP).

CMake Variable
~~~~~~~~~~~~~~
You can set VERSION_STRING to the version string you want to use when calling CMake.
This will override the VERSION_STRING determined from git, but not the VERSION_TIMESTAMP.

BareosVersion.cmake
~~~~~~~~~~~~~~~~~~~
CMake will try to load the module BareosVersion.
If loading succeeds it will use the version information provided by that module.
CMake expects the module to set VERSION_STRING and VERSION_TIMESTAMP to the version and the timestamp.

This method is for situations when there is no git metadata or no git client available.
Usually this happens when you obtain the sources with a tarball.

.. _section-generating-bareosversion:

Generating BareosVersion.cmake
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
You can generate the BareosVersion module using the command ``cmake -P write_version_files.cmake`` from a git checkout.
A file called :file:`BareosVersion.cmake` will then be written to :file:`core/cmake` and :file:`webui/cmake`.

Git Metadata
~~~~~~~~~~~~
If there is no BareosVersion module, CMake will try to call git to determine the required version information.
First it runs ``git log -1 --pretty=format:%ct`` to determine the timestamp of the current committer date.
This will be the value for VERSION_TIMESTAMP.

To determine the version number CMake will run ``git describe --tags --exact-match --match "Release/*" --dirty=.dirty`` and see if that command returns a parseable version number.
If that is the case, this is a release build and VERSION_STRING is set to the parsed version.
Otherwise CMake will run ``git describe --tags --match "WIP/*" --dirty=.dirty``.
This generates a version number based on a work-in-progress tag and sets VERSION_STRING accordingly.
Such a version will contain the relative distance (in number of commits) from the work-in-progress tag and the git hash.

Troubleshooting
---------------
If CMake fails and complains it cannot determine version information something has gone wrong.
You can test using ``cmake -P get_version.cmake`` in the top directory.
As soon as this returns a version number, everything should work fine.

The following sections describe some of the common pitfalls.

No git
~~~~~~
If you don't have ``git`` installed automatically determining the version does not work.
In this case it is probably a good idea to just install ``git``.

Not a git repository
~~~~~~~~~~~~~~~~~~~~
If the directory you're trying to build from is not a git repository and does not contain :file:`BareosVersion.cmake` in :file:`core/cmake` and :file:`core/webui`, then something in the process of retrieving the sources was flawed.
If you retrieved the sources as a tarball, that tarball was not generated correctly.
Right now only release-tags will yield a usable tarball with ``git archive``.
If you want to build unreleased content, you should retrieve the sources by cloning the git repository.

Git repository missing tags
~~~~~~~~~~~~~~~~~~~~~~~~~~~
In some rare cases it might be possible that you cloned your Bareos sources from a repository where the work-in-progress tags are missing or you simply skipped fetching the tags.
This will make the version generation fail.
You can check what tags you have using ``git tag -l``. For all work-in-progress tags use ``git tag -l 'WIP/*'``.

If you just cloned without tags, you can fix this with ``git fetch --tags``.
If you have cloned from a fork that is missing the tags you will need to add the Bareos GitHub repository as a remote and fetch the tags from there.
You can do this by ``git remote add upstream https://github.com/bareos/bareos.git`` followed by ``git fetch --tags upstream``.

Your repository should now have the required tags.
You can verify this with ``git tag -l 'WIP/*'`` and ``cmake -P get_version.cmake`` in the top directory of the repository.

Building in the wrong directory
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
If you try to run ``cmake`` in :file:`core/` or :file:`webui/` it will not be able to determine the version using git.
Either build in the top directory or :ref:`generate BareosVersion.cmake <subsubsection-generating-bareosversion>` so information is not retrieved from git.

Last resort
~~~~~~~~~~~
If all else fails, you can manually generate :file:`BareosVersion.cmake` that sets VERSION_STRING and VERSION_TIMESTAMP and copy it to :file:`core/cmake` and :file:`webui/cmake`.

.. code-block:: cmake
    :caption: cmake/BareosVersion.cmake

    set(VERSION_STRING "12.3.4")
    set(VERSION_TIMESTAMP "1234567890")


The value in VERSION_TIMESTAMP should be seconds since the epoch (1970-01-01 00:00:00 UTC) and can be generated using ``date +%s``.
When generated automatically this will match the time of the latest commit.

When you need to fallback to this, either you or we did something horribly wrong.
Please consider opening a bug report or describe what you were doing on the bareos-users mailinglist, so we can figure out how to make it work for you.
