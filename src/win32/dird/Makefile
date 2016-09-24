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

SVRSRCS = admin.c archive.c authenticate.c autoprune.c backup.c bsr.c \
	  catreq.c consolidate.c dir_plugins.c dird_conf.c dird.c \
	  expand.c fd_cmds.c getmsg.c inc_conf.c job.c jobq.c migrate.c \
	  mountreq.c msgchan.c ndmp_dma_backup.c ndmp_dma_generic.c \
	  ndmp_dma_restore.c ndmp_dma_storage.c ndmp_fhdb_helpers.c \
	  ndmp_fhdb_mem.c ndmp_fhdb_lmdb.c newvol.c next_vol.c quota.c \
	  recycle.c restore.c run_conf.c sd_cmds.c scheduler.c \
	  socket_server.c stats.c storage.c ua_acl.c ua_audit.c ua_cmds.c \
	  ua_configure.c ua_db.c ua_dotcmds.c ua_input.c ua_impexp.c \
	  ua_label.c ua_output.c ua_prune.c ua_purge.c ua_query.c \
	  ua_restore.c ua_run.c ua_select.c ua_server.c ua_status.c \
	  ua_tree.c ua_update.c vbackup.c verify.c service.c main.c
SVROBJS = $(SVRSRCS:.c=.o)

DBCHKSRCS = dbcheck.c dird_conf.c ua_acl.c ua_audit.c run_conf.c inc_conf.c
DBCHKOBJS = $(DBCHKSRCS:.c=.o)

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
.c.o:
	@echo "Compiling $<"
	$(CXX) $(DLL_USAGE) $(CXXFLAGS) -c -o $@ $<

%.o : %.rc
	@echo "Ressource Compiling $^"
	$(WINDRES) $^ -o $@
