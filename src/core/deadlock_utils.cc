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

seastar::task* previous_task(seastar::task *task) {
    seastar::task* result = nullptr;
    for (const auto &t: task_list()) {
        if (t->waiting_task() == task) {
            if (result) {
                // Note about strange behaviour.
            }
            result = t;
        }
    }
    return result;
}

static std::ostream& get_output_stream() {
    static thread_local std::ofstream stream(fmt::format("graphdump.{}.json", this_shard_id()));
    return stream;
}

template<typename T>
static void write_data(T data) {
    std::string serialized = json::formatter::to_json(data);
    get_output_stream() << serialized << std::endl;
}

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

template<typename T>
std::map<const char*, size_t> serialize_vertex(T* ptr) {
    return {{"value", reinterpret_cast<uintptr_t>(ptr)}, {"type", get_type(ptr)}};
}

template<typename T1, typename T2>
void trace_runtime_edge(T1* pre, T2* post, bool speculative) {
    std::map<const char*, size_t> edge {
        {"pre", serialize_vertex(pre)},
        {"post", serialize_vertex(post)},
    };
    const char* name = speculative ? "edge" : "edge_speculative";
    std::map<const char*, decltype(edge)> data {{name, edge}};
    write_data(data);
}

template<typename T>
void trace_runtime_vertex_constructor(T* v) {
    std::map<const char*, size_t> data {{"constructor", serialize_vertex(v)}};
    write_data(data);
}

template<typename T>
void trace_runtime_vertex_destructor(T* v) {
    std::map<const char*, size_t> data {{"destructor", serialize_vertex(v)}};
    write_data(data);
}
}
#endif
