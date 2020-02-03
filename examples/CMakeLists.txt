cmake_minimum_required(VERSION 2.6)

PROJECT(nanio_examples)

ADD_DEFINITIONS(-pipe --std=gnu99 -ftabstop=4 -Wno-unused-function)
ADD_DEFINITIONS(-Wall -Wmissing-declarations -Winit-self -Wswitch-enum -Wundef)
ADD_DEFINITIONS(-Wmissing-field-initializers -Wredundant-decls -Wpointer-arith)

ADD_DEFINITIONS(-D_GNU_SOURCE)

ADD_EXECUTABLE(sock_echo sock-echo.c)
TARGET_LINK_LIBRARIES(sock_echo nanio_static)

SET(CMAKE_C_FLAGS_RELEASE "-Os -Werror -ffast-math -fmerge-all-constants -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fvisibility=hidden")
SET(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")

SET(CMAKE_LD_FLAGS_RELEASE "-Os -Werror -ffast-math -fmerge-all-constants -ffunction-sections -fdata-sections -fno-asynchronous-unwind-tables -fvisibility=hidden")

#SET_TARGET_PROPERTIES(sock_echo PROPERTIES COMPILE_FLAGS -fvisibility=hidden)
SET_TARGET_PROPERTIES(sock_echo PROPERTIES LINK_FLAGS_RELEASE -Wl,--gc-sections,--build-id=none)


INSTALL(TARGETS sock_echo
	ARCHIVE DESTINATION lib
	LIBRARY DESTINATION lib
	RUNTIME DESTINATION bin
)

