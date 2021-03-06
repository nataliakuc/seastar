#
# This file is open source software, licensed to you under the terms
# of the Apache License, Version 2.0 (the "License").  See the NOTICE file
# distributed with this work for additional information regarding copyright
# ownership.  You may not use this file except in compliance with the License.
#
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#

#
# Copyright (C) 2021 Scylladb, Ltd.
#

# Logical target for all perf tests.
add_custom_target (deadlock_tests)

macro (seastar_add_test name)
  set (args ${ARGN})

  cmake_parse_arguments (
    parsed_args
    ""
    ""
    "SOURCES"
    ${args})

  set (target test_deadlock_${name})
  add_executable (${target} ${parsed_args_SOURCES})

  set (libraries seastar)

  target_link_libraries (${target}
    PRIVATE ${libraries})

  target_include_directories (${target}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}
      ${Seastar_SOURCE_DIR}/src)

  set_target_properties (${target}
    PROPERTIES
      OUTPUT_NAME ${name}_deadlock)

  add_dependencies (deadlock_tests ${target})
  set (${name}_test ${target})
endmacro ()

seastar_add_test (multiple_fibers_for_each
  SOURCES multiple_fibers_for_each.cc)

