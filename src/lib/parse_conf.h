/*
   Bacula® - The Network Backup Solution

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.

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
 *     Kern Sibbald, January MM
 *
 */

struct RES_ITEM;                    /* Declare forward referenced structure */
struct RES_ITEM2;                  /* Declare forward referenced structure */
class RES;                         /* Declare forware referenced structure */
typedef void (MSG_RES_HANDLER)(LEX *lc, RES_ITEM *item, int index, int pass);
typedef void (INC_RES_HANDLER)(LEX *lc, RES_ITEM2 *item, int index, int pass, bool exclude);



/* This is the structure that defines
 * the record types (items) permitted within each
 * resource. It is used to define the configuration
 * tables.
 */
struct RES_ITEM {
   const char *name;                  /* Resource name i.e. Director, ... */
   MSG_RES_HANDLER *handler;          /* Routine storing the resource item */
   union {
      char **value;                   /* Where to store the item */
      char **charvalue;
      uint32_t ui32value;
      int32_t i32value;
      uint64_t ui64value;
      int64_t i64value;
      bool boolvalue;
      utime_t utimevalue;
      RES *resvalue;
      RES **presvalue;
   };
   int32_t  code;                     /* item code/additional info */
   uint32_t  flags;                   /* flags: default, required, ... */
   int32_t  default_value;            /* default value */
};

struct RES_ITEM2 {
   const char *name;                  /* Resource name i.e. Director, ... */
   INC_RES_HANDLER *handler;          /* Routine storing the resource item */
   union {
      char **value;                   /* Where to store the item */
      char **charvalue;
      uint32_t ui32value;
      int32_t i32value;
      uint64_t ui64value;
      int64_t i64value;
      bool boolvalue;
      utime_t utimevalue;
      RES *resvalue;
      RES **presvalue;
   };
   int32_t  code;                     /* item code/additional info */
   uint32_t  flags;                   /* flags: default, required, ... */
   int32_t  default_value;            /* default value */
};


/* For storing name_addr items in res_items table */
#define ITEM(x) {(char **)&res_all.x}

#define MAX_RES_ITEMS 80              /* maximum resource items per RES */

/* This is the universal header that is
 * at the beginning of every resource
 * record.
 */
class RES {
public:
   RES *next;                         /* pointer to next resource of this type */
   char *name;                        /* resource name */
   char *desc;                        /* resource description */
   uint32_t rcode;                    /* resource id or type */
   int32_t  refcnt;                   /* reference count for releasing */
   char  item_present[MAX_RES_ITEMS]; /* set if item is present in conf file */
};


/*
 * Master Resource configuration structure definition
 * This is the structure that defines the
 * resources that are available to this daemon.
 */
struct RES_TABLE {
   const char *name;                  /* resource name */
   RES_ITEM *items;                   /* list of resource keywords */
   uint32_t rcode;                    /* code if needed */
};



/* Common Resource definitions */

#define MAX_RES_NAME_LENGTH MAX_NAME_LENGTH-1       /* maximum resource name length */

#define ITEM_REQUIRED    0x1          /* item required */
#define ITEM_DEFAULT     0x2          /* default supplied */
#define ITEM_NO_EQUALS   0x4          /* Don't scan = after name */

/* Message Resource */
class MSGS {
public:
   RES   hdr;
   char *mail_cmd;                    /* mail command */
   char *operator_cmd;                /* Operator command */
   DEST *dest_chain;                  /* chain of destinations */
   char send_msg[nbytes_for_bits(M_MAX+1)];  /* bit array of types */

private:
   bool m_in_use;                     /* set when using to send a message */
   bool m_closing;                    /* set when closing message resource */

public:
   /* Methods */
   char *name() const;
   void clear_in_use() { lock(); m_in_use=false; unlock(); }
   void set_in_use() { wait_not_in_use(); m_in_use=true; unlock(); }
   void set_closing() { m_closing=true; }
   bool get_closing() { return m_closing; }
   void clear_closing() { lock(); m_closing=false; unlock(); }
   bool is_closing() { lock(); bool rtn=m_closing; unlock(); return rtn; }

