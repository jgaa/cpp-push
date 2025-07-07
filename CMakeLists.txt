cmake_minimum_required(VERSION 3.21)
project(cpp-push VERSION 0.1.0 LANGUAGES CXX)

set(CPP_PUSH_VERSION "${PROJECT_VERSION}")
add_definitions(-DCPP_PUSH_VERSION="${CPP_PUSH_VERSION}")
add_definitions(-DRESTINCURL_ENABLE_ASIO=1 -DRESTINCURL_ENABLE_ASYNC=1)
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
  url
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

