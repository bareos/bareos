/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2000-2010 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2016 Bareos GmbH & Co. KG

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
 * store all TLS specific settings
 * and backend specific context.
 */
struct tls_t {
   bool authenticate;             /* Authenticate with TLS */
   bool enable;                   /* Enable TLS */
   bool require;                  /* Require TLS */
   bool verify_peer;              /* TLS Verify Peer Certificate */
   char *ca_certfile;             /* TLS CA Certificate File */
   char *ca_certdir;              /* TLS CA Certificate Directory */
   char *crlfile;                 /* TLS CA Certificate Revocation List File */
   char *certfile;                /* TLS Client Certificate File */
   char *keyfile;                 /* TLS Client Key File */
   char *cipherlist;              /* TLS Cipher List */
   char *dhfile;                  /* TLS Diffie-Hellman File */
   alist *allowed_cns;            /* TLS Allowed Certificate Common Names (Clients) */
   TLS_CONTEXT *ctx;              /* Shared TLS Context */
};

/*
 * free the tls_t structure.
 */
void free_tls_t(tls_t &tls);

/*
 * All configuration resources using TLS have the same directives,
 * therefore define it once and use it in every resource.
 */
#define TLS_CONFIG(res) \
   { "TlsAuthenticate", CFG_TYPE_BOOL, ITEM(res.tls.authenticate), 0, CFG_ITEM_DEFAULT, "false", NULL, \
         "Use TLS only to authenticate, not for encryption." }, \
   { "TlsEnable", CFG_TYPE_BOOL, ITEM(res.tls.enable), 0, CFG_ITEM_DEFAULT, "false", NULL, \
         "Enable TLS support." }, \
   { "TlsRequire", CFG_TYPE_BOOL, ITEM(res.tls.require), 0, CFG_ITEM_DEFAULT, "false", NULL, \
         "Without setting this to yes, Bareos can fall back to use unencryption connections. " \
         "Enabling this implicietly sets \"TLS Enable = yes\"." }, \
   { "TlsVerifyPeer", CFG_TYPE_BOOL, ITEM(res.tls.verify_peer), 0, CFG_ITEM_DEFAULT, "true", NULL, \
         "If disabled, all certificates signed by a known CA will be accepted. " \
         "If enabled, the CN of a certificate must the Address or in the \"TLS Allowed CN\" list." }, \
   { "TlsCaCertificateFile", CFG_TYPE_DIR, ITEM(res.tls.ca_certfile), 0, 0, NULL, NULL, \
         "Path of a PEM encoded TLS CA certificate(s) file." }, \
   { "TlsCaCertificateDir", CFG_TYPE_DIR, ITEM(res.tls.ca_certdir), 0, 0, NULL, NULL, \
         "Path of a TLS CA certificate directory." }, \
   { "TlsCertificateRevocationList", CFG_TYPE_DIR, ITEM(res.tls.crlfile), 0, 0, NULL, NULL, \
         "Path of a Certificate Revocation List file." }, \
   { "TlsCertificate", CFG_TYPE_DIR, ITEM(res.tls.certfile), 0, 0, NULL, NULL, \
         "Path of a PEM encoded TLS certificate." }, \
   { "TlsKey", CFG_TYPE_DIR, ITEM(res.tls.keyfile), 0, 0, NULL, NULL, \
         "Path of a PEM encoded private key. It must correspond to the specified \"TLS Certificate\"." }, \
   { "TlsCipherList", CFG_TYPE_STR, ITEM(res.tls.cipherlist), 0, CFG_ITEM_PLATFORM_SPECIFIC, NULL, NULL, \
         "List of valid TLS Ciphers." }, \
   { "TlsAllowedCn", CFG_TYPE_ALIST_STR, ITEM(res.tls.allowed_cns), 0, 0, NULL, NULL, \
         "\"Common Name\"s (CNs) of the allowed peer certificates."  }, \
   { "TlsDhFile", CFG_TYPE_DIR, ITEM(res.tls.dhfile), 0, 0, NULL, NULL, \
         "Path to PEM encoded Diffie-Hellman parameter file. " \
         "If this directive is specified, DH key exchange will be used for the ephemeral keying, " \
         "allowing for forward secrecy of communications." },

/*
 * This is the structure that defines the record types (items) permitted within each
 * resource. It is used to define the configuration tables.
 */
