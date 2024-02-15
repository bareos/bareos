#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2023-2024 Bareos GmbH & Co. KG
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

"""
this module provides commands to do a backport of a specific
bareos PR into another branch
"""

import logging
from os import environ
from sys import stdout, stderr
from subprocess import run
from tempfile import NamedTemporaryFile
from importlib import resources
from git import RemoteReference
from git.exc import GitCommandError
from .github import Gh, InvokationError, whoami

logger = logging.getLogger("backport")


class UnmanagedBranchException(Exception):
    pass


def git_editor(repo, file):
    if "GIT_EDITOR" in environ:
        editor = environ["GIT_EDITOR"]
    else:
        editor = repo.git.var("GIT_EDITOR")
    res = run([editor, file], check=False)
    return res.returncode == 0


def find_user_remote(repo, username):
    expected_url = f"git@github.com:{username}/bareos.git"
    for remote in repo.remotes:
        remote_urls = list(remote.urls)
        if len(remote_urls) > 1:
            logger.warning("ignoring remote '%s' with multiple urls", remote.name)
            continue
        if expected_url == remote_urls[0]:
            return remote
    return None


def get_branch_by_name(repo, name):
    for branch in repo.branches:
        if branch.name == name:
            return branch
    return None


def resolve_target_branch(repo, git_remote, branch_name):
    if branch_name is not None:
        local_branch = get_branch_by_name(repo, branch_name)
        if not local_branch:
            logger.error("Branch %s not found", branch_name)
            return None
    else:
        try:
            local_branch = repo.active_branch
        except TypeError as e:
            logger.error(e)
            return None

    remote_branch = local_branch.tracking_branch()
    if remote_branch is None:
        logger.error("%s does not track a remote branch", local_branch.name)
        logger.error(
            "You can change the tracking branch using 'git branch --set-upstream-to %s/<target-branch>'",
            git_remote,
        )
        return None
    if remote_branch.remote_name != git_remote.name:
        logger.error(
            "%s does not track a branch on remote %s", local_branch.name, git_remote
        )
        logger.error(
            "You can change the tracking branch using 'git branch --set-upstream-to %s/<target-branch>'",
            git_remote,
        )
        return None
    logger.info("Remote branch is '%s'", remote_branch.remote_head)
    return remote_branch.remote_head


def get_pr_metadata(*, repo):
    cfg = repo.head.reference.config_reader()
    if not cfg.has_option("bareos-original-pr") or not cfg.has_option(
        "bareos-backport-into"
    ):
        raise UnmanagedBranchException(
            f"Branch {repo.head.reference.name} is not a managed backport branch."
        )
    pr = cfg.get_value("bareos-original-pr")
    into = cfg.get_value("bareos-backport-into")
    return (pr, into)


def cherry_pick(*, repo, upstream, select_all=False, allow_reset=False):
    try:
        original_pr, base_branch_name = get_pr_metadata(repo=repo)
    except UnmanagedBranchException as e:
        logger.critical(e)
        return False

    upstream.fetch([base_branch_name, f"refs/pull/{original_pr}/head"])

    base_commit = upstream.refs[base_branch_name]
    if repo.head.commit != base_commit:
        if allow_reset:
            logger.info("Resetting HEAD to %s", base_commit)
            repo.head.reset(base_commit, working_tree=True, index=True)
        else:
            logger.critical(
                "Your HEAD does not match %s. Either rerun with --reset or reset manually first.",
                base_commit,
            )
            return False

    pr_data = Gh().pr.view(str(original_pr), json=["title", "commits"])
    return cherry_pick_impl(
        repo=repo,
        original_pr=original_pr,
        commits=pr_data["commits"],
        title=pr_data["title"],
        select_all=select_all,
    )


def generate_commit_lines(original_pr, title, commits):
    lines = [
        f"# Backport of #{original_pr}: {title}\n",
        "# List all commits to cherry-pick in order, remove commits you do not want.\n",
    ]
    for c in commits:
        if c["messageHeadline"] == "Update CHANGELOG.md":
            lines.append(f"# {c['oid'][0:9]} {c['messageHeadline']}\n")
        else:
            lines.append(f"{c['oid'][0:9]} {c['messageHeadline']}\n")
    return lines


def parse_commit_lines(lines):
    commit_lines = [
        l.lstrip().partition(" ")
        for l in lines
        if not l.strip().startswith("#") and l.strip() != ""
    ]
    todo = []
    for commit, space, descr in commit_lines:
        if space != " " or not descr.endswith("\n"):
            logger.critical("Cannot parse '%s%s%s', abort.", commit, space, descr)
            return False
        todo.append(commit)
    return todo


def cherry_pick_impl(*, repo, original_pr, commits, title, select_all=False):
    lines = generate_commit_lines(original_pr, title, commits)

    if not select_all:
        with NamedTemporaryFile("w+") as fp:
            fp.writelines(lines)
            fp.flush()
            fp.seek(0)
            if not git_editor(repo, fp.name):
                logger.critical("Editor returned non-zero. Ignoring selection.")
                return False
            lines = fp.readlines()

    todo = parse_commit_lines(lines)
    if not todo:
        return False
    if len(todo) == 0:
        logger.info("No commits selected. Nothing to do.")
        return True

    logger.debug("Canceling any ongoing cherry-pick.")
    repo.git.cherry_pick("--quit")
    logger.info("Cherry-picking commits %s", todo)
    try:
        res = repo.git.cherry_pick("-x", todo)
        print(res)
    except GitCommandError as e:
        # this is a bit weird, but we cannot really pass through the output
        # as cherry-pick when it fails usually writes to stdout first and
        # stderr later, we print the captured output in that order.
        # sadly, GitCommandError wraps its stdout/stderr members weirdly,
        # so we extract things from args
        print(e.args[3].decode(errors="replace"), file=stdout)
        print(e.args[2].decode(errors="replace"), file=stderr)
        return False
    return True


