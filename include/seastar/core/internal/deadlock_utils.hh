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

enum VertexType : int {
    TASK     = 0,
    PROMISE  = 1,
    FUTURE   = 2
};

class future_base;
class promise_base;
seastar::task* previous_task(seastar::task* task);

template<typename T1, typename T2>
void trace_runtime_edge(T1* pre, T2* post, bool speculative);
template<typename T>
void trace_runtime_vertex_constructor(T* v);
template<typename T>
void trace_runtime_vertex_destructor(T* v);
}
#else
namespace internal {
constexpr inline void trace_runtime_edge(void*, void*, bool = false) {}
constexpr inline void trace_runtime_vertex_constructor(void*) {}
constexpr inline void trace_runtime_vertex_destructor(void*) {}
}
#endif
}
