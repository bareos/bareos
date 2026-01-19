get_filename_component(BASENAME ${CMAKE_CURRENT_BINARY_DIR} NAME)

find_program(INCUS incus)

# The following dependencies are needed to create a tiny Alpine image for Incus
find_program(GUESTFISH guestfish)
find_program(QEMUIMG qemu-img)
find_program(VIRTRESIZE virt-resize)

if(INCUS
   AND GUESTFISH
   AND QEMUIMG
   AND VIRTRESIZE
   AND TARGET python3-fd
   AND ENABLE_INCUS_PLUGIN
)
  create_systemtest(${SYSTEMTEST_PREFIX} ${BASENAME})
else()
  create_systemtest(
    ${SYSTEMTEST_PREFIX} ${BASENAME} DISABLED COMMENT
    "disabled as not all requirements were met"
  )
endif()