def create(*, pr, into, repo, upstream, select_all=False):
    # determine which remote the user's fork is on
    origin = find_user_remote(repo, whoami())
    if not origin:
        logger.critical("Cannot find a git remote for your GitHub fork")
        return False

    pr_data = get_pr_info(str(pr), ["title", "labels", "headRefName", "commits"])
    if not pr_data:
        logger.critical("Cannot get information for PR %s. Does it exist?", pr)
        return False

    short_headref_name = pr_data["headRefName"].split("/")[-1]
    short_baseref_name = into
    working_branch_name = f"backport/{short_baseref_name}/{short_headref_name}"
    if get_branch_by_name(repo, working_branch_name):
        logger.critical("Branch '%s' already exists.", working_branch_name)
        return False

    # make sure we have the required branches and commits up-to-date
    upstream.fetch([into, f"refs/pull/{pr}/head"])

    working_branch = repo.create_head(working_branch_name, commit=into)

    # save pr-number and destination branch to git configuration so we use it later
    with working_branch.config_writer() as cfg:
        cfg.set_value("bareos-original-pr", pr)
        cfg.set_value("bareos-backport-into", into)

    working_branch.checkout()

    # configure upstream so we can push
    origin_ref = RemoteReference(
        repo, f"refs/remotes/{origin.name}/{working_branch.name}"
    )
    working_branch.set_tracking_branch(origin_ref)

    return cherry_pick_impl(
        repo=repo,
        original_pr=pr,
        title=pr_data["title"],
        commits=pr_data["commits"],
        select_all=select_all,
    )


def get_pr_info(pr_spec, fields):
    try:
        data = Gh().pr.view(pr_spec, json=fields)
    except InvokationError as e:
        if "no pull requests found for branch" in e.args[0]:
            return None
        if "Could not resolve to a PullRequest with the number of" in e.args[0]:
            return None
        raise
    return data


def munge_labels(labels, base_branch=None):
    """convert github label list to plain list of label names.
    will also strip all "requires backport to" labels and add
    an "is a backport to" label for the base branch"""
    label_names = [
        l["name"] for l in labels if not l["name"].startswith("requires backport to")
    ]

    if base_branch and base_branch.startswith("bareos-"):
        label_names.append(f"is a backport to {base_branch[7:]}")
    return label_names


def check_branch_and_get_spec(*, repo, local_branch, dry_run):
    username = whoami()
    remote_branch = local_branch.tracking_branch()
    gh_branch_spec = f"{username}:{remote_branch.remote_head}"

    backport_data = get_pr_info(gh_branch_spec, ["number", "title", "url"])
    if backport_data:
        logger.critical(
            "This branch is already associated with PR #%s %s",
            backport_data["number"],
            backport_data["title"],
        )
        logger.critical("See %s", backport_data["url"])
        if not dry_run:
            return None

    if not remote_branch.is_valid():
        if dry_run:
            logger.warning("Remote branch is not valid. Not pushing during dry-run!")
        else:
            logger.warning("Remote branch is not valid. Pushing now!")
            remote = repo.remote(name=remote_branch.remote_name)
            remote.push(repo.head.ref)
    elif local_branch.commit != remote_branch.commit:
        logger.critical("Remote branch is not up-to-date. Did you forget to push?")
        if not dry_run:
            return None
    return gh_branch_spec


def _fill_template(in_, out_, vars_):
    for line in in_:
        out_.write(line.format(**vars_))
    out_.flush()


def publish(*, repo, dry_run=False):
    try:
        original_pr, base_branch = get_pr_metadata(repo=repo)
    except UnmanagedBranchException as e:
        logger.critical(e)
        return False

    gh_branch_spec = check_branch_and_get_spec(
        repo=repo, local_branch=repo.head.ref, dry_run=dry_run
    )
    if not gh_branch_spec and not dry_run:
        return False

    original_data = get_pr_info(str(original_pr), ["title", "labels"])
    if not original_data:
        logger.critical("Could not retrieve information for PR %s", original_pr)
        return False

    labels = munge_labels(original_data["labels"], base_branch)

    template_file = resources.files(__package__).joinpath("backport_pr_template.md")
    template_vars = {"original_pr": original_pr, "base_branch": base_branch}
    with template_file.open("r", encoding="utf-8") as template_fp:
        with NamedTemporaryFile("w+", suffix=".md") as body_fp:
            _fill_template(template_fp, body_fp, template_vars)
            if not git_editor(repo, body_fp.name):
                logger.critical("Editor returned non-zero. Abort.")
                return False
            new_pr = {
                "title": original_data["title"],
                "base": base_branch,
                "head": gh_branch_spec,
                "body-file": body_fp.name,
                "label": labels,
            }
            res = Gh(dryrun=dry_run).pr.create(**new_pr)
            print(res)
    return True
