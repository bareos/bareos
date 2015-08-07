/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2014 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * Kern Sibbald, January MM
 */

struct RES_ITEM;                        /* Declare forward referenced structure */
class RES;                              /* Declare forward referenced structure */

/*
 * Parser state
 */
enum parse_state {
   p_none,
   p_resource
};

/*
 * Password encodings.
 */
enum password_encoding {
   p_encoding_clear,
   p_encoding_md5
};

/*
 * Used for message destinations.
 */
struct s_mdestination {
   int code;
   const char *destination;
   bool where;
};

/*
 * Used for message types.
 */
struct s_mtypes {
   const char *name;
   uint32_t token;
};

/*
 * Used for certain KeyWord tables
 */
struct s_kw {
   const char *name;
   uint32_t token;
};

/*
 * Used to store passwords with their encoding.
 */
struct s_password {
   enum password_encoding encoding;
   char *value;
};

/*
 * This is the structure that defines the record types (items) permitted within each
 * resource. It is used to define the configuration tables.
 */
struct RES_ITEM {
   const char *name;                    /* Resource name i.e. Director, ... */
   const int type;
   union {
      char **value;                     /* Where to store the item */
      uint32_t *ui32value;
      int32_t *i32value;
      uint64_t *ui64value;
      int64_t *i64value;
      bool *boolvalue;
      utime_t *utimevalue;
      s_password *pwdvalue;
      RES **resvalue;
      alist **alistvalue;
      dlist **dlistvalue;
      char *bitvalue;
   };
   int32_t code;                        /* Item code/additional info */
   uint32_t flags;                      /* Flags: See CFG_ITEM_* */
   const char *default_value;           /* Default value */
   /*
    * version string in format: [start_version]-[end_version]
    * start_version: directive has been introduced in this version
    * end_version:   directive is deprecated since this version
    */
   const char *versions;
   /*
    * description of the directive, used for the documentation.
    * Full sentence.
    * Every new directive should have a description.
    */
   const char *description;
};

/* For storing name_addr items in res_items table */
#define ITEM(x) {(char **)&res_all.x}

#define MAX_RES_ITEMS 88                /* maximum resource items per RES */

/*
 * This is the universal header that is at the beginning of every resource record.
 */
class RES {
public:
   RES *next;                           /* Pointer to next resource of this type */
   char *name;                          /* Resource name */
   char *desc;                          /* Resource description */
   uint32_t rcode;                      /* Resource id or type */
   int32_t refcnt;                      /* Reference count for releasing */
   char item_present[MAX_RES_ITEMS];    /* Set if item is present in conf file */
   char inherit_content[MAX_RES_ITEMS]; /* Set if item has inherited content */
};

/*
 * Master Resource configuration structure definition
 * This is the structure that defines the resources that are available to this daemon.
 */
struct RES_TABLE {
   const char *name;                    /* Resource name */
   RES_ITEM *items;                     /* List of resource keywords */
   uint32_t rcode;                      /* Code if needed */
   uint32_t size;                       /* Size of resource */
};

/*
 * Common Resource definitions
 */
#define MAX_RES_NAME_LENGTH MAX_NAME_LENGTH - 1 /* maximum resource name length */

/*
 * Config item flags.
 */
#define CFG_ITEM_REQUIRED          0x1  /* Item required */
#define CFG_ITEM_DEFAULT           0x2  /* Default supplied */
#define CFG_ITEM_NO_EQUALS         0x4  /* Don't scan = after name */
#define CFG_ITEM_DEPRECATED        0x8  /* Deprecated config option */
#define CFG_ITEM_ALIAS             0x10 /* Item is an alias for another */

/*
 * CFG_ITEM_DEFAULT_PLATFORM_SPECIFIC: the value may differ between different
 * platforms (or configure settings). This information is used for the documentation.
 */
#define CFG_ITEM_PLATFORM_SPECIFIC 0x20

