add_test(NAME spider_read_dicom.exit_0
  COMMAND spider_read_dicom
  "${SPIDER_TEST_DATA_DIR}/PETAC" "${SPIDER_TEST_DATA_DIR}/PETAC"
)
