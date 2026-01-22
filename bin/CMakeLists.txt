include(GNUInstallDirs)
install(FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/../etc/Parameters_Rigid.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/spider"
)

configure_file(spider.sh.in spider @ONLY)

install(PROGRAMS
  "${CMAKE_CURRENT_BINARY_DIR}/spider"
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
