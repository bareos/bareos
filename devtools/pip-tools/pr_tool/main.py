#!/usr/bin/python3
#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2022-2022 Bareos GmbH & Co. KG
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


import json
import logging
import re

from argparse import ArgumentParser
from git import Repo
import git.exc
from os import environ, chdir
from pprint import pprint
from subprocess import run, PIPE, DEVNULL
from sys import stderr
from time import sleep

from changelog_utils import file_has_pr_entry, add_entry_to_file, update_links

PASS_MARK = " ✓ "
INFO_MARK = "   "
FAIL_MARK = " ✗ "


class InvokationError(Exception):
    def __init__(self, result):
        self.result = result
        super().__init__(
            "invocation of {} returned {}\nMessage:\n{}".format(
                result.args, result.returncode, result.stderr
            )
        )


class Gh:
    """thin and simple proxy wrapper around github cli"""

    @staticmethod
    def make_option_str(k, v):
        param = k.replace("_", "-")
        if isinstance(v, list):
            return "--{}={}".format(param, ",".join(v))
        elif v:
            return "--{}={}".format(param, v)
        else:
            return "--{}".format(param)

    def __init__(self, *, cmd=["gh"], dryrun=False):
        environ["NO_COLOR"] = "1"
        environ["GH_NO_UPDATE_NOTIFIER"] = "1"
        environ["GH_PROMPT_DISABLED"] = "1"
        self.cmd = cmd
        self.dryrun = dryrun

    def __getattr__(self, name):
        """return another proxy object with the name added as additional
        positional parameter"""
        return Gh(cmd=self.cmd + [name], dryrun=self.dryrun)

    def __call__(self, *args, **kwargs):
        """invoke command, parse response if json and return"""
        # filter out args that evaluate to False (i.e. None or "")
        params = list(filter(lambda x: x, args))
        # convert kwargs to "--key-word=value"
        opts = [Gh.make_option_str(k, v) for (k, v) in kwargs.items()]
        if self.dryrun:
            print(self.cmd + opts + params)
        else:
            res = run(
                self.cmd + opts + params,
                stdin=DEVNULL,
                stdout=PIPE,
                stderr=PIPE,
                encoding="UTF-8",
            )

            if res.returncode != 0:
                raise InvokationError(res)

            if "json" in kwargs:
                return json.loads(res.stdout)

            return res.stdout


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


class Checklist:
    def __init__(self):
        self.is_ok = True

    def ok(self, text):
        print("{} {}".format(PASS_MARK, text))

    def info(self, text):
        print("{} {}".format(INFO_MARK, text))

    def fail(self, text):
        self.is_ok = False
        print("{} {}".format(FAIL_MARK, text))

    def check(self, condition, ok_str, fail_str=None):
        if not fail_str:
            fail_str = ok_str
        if condition:
            self.ok(ok_str)
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
            cl.check(
                status_check["state"] == "SUCCESS",
                "Status check '{context}': {state}\n\t{targetUrl}".format(
                    **status_check
                ),
            )

    return cl.all_checks_ok()


def check_changelog_entry(repo, pr):
    changelog = "{}/CHANGELOG.md".format(repo.working_tree_dir)
    with open(changelog, "r") as fp:
        return file_has_pr_entry(fp, pr["number"])


def add_changelog_entry(repo, pr):
    if pr["isCrossRepository"] and not pr["maintainerCanModify"]:
        print(
            "Not adding Changelog to cross-repository PR that denies"
            + " maintainer edit (because we won't be able to push)."
        )
        return False
    changelog = "{}/CHANGELOG.md".format(repo.working_tree_dir)
    with open(changelog, "r+") as fp:
        if not add_entry_to_file(fp, pr["title"], pr=pr["number"]):
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

    subparsers = parser.add_subparsers(dest="subcommand")
    check_parser = subparsers.add_parser("check")
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

    args = parser.parse_args()

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
    return gh.pr.merge(
        admin_param,
        "--merge",
        "--delete-branch",
        match_head_commit=repo.head.commit.hexsha,
        subject="Merge pull request #{}".format(pr["number"]),
    )


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
            base_branch = "origin/{}".format(pr["baseRefName"])
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
    return Gh().pr.view(
        json=[
            "body",
            "baseRefName",
            "headRefOid",
            "isCrossRepository",
            "isDraft",
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


def main():
    logging.basicConfig(format="%(levelname)s: %(name)s: %(message)s", stream=stderr)
    args = parse_cmdline_args()

    chdir(args.directory)
    try:
        repo = Repo(".", search_parent_directories=True)
    except git.exc.InvalidGitRepositoryError:
        print("not a git repository")
        return 2

    pr_data = get_current_pr_data()
    if args.subcommand == "check":
        ret = check_merge_prereq(repo, pr_data)
        if check_changelog_entry(repo, pr_data):
            print("{} ChangeLog record present".format(PASS_MARK))
        elif not pr_data["isCrossRepository"] or pr_data["maintainerCanModify"]:
            print("{} ChangeLog record can be added automatically".format(INFO_MARK))
        else:
            print(
                "{} ChangeLog record cannot be added, "
                + "author denies access".format(FAIL_MARK)
            )
            ret = False

        handle_ret(ret)
    elif args.subcommand == "add-changelog":
        if check_changelog_entry(repo, pr_data):
            print("Already have Changelog for this PR")
            exit(1)
        handle_ret(add_changelog_entry(repo, pr_data))
    elif args.subcommand == "merge":
        handle_ret(
            merge_pr(
                repo,
                pr_data,
                skip_merge=args.skip_merge,
                admin_override=args.admin_override,
                ignore_status_checks=args.ignore_status_checks,
            )
        )
    elif args.subcommand == "dump":
        pprint(pr_data)
