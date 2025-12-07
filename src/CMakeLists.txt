add_library(spider_tia
  STATIC
  tia_pipeline.cc
)
target_include_directories(spider_tia
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(spider_tia
  PUBLIC
  ${ITK_LIBRARIES}
)

add_executable(
  spider_make_tia_benchmark
  spider_make_tia_benchmark.cc
)
target_link_libraries(spider_make_tia_benchmark spider_tia)
target_compile_definitions(spider_make_tia_benchmark
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

install(TARGETS
  spider_make_tia_benchmark
)
