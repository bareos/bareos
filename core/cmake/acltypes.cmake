#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2019-2019 Bareos GmbH & Co. KG
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

check_c_source_compiles(
  "#include <sys/types.h>
   #include <sys/acl.h>
   int main(void) { return ACL_TYPE_DEFAULT_DIR; }" HAVE_ACL_TYPE_DEFAULT_DIR
)

check_c_source_compiles(
  "#include <sys/types.h>
   #include <sys/acl.h>
   int main(void) { return ACL_TYPE_EXTENDED; }" HAVE_ACL_TYPE_EXTENDED
)

check_c_source_compiles(
  "#include <sys/types.h>
   #include <sys/acl.h>
   int main(void) { return ACL_TYPE_NFS4; }" HAVE_ACL_TYPE_NFS4
)
