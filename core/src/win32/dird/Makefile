#
# BAREOS® - Backup Archiving REcovery Open Sourced
#
# Copyright (C) 2014-2014 Bareos GmbH & Co. KG
#
# This program is Free Software; you can redistribute it and/or
# modify it under the terms of version three of the GNU Affero General Public
# License as published by the Free Software Foundation and included
# in the file LICENSE.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
VPATH = .:../../dird:../generic

include ../Makefile.inc

INCLUDES = -I. \
	   -I../.. \
	   -I../../include \
	   -I../include \
	   -I../compat/include
LDLIBS = ../lib/libbareos.a \
	 ../lib/libbareos.dll \
         ../findlib/libbareosfind.dll \
	 ../cats/libbareoscats.dll \
         $(MINGW_LIB)/libwinpthread.a \
         $(MINGW_LIB)/libjansson.a \
	 $(WINSOCKLIB) -lole32 -loleaut32 -luuid -lcomctl32

SVRSRCS = admin.cc archive.cc authenticate.cc autoprune.cc backup.cc bsr.cc \
	  catreq.cc consolidate.cc dir_plugins.cc dird_conf.cc dird.cc \
	  expand.cc fd_cmds.cc getmsg.cc inc_conf.cc job.cc jobq.cc migrate.cc \
	  mountreq.cc msgchan.cc \
	  ndmp_dma_backup_common.cc ndmp_dma_backup_NDMP_BAREOS.cc \
	  ndmp_dma_backup_NDMP_NATIVE.cc \
	  ndmp_dma_generic.cc\
	  ndmp_dma_restore_common.cc ndmp_dma_restore_NDMP_BAREOS.cc \
	  ndmp_dma_restore_NDMP_NATIVE.cc \
	  ndmp_dma_storage.cc ndmp_fhdb_helpers.cc \
	  ndmp_fhdb_mem.cc ndmp_fhdb_lmdb.cc newvol.cc next_vol.cc quota.cc \
	  recycle.cc restore.cc run_conf.cc sd_cmds.cc scheduler.cc \
	  socket_server.cc stats.cc storage.cc ua_acl.cc ua_audit.cc ua_cmds.cc \
	  ua_configure.cc ua_db.cc ua_dotcmds.cc ua_input.cc ua_impexp.cc \
	  ua_label.cc ua_output.cc ua_prune.cc ua_purge.cc ua_query.cc \
	  ua_restore.cc ua_run.cc ua_select.cc ua_server.cc ua_status.cc \
	  ua_tree.cc ua_update.cc vbackup.cc verify.cc service.cc main.cc
SVROBJS = $(SVRSRCS:.cc=.o)

DBCHKSRCS = dbcheck.cc dird_conf.cc ua_acl.cc ua_audit.cc run_conf.cc inc_conf.cc
DBCHKOBJS = $(DBCHKSRCS:.cc=.o)

WINDRESSRCS = dirdres.rc
WINDRESOBJS = $(WINDRESSRCS:.rc=.o)

DBCHKRESSRCS = dbcheckres.rc
DBCHKRESOBJS = $(DBCHKRESSRCS:.rc=.o)

all: bareos-dir.exe bareos-dbcheck.exe

bareos-dir.exe: DLL_USAGE = -DUSING_DLL
bareos-dir.exe: $(SVROBJS) $(WINDRESOBJS)
	$(CXX) $(LDFLAGS_WINAPP) $(SVROBJS) $(WINDRESOBJS) $(LDLIBS) -o $@

bareos-dbcheck.exe: DLL_USAGE = -DUSING_DLL
bareos-dbcheck.exe: $(DBCHKOBJS) $(DBCHKRESOBJS)
	$(CXX) $(LDFLAGS_CONS) $(DBCHKOBJS) $(LDLIBS) $(DBCHKRESOBJS) -o $@

clean:
	rm -f *.o

distclean:: clean
	rm -f bareos-dir.exe bareos-dbcheck.exe

# inference rules
.cc.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
