add_executable(
  spider_tia
  spider_tia.cc
)
target_link_libraries(spider_tia spider_spect spider_tia_pipeline)
target_compile_definitions(spider_tia
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

if(MSVC)
  install(TARGETS
    spider_tia
    RUNTIME_DEPENDENCY_SET spider_runtime_deps
  )
  option(SPIDER_BUNDLE_EXTERNAL_DLLS
    "Bundle external DLL dependencies in the install tree."
    ON
  )
  if(SPIDER_BUNDLE_EXTERNAL_DLLS)
    install(RUNTIME_DEPENDENCY_SET spider_runtime_deps
      PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
      POST_EXCLUDE_REGEXES ".*WINDOWS[\\/]system32.*"
    )
  endif()
else()
  install(TARGETS
    spider_tia
  )
endif()
