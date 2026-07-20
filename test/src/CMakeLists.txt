add_executable(test_output_filenames test_output_filenames.cc)
target_link_libraries(test_output_filenames
  PRIVATE
  spider_output_filenames
  ${ITK_LIBRARIES}
  GTest::gtest_main
)

add_executable(
  test_spect
  test_spect.cc
)
target_link_libraries(test_spect
  PRIVATE
  spider_spect
  GTest::gtest_main
)
target_compile_definitions(test_spect
  PRIVATE SPIDER_TEST_DATA_DIR="${SPIDER_TEST_DATA_DIR}")

include(GoogleTest)
gtest_discover_tests(test_output_filenames)
gtest_discover_tests(test_spect)

add_subdirectory(tia)
