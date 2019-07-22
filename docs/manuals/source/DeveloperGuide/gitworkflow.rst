.. _git-workflow:

Git Workflow
============

Getting new code or changes into Bareos
---------------------------------------
All new code should be targeted at our master branch.
There are no exceptions, so even if you fix a bug in an ancient release, you fix it in master.
You can then backport your changes to an older release.
The Git workflow we use is `Github Flow`_.

Basically this means we want you to create pull requests against our master branch.
We will then build and review the code, request changes as required and eventually merge the pull request.
You may need to rebase and force-push during the review-phase as master is a moving target.

.. _Github Flow: https://help.github.com/en/articles/github-flow

Releases and Backporting
------------------------
The individual releases are tagged with their version number.
Each major release has its own release-branch.
Backporting a change into an already release major release is done by applying the change to the major release's branch.
As with the master branch changes are only accepted as pull requests.
If you backport a change into a major release, you must make sure it has also been backported into every newer major release.
So when you backport a change into bareos-17.2, you have to backport it into bareos-18.2 first.

The best practice workflow for this is as follows:

#. `git checkout <major-release-branch>`
#. `git checkout -b backport-xyz`
#. Apply your changes using `git cherry-pick -x` or `git am`
#. `git push -u origin HEAD`
#. open pull request

Feature Branches
----------------
Branches that stage changes like features and bugfixes are considered topic branches and should be short-lived.
If you merged your topic branch into master (or another release branch while backporting), that branch is then obsolete and should be removed.

Commits
-------
A commit should be a wrapper for related changes.
For example, fixing two different bugs should produce two separate commits.
Small commits make it easier for other developers to understand the changes and roll them back if something went wrong.

Begin your message with a short summary of your changes (up to 50 characters as a guideline).
Separate it from the following body by including a blank line.

The body of your message should provide detailed answers to the following questions:

* What was the motivation for the change?
* How does it differ from the previous implementation?

Use the imperative, present tense ("change", not "changed" or "changes") to be consistent with generated messages from commands like git merge.

Commit message guideline
~~~~~~~~~~~~~~~~~~~~~~~~
Start with a short (<= 50 characters) summary on a single line, followed by an empty line.
If your commit changes a specific component of bareos try to mention it at the start of the summary.

If your commit fixes or affects an existing bug, add a single line in the format ``Fixes #12345: The exact title of the bug you fix.``, followed by another empty line.

You can now continue with a detailed commit message.
It should be wrapped at 72 chars and can consist of multiple paragraphs separated by empty lines.
You should write in imperative: "Fix bug" and not "Fixed bug" or "Fixes bug."

::

  core: sample commit for docs

  Fixes #1234: Documentation requires a sample commit

  This patch adds a sample commit message to the documentation that was
  previously missing.

  While this does not fix an actual bug, the commit is required so we can
  show some of the best practices for commit messages. This message
  applies at least the following practices:
  - short summary line up to 50 characters
  - bugfix info for the bugtracker
  - imperative language
  - hard limit of 72 characters in the long description
