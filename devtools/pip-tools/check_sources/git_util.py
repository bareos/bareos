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

import logging
import gitdb.exc
from pathlib import Path

logger = logging.getLogger(__name__)


class NoCommonAncestor(Exception):
    pass


class NoMergeCommit(Exception):
    pass


class MultipleCommonAncestors(Exception):
    pass


class FileHistory:
    def __init__(self, repo):
        self.repo = repo
        self.file_commits = None
        self.changed_files = None

    def load_changed_files(self):
        if self.changed_files is not None:
            return True
        self.changed_files = set(
            (
                [Path(i.b_path) for i in self.repo.index.diff(None)]
                + [Path(i.b_path) for i in self.repo.index.diff(self.repo.head.commit)]
                + [Path(f) for f in self.repo.untracked_files]
            )
        )

    def load_file_commits(self, ignore=[]):
        if self.file_commits is not None:
            return True

        res = self.repo.git.log("--raw", "--pretty=+%H")
        file_commits = {}
        for line in res.splitlines():
            if line == "":
                continue
            if line[0] == ":":
                if commit is not None:
                    file_path = Path(line.split("\t")[-1])
                    if not file_path in file_commits:
                        file_commits[file_path] = []
                    file_commits[file_path].append(commit)
            elif line[0] == "+":
                commit = self.repo.commit(line[1:])
                if commit in ignore:
                    commit = None

        self.file_commits = file_commits

    def is_changed(self, file_path):
        self.load_changed_files()
        return file_path in self.changed_files

    def get_commits_for(self, file_path, ignore=[]):
        # we pass ignore here which is a bug: subsequent calls will always use
        # the same ignore list as the first call
        self.load_file_commits(ignore)

        if file_path in self.file_commits:
            return self.file_commits[file_path]

        # fallback to the old per-file approach
        # needed for files that have been renamed
        res = self.repo.git.log(
            "--pretty=format:%H", "--follow", "--remove-empty", "--", file_path
        )
        return list(
            filter(
                lambda x: x not in ignore,
                [self.repo.commit(x) for x in res.splitlines()],
            )
        )

    def get_latest_commit(self, file_path, ignore=[]):
        try:
            return self.get_commits_for(file_path, ignore)[0]
        except IndexError:
            logger.warning("No commits for {}".format(file_path))
            return None


def commit_to_human(commit):
    short_hash = str(commit)[0:7]
    subject = commit.message.split("\n")[0]
    return "{} {}".format(short_hash, subject)


def commit_to_short(commit):
    return str(commit)[0:7]


def find_common_ancestor(commit_a, commit_b):
    if commit_a.repo != commit_b.repo:
        raise NoCommonAncestor()

    merge_bases = commit_a.repo.merge_base(commit_a, commit_b)
    if len(merge_bases) == 0:
        raise NoCommonAncestor()
    elif len(merge_bases) > 1:
        raise MultipleCommonAncestors()
    else:
        return merge_bases[0]


def find_last_merge(commit):
    while len(commit.parents) == 1:
        commit = commit.parents[0]
    if len(commit.parents) == 0:
        raise NoMergeCommit
    return commit


def get_filelist(repo, *, all=False, since=None, changed=True, untracked=False):
    def list_paths(root_tree, path=Path(".")):
        for blob in root_tree.blobs:
            yield path / blob.name
        for tree in root_tree.trees:
            yield from list_paths(tree, path / tree.name)

    files = set()

    if all:
        files.update(list_paths(repo.head.commit.tree, Path(".")))

    if since:
        files.update([Path(item.a_path) for item in repo.index.diff(since)])

    if changed:
        files.update([Path(item.a_path) for item in repo.index.diff(repo.head.commit)])
        files.update([Path(item.a_path) for item in repo.index.diff(None)])

    if untracked:
        files.update([Path(f) for f in repo.untracked_files])

    file_list = list(files)
    file_list.sort()
    return file_list


def get_conf_option_value(repo, section, option, default=None):
    from configparser import NoSectionError, NoOptionError

    val = default
    for conf in "repository", "global":
        try:
            val = repo.config_reader(conf).get_value("commit", "gpgsigxn")
        except (NoSectionError, NoOptionError):
            continue
        break
    return val


def load_ignorelist(ignore_file):
    ignore_list = []
    try:
        with ignore_file.open() as infile:
            for line in infile.readlines():
                line = line.strip()
                if len(line) == 0 or line.startswith("#"):
                    continue

                ignore_list.append(line)
        logger.debug(
            "Loaded {} ignore-pattern(s) from {}".format(len(ignore_list), ignore_file)
        )
    except FileNotFoundError:
        logger.debug("Ignore file {} not found.".format(ignore_file))
        pass
    return ignore_list


def load_ignore_blame_revs(repo):
    ignore_file = Path(repo.working_tree_dir) / Path(
        get_conf_option_value(repo, "blame", "ignoreRevsFile", ".git-blame-ignore-revs")
    )
    ignorelist = []
    for commit_hash in load_ignorelist(ignore_file):
        try:
            ignorelist.append(repo.commit(commit_hash))
            logger.debug("Added commit {} to ignore list".format(commit_hash))
        except gitdb.exc.BadName:
            logger.warning(
                "Will not add non-existant commit {} to ignore list".format(commit_hash)
            )
    return ignorelist
