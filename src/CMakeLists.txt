add_library(spider_spect
  STATIC
  spect.cc
)
target_include_directories(spider_spect
  PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}"
)
target_link_libraries(spider_spect
  PUBLIC
  gdcmMSFF
)
if(NOT SPIDER_HAVE_STD_CHRONO_TZ)
  target_link_libraries(spider_spect
    PUBLIC
    date::date-tz
  )
endif()
target_compile_definitions(spider_spect
  PUBLIC
  SPIDER_HAVE_STD_CHRONO_TZ=$<BOOL:${SPIDER_HAVE_STD_CHRONO_TZ}>
)

add_subdirectory(tia)
