#!/usr/bin/python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2024 Bareos GmbH & Co. KG
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


import logging
import re

from argparse import ArgumentParser, Namespace, ArgumentTypeError
from os import environ, chdir
from pprint import pprint
from sys import stdout, stderr
from time import sleep
from io import StringIO

from git import Repo
import git.exc

from changelog_utils import (
    file_has_pr_entry,
    add_entry_to_file,
    update_links,
    guess_section,
)

from check_sources.main import main_program as check_sources
from . import backport
from .github import Gh


def positive_int(val):
    intval = int(val)
    if intval <= 0:
        raise ArgumentTypeError(f"'{val}' is not a positive integer")
    return intval


class Mark:
    PASS = " ✓ "
    INFO = " ➜ "
    FAIL = " ✗ "

    _RED = "\x1b[91m"
    _CYAN = "\x1b[96m"
    _GREEN = "\x1b[92m"
    _ENDC = "\x1b[0m"

    @classmethod
    def enable_colors(cls):
        cls.PASS = cls._decorate(cls.PASS, cls._GREEN)
        cls.INFO = cls._decorate(cls.INFO, cls._CYAN)
        cls.FAIL = cls._decorate(cls.FAIL, cls._RED)

    @classmethod
    def _decorate(cls, text, color):
        return "{}{}{}".format(color, text, cls._ENDC)


class CheckSources:
    class ScreenManagerMock:
        class Updatable:
            def update(self, *args, **kwargs):
                pass

        def status_bar(self, *args, **kwargs):
            return self.Updatable()

        def counter(self, *args, **kwargs):
            return self.Updatable()

        def stop(self):
            pass

    def __init__(self, repo, merge_base):
        log_buffer = StringIO()

        # emulate commandline options
        args = Namespace()
        args.plugin = []
        args.ignore_file = ".bareos-check-sources-ignore"
        args.diff = False
        args.modify = False
        args.since = merge_base
        args.since_merge = None
        args.all = False
        args.untracked = False

        logging.debug("Running bareos-check-sources")
        self.status = check_sources(
            args=args,
            screen_manager=self.ScreenManagerMock(),
            log_file=log_buffer,
            repo=repo,
        )
        self.log_lines = log_buffer.getvalue().split("\n")
        logging.debug(
            "Running bareos-check-sources finished with return code {}".format(
                self.status
            )
        )

    def ok(self):
        return self.status == 0

    def issues(self):
        return self.log_lines


class CheckmarkAnalyzer:
    pattern = re.compile(r"\[([ x])\] +(.*)$", re.MULTILINE)

    def __init__(self, text):
        matches = self.pattern.findall(text)
        self.checked = 0
        self.unchecked = 0
        self.tasks_open = []
        self.tasks_done = []
        for mark, descr in matches:
            if mark == "x":
                self.checked += 1
                self.tasks_done.append(descr)
            else:
                self.unchecked += 1
                self.tasks_open.append(descr)
        self.total = self.checked + self.unchecked

    def completed(self):
        return self.unchecked == 0

    def printOpenTasks(self):
        for task in self.tasks_open:
            print("- {}".format(task))


class CommitAnalyzer:
    headline_pattern = re.compile(r"(fixup|\[[^\]]*\])")

    def __init__(self, commits):
        self.is_ok = True
        self.commit_issues = []

        for commit in commits:
            if not self.check_commit(commit):
                self.is_ok = False

    def check_commit(self, commit):
        headline, *messageBody = commit.message.split("\n")
        issues = []
        if len(messageBody) == 0:
            if not "(POEditor.com)" in commit.message:
                issues.append("missing newline at end of headline")
        elif messageBody[0] == "":
            messageBody.pop(0)
        else:
            issues.append("missing empty line after headline")

        issues.extend(self._check_headline(headline))
        issues.extend(self._check_body("\n".join(messageBody)))

        if len(issues) > 0:
            self._record_issues(commit, headline, issues)
            return False
        else:
            return True

    @classmethod
    def _check_headline(cls, text):
        issues = []
        # we encourage to use no more than 50 chars, but still accept up to 60
        if len(text) > 60:
            issues.append("headline too long")
        res = cls.headline_pattern.match(text)
        if res:
            issues.append("headline starts with '{}'".format(res.group(0)))
        return issues

    @staticmethod
    def _check_body(text):
        for line in text.split("\n"):
            lowerline = line.lower()
            if lowerline.startswith("co-authored-by:") or line.startswith(
                "signed-off-by:"
            ):
                continue
            if len(line) > 72:
                return ["body contains line longer 72 chars"]
        return []

    def _record_issues(self, commit, headline, issues):
        commit_descr = "{:9.9} {}".format(str(commit), headline)
        for issue in issues:
            self.commit_issues.append("{}: {}".format(commit_descr, issue))

    def ok(self):
        return self.is_ok

    def issues(self):
        return self.commit_issues


