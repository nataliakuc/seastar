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
#include <stdint.h>
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

template<typename T>
VertexType get_type(T* ptr);

template<>
VertexType get_type(task* ptr) {
    return TASK;
}
template<>
VertexType get_type(promise_base* ptr) {
    return PROMISE;
}
template<>
VertexType get_type(future_base* ptr) {
    return FUTURE;
}

using EncodedVertex = std::map<const char*, size_t>;

template<typename T>
EncodedVertex serialize_vertex(T* ptr) {
    return {{"value", reinterpret_cast<uintptr_t>(ptr)}, {"type", get_type(ptr)}};
}

void trace_runtime_edge(EncodedVertex&& pre, EncodedVertex&& post, bool speculative);
void trace_runtime_vertex_constructor(EncodedVertex&& v);
void trace_runtime_vertex_destructor(EncodedVertex&& v);
}
#else
namespace internal {
constexpr inline void trace_runtime_edge(void*, void*, bool = false) {}
constexpr inline void trace_runtime_vertex_constructor(void*) {}
constexpr inline void trace_runtime_vertex_destructor(void*) {}
}
#endif
}
