#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2022 Bareos GmbH & Co. KG
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

import enlighten
import git.exc
import gitdb.exc
import io
import logging
import os

from argparse import ArgumentParser
from git import Repo
from pathlib import Path
from sys import stderr

from . import git_util
from .diff_util import print_diff
from .file_util import is_valid_textfile, get_stemmed_file_path
from .plugins import load_plugins
from .registry import get_checkers, get_modifiers

logger = logging.getLogger(__name__)


def parse_cmdline_args():
    parser = ArgumentParser()
    subgroup = parser.add_mutually_exclusive_group()
    subgroup.add_argument(
        "--all", action="store_true", help="check all committed files"
    )
    subgroup.add_argument(
        "--since",
        default=None,
        metavar="<commit-ish>",
        help="check committed files changed since commitish",
    )
    subgroup.add_argument(
        "--since-merge",
        action="store_true",
        help="check committed files up to latest merge commit",
    )
    parser.add_argument(
        "--untracked", "-u", action="store_true", help="check untracked files"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="increase verbosity"
    )
    parser.add_argument(
        "--debug", "-d", action="store_true", help="enable debug logging"
    )
    parser.add_argument(
        "--modify", action="store_true", help="modify files instead of just reporting"
    )
    parser.add_argument(
        "--diff", action="store_true", help="print diffs for modifications"
    )
    parser.add_argument(
        "--directory",
        "--dir",
        default=".",
        metavar="<path>",
        help="path to local git repository",
    )
    parser.add_argument(
        "--plugin",
        action="append",
        metavar=("<plugin1> [...]"),
        help="use subset of plugins (default: all)",
    )
    parser.add_argument(
        "--ignore-file",
        default=".bareos-check-sources-ignore",
        metavar=("<ignore-file>"),
        help="ignore patterns listed in <ignore-file> "
        + "(default: .bareos-check-sources-ignore)",
    )

    return parser.parse_args()


def setup_logging(*, verbose, debug):
    logging.basicConfig(format="%(levelname)s: %(name)s: %(message)s", stream=stderr)

    if verbose:
        logging.getLogger().setLevel(logging.INFO)

    if debug:
        logging.getLogger().setLevel(logging.DEBUG)
        # disable debug logging for git commands
        logging.getLogger("git.cmd").setLevel(logging.INFO)
        # disable debug logging of black parser
        logging.getLogger("blib2to3.pgen2.driver").setLevel(logging.INFO)


def get_since_commit(since, repo):
    """find the common base commit of the current HEAD and provided since."""
    try:
        since_commit = repo.commit(since)
    except (gitdb.exc.BadName, ValueError):
        logger.error("{} does not resolve to a git commit".format(since))
        raise
    try:
        return git_util.find_common_ancestor(since_commit, repo.head.commit)
    except git_util.NoCommonAncestor:
        logger.error(
            "No common ancestor for the following commits:\n  {}\n  {}".format(
                git_util.commit_to_human(since_commit),
                git_util.commit_to_human(repo.head.commit),
            )
        )
        raise
    except git_util.MultipleCommonAncestors:
        logger.error(
            (
                "Multiple common ancestors for the following " "commits:\n  {}\n  {}"
            ).format(
                git_util.commit_to_human(since_commit),
                git_util.commit_to_human(repo.head.commit),
            )
        )
        raise


def get_commit_of_last_merge(repo):
    try:
        return git_util.find_last_merge(repo.head.commit)
    except git_util.NoMergeCommit:
        logger.error(
            "No merge commit in history of {}".format(
                git_util.commit_to_human(repo.head.commit)
            )
        )
        raise


class NoPluginsFound(Exception):
    pass


def get_plugins_for_file(file_path):
    stemmed_file_path = get_stemmed_file_path(file_path)

    checkers = get_checkers(stemmed_file_path)
    modifiers = get_modifiers(stemmed_file_path)

    if len(checkers) + len(modifiers) == 0:
        logger.debug("No plugins to handle {}".format(file_path))
        raise NoPluginsFound
    return (checkers, modifiers)


def main_program(*, args, screen_manager, log_file=stderr, repo=None):
    if not repo:
        try:
            repo = Repo(args.directory, search_parent_directories=True)
        except git.exc.InvalidGitRepositoryError:
            logger.error("{} is not in a git repository".format(args.directory))
            return 2
        logger.info("Using git repository {}".format(repo.working_tree_dir))
    os.chdir(repo.working_tree_dir)

    load_plugins(args.plugin)
    file_ignorelist = git_util.load_ignorelist(Path(args.ignore_file))

    try:
        if args.since:
            merge_base = get_since_commit(args.since, repo)
        elif args.since_merge:
            merge_base = git_util.find_last_merge(repo.head.commit)
        else:
            merge_base = None
    except Exception:
        return 2

    if merge_base:
        logger.info("Starting at '{}'".format(git_util.commit_to_human(merge_base)))

    files = git_util.get_filelist(
        repo, all=args.all, since=merge_base, untracked=args.untracked
    )

    status_bar = screen_manager.status_bar(
        status_format="file {file}{fill}plugin {plugin}",
        color="black_on_lightgray",
        file="none",
        plugin="none",
    )
    file_progress = screen_manager.counter(
        bar_format="{desc}{desc_pad}{percentage:3.0f}%|{bar}| "
        + "{count:{len_total}d}/{total:d} [{elapsed}]",
        color="green_on_lightgray",
        total=len(files),
        desc="analyzing",
    )

    plugin_args = {
        "git_repo": repo,
        "file_history": git_util.FileHistory(repo),
        "blame_ignore_revs": git_util.load_ignore_blame_revs(repo),
    }

    success = True
    for f in files:
        file_progress.update()

        if not is_valid_textfile(f, file_ignorelist):
            continue

        try:
            checkers, modifiers = get_plugins_for_file(f)
            with io.open(f, "r", encoding="utf-8", newline="") as infile:
                file_content = infile.read()
        except NoPluginsFound:
            continue
        except UnicodeDecodeError:
            logger.debug("Skipping non-utf-8 file {}".format(f))
            continue

        plugin_args["file_path"] = f
        plugin_args["file_content"] = file_content

        for name, plugin in checkers:
            status_bar.update(file=str(f), plugin=name)
            if plugin(**plugin_args):
                logger.debug("File '{}' passed check '{}'".format(f, name))
            else:
                print("File '{}' failed check '{}'".format(f, name), file=log_file)
                success = False

        modified = False
        for name, plugin in modifiers:
            status_bar.update(file=str(f), plugin=name)
            plugin_args["file_content"] = file_content
            modified_content = plugin(**plugin_args)
            if modified_content != file_content:
                modified = True
                if args.modify:
                    print("Plugin '{}' modified '{}'".format(name, f), file=log_file)
                else:
                    print(
                        "Plugin '{}' would modify '{}'".format(name, f), file=log_file
                    )
                    success = False

                if args.diff:
                    print_diff(
                        file_content, modified_content, file_path=f, plugin_name=name
                    )

                file_content = modified_content
            else:
                logger.debug("File '{}' NOT modified by '{}'".format(f, name))

        if modified and args.modify:
            with io.open(f, "w", encoding="utf-8", newline="") as outfile:
                outfile.write(modified_content)

    screen_manager.stop()
    return 0 if success else 1


def main():
    args = parse_cmdline_args()
    setup_logging(verbose=args.verbose, debug=args.debug)
    screen_manager = enlighten.get_manager()
    try:
        return main_program(args=args, screen_manager=screen_manager)
    except KeyboardInterrupt:
        exit(130)
