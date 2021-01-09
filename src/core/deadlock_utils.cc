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

#ifdef SEASTAR_DEADLOCK_DETECTION
#include <seastar/core/internal/deadlock_utils.hh>
#include <seastar/core/task.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/json/formatter.hh>
#include <list>
#include <map>
#include <fstream>
#include <string>

namespace seastar::internal {

static std::ostream& get_output_stream() {
    static thread_local std::ofstream stream(fmt::format("graphdump.{}.json", this_shard_id()));
    return stream;
}

static traced_ptr& current_traced_ptr() {
    static thread_local traced_ptr ptr;
    return ptr;
}

template<typename T>
static void write_data(T data) {
    std::string serialized = json::formatter::to_json(data);
    get_output_stream() << serialized << std::endl;
}

static std::map<const char*, size_t> serialize_vertex(traced_ptr v) {
    return {{"value", reinterpret_cast<uintptr_t>(v._ptr)},
            {"type",  v._type}};
}

seastar::task* previous_task(seastar::task* task) {
    seastar::task* result = nullptr;
    for (const auto& t: task_list()) {
        if (t->waiting_task() == task) {
            if (result) {
                // Note about strange behaviour.
            }
            result = t;
        }
    }
    return result;
}


traced_ptr get_current_traced_ptr() {
    return current_traced_ptr();
}

update_current_traced_vertex::update_current_traced_vertex(traced_ptr new_ptr) : _previous_ptr(current_traced_ptr()), _new_ptr(new_ptr) {
    current_traced_ptr() = new_ptr;
}

update_current_traced_vertex::~update_current_traced_vertex() {
    assert(current_traced_ptr() == _new_ptr);
    current_traced_ptr() = _previous_ptr;
}

void trace_runtime_edge(traced_ptr pre, traced_ptr post, bool speculative) {
    std::map<const char*, std::map<const char*, size_t> > edge {
        {"pre", serialize_vertex(pre)},
        {"post", serialize_vertex(post)},
    };
    const char* name = speculative ? "edge" : "edge_speculative";
    std::map<const char*, decltype(edge)> data {{name, edge}};
    write_data(data);
}

void trace_runtime_vertex_constructor(traced_ptr v) {
    std::map<const char*, std::map<const char*, size_t> > data {{"constructor", serialize_vertex(v)}};
    write_data(data);
}

void trace_runtime_vertex_destructor(traced_ptr v) {
    std::map<const char*, std::map<const char*, size_t> > data {{"destructor", serialize_vertex(v)}};
    write_data(data);
}
}
#endif
