/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2012 Free Software Foundation Europe e.V.

   The main author of Bacula is Kern Sibbald, with contributions from
   many others, a complete list can be found in the file AUTHORS.
   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.

   Bacula® is a registered trademark of Kern Sibbald.
   The licensor of Bacula is the Free Software Foundation Europe
   (FSFE), Fiduciary Program, Sumatrastrasse 25, 8006 Zürich,
   Switzerland, email:ftf@fsfeurope.org.
*/
/*
 *
 * Bacula interface to Python for the Director
 *
 * Kern Sibbald, November MMIV
 *
 *
 */

#include "bacula.h"
#include "dird.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>

#include <lib/pythonlib.h>

extern struct s_jl joblevels[];

static PyObject *set_job_events(PyObject *self, PyObject *arg);
static PyObject *job_run(PyObject *self, PyObject *arg);
static PyObject *job_write(PyObject *self, PyObject *arg);
static PyObject *job_cancel(PyObject *self, PyObject *arg);
static PyObject *job_does_vol_exist(PyObject *self, PyObject *arg);

PyMethodDef JobMethods[] = {
    {"set_events", set_job_events, METH_VARARGS, "Set Job events"},
    {"run", job_run, METH_VARARGS, "Run a Job"},
    {"write", job_write, METH_VARARGS, "Write to output"},
    {"cancel", job_cancel, METH_VARARGS, "Cancel a Job"},
    {"DoesVolumeExist", job_does_vol_exist, METH_VARARGS, "Does Volume Exist"},
    {NULL, NULL, 0, NULL}             /* last item */
};
 
struct s_vars {
   const char *name;
   const char *fmt;
};

/* Read-only variables */
static struct s_vars getvars[] = {
   { "Job",        "s"},
   { "Level",      "s"},
   { "Type",       "s"},
   { "JobId",      "i"},
   { "Client",     "s"},
   { "NumVols",    "i"},
   { "Pool",       "s"},
   { "Storage",    "s"},
   { "Catalog",    "s"},
   { "MediaType",  "s"},
   { "JobName",    "s"},
   { "JobStatus",  "s"},
   { "Priority",   "i"},
   { "VolumeName", "s"},
   { "CatalogRes", "(sssssis)"},
   { "JobErrors",  "i"},
   { "JobFiles",   "i"},
   { "SDJobFiles", "i"},
   { "SDErrors",   "i"},
   { "FDJobStatus","s"},
   { "SDJobStatus","s"},

   { NULL,             NULL}
};

/* Writable variables */
static struct s_vars setvars[] = {
   { "JobReport",   "s"},
   { "VolumeName",  "s"},
   { "Priority",    "i"},
   { "JobLevel",    "s"},

   { NULL,             NULL}
};


/* Return Job variables */
/* Returns:  NULL if error
 *           PyObject * return value if OK
 */
PyObject *job_getattr(PyObject *self, char *attrname)
{
   JCR *jcr;
   bool found = false;
   int i;
   char buf[10];
   char errmsg[200];

   Dmsg0(100, "In job_getattr.\n");
   jcr = get_jcr_from_PyObject(self);
   if (!jcr) {
      bstrncpy(errmsg, _("Job pointer not found."), sizeof(errmsg));
      goto bail_out;
   }
   for (i=0; getvars[i].name; i++) {
      if (strcmp(getvars[i].name, attrname) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      /* Try our methods */
      return Py_FindMethod(JobMethods, self, attrname);
   }
   switch (i) {
   case 0:                            /* Job */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->job->hdr.name);
   case 1:                            /* level */
      return Py_BuildValue((char *)getvars[i].fmt, job_level_to_str(jcr->getJobLevel()));
   case 2:                            /* type */
      return Py_BuildValue((char *)getvars[i].fmt, job_type_to_str(jcr->getJobType()));
   case 3:                            /* JobId */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->JobId);
   case 4:                            /* Client */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->client->hdr.name);
   case 5:                            /* NumVols */
      POOL_DBR pr;
      memset(&pr, 0, sizeof(pr));
      bstrncpy(pr.Name, jcr->pool->hdr.name, sizeof(pr.Name));
      if (db_get_pool_record(jcr, jcr->db, &pr)) {
         jcr->NumVols = pr.NumVols;
         return Py_BuildValue((char *)getvars[i].fmt, jcr->NumVols);
      } else {
         bsnprintf(errmsg, sizeof(errmsg), _("Pool record not found."));
         goto bail_out;
      }
   case 6:                            /* Pool */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->pool->name());
   case 7:                            /* Storage */
      if (jcr->wstore) {
         return Py_BuildValue((char *)getvars[i].fmt, jcr->wstore->name());
      } else if (jcr->rstore) {
         return Py_BuildValue((char *)getvars[i].fmt, jcr->rstore->name());
      } else {
         goto bail_out;
      }
   case 8:
      return Py_BuildValue((char *)getvars[i].fmt, jcr->catalog->name());
   case  9:                           /* MediaType */
      if (jcr->wstore) {
         return Py_BuildValue((char *)getvars[i].fmt, jcr->wstore->media_type);
      } else if (jcr->rstore) {
         return Py_BuildValue((char *)getvars[i].fmt, jcr->rstore->media_type);
      } else {
         goto bail_out;
      }
   case 10:                           /* JobName */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->Job);
   case 11:                           /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue((char *)getvars[i].fmt, buf);
   case 12:                           /* Priority */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->JobPriority);
   case 13:
      return Py_BuildValue((char *)getvars[i].fmt, jcr->VolumeName);
   case 14:                           /* CatalogRes */
      return Py_BuildValue((char *)getvars[i].fmt,
         jcr->catalog->db_name, jcr->catalog->db_address, 
         jcr->catalog->db_user, jcr->catalog->db_password,
         jcr->catalog->db_socket, jcr->catalog->db_port,
         db_get_type(jcr->db));
   case 15:                           /* JobErrors */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->JobErrors);
   case 16:                           /* JobFiles */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->JobFiles);
   case 17:                           /* SDJobFiles */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->SDJobFiles);
   case 18:                           /* SDErrors */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->SDErrors);
   case 19:                           /* FDJobStatus */
      buf[1] = 0;
      buf[0] = jcr->FDJobStatus;
      return Py_BuildValue((char *)getvars[i].fmt, buf);
   case 20:                           /* SDJobStatus */
      buf[1] = 0;
      buf[0] = jcr->SDJobStatus;
      return Py_BuildValue((char *)getvars[i].fmt, buf);
   }
   bsnprintf(errmsg, sizeof(errmsg), _("Attribute %s not found."), attrname);