   void wait_not_in_use();            /* in message.c */
   void lock();                       /* in message.c */
   void unlock();                     /* in message.c */
};

inline char *MSGS::name() const { return hdr.name; }

/* 
 * Old C style configuration routines -- deprecated do not use.
 */
//int   parse_config(const char *cf, LEX_ERROR_HANDLER *scan_error = NULL, int err_type=M_ERROR_TERM);
void    free_config_resources(void);
RES   **save_config_resources(void);
RES   **new_res_head();

/*
 * New C++ configuration routines
 */

class CONFIG {
public:
   const char *m_cf;                   /* config file */
   LEX_ERROR_HANDLER *m_scan_error;    /* error handler if non-null */
   int32_t m_err_type;                 /* the way to terminate on failure */
   void *m_res_all;                    /* pointer to res_all buffer */
   int32_t m_res_all_size;             /* length of buffer */

   /* The below are not yet implemented */
   int32_t m_r_first;                  /* first daemon resource type */
   int32_t m_r_last;                   /* last daemon resource type */
   RES_TABLE *m_resources;             /* pointer to table of permitted resources */      
   RES **m_res_head;                   /* pointer to defined resources */
   brwlock_t m_res_lock;               /* resource lock */

   /* functions */
   void init(
      const char *cf,
      LEX_ERROR_HANDLER *scan_error,
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
};
 
CONFIG *new_config_parser();


/* Resource routines */
RES *GetResWithName(int rcode, const char *name);
RES *GetNextRes(int rcode, RES *res);
void b_LockRes(const char *file, int line);
void b_UnlockRes(const char *file, int line);
void dump_resource(int type, RES *res, void sendmsg(void *sock, const char *fmt, ...), void *sock);
void free_resource(RES *res, int type);
void init_resource(int type, RES_ITEM *item);
void save_resource(int type, RES_ITEM *item, int pass);
const char *res_to_str(int rcode);

/* Loop through each resource of type, returning in var */
#ifdef HAVE_TYPEOF
#define foreach_res(var, type) \
        for((var)=NULL; ((var)=(typeof(var))GetNextRes((type), (RES *)var));)
#else 
#define foreach_res(var, type) \
    for(var=NULL; (*((void **)&(var))=(void *)GetNextRes((type), (RES *)var));)
#endif


/*
 * Standard global parsers defined in parse_config.c
 */
void store_str(LEX *lc, RES_ITEM *item, int index, int pass);
void store_dir(LEX *lc, RES_ITEM *item, int index, int pass);
void store_password(LEX *lc, RES_ITEM *item, int index, int pass);
void store_name(LEX *lc, RES_ITEM *item, int index, int pass);
void store_strname(LEX *lc, RES_ITEM *item, int index, int pass);
void store_res(LEX *lc, RES_ITEM *item, int index, int pass);
void store_alist_res(LEX *lc, RES_ITEM *item, int index, int pass);
void store_alist_str(LEX *lc, RES_ITEM *item, int index, int pass);
void store_int32(LEX *lc, RES_ITEM *item, int index, int pass);
void store_pint32(LEX *lc, RES_ITEM *item, int index, int pass);
void store_msgs(LEX *lc, RES_ITEM *item, int index, int pass);
void store_int64(LEX *lc, RES_ITEM *item, int index, int pass);
void store_bit(LEX *lc, RES_ITEM *item, int index, int pass);
void store_bool(LEX *lc, RES_ITEM *item, int index, int pass);
void store_time(LEX *lc, RES_ITEM *item, int index, int pass);
void store_size64(LEX *lc, RES_ITEM *item, int index, int pass);
void store_size32(LEX *lc, RES_ITEM *item, int index, int pass);
void store_speed(LEX *lc, RES_ITEM *item, int index, int pass);
void store_defs(LEX *lc, RES_ITEM *item, int index, int pass);
void store_label(LEX *lc, RES_ITEM *item, int index, int pass);

/* ***FIXME*** eliminate these globals */
extern int32_t r_first;
extern int32_t r_last;
extern RES_TABLE resources[];
extern RES **res_head;
extern int32_t res_all_size;
