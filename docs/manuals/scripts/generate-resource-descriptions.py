#!/usr/bin/env python3
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2018-2024 Bareos GmbH & Co. KG
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


from __future__ import absolute_import, division, print_function, unicode_literals

import argparse
import logging
import json
import os
from pprint import pprint
import re
import sys


def touch(filename):
    open(filename, "a").close()


class daemonName:
    def __init__(self):
        table = {"Director": "Dir", "StorageDaemon": "Sd", "FileDaemon": "Fd"}

    @staticmethod
    def getLong(string):
        if string.lower() == "director" or string.lower() == "dir":
            return "Director"
        elif string.lower() == "storagedaemon" or string.lower() == "sd":
            return "StorageDaemon"
        elif string.lower() == "filedaemon" or string.lower() == "fd":
            return "FileDaemon"

    @staticmethod
    def getShort(string):
        if (
            string.lower() == "bareos-dir"
            or string.lower() == "director"
            or string.lower() == "dir"
        ):
            return "Dir"
        elif (
            string.lower() == "bareos-sd"
            or string.lower() == "storagedaemon"
            or string.lower() == "sd"
        ):
            return "Sd"
        elif (
            string.lower() == "bareos-fd"
            or string.lower() == "filedaemon"
            or string.lower() == "fd"
        ):
            return "Fd"
        elif (
            string.lower() == "bareos-console"
            or string.lower() == "bconsole"
            or string.lower() == "con"
        ):
            return "Console"
        elif string.lower() == "bareos-tray-monitor":
            return "Console"

    @staticmethod
    def getLowShort(string):
        return daemonName.getShort(string).lower()


class BareosConfigurationSchema:
    def __init__(self, json):
        self.format_version_min = 2
        self.logger = logging.getLogger()
        self.json = json
        try:
            self.format_version = json["format-version"]
        except KeyError as e:
            raise
        logger.info("format-version: " + str(self.format_version))
        if self.format_version < self.format_version_min:
            raise RuntimeError(
                "format-version is "
                + str(self.format_version)
                + ". Required: >= "
                + str(self.format_version_min)
            )

    def open(self, filename=None, mode="r"):
        if filename:
            self.out = open(filename, mode)
        else:
            self.out = sys.stdout

    def close(self):
        if self.out != sys.stdout:
            self.out.close()

    def getDaemons(self):
        return sorted(filter(None, self.json["resource"].keys()))

    def getDatatypes(self):
        try:
            return sorted(filter(None, self.json["datatype"].keys()))
        except KeyError:
            return

    def convertCamelCase2Spaces(self, valueCC):
        s1 = re.sub("([a-z0-9])([A-Z])", r"\1 \2", valueCC)
        result = []
        for token in s1.split(" "):
            u = token.upper()
            # TODO: add LAN
            if u in [
                "ACL",
                "CA",
                "CN",
                "DB",
                "DH",
                "FD",
                "LMDB",
                "NDMP",
                "PSK",
                "SD",
                "SSL",
                "TLS",
                "VSS",
            ]:
                token = u
            result.append(token)
        return " ".join(result)

    def isValidCamelCase(self, key):
        if key in ("EnableVSS", "AutoXFlateOnReplication"):
            return True
        return re.match("^([A-Z][a-z]+)+$", key) is not None

    def getDatatype(self, name):
        return self.json["datatype"][name]

    def getResources(self, daemon):
        return sorted(filter(None, self.json["resource"][daemon].keys()))

    def getResource(self, daemon, resourcename):
        return self.json["resource"][daemon][resourcename]

    def getDefaultValue(self, data):
        default = ""
        if data.get("default_value"):
            default = data.get("default_value")
            if data.get("platform_specific"):
                default += " (platform specific)"
        return default

    def getConvertedResources(self, daemon):
        result = ""
        for i in self.getResources(daemon):
            result += i + "\n"
        return result

    def getResourceDirectives(self, daemon, resourcename):
        return sorted(filter(None, self.getResource(daemon, resourcename).keys()))

    def getResourceDirective(self, daemon, resourcename, directive, deprecated=None):
        # TODO:
        # deprecated:
        #   None:  include deprecated
        #   False: exclude deprecated
        #   True:  only deprecated
        return BareosConfigurationSchemaDirective(
            self.json["resource"][daemon][resourcename][directive]
        )

    def getConvertedResourceDirectives(self, daemon, resourcename):
        # OVERWRITE
        return None

    def writeResourceDirectives(self, daemon, resourcename, filename=None):
        self.open(filename, "w")
        self.out.write(self.getConvertedResourceDirectives(daemon, resourcename))
        self.close()

    def getStringsWithModifiers(self, text, strings):
        strings["text"] = strings[text]
        if strings[text]:
            if strings.get("mo"):
                return "%(mo)s%(text)s%(mc)s" % (strings)
            else:
                return "%(text)s" % (strings)
        else:
            return ""