enum {
   /*
    * Standard resource types. handlers in res.c
    */
   CFG_TYPE_STR = 1,                    /* String */
   CFG_TYPE_DIR = 2,                    /* Directory */
   CFG_TYPE_MD5PASSWORD = 3,            /* MD5 hashed Password */
   CFG_TYPE_CLEARPASSWORD = 4,          /* Clear text Password */
   CFG_TYPE_AUTOPASSWORD = 5,           /* Password stored in clear when needed otherwise hashed */
   CFG_TYPE_NAME = 6,                   /* Name */
   CFG_TYPE_STRNAME = 7,                /* String Name */
   CFG_TYPE_RES = 8,                    /* Resource */
   CFG_TYPE_ALIST_RES = 9,              /* List of resources */
   CFG_TYPE_ALIST_STR = 10,             /* List of strings */
   CFG_TYPE_ALIST_DIR = 11,             /* List of dirs */
   CFG_TYPE_INT32 = 12,                 /* 32 bits Integer */
   CFG_TYPE_PINT32 = 13,                /* Positive 32 bits Integer (unsigned) */
   CFG_TYPE_MSGS = 14,                  /* Message resource */
   CFG_TYPE_INT64 = 15,                 /* 64 bits Integer */
   CFG_TYPE_BIT = 16,                   /* Bitfield */
   CFG_TYPE_BOOL = 17,                  /* Boolean */
   CFG_TYPE_TIME = 18,                  /* Time value */
   CFG_TYPE_SIZE64 = 19,                /* 64 bits file size */
   CFG_TYPE_SIZE32 = 20,                /* 32 bits file size */
   CFG_TYPE_SPEED = 21,                 /* Speed limit */
   CFG_TYPE_DEFS = 22,                  /* Definition */
   CFG_TYPE_LABEL = 23,                 /* Label */
   CFG_TYPE_ADDRESSES = 24,             /* List of ip addresses */
   CFG_TYPE_ADDRESSES_ADDRESS = 25,     /* Ip address */
   CFG_TYPE_ADDRESSES_PORT = 26,        /* Ip port */
   CFG_TYPE_PLUGIN_NAMES = 27,          /* Plugin Name(s) */

   /*
    * Director resource types. handlers in dird_conf.
    */
   CFG_TYPE_ACL = 50,                   /* User Access Control List */
   CFG_TYPE_AUDIT = 51,                 /* Auditing Command List */
   CFG_TYPE_AUTHPROTOCOLTYPE = 52,      /* Authentication Protocol */
   CFG_TYPE_AUTHTYPE = 53,              /* Authentication Type */
   CFG_TYPE_DEVICE = 54,                /* Device resource */
   CFG_TYPE_JOBTYPE = 55,               /* Type of Job */
   CFG_TYPE_PROTOCOLTYPE = 56,          /* Protocol */
   CFG_TYPE_LEVEL = 57,                 /* Backup Level */
   CFG_TYPE_REPLACE = 58,               /* Replace option */
   CFG_TYPE_SHRTRUNSCRIPT = 59,         /* Short Runscript definition */
   CFG_TYPE_RUNSCRIPT = 60,             /* Runscript */
   CFG_TYPE_RUNSCRIPT_CMD = 61,         /* Runscript Command */
   CFG_TYPE_RUNSCRIPT_TARGET = 62,      /* Runscript Target (Host) */
   CFG_TYPE_RUNSCRIPT_BOOL = 63,        /* Runscript Boolean */
   CFG_TYPE_RUNSCRIPT_WHEN = 64,        /* Runscript When expression */
   CFG_TYPE_MIGTYPE = 65,               /* Migration Type */
   CFG_TYPE_INCEXC = 66,                /* Include/Exclude item */
   CFG_TYPE_RUN = 67,                   /* Schedule Run Command */
   CFG_TYPE_ACTIONONPURGE = 68,         /* Action to perform on Purge */

   /*
    * Director fileset options. handlers in dird_conf.
    */
   CFG_TYPE_FNAME = 80,                 /* Filename */
   CFG_TYPE_PLUGINNAME = 81,            /* Pluginname */
   CFG_TYPE_EXCLUDEDIR = 82,            /* Exclude directory */
   CFG_TYPE_OPTIONS = 83,               /* Options block */
   CFG_TYPE_OPTION = 84,                /* Option of Options block */
   CFG_TYPE_REGEX = 85,                 /* Regular Expression */
   CFG_TYPE_BASE = 86,                  /* Basejob Expression */
   CFG_TYPE_WILD = 87,                  /* Wildcard Expression */
   CFG_TYPE_PLUGIN = 88,                /* Plugin definition */
   CFG_TYPE_FSTYPE = 89,                /* FileSytem match criterium (UNIX)*/
   CFG_TYPE_DRIVETYPE = 90,             /* DriveType match criterium (Windows) */
   CFG_TYPE_META = 91,                  /* Meta tag */

   /*
    * Storage daemon resource types
    */
   CFG_TYPE_DEVTYPE = 201,               /* Device Type */
   CFG_TYPE_MAXBLOCKSIZE = 202,          /* Maximum Blocksize */
   CFG_TYPE_IODIRECTION = 203,           /* IO Direction */
   CFG_TYPE_CMPRSALGO = 204,             /* Compression Algorithm */

   /*
    * File daemon resource types
    */
   CFG_TYPE_CIPHER = 301                /* Encryption Cipher */
};

struct DATATYPE_NAME {
   const int number;
   const char *name;
   const char *description;
};


/*
 * Base Class for all Resource Classes
 */
class BRSRES {
public:
   RES hdr;

   /* Methods */
   char *name() const;
   bool print_config(POOL_MEM &buf, bool hide_sensitive_data = false);
};

inline char *BRSRES::name() const { return this->hdr.name; }

/*
 * Message Resource
 */
