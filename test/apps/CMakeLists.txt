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