bail_out:
   PyErr_SetString(PyExc_AttributeError, errmsg);
   return NULL;
}


/* Set Job variables */
/*  Returns:   0 for OK
 *            -1 for error
 */
int job_setattr(PyObject *self, char *attrname, PyObject *value)
{
   JCR *jcr;
   bool found = false;
   char *strval = NULL;
   int intval = 0;
   int i;

   Dmsg2(100, "In job_setattr=%s val=%p.\n", attrname, value);
   if (value == NULL) {                /* Cannot delete variables */
       goto bail_out;
   }
   jcr = get_jcr_from_PyObject(self);
   if (!jcr) {
      goto bail_out;
   }

   /* Find attribute name in list */
   for (i=0; setvars[i].name; i++) {
      if (strcmp(setvars[i].name, attrname) == 0) {
         found = true;
         break;
      }
   }
   if (!found) {
      goto bail_out;
   }
   /* Get argument value */
   if (setvars[i].fmt != NULL) {
      switch (setvars[i].fmt[0]) {
      case 's':
         if (!PyArg_Parse(value, (char *)setvars[i].fmt, &strval)) {
            PyErr_SetString(PyExc_TypeError, _("Read-only attribute"));
            return -1;
         }
         break;
      case 'i':
         if (!PyArg_Parse(value, (char *)setvars[i].fmt, &intval)) {
            PyErr_SetString(PyExc_TypeError, _("Read-only attribute"));
            return -1;
         }
         break;
      }
   }   
   switch (i) {
   case 0:                            /* JobReport */
      Jmsg(jcr, M_INFO, 0, "%s", strval);
      return 0;
   case 1:                            /* VolumeName */
      /* Make sure VolumeName is valid and we are in VolumeName event */
      if (strcmp("NewVolume", jcr->event) == 0 &&
          is_volume_name_legal(NULL, strval)) {
         pm_strcpy(jcr->VolumeName, strval);
         Dmsg1(100, "Set Vol=%s\n", strval);
         return 0;
      } else {
         jcr->VolumeName[0] = 0;
      }
      break;
   case 2:                            /* Priority */
      Dmsg1(000, "Set priority=%d\n", intval);
      if (intval >= 1 && intval <= 100) {
         jcr->JobPriority = intval;
      } else {
         PyErr_SetString(PyExc_ValueError, _("Priority must be 1-100"));
         return -1;
      }
   case 3:                            /* Job Level */
      if (strcmp("JobInit", jcr->event) != 0) {
         PyErr_SetString(PyExc_RuntimeError, _("Job Level can be set only during JobInit"));
         return -1;
      }
      if (strval != NULL) {
         for (i=0; joblevels[i].level_name; i++) {
            if (strcmp(strval, joblevels[i].level_name) == 0) {
               if (joblevels[i].job_type == jcr->getJobType()) {
                  jcr->setJobLevel(joblevels[i].level);
                  jcr->jr.JobLevel = jcr->getJobLevel();
                  return 0;
               }
            }
         }
      }
      PyErr_SetString(PyExc_ValueError, _("Bad JobLevel string"));
      return -1;
   }
bail_out:
   PyErr_SetString(PyExc_AttributeError, attrname);
   return -1;
}

