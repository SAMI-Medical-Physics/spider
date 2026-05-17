include(GNUInstallDirs)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../etc/Parameters_Rigid.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/spider"
)

if(MSVC)
  # SPIDER_DATADIR_FROM_BINDIR is used in spider.ps1.in to support
  # relocation of the install tree.
  cmake_path(
    RELATIVE_PATH CMAKE_INSTALL_FULL_DATADIR
    BASE_DIRECTORY "${CMAKE_INSTALL_FULL_BINDIR}"
    OUTPUT_VARIABLE SPIDER_DATADIR_FROM_BINDIR
  )
  configure_file(spider.ps1.in spider.ps1 @ONLY)
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/spider.ps1"
    spider.cmd
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )

  # For deploying to other Windows machines.
  option(SPIDER_BUNDLE_EXTERNAL_EXECUTABLES
    "Bundle external executable dependencies in the install tree."
    OFF
  )
  if(SPIDER_BUNDLE_EXTERNAL_EXECUTABLES)
    # Use the programs installed on the host machine.
    find_program(DCM2NIIX_EXE dcm2niix REQUIRED)
    add_executable(dcm2niix::dcm2niix IMPORTED)
    set_target_properties(dcm2niix::dcm2niix
      PROPERTIES IMPORTED_LOCATION "${DCM2NIIX_EXE}"
    )
    install(IMPORTED_RUNTIME_ARTIFACTS dcm2niix::dcm2niix
      RUNTIME_DEPENDENCY_SET spider_external_exe_dlls
    )

    find_program(ELASTIX_EXE elastix REQUIRED)
    add_executable(elastix::elastix IMPORTED)
    set_target_properties(elastix::elastix
      PROPERTIES IMPORTED_LOCATION "${ELASTIX_EXE}"
    )
    install(IMPORTED_RUNTIME_ARTIFACTS elastix::elastix
      RUNTIME_DEPENDENCY_SET spider_external_exe_dlls
    )

    install(RUNTIME_DEPENDENCY_SET spider_external_exe_dlls
      PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
      POST_EXCLUDE_REGEXES ".*WINDOWS[\\/]system32.*"
    )
  endif()
else()
  configure_file(spider.sh.in spider @ONLY)
  install(PROGRAMS
    "${CMAKE_CURRENT_BINARY_DIR}/spider"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
endif()
