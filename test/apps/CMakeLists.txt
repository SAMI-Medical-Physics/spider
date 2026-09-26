add_test(
  NAME spider_id
  COMMAND $<TARGET_FILE:spider_id>
  "${SPIDER_TEST_DATA_DIR}/PETAC"
)

set_property(TEST spider_id PROPERTY
  # CMake uses a non-standard regexp implementation.  See:
  # <https://cmake.org/cmake/help/latest/command/string.html#regex-specification>.
  PASS_REGULAR_EXPRESSION
  "^C11Phantom--C11Phantom__\\^11\\^Carbon\\+\\+20181105120000\n$"
)

# This test checks that spider_id's directory operand may contain
# Unicode characters on MS-Windows.
if(WIN32)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/テスト/PETAC")
  set(TEST_FILE "C11PHANTOM.PT.PET-MR_QA_MULTIPLE_PET_PHANTOM.30003.0001.2018.11.07.17.59.46.799308.153686696.IMA")
  file(COPY_FILE
    "${SPIDER_TEST_DATA_DIR}/PETAC/${TEST_FILE}"
    "${CMAKE_CURRENT_BINARY_DIR}/テスト/PETAC/${TEST_FILE}"
  )
  add_test(
    NAME spider_id_unicode
    COMMAND $<TARGET_FILE:spider_id>
    "テスト/PETAC"
  )
endif()
