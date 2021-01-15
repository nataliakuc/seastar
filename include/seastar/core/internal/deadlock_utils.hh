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
    const void* _ptr;

    traced_ptr() : _type(VertexType::NONE), _ptr(nullptr) {}
    traced_ptr(const task* ptr) : _type(VertexType::TASK), _ptr(ptr) {}
    traced_ptr(const promise_base* ptr) : _type(VertexType::PROMISE), _ptr(ptr) {}
    traced_ptr(const future_base* ptr) : _type(VertexType::FUTURE), _ptr(ptr) {}

    uintptr_t get_ptr() const;

    friend bool operator==(const traced_ptr& lhs, const traced_ptr& rhs);
};

seastar::task* previous_task(seastar::task* task);

traced_ptr get_current_traced_ptr();

class update_current_traced_vertex {
    traced_ptr _previous_ptr;
    traced_ptr _new_ptr;

public:
    update_current_traced_vertex(traced_ptr new_ptr);

    ~update_current_traced_vertex();
};

void trace_runtime_edge(traced_ptr pre, traced_ptr post, bool speculative);
inline void trace_runtime_edge(traced_ptr pre, traced_ptr post) {
    trace_runtime_edge(pre, post, false);
}
void trace_runtime_vertex_constructor(traced_ptr v);
void trace_runtime_vertex_destructor(traced_ptr v);

void trace_runtime_semaphore_constructor(void const* sem, size_t count);
template<typename T1, typename T2>
void trace_runtime_semaphore_constructor(basic_semaphore<T1, T2> const* sem) {
    trace_runtime_semaphore_constructor(reinterpret_cast<void const*>(sem), sem->available_units());
}

void trace_runtime_semaphore_destructor(void const* sem, size_t count);
template<typename T1, typename T2>
void trace_runtime_semaphore_destructor(basic_semaphore<T1, T2> const* sem) {
    trace_runtime_semaphore_destructor(reinterpret_cast<void const*>(sem), sem->available_units());
}

void trace_runtime_semaphore_signal_caller(void const* sem, size_t count, traced_ptr caller);
template<typename T1, typename T2>
void trace_runtime_semaphore_signal_caller(basic_semaphore<T1, T2> const* sem, size_t count, traced_ptr caller) {
    trace_runtime_semaphore_signal_caller(reinterpret_cast<void const*>(sem), count, caller);
}

void trace_runtime_semaphore_signal_schedule(void const* sem, traced_ptr unlocked);
template<typename T1, typename T2>
void trace_runtime_semaphore_signal_schedule(basic_semaphore<T1, T2> const* sem, traced_ptr unlocked) {
    trace_runtime_semaphore_signal_schedule(reinterpret_cast<void const*>(sem), unlocked);
}

void trace_runtime_semaphore_wait(void const* sem, traced_ptr caller);

template<typename T1, typename T2>
void trace_runtime_semaphore_wait(basic_semaphore<T1, T2> const* sem, traced_ptr caller) {
    trace_runtime_semaphore_wait(reinterpret_cast<void const*>(sem), caller);
}

}
#else
namespace internal {
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
