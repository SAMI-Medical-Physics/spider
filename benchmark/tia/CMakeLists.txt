configure_file(run.sh.in run.sh @ONLY)

file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
  PERMISSIONS OWNER_READ OWNER_EXECUTE)

add_executable(log10_ratio log10_ratio.cc)
target_link_libraries(log10_ratio ${ITK_LIBRARIES})