class BareosConfigurationSchemaDirective(dict):
    def getDefaultValue(self):
        default = None
        if dict.get(self, "default_value"):
            if (dict.get(self, "default_value") == "true") or (
                dict.get(self, "default_value") == "on"
            ):
                default = "yes"
            elif (dict.get(self, "default_value") == "false") or (
                dict.get(self, "default_value") == "off"
            ):
                default = "no"
            else:
                default = dict.get(self, "default_value")
        return default

    def getStartVersion(self):
        if dict.get(self, "versions"):
            version = dict.get(self, "versions").partition("-")[0]
            if version:
                return version

    def getEndVersion(self):
        if dict.get(self, "versions"):
            version = dict.get(self, "versions").partition("-")[2]
            if version:
                return version

    def get(self, key, default=None):
        if key == "default_value" or key == "default":
            return self.getDefaultValue()
        elif key == "start_version":
            if self.getStartVersion():
                return self.getStartVersion()
        elif key == "end_version":
            if self.getEndVersion():
                return self.getEndVersion()
        return dict.get(self, key, default)


class BareosConfigurationSchema2Sphinx(BareosConfigurationSchema):
    def indent(self, text, amount, ch=" "):
        padding = amount * ch
        return "".join(padding + line for line in text.splitlines(True))

    def getDefaultValue(self, data):
        default = ""
        if data.get("default_value"):
            default = data.get("default_value")
            if data.get("platform_specific"):
                default += " *(platform specific)*"
        return default

    def getDescription(self, data):
        description = data.get("description", "")
        # Raise exception, when descriptions look like RST formatting.
        if re.search(r"\*\*|`", description) is not None:
            raise RuntimeError(f"Description is not plain text: '{description}'")
        # Some descriptions contains text surrounded by '*'.
        # This regex puts backticks around these texts.
        description_rst = re.sub(r"(\*.*?\*)", r":strong:`\1`", description)
        return self.indent(description_rst, 3)

    def getConvertedResourceDirectives(self, daemon, resourcename):
        logger = logging.getLogger()

        result = ""
        # only useful, when file is included by toctree.
        # result='{}\n{}\n\n'.format(resourcename, len(resourcename) * '-')
        for directive in self.getResourceDirectives(daemon, resourcename):
            data = self.getResourceDirective(daemon, resourcename, directive)

            strings = {
                "program": daemon,
                "daemon": daemonName.getLowShort(daemon),
                "resource": resourcename.lower(),
                "directive": directive,
                "datatype": data["datatype"],
                "default": self.getDefaultValue(data),
                "version": data.get("start_version", ""),
                "description": self.getDescription(data),
                "required": "",
            }

            if data.get("alias"):
                if not strings["description"]:
                    strings["description"] = "   *This directive is an alias.*"

            if data.get("deprecated"):
                # overwrites start_version
                strings["version"] = "deprecated"

            includefilename = "/manually_added_config_directive_descriptions/{daemon}-{resource}-{directive}.rst.inc".format(
                **strings
            )

            result += ".. config:option:: {daemon}/{resource}/{directive}\n\n".format(
                **strings
            )

            if data.get("required"):
                strings["required"] = "True"
                result += "   :required: {required}\n".format(**strings)

            result += "   :type: :config:datatype:`{datatype}`\n".format(**strings)

            if data.get("default_value"):
                result += "   :default: {default}\n".format(**strings)

            if strings.get("version"):
                result += "   :version: {version}\n".format(**strings)

            result += "\n"

            if strings["description"]:
                result += strings["description"] + "\n\n"

            # make sure, file exists, so that there are no problems with include.
            checkincludefilename = "source/{}".format(includefilename)
            if not os.path.exists(checkincludefilename):
                touch(checkincludefilename)

            result += "   .. include:: {}\n\n".format(includefilename)

            result += "\n\n"

        return result

    def getHeader(self):
        result = ".. csv-table::\n"
        result += "   :header: "
        result += self.getHeaderColumns()
        result += "\n\n"
        return result

    def getHeaderColumns(self):
        columns = [
            "configuration directive name",
            "type of data",
            "default value",
            "remark",
        ]

        return '"{}"'.format('", "'.join(columns))

    def getRows(self, daemon, resourcename, subtree, link):
        result = ""
        for key in sorted(filter(None, subtree.keys())):
            data = BareosConfigurationSchemaDirective(subtree[key])

            if not self.isValidCamelCase(key):
                raise ValueError(
                    "The directive {} from {}/{} is not valid CamelCase.".format(
                        key, daemon, resourcename
                    )
                )

            strings = {
                "key": self.convertCamelCase2Spaces(key),
                "extra": [],
                "mc": "* ",
                "default": self.getDefaultValue(data),
                "program": daemon,
                "daemon": daemonName.getLowShort(daemon),
                "resource": resourcename.lower(),
                "directive": key,
            }

            strings["directive_link"] = link % strings

            if data.get("equals"):
                strings["datatype"] = "= :config:datatype:`{datatype}`\ ".format(**data)
            else:
                strings["datatype"] = "{{ :config:datatype:`{datatype}`\ }}".format(
                    **data
                )

            extra = []
            if data.get("alias"):
                extra.append("alias")
                strings["mo"] = "*"
            if data.get("deprecated"):
                extra.append("deprecated")
                strings["mo"] = "*"
            if data.get("required"):
                extra.append("required")
                strings["mo"] = "**"
                strings["mc"] = "** "
            strings["extra"] = ", ".join(extra)

            strings["t_default"] = '"{}"'.format(
                self.getStringsWithModifiers("default", strings)
            )
            strings["t_extra"] = '"{}"'.format(
                self.getStringsWithModifiers("extra", strings)
            )

            # result+='   %(directive_link)-60s, %(t_datatype)-20s, %(t_default)-20s, %(t_extra)s\n' % ( strings )
            result += (
                "   %(directive_link)-60s, %(datatype)s, %(t_default)s, %(t_extra)s\n"
                % (strings)
            )

        return result

    def getFooter(self):
        result = "\n"
        return result

    def getTable(
        self,
        daemon,
        resourcename,
        subtree,
        link=":config:option:`%(daemon)s/%(resource)s/%(directive)s`\ ",
    ):
        result = self.getHeader()
        result += self.getRows(daemon, resourcename, subtree, link)
        result += self.getFooter()

        return result

    def writeResourceDirectivesTable(self, daemon, resourcename, filename=None):
        ds = daemonName.getShort(daemon)
        self.open(filename, "w")
        self.out.write(
            self.getTable(
                daemon, resourcename, self.json["resource"][daemon][resourcename]
            )
        )
        self.close()


