cmake_minimum_required(VERSION 3.5...3.10)

# Get name from package.xml to avoid duplicating the version number
file(READ package.xml SERIAL_CPP_PACKAGE_XML)
string(REGEX MATCH "<version>[0-9]+\\.[0-9]+\\.[0-9]+</version>" SERIAL_CPP_DIRTY_VERSION_STRING ${SERIAL_CPP_PACKAGE_XML})
string(REGEX REPLACE "^<version>([0-9]+)\\.([0-9]+)\\.([0-9]+)</version>$" "\\1" SERIAL_CPP_MAJOR_VERSION "${SERIAL_CPP_DIRTY_VERSION_STRING}")
string(REGEX REPLACE "^<version>([0-9]+)\\.([0-9]+)\\.([0-9]+)</version>$" "\\2" SERIAL_CPP_MINOR_VERSION "${SERIAL_CPP_DIRTY_VERSION_STRING}")
string(REGEX REPLACE "^<version>([0-9]+)\\.([0-9]+)\\.([0-9]+)</version>$" "\\3" SERIAL_CPP_PATCH_VERSION "${SERIAL_CPP_DIRTY_VERSION_STRING}")

project(serial_cpp VERSION ${SERIAL_CPP_MAJOR_VERSION}.${SERIAL_CPP_MINOR_VERSION}.${SERIAL_CPP_PATCH_VERSION})

# CMake options

# For historical reasons, the default library type is static, but shared is also supported
option(BUILD_SHARED_LIBS "Build libraries as shared as opposed to static" OFF)
option(BUILD_TESTING "Build tests" OFF)
option(serial_cpp_INSTALL "Enable generation of serial_cpp install targets" ON)
mark_as_advanced(serial_cpp_INSTALL)
option(serial_cpp_FORCE_RESPECT_BUILD_TESTING "If also BUILD_TESTING is ON, enable tests even if project was included via add_subdirectory/FetchContent" OFF)
mark_as_advanced(serial_cpp_FORCE_RESPECT_BUILD_TESTING)

# Windows async (overlapped) I/O option
# When ON, uses FILE_FLAG_OVERLAPPED allowing concurrent read/write from different threads.
# When OFF (default), uses the original blocking synchronous implementation.
option(SERIAL_CPP_WIN_ASYNC "Use overlapped (async) I/O on Windows for non-blocking concurrent read/write" OFF)

# Detect if project is top level or not (fall back for CMake < 3.21)
if(CMAKE_VERSION VERSION_LESS "3.21.0")
    if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
        set(PROJECT_IS_TOP_LEVEL ON)
    else()
        set(PROJECT_IS_TOP_LEVEL OFF)
    endif()
endif()


# Default install locations
include(GNUInstallDirs)

if(APPLE)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(FOUNDATION_LIBRARY Foundation)
endif()

## Sources
set(serial_cpp_SRCS
    src/serial.cc
    include/serial_cpp/serial.h
    include/serial_cpp/v8stdint.h
    include/serial_cpp/serial_compat.h
)
if(APPLE)
    # If OSX
    list(APPEND serial_cpp_SRCS src/impl/unix.cc)
    list(APPEND serial_cpp_SRCS src/impl/list_ports/list_ports_osx.cc)
elseif(UNIX)
    # If unix
    list(APPEND serial_cpp_SRCS src/impl/unix.cc)
    list(APPEND serial_cpp_SRCS src/impl/list_ports/list_ports_linux.cc)
else()
    # If windows
    if(SERIAL_CPP_WIN_ASYNC)
        list(APPEND serial_cpp_SRCS src/impl/win_async.cc)
    else()
        list(APPEND serial_cpp_SRCS src/impl/win.cc)
    endif()
    list(APPEND serial_cpp_SRCS src/impl/list_ports/list_ports_win.cc)
endif()

## Add serial library
add_library(${PROJECT_NAME} ${serial_cpp_SRCS})
## Add alias for FetchContent/add_subdirectory usage
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

## Include directories
target_include_directories(${PROJECT_NAME} PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
                                                  "$<INSTALL_INTERFACE:$<INSTALL_PREFIX>/${CMAKE_INSTALL_INCLUDEDIR}>")

# Link neeed libraries
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FOUNDATION_LIBRARY} ${IOKIT_LIBRARY})
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} PRIVATE rt pthread)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE setupapi)
endif()

## Shared library support on Windows
if(WIN32 AND BUILD_SHARED_LIBS)
    set_target_properties(${PROJECT_NAME} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

## Examples
add_executable(serial_cpp_example examples/serial_cpp_example.cc)
add_dependencies(serial_cpp_example ${PROJECT_NAME}::${PROJECT_NAME})
target_link_libraries(serial_cpp_example ${PROJECT_NAME}::${PROJECT_NAME})

## Async test example (demonstrates concurrent read/write)
add_executable(serial_cpp_async_test examples/serial_cpp_async_test.cc)
add_dependencies(serial_cpp_async_test ${PROJECT_NAME}::${PROJECT_NAME})
target_link_libraries(serial_cpp_async_test ${PROJECT_NAME}::${PROJECT_NAME})
if(WIN32)


    if(SERIAL_CPP_WIN_ASYNC)
        target_compile_definitions(${PROJECT_NAME} PRIVATE -DSERIAL_CPP_WIN_ASYNC)
    endif()

    target_link_libraries(serial_cpp_async_test Threads::Threads)
endif()
find_package(Threads REQUIRED)
target_link_libraries(serial_cpp_async_test Threads::Threads)

## Install library
if(serial_cpp_INSTALL)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    ## Install headers
    install(FILES include/serial_cpp/serial.h include/serial_cpp/v8stdint.h include/serial_cpp/serial_compat.h
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME}")
    
    ## Generate and install CMake config files
    include(CMakePackageConfigHelpers)
    
    set(SERIAL_CPP_CMAKE_FILES_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}")
    
    install(EXPORT ${PROJECT_NAME}Targets
            FILE ${PROJECT_NAME}Targets.cmake
            NAMESPACE ${PROJECT_NAME}::
            DESTINATION "${SERIAL_CPP_CMAKE_FILES_INSTALL_DIR}"
    )
    
    configure_package_config_file(${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
      "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
      INSTALL_DESTINATION "${SERIAL_CPP_CMAKE_FILES_INSTALL_DIR}"
    )
    
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        VERSION "${serial_cpp_VERSION}"
        COMPATIBILITY AnyNewerVersion
    )
    
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
                  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION "${SERIAL_CPP_CMAKE_FILES_INSTALL_DIR}")
    
    
    ## Install package.xml
    install(FILES package.xml
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}")
endif()

## Tests
include(CTest)
# Tests are enabled if BUILD_TESTING is enabled and PROJECT_IS_TOP_LEVEL is ON,
# or if serial_cpp_FORCE_RESPECT_BUILD_TESTING is ON, so if you include the project
# via FetchContent or add_subdirectory and if you want to compile tests, you
# need to set both BUILD_TESTING and serial_cpp_FORCE_RESPECT_BUILD_TESTING to ON
if(BUILD_TESTING AND (PROJECT_IS_TOP_LEVEL OR serial_cpp_FORCE_RESPECT_BUILD_TESTING))
    add_subdirectory(tests)
endif()
