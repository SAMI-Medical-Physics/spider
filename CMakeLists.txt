cmake_minimum_required(VERSION 3.21) # for RUNTIME_DEPENDENCY_SET
project(Spider VERSION 0.0.0)

# This requirement is driven by the calendar and time zone
# functionality added in C++20.  We also use C++23 <expected> for
# error handling and <print> for logging.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Determine whether the C++ standard library has C++20 calendar and
# time zone.  If not, fall back to Howard Hinnant's date and time
# library.  libc++ status:
# <https://github.com/llvm/llvm-project/issues/99982>.
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

set(SPIDER_ITK_REQUIRED_COMPONENTS
  ITKCommon
  ITKIOImageBase
  ITKIONIFTI
  ITKStatistics
)

option(SPIDER_BUILD_BENCHMARKS
  "Build the benchmarks.  Warning: Downloads 155 MB by default."
  OFF)

if(SPIDER_BUILD_BENCHMARKS)
  list(APPEND SPIDER_ITK_REQUIRED_COMPONENTS ITKIOPNG ITKTransform)
endif()

find_package(ITK REQUIRED
  COMPONENTS ${SPIDER_ITK_REQUIRED_COMPONENTS}
  OPTIONAL_COMPONENTS
  ITKIOMeta
  ITKIONRRD
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

# spider_workbench requires a POSIX-conformant system.
include(CMakeDependentOption)
cmake_dependent_option(
  SPIDER_BUILD_WORKBENCH
  "Build the graphical program Spider Workbench"
  OFF
  "NOT WIN32"
  OFF
)
add_subdirectory(apps)

if(SPIDER_BUILD_BENCHMARKS)
  add_subdirectory(benchmark)
endif()

option(BUILD_TESTING "Build the testing tree." OFF)
if(BUILD_TESTING)
  find_package(GTest REQUIRED)
  enable_testing()
  add_subdirectory(test)
endif()

add_subdirectory(bin)

add_subdirectory(man)

# Improve discoverability of options that are OFF by default.
message(STATUS "")
message(STATUS "Spider configuration (options OFF by default):")
if(NOT WIN32)
  message(STATUS
    "  Build Spider Workbench (SPIDER_BUILD_WORKBENCH): "
    "${SPIDER_BUILD_WORKBENCH}"
  )
endif()
if(MSVC)
  message(STATUS
    "  Bundle external executables (SPIDER_BUNDLE_EXTERNAL_EXECUTABLES): "
    "${SPIDER_BUNDLE_EXTERNAL_EXECUTABLES}"
  )
endif()
message(STATUS "  Build tests (BUILD_TESTING): ${BUILD_TESTING}")
message(STATUS "  Build benchmarks (SPIDER_BUILD_BENCHMARKS): "
  "${SPIDER_BUILD_BENCHMARKS}"
)
message(STATUS "")
