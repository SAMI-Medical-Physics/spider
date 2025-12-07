add_executable(test_exp_fit_functor test_exp_fit_functor.cc)
target_include_directories(test_exp_fit_functor PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(
  test_exp_fit_functor
  GTest::gtest_main
  ${ITK_LIBRARIES}
)
# GoogleTest requires at least C++17.
target_compile_features(test_exp_fit_functor PRIVATE cxx_std_17)

add_executable(
  test_tia_pipeline
  test_tia_pipeline.cc
)
target_link_libraries(
  test_tia_pipeline
  spider_tia
  GTest::gtest_main
)
target_compile_features(test_tia_pipeline PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(test_exp_fit_functor)
gtest_discover_tests(test_tia_pipeline)