class Checklist:
    def __init__(self):
        self.is_ok = True

    def ok(self, text):
        print("{} {}".format(Mark.PASS, text))

    def info(self, text):
        print("{} {}".format(Mark.INFO, text))

    def fail(self, text):
        self.is_ok = False
        print("{} {}".format(Mark.FAIL, text))

    def softfail(self, text):
        print("{} [optional] {}".format(Mark.FAIL, text))

    def check(self, condition, ok_str, fail_str=None, optional=False):
        if not fail_str:
            fail_str = ok_str
        if condition:
            self.ok(ok_str)
        elif optional:
            self.softfail(fail_str)
        else:
            self.fail(fail_str)

    def all_checks_ok(self):
        return self.is_ok


def check_merge_prereq(repo, pr, ignore_status_checks=False):
    cl = Checklist()

    cl.check(
        pr["state"] == "OPEN", "PR state is {}\n\t{}".format(pr["state"], pr["url"])
    )

    cl.check(
        pr["headRefOid"] == repo.head.commit.hexsha,
        "Local and PR head commit match",
        "Local and PR head commit are different",
    )

    cl.check(pr["mergeable"] != "CONFLICTING", "PR is mergeable", "PR has conflict")

    cl.check(not pr["isDraft"], "PR is not a draft", "PR is a draft")

    if pr["reviewDecision"] == "APPROVED":
        cl.ok("Changed were approved")
    elif not pr["reviewDecision"]:
        cl.fail("Not reviewed and no review requested")
    elif pr["reviewDecision"] == "REVIEW_REQUIRED":
        cl.fail("Needs review")
    elif pr["reviewDecision"] == "CHANGES_REQUESTED":
        cl.fail("Changes requested during review have not yet been addressed")
    else:
        cl.fail("Unhandled reviewDecision: {}".format(pr["reviewDecision"]))

    cma = CheckmarkAnalyzer(pr["body"])
    cl.check(
        cma.completed(),
        "No open tasks",
        "{} of {} tasks open".format(cma.unchecked, cma.total),
    )

    cl.check(not repo.is_dirty(), "No dirty files", "Repository has dirty files")

    if not ignore_status_checks:
        for status_check in pr["statusCheckRollup"]:
            optional = (
                status_check["context"] != "continuous-integration/jenkins/pr-merge"
            )
            cl.check(
                status_check["state"] == "SUCCESS",
                "Status check '{context}': {state}\n\t{targetUrl}".format(
                    **status_check
                ),
                "Status check '{context}': {state}\n\t{targetUrl}".format(
                    **status_check
                ),
                optional,
            )
        cl.check(
            have_status_context(
                "continuous-integration/jenkins/pr-merge", pr["statusCheckRollup"]
            ),
            "Required status check present",
            "Required status check missing",
        )

    commit_iter = pr["_repo"].iter_commits("{}..HEAD".format(pr["_merge_base"]))
    commitRes = CommitAnalyzer(commit_iter)
    cl.check(
        commitRes.ok(),
        "Commit checks passed",
        "Commit checks failed:\n\t{}".format("\n\t".join(commitRes.issues())),
    )

    checkSourcesRes = CheckSources(pr["_repo"], pr["_merge_base"])
    cl.check(
        checkSourcesRes.ok(),
        "bareos-check-sources --since={} reported no problems".format(
            pr["_merge_base"]
        ),
        "bareos-check-sources --since={} reported:\n\t{}".format(
            pr["_merge_base"], "\n\t".join(checkSourcesRes.issues())
        ),
    )

    return cl.all_checks_ok()


