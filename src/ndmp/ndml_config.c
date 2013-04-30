/*
 * Copyright (c) 1998,1999,2000
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
 * Description:
 *
 */


#include "ndmlib.h"

#define CFG_BUF_SIZE	512
#define CFG_MAX_SV	32

/* control block */
struct cfg_cb {
	FILE *			fp;
	ndmp9_config_info *	config_info;
	char			buf[CFG_BUF_SIZE];
	char *			sv[CFG_MAX_SV];
	int			sc;
	int			n_error;
};


static int	cfg_butype (struct cfg_cb *cb);
static int	cfg_fs (struct cfg_cb *cb);
static int	cfg_tape (struct cfg_cb *cb);
static int	cfg_scsi (struct cfg_cb *cb);
static int	cfg_device (struct cfg_cb *cb, u_int *n_device,
			ndmp9_device_info **pp);
static int	cfg_add_env (struct cfg_cb *cb, u_int *n_env,
			ndmp9_pval **pp, char *name, char *value);




int
ndmcfg_load (char *filename, ndmp9_config_info *config_info)
{
	FILE *		fp;
	int		rc;

	fp = fopen (filename, "r");
	if (!fp)
		return -1;

	rc = ndmcfg_loadfp (fp, config_info);

	fclose (fp);

	return rc;
}

int
ndmcfg_loadfp (FILE *fp, ndmp9_config_info *config_info)
{
	struct cfg_cb	_cb, *cb = &_cb;
	int		rc;

	NDMOS_MACRO_ZEROFILL (cb);

	cb->fp = fp;
	cb->config_info = config_info;

	for (;;) {
		rc = ndmstz_getstanza (cb->fp, cb->buf, sizeof cb->buf);
		if (rc == EOF) {
			break;
		}

		cb->sc = ndmstz_parse (cb->buf, cb->sv, CFG_MAX_SV);
		if (cb->sc < 1) {
			continue;
		}

		if (strcmp (cb->sv[0], "butype") == 0 && cb->sc == 2) {
			cfg_butype (cb);
			continue;
		}

		if (strcmp (cb->sv[0], "fs") == 0 && cb->sc == 2) {
			cfg_fs (cb);
			continue;
		}

		if (strcmp (cb->sv[0], "tape") == 0 && cb->sc == 2) {
			cfg_tape (cb);
			continue;
		}

		if (strcmp (cb->sv[0], "scsi") == 0 && cb->sc == 2) {
			cfg_scsi (cb);
			continue;
		}

		/*
		 * Unrecognized stanzas are deemed for other purposes
		 * and tolerated.
		 */
	}

	return cb->n_error;
}

/*
 * [butype BUTYPE]
 *	v2attr	0xATTR
 *	v3attr	0xATTR
 *	v4attr	0xATTR
 *	default_env NAME VALUE
 */

static int
cfg_butype (struct cfg_cb *cb)
{
	ndmp9_config_info *	cfg = cb->config_info;
	ndmp9_butype_info *	ent = cfg->butype_info.butype_info_val;
	int			n_ent = cfg->butype_info.butype_info_len;
	int			i, rc;

	if (!ent)
		n_ent = 0;

	ent = NDMOS_MACRO_NEWN(ndmp9_butype_info, n_ent+1);
	if (!ent) {
		cb->n_error++;
		return -1;
	}

	for (i = 0; i < n_ent; i++) {
		ent[i] = cfg->butype_info.butype_info_val[i];
	}

	if (cfg->butype_info.butype_info_val) {
		NDMOS_API_FREE (cfg->butype_info.butype_info_val);
	}
	cfg->butype_info.butype_info_val = ent;
	cfg->butype_info.butype_info_len = n_ent+1;
	ent += n_ent;

	NDMOS_MACRO_ZEROFILL (ent);

	ent->butype_name = NDMOS_API_STRDUP (cb->sv[1]);

	for (;;) {
		rc = ndmstz_getline (cb->fp, cb->buf, CFG_BUF_SIZE);
		if (rc < 0)
			break;

		cb->sc = ndmstz_parse (cb->buf, cb->sv, CFG_MAX_SV);
		if (cb->sc < 1) {
			continue;
		}

		if (strcmp (cb->sv[0], "v2attr") == 0 && cb->sc == 2) {
			ent->v2attr.valid = NDMP9_VALIDITY_VALID;
			ent->v2attr.value = strtol (cb->sv[1], 0, 0);
			continue;
		}

		if (strcmp (cb->sv[0], "v3attr") == 0 && cb->sc == 2) {
			ent->v3attr.valid = NDMP9_VALIDITY_VALID;
			ent->v3attr.value = strtol (cb->sv[1], 0, 0);
			continue;
		}

		if (strcmp (cb->sv[0], "v4attr") == 0 && cb->sc == 2) {
			ent->v4attr.valid = NDMP9_VALIDITY_VALID;
			ent->v4attr.value = strtol (cb->sv[1], 0, 0);
			continue;
		}

		if (strcmp (cb->sv[0], "default_env") == 0 && cb->sc == 3) {
			cfg_add_env (cb, &ent->default_env.default_env_len,
				&ent->default_env.default_env_val,
				cb->sv[1], cb->sv[2]);
			continue;
		}

		/*
		 * Unrecognized lines are deemed version differences
		 * and tolerated.
		 */
	}

	return 0;
}

