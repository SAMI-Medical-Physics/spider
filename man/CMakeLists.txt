include(GNUInstallDirs)
install(FILES
  spider.1
  spider_clamp.1
  spider_tia.1
  DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
)
