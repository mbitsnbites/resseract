# -*- mode: CMake; tab-width: 2; indent-tabs-mode: nil; -*-
cmake_minimum_required(VERSION 3.5)
project(Resseract)

# Add zlib.
find_package(ZLIB REQUIRED)

# Add OpenGL.
find_package(OpenGL REQUIRED)

# Platform specific libraries.
if(UNIX AND NOT APPLE)
  set(PLATFORM_LIBRARIES rt)
endif()

# Add third party libraries (e.g. built from source).
add_subdirectory(third_party)

# Client executable.
set(CLIENT_SOURCES
    shared/crypto.cpp
    shared/geom.cpp
    shared/glemu.cpp
    shared/stream.cpp
    shared/tools.cpp
    shared/zip.cpp
    engine/aa.cpp
    engine/bih.cpp
    engine/blend.cpp
    engine/client.cpp
    engine/command.cpp
    engine/console.cpp
    engine/dynlight.cpp
    engine/grass.cpp
    engine/light.cpp
    engine/main.cpp
    engine/material.cpp
    engine/menus.cpp
    engine/movie.cpp
    engine/normal.cpp
    engine/octa.cpp
    engine/octaedit.cpp
    engine/octarender.cpp
    engine/physics.cpp
    engine/pvs.cpp
    engine/rendergl.cpp
    engine/renderlights.cpp
    engine/rendermodel.cpp
    engine/renderparticles.cpp
    engine/rendersky.cpp
    engine/rendertext.cpp
    engine/renderva.cpp
    engine/server.cpp
    engine/serverbrowser.cpp
    engine/shader.cpp
    engine/sound.cpp
    engine/stain.cpp
    engine/texture.cpp
    engine/ui.cpp
    engine/water.cpp
    engine/world.cpp
    engine/worldio.cpp
    game/ai.cpp
    game/client.cpp
    game/entities.cpp
    game/game.cpp
    game/render.cpp
    game/scoreboard.cpp
    game/server.cpp
    game/waypoint.cpp
    game/weapon.cpp
  )
set(CLIENT_INCLUDES
    shared
    engine
    game
    )
set(CLIENT_LIBRARIES
    enet
    sdl2
    ${OPENGL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${PLATFORM_LIBRARIES}
    )
add_executable(resseract ${CLIENT_SOURCES})
target_include_directories(resseract PRIVATE ${CLIENT_INCLUDES})
target_link_libraries(resseract ${CLIENT_LIBRARIES})

# Resseract client launcher.
if(UNIX)
  configure_file("scripts/runclient.sh.in" "${CMAKE_CURRENT_BINARY_DIR}/runclient.sh" @ONLY)
endif()

# Server executable.
set(SERVER_SOURCES
    shared/crypto.cpp
    shared/stream.cpp
    shared/tools.cpp
    engine/command.cpp
    engine/server.cpp
    engine/worldio.cpp
    game/entities.cpp
    game/server.cpp
    )
set(SERVER_INCLUDES
    shared
    engine
    game
    )
set(SERVER_LIBRARIES
    enet
    ${ZLIB_LIBRARIES}
    )
add_executable(ress_server ${SERVER_SOURCES})
target_compile_definitions(ress_server PRIVATE -DSTANDALONE)
target_include_directories(ress_server PRIVATE ${SERVER_INCLUDES})
target_link_libraries(ress_server ${SERVER_LIBRARIES})

# Master executable.
set(MASTER_SOURCES
    shared/crypto.cpp
    shared/stream.cpp
    shared/tools.cpp
    engine/command.cpp
    engine/master.cpp
    )
set(MASTER_INCLUDES
    shared
    engine
    game
    )
set(MASTER_LIBRARIES
    enet
    ${ZLIB_LIBRARIES}
    )
add_executable(ress_master ${MASTER_SOURCES})
target_compile_definitions(ress_master PRIVATE -DSTANDALONE)
target_include_directories(ress_master PRIVATE ${MASTER_INCLUDES})
target_link_libraries(ress_master ${MASTER_LIBRARIES})

