# Copyright Tomas Zeman 2019.
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE_1_0.txt or copy at
# http://www.boost.org/LICENSE_1_0.txt)

function(target_clangformat_setup)
  if(NOT CLANGFORMAT_EXECUTABLE)
    set(CLANGFORMAT_EXECUTABLE clang-format)
  endif()

  if(NOT EXISTS ${CLANGFORMAT_EXECUTABLE})
    find_program(clangformat_executable_tmp ${CLANGFORMAT_EXECUTABLE})
    if(clangformat_executable_tmp)
      set(CLANGFORMAT_EXECUTABLE ${clangformat_executable_tmp})
      unset(clangformat_executable_tmp)
    else()
      message(FATAL_ERROR "ClangFormat: ${CLANGFORMAT_EXECUTABLE} not found! Aborting")
    endif()
  endif()

  get_target_property(target_sources ${ARGV} SOURCES)

  foreach(clangformat_source ${target_sources})
    get_filename_component(clangformat_source ${clangformat_source} ABSOLUTE)
    list(APPEND clangformat_sources ${clangformat_source})
  endforeach()

  add_custom_target(${ARGV}_clangformat
    COMMAND
      ${CLANGFORMAT_EXECUTABLE}
      -style=file
      -i
      ${clangformat_sources}
    WORKING_DIRECTORY
      ${CMAKE_SOURCE_DIR}
    COMMENT
      "Formating with ${CLANGFORMAT_EXECUTABLE} ..."
  )

  if(TARGET format)
    add_dependencies(format ${ARGV}_clangformat)
  else()
    add_custom_target(format DEPENDS ${ARGV}_clangformat)
  endif()
endfunction()