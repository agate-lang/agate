cmake_minimum_required(VERSION 3.18)

project(AGATE
  VERSION 0.1.0
  LANGUAGES C
  HOMEPAGE_URL "https://agate-lang.github.io/"
)

option(AGATE_DEV "Developper mode" ON)

if(UNIX)
  set(CMAKE_C_FLAGS "-Wall -pedantic")
endif()

include(GNUInstallDirs)

add_library(agate0
  agate.c
)

target_link_libraries(agate0
  PUBLIC
    m
)

target_compile_features(agate0
  PUBLIC
    c_std_99
)

if(MSVC)
  target_compile_options(agate0 PRIVATE /W3)
else()
  target_compile_options(agate0 PRIVATE -Wall -Wpedantic)
endif()

set_target_properties(agate0
  PROPERTIES
    C_STANDARD 99
    C_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# support

add_library(agate-support0
  agate-support.c
)

target_link_libraries(agate-support0
  PUBLIC
    agate0
)

target_include_directories(agate-support0
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  target_compile_options(agate-support0 PRIVATE /W3)
else()
  target_compile_options(agate-support0 PRIVATE -Wall -Wpedantic)
endif()

# run

add_executable(agate-run
  agate-run.c
)

target_link_libraries(agate-run
  PRIVATE
    agate0
)

if(MSVC)
  target_compile_options(agate-run PRIVATE /W3)
else()
  target_compile_options(agate-run PRIVATE -Wall -Wpedantic)
endif()

if(UNIX AND AGATE_DEV)
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
      tests/api/api_tests.c
      # tests
      tests/api/arrays.c
      tests/api/call.c
      tests/api/call_foreign.c
      tests/api/foreign_class.c
      tests/api/handle.c
      tests/api/maps.c
      tests/api/reentrancy.c
      tests/api/slots.c
      tests/api/tuples.c
    )

    target_compile_definitions(agate-tests
      PRIVATE
        _POSIX_C_SOURCE=200809L
    )

    if(MSVC)
      target_compile_options(agate-tests PRIVATE /W3)
    else()
      target_compile_options(agate-tests PRIVATE -Wall -Wpedantic)
    endif()

    target_include_directories(agate-tests
      PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(agate-tests
      PRIVATE
        agate0
        agate-support0
    )
  endif()
endif()
