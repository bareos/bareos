#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2024 Bareos GmbH & Co. KG
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

import git.exc
import logging
from datetime import datetime
import os

from argparse import ArgumentParser
from git import Repo
from pathlib import Path
from sys import stderr

from . import git_util
from .diff_util import print_diff
from .file_util import is_valid_textfile, get_stemmed_file_path, file_is_ignored
from .plugins import copyright_plugin

logger = logging.getLogger(__name__)


def parse_cmdline_args():
    parser = ArgumentParser()
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="increase verbosity"
    )
    parser.add_argument(
        "--debug", "-d", action="store_true", help="enable debug logging"
    )
    parser.add_argument(
        "--diff-only",
        action="store_true",
        help="print diffs instead of changing the file",
    )
    parser.add_argument(
        "--ignore-file",
        default=".bareos-check-sources-ignore",
        metavar=("<ignore-file>"),
        help="ignore patterns listed in <ignore-file> "
        + "(default: .bareos-check-sources-ignore)",
    )
    parser.add_argument(
        "filename", nargs="+", help="the files to which we add a header"
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


class HeaderAdder:
    __FIRST_PART = ["BAREOS® - Backup Archiving REcovery Open Sourced", ""]
    __SECOND_PART = [
        "",
        "This program is Free Software; you can redistribute it and/or",
        "modify it under the terms of version three of the GNU Affero General Public",
        "License as published by the Free Software Foundation and included",
        "in the file LICENSE.",
        "",
        "This program is distributed in the hope that it will be useful, but",
        "WITHOUT ANY WARRANTY; without even the implied warranty of",
        "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU",
        "Affero General Public License for more details.",
        "",
        "You should have received a copy of the GNU Affero General Public License",
        "along with this program; if not, write to the Free Software",
        "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA",
        "02110-1301, USA.",
    ]

    def __init__(self, *, lines_before=[], lines_after=[], line_prefix=""):
        self.lines_before = lines_before
        self.lines_after = lines_after
        self.line_prefix = line_prefix
        self.first_part = self.lines_before + list(
            map(self.add_prefix, self.__FIRST_PART)
        )
        self.second_part = (
            list(map(self.add_prefix, self.__SECOND_PART)) + self.lines_after
        )

    def __call__(self, *, file_path, file_content, first_date, last_date):
        in_lines = file_content.split("\n")
        out_lines = []

        def fuzz_append(add_lines):
            """add lines in add_lines if they are not already present in in_lines.

            will move in_lines to out_lines if lines already exist.
            """
            len_ = len(add_lines)
            if self.fuzz_compare_lines(add_lines, in_lines[0:len_]):
                for i in range(len(add_lines)):
                    out_lines.append(in_lines.pop(0))
            else:
                for line in add_lines:
                    if line.strip() == "":
                        out_lines.append("")
                    else:
                        out_lines.append(line.rstrip())

        if file_content[0:2] == "#!":
            out_lines.append(in_lines.pop(0))

        fuzz_append(self.first_part)

        # duplicate existing copyright lines
        while self.strip(in_lines[0]).startswith("Copyright"):
            out_lines.append(in_lines.pop(0))

        # append our own copyright line
        out_lines.append(
            "{}Copyright (C) {}-{} Bareos GmbH & Co. KG".format(
                self.line_prefix, first_date.year, last_date.year
            )
        )

        fuzz_append(self.second_part)

        for line in in_lines:
            out_lines.append(line)
        return "\n".join(out_lines)

    def add_prefix(self, str_):
        return self.line_prefix + str_

    def strip(self, str_):
        stripped_prefix = self.line_prefix.strip()
        if str_.startswith(stripped_prefix):
            len_ = len(stripped_prefix)
            str_ = str_[len_:]
        return str_.strip()

    def fuzz_compare_lines(self, lines_a, lines_b):
        def not_empty(str_):
            return len(str_) > 0

        lines_a = list(filter(not_empty, map(self.strip, lines_a)))
        lines_b = list(filter(not_empty, map(self.strip, lines_b)))

        if len(lines_a) != len(lines_b):
            return False

        for i in range(len(lines_a)):
            if lines_a[i] != lines_b[i]:
                return False
        return True


add_cpp_header = HeaderAdder(lines_before=["/*"], lines_after=["*/"], line_prefix="   ")
add_script_header = HeaderAdder(lines_after=[""], line_prefix="#   ")


def add_copyright_header(*, file_path, **kwargs):
    sfp = get_stemmed_file_path(file_path)

    if sfp.match("*.[ch]") or sfp.match("*.cc"):
        func = add_cpp_header
    else:
        func = add_script_header

    return func(file_path=file_path, **kwargs)


def main_program(args, file_name):
    setup_logging(verbose=args.verbose, debug=args.debug)

    original_file_path = Path(file_name).resolve()

    if not is_valid_textfile(original_file_path, []):
        logger.error("File {} can not be handled.".format(original_file_path))
        return 1

    try:
        repo = Repo(original_file_path.parent, search_parent_directories=True)
    except git.exc.InvalidGitRepositoryError:
        logger.error("{} is not in a git repository.".format(original_file_path.parent))
        return 2
    logger.info("Using git repository {}".format(repo.working_tree_dir))
    prev_cwd = os.getcwd()
    os.chdir(repo.working_tree_dir)
    if original_file_path.is_absolute():
        file_path = original_file_path.relative_to(Path(repo.working_tree_dir))
    else:
        file_path = original_file_path

    file_ignorelist = git_util.load_ignorelist(Path(args.ignore_file))
    if file_is_ignored(file_path, file_ignorelist):
        logger.error("File {} is on ignorelist.".format(file_path))
        return 1

    try:
        with file_path.open("r", encoding="utf-8", newline="") as infile:
            file_content = infile.read()
    except UnicodeDecodeError:
        logger.error("File {} is not valid UTF-8.".format(file_path))
        return 1

    # check if header exists
    if copyright_plugin.check_copyright_notice(
        file_path=file_path, file_content=file_content
    ):
        logger.error("File {} already contains a copyright notice.".format(file_path))
        return 3

    file_history = git_util.FileHistory(repo)
    blame_ignore_revs = git_util.load_ignore_blame_revs(repo)

    if file_history.is_changed(file_path):
        first_date = datetime.now()
        last_date = datetime.now()
    else:
        commits = file_history.get_commits_for(file_path, blame_ignore_revs)
        if commits:
            first_date = datetime.utcfromtimestamp(commits[-1].committed_date)
            last_date = datetime.utcfromtimestamp(commits[0].committed_date)
        else:
            logger.error("No commit found for {}".format(file_path))
            return 4

    modified_content = add_copyright_header(
        file_path=file_path,
        file_content=file_content,
        first_date=first_date,
        last_date=last_date,
    )
    if modified_content == file_content:
        logger.error("No changes made to file.")
        return 4

    if args.diff_only:
        print_diff(
            file_content,
            modified_content,
            file_path=file_path,
            plugin_name="add copyright header",
        )
    else:
        with file_path.open("w", encoding="utf-8", newline="") as outfile:
            outfile.write(modified_content)
        logger.info("Changed file {}".format(file_path))
    os.chdir(prev_cwd)
    return 0


def main():
    args = parse_cmdline_args()
    try:
        for f in args.filename:
            res = main_program(args, f)
            if res != 0:
                exit(res)
        exit(0)
    except KeyboardInterrupt:
        exit(130)
