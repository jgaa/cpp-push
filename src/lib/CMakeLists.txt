project (CppPush
        VERSION ${CPP_PUSH_VERSION}
        DESCRIPTION "C++ client library for Google/Apple Push APIs")

add_library(
    ${PROJECT_NAME}
    STATIC
    ${CPP_PUSH_ROOT}/include/cpp-push/GooglePusher.h
    ${CPP_PUSH_ROOT}/include/cpp-push/Pusher.h
    ${CPP_PUSH_ROOT}/include/cpp-push/cpp-push.h
    ${CPP_PUSH_ROOT}/include/cpp-push/logging.h
    GooglePusher.cpp
    Pusher.cpp
)

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
    $<BUILD_INTERFACE:${CPP_PUSH_ROOT}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    RESTinCurl::RESTinCurl
    logfault
    jwt-cpp::jwt-cpp
    Boost::system
    Boost::json
    Boost::url
)

# Create an alias target for modern CMake usage
add_library(CppPush::CppPush ALIAS ${PROJECT_NAME})

# Only add install configuration if this is the main project
# and not included as a subproject
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Export the library for installation
    install(TARGETS ${PROJECT_NAME}
        EXPORT CppPushTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
    )

    # Install header files
    install(DIRECTORY ${CPP_PUSH_ROOT}/include/
        DESTINATION include
        FILES_MATCHING PATTERN "*.h"
    )

    # Install the targets file
    install(EXPORT CppPushTargets
        FILE CppPushTargets.cmake
        NAMESPACE CppPush::
        DESTINATION lib/cmake/CppPush
    )

    # Generate package configuration files
    include(CMakePackageConfigHelpers)

    # Create CppPushConfig.cmake
    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/CppPushConfig.cmake
        INSTALL_DESTINATION lib/cmake/CppPush
    )

    # Create CppPushConfigVersion.cmake
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/CppPushConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    # Install package configuration files
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/CppPushConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/CppPushConfigVersion.cmake
        DESTINATION lib/cmake/CppPush
    )
endif()
