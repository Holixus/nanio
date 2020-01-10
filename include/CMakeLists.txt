option(INSTALL_NANIO_HEADERS "Install libnanio headers." ON)

if(INSTALL_NANIO_HEADERS)

	file(GLOB headers "nano/*.h")

	install(FILES ${headers}
		DESTINATION include/nano
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

	CONFIGURE_FILE(nano/io.h.in nano/io.h @ONLY)

endif(INSTALL_NANIO_HEADERS)
