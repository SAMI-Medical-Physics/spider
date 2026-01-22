add_executable(
  spider_dicom_dump
  spider_dicom_dump.cc
)
target_link_libraries(spider_dicom_dump spider_spect)
target_compile_definitions(spider_dicom_dump
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

add_executable(
  spider_make_tia_benchmark
  spider_make_tia_benchmark.cc
)
target_link_libraries(spider_make_tia_benchmark spider_tia_pipeline)
target_compile_definitions(spider_make_tia_benchmark
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

add_executable(
  spider_tia
  spider_tia.cc
)
target_link_libraries(spider_tia spider_spect spider_tia_pipeline)
target_compile_definitions(spider_tia
  PRIVATE SPIDER_VERSION="${PROJECT_VERSION}")

install(TARGETS
  spider_dicom_dump
  spider_make_tia_benchmark
  spider_tia
)