def have_status_context(context, status_check_rollup):
    for status_check in status_check_rollup:
        if status_check["context"] == context:
            return True
    return False


def check_changelog_entry(repo, pr):
    changelog = "{}/CHANGELOG.md".format(repo.working_tree_dir)
    with open(changelog, "r") as fp:
        return file_has_pr_entry(fp, pr["number"])


def get_plain_label_list(label_list):
    return [x["name"].lower() for x in label_list]


def get_changelog_section(pr):
    labels = get_plain_label_list(pr["labels"])
    if "documentation" in labels:
        return "Documentation"
    if "bugfix" in labels:
        return "Fixed"
    if "removal" in labels:
        return "Removed"
    if "feature" in labels:
        return "Added"
    return guess_section(pr["title"])


def add_changelog_entry(repo, pr):
    if pr["isCrossRepository"] and not pr["maintainerCanModify"]:
        print(
            "Not adding Changelog to cross-repository PR that denies"
            + " maintainer edit (because we won't be able to push)."
        )
        return False
    changelog = "{}/CHANGELOG.md".format(repo.working_tree_dir)
    section = get_changelog_section(pr)
    with open(changelog, "r+") as fp:
        if not add_entry_to_file(fp, pr["title"], pr=pr["number"], section=section):
            return False
        update_links(fp)
    repo.git.add("CHANGELOG.md")
    repo.git.commit("-m", "Update CHANGELOG.md")
    return True


def parse_cmdline_args():
    parser = ArgumentParser()

    parser.add_argument(
        "--directory",
        "--dir",
        default=".",
        metavar="<path>",
        help="path to local git repository",
    )
    parser.add_argument(
        "--skip-sanity-checks",
        action="store_true",
        help="skip checking for safe environment",
    )
    log_group = parser.add_mutually_exclusive_group()
    log_group.add_argument("--debug", "-d", action="store_true")
    log_group.add_argument("--verbose", "-v", action="store_true")
    color_group = parser.add_mutually_exclusive_group()
    color_group.add_argument(
        "--color", action="store_true", help="enable colors (default on TTY)"
    )
    color_group.add_argument("--no-color", action="store_true", help="disable colors")

    subparsers = parser.add_subparsers(dest="subcommand")
    check_parser = subparsers.add_parser("check")
    check_parser.add_argument(
        "--ignore-status-checks",
        action="store_true",
        help="ignore (required) github status checks",
    )
    changelog_parser = subparsers.add_parser("add-changelog")
    merge_parser = subparsers.add_parser("merge")
    merge_parser.add_argument(
        "--skip-merge",
        action="store_true",
        help="do everything except the final call to 'gh pr merge'",
    )
    merge_parser.add_argument(
        "--admin-override",
        action="store_true",
        help="use --admin when running 'gh pr merge'",
    )
    merge_parser.add_argument(
        "--ignore-status-checks",
        action="store_true",
        help="ignore (required) github status checks",
    )
    dump_parser = subparsers.add_parser("dump")
    backport_parser = subparsers.add_parser("backport")
    backport_sp = backport_parser.add_subparsers(dest="bp_mode")
    backport_create_parser = backport_sp.add_parser("create")
    backport_create_parser.add_argument(
        "pr",
        metavar="<pr number>",
        help="GitHub PR number of the PR to backport from",
        type=positive_int,
    )
    backport_create_parser.add_argument(
        "--into",
        metavar="<base_branch>",
        help="Branch into which the backport should be done (defaults to current branch)",
        type=str,
    )
    backport_create_parser.add_argument(
        "--all", action="store_true", help="pick all commits"
    )
    backport_cherrypick_parser = backport_sp.add_parser("cherry-pick")
    backport_cherrypick_parser.add_argument(
        "--reset", action="store_true", help="reset before cherry-pick"
    )
    backport_cherrypick_parser.add_argument(
        "--all", action="store_true", help="pick all commits"
    )
    backport_publish_parser = backport_sp.add_parser("publish")
    backport_publish_parser.add_argument(
        "--dry-run", "-n", action="store_true", help="no push and no pr create"
    )

    args = parser.parse_args()

    if not args.no_color and stdout.isatty() or args.color:
        Mark.enable_colors()

    if not args.subcommand:
        parser.print_help()
        exit(2)
    return args


