cmake_minimum_required(VERSION 3.10)
project(Spider VERSION 0.0.0)

# This requirement is driven by the calendar and time zone
# functionality added in C++20 and std::expected from C++23.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Determine whether the C++ standard library has C++20 calendar and
# time zone.  If not, fall back to Howard Hinnant's date and time
# library.
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
  #include <chrono>

  int
  main()
  {
    std::chrono::zoned_time<std::chrono::seconds> {};
    return 0;
  }
" SPIDER_HAVE_STD_CHRONO_TZ)
if(NOT SPIDER_HAVE_STD_CHRONO_TZ)
  find_package(date CONFIG REQUIRED) # https://github.com/HowardHinnant/date
endif()

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

option(SPIDER_BUILD_BENCHMARKS
  "Build the benchmarks.  Warning: Downloads 155 MB by default."
  OFF)
if(SPIDER_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()

option(BUILD_TESTING "Build the testing tree." OFF)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(test)
endif()
