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
    Boost::filesystem
    Boost::system
    Boost::json
    Boost::url
)
