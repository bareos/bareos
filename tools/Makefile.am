bin_PROGRAMS=dplsh

dplsh_CFLAGS = -I$(top_srcdir)/libdroplet/include
dplsh_LDADD = $(top_builddir)/libdroplet/libdroplet.la $(READLINE_LIBS)
dplsh_SOURCES = \
	cmd_cd.c \
	cmd_cp.c \
	cmd_genurl.c \
	cmd_getattr.c \
	cmd_get.c \
	cmd_la.c \
	cmd_lcd.c \
	cmd_ls.c \
	cmd_mb.c \
	cmd_mkdir.c \
	cmd_put.c \
	cmd_pwd.c \
	cmd_rb.c \
	cmd_rm.c \
	cmd_rmdir.c \
	cmds.c \
	cmd_setattr.c \
	cmd_set.c \
	cmd_unset.c \
	dplsh.c \
	filecompl.c \
	shell.c \
	usage.c \
	utils.c \
	vars.c \
	xfuncs.c

noinst_HEADERS = \
	cmds.h \
	dplsh.h \
	shell.h \
	usage.h \
	utils.h \
	vars.h \
	xfuncs.h


