add_executable(
  spider_tia
  spider_tia.cc
)
target_link_libraries(spider_tia spider_spect spider_tia_pipeline)
target_compile_definitions(spider_tia
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

install(TARGETS
  spider_tia
)
