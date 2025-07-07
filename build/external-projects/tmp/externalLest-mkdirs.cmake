# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest")
  file(MAKE_DIRECTORY "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest")
endif()
file(MAKE_DIRECTORY
  "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest-build"
  "/home/runner/work/cpp-push/cpp-push/build/external-projects"
  "/home/runner/work/cpp-push/cpp-push/build/external-projects/tmp"
  "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest-stamp"
  "/home/runner/work/cpp-push/cpp-push/build/external-projects/src"
  "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/runner/work/cpp-push/cpp-push/build/external-projects/src/externalLest-stamp${cfgdir}") # cfgdir has leading slash
endif()