/*
 * [fs MOUNTPOINT]
 *	fs_type TYPE
 *	fs_physical_device DEVICEPATH
 *	fs_status "COMMENT"
 *	fs_env NAME VALUE
 */

static int
cfg_fs (struct cfg_cb *cb)
{
	ndmp9_config_info *	cfg = cb->config_info;
	ndmp9_fs_info *		ent = cfg->fs_info.fs_info_val;
	int			n_ent = cfg->fs_info.fs_info_len;
	int			i, rc;

	if (!ent)
		n_ent = 0;

	ent = NDMOS_MACRO_NEWN(ndmp9_fs_info, n_ent+1);
	if (!ent) {
		cb->n_error++;
		return -1;
	}

	for (i = 0; i < n_ent; i++) {
		ent[i] = cfg->fs_info.fs_info_val[i];
	}

	if (cfg->fs_info.fs_info_val) {
		NDMOS_API_FREE (cfg->fs_info.fs_info_val);
	}
	cfg->fs_info.fs_info_val = ent;
	cfg->fs_info.fs_info_len = n_ent+1;
	ent += n_ent;

	NDMOS_MACRO_ZEROFILL (ent);

	ent->fs_logical_device = NDMOS_API_STRDUP (cb->sv[1]);

	for (;;) {
		rc = ndmstz_getline (cb->fp, cb->buf, CFG_BUF_SIZE);
		if (rc < 0)
			break;

		cb->sc = ndmstz_parse (cb->buf, cb->sv, CFG_MAX_SV);
		if (cb->sc < 1) {
			continue;
		}

		if (strcmp (cb->sv[0], "fs_type") == 0 && cb->sc == 2) {
			ent->fs_type = NDMOS_API_STRDUP (cb->sv[1]);
			continue;
		}

		if (strcmp (cb->sv[0], "fs_physical_device") == 0
		 && cb->sc == 2) {
			ent->fs_physical_device = NDMOS_API_STRDUP (cb->sv[1]);
			continue;
		}

		if (strcmp (cb->sv[0], "fs_status") == 0 && cb->sc == 2) {
			ent->fs_status = NDMOS_API_STRDUP (cb->sv[1]);
			continue;
		}

		if (strcmp (cb->sv[0], "fs_env") == 0 && cb->sc == 3) {
			cfg_add_env (cb, &ent->fs_env.fs_env_len,
				&ent->fs_env.fs_env_val,
				cb->sv[1], cb->sv[2]);
			continue;
		}

		/*
		 * Unrecognized lines are deemed version differences
		 * and tolerated.
		 */
	}

	return 0;
}

static int
cfg_tape (struct cfg_cb *cb)
{
	ndmp9_config_info *	cfg = cb->config_info;

	return cfg_device (cb, &cfg->tape_info.tape_info_len,
				&cfg->tape_info.tape_info_val);
}

static int
cfg_scsi (struct cfg_cb *cb)
{
	ndmp9_config_info *	cfg = cb->config_info;

	return cfg_device (cb, &cfg->scsi_info.scsi_info_len,
				&cfg->scsi_info.scsi_info_val);
}

/*
 * [tape IDENT]  or  [scsi IDENT]
 *	device DEVICEPATH
 *	v3attr 0xATTR
 *	v4attr 0xATTR
 *	capability NAME VALUE
 */

