project (pushcli
        VERSION ${CPP_PUSH_VERSION}
        DESCRIPTION "cli program for Google/Apple Push APIs")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Boost 1.80 REQUIRED COMPONENTS
  filesystem
  program_options
  )


add_executable(${PROJECT_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    CppPush
    Boost::filesystem
    Boost::program_options
    Boost::system
)
