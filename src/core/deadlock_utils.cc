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

#include <seastar/core/internal/deadlock_utils.hh>
#include <seastar/core/task.hh>
#include <seastar/core/semaphore.hh>

#ifdef SEASTAR_DEADLOCK_DETECTION
#include <list>
namespace seastar::internal {

std::map<seastar::task*, std::map<seastar::semaphore*, size_t>>& semaphore_map() {
    static thread_local std::map<seastar::task*, std::map<seastar::semaphore*, size_t>> map{};
    return map;
}

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
}
#endif

