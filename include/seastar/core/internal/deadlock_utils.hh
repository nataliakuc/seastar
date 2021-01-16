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
#include <stdint.h>

namespace seastar {

#ifdef SEASTAR_DEADLOCK_DETECTION

class task;
template<typename, typename>
class basic_semaphore;

namespace internal {

/// Specifies type of runtime vertex.
enum runtime_vertex_type : int {
    NONE = 0,
    TASK = 1,
    PROMISE = 2,
    FUTURE = 3
};

class future_base;
class promise_base;

/// Represents runtime vertex (e.g. task, promise, future),
/// in a way that doesn't use template and allows to remove cyclical dependencies.
struct runtime_vertex {
    runtime_vertex_type _type;
    const void* _ptr;

    runtime_vertex() : _type(runtime_vertex_type::NONE), _ptr(nullptr) {}
    runtime_vertex(const task* ptr) : _type(runtime_vertex_type::TASK), _ptr(ptr) {}
    runtime_vertex(const promise_base* ptr) : _type(runtime_vertex_type::PROMISE), _ptr(ptr) {}
    runtime_vertex(const future_base* ptr) : _type(runtime_vertex_type::FUTURE), _ptr(ptr) {}

    uintptr_t get_ptr() const;

    friend bool operator==(const runtime_vertex& lhs, const runtime_vertex& rhs);
};

/// Get current vertex that is being executed.
runtime_vertex get_current_traced_ptr();

/// Updated current traced vertex using RAII.
class update_current_traced_vertex {
    runtime_vertex _previous_ptr;
    runtime_vertex _new_ptr;

public:
    update_current_traced_vertex(runtime_vertex new_ptr);

    ~update_current_traced_vertex();
};

/// Traces causal edge between two vertices.
/// e.g from task to promise that it completes.
void trace_runtime_edge(runtime_vertex pre, runtime_vertex post, bool speculative);
inline void trace_runtime_edge(runtime_vertex pre, runtime_vertex post) {
    trace_runtime_edge(pre, post, false);
}

/// Traces creation of runtime vertex (or reinitialization).
void trace_runtime_vertex_constructor(runtime_vertex v);
/// Traced destruction of runtime vertex (or deinitialization).
void trace_runtime_vertex_destructor(runtime_vertex v);

/// Traces construction of semaphore.
void trace_runtime_semaphore_constructor(void const* sem, size_t count);
template<typename T1, typename T2>
void inline trace_runtime_semaphore_constructor(basic_semaphore<T1, T2> const* sem) {
    trace_runtime_semaphore_constructor(static_cast<void const*>(sem), sem->available_units());
}

/// Traces destruction of semaphore.
void trace_runtime_semaphore_destructor(void const* sem, size_t count);
template<typename T1, typename T2>
void inline trace_runtime_semaphore_destructor(basic_semaphore<T1, T2> const* sem) {
    trace_runtime_semaphore_destructor(static_cast<void const*>(sem), sem->available_units());
}

/// Traces calling of signal on semaphore by certain runtime vertex.
void trace_runtime_semaphore_signal_caller(void const* sem, size_t count, runtime_vertex caller);
template<typename T1, typename T2>
void inline trace_runtime_semaphore_signal_caller(basic_semaphore<T1, T2> const* sem, size_t count, runtime_vertex caller) {
    trace_runtime_semaphore_signal_caller(static_cast<void const*>(sem), count, caller);
}

/// Traces scheduling of runtime vertex after signal was called on semaphore.
void trace_runtime_semaphore_signal_schedule(void const* sem, runtime_vertex unlocked);
template<typename T1, typename T2>
void inline trace_runtime_semaphore_signal_schedule(basic_semaphore<T1, T2> const* sem, runtime_vertex unlocked) {
    trace_runtime_semaphore_signal_schedule(static_cast<void const*>(sem), unlocked);
}

/// Traces successful wait on a semaphore with the vertex that is result of wait.
void trace_runtime_semaphore_wait(void const* sem, runtime_vertex caller);
template<typename T1, typename T2>
void inline trace_runtime_semaphore_wait(basic_semaphore<T1, T2> const* sem, runtime_vertex caller) {
    trace_runtime_semaphore_wait(static_cast<void const*>(sem), caller);
}

}

#else

namespace internal {
// Mutliple empty functions to be optimized out when deadlock detection is disabled.
constexpr void trace_runtime_edge(void*, void*, bool = false) {}
constexpr void trace_runtime_vertex_constructor(void*) {}
constexpr void trace_runtime_vertex_destructor(void*) {}
constexpr void trace_runtime_semaphore_constructor(void const*) {}
constexpr void trace_runtime_semaphore_destructor(void const*) {}
constexpr void trace_runtime_semaphore_signal_caller(void*, size_t, void*) {}
constexpr void trace_runtime_semaphore_signal_schedule(void*, void*) {}
constexpr void trace_runtime_semaphore_wait(void*, void*) {}

constexpr nullptr_t get_current_traced_ptr() {
    return nullptr;
}

struct update_current_traced_vertex {
    constexpr update_current_traced_vertex(void*) noexcept {}
};
}

#endif
}
