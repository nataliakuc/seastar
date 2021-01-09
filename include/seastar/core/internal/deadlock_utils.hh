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

#pragma once

#include <list>
#include <map>
#include <variant>
#include <assert.h>

namespace seastar {

#ifdef SEASTAR_DEADLOCK_DETECTION
class task;
namespace internal {

enum VertexType : int {
    NONE = 0,
    TASK = 1,
    PROMISE = 2,
    FUTURE = 3
};

class future_base;
class promise_base;
struct traced_ptr {
    VertexType _type;
    void* _ptr;

    traced_ptr() : _type(VertexType::NONE), _ptr(nullptr) {}
    traced_ptr(task* ptr) : _type(VertexType::TASK), _ptr(ptr) {}
    traced_ptr(future_base* ptr) : _type(VertexType::FUTURE), _ptr(ptr) {}
    traced_ptr(promise_base* ptr) : _type(VertexType::PROMISE), _ptr(ptr) {}

    friend bool operator==(const traced_ptr &lhs, const traced_ptr &rhs);
};

inline bool operator==(const traced_ptr &lhs, const traced_ptr &rhs) {
    return lhs._type == rhs._type && lhs._ptr == rhs._ptr;
}

seastar::task* previous_task(seastar::task* task);

traced_ptr& current_traced_ptr();

inline traced_ptr get_current_traced_ptr() {
    return current_traced_ptr();
}

struct update_current_traced_vertex {
    traced_ptr _previous_ptr;
    traced_ptr _new_ptr;
    update_current_traced_vertex(traced_ptr new_ptr) : _previous_ptr(current_traced_ptr()), _new_ptr(new_ptr) {
        current_traced_ptr() = new_ptr;
    }

    ~update_current_traced_vertex() {
        assert(current_traced_ptr() == _new_ptr);
        current_traced_ptr() = _previous_ptr;
    }
};

void trace_runtime_edge(traced_ptr pre, traced_ptr post, bool speculative);
inline void trace_runtime_edge(traced_ptr pre, traced_ptr post) {
    trace_runtime_edge(pre, post, false);
}
void trace_runtime_vertex_constructor(traced_ptr v);
void trace_runtime_vertex_destructor(traced_ptr v);
}
#else
namespace internal {
constexpr inline void trace_runtime_edge(void*, void*, bool = false) {}
constexpr inline void trace_runtime_vertex_constructor(void*) {}
constexpr inline void trace_runtime_vertex_destructor(void*) {}

inline constexpr nullptr_t get_current_traced_ptr() {
    return nullptr;
}

struct update_current_traced_vertex {
    constexpr update_current_traced_vertex(void*) noexcept {}
};
}
#endif
}
