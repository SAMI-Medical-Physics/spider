include(GNUInstallDirs)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../etc/Parameters_Rigid.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/spider"
)

if(MSVC)
  # SPIDER_DATADIR_FROM_BINDIR is used in spider.ps1.in to locate
  # Parameters_Rigid.txt.  It is a relative path to support relocation
  # of the install tree.  It is allowed to be different for the build
  # and the installation to avoid mirroring the installation paths in
  # the build, since that could require making directories and files
  # outside of the build tree.

  # For the build tree.
  cmake_path(
    RELATIVE_PATH CMAKE_SOURCE_DIR
    BASE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    OUTPUT_VARIABLE SPIDER_SOURCEDIR_FROM_BINDIR
  )
  set(SPIDER_DATADIR_FROM_BINDIR
    "${SPIDER_SOURCEDIR_FROM_BINDIR}/etc"
  )
  configure_file(spider.ps1.in spider.ps1 @ONLY)
  file(COPY spider.cmd
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}
  )

  # For the installation.
  cmake_path(
    RELATIVE_PATH CMAKE_INSTALL_FULL_DATADIR
    BASE_DIRECTORY "${CMAKE_INSTALL_FULL_BINDIR}"
    OUTPUT_VARIABLE SPIDER_DATADIR_FROM_BINDIR
  )
  set(SPIDER_DATADIR_FROM_BINDIR
    "${SPIDER_DATADIR_FROM_BINDIR}/spider"
  )
  configure_file(spider.ps1.in spider.ps1.install @ONLY)
  install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/spider.ps1.install"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    RENAME spider.ps1
  )
  install(FILES
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
      PRE_EXCLUDE_REGEXES
      "api-ms-"
      "ext-ms-"
      # Windows system DLL dependencies that are not resolved on the
      # GitHub-hosted runner image Windows Server 2025.  On Windows,
      # DLL dependency names are converted to lowercase for matching
      # filters.
      "^azureattestmanager\\.dll$"
      "^azureattestnormal\\.dll$"
      "^hvsifiletrust\\.dll$"
      "^pdmutilities\\.dll$"
      "^wtdsensor\\.dll$"
      "^wpaxholder\\.dll$"
      "^wtdccm\\.dll$"
      POST_EXCLUDE_REGEXES "system32"
    )
  endif()
else()
  # SPIDER_DATADIR and SPIDER_BINDIR are used in spider.sh.in to
  # locate Parameters_Rigid.txt and spider_tia, respectively.

  # For the build tree.
  set(SPIDER_DATADIR
    "${CMAKE_SOURCE_DIR}/etc"
  )
  set(SPIDER_BINDIR
    "${CMAKE_CURRENT_BINARY_DIR}"
  )
  configure_file(spider.sh.in spider @ONLY)
  file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/spider"
    PERMISSIONS
    OWNER_READ
    OWNER_EXECUTE
  )

  # For the installation.
  set(SPIDER_DATADIR
    "${CMAKE_INSTALL_FULL_DATADIR}/spider"
  )
  set(SPIDER_BINDIR
    "${CMAKE_INSTALL_FULL_BINDIR}"
  )
  configure_file(spider.sh.in spider.install @ONLY)
  install(PROGRAMS
    "${CMAKE_CURRENT_BINARY_DIR}/spider.install"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    RENAME spider
  )
endif()