def createSphinx(data):
    logger = logging.getLogger()

    logger.info("Create RST/Sphinx files ...")

    sphinx = BareosConfigurationSchema2Sphinx(data)

    for daemon in sphinx.getDaemons():

        # pprint(schema.getResources(daemon))
        for resource in sphinx.getResources(daemon):
            logger.info("daemon: " + daemon + ", resource: " + resource)

            sphinx.writeResourceDirectives(
                daemon,
                resource,
                "source/include/autogenerated/"
                + daemon.lower()
                + "-resource-"
                + resource.lower()
                + "-description.rst.inc",
            )

            sphinx.writeResourceDirectivesTable(
                daemon,
                resource,
                "source/include/autogenerated/"
                + daemon.lower()
                + "-resource-"
                + resource.lower()
                + "-table.rst.inc",
            )

    # if sphinx.getDatatypes():
    #    print(sphinx.getDatatypes())


if __name__ == "__main__":
    logging.basicConfig(
        format="%(levelname)s %(module)s.%(funcName)s: %(message)s", level=logging.INFO
    )
    logger = logging.getLogger()

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-q", "--quiet", action="store_true", help="suppress logging output"
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="enable debugging output"
    )
    parser.add_argument(
        "--sphinx", action="store_true", help="Deprecated. Now the default."
    )
    parser.add_argument("filename", help="json data file")
    args = parser.parse_args()
    if args.debug:
        logger.setLevel(logging.DEBUG)

    if args.quiet:
        logger.setLevel(logging.CRITICAL)

    with open(args.filename) as data_file:
        data = json.load(data_file)
    # pprint(data)

    createSphinx(data)
