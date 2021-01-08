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

static uintptr_t traced_ptr_to_voidptr(traced_ptr ptr) {
    void* ret = {};
    if (auto ppval = std::get_if<task*>(&ptr)) {
        ret = *ppval;
    } else if (auto ppval = std::get_if<future_base*>(&ptr)) {
        ret = *ppval;
    } else if (auto ppval = std::get_if<promise_base*>(&ptr)) {
        ret = *ppval;
    }
    return static_cast<size_t>(reinterpret_cast<uintptr_t>(ret));
}

void trace_runtime_edge(traced_ptr pre, traced_ptr post, bool speculative) {
    std::map<std::string, size_t> edge {
        {"pre", traced_ptr_to_voidptr(pre)},
        {"post", traced_ptr_to_voidptr(post)},
        {"speculative", static_cast<size_t>(speculative)}
    };
    std::map<std::string, decltype(edge)> data {{"edge", edge}};
    write_data(data);
}

void trace_runtime_vertex_constructor(traced_ptr v) {
    std::map<std::string, size_t> data {{"constructor", traced_ptr_to_voidptr(v)}};
    write_data(data);
}

void trace_runtime_vertex_destructor(traced_ptr v) {
    std::map<std::string, size_t> data {{"destructor", traced_ptr_to_voidptr(v)}};
    write_data(data);
}
}
#endif
