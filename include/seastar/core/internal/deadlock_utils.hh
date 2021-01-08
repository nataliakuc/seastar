/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2020 ScyllaDB
 */

#include <list>
#include <map>
#include <variant>
namespace seastar {

#ifdef SEASTAR_DEADLOCK_DETECTION
class task;
namespace internal {

class future_base;
class promise_base;
seastar::task* previous_task(seastar::task* task);

using traced_ptr = std::variant<task*, future_base*, promise_base*>;

void trace_runtime_edge(traced_ptr pre, traced_ptr post, bool speculative = false);
void trace_runtime_vertex_constructor(traced_ptr v);
void trace_runtime_vertex_deconstructor(traced_ptr v);
}
#else

namespace internal {
constexpr void trace_runtime_edge(void*, void*, bool = false) {}
constexpr void trace_runtime_vertex_constructor(void*) {}
constexpr void trace_runtime_vertex_deconstructor(void*) {}
}
#endif
}
