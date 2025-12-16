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

# ITK can include GDCM targets.  They may be different to the system
# GDCM's targets unless ITK is built with '-DITK_USE_SYSTEM_GDCM=ON'.
#
# XXX: Remove if/when we require ITKIOGDCM.
if(TARGET gdcmMSFF)
  message(VERBOSE "Using GDCM from ITK.")
else()
  find_package(GDCM REQUIRED)
  message(VERBOSE "Using system GDCM.")
endif()

add_subdirectory(src)
add_subdirectory(apps)

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
