#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2015-2024 Bareos GmbH & Co. KG
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
Handle file paths.
"""


class Path(object):
    """
    Class to handle file paths.
    """

    def __init__(self, path=None):
        """\

        Args:
           path (str, optional): string representation of the file system path.
        """
        self.__set_defaults()
        self.set_path(path)

    def __str__(self):
        result = ""
        if self.is_root():
            result += "/"
        result += "/".join(self.path)
        if (not self.is_root()) or self.len() > 0:
            if self.is_directory():
                result += "/"
        return result

    def __set_defaults(self):
        self.path_orig = None
        self.root = False
        self.directory = False
        self.path = None

    def set_path(self, path):
        if path is None:
            self.__set_defaults()
        elif isinstance(path, str):
            self.path_orig = path
            components = self.path_orig.split("/")
            self.path = [i for i in components if i != ""]
            if path == "":
                self.root = False
                self.directory = True
            else:
                self.root = False
                if self.path_orig[0] == "/":
                    self.root = True
                self.directory = False
                if components[-1] == "":
                    self.directory = True
        else:
            # exception
            pass

    def get(self, index=None):
        if index is None:
            return self.path
        return self.path[index]

    def shift(self):
        """
        Removes the first component of the path.

        Example:

           .. code:: python

              >>> path = Path("/usr/bin/python")
              >>> path.shift()
              'usr'
              >>> print(path)
              /bin/python

        Returns:
            str: First component of the path.

        Raises:
            IndexError: if path can't be shifted (path is empty).
        """
        result = self.get(0)
        self.remove(0)
        return result

    def is_directory(self):
        return self.directory

    def is_root(self):
        return self.root

    def remove(self, index):
        del self.path[index]

    def len(self):
        return len(self.path)
