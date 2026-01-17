add_test(NAME spider_dicom_dump.exit_0
  COMMAND spider_dicom_dump
  "${SPIDER_TEST_DATA_DIR}/PETAC" "${SPIDER_TEST_DATA_DIR}/PETAC"
)
