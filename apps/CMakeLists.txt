add_executable(
  spider_read_dicom
  spider_read_dicom.cc
)
target_link_libraries(spider_read_dicom spider_spect)
target_compile_definitions(spider_read_dicom
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

add_executable(
  spider_make_tia_benchmark
  spider_make_tia_benchmark.cc
)
target_link_libraries(spider_make_tia_benchmark spider_tia)
target_compile_definitions(spider_make_tia_benchmark
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

install(TARGETS
  spider_read_dicom
  spider_make_tia_benchmark
)
