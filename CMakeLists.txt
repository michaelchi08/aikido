# CMake 2.8.12 or above is required for CMakeParseArguments.
# CMake 2.8.11 or above is required for target_include_directories
cmake_minimum_required(VERSION 2.8.12)

# Use MACOSX_RPATH by default on OS X. This was added in CMake 2.8.12 and
# became default in CMake 3.0. Explicitly setting this policy is necessary to
# suppress a warning in CMake 3.0 and above.
if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW)
endif()

project(aikido)

list(INSERT CMAKE_MODULE_PATH 0 "${PROJECT_SOURCE_DIR}/cmake") 

#==============================================================================
# Configuration Options
#

set(INCLUDE_INSTALL_DIR "include")
set(LIBRARY_INSTALL_DIR "lib")
set(CONFIG_INSTALL_DIR "${LIBRARY_INSTALL_DIR}/${PROJECT_NAME}/cmake")

option(COVERALLS "Enable coveralls support" OFF)
option(COVERALLS_UPLOAD "Upload coveralls JSON output" ON)
option(DOWNLOAD_TAGFILES "Download Doxygen tagfiles for dependencies" OFF)

# Enable coveralls. This must appear before any targets are created.
if(COVERALLS)
  include(Coveralls)
  coveralls_turn_on_coverage()
endif()

#==============================================================================
# Helper functions.
#
function(aikido_space_delimit output_variable input_list)
  set(accumulator)
  foreach(input_element ${input_list})
    set(accumulator "${accumulator} ${input_element}")
  endforeach()

  set("${output_variable}" "${accumulator}")
endfunction()

macro(aikido_check_package variable component dependency)
  if(${${variable}_FOUND})
    message(STATUS "Looking for ${dependency} - version ${${variable}_VERSION}"
                   " found")
  else()
    message(STATUS "Looking for ${dependency} - NOT found, to use"
                   " ${component}, please install ${dependency}")
    return()
  endif()
endmacro()

#==============================================================================
# Register an Aikido include directory.
#
set_property(GLOBAL PROPERTY AIKIDO_INCLUDE_DIRS)

function(aikido_include_directories)
  include_directories(SYSTEM ${ARGN})
  set_property(GLOBAL APPEND PROPERTY AIKIDO_INCLUDE_DIRS ${ARGN})
endfunction()

#==============================================================================
# Register an Aikido library.
#
set_property(GLOBAL PROPERTY AIKIDO_LIBRARIES)
set_property(GLOBAL PROPERTY AIKIDO_INTERNAL_SOURCES)
set_property(GLOBAL PROPERTY AIKIDO_EXTERNAL_SOURCES)

function(aikido_add_library target_name)
  cmake_parse_arguments(AIKIDO_ADD_LIBRARY "EXTERNAL" "" "" ${ARGN})
  set(is_external ${AIKIDO_ADD_LIBRARY_EXTERNAL})
  set(rel_sources ${AIKIDO_ADD_LIBRARY_UNPARSED_ARGUMENTS})

  add_library("${target_name}" SHARED ${rel_sources})
  set_property(GLOBAL APPEND PROPERTY AIKIDO_LIBRARIES "${target_name}")

  install(TARGETS "${target_name}"
    EXPORT "${PROJECT_NAME}Targets"
    LIBRARY DESTINATION "${LIBRARY_INSTALL_DIR}"
  )

  # Compile a list of all source files to pass to coveralls. This function may
  # be called from a subdirctory, so we first convert to absolute paths.
  set(abs_sources)
  foreach(rel_source ${rel_sources})
    get_filename_component(abs_source
      "${CMAKE_CURRENT_SOURCE_DIR}/${rel_source}" ABSOLUTE)
    list(APPEND abs_sources "${abs_source}")
  endforeach()

  if(${is_external})
    set_property(GLOBAL APPEND PROPERTY AIKIDO_EXTERNAL_SOURCES ${abs_sources})
  else()
    set_property(GLOBAL APPEND PROPERTY AIKIDO_INTERNAL_SOURCES ${abs_sources})
  endif()
endfunction()


#==============================================================================
# Register an Aikido test.
#
set_property(GLOBAL PROPERTY AIKIDO_TESTS)

function(aikido_add_test target_name)
  add_executable("${target_name}" ${ARGN})
  add_test("${target_name}" "${target_name}")

  target_link_libraries("${target_name}" gtest gtest_main)

  set_property(GLOBAL APPEND PROPERTY AIKIDO_TESTS "${target_name}")
endfunction()

#==============================================================================
# Required Dependencies
#
set(AIKIDO_INCLUDE_DIRS)

include(CMakePackageConfigHelpers)
include(CMakeParseArguments)
include(FindPkgConfig)

find_package(Boost COMPONENTS filesystem)
aikido_include_directories(${Boost_INCLUDE_DIRS})

# TODO: utils-urdf should only be required for aikido::perception.
find_package(DART REQUIRED COMPONENTS optimizer-nlopt utils-urdf)
aikido_include_directories(${DART_INCLUDE_DIRS})

find_package(TinyXML2 REQUIRED)
aikido_include_directories(${TinyXML2_INCLUDE_DIRS})