def check_merge_is_possible(repo, pr):
    wanted_oid = repo.head.commit.hexsha
    while wanted_oid != pr["headRefOid"] or pr["mergeable"] == "UNKNOWN":
        sleep(0.5)
        pr = get_current_pr_data()
    return pr["mergeable"] == "MERGEABLE"


def do_github_merge(repo, pr, *, skip_merge=False, admin_override=False):
    if admin_override:
        admin_param = "--admin"
    else:
        admin_param = None

    if skip_merge:
        print("Merge would run the following gh commandline:")
        gh = Gh(dryrun=True)
    else:
        gh = Gh()
    gh.pr.merge(
        admin_param,
        "--merge",
        "--delete-branch",
        match_head_commit=repo.head.commit.hexsha,
        subject="Merge pull request #{}".format(pr["number"]),
    )
    return True


def merge_pr(
    repo, pr, *, skip_merge=False, admin_override=False, ignore_status_checks=False
):
    print("Checking merge prerequisites:")
    if not check_merge_prereq(repo, pr, ignore_status_checks):
        print("Unmet requirements. Will not merge!")
        return False
    print()

    original_commit = repo.head.commit
    try:
        if check_changelog_entry(repo, pr):
            print("Keeping existing changelog entry.")
        else:
            print("Adding changelog entry")
            if not add_changelog_entry(repo, pr):
                raise Exception("Adding changelog failed")
            repo.git.push()

        if check_merge_is_possible(repo, pr):
            return do_github_merge(
                repo, pr, skip_merge=skip_merge, admin_override=admin_override
            )

        else:
            base_branch = pr["_base_branch"]
            print(
                "Resetting to {} and rebasing onto {}".format(
                    original_commit.hexsha, base_branch
                )
            )

            # we rollback to the commit before we added the changelog
            # as this was mergeable, the rebase should work flawlessly
            repo.head.commit = original_commit
            repo.head.reset(index=True, working_tree=True)

            try:
                repo.git.rebase(base_branch)
            except git.exc.GitCommandError as e:
                print("Automatic rebase interrupted, aborting rebase")
                repo.git.rebase("--abort")
                raise e

            print("Adding changelog entry again")
            if not add_changelog_entry(repo, pr):
                raise Exception("Adding changelog failed")
            repo.git.push("-f")

        if check_merge_is_possible(repo, pr):
            return do_github_merge(
                repo, pr, skip_merge=skip_merge, admin_override=admin_override
            )
        else:
            raise Exception("PR merge still not possible after rebase.")

    except Exception as e:
        print("Exception caught!")
        print("Rolling back to commit {}".format(original_commit.hexsha))
        print("Propagating exception")
        repo.head.commit = original_commit
        repo.head.reset(index=True, working_tree=True)
        repo.git.push("-f")  # we might have removed a commit, so we need to force-push
        raise e


def handle_ret(ret):
    if ret:
        exit(0)
    else:
        exit(1)


def get_current_pr_data():
    logging.debug("Getting pull request data from GitHub")
    return Gh().pr.view(
        json=[
            "body",
            "baseRefName",
            "commits",
            "headRefOid",
            "isCrossRepository",
            "isDraft",
            "labels",
            "maintainerCanModify",
            "mergeable",
            "number",
            "reviewDecision",
            "state",
            "statusCheckRollup",
            "title",
            "url",
        ]
    )


def get_remote_ref(branch, owner="{owner}", repo="{repo}"):
    return Gh().api(
        "repos/{owner}/{repo}/git/refs/heads/{branch}".format(
            owner=owner, repo=repo, branch=branch
        )
    )


def repo_up_to_date(repo, pr_data, remote_ref):
    local_head = repo.commit(pr_data["_base_branch"])
    remote_head = remote_ref["object"]["sha"]
    return str(local_head) == str(remote_head)


def setup_logging(*, verbose, debug):
    logging.basicConfig(format="%(levelname)s: %(name)s: %(message)s", stream=stderr)

    if verbose:
        logging.getLogger().setLevel(logging.INFO)

    if debug:
        logging.getLogger().setLevel(logging.DEBUG)
        # disable debug logging for git commands
        logging.getLogger("git.cmd").setLevel(logging.INFO)