/*
 * Set pointer to instantiated events class
 */
static PyObject *set_job_events(PyObject *self, PyObject *arg)
{
   PyObject *eObject;
   JCR *jcr;

   Dmsg0(100, "In set_job_events.\n");
   if (!PyArg_ParseTuple(arg, "O:set_events", &eObject)) {
      Dmsg0(000, "Error in ParseTuple\n");
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   Py_XDECREF((PyObject *)jcr->Python_events);
   Py_INCREF(eObject);
   jcr->Python_events = (void *)eObject;
   Py_INCREF(Py_None);
   return Py_None;
}

/* Run a Bacula job */
static PyObject *job_run(PyObject *self, PyObject *arg)
{
   JCR *jcr;
   char *item;
   int stat;

   if (!PyArg_ParseTuple(arg, "s:run", &item)) {
      Dmsg0(000, "Error in ParseTuple\n");
      return NULL;
   }
   /* Release lock due to recursion */
// PyEval_ReleaseLock();
   jcr = get_jcr_from_PyObject(self);
   UAContext *ua = new_ua_context(jcr);
   ua->batch = true;
   pm_strcpy(ua->cmd, item);          /* copy command */
   parse_ua_args(ua);                 /* parse command */
   stat = run_cmd(ua, ua->cmd);
   free_ua_context(ua);
// PyEval_AcquireLock();
   return PyInt_FromLong((long)stat);
}

static PyObject *job_write(PyObject *self, PyObject *args)
{
   char *text = NULL;

   if (!PyArg_ParseTuple(args, "s:write", &text)) {
      Dmsg0(000, "Parse tuple error in job_write\n");
      return NULL;
   }
   if (text) {
      JCR *jcr = get_jcr_from_PyObject(self);
      Jmsg(jcr, M_INFO, 0, "%s", text);
   }
   Py_INCREF(Py_None);
   return Py_None;
}

static PyObject *job_does_vol_exist(PyObject *self, PyObject *args)
{
   char *VolName = NULL;

   if (!PyArg_ParseTuple(args, "s:does_volume_exist", &VolName)) {
      Dmsg0(000, "Parse tuple error in job_does_vol_exist\n");
      return NULL;
   }
   if (VolName) {
      MEDIA_DBR mr;
      int ok;
      JCR *jcr = get_jcr_from_PyObject(self);
      bstrncpy(mr.VolumeName, VolName, sizeof(mr.VolumeName));
      ok = db_get_media_record(jcr, jcr->db, &mr);
      return Py_BuildValue("i", ok);
   }
   Py_INCREF(Py_None);
   return Py_None;
}


static PyObject *job_cancel(PyObject *self, PyObject *args)
{
   JobId_t JobId = 0;
   JCR *jcr;
   bool found = false;

   if (!PyArg_ParseTuple(args, "i:cancel", &JobId)) {
      Dmsg0(000, "Parse tuple error in job_write\n");
      return NULL;
   }
   foreach_jcr(jcr) {
      if (jcr->JobId == 0) {
         continue;
      }
      if (jcr->JobId == JobId) {
         found = true;
         break;
      }
   }
   /* endeach_jcr(jcr) not needed because freed below */

   if (!found) {
      /* ***FIXME*** raise exception */
      return NULL;
   }
// PyEval_ReleaseLock();
   UAContext *ua = new_ua_context(jcr);
   ua->batch = true;
   if (!cancel_job(ua, jcr)) {
      /* ***FIXME*** raise exception */
      return NULL;
   }
   free_ua_context(ua);
   free_jcr(jcr);
// PyEval_AcquireLock();   
   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Generate a Job event, which means look up the event
 *  method defined by the user, and if it exists, 
 *  call it.
 */
int generate_job_event(JCR *jcr, const char *event)
{
   PyObject *method = NULL;
   PyObject *Job = (PyObject *)jcr->Python_job;
   PyObject *events = (PyObject *)jcr->Python_events;
   PyObject *result = NULL;
   int stat = 0;

   if (!Job || !events) {
      return 0;
   }

   lock_python();
// PyEval_AcquireLock();

   method = find_method(events, method, event);
   if (!method) {
      goto bail_out;
   }

   bstrncpy(jcr->event, event, sizeof(jcr->event));
   result = PyObject_CallFunction(method, (char *)"O", Job);
   jcr->event[0] = 0;             /* no event in progress */
   if (result == NULL) {
      if (PyErr_Occurred()) {
         PyErr_Print();
         Dmsg1(000, "Error in Python method %s\n", event);
      }
   } else {
      stat = 1;
   }
   Py_XDECREF(result);

bail_out:
   unlock_python();
// PyEval_ReleaseLock();
   return stat;
}

bool python_set_prog(JCR*, char const*) { return false; }

#else

/* Dummy if Python not configured */
int generate_job_event(JCR *jcr, const char *event) { return 1; }
   

#endif /* HAVE_PYTHON */
