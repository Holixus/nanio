cmake_minimum_required(VERSION 2.6)

PROJECT(nanio)

OPTION(BUILD_SHARED_LIBRARY "Build shared library" OFF)
OPTION(BUILD_TESTS "Build tests application" OFF)

OPTION(IPV6_SOCKET "" ON)
OPTION(UNIX_SOCKET "" ON)
OPTION(IPV4_SOCKET "" ON)


OPTION(HOST_DEBUG "Log to console" OFF)


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

ADD_LIBRARY(${static_library_target} STATIC ${LIB_SOURCES})

IF(BUILD_SHARED_LIBRARY)
	ADD_LIBRARY(${shared_library_target} SHARED ${LIB_SOURCES})
	SET_TARGET_PROPERTIES(${shared_library_target} PROPERTIES PUBLIC_HEADER ${LIB_HEADERS} nano/io.h)
ENDIF(BUILD_SHARED_LIBRARY)


SET(CMAKE_C_FLAGS_RELEASE "-Os -Werror -ffast-math -fmerge-all-constants -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables")
SET(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")


IF(BUILD_TESTS)
	SET_TARGET_PROPERTIES(tests PROPERTIES COMPILE_FLAGS -fvisibility=hidden)
	SET_TARGET_PROPERTIES(tests PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)
ENDIF(BUILD_TESTS)

SET_TARGET_PROPERTIES(${static_library_target} PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)

IF(BUILD_SHARED_LIBRARY)
	SET_TARGET_PROPERTIES(${shared_library_target} PROPERTIES LINK_FLAGS -Wl,--gc-sections,--build-id=none)
ENDIF()


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
