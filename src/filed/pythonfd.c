/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2005-2008 Free Software Foundation Europe e.V.

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
 * Bacula interface to Python for the File Daemon
 *
 * Kern Sibbald, March MMV
 *
 *   Version $Id$
 *
 */

#include "bacula.h"
#include "filed.h"

#ifdef HAVE_PYTHON
#undef _POSIX_C_SOURCE
#include <Python.h>

#include <lib/pythonlib.h>

/* Forward referenced functions */
static PyObject *set_job_events(PyObject *self, PyObject *arg);
static PyObject *job_write(PyObject *self, PyObject *arg);

PyMethodDef JobMethods[] = {
    {"set_events", set_job_events, METH_VARARGS, "Set Job events"},
    {"write", job_write, METH_VARARGS, "Write to output"},
    {NULL, NULL, 0, NULL}             /* last item */
};


struct s_vars {
   const char *name;
   const char *fmt;
};

/* Read-only variables */
static struct s_vars getvars[] = {
   { NT_("FDName"),     "s"},          /* 0 */
   { NT_("Level"),      "s"},          /* 1 */
   { NT_("Type"),       "s"},          /* 2 */
   { NT_("JobId"),      "i"},          /* 3 */
   { NT_("Client"),     "s"},          /* 4 */
   { NT_("JobName"),    "s"},          /* 5 */
   { NT_("JobStatus"),  "s"},          /* 6 */

   { NULL,             NULL}
};

/* Writable variables */
static struct s_vars setvars[] = {
   { NT_("JobReport"),   "s"},

   { NULL,             NULL}
};

/* Return Job variables */
PyObject *job_getattr(PyObject *self, char *attrname)
{
   JCR *jcr;
   bool found = false;
   int i;
   char buf[10];
   char errmsg[200];

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
   case 0:                            /* FD's name */
      return Py_BuildValue((char *)getvars[i].fmt, my_name);
   case 1:                            /* level */
      return Py_BuildValue((char *)getvars[i].fmt, job_level_to_str(jcr->getJobLevel()));
   case 2:                            /* type */
      return Py_BuildValue((char *)getvars[i].fmt, job_type_to_str(jcr->getJobType()));
   case 3:                            /* JobId */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->JobId);
   case 4:                            /* Client */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->client_name);
   case 5:                            /* JobName */
      return Py_BuildValue((char *)getvars[i].fmt, jcr->Job);
   case 6:                            /* JobStatus */
      buf[1] = 0;
      buf[0] = jcr->JobStatus;
      return Py_BuildValue((char *)getvars[i].fmt, buf);
   }
   bsnprintf(errmsg, sizeof(errmsg), _("Attribute %s not found."), attrname);
bail_out:
   PyErr_SetString(PyExc_AttributeError, errmsg);
   return NULL;
}

int job_setattr(PyObject *self, char *attrname, PyObject *value)
{
  JCR *jcr;
   bool found = false;
   char *strval = NULL;
   char buf[200];
   char *errmsg;
   int i;

   Dmsg2(100, "In job_setattr=%s val=%p.\n", attrname, value);
   if (value == NULL) {                /* Cannot delete variables */
      bsnprintf(buf, sizeof(buf), _("Cannot delete attribute %s"), attrname);
      errmsg = buf;
      goto bail_out;
   }
   jcr = get_jcr_from_PyObject(self);
   if (!jcr) {
      errmsg = _("Job pointer not found.");
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
      bsnprintf(buf, sizeof(buf), _("Cannot find attribute %s"), attrname);
      errmsg = buf;
      goto bail_out;
   }
   /* Get argument value ***FIXME*** handle other formats */
   if (setvars[i].fmt != NULL) {
      if (!PyArg_Parse(value, (char *)setvars[i].fmt, &strval)) {
         PyErr_SetString(PyExc_TypeError, _("Read-only attribute"));
         return -1;
      }
   }   
   switch (i) {
   case 0:                            /* JobReport */
      Jmsg(jcr, M_INFO, 0, "%s", strval);
      return 0;
   }
   bsnprintf(buf, sizeof(buf), _("Cannot find attribute %s"), attrname);
   errmsg = buf;
bail_out:
   PyErr_SetString(PyExc_AttributeError, errmsg);
   return -1;
}


static PyObject *job_write(PyObject *self, PyObject *args)
{
   char *text = NULL;

   if (!PyArg_ParseTuple(args, "s:write", &text)) {
      Pmsg0(000, "Parse tuple error in job_write\n");
      return NULL;
   }
   if (text) {
      Jmsg(NULL, M_INFO, 0, "%s", text);
   }
   Py_INCREF(Py_None);
   return Py_None;
}


static PyObject *set_job_events(PyObject *self, PyObject *arg)
{
   PyObject *eObject;
   JCR *jcr;

   Dmsg0(100, "In set_job_events.\n");
   if (!PyArg_ParseTuple(arg, "O", &eObject)) {
      Pmsg0(000, "Parse error looking for Object argument\n");
      return NULL;
   }
   jcr = get_jcr_from_PyObject(self);
   if (!jcr) {
      PyErr_SetString(PyExc_AttributeError, _("Job pointer not found."));
      return NULL;
   }
   Py_XDECREF((PyObject *)jcr->Python_events);  /* release any old events Object */
   Py_INCREF(eObject);
   jcr->Python_events = (void *)eObject;        /* set new events */

   Py_INCREF(Py_None);
   return Py_None;
}


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
         Pmsg1(000, "Error in Python method %s\n", event);
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


#else

/* Dummy if Python not configured */
int generate_job_event(JCR *jcr, const char *event)
{ return 1; }


#endif /* HAVE_PYTHON */
