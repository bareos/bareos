/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2013-2014 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

   This program is Free Software; you can modify it under the terms of
   version three of the GNU Affero General Public License as published by the Free
   Software Foundation, which is listed in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/**
 * @file
 * This defines the Python types in C++ and the callbacks from Python we support.
 */

#ifndef BAREOS_PLUGINS_FILED_PYTHON_FD_H_
#define BAREOS_PLUGINS_FILED_PYTHON_FD_H_ 1

#include "structmember.h"

namespace filedaemon {

/**
 * This defines the arguments that the plugin parser understands.
 */
enum plugin_argument_type {
   argument_none,
   argument_module_path,
   argument_module_name
};

struct plugin_argument {
   const char *name;
   enum plugin_argument_type type;
};

static plugin_argument plugin_arguments[] = {
   { "module_path", argument_module_path },
   { "module_name", argument_module_name },
   { NULL, argument_none }
};

/**
 * Python structures mapping C++ ones.
 */

/**
 * This packet is used for the restore objects.
 * It is passed to the plugin when restoring the object.
 */
typedef struct {
   PyObject_HEAD
   PyObject *object_name;            /* Object name */
   PyObject *object;                 /* Restore object data to restore */
   char *plugin_name;                /* Plugin name */
   int32_t object_type;              /* FT_xx for this file */
   int32_t object_len;               /* restore object length */
   int32_t object_full_len;          /* restore object uncompressed length */
   int32_t object_index;             /* restore object index */
   int32_t object_compression;       /* set to compression type */
   int32_t stream;                   /* attribute stream id */
   uint32_t JobId;                   /* JobId object came from */
} PyRestoreObject;

/**
 * Forward declarations of type specific functions.
 */
static void PyRestoreObject_dealloc(PyRestoreObject *self);
static int PyRestoreObject_init(PyRestoreObject *self, PyObject *args, PyObject *kwds);
static PyObject *PyRestoreObject_repr(PyRestoreObject *self);

static PyMethodDef PyRestoreObject_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PyRestoreObject_members[] = {
   { (char *)"object_name", T_OBJECT, offsetof(PyRestoreObject, object_name), 0, (char *)"Object Name" },
   { (char *)"object", T_OBJECT, offsetof(PyRestoreObject, object), 0, (char *)"Object Content" },
   { (char *)"plugin_name", T_STRING, offsetof(PyRestoreObject, plugin_name), 0, (char *)"Plugin Name" },
   { (char *)"object_type", T_INT, offsetof(PyRestoreObject, object_type), 0, (char *)"Object Type" },
   { (char *)"object_len", T_INT, offsetof(PyRestoreObject, object_len), 0, (char *)"Object Length" },
   { (char *)"object_full_len", T_INT, offsetof(PyRestoreObject, object_full_len), 0, (char *)"Object Full Length" },
   { (char *)"object_index", T_INT, offsetof(PyRestoreObject, object_index), 0, (char *)"Object Index" },
   { (char *)"object_compression", T_INT, offsetof(PyRestoreObject, object_compression), 0, (char *)"Object Compression" },
   { (char *)"stream", T_INT, offsetof(PyRestoreObject, stream), 0, (char *)"Attribute Stream" },
   { (char *)"jobid", T_UINT, offsetof(PyRestoreObject, JobId), 0, (char *)"Jobid" },
   { NULL }
};

static PyTypeObject PyRestoreObjectType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "restore_object",                  /* tp_name */
   sizeof(PyRestoreObject),           /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PyRestoreObject_dealloc, /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PyRestoreObject_repr,    /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "io_pkt object",                   /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PyRestoreObject_methods,           /* tp_methods */
   PyRestoreObject_members,           /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PyRestoreObject_init,    /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * The PyStatPacket type
 */
typedef struct {
   PyObject_HEAD
   uint32_t dev;
   uint64_t ino;
   uint16_t mode;
   int16_t nlink;
   uint32_t uid;
   uint32_t gid;
   uint32_t rdev;
   uint64_t size;
   time_t atime;
   time_t mtime;
   time_t ctime;
   uint32_t blksize;
   uint64_t blocks;
} PyStatPacket;

/**
 * Forward declarations of type specific functions.
 */
static void PyStatPacket_dealloc(PyStatPacket *self);
static int PyStatPacket_init(PyStatPacket *self, PyObject *args, PyObject *kwds);
static PyObject *PyStatPacket_repr(PyStatPacket *self);

static PyMethodDef PyStatPacket_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PyStatPacket_members[] = {
   { (char *)"dev", T_UINT, offsetof(PyStatPacket, dev), 0, (char *)"Device" },
   { (char *)"ino", T_ULONGLONG, offsetof(PyStatPacket, ino), 0, (char *)"Inode number" },
   { (char *)"mode", T_USHORT, offsetof(PyStatPacket, mode), 0, (char *)"Mode" },
   { (char *)"nlink", T_USHORT, offsetof(PyStatPacket, nlink), 0, (char *)"Number of hardlinks" },
   { (char *)"uid", T_UINT, offsetof(PyStatPacket, uid), 0, (char *)"User Id" },
   { (char *)"gid", T_UINT, offsetof(PyStatPacket, gid), 0, (char *)"Group Id" },
   { (char *)"rdev", T_UINT, offsetof(PyStatPacket, rdev), 0, (char *)"Rdev" },
   { (char *)"size", T_ULONGLONG, offsetof(PyStatPacket, size), 0, (char *)"Size" },
   { (char *)"atime", T_UINT, offsetof(PyStatPacket, atime), 0, (char *)"Access Time" },
   { (char *)"mtime", T_UINT, offsetof(PyStatPacket, mtime), 0, (char *)"Modification Time" },
   { (char *)"ctime", T_UINT, offsetof(PyStatPacket, ctime), 0, (char *)"Change Time" },
   { (char *)"blksize", T_UINT, offsetof(PyStatPacket, blksize), 0, (char *)"Blocksize" },
   { (char *)"blocks", T_ULONGLONG, offsetof(PyStatPacket, blocks), 0, (char *)"Blocks" },
   { NULL }
};

static PyTypeObject PyStatPacketType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "stat_pkt",                        /* tp_name */
   sizeof(PyStatPacket),              /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PyStatPacket_dealloc,  /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PyStatPacket_repr,       /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "io_pkt object",                   /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PyStatPacket_methods,              /* tp_methods */
   PyStatPacket_members,              /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PyStatPacket_init,       /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * The PySavePacket type
 */
typedef struct {
   PyObject_HEAD
   PyObject *fname;                   /* Full path and filename */
   PyObject *link;                    /* Link name if any */
   PyObject *statp;                   /* System stat() packet for file */
   int32_t type;                      /* FT_xx for this file */
   PyObject *flags;                   /* Bareos internal flags */
   bool no_read;                      /* During the save, the file should not be saved */
   bool portable;                     /* set if data format is portable */
   bool accurate_found;               /* Found in accurate list (valid after CheckChanges()) */
   char *cmd;                         /* Command */
   time_t save_time;                  /* Start of incremental time */
   uint32_t delta_seq;                /* Delta sequence number */
   PyObject *object_name;             /* Object name to create */
   PyObject *object;                  /* Restore object data to save */
   int32_t object_len;                /* Restore object length */
   int32_t object_index;              /* Restore object index */
} PySavePacket;

/**
 * Forward declarations of type specific functions.
 */
static void PySavePacket_dealloc(PySavePacket *self);
static int PySavePacket_init(PySavePacket *self, PyObject *args, PyObject *kwds);
static PyObject *PySavePacket_repr(PySavePacket *self);

static PyMethodDef PySavePacket_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PySavePacket_members[] = {
   { (char *)"fname", T_OBJECT, offsetof(PySavePacket, fname), 0, (char *)"Filename" },
   { (char *)"link", T_OBJECT, offsetof(PySavePacket, link), 0, (char *)"Linkname" },
   { (char *)"statp", T_OBJECT, offsetof(PySavePacket, statp), 0, (char *)"Stat Packet" },
   { (char *)"type", T_INT, offsetof(PySavePacket, type), 0, (char *)"Type" },
   { (char *)"flags", T_OBJECT, offsetof(PySavePacket, flags), 0, (char *)"Flags" },
   { (char *)"no_read", T_BOOL, offsetof(PySavePacket, no_read), 0, (char *)"No Read" },
   { (char *)"portable", T_BOOL, offsetof(PySavePacket, portable), 0, (char *)"Portable" },
   { (char *)"accurate_found", T_BOOL, offsetof(PySavePacket, accurate_found), 0, (char *)"Accurate Found" },
   { (char *)"cmd", T_STRING, offsetof(PySavePacket, cmd), 0, (char *)"Command" },
   { (char *)"save_time", T_UINT, offsetof(PySavePacket, save_time), 0, (char *)"Save Time" },
   { (char *)"delta_seq", T_UINT, offsetof(PySavePacket, delta_seq), 0, (char *)"Delta Sequence" },
   { (char *)"object_name", T_OBJECT, offsetof(PySavePacket, object_name), 0, (char *)"Restore Object Name" },
   { (char *)"object", T_OBJECT, offsetof(PySavePacket, object), 0, (char *)"Restore ObjectName" },
   { (char *)"object_len", T_INT, offsetof(PySavePacket, object_len), 0, (char *)"Restore ObjectLen" },
   { (char *)"object_index", T_INT, offsetof(PySavePacket, object_index), 0, (char *)"Restore ObjectIndex" },
   { NULL }
};

static PyTypeObject PySavePacketType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "save_pkt",                        /* tp_name */
   sizeof(PySavePacket),              /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PySavePacket_dealloc,  /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PySavePacket_repr,       /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "save_pkt object",                 /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PySavePacket_methods,              /* tp_methods */
   PySavePacket_members,              /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PySavePacket_init,       /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * The PyRestorePacket type
 */
typedef struct {
   PyObject_HEAD
   int32_t stream;                    /* Attribute stream id */
   int32_t data_stream;               /* Id of data stream to follow */
   int32_t type;                      /* File type FT */
   int32_t file_index;                /* File index */
   int32_t LinkFI;                    /* File index to data if hard link */
   uint32_t uid;                      /* Userid */
   PyObject *statp;                   /* Decoded stat packet */
   const char *attrEx;                /* Extended attributes if any */
   const char *ofname;                /* Output filename */
   const char *olname;                /* Output link name */
   const char *where;                 /* Where */
   const char *RegexWhere;            /* Regex where */
   int replace;                       /* Replace flag */
   int create_status;                 /* Status from createFile() */
} PyRestorePacket;

/**
 * Forward declarations of type specific functions.
 */
static void PyRestorePacket_dealloc(PyRestorePacket *self);
static int PyRestorePacket_init(PyRestorePacket *self, PyObject *args, PyObject *kwds);
static PyObject *PyRestorePacket_repr(PyRestorePacket *self);

static PyMethodDef PyRestorePacket_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PyRestorePacket_members[] = {
   { (char *)"stream", T_INT, offsetof(PyRestorePacket, stream), 0, (char *)"Attribute stream id" },
   { (char *)"data_stream", T_INT, offsetof(PyRestorePacket, data_stream), 0, (char *)"Id of data stream to follow" },
   { (char *)"type", T_INT, offsetof(PyRestorePacket, type), 0, (char *)"File type FT" },
   { (char *)"file_index", T_INT, offsetof(PyRestorePacket, file_index), 0, (char *)"File index" },
   { (char *)"linkFI", T_INT, offsetof(PyRestorePacket, LinkFI), 0, (char *)"File index to data if hard link" },
   { (char *)"uid", T_UINT, offsetof(PyRestorePacket, uid), 0, (char *)"User Id" },
   { (char *)"statp", T_OBJECT, offsetof(PyRestorePacket, statp), 0, (char *)"Stat Packet" },
   { (char *)"attrEX", T_STRING, offsetof(PyRestorePacket, attrEx), 0, (char *)"Extended attributes" },
   { (char *)"ofname", T_STRING, offsetof(PyRestorePacket, ofname), 0, (char *)"Output filename" },
   { (char *)"olname", T_STRING, offsetof(PyRestorePacket, olname), 0, (char *)"Output link name" },
   { (char *)"where", T_STRING, offsetof(PyRestorePacket, where), 0, (char *)"Where" },
   { (char *)"regexwhere", T_STRING, offsetof(PyRestorePacket, RegexWhere), 0, (char *)"Regex where" },
   { (char *)"replace", T_INT, offsetof(PyRestorePacket, replace), 0, (char *)"Replace flag" },
   { (char *)"create_status", T_INT, offsetof(PyRestorePacket, create_status), 0, (char *)"Status from createFile()" },
   { NULL }
};

static PyTypeObject PyRestorePacketType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "restore_pkt",                     /* tp_name */
   sizeof(PyRestorePacket),           /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PyRestorePacket_dealloc, /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PyRestorePacket_repr,    /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "restore_pkt object",              /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PyRestorePacket_methods,           /* tp_methods */
   PyRestorePacket_members,           /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PyRestorePacket_init,    /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * The PyIOPacket type
 */
typedef struct {
   PyObject_HEAD
   uint16_t func;                     /* Function code */
   int32_t count;                     /* Read/Write count */
   int32_t flags;                     /* Open flags */
   int32_t mode;                      /* Permissions for created files */
   PyObject *buf;                     /* Read/Write buffer */
   const char *fname;                 /* Open filename */
   int32_t status;                    /* Return status */
   int32_t io_errno;                  /* Errno code */
   int32_t lerror;                    /* Win32 error code */
   int32_t whence;                    /* Lseek argument */
   int64_t offset;                    /* Lseek argument */
   bool win32;                        /* Win32 GetLastError returned */
} PyIoPacket;

/**
 * Forward declarations of type specific functions.
 */
static void PyIoPacket_dealloc(PyIoPacket *self);
static int PyIoPacket_init(PyIoPacket *self, PyObject *args, PyObject *kwds);
static PyObject *PyIoPacket_repr(PyIoPacket *self);

static PyMethodDef PyIoPacket_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PyIoPacket_members[] = {
   { (char *)"func", T_USHORT, offsetof(PyIoPacket, func), 0, (char *)"Function code" },
   { (char *)"count", T_INT, offsetof(PyIoPacket, count), 0, (char *)"Read/write count" },
   { (char *)"flags", T_INT, offsetof(PyIoPacket, flags), 0, (char *)"Open flags" },
   { (char *)"mode", T_INT, offsetof(PyIoPacket, mode), 0, (char *)"Permissions for created files" },
   { (char *)"buf", T_OBJECT, offsetof(PyIoPacket, buf), 0, (char *)"Read/write buffer" },
   { (char *)"fname", T_STRING, offsetof(PyIoPacket, fname), 0, (char *)"Open filename" },
   { (char *)"status", T_INT, offsetof(PyIoPacket, status), 0, (char *)"Return status" },
   { (char *)"io_errno", T_INT, offsetof(PyIoPacket, io_errno), 0, (char *)"Errno code" },
   { (char *)"lerror", T_INT, offsetof(PyIoPacket, lerror), 0, (char *)"Win32 error code" },
   { (char *)"whence", T_INT, offsetof(PyIoPacket, whence), 0, (char *)"Lseek argument" },
   { (char *)"offset", T_LONGLONG, offsetof(PyIoPacket, offset), 0, (char *)"Lseek argument" },
   { (char *)"win32", T_BOOL, offsetof(PyIoPacket, win32), 0, (char *)"Win32 GetLastError returned" },
   { NULL }
};

static PyTypeObject PyIoPacketType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "io_pkt",                          /* tp_name */
   sizeof(PyIoPacket),                /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PyIoPacket_dealloc,    /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PyIoPacket_repr,         /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "io_pkt object",                   /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PyIoPacket_methods,                /* tp_methods */
   PyIoPacket_members,                /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PyIoPacket_init,         /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * The PyAclPacket type
 */
typedef struct {
   PyObject_HEAD
   const char *fname;                 /* Filename */
   PyObject *content;                 /* ACL content */
} PyAclPacket;

/**
 * Forward declarations of type specific functions.
 */
static void PyAclPacket_dealloc(PyAclPacket *self);
static int PyAclPacket_init(PyAclPacket *self, PyObject *args, PyObject *kwds);
static PyObject *PyAclPacket_repr(PyAclPacket *self);

static PyMethodDef PyAclPacket_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PyAclPacket_members[] = {
   { (char *)"fname", T_STRING, offsetof(PyAclPacket, fname), 0, (char *)"Filename" },
   { (char *)"content", T_OBJECT, offsetof(PyAclPacket, content), 0, (char *)"ACL content buffer" },
   { NULL }
};

static PyTypeObject PyAclPacketType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "acl_pkt",                         /* tp_name */
   sizeof(PyAclPacket),               /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PyAclPacket_dealloc,   /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PyAclPacket_repr,        /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "acl_pkt object",                  /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PyAclPacket_methods,               /* tp_methods */
   PyAclPacket_members,               /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PyAclPacket_init,        /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * The PyXattrPacket type
 */
typedef struct {
   PyObject_HEAD
   const char *fname;                 /* Filename */
   PyObject *name;                    /* XATTR name */
   PyObject *value;                   /* XATTR value */
} PyXattrPacket;

/**
 * Forward declarations of type specific functions.
 */
static void PyXattrPacket_dealloc(PyXattrPacket *self);
static int PyXattrPacket_init(PyXattrPacket *self, PyObject *args, PyObject *kwds);
static PyObject *PyXattrPacket_repr(PyXattrPacket *self);

static PyMethodDef PyXattrPacket_methods[] = {
   { NULL }                           /* Sentinel */
};

static PyMemberDef PyXattrPacket_members[] = {
   { (char *)"fname", T_STRING, offsetof(PyXattrPacket, fname), 0, (char *)"Filename" },
   { (char *)"name", T_OBJECT, offsetof(PyXattrPacket, name), 0, (char *)"XATTR name buffer" },
   { (char *)"value", T_OBJECT, offsetof(PyXattrPacket, value), 0, (char *)"XATTR value buffer" },
   { NULL }
};

static PyTypeObject PyXattrPacketType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   "xattr_pkt",                       /* tp_name */
   sizeof(PyXattrPacket),             /* tp_basicsize */
   0,                                 /* tp_itemsize */
   (destructor)PyXattrPacket_dealloc, /* tp_dealloc */
   0,                                 /* tp_print */
   0,                                 /* tp_getattr */
   0,                                 /* tp_setattr */
   0,                                 /* tp_compare */
   (reprfunc)PyXattrPacket_repr,      /* tp_repr */
   0,                                 /* tp_as_number */
   0,                                 /* tp_as_sequence */
   0,                                 /* tp_as_mapping */
   0,                                 /* tp_hash */
   0,                                 /* tp_call */
   0,                                 /* tp_str */
   0,                                 /* tp_getattro */
   0,                                 /* tp_setattro */
   0,                                 /* tp_as_buffer */
   Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags*/
   "xattr_pkt object",                /* tp_doc */
   0,                                 /* tp_traverse */
   0,                                 /* tp_clear */
   0,                                 /* tp_richcompare */
   0,                                 /* tp_weaklistoffset */
   0,                                 /* tp_iter */
   0,                                 /* tp_iternext */
   PyXattrPacket_methods,             /* tp_methods */
   PyXattrPacket_members,             /* tp_members */
   0,                                 /* tp_getset */
   0,                                 /* tp_base */
   0,                                 /* tp_dict */
   0,                                 /* tp_descr_get */
   0,                                 /* tp_descr_set */
   0,                                 /* tp_dictoffset */
   (initproc)PyXattrPacket_init,      /* tp_init */
   0,                                 /* tp_alloc */
   0,                                 /* tp_new */
};

