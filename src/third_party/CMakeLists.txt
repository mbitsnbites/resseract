# -*- mode: CMake; tab-width: 2; indent-tabs-mode: nil; -*-

#-----------------------------------------------------------------------------
# ENet
# Note: We use custom CMake rules instead of the ENet CMakeLists.txt in order
# to have a cleaner interface.
#-----------------------------------------------------------------------------
set(ENET_SOURCE_DIR enet)
add_library(enet
    ${ENET_SOURCE_DIR}/callbacks.c
    ${ENET_SOURCE_DIR}/compress.c
    ${ENET_SOURCE_DIR}/host.c
    ${ENET_SOURCE_DIR}/list.c
    ${ENET_SOURCE_DIR}/packet.c
    ${ENET_SOURCE_DIR}/peer.c
    ${ENET_SOURCE_DIR}/protocol.c
    ${ENET_SOURCE_DIR}/unix.c
    ${ENET_SOURCE_DIR}/win32.c
    )
target_include_directories(enet PUBLIC ${ENET_SOURCE_DIR}/include)
if(MINGW)
  target_link_libraries(enet winmm ws2_32)
endif()
if(MSVC)
  add_definitions(-W3)
else()
  add_definitions(-Wno-error)
endif()

include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckTypeSize)
check_function_exists("fcntl" HAS_FCNTL)
check_function_exists("poll" HAS_POLL)
check_function_exists("getaddrinfo" HAS_GETADDRINFO)
check_function_exists("getnameinfo" HAS_GETNAMEINFO)
check_function_exists("gethostbyname_r" HAS_GETHOSTBYNAME_R)
check_function_exists("gethostbyaddr_r" HAS_GETHOSTBYADDR_R)
check_function_exists("inet_pton" HAS_INET_PTON)
check_function_exists("inet_ntop" HAS_INET_NTOP)
check_struct_has_member("struct msghdr" "msg_flags" "sys/types.h;sys/socket.h" HAS_MSGHDR_FLAGS)
set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h" "sys/socket.h")
check_type_size("socklen_t" HAS_SOCKLEN_T BUILTIN_TYPES_ONLY)
unset(CMAKE_EXTRA_INCLUDE_FILES)
if(HAS_FCNTL)
  target_compile_definitions(enet PRIVATE -DHAS_FCNTL=1)
endif()
if(HAS_POLL)
  target_compile_definitions(enet PRIVATE -DHAS_POLL=1)
endif()
if(HAS_GETNAMEINFO)
  target_compile_definitions(enet PRIVATE -DHAS_GETNAMEINFO=1)
endif()
if(HAS_GETADDRINFO)
  target_compile_definitions(enet PRIVATE -DHAS_GETADDRINFO=1)
endif()
if(HAS_GETHOSTBYNAME_R)
  target_compile_definitions(enet PRIVATE -DHAS_GETHOSTBYNAME_R=1)
endif()
if(HAS_GETHOSTBYADDR_R)
  target_compile_definitions(enet PRIVATE -DHAS_GETHOSTBYADDR_R=1)
endif()
if(HAS_INET_PTON)
  target_compile_definitions(enet PRIVATE -DHAS_INET_PTON=1)
endif()
if(HAS_INET_NTOP)
  target_compile_definitions(enet PRIVATE -DHAS_INET_NTOP=1)
endif()
if(HAS_MSGHDR_FLAGS)
  target_compile_definitions(enet PRIVATE -DHAS_MSGHDR_FLAGS=1)
endif()
if(HAS_SOCKLEN_T)
  target_compile_definitions(enet PRIVATE -DHAS_SOCKLEN_T=1)
endif()

#-----------------------------------------------------------------------------
# SDL2
# Note: We'd like to use find_package here, but it doesn't support SDL2.
#-----------------------------------------------------------------------------
find_path(SDL2_INCLUDE_DIRS SDL.h
    HINTS $ENV{SDL2}
    PATH_SUFFIXES include/SDL2 include SDL2
    PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include/SDL2
    /usr/include/SDL2
    )
find_path(SDL2_MIXER_INCLUDE_DIR SDL_mixer.h
    HINTS $ENV{SDL2}
    PATH_SUFFIXES include/SDL2 include SDL2
    PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include/SDL2
    /usr/include/SDL2
    )
if(SDL2_MIXER_INCLUDE_DIR)
  list(APPEND SDL2_INCLUDE_DIRS ${SDL2_MIXER_INCLUDE_DIR})
endif()
find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h
    HINTS $ENV{SDL2}
    PATH_SUFFIXES include/SDL2 include SDL2
    PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local/include/SDL2
    /usr/include/SDL2
    )
if(SDL2_IMAGE_INCLUDE_DIR)
  list(APPEND SDL2_INCLUDE_DIRS ${SDL2_IMAGE_INCLUDE_DIR})
endif()
find_library(SDL2_LIBRARIES NAMES SDL2
    HINTS $ENV{SDL2}
    PATH_SUFFIXES lib64 lib)
find_library(SDL2_MIXER_LIBRARY NAMES SDL2_mixer
    HINTS $ENV{SDL2}
    PATH_SUFFIXES lib64 lib)
list(APPEND SDL2_LIBRARIES ${SDL2_MIXER_LIBRARY})
find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image
    HINTS $ENV{SDL2}
    PATH_SUFFIXES lib64 lib)
list(APPEND SDL2_LIBRARIES ${SDL2_IMAGE_LIBRARY})
if(NOT APPLE)
  find_library(SDL2MAIN_LIBRARY NAMES SDL2main
      HINTS $ENV{SDL2}
      PATH_SUFFIXES lib64 lib)
  list(APPEND SDL2_LIBRARIES ${SDL2MAIN_LIBRARY})
endif()
add_library(sdl2 INTERFACE)
target_include_directories(sdl2 INTERFACE ${SDL2_INCLUDE_DIRS})
target_link_libraries(sdl2 INTERFACE ${SDL2_LIBRARIES})