#==============================================================================
# Building libraries and unit tests.
#
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories("include" "src")

add_subdirectory("src")

enable_testing()
add_subdirectory("tests" EXCLUDE_FROM_ALL)

# Targets to mimic a Catkin package. "tests" builds tests and "test" (or
# "run_tests") runs them.
get_property(all_tests GLOBAL PROPERTY AIKIDO_TESTS)
add_custom_target(tests DEPENDS ${all_tests})
add_custom_target(run_tests COMMAND "${CMAKE_CTEST_COMMAND}")

#==============================================================================
# Doxygen.
#
find_package(Doxygen QUIET)

if(${DOXYGEN_FOUND})
  message(STATUS "Looking for Doxygen - version ${DOXYGEN_VERSION} found")

  set(DOXYGEN_INPUT_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/include")
  set(DOXYGEN_OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/doxygen")
  set(DOXYGEN_GENERATE_TAGFILE "${DOXYGEN_OUTPUT_ROOT}/aikido.tag")
  set(DOXYGEN_HTML_INDEX "${DOXYGEN_OUTPUT_ROOT}/html/index.html")
  set(DOXYGEN_WORKING_DIR "${CMAKE_CURRENT_BINARY_DIR}/doxygen_working")
  set(DOXYGEN_EXCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/src/external")

  # Strip include_directories() being used by CMake from #include paths.
  get_property(DOXYGEN_STRIP_FROM_INC_PATH_LIST
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    PROPERTY INCLUDE_DIRECTORIES)
  aikido_space_delimit(DOXYGEN_STRIP_FROM_INC_PATH
    "${DOXYGEN_STRIP_FROM_INC_PATH_LIST}")

  # Optionally download Doxygen tagfiles for dependencies.
  set(DOXYGEN_TAGFILES)
  if(DOWNLOAD_TAGFILES)
    file(DOWNLOAD "http://dartsim.github.io/dart/dart.tag"
        "${DOXYGEN_WORKING_DIR}/dart.tag")
    set(DOXYGEN_TAGFILES
      "${DOXYGEN_TAGFILES} dart.tag=http://dartsim.github.io/dart/")

    if(${OMPL_FOUND})
      file(DOWNLOAD "http://ompl.kavrakilab.org/core/ompl.tag"
        "${DOXYGEN_WORKING_DIR}/ompl.tag")
      set(DOXYGEN_TAGFILES
        "${DOXYGEN_TAGFILES} ompl.tag=http://ompl.kavrakilab.org/")
    endif()
  endif()

  # Generate a Doxyfile.
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/Doxyfile.in"
    "${DOXYGEN_WORKING_DIR}/Doxyfile" @ONLY)

  # Create the "docs" target that runs Doxygen.
  add_custom_command(OUTPUT "${DOXYGEN_HTML_INDEX}"
    COMMAND "${DOXYGEN_EXECUTABLE}"
    DEPENDS "${DOXYGEN_WORKING_DIR}/Doxyfile"
    WORKING_DIRECTORY "${DOXYGEN_WORKING_DIR}")

  add_custom_target(docs
    DEPENDS "${DOXYGEN_HTML_INDEX}"
    COMMENT "Generating documentation.")
else()
  message(STATUS "Looking for Doxygen - NOT found, to generate API"
                 " documentation, please install Doxygen")
endif()

#==============================================================================
# Coveralls.
#
if(COVERALLS)
  get_property(internal_sources GLOBAL PROPERTY AIKIDO_INTERNAL_SOURCES)
  get_property(external_sources GLOBAL PROPERTY AIKIDO_EXTERNAL_SOURCES)

  list(LENGTH internal_sources internal_sources_length)
  list(LENGTH external_sources external_sources_length)

  message(STATUS "Computing test coverage for ${internal_sources_length} files"
                 " (skipped ${external_sources_length} external files).")

  coveralls_setup("${internal_sources}"
    ${COVERALLS_UPLOAD}
    "${PROJECT_SOURCE_DIR}/cmake")
endif()

#==============================================================================
# Installation.
#
install(DIRECTORY "include/"
  DESTINATION "${INCLUDE_INSTALL_DIR}")

# Install the package.xml file (to satisfy REP-136).
install(FILES "package.xml"
  DESTINATION "share/${PROJECT_NAME}")

# Generate and install a Targets.cmake file that contains EXPORTED targets, to
# be included() in the Config.cmake file. The target set "aikidoTargets" is
# generated by the install(TARGETS) directive above.
install(EXPORT "${PROJECT_NAME}Targets"
  FILE "${PROJECT_NAME}Targets.cmake"
  DESTINATION "${CONFIG_INSTALL_DIR}")

# Generate and install a Config.cmake file.
get_property(AIKIDO_LIBRARIES GLOBAL PROPERTY AIKIDO_LIBRARIES)
get_property(AIKIDO_INCLUDE_DIRS GLOBAL PROPERTY AIKIDO_INCLUDE_DIRS)

configure_package_config_file("cmake/${PROJECT_NAME}Config.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  INSTALL_DESTINATION "${CONFIG_INSTALL_DIR}"
  PATH_VARS INCLUDE_INSTALL_DIR)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
  DESTINATION "${CONFIG_INSTALL_DIR}")