option(INSTALL_NANIO_HEADERS "Install libnanio headers." ON)

if(INSTALL_NANIO_HEADERS)

	file(GLOB headers "nano/*.h")

	FILE(GLOB IPV6_HEADERS nano/io_ipv6.h)
	FILE(GLOB IPV4_HEADERS nano/io_ipv4.h)

	IF(NOT IPV6_SOCKET)
		LIST(REMOVE_ITEM headers ${IPV6_HEADERS})
	ENDIF()

	IF(NOT IPV4_SOCKET)
		LIST(REMOVE_ITEM headers ${IPV4_HEADERS})
	ENDIF()

	install(FILES ${headers}
		DESTINATION include/nano
		PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ)

	CONFIGURE_FILE(nano/io.h.in nano/io.h @ONLY)

endif(INSTALL_NANIO_HEADERS)
