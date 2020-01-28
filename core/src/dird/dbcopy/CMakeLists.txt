if(dynamic-cats-backends)
  add_executable(bareos-dbcopy dbcopy.cc database_table_descriptions.cc
                 database_column_descriptions.cc column_description.cc
                 database_import.cc database_import_mysql.cc database_export.cc
                 database_export_postgresql.cc)
  target_link_libraries(bareos-dbcopy
      bareos
      dird_objects
      bareosfind
      bareoscats
      bareossql
      $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>
      ${LMDB_LIBS}
  )

install(TARGETS
  bareos-dbcopy
  DESTINATION "${sbindir}"
)



endif()
