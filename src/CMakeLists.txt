add_library(spider_spect
  STATIC
  spect.cc
)
target_include_directories(spider_spect
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(spider_spect
  PUBLIC
  gdcmMSFF
)

add_subdirectory(tia)