def find_gh_remote(repo):
    """determine the git remote that matches the github repository gh will
    operate on, the so-called base repository"""

    # as the configuration would allow you to have multiple repositories with
    # this parameter set, we will not terminate the loop until we're sure we
    # didn't find two origins...
    gh_remote = None
    for remote in repo.remotes:
        if (
            remote.config_reader.has_option("gh-resolved")
            and remote.config_reader.get_value("gh-resolved") == "base"
        ):
            if gh_remote:
                raise foo
            gh_remote = remote

    return gh_remote


def main():
    args = parse_cmdline_args()
    setup_logging(verbose=args.verbose, debug=args.debug)

    if not args.skip_sanity_checks:
        if "GH_HOST" in environ or "GH_REPO" in environ:
            logging.critical(
                "cannot work correctly with GH_HOST or GH_REPO env variables set"
            )
            return 2

    chdir(args.directory)
    try:
        repo = Repo(".", search_parent_directories=True)
    except git.exc.InvalidGitRepositoryError:
        logging.critical("not a git repository")
        return 2

    git_remote = find_gh_remote(repo)
    if not git_remote:
        logging.critical("Could not find the remote that is used by gh.")
        return 2
    logging.info("Using git remote '{}'".format(git_remote))

    if args.subcommand == "backport":
        if repo.is_dirty():
            logging.critical("working copy is not clean")
            return 2
        if args.bp_mode == "cherry-pick":
            if backport.cherry_pick(
                repo=repo,
                select_all=args.all,
                allow_reset=args.reset,
                upstream=git_remote,
            ):
                return 0
            else:
                return 2
        elif args.bp_mode == "create":
            target_branch = backport.resolve_target_branch(repo, git_remote, args.into)
            if target_branch:
                if backport.create(
                    pr=args.pr,
                    into=target_branch,
                    repo=repo,
                    upstream=git_remote,
                    select_all=args.all,
                ):
                    return 0
                else:
                    return 1
        elif args.bp_mode == "publish":
            if backport.publish(repo=repo, dry_run=args.dry_run):
                return 0
            else:
                return 1
        return 2

    pr_data = get_current_pr_data()
    pr_data["_repo"] = repo
    pr_data["_base_branch"] = "{}/{}".format(git_remote, pr_data["baseRefName"])

    pr_data["_merge_base"] = repo.merge_base(
        repo.head.commit, repo.commit(pr_data["_base_branch"])
    )[0]
    logging.info("Merge-base is {}".format(pr_data["_merge_base"]))

    remote_ref = get_remote_ref(pr_data["baseRefName"])

    if not repo_up_to_date(repo, pr_data, remote_ref):
        logging.warning(
            "You repository is out of date. Please run 'git fetch {}'".format(
                git_remote
            )
        )
        if args.subcommand == "merge":
            logging.critical(
                "Merge will probably fail with an out-of-date repository, exiting"
            )
            return 2

    if args.subcommand == "check":
        ret = check_merge_prereq(
            repo, pr_data, ignore_status_checks=args.ignore_status_checks
        )
        if check_changelog_entry(repo, pr_data):
            print("{} ChangeLog record present".format(Mark.PASS))
        elif not pr_data["isCrossRepository"] or pr_data["maintainerCanModify"]:
            section = get_changelog_section(pr_data)
            print(
                "{} ChangeLog record WILL be automatically added to section '{}' during merge".format(
                    Mark.INFO, section
                )
            )
        else:
            print(
                "{} ChangeLog record cannot be added, author denies access".format(
                    Mark.FAIL
                )
            )
            ret = False

        handle_ret(ret)
    elif args.subcommand == "add-changelog":
        if check_changelog_entry(repo, pr_data):
            print("Already have Changelog for this PR")
            exit(1)
        handle_ret(add_changelog_entry(repo, pr_data))
    elif args.subcommand == "merge":
        ret = merge_pr(
            repo,
            pr_data,
            skip_merge=args.skip_merge,
            admin_override=args.admin_override,
            ignore_status_checks=args.ignore_status_checks,
        )
        if ret:
            print("{} Merged pull request.".format(Mark.PASS))
        else:
            print("{} Pull request not merged.".format(Mark.FAIL))
        handle_ret(ret)
    elif args.subcommand == "dump":
        pprint(pr_data)
