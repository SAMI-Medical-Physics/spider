option(SPIDER_DOWNLOAD_TEST_DATA "Download the test data." ON)
if(SPIDER_DOWNLOAD_TEST_DATA)
  include(FetchContent)
  # PET phantom data from Siemens Biograph mMR with Carbon-11 scanned
  # over 12 half-lives.  35.4 MB.  License: CC BY 4.0.
  #
  # FIXME: The tests only require 1 DICOM file and this dataset
  # contains 254.
  FetchContent_Declare(
    fetch_test_data
    URL https://zenodo.org/records/4751233/files/pet-images-01.zip?download=1
    URL_HASH SHA256=96a764edff00fdbae519ddc7f8aa7b36d667ccc4773aa40b0d6c3c437165ced9
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(FETCHCONTENT_QUIET OFF)
  FetchContent_MakeAvailable(fetch_test_data)
  set(SPIDER_TEST_DATA_DIR "${fetch_test_data_SOURCE_DIR}")
else()
  # Allow configuring without network access.
  set(SPIDER_TEST_DATA_DIR "" CACHE PATH
    "Test data.  Extracted and containing 'PETAC' directory.")
endif()

if(NOT SPIDER_TEST_DATA_DIR)
  message(WARNING
    "SPIDER_DOWNLOAD_TEST_DATA is set to OFF but SPIDER_TEST_DATA_DIR is unset.")
endif()

add_executable(
  test_spect
  test_spect.cc
)
target_link_libraries(
  test_spect
  spider_spect
  GTest::gtest_main
)
target_compile_definitions(test_spect
  PRIVATE SPIDER_TEST_DATA_DIR="${SPIDER_TEST_DATA_DIR}")
target_compile_features(test_spect PRIVATE cxx_std_17)

include(GoogleTest)
gtest_discover_tests(test_spect)

add_subdirectory(tia)
