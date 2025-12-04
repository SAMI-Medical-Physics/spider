add_executable(
  spider_make_tia_benchmark
  spider_make_tia_benchmark.cc
  tia_pipeline.cc
)
target_link_libraries(spider_make_tia_benchmark ${ITK_LIBRARIES})
target_compile_definitions(spider_make_tia_benchmark
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

install(TARGETS
  spider_make_tia_benchmark
)