struct RES_ITEM {
   const char *name;                    /* Resource name i.e. Director, ... */
   const int type;
   union {
      char **value;                     /* Where to store the item */
      uint16_t *ui16value;
      uint32_t *ui32value;
      int16_t *i16value;
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

#define MAX_RES_ITEMS 90                /* maximum resource items per RES */

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
   CFG_TYPE_INT16 = 12,                 /* 16 bits Integer */
   CFG_TYPE_PINT16 = 13,                /* Positive 16 bits Integer (unsigned) */
   CFG_TYPE_INT32 = 14,                 /* 32 bits Integer */
   CFG_TYPE_PINT32 = 15,                /* Positive 32 bits Integer (unsigned) */
   CFG_TYPE_MSGS = 16,                  /* Message resource */
   CFG_TYPE_INT64 = 17,                 /* 64 bits Integer */
   CFG_TYPE_BIT = 18,                   /* Bitfield */
   CFG_TYPE_BOOL = 19,                  /* Boolean */
   CFG_TYPE_TIME = 20,                  /* Time value */
   CFG_TYPE_SIZE64 = 21,                /* 64 bits file size */
   CFG_TYPE_SIZE32 = 22,                /* 32 bits file size */
   CFG_TYPE_SPEED = 23,                 /* Speed limit */
   CFG_TYPE_DEFS = 24,                  /* Definition */
   CFG_TYPE_LABEL = 25,                 /* Label */
   CFG_TYPE_ADDRESSES = 26,             /* List of ip addresses */
   CFG_TYPE_ADDRESSES_ADDRESS = 27,     /* Ip address */
   CFG_TYPE_ADDRESSES_PORT = 28,        /* Ip port */
   CFG_TYPE_PLUGIN_NAMES = 29,          /* Plugin Name(s) */

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
   CFG_TYPE_POOLTYPE = 69,              /* Pool Type */

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
   CFG_TYPE_FSTYPE = 89,                /* FileSystem match criterium (UNIX)*/
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
   bool print_config(POOL_MEM &buf, bool hide_sensitive_data = false, bool verbose = false);
   /*
    * validate can be defined by inherited classes,
    * when special rules for this resource type must be checked.
    */
   bool validate();
};

inline char *BRSRES::name() const { return this->hdr.name; }
inline bool BRSRES::validate() { return true; }

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
   char *timestamp_format;            /* Timestamp format */
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
   bool print_config(POOL_MEM &buff, bool hide_sensitive_data = false, bool verbose = false);
};

typedef void (INIT_RES_HANDLER)(RES_ITEM *item, int pass);
typedef void (STORE_RES_HANDLER)(LEX *lc, RES_ITEM *item, int index, int pass);
typedef void (PRINT_RES_HANDLER)(RES_ITEM *items, int i, POOL_MEM &cfg_str, bool hide_sensitive_data, bool inherited);

/*
 * New C++ configuration routines
 */
class CONFIG {
public:
   /*
    * Members
    */
   const char *m_cf;                    /* Config file parameter */
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
   void set_default_config_filename(const char *filename);
   void set_config_include_dir(const char *rel_path);
   bool is_using_config_include_dir() { return m_use_config_include_dir; };
   bool parse_config();
   bool parse_config_file(const char *cf, void *caller_ctx, LEX_ERROR_HANDLER *scan_error = NULL,
                          LEX_WARNING_HANDLER *scan_warning = NULL, int32_t err_type = M_ERROR_TERM);
   const char *get_base_config_path() { return m_used_config_path; };
   void free_resources();
   RES **save_resources();
   RES **new_res_head();
   void init_resource(int type, RES_ITEM *items, int pass);
   bool remove_resource(int type, const char *name);
   void dump_resources(void sendit(void *sock, const char *fmt, ...),
                       void *sock, bool hide_sensitive_data = false);
   const char *get_resource_type_name(int code);
   int get_resource_code(const char *resource_type);
   RES_TABLE *get_resource_table(int resource_type);
   RES_TABLE *get_resource_table(const char *resource_type_name);
   int get_resource_item_index(RES_ITEM *res_table, const char *item);
   RES_ITEM *get_resource_item(RES_ITEM *res_table, const char *item);
   bool get_path_of_resource(POOL_MEM &path, const char *component, const char *resourcetype,
                             const char *name, bool set_wildcards = false);
   bool get_path_of_new_resource(POOL_MEM &path, POOL_MEM &extramsg, const char *component,
                                 const char *resourcetype, const char *name,
                                 bool error_if_exits = false, bool create_directories = false);

protected:
   const char *m_config_default_filename;         /* default config filename, that is used, if no filename is given */
   const char *m_config_dir;                      /* base directory of configuration files */
   const char *m_config_include_dir;              /* rel. path to the config include directory
                                                     (bareos-dir.d, bareos-sd.d, bareos-fd.d, ...) */
   bool m_use_config_include_dir;                 /* Use the config include directory */
   const char *m_config_include_naming_format;    /* Format string for file paths of resources */
   const char *m_used_config_path;                /* Config file that is used. */

   const char *get_default_configdir();
   bool get_config_file(POOL_MEM &full_path, const char *config_dir, const char *config_filename);
   bool get_config_include_path(POOL_MEM &full_path, const char *config_dir);
   bool find_config_path(POOL_MEM &full_path);
   int get_resource_table_index(int resource_type);
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
RES *GetResWithName(int rcode, const char *name, bool lock = true);
RES *GetNextRes(int rcode, RES *res);
void b_LockRes(const char *file, int line);
void b_UnlockRes(const char *file, int line);
void dump_resource(int type, RES *res, void sendmsg(void *sock, const char *fmt, ...),
                   void *sock, bool hide_sensitive_data = false, bool verbose = false);
void indent_config_item(POOL_MEM &cfg_str, int level, const char *config_item, bool inherited = false);
void free_resource(RES *res, int type);
void init_resource(int type, RES_ITEM *item);
bool save_resource(int type, RES_ITEM *item, int pass);
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
