/*
 * Copyright (c) 2001,2002
 *	Traakan, Inc., Los Altos, CA
 *	All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Project:  NDMJOB
 * Ident:    $Id: $
 *
 * Description: FileIndex database handling functions.
 *              Implemented as callbacks from the generic framework.
 *              Original code extracted from ndml_fhdb.c
 *
 */


#include "ndmlib.h"



int
ndmjobfhdb_add_file (struct ndmlog *ixlog, int tagc,
  char *raw_name, ndmp9_file_stat *fstat)
{
	char		prefix[8];
	char		statbuf[100];
	char		namebuf[NDMOS_CONST_PATH_MAX];

	strcpy (prefix, "DHf");
	prefix[0] = tagc;

	ndm_fstat_to_str (fstat, statbuf);

	ndmcstr_from_str (raw_name, namebuf, sizeof namebuf);

	ndmlogf (ixlog, prefix, 0, "%s UNIX %s", namebuf, statbuf);

	return 0;
}

int
ndmjobfhdb_add_dir (struct ndmlog *ixlog, int tagc,
  char *raw_name, ndmp9_u_quad dir_node, ndmp9_u_quad node)
{
	char		prefix[8];
	char		namebuf[NDMOS_CONST_PATH_MAX];

	strcpy (prefix, "DHd");
	prefix[0] = tagc;

	ndmcstr_from_str (raw_name, namebuf, sizeof namebuf);

	ndmlogf (ixlog, prefix, 0, "%llu %s UNIX %llu",
		dir_node, namebuf, node);

	return 0;
}

int
ndmjobfhdb_add_node (struct ndmlog *ixlog, int tagc,
  ndmp9_u_quad node, ndmp9_file_stat *fstat)
{
	char		prefix[8];
	char		statbuf[100];

	strcpy (prefix, "DHn");
	prefix[0] = tagc;

	ndm_fstat_to_str (fstat, statbuf);

	ndmlogf (ixlog, prefix, 0, "%llu UNIX %s", node, statbuf);

	return 0;
}

int
ndmjobfhdb_add_dirnode_root (struct ndmlog *ixlog, int tagc,
  ndmp9_u_quad root_node)
{
	char		prefix[8];

	strcpy (prefix, "DHr");
	prefix[0] = tagc;

	ndmlogf (ixlog, prefix, 0, "%llu", root_node);

	return 0;
}
