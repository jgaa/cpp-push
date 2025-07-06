cmake_minimum_required(VERSION 3.21)
project(cpp-push VERSION 0.1.0 LANGUAGES CXX)

set(CPP_PUSH_VERSION "${PROJECT_VERSION}")
add_definitions(-DCPP_PUSH_VERSION="${CPP_PUSH_VERSION}")
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(USE_STATIC_BOOST "Link Boost statically" ON)
option(WITH_LOGFAULT "Use logfault library for logging" ON)
set(CPP_PUSH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

if (WITH_LOGFAULT)
  add_definitions(-DWITH_LOGFAULT)
  message(STATUS "Using logfault for logging")
else()
  message(STATUS "Not using logfault for logging")
endif()

if (USE_STATIC_BOOST)
    set(Boost_USE_STATIC_LIBS ON)
endif()

find_package(Threads REQUIRED)

# --------------------------------------------------
# Tell CMake to use only Config-mode Boost (no legacy FindBoost)
# --------------------------------------------------
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)   # Remove old FindBoost module
endif()

find_package(Boost 1.80 REQUIRED COMPONENTS
  filesystem
  system
  json
  )

include(FetchContent)

# ——— Helper function: try find, else fetch ———
function(ensure_dep NAME CONFIG_NAME GIT_REPO GIT_TAG)
  # 1) First try to find an existing config
  find_package(${CONFIG_NAME} QUIET)
  if(NOT TARGET ${CONFIG_NAME}::${CONFIG_NAME})
    message(STATUS "Fetching ${NAME}…")
    FetchContent_Declare(
      ${NAME}
      GIT_REPOSITORY ${GIT_REPO}
      GIT_TAG        ${GIT_TAG}
    )
    FetchContent_MakeAvailable(${NAME})
  else()
    message(STATUS "Using pre-existing ${NAME}")
  endif()
endfunction()

# ——— Dependencies ———

# logfault
ensure_dep(
  logfault
  logfault
  https://github.com/jgaa/logfault.git
  proper-cmake
)

# RESTinCurl
ensure_dep(
  RESTinCurl
  RESTinCurl
  https://github.com/jgaa/RESTinCurl.git
  master
)

# jwt-cpp
ensure_dep(
  jwt-cpp
  jwt-cpp
  https://github.com/Thalhammer/jwt-cpp.git
  master
)

add_subdirectory(src/lib)
add_subdirectory(src/cli)

# # ——— Your library ———
# add_library(cpp-push
#   src/pusher.cpp
#   include/pusher.hpp
# )
# target_link_libraries(cpp-push
#   PUBLIC
#     RESTinCurl::RESTinCurl
#     logfault::logfault
#     jwt-cpp::jwt-cpp
# )
#
# target_include_directories(cpp-push
#   PUBLIC
#     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
#     $<INSTALL_INTERFACE:include>
# )
#
# # ——— Tests ———
# if(BUILD_TESTING)
#   enable_testing()
#   add_executable(test-push tests/test_pusher.cpp)
#   target_link_libraries(test-push PRIVATE cpp-push)
#   add_test(NAME pusher-tests COMMAND test-push)
# endif()
#
# # ——— CLI tool ———
# add_executable(push-cli src/main.cpp)
# target_link_libraries(push-cli PRIVATE cpp-push)
#
# # …and optionally install/export targets as usual…
