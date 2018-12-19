#   BAREOS�� - Backup Archiving REcovery Open Sourced
#
#   Copyright (C) 2017-2017 Bareos GmbH & Co. KG
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


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDMOS_OPTION_NO_TEST_AGENTS")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDMOS_CONST_NDMJOBLIB_REVISION=\\\"${FULLVERSION}\\\"")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDMOS_CONST_VENDOR_NAME=\\\"Bareos\\ GmbH\\ \\&\\ Co.KG\\\"")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDMOS_CONST_PRODUCT_NAME=\\\"Bareos\\\"")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DNDMOS_CONST_PRODUCT_REVISION=\\\"${FULLVERSION}\\\"")

if (${HAVE_LINUX_OS})
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_LINUX_OS")
elseif (${HAVE_SUN_OS})
   set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DHAVE_SUN_OS")
endif()

set(RPC_GEN_FILES ndmp0.h ndmp0_xdr.c
                ndmp2.h ndmp2_xdr.c
                ndmp3.h ndmp3_xdr.c
                ndmp4.h ndmp4_xdr.c
                ndmp9.h ndmp9_xdr.c
)


set(INCLUDE_FILES = ndmagents.h ndmlib.h ndmp_ammend.h ndmp_msg_buf.h ndmp_translate.h
		ndmp0_enum_strs.h ndmp0.h ndmp2_enum_strs.h ndmp2_translate.h ndmp2.h
		ndmp3_enum_strs.h ndmp3_translate.h ndmp3.h ndmp4_enum_strs.h
		ndmp4_translate.h ndmp4.h ndmp9_enum_strs.h ndmp9.h ndmprotocol.h
		scsiconst.h smc_priv.h smc_raw.h smc.h wraplib.h
)

set(NDMP_PROTO_SRCS ndmp_translate.c ndmp0_enum_strs.c ndmp0_pp.c ndmp0_xdr.c
                  ndmp0_xmt.c ndmp2_enum_strs.c ndmp2_pp.c ndmp2_translate.c
                  ndmp2_xdr.c ndmp2_xmt.c ndmp3_enum_strs.c ndmp3_pp.c ndmp3_translate.c
                  ndmp3_xdr.c ndmp3_xmt.c ndmp4_enum_strs.c ndmp4_pp.c ndmp4_translate.c
                  ndmp4_xdr.c ndmp4_xmt.c ndmp9_enum_strs.c ndmp9_xdr.c ndmp9_xmt.c ndmprotocol.c
)

set(NDMP_LIB_SRCS ndml_agent.c ndml_bstf.c ndml_chan.c ndml_config.c ndml_conn.c ndml_cstr.c
                ndml_fhdb.c ndml_fhh.c ndml_log.c ndml_md5.c ndml_media.c ndml_nmb.c
                ndml_scsi.c ndml_stzf.c ndml_util.c
)

set(NDMP_AGENT_SRCS ndma_comm_dispatch.c ndma_comm_job.c ndma_comm_session.c ndma_comm_subr.c
		  ndma_control.c ndma_cops_backreco.c ndma_cops_labels.c ndma_cops_query.c
		  ndma_cops_robot.c ndma_ctrl_calls.c ndma_ctrl_conn.c ndma_ctrl_media.c
		  ndma_ctrl_robot.c ndma_ctst_data.c ndma_ctst_mover.c ndma_ctst_subr.c
		  ndma_ctst_tape.c ndma_data_fh.c ndma_data_pfe.c ndma_data.c
		  ndma_image_stream.c ndma_listmgmt.c ndma_noti_calls.c
		  ndma_robot.c ndma_robot_simulator.c
		  ndma_tape.c ndma_tape_simulator.c
)

set(NDMP_HELPER_SRCS md5c.c ndmos.c smc_api.c smc_parse.c smc_pp.c wraplib.c)

set(NDMJOB_SRCS ndmjob_args.c ndmjob_fhdb.c ndmjob_job.c ndmjob_main.c
	      ndmjob_main_util.c ndmjob_rules.c ndmjob_simulator.c ndmjr_none.c)


set(LIBBAREOSNDMP_SRCS  ${NDMP_PROTO_SRCS} ${NDMP_LIB_SRCS} ${NDMP_AGENT_SRCS} ${NDMP_HELPER_SRCS})

set(RPCGEN_PARAMS "-CM")


# create files with rpcgen

STRING(COMPARE EQUAL ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} IN_SOURCE_BUILD)
foreach(NDMP_VERSION 0 1 2 3 4 9)
   IF(IN_SOURCE_BUILD)
      ADD_CUSTOM_COMMAND(
         PRE_BUILD
         OUTPUT ndmp${NDMP_VERSION}.h ndmp${NDMP_VERSION}_xdr.c
         COMMAND  ${RPCGEN} ${RPCGEN_PARAMS} ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}.x

         DEPENDS ${RPCGEN} ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}.x
         )
   ELSE()
      ADD_CUSTOM_COMMAND(
         PRE_BUILD
         OUTPUT ndmp${NDMP_VERSION}.h ndmp${NDMP_VERSION}_xdr.c
         COMMAND  ${RPCGEN} ${RPCGEN_PARAMS} ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}.x
         COMMAND  test -e ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}.h && cp ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}.h ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}_xdr.c ${CMAKE_CURRENT_BINARY_DIR} || :
         COMMAND  test -e ndmp${NDMP_VERSION}.h && cp ndmp${NDMP_VERSION}.h ndmp${NDMP_VERSION}_xdr.c ${CMAKE_CURRENT_SOURCE_DIR} || :
         DEPENDS ${RPCGEN} ${CMAKE_CURRENT_SOURCE_DIR}/ndmp${NDMP_VERSION}.x
         )
   ENDIF()
endforeach()

add_library(bareosndmp SHARED ${LIBBAREOSNDMP_SRCS})
target_link_libraries(bareosndmp ${WRAP_LIBS} ${TIRPC_LIBRARIES})

INSTALL(TARGETS bareosndmp DESTINATION ${libdir})

set_target_properties(bareosndmp PROPERTIES VERSION "${VERSION}"
                                          SOVERSION "${SOVERSION}"

                                          )
IF(build_ndmjob)
  add_executable(ndmjob ${NDMJOB_SRCS})
  target_link_libraries(ndmjob bareosndmp)
ENDIF()