class MSGSRES : public BRSRES {
   /*
    * Members
    */
public:
   char *mail_cmd;                    /* Mail command */
   char *operator_cmd;                /* Operator command */
   DEST *dest_chain;                  /* chain of destinations */
   char send_msg[nbytes_for_bits(M_MAX+1)]; /* Bit array of types */

private:
   bool m_in_use;                     /* Set when using to send a message */
   bool m_closing;                    /* Set when closing message resource */

public:
   /*
    * Methods
    */
   void clear_in_use() { lock(); m_in_use=false; unlock(); }
   void set_in_use() { wait_not_in_use(); m_in_use=true; unlock(); }
   void set_closing() { m_closing=true; }
   bool get_closing() { return m_closing; }
   void clear_closing() { lock(); m_closing=false; unlock(); }
   bool is_closing() { lock(); bool rtn=m_closing; unlock(); return rtn; }

   void wait_not_in_use();            /* in message.c */
   void lock();                       /* in message.c */
   void unlock();                     /* in message.c */
   bool print_config(POOL_MEM &buff, bool hide_sensitive_data = false);
};

typedef void (INIT_RES_HANDLER)(RES_ITEM *item, int pass);
typedef void (STORE_RES_HANDLER)(LEX *lc, RES_ITEM *item, int index, int pass);
typedef void (PRINT_RES_HANDLER)(RES_ITEM *items, int i, POOL_MEM &cfg_str, bool hide_sensitive_data);

/*
 * New C++ configuration routines
 */
class CONFIG {
public:
   /*
    * Members
    */
   const char *m_cf;                    /* Config file */
   LEX_ERROR_HANDLER *m_scan_error;     /* Error handler if non-null */
   LEX_WARNING_HANDLER *m_scan_warning; /* Warning handler if non-null */
   INIT_RES_HANDLER *m_init_res;        /* Init resource handler for non default types if non-null */
   STORE_RES_HANDLER *m_store_res;      /* Store resource handler for non default types if non-null */
   PRINT_RES_HANDLER *m_print_res;      /* Print resource handler for non default types if non-null */

   int32_t m_err_type;                  /* The way to terminate on failure */
   void *m_res_all;                     /* Pointer to res_all buffer */
   int32_t m_res_all_size;              /* Length of buffer */
   bool m_omit_defaults;                /* Omit config variables with default values when dumping the config */

   int32_t m_r_first;                   /* First daemon resource type */
   int32_t m_r_last;                    /* Last daemon resource type */
   RES_TABLE *m_resources;              /* Pointer to table of permitted resources */
   RES **m_res_head;                    /* Pointer to defined resources */
   brwlock_t m_res_lock;                /* Resource lock */

   /*
    * Methods
    */
   void init(
      const char *cf,
      LEX_ERROR_HANDLER *scan_error,
      LEX_WARNING_HANDLER *scan_warning,
      INIT_RES_HANDLER *init_res,
      STORE_RES_HANDLER *store_res,
      PRINT_RES_HANDLER *print_res,
      int32_t err_type,
      void *vres_all,
      int32_t res_all_size,
      int32_t r_first,
      int32_t r_last,
      RES_TABLE *resources,
      RES **res_head);

   bool parse_config();
   void free_resources();
   RES **save_resources();
   RES **new_res_head();
   void init_resource(int type, RES_ITEM *items, int pass);
   void dump_resources(void sendit(void *sock, const char *fmt, ...),
                       void *sock, bool hide_sensitive_data = false);
};

CONFIG *new_config_parser();

void prtmsg(void *sock, const char *fmt, ...);

/*
 * Data type routines
 */
DATATYPE_NAME *get_datatype(int number);
const char *datatype_to_str(int type);
const char *datatype_to_description(int type);

/*
 * Resource routines
 */
RES *GetResWithName(int rcode, const char *name);
RES *GetNextRes(int rcode, RES *res);
void b_LockRes(const char *file, int line);
void b_UnlockRes(const char *file, int line);
void dump_resource(int type, RES *res, void sendmsg(void *sock, const char *fmt, ...),
                   void *sock, bool hide_sensitive_data = false);
void free_resource(RES *res, int type);
void init_resource(int type, RES_ITEM *item);
void save_resource(int type, RES_ITEM *item, int pass);
bool store_resource(int type, LEX *lc, RES_ITEM *item, int index, int pass);
const char *res_to_str(int rcode);

#ifdef HAVE_JANSSON
/*
 * JSON output helper functions
 */
json_t *json_item(s_kw *item);
json_t *json_item(RES_ITEM *item);
json_t *json_items(RES_ITEM items[]);
#endif

/*
 * Loop through each resource of type, returning in var
 */
#ifdef HAVE_TYPEOF
#define foreach_res(var, type) \
        for((var)=NULL; ((var)=(typeof(var))GetNextRes((type), (RES *)var));)
#else
#define foreach_res(var, type) \
    for(var=NULL; (*((void **)&(var))=(void *)GetNextRes((type), (RES *)var));)
#endif