/**
 * Callback methods from Python.
 */
static PyObject *PyBareosGetValue(PyObject *self, PyObject *args);
static PyObject *PyBareosSetValue(PyObject *self, PyObject *args);
static PyObject *PyBareosDebugMessage(PyObject *self, PyObject *args);
static PyObject *PyBareosJobMessage(PyObject *self, PyObject *args);
static PyObject *PyBareosRegisterEvents(PyObject *self, PyObject *args);
static PyObject *PyBareosUnRegisterEvents(PyObject *self, PyObject *args);
static PyObject *PyBareosGetInstanceCount(PyObject *self, PyObject *args);
static PyObject *PyBareosAddExclude(PyObject *self, PyObject *args);
static PyObject *PyBareosAddInclude(PyObject *self, PyObject *args);
static PyObject *PyBareosAddOptions(PyObject *self, PyObject *args);
static PyObject *PyBareosAddRegex(PyObject *self, PyObject *args);
static PyObject *PyBareosAddWild(PyObject *self, PyObject *args);
static PyObject *PyBareosNewOptions(PyObject *self, PyObject *args);
static PyObject *PyBareosNewInclude(PyObject *self, PyObject *args);
static PyObject *PyBareosNewPreInclude(PyObject *self, PyObject *args);
static PyObject *PyBareosCheckChanges(PyObject *self, PyObject *args);
static PyObject *PyBareosAcceptFile(PyObject *self, PyObject *args);
static PyObject *PyBareosSetSeenBitmap(PyObject *self, PyObject *args);
static PyObject *PyBareosClearSeenBitmap(PyObject *self, PyObject *args);

