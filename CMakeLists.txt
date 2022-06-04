cmake_minimum_required(VERSION 3.10)

project(AGATE C)

option(AGATE_DEV "Developper mode" ON)

set(CMAKE_C_FLAGS "-Wall -pedantic -std=c99 -O1 -g")

include(GNUInstallDirs)

add_library(agate0
  agate.c
)

target_link_libraries(agate0
  PUBLIC
    m
)

add_executable(agate-cli
  agate-cli.c
)

target_link_libraries(agate-cli
  PRIVATE
    agate0
)

if(AGATE_DEV)
  find_package(BISON REQUIRED)
  find_package(FLEX REQUIRED)

  bison_target(agate-parser
    "agate.y"
    "${CMAKE_CURRENT_BINARY_DIR}/agate-parser.c"
    COMPILE_FLAGS "-v -Wcounterexamples"
  )

  flex_target(agate-lexer
    "agate.l"
    "${CMAKE_CURRENT_BINARY_DIR}/agate-lexer.c"
    DEFINES_FILE "${CMAKE_CURRENT_BINARY_DIR}/agate-lexer.h"
  )

  add_flex_bison_dependency(agate-lexer agate-parser)

  add_executable(agate-syntax
    syntax.c
    ${BISON_agate-parser_OUTPUTS}
    ${FLEX_agate-lexer_OUTPUTS}
  )

  target_include_directories(agate-syntax
    PRIVATE
      "${CMAKE_CURRENT_BINARY_DIR}"
  )


  if(UNIX)
    add_executable(agate-tests
      tests.c
      ${BISON_agate-parser_OUTPUTS}
      ${FLEX_agate-lexer_OUTPUTS}
    )

    target_compile_definitions(agate-tests
      PRIVATE
        _POSIX_C_SOURCE=200809L
    )

    target_include_directories(agate-tests
      PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
    )

    target_link_libraries(agate-tests
      PRIVATE
        agate0
    )
  endif()

endif()
