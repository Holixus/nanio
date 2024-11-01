cmake_minimum_required(VERSION 2.6)

PROJECT(nanio)

OPTION(BUILD_SHARED_LIBRARY "Build shared library" OFF)

OPTION(IPV6_SOCKET "" ON)
OPTION(UNIX_SOCKET "" ON)
OPTION(IPV4_SOCKET "" ON)


OPTION(HOST_DEBUG "Enable debug logging" OFF)
OPTION(USE_SYSLOG "Enable logging to syslog" OFF)

CONFIGURE_FILE(io_log.c.in io_log.c @ONLY)


ADD_DEFINITIONS(-pipe --std=gnu99 -ftabstop=4 -Wno-unused-function)
ADD_DEFINITIONS(-Wall -Wmissing-declarations -Winit-self -Wswitch-enum -Wundef)
ADD_DEFINITIONS(-Wmissing-field-initializers -Wredundant-decls -Wpointer-arith)

ADD_DEFINITIONS(-D_GNU_SOURCE)

SET(static_library_target nanio_static)
SET(shared_library_target nanio)

IF(BUILD_TESTS)
	ADD_EXECUTABLE(tests tests.c)
	TARGET_LINK_LIBRARIES(tests ${static_library_target})
ENDIF(BUILD_TESTS)

FILE(GLOB LIB_SOURCES io_*.c io.c)
FILE(GLOB IPV6_SOURCES io_ipv6.c)
FILE(GLOB IPV4_SOURCES io_ipv4.c)

IF(NOT IPV6_SOCKET)
	LIST(REMOVE_ITEM LIB_SOURCES ${IPV6_SOURCES})
	message(STATUS "removed IPv6: ${IPV6_SOURCES}")
	message(STATUS "cbd: ${CMAKE_CURRENT_BINARY_DIR}")
ENDIF()

IF(NOT IPV4_SOCKET)
	LIST(REMOVE_ITEM LIB_SOURCES ${IPV4_SOURCES})
	message(STATUS "removed IPv4: ${IPV4_SOURCES}")
	message(STATUS "cbd: ${CMAKE_CURRENT_BINARY_DIR}")
ENDIF()

ADD_LIBRARY(${static_library_target} STATIC ${LIB_SOURCES})

IF(BUILD_SHARED_LIBRARY)
	ADD_LIBRARY(${shared_library_target} SHARED ${LIB_SOURCES})
ENDIF(BUILD_SHARED_LIBRARY)


SET(CMAKE_C_FLAGS_RELEASE "-Os -Werror -ffast-math -fmerge-all-constants -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables")
SET(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")


IF(BUILD_TESTS)
	SET_TARGET_PROPERTIES(tests PROPERTIES COMPILE_FLAGS -fvisibility=hidden)
	SET_TARGET_PROPERTIES(tests PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)
ENDIF(BUILD_TESTS)


IF(BUILD_TESTS)
	INSTALL(TARGETS tests
		ARCHIVE DESTINATION lib
		LIBRARY DESTINATION lib
		RUNTIME DESTINATION bin
	)
ENDIF(BUILD_TESTS)

INSTALL(TARGETS ${static_library_target}
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

IF(BUILD_SHARED_LIBRARY)
	INSTALL(TARGETS ${shared_library_target}
		ARCHIVE DESTINATION lib
		LIBRARY DESTINATION lib
		RUNTIME DESTINATION bin
	)
ENDIF()
