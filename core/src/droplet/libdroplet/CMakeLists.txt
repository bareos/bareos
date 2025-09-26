#   BAREOS® - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2020-2025 Bareos GmbH & Co. KG
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
message("Entering ${CMAKE_CURRENT_SOURCE_DIR}")

add_library(
  droplet STATIC
  src/conn.c
  src/converters.c
  src/value.c
  src/dict.c
  src/droplet.c
  src/httprequest.c
  src/httpreply.c
  src/pricing.c
  src/profile.c
  src/req.c
  src/vec.c
  src/sbuf.c
  src/dbuf.c
  src/ntinydb.c
  src/utils.c
  src/rest.c
  src/sysmd.c
  src/id_scheme.c
  src/task.c
  src/async.c
  src/addrlist.c
  src/getdate.c
  src/vfs.c
  src/uks.c
  src/gc.c
  src/backend/s3/backend.c
  src/backend/s3/replyparser.c
  src/backend/s3/reqbuilder.c
  src/backend/s3/auth/v2.c
  src/backend/s3/auth/v4.c
  src/backend/s3/backend/copy.c
  src/backend/s3/backend/delete.c
  src/backend/s3/backend/delete_all.c
  src/backend/s3/backend/delete_bucket.c
  src/backend/s3/backend/genurl.c
  src/backend/s3/backend/get.c
  src/backend/s3/backend/get_capabilities.c
  src/backend/s3/backend/head.c
  src/backend/s3/backend/head_raw.c
  src/backend/s3/backend/list_all_my_buckets.c
  src/backend/s3/backend/list_bucket.c
  src/backend/s3/backend/list_bucket_attrs.c
  src/backend/s3/backend/make_bucket.c
  src/backend/s3/backend/put.c
  src/backend/s3/backend/multipart.c
  src/backend/s3/backend/stream_resume.c
  src/backend/s3/backend/stream_get.c
  src/backend/s3/backend/stream_put.c
  src/backend/s3/backend/stream_flush.c
  src/backend/cdmi/backend.c
  src/backend/cdmi/replyparser.c
  src/backend/cdmi/reqbuilder.c
  src/backend/cdmi/crcmodel.c
  src/backend/cdmi/object_id.c
  src/backend/swift/backend.c
  src/backend/swift/replyparser.c
  src/backend/swift/reqbuilder.c
  src/backend/srws/backend.c
  src/backend/srws/replyparser.c
  src/backend/srws/reqbuilder.c
  src/backend/srws/key.c
  src/backend/sproxyd/backend.c
  src/backend/sproxyd/replyparser.c
  src/backend/sproxyd/reqbuilder.c
  src/backend/sproxyd/backend/copy_id.c
  src/backend/sproxyd/backend/delete_all_id.c
  src/backend/sproxyd/backend/delete_id.c
  src/backend/sproxyd/backend/get_id.c
  src/backend/sproxyd/backend/head_id.c
  src/backend/sproxyd/backend/put_id.c
  src/backend/sproxyd/key.c
  src/backend/posix/backend.c
  src/backend/posix/reqbuilder.c
  src/backend/posix/replyparser.c
)

include(BareosCompilerFlags)
bareos_disable_warnings(
  TARGETS
  droplet
  WARNINGS
  -Wdeprecated-declarations
  -Wunused-but-set-variable
  -Wnon-literal-null-conversion
  -Wsign-compare
  -Wunused-parameter
  -Wdeprecated-non-prototype
  -Wold-style-definition
  C_ONLY
  ADD_FOR_LTO
)

target_include_directories(droplet PUBLIC include ${JSONC_INCLUDE_DIRS})
target_link_libraries(
  droplet PRIVATE LibXml2::LibXml2 ${JSONC_LIBRARIES} OpenSSL::SSL
)

include(DebugEdit)
