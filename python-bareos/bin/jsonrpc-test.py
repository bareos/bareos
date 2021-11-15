#!/usr/bin/env python
#   BAREOS - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2020 Bareos GmbH & Co. KG
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

# coding: utf-8

from pprint import pprint
import pyjsonrpc

http_client = pyjsonrpc.HttpClient(
    url="http://localhost:8080",
    # username = "Username",
    # password = "Password"
)

# try:
# print http_client.call("a2")
## Result: 3
# except:
# pass

# It is also possible to use the *method* name as *attribute* name.
# print http_client.add(1, 2)

# provoke an error
# print http_client.add(2,"abc")


result = http_client.execute_fullresult("list jobs last")
pprint(result)

result = http_client.execute("list jobs last")
pprint(result)

result = http_client.list("jobs last")
pprint(result)


# try:
# pprint(result['result'])
# except:
# pprint( result )
