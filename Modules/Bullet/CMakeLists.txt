cmake_minimum_required(VERSION 3.28)
project(RayneBullet)

include(../../CMake/LibraryTarget.cmake)
include(../../CMake/Rayne.cmake)

if(WIN32)
        set(BULLET_PATCH_OPTIONS PATCH_COMMAND powershell -Command "(Get-Content <SOURCE_DIR>/src/LinearMath/btScalar.h).replace('#define BT_DEBUG', '') | Set-Content <SOURCE_DIR>/src/LinearMath/btScalar.h")
endif()

set(BUILD_SHARED_LIBS OFF)
set(USE_GRAPHICAL_BENCHMARK OFF)
set(USE_MSVC_RUNTIME_LIBRARY_DLL ON)
set(BUILD_CPU_DEMOS OFF)
set(USE_GLUT OFF)
set(BUILD_ENET OFF)
set(BUILD_CLSOCKET OFF)
set(BUILD_OPENGL3_DEMOS OFF)
set(BUILD_BULLET2_DEMOS OFF)
set(BUILD_EXTRAS OFF)
set(BUILD_UNIT_TESTS OFF)
set(BUILD_BULLET3 OFF)

block(SCOPE_FOR VARIABLES)
        set(CMAKE_POLICY_DEFAULT_CMP0115 OLD)
        rayne_fetch_content(Bullet
                GIT_REPOSITORY https://github.com/bulletphysics/bullet3.git
                GIT_TAG refs/tags/3.06
                GIT_SHALLOW ON
                GIT_PROGRESS ON
                ${BULLET_PATCH_OPTIONS})
endblock()
FetchContent_GetProperties(Bullet SOURCE_DIR BULLET_SOURCE_DIR)

set(VERSION_MAJOR 0)
set(VERSION_MINOR 5)
set(VERSION_PATCH 0)
set(VERSION_ABI 1)
set(VERSION_STRING "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

set(SOURCES
        RNBulletInit.cpp
        RNBulletWorld.cpp
        RNBulletCollisionObject.cpp
        RNBulletKinematicController.cpp
        RNBulletMaterial.cpp
        RNBulletRigidBody.cpp
        RNBulletShape.cpp
        RNBulletConstraint.cpp
        RNBulletInternals.cpp)

set(HEADERS
	RNBullet.h
        RNBulletWorld.h
        RNBulletCollisionObject.h
        RNBulletKinematicController.h
        RNBulletMaterial.h
        RNBulletRigidBody.h
        RNBulletShape.h
        RNBulletConstraint.h)

set(PRIVATE_HEADERS
        RNBulletInternals.h)

set(DEFINES RN_BUILD_BULLET)

set(VENDOR_INCLUDES
        "${BULLET_SOURCE_DIR}/src")

rayne_add_library(RayneBullet "${SOURCES}" "${HEADERS}" "${VERSION_STRING}" "${VERSION_ABI}" PUBLIC_LIBRARIES Rayne PRIVATE_LIBRARIES BulletDynamics BulletCollision LinearMath PRIVATE_HEADERS "${PRIVATE_HEADERS}")
rayne_set_module_output_directory(RayneBullet)

target_include_directories(RayneBullet SYSTEM PRIVATE ${VENDOR_INCLUDES})
target_compile_definitions(RayneBullet PRIVATE "${DEFINES}")
