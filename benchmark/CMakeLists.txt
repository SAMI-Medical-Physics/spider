add_executable(slice_compare slice_compare.cc)
target_link_libraries(slice_compare ${ITK_LIBRARIES})

add_executable(joint_hist joint_hist.cc)
target_link_libraries(joint_hist ${ITK_LIBRARIES})

option(SPIDER_DOWNLOAD_BENCHMARK_DATA "Download the benchmark data." ON)

add_subdirectory(snmmi)

configure_file(run.sh.in run.sh @ONLY)
file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