static int
cfg_device (struct cfg_cb *cb, u_int *n_device, ndmp9_device_info **pp)
{
	ndmp9_device_info *	ent = *pp;
	ndmp9_device_capability *dcap;
	int			rc;
	unsigned int		i, n_ent = *n_device;

	if (!ent)
		n_ent = 0;

	for (i = 0; i < n_ent; i++) {
		if (strcmp(ent[i].model, (*pp)[i].model) == 0) {
			ent += i;
			goto got_model;
		}
	}

	ent = NDMOS_MACRO_NEWN(ndmp9_device_info, n_ent+1);
	if (!ent) {
		cb->n_error++;
		return -1;
	}

	for (i = 0; i < n_ent; i++) {
		ent[i] = (*pp)[i];
	}

	if (*pp) {
		NDMOS_API_FREE (*pp);
	}
	*pp = ent;
	*n_device = n_ent+1;
	ent += n_ent;

	NDMOS_MACRO_ZEROFILL (ent);
	ent->model = NDMOS_API_STRDUP (cb->sv[1]);

  got_model:
	dcap = NDMOS_MACRO_NEWN (ndmp9_device_capability,
			ent->caplist.caplist_len+1);
	if (!dcap) {
		cb->n_error++;
		return -1;
	}

	for (i = 0; i < ent->caplist.caplist_len; i++) {
		dcap[i] = ent->caplist.caplist_val[i];
	}
	if (ent->caplist.caplist_val) {
		NDMOS_API_FREE (ent->caplist.caplist_val);
	}

	ent->caplist.caplist_val = dcap;
	dcap += ent->caplist.caplist_len++;
	NDMOS_MACRO_ZEROFILL (dcap);

	for (;;) {
		rc = ndmstz_getline (cb->fp, cb->buf, CFG_BUF_SIZE);
		if (rc < 0)
			break;

		cb->sc = ndmstz_parse (cb->buf, cb->sv, CFG_MAX_SV);
		if (cb->sc < 1) {
			continue;
		}

		if (strcmp (cb->sv[0], "device") == 0 && cb->sc == 2) {
			dcap->device = NDMOS_API_STRDUP (cb->sv[1]);
			continue;
		}

		if (strcmp (cb->sv[0], "v3attr") == 0 && cb->sc == 2) {
			dcap->v3attr.valid = NDMP9_VALIDITY_VALID;
			dcap->v3attr.value = strtol (cb->sv[1], 0, 0);
			continue;
		}

		if (strcmp (cb->sv[0], "v4attr") == 0 && cb->sc == 2) {
			dcap->v4attr.valid = NDMP9_VALIDITY_VALID;
			dcap->v4attr.value = strtol (cb->sv[1], 0, 0);
			continue;
		}

		if (strcmp (cb->sv[0], "capability") == 0 && cb->sc == 3) {
			cfg_add_env (cb, &dcap->capability.capability_len,
				&dcap->capability.capability_val,
				cb->sv[1], cb->sv[2]);
			continue;
		}

		/*
		 * Unrecognized lines are deemed version differences
		 * and tolerated.
		 */
	}

	return 0;
}

static int
cfg_add_env (struct cfg_cb *cb, u_int *n_env,
  ndmp9_pval **pp, char *name, char *value)
{
	ndmp9_pval *		ent = *pp;
	int			n_ent = *n_env;
	int			i;

	if (!ent)
		n_ent = 0;

	ent = NDMOS_MACRO_NEWN(ndmp9_pval, n_ent+1);
	if (!ent) {
		cb->n_error++;
		return -1;
	}

	for (i = 0; i < n_ent; i++) {
		ent[i] = (*pp)[i];
	}

	if (*pp) {
		NDMOS_API_FREE (*pp);
	}

	*pp = ent;
	*n_env = n_ent+1;
	ent += n_ent;

	NDMOS_MACRO_ZEROFILL (ent);
	ent->name = NDMOS_API_STRDUP (name);
	ent->value = NDMOS_API_STRDUP (value);

	return 0;
}

#ifdef SELF_TEST

