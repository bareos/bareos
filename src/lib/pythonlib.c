/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2004-2011 Free Software Foundation Europe e.V.

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
 * Bacula common code library interface to Python
 *
 * Kern Sibbald, November MMIV
 *
 */

#include "bacula.h"
#include "jcr.h"

#ifdef HAVE_PYTHON

#undef _POSIX_C_SOURCE
#include <Python.h>

#include "pythonlib.h"

/* Forward referenced subroutines */
static void init_python_lock();
static void term_python_lock();

static PyObject *bacula_module = NULL;    /* We create this */
static PyObject *StartUp_module = NULL;   /* We import this */

/* These are the daemon events or methods that are defined */
static PyObject *JobStart_method = NULL;
static PyObject *JobEnd_method = NULL;
static PyObject *Exit_method = NULL;

static PyObject *set_bacula_events(PyObject *self, PyObject *args);
static PyObject *bacula_write(PyObject *self, PyObject *args);

PyObject *find_method(PyObject *eventsObject, PyObject *method, const char *name);

/* Define Bacula daemon method entry points */
static PyMethodDef BaculaMethods[] = {
    {"set_events", set_bacula_events, METH_VARARGS, "Define Bacula events."},
    {"write", bacula_write, METH_VARARGS, "Write output."},
    {NULL, NULL, 0, NULL}             /* last item */
};

static char my_version[] = VERSION " " BDATE;

/*
 * This is a Bacula Job type as defined in Python. We store a pointer
 * to the jcr. That is all we need, but the user's script may keep
 * local data attached to this.
 */
typedef struct s_JobObject {
    PyObject_HEAD
    JCR *jcr;
} JobObject;

static PyTypeObject JobType = {
    PyObject_HEAD_INIT(NULL)
    /* Other items filled in in code below */
};

/* Return the JCR pointer from the JobObject */
JCR *get_jcr_from_PyObject(PyObject *self)
{
   if (!self) {
      return NULL;
   }
   return ((JobObject *)self)->jcr;
}

/* Start the interpreter */
void init_python_interpreter(init_python_interpreter_args *args)
{
   char buf[MAXSTRING];

   if (!args->scriptdir || args->scriptdir[0] == 0) {
      Dmsg1(100, "No script dir. prog=%s\n", args->modulename);
      return;
   }
   Dmsg2(100, "Script dir=%s prog=%s\n", args->scriptdir, args->modulename);

   Py_SetProgramName((char *)args->progname);
   Py_Initialize();
   PyEval_InitThreads();
   bacula_module = Py_InitModule("bacula", BaculaMethods);
   PyModule_AddStringConstant(bacula_module, "Name", my_name);
   PyModule_AddStringConstant(bacula_module, "Version", my_version);
   PyModule_AddStringConstant(bacula_module, "ConfigFile", (char *)args->configfile);
   PyModule_AddStringConstant(bacula_module, "WorkingDir", (char *)args->workingdir);
   if (!bacula_module) {
      Jmsg0(NULL, M_ERROR_TERM, 0, _("Could not initialize Python\n"));
   }
   bsnprintf(buf, sizeof(buf), "import sys\n"
            "sys.path.append('%s')\n", args->scriptdir);
   if (PyRun_SimpleString(buf) != 0) {
      Jmsg1(NULL, M_ERROR_TERM, 0, _("Could not Run Python string %s\n"), buf);
   }   

   /* Explicitly set values we want */
   JobType.tp_name = "Bacula.Job";
   JobType.tp_basicsize = sizeof(JobObject);
   JobType.tp_flags = Py_TPFLAGS_DEFAULT;
   JobType.tp_doc = "Bacula Job object";
   JobType.tp_getattr = args->job_getattr;
   JobType.tp_setattr = args->job_setattr;

   if (PyType_Ready(&JobType) != 0) {
      Jmsg0(NULL, M_ERROR_TERM, 0, _("Could not initialize Python Job type.\n"));
      PyErr_Print();
   }   
   StartUp_module = PyImport_ImportModule((char *)args->modulename);
   if (!StartUp_module) {
      Emsg2(M_ERROR, 0, _("Could not import Python script %s/%s. Python disabled.\n"),
           args->scriptdir, args->modulename);
      if (PyErr_Occurred()) {
         PyErr_Print();
         Dmsg0(000, "Python Import error.\n");
      }
   }
   PyEval_ReleaseLock();
   init_python_lock();
}

void term_python_interpreter()
{
   if (StartUp_module) {
      Py_XDECREF(StartUp_module);
      Py_Finalize();
   }
   term_python_lock();
}

static PyObject *set_bacula_events(PyObject *self, PyObject *args)
{
   PyObject *eObject;

   Dmsg0(100, "In set_bacula_events.\n");
   if (!PyArg_ParseTuple(args, "O:set_bacula_events", &eObject)) {
      return NULL;
   }
   JobStart_method = find_method(eObject, JobStart_method, "JobStart");
   JobEnd_method = find_method(eObject, JobEnd_method, "JobEnd");
   Exit_method = find_method(eObject, Exit_method, "Exit");

   Py_XINCREF(eObject);
   Py_INCREF(Py_None);
   return Py_None;
}

/* Write text to daemon output */
static PyObject *bacula_write(PyObject *self, PyObject *args)
{
   char *text;
   if (!PyArg_ParseTuple(args, "s:write", &text)) {
      return NULL;
   }
   if (text) {
      Jmsg(NULL, M_INFO, 0, "%s", text);
   }
   Py_INCREF(Py_None);
   return Py_None;
}

