cmake_minimum_required (VERSION 2.8)
project (z80cpp)

# Set the rpath policy and C++ version for MacOS builds
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    # set the RPATH policy
    set(CMAKE_MACOSX_RPATH 1)

    # specify the C++ standard
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED True)
endif()

set (API_REVISION 1)
set (VERSION_MAJOR 0)
set (VERSION_MINOR 0)
set (RELEASE_TYPE "")
set (VERSION_STR "${API_REVISION}.${VERSION_MAJOR}.${VERSION_MINOR}${RELEASE_TYPE}")

if (CMAKE_COMPILER_IS_GNUCXX)
    set (CMAKE_CXX_FLAGS "-Wall -O3 -std=c++14")
endif ()

include_directories(BEFORE . include)

set (z80cpp_sources src/z80.cpp)
add_library (z80cpp-static STATIC ${z80cpp_sources})
set_target_properties (z80cpp-static PROPERTIES OUTPUT_NAME z80cpp)
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    add_library (z80cpp SHARED ${z80cpp_sources})
# Affects Win32 only: avoid dynamic/static *.lib files naming conflict 
    set_target_properties (z80cpp-static PROPERTIES PREFIX "lib")
endif ()

if (NOT DEFINED Z80CPP_STATIC_ONLY)
    set_target_properties(z80cpp
        PROPERTIES VERSION ${VERSION_STR} SOVERSION ${API_REVISION}
    )
endif ()
set_target_properties(z80cpp-static
    PROPERTIES VERSION ${VERSION_STR} SOVERSION ${API_REVISION}
)

if ("${CMAKE_CPP_IMPLICIT_LINK_DIRECTORIES}" MATCHES "lib64")
    set (LIB_DIR "lib64")
else ()
    set (LIB_DIR "lib")
endif ()
if (NOT DEFINED Z80CPP_STATIC_ONLY)
    install (TARGETS z80cpp LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
endif ()
install (TARGETS z80cpp-static LIBRARY DESTINATION ${LIB_DIR} ARCHIVE DESTINATION ${LIB_DIR})
install (DIRECTORY include/ DESTINATION include/z80cpp PATTERN "*.h" )
