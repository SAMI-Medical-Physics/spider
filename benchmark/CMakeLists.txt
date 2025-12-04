# Benchmark Spider's performance using image data from the SNMMI
# Lu-177 Dosimetry Challenge 2021:
# <https://snmmi.org/Therapy/SNMMI-THERAPY/Dosimetry_Challenge.aspx>.

option(SPIDER_DOWNLOAD_BENCHMARK_DATA "Download the benchmark data." ON)
if(SPIDER_DOWNLOAD_BENCHMARK_DATA)
  include(FetchContent)

  # Lu-177 DOTATATE Anonymized Patient Datasets: Multi-Time Point
  # Lu-177 SPECT/CT Scans.  License: CC BY-NC 4.0.
  FetchContent_Declare(
    fetch_benchmark_spectcts
    URL https://zenodo.org/records/17271433/files/patient_4.zip?download=1
    URL_HASH SHA256=e7f8e89a81bd92c72f5c7190c55253276d94abe42f4068b69e1ff042f6a12a38
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  # Lu-177 DOTATATE Anonymized Patient Datasets: Lu-177 SPECT Time
  # Integrated Activity Maps.  License: CC BY-NC 4.0.
  FetchContent_Declare(
    fetch_benchmark_tia
    URL https://zenodo.org/records/17680076/files/patient_4.zip?download=1
    URL_HASH SHA256=3a91871d08fdebac70ff685885308843acf27f20dc39badb089a07c291f86405
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )

  set(FETCHCONTENT_QUIET OFF)
  FetchContent_MakeAvailable(fetch_benchmark_spectcts)
  FetchContent_MakeAvailable(fetch_benchmark_tia)

  set(SPIDER_BENCHMARK_SPECTCTS_DIR "${fetch_benchmark_spectcts_SOURCE_DIR}")
  set(SPIDER_BENCHMARK_TIA_DIR "${fetch_benchmark_tia_SOURCE_DIR}")
else()
  # Allow configuring without network access.
  set(SPIDER_BENCHMARK_SPECTCTS_DIR "" CACHE PATH
    "Benchmark SPECT/CTs.  Extracted and containing 'SPECT_Cts' directory.")
  set(SPIDER_BENCHMARK_TIA_DIR "" CACHE PATH
    "Benchmark TIA image.  Extracted and containing 'TIA' directory.")
endif()

if(NOT SPIDER_BENCHMARK_SPECTCTS_DIR OR NOT SPIDER_BENCHMARK_TIA_DIR)
  message(WARNING
    "SPIDER_BUILD_BENCHMARKS is set to ON and "
    "SPIDER_DOWNLOAD_BENCHMARK_DATA is set to OFF, but "
    "SPIDER_BENCHMARK_SPECTCTS_DIR or SPIDER_BENCHMARK_TIA_DIR is unset."
  )
endif()

configure_file(tia.sh.in tia.sh @ONLY)

file(CHMOD "${CMAKE_CURRENT_BINARY_DIR}/tia.sh"
  PERMISSIONS OWNER_READ OWNER_EXECUTE)

add_executable(log10_ratio log10_ratio.cc)
target_link_libraries(log10_ratio ${ITK_LIBRARIES})
