check_c_source_compiles(
  "#include <sys/types.h>
   #include <sys/acl.h>
   int main(void) { return ACL_TYPE_DEFAULT_DIR; }" HAVE_ACL_TYPE_DEFAULT_DIR
)

check_c_source_compiles(
  "#include <sys/types.h>
   #include <sys/acl.h>
   int main(void) { return ACL_TYPE_EXTENDED; }" HAVE_ACL_TYPE_EXTENDED
)

check_c_source_compiles(
  "#include <sys/types.h>
   #include <sys/acl.h>
   int main(void) { return ACL_TYPE_NFS4; }" HAVE_ACL_TYPE_NFS4
)