static PyMethodDef BareosFDMethods[] = {
   { "GetValue", PyBareosGetValue, METH_VARARGS, "Get a Plugin value" },
   { "SetValue", PyBareosSetValue, METH_VARARGS, "Set a Plugin value" },
   { "DebugMessage", PyBareosDebugMessage, METH_VARARGS, "Print a Debug message" },
   { "JobMessage", PyBareosJobMessage, METH_VARARGS, "Print a Job message" },
   { "RegisterEvents", PyBareosRegisterEvents, METH_VARARGS, "Register Plugin Events" },
   { "UnRegisterEvents", PyBareosUnRegisterEvents, METH_VARARGS, "Unregister Plugin Events" },
   { "GetInstanceCount", PyBareosGetInstanceCount, METH_VARARGS, "Get number of instances of current plugin" },
   { "AddExclude", PyBareosAddExclude, METH_VARARGS, "Add Exclude pattern" },
   { "AddInclude", PyBareosAddInclude, METH_VARARGS, "Add Include pattern" },
   { "AddOptions", PyBareosAddOptions, METH_VARARGS, "Add Include options" },
   { "AddRegex", PyBareosAddRegex, METH_VARARGS, "Add regex" },
   { "AddWild", PyBareosAddWild, METH_VARARGS, "Add wildcard" },
   { "NewOptions", PyBareosNewOptions, METH_VARARGS, "Add new option block" },
   { "NewInclude", PyBareosNewInclude, METH_VARARGS, "Add new include block" },
   { "NewPreInclude", PyBareosNewPreInclude, METH_VARARGS, "Add new pre include block" },
   { "CheckChanges", PyBareosCheckChanges, METH_VARARGS, "Check if a file have to be backuped using Accurate code" },
   { "AcceptFile", PyBareosAcceptFile, METH_VARARGS, "Check if a file would be saved using current Include/Exclude code" },
   { "SetSeenBitmap", PyBareosSetSeenBitmap, METH_VARARGS, "Set bit in the Accurate Seen bitmap" },
   { "ClearSeenBitmap", PyBareosClearSeenBitmap, METH_VARARGS, "Clear bit in the Accurate Seen bitmap" },
   { NULL, NULL, 0, NULL }
};

} /* namespace filedaemon */
#endif /* BAREOS_PLUGINS_FILED_PYTHON_FD_H_ */
