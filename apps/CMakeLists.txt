add_executable(
  spider_clamp
  spider_clamp.cc
)
target_link_libraries(spider_clamp
  PRIVATE
  spider_logging
  spider_output_filenames
  ${ITK_LIBRARIES}
)
target_compile_definitions(spider_clamp
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")
# For running Spider pipeline scripts from the build tree.
set_target_properties(spider_clamp PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_executable(
  spider_id
  spider_id.cc
)
target_link_libraries(spider_id
  PRIVATE
  spider_logging
  spider_spect
)
target_compile_definitions(spider_id
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")
# For running Spider pipeline scripts from the build tree.
set_target_properties(spider_id PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Spider Workbench.
if(SPIDER_BUILD_WORKBENCH)
  # Use separate targets for build and install trees due to
  # posix_spawn; see SPIDER_PATHNAME compile definitions below.
  add_executable(
    spider_workbench-build
    spider_workbench.cc
  )
  add_executable(
    spider_workbench
    spider_workbench.cc
  )

  # Require FLTK (<https://www.fltk.org>) built with CMake.
  find_package(FLTK CONFIG REQUIRED)
  # Prefer shared libraries.
  if(TARGET fltk::fltk-shared)
    target_link_libraries(spider_workbench-build
      PRIVATE
      fltk::fltk-shared
    )
    target_link_libraries(spider_workbench
      PRIVATE
      fltk::fltk-shared
    )
  elseif(TARGET fltk::fltk)
    target_link_libraries(spider_workbench-build
      PRIVATE
      fltk::fltk
    )
    target_link_libraries(spider_workbench
      PRIVATE
      fltk::fltk
    )
  elseif(TARGET fltk)           # FLTK 1.3
    find_package(X11 REQUIRED)
    target_link_libraries(spider_workbench-build
      PRIVATE
      fltk
      X11::X11
    )
    target_link_libraries(spider_workbench
      PRIVATE
      fltk
      X11::X11
    )
  else()
    message(FATAL_ERROR
      "Found FLTK but not any of the targets: "
      "fltk::fltk-shared, fltk::fltk, or fltk"
    )
  endif()

  target_compile_definitions(spider_workbench-build PRIVATE
    SPIDER_PATHNAME="${CMAKE_BINARY_DIR}/bin/spider"
    SPIDER_VERSION="${PROJECT_VERSION}"
  )

  include(GNUInstallDirs)     # for CMAKE_INSTALL_FULL_BINDIR
  target_compile_definitions(spider_workbench PRIVATE
    SPIDER_PATHNAME="${CMAKE_INSTALL_FULL_BINDIR}/spider"
    SPIDER_VERSION="${PROJECT_VERSION}"
  )
endif()

add_executable(
  spider_tia
  spider_tia.cc
)
target_link_libraries(spider_tia
  PRIVATE
  spider_logging
  spider_output_filenames
  spider_spect
  spider_tia_pipeline
)
target_compile_definitions(spider_tia
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")
# For running Spider pipeline scripts from the build tree.
set_target_properties(spider_tia PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install.
if(MSVC)
  install(TARGETS
    spider_clamp
    spider_id
    spider_tia
    RUNTIME_DEPENDENCY_SET spider_runtime_deps
  )
else()
  install(TARGETS
    spider_clamp
    spider_id
    spider_tia
  )
endif()

if(SPIDER_BUILD_WORKBENCH)
  install(TARGETS spider_workbench)
endif()

include(CMakeDependentOption)
cmake_dependent_option(
  SPIDER_BUNDLE_EXTERNAL_DLLS
  "Bundle external DLL dependencies in the install tree."
  ON
  MSVC
  OFF
)
if(SPIDER_BUNDLE_EXTERNAL_DLLS)
  install(RUNTIME_DEPENDENCY_SET spider_runtime_deps
    PRE_EXCLUDE_REGEXES
    "api-ms-"
    "ext-ms-"
    # Windows system DLL dependencies that are not resolved on the
    # GitHub-hosted runner image Windows Server 2025.  On Windows, DLL
    # dependency names are converted to lowercase for matching
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
