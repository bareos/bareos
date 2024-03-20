.. _git-workflow:

Git Workflow
============

Getting new code or changes into Bareos
---------------------------------------
All new code should be targeted at our master branch.
There are no exceptions, so even if you fix a bug in an ancient release,
you fix it in master.
You can then backport your changes to an older release.
The Git workflow we use is `Github Flow`_.

Basically this means we want you to create pull requests (PR) against our master branch.
The pull request title will later be used as a line to the CHANGELOG file.
We will then build and review the code,
request changes as required and eventually merge the pull request.

During the review-phase
only additional commits and normal-pushes (avoid force pushes) should be used
if not otherwise agreed with the reviewer.
After the pull request is approved, a final rebase and squashing of commits can and should be done.

Also note, that a conversation started by a reviewer should also be resolved by the reviewer.

To summarize:

* You: create a PR.

  * The PR title should be usable as CHANGELOG entry.

* Bareos Dev team: will assign a reviewer to the PR.
* Reviewer: review phase:

  * until Reviewer is satisfied:

    * Reviewer: reviews the PR, make suggestions (with comments, sometimes with additional commits).
    * You: address the suggestions by additional commits (or comments).

* When the reviewer is satisfied with the changes, often a final step is required:

  * You: rebase the git branch and cleanup (squash) the commits.
  * You (or the Reviewer): create a separate commit for the CHANGELOG entry, see :ref:`section-Changelog`.

* Reviewer: approves the PR and someone from Bareos will merge it.


.. _Github Flow: https://docs.github.com/en/get-started/using-github/github-flow

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

Commit message guideline
~~~~~~~~~~~~~~~~~~~~~~~~
Start with a short (<= 50 characters) summary on a single line, followed by an empty line.
If your commit changes a specific component of bareos try to mention it at the start of the summary.
You should write the summary in imperative mood: "Fix bug" and not "Fixed bug" or "Fixes bug."

If your commit fixes or affects an existing bug, add a single line in the format ``Fixes #12345: The exact title of the bug you fix.``, followed by another empty line.

You can now continue with a detailed commit message.
It should be wrapped at 72 chars and can consist of multiple paragraphs separated by empty lines.

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

Sometimes during development, you may want to refactor code (without changing overall software behaviour) either to accommodate the new (functional) changes that follow or just for cleanliness.

Refactoring is always welcome but can sometimes require extra care and attention from reviewers to ensure those changes do not actually break current behaviour.

If you mix refactoring and actual functional changes (goal of your PR), it may become a change soup and make it difficult for the reviewer and lengthen the review process even more.

Make sure you differentiate refactoring changes and functional changes (goals of the PR), refactoring commits should be prepended with either ``refactoring:`` or ``refactor:`` to make it even easier for reviewers to spot.

Refactoring commits should be lined up together either at the start of the branch or at the end.

A branch commit tree would look something like this:
::

    * systemtests: update related tests
    * docs: update documentation
    * Fix: filed: fix a bug that makes xxx break
    * dird: stored: my new fancy feature
    * refactor: backup.cc: replace `goto` statements
    * refactor: stored: extract blabla code to its own function
    * refactor: msgchan.cc: remove unnecessary pool memory variables
    * refactor: msgchan.cc: change variable names



.. _section-Changelog:

CHANGELOG
---------

Each pull-request (PR) should add an entry in the https://github.com/bareos/bareos/blob/master/CHANGELOG.md file of your branch.
The title of the PR should be usable as the CHANGELOG entry.

The entry in the CHANGELOG.md file should

* be in a separate commit.
* refers to the PR.
* be added at the bottom of the relevant section.

The commit message should be: ``update CHANGELOG.md``

Normally the CHANGELOG commit will be added as the last step in the review process to minimize the number of conflicts.