/*
 * Check that a method exists and is callable.
 */
PyObject *find_method(PyObject *eventsObject, PyObject *method, const char *name)
{
   Py_XDECREF(method);             /* release old method if any */
   method = PyObject_GetAttrString(eventsObject, (char *)name);
   if (method == NULL) {
       Dmsg1(000, "Python method %s not found\n", name);
   } else if (PyCallable_Check(method) == 0) {
       Dmsg1(000, "Python object %s found but not a method.\n", name);
       Py_XDECREF(method);
       method = NULL;
   } else {
       Dmsg1(100, "Got method %s\n", name);
   }
   return method; 
}


/*
 * Generate and process a Bacula event by importing a Python
 *  module and running it.
 *
 *  Returns: 0 if Python not configured or module not found
 *          -1 on Python error
 *           1 OK
 */
int generate_daemon_event(JCR *jcr, const char *event)
{
   PyObject *pJob;
   int stat = -1;
   PyObject *result = NULL;
   char *obj_fmt = (char *)"O";

   if (!StartUp_module) {
      Dmsg0(100, "No startup module.\n");
      return 0;
   }

   Dmsg1(100, "event=%s\n", event);
   lock_python();
// PyEval_AcquireLock();
   if (strcmp(event, "JobStart") == 0) {
      if (!JobStart_method) {
         stat = 0;
         goto bail_out;
      }
      /* Create JCR argument to send to function */
      pJob = (PyObject *)PyObject_New(JobObject, &JobType);
      if (!pJob) {
         Jmsg(jcr, M_ERROR, 0, _("Could not create Python Job Object.\n"));
         goto bail_out;
      }
      ((JobObject *)pJob)->jcr = jcr;
      bstrncpy(jcr->event, event, sizeof(jcr->event));
      result = PyObject_CallFunction(JobStart_method, obj_fmt, pJob);
      jcr->event[0] = 0;             /* no event in progress */
      if (result == NULL) {
         JobStart_method = NULL;
         if (PyErr_Occurred()) {
            PyErr_Print();
            Dmsg0(000, "Python JobStart error.\n");
         }
         Jmsg(jcr, M_ERROR, 0, _("Python function \"%s\" not found.\n"), event);
         Py_XDECREF(pJob);
         goto bail_out;
      }
      jcr->Python_job = (void *)pJob;
      stat = 1;                       /* OK */
      goto jobstart_ok;

   } else if (strcmp(event, "JobEnd") == 0) {
      if (!JobEnd_method || !jcr->Python_job) {
         stat = 0;                    /* probably already here */
         goto bail_out;
      }
      bstrncpy(jcr->event, event, sizeof(jcr->event));
      Dmsg1(100, "Call daemon event=%s\n", event);
      result = PyObject_CallFunction(JobEnd_method, obj_fmt, jcr->Python_job);
      jcr->event[0] = 0;             /* no event in progress */
      if (result == NULL) {
         if (PyErr_Occurred()) {
            PyErr_Print();
            Dmsg2(000, "Python JobEnd error. job=%p JobId=%d\n", jcr->Python_job,
               jcr->JobId);
            JobEnd_method = NULL;
         }
         Jmsg(jcr, M_ERROR, 0, _("Python function \"%s\" not found.\n"), event);
         goto bail_out;
      }
      stat = 1;                    /* OK */
   } else if (strcmp(event, "Exit") == 0) {
      if (!Exit_method) {
         stat = 0;
         goto bail_out;
      }
      result = PyObject_CallFunction(Exit_method, NULL);
      if (result == NULL) {
         goto bail_out;
      }
      stat = 1;                    /* OK */
   } else {
      Jmsg1(jcr, M_ABORT, 0, _("Unknown Python daemon event %s\n"), event);
   }

bail_out:
   if (jcr) {
      Py_XDECREF((PyObject *)jcr->Python_job);
      jcr->Python_job = NULL;
      Py_XDECREF((PyObject *)jcr->Python_events);
      jcr->Python_events = NULL;
   }
   /* Fall through */
jobstart_ok:
   Py_XDECREF(result);
   unlock_python();
// PyEval_ReleaseLock();
   return stat; 
}

static brwlock_t python_rwlock;

static void init_python_lock()
{
   int errstat;
   if ((errstat=rwl_init(&python_rwlock)) != 0) {
      berrno be;
      Emsg1(M_ABORT, 0, _("Unable to initialize the Python lock. ERR=%s\n"),
            be.bstrerror(errstat));
   }

}

static void term_python_lock()
{
   rwl_destroy(&python_rwlock);
}

/* This applies to a drive and to Volumes */
void lock_python()
{
   int errstat;
   if ((errstat=rwl_writelock(&python_rwlock)) != 0) {
      berrno be;
      Emsg2(M_ABORT, 0, "Python rwl_writelock failure. stat=%d: ERR=%s\n",
           errstat, be.bstrerror(errstat));
   }
}

void unlock_python()
{
   int errstat;
   if ((errstat=rwl_writeunlock(&python_rwlock)) != 0) {
      berrno be;
      Emsg2(M_ABORT, 0, "Python rwl_writeunlock failure. stat=%d: ERR=%s\n",
           errstat, be.bstrerror(errstat));
   }
}

#else

/*
 *  No Python configured -- create external entry points and
 *    dummy routines so that library code can call this without
 *    problems even if it is not configured.
 */
int generate_daemon_event(JCR *jcr, const char *event) { return 0; }

#endif /* HAVE_PYTHON */
