# Benchmark Spider's performance using image data from patient 6 of
# the SNMMI Lu-177 Dosimetry Challenge 2021:
# <https://deepblue.lib.umich.edu/data/collections/hm50ts030>.

if(SPIDER_DOWNLOAD_BENCHMARK_DATA)
  include(FetchContent)

  # Lu-177 DOTATATE Anonymized Patient Datasets: Multi-Time Point
  # Lu-177 SPECT/CT Scans.  License: CC BY-NC 4.0.
  FetchContent_Declare(
    fetch_benchmark_snmmi_pt6_spectcts
    URL https://zenodo.org/records/17271433/files/patient_6.zip?download=1
    URL_HASH SHA256=f81ebef54fb2957cb5b5ea617bb82db504e6a97a88002013cfd3c2f4e0191caa
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  # Lu-177 DOTATATE Anonymized Patient Datasets: Lu-177 SPECT Time
  # Integrated Activity Maps.  License: CC BY-NC 4.0.
  FetchContent_Declare(
    fetch_benchmark_snmmi_pt6_tia
    URL https://zenodo.org/records/17680076/files/patient_6.zip?download=1
    URL_HASH SHA256=f920556d04c4c654315d7a2bc3e400b8d556cb95514a12261ea409c7578d1cbf
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  set(FETCHCONTENT_QUIET OFF)
  FetchContent_MakeAvailable(fetch_benchmark_snmmi_pt6_spectcts)
  FetchContent_MakeAvailable(fetch_benchmark_snmmi_pt6_tia)

  set(SPIDER_BENCHMARK_SNMMI_PT6_SPECTCTS_DIR "${fetch_benchmark_snmmi_pt6_spectcts_SOURCE_DIR}")
  set(SPIDER_BENCHMARK_SNMMI_PT6_TIA_DIR "${fetch_benchmark_snmmi_pt6_tia_SOURCE_DIR}")
else()
  # Allow configuring without network access.
  set(SPIDER_BENCHMARK_SNMMI_PT6_SPECTCTS_DIR "" CACHE PATH
    "Extracted SNMMI Challenge patient 6 serial SPECT/CT dataset (containing 'SPECT_Cts' directory)."
  )
  set(SPIDER_BENCHMARK_SNMMI_PT6_TIA_DIR "" CACHE PATH
    "Extracted SNMMI Challenge patient 6 TIA image dataset (containing 'TIA' directory)."
  )
endif()

if(NOT SPIDER_BENCHMARK_SNMMI_PT6_SPECTCTS_DIR OR NOT SPIDER_BENCHMARK_SNMMI_PT6_TIA_DIR)
  message(WARNING
    "SPIDER_BUILD_BENCHMARKS is set to ON and "
    "SPIDER_DOWNLOAD_BENCHMARK_DATA is set to OFF, but "
    "SPIDER_BENCHMARK_SNMMI_PT6_SPECTCTS_DIR or "
    "SPIDER_BENCHMARK_SNMMI_PT6_TIA_DIR is unset."
  )
endif()

add_executable(set_modality_pt
  set_modality_pt.cc
)
target_link_libraries(set_modality_pt
  PRIVATE
  gdcmMSFF
)

configure_file(run.sh.in run.sh @ONLY)

file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/run.sh"
  PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
