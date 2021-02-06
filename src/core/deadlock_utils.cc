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
#include <sys/types.h>
#include <seastar/core/internal/deadlock_utils.hh>
#include <seastar/core/task.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>
#include <seastar/json/formatter.hh>
#include <seastar/json/json_elements.hh>
#include <map>
#include <fstream>
#include <string>


using dumped_type = std::variant<std::string, uintptr_t, nullptr_t>;

struct json_object : public seastar::json::jsonable {
    dumped_type _value;
    json_object(std::string value) : _value(std::move(value)) {}
    json_object(const char* value) {
        if (value) {
            _value = std::string(value);
        } else {
            _value = nullptr;
        }
    }
    json_object(uintptr_t value) : _value(value) {}
    std::string to_json() const override {
        if (auto ptr = std::get_if<std::string>(&_value)) {
            return "\"" + *ptr + "\"";
        }
        if (auto ptr = std::get_if<uintptr_t>(&_value)) {
            return std::to_string(*ptr);
        }
        if (auto ptr = std::get_if<nullptr_t>(&_value)) {
            (void) ptr;
            return "null";
        }
        throw std::bad_variant_access();
    }
};

using dumped_value = std::map<const char*, json_object>;

namespace seastar::deadlock_detection {

bool operator==(const runtime_vertex& lhs, const runtime_vertex& rhs) {
    return lhs._ptr == rhs._ptr && lhs._base_type->hash_code() == rhs._base_type->hash_code();
}

uintptr_t runtime_vertex::get_ptr() const noexcept {
    return (uintptr_t)_ptr;
}

/// \brief Get output unique to each threads for dumping graph.
///
/// For each thread creates unique file for dumping graph.
/// Is thread and not shard-based because there are multiple threads in shard 0.
static std::ostream& get_output_stream() {
    static thread_local std::ofstream stream(fmt::format("deadlock_detection_graphdump.{}.json", gettid()));
    return stream;
}

/// Global variable for storing currently executed runtime graph vertex.
static runtime_vertex& current_traced_ptr() {
    static thread_local runtime_vertex ptr(nullptr);
    return ptr;
}

/// Serializes and writes data to appropriate file.
template <typename T>
static void write_data(T data) {
    std::string serialized = json::formatter::to_json(data);
    get_output_stream() << serialized << std::endl;
}

/// Converts runtime vertex to serializable data.
static dumped_value serialize_vertex(runtime_vertex v) {
    return {{"value", v._ptr},
            {"base_type", v._base_type->name()},
            {"type", v._type->name()}};
}

/// Converts semaphore to serializable data.
static dumped_value serialize_semaphore(const void* sem, size_t count) {
    return {{"address", reinterpret_cast<uintptr_t>(sem)}, {"available_units", count}};
}

runtime_vertex get_current_traced_ptr() {
    return current_traced_ptr();
}

current_traced_vertex_updater::current_traced_vertex_updater(runtime_vertex new_ptr) : _previous_ptr(current_traced_ptr()), _new_ptr(new_ptr) {
    current_traced_ptr() = new_ptr;
}

current_traced_vertex_updater::~current_traced_vertex_updater() {
    assert(current_traced_ptr() == _new_ptr);
    current_traced_ptr() = _previous_ptr;
}

void trace_edge(runtime_vertex pre, runtime_vertex post, bool speculative) {
    std::map<const char*, dumped_value> edge{
            {"pre", serialize_vertex(pre)},
            {"post", serialize_vertex(post)},
    };
    const char* name = speculative ? "edge" : "edge_speculative";
    std::map<const char*, decltype(edge)> data{{name, edge}};
    write_data(data);
}

void trace_vertex_constructor(runtime_vertex v) {
    std::map<const char*, dumped_value> data{{"constructor", serialize_vertex(v)}};
    write_data(data);
}

void trace_vertex_destructor(runtime_vertex v) {
    std::map<const char*, dumped_value> data{{"destructor", serialize_vertex(v)}};
    write_data(data);
}

void trace_semaphore_constructor(const void* sem, size_t count) {
    auto serialized_sem = serialize_semaphore(sem, count);
    std::map<const char*, dumped_value> data{{"semaphore_constructor", serialized_sem}};
    write_data(data);
}

void trace_semaphore_destructor(const void* sem, size_t count) {
    auto serialized_sem = serialize_semaphore(sem, count);
    std::map<const char*, dumped_value> data{{"semaphore_destructor", serialized_sem}};
    write_data(data);
}


void trace_semaphore_signal_caller(const void* sem, size_t count, runtime_vertex caller) {
    std::map<const char*, dumped_value> data{{
            "semaphore_signal_caller", {
                    {"address", reinterpret_cast<uintptr_t>(sem)},
                    {"count", count},
                    {"caller", caller.get_ptr()}}}};
    write_data(data);
}

void trace_semaphore_signal_schedule(const void* sem, runtime_vertex unlocked) {
    std::map<const char*, dumped_value> data{{
            "semaphore_signal_schedule",
            {{"address", reinterpret_cast<uintptr_t>(sem)},
                    {"callee", unlocked.get_ptr()}}
    }};
    write_data(data);
}

void trace_semaphore_wait(const void* sem, size_t count, runtime_vertex caller) {
    std::map<const char*, dumped_value> data{{
            "semaphore_wait",
            {{"address", reinterpret_cast<uintptr_t>(sem)},
                    {"caller", caller.get_ptr()},
                    {"count", count}}
    }};
    write_data(data);
}

}
#endif
