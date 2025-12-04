cmake_minimum_required(VERSION 3.10)
project(Spider VERSION 0.0.0)

find_package(ITK REQUIRED
  COMPONENTS
  ITKCommon
  ITKIOImageBase
  ITKIONIFTI
  ITKStatistics
)

# To register the IO factory.  This is required before 'add_executable' or
# 'add_library'.
include(${ITK_USE_FILE})        # sets CMAKE_CXX_STANDARD to 17 if unset

add_subdirectory(src)

option(BUILD_TESTING "Build the testing tree." OFF)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(test)
endif()

option(SPIDER_BUILD_BENCHMARKS
  "Build the benchmarks.  Warning: Downloads 155 MB by default."
  OFF)
if(SPIDER_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()