int
main (int argc, char *argv[])
{
	ndmp9_config_info	config_info;
	int			rc, i, j, k;

	if (argc != 2) {
		printf ("usage: %s FILE\n", argv[0]);
		return 1;
	}

	NDMOS_MACRO_ZEROFILL (&config_info);

	rc = ndmcfg_load (argv[1], &config_info);
	printf ("%d errors\n", rc);

	for (i = 0; i < config_info.butype_info.butype_info_len; i++) {
		ndmp9_butype_info *	bu;

		bu = &config_info.butype_info.butype_info_val[i];
		printf ("butype[%d] name='%s'\n", i, bu->butype_name);
		if (bu->v2attr.valid) {
			printf ("  v2attr 0x%x\n", bu->v2attr.value);
		} else {
			printf ("  v2attr -invalid-\n");
		}
		if (bu->v3attr.valid) {
			printf ("  v3attr 0x%x\n", bu->v3attr.value);
		} else {
			printf ("  v3attr -invalid-\n");
		}
		if (bu->v4attr.valid) {
			printf ("  v4attr 0x%x\n", bu->v4attr.value);
		} else {
			printf ("  v4attr -invalid-\n");
		}
		for (j = 0; j < bu->default_env.default_env_len; j++) {
			ndmp9_pval *	env;

			env = &bu->default_env.default_env_val[j];
			printf ("  default_env[%d] '%s'='%s'\n",
				j, env->name, env->value);
		}
	}

	for (i = 0; i < config_info.fs_info.fs_info_len; i++) {
		ndmp9_fs_info *	fs;

		fs = &config_info.fs_info.fs_info_val[i];
		printf ("fs[%d] fs_logical_device='%s'\n",
			 i, fs->fs_logical_device);
		if (fs->fs_physical_device) {
			printf ("  fs_physical_device '%s'\n",
				fs->fs_physical_device);
		} else {
			printf ("  fs_physical_device -null-\n");
		}
		if (fs->fs_type) {
			printf ("  fs_type '%s'\n", fs->fs_type);
		} else {
			printf ("  fs_type -null-\n");
		}
		if (fs->fs_status) {
			printf ("  fs_status '%s'\n", fs->fs_status);
		} else {
			printf ("  fs_status -null-\n");
		}
		if (fs->total_size.valid) {
			printf ("  total_size %llu\n", fs->total_size.value);
		} else {
			printf ("  total_size -invalid-\n");
		}
		if (fs->used_size.valid) {
			printf ("  used_size %llu\n", fs->used_size.value);
		} else {
			printf ("  used_size -invalid-\n");
		}
		if (fs->avail_size.valid) {
			printf ("  avail_size %llu\n", fs->avail_size.value);
		} else {
			printf ("  avail_size -invalid-\n");
		}
		if (fs->total_inodes.valid) {
			printf ("  total_inodes %llu\n",
				fs->total_inodes.value);
		} else {
			printf ("  total_inodes -invalid-\n");
		}
		if (fs->used_inodes.valid) {
			printf ("  used_inodes %llu\n", fs->used_inodes.value);
		} else {
			printf ("  used_inodes -invalid-\n");
		}

		for (j = 0; j < fs->fs_env.fs_env_len; j++) {
			ndmp9_pval *	env;

			env = &fs->fs_env.fs_env_val[j];
			printf ("  fs_env[%d] '%s'='%s'\n",
				j, env->name, env->value);
		}
	}

	for (i = 0; i < config_info.tape_info.tape_info_len; i++) {
		ndmp9_device_info *	dev;

		dev = &config_info.tape_info.tape_info_val[i];
		printf ("tape[%d] model='%s'\n", i, dev->model);

		for (j = 0; j < dev->caplist.caplist_len; j++) {
			struct ndmp9_device_capability *dcap;

			dcap = &dev->caplist.caplist_val[j];
			printf (" capability %d\n", j);

			if (dcap->device) {
				printf ("  device '%s'\n", dcap->device);
			} else {
				printf ("  device -null-\n");
			}
			if (dcap->v3attr.valid) {
				printf ("  v3attr 0x%x\n", dcap->v3attr.value);
			} else {
				printf ("  v3attr -invalid-\n");
			}
			if (dcap->v4attr.valid) {
				printf ("  v4attr 0x%x\n", dcap->v4attr.value);
			} else {
				printf ("  v4attr -invalid-\n");
			}
			k = 0;
			for (; k < dcap->capability.capability_len; k++) {
				ndmp9_pval *env;
				env = &dcap->capability.capability_val[k];
				printf ("  capability[%d] '%s'='%s'\n",
					k, env->name, env->value);
			}
		}
	}

	for (i = 0; i < config_info.scsi_info.scsi_info_len; i++) {
		ndmp9_device_info *	dev;

		dev = &config_info.scsi_info.scsi_info_val[i];
		printf ("scsi[%d] model='%s'\n", i, dev->model);

		for (j = 0; j < dev->caplist.caplist_len; j++) {
			struct ndmp9_device_capability *dcap;

			dcap = &dev->caplist.caplist_val[j];
			printf (" capability %d\n", j);

			if (dcap->device) {
				printf ("  device '%s'\n", dcap->device);
			} else {
				printf ("  device -null-\n");
			}
			if (dcap->v3attr.valid) {
				printf ("  v3attr 0x%x\n", dcap->v3attr.value);
			} else {
				printf ("  v3attr -invalid-\n");
			}
			if (dcap->v4attr.valid) {
				printf ("  v4attr 0x%x\n", dcap->v4attr.value);
			} else {
				printf ("  v4attr -invalid-\n");
			}
			k = 0;
			for (; k < dcap->capability.capability_len; k++) {
				ndmp9_pval *env;
				env = &dcap->capability.capability_val[k];
				printf ("  capability[%d] '%s'='%s'\n",
					k, env->name, env->value);
			}
		}
	}


	return 0;
}

#endif /* SELF_TEST */
