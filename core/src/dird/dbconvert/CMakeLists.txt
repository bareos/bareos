if(dynamic-cats-backends)
  add_executable(bareos-dbconvert dbconvert.cc database_tables.cc
                 database_table_description.cc)
  target_link_libraries(bareos-dbconvert
      bareos
      dird_objects
      bareosfind
      bareoscats
      bareossql
      $<$<BOOL:HAVE_PAM>:${PAM_LIBRARIES}>
      ${LMDB_LIBS}
  )
endif()
