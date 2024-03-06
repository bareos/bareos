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
this module provides a wrapper to the GitHub CLI "gh"
"""

from os import environ
from subprocess import run, PIPE, DEVNULL
from platform import system
import json
import yaml


def get_config_dir():
    if "GH_CONFIG_DIR" in environ:
        return environ["GH_CONFIG_DIR"]
    if "XDG_CONFIG_HOME" in environ:
        return environ["XDG_CONFIG_HOME"] + "/gh"
    if system() == "Windows" and "AppData" in environ:
        return environ["AppData"] + "/GitHub CLI"
    return environ["HOME"] + "/.config/gh"


def whoami():
    with open(get_config_dir() + "/hosts.yml", "rb") as fp:
        host_cfg = yaml.safe_load(fp)
        try:
            return host_cfg["github.com"]["user"]
        except KeyError:
            return None


class InvokationError(Exception):
    def __init__(self, result):
        self.result = result
        super().__init__(
            f"invocation of {result.args} returned {result.returncode}\nMessage:\n{result.stderr}"
        )


class Gh:
    """thin and simple proxy wrapper around github cli"""

    @staticmethod
    def make_option_str(k, v):
        param = k.replace("_", "-")
        if isinstance(v, list):
            values = ",".join(v)
            return f"--{param}={values}"
        if v:
            return f"--{param}={v}"
        return f"--{param}"

    def __init__(self, *, cmd=None, dryrun=False):
        environ["NO_COLOR"] = "1"
        environ["GH_NO_UPDATE_NOTIFIER"] = "1"
        environ["GH_PROMPT_DISABLED"] = "1"
        self.cmd = cmd if cmd is not None else ["gh"]
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
            return ""
        res = run(
            self.cmd + opts + params,
            stdin=DEVNULL,
            stdout=PIPE,
            stderr=PIPE,
            encoding="UTF-8",
            check=False,
        )

        if res.returncode != 0:
            raise InvokationError(res)

        if "json" in kwargs or len(self.cmd) >= 2 and self.cmd[1] == "api":
            return json.loads(res.stdout)

        return res.stdout
