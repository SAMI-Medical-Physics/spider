add_executable(test_exp_fit_functor test_exp_fit_functor.cc)
target_include_directories(test_exp_fit_functor
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
  "${CMAKE_SOURCE_DIR}/src"
)
target_link_libraries(
  test_exp_fit_functor
  GTest::gtest_main
  ${ITK_LIBRARIES}
)

add_executable(
  test_tia_pipeline
  test_tia_pipeline.cc
)
target_include_directories(test_tia_pipeline
  PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
target_link_libraries(
  test_tia_pipeline
  spider_tia_pipeline
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(test_exp_fit_functor)
gtest_discover_tests(test_tia_pipeline)
