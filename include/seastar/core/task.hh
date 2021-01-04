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
 * Copyright (C) 2015 Cloudius Systems, Ltd.
 */

#pragma once

#include <memory>
#include <seastar/core/scheduling.hh>
#include <seastar/util/backtrace.hh>
#include <seastar/core/shared_ptr.hh>

#ifdef SEASTAR_DEADLOCK_DETECTION
#include <list>
namespace seastar {
class task;
namespace internal {
std::list<seastar::task*>& task_list();
}
}
#endif

namespace seastar {
class held_locks;
namespace internal {
class promise_base;
}

class task {
    scheduling_group _sg;
#ifdef SEASTAR_TASK_BACKTRACE
    shared_backtrace _bt;
#endif
#ifdef SEASTAR_DEADLOCK_DETECTION
    std::list<seastar::task*>::iterator task_list_iterator;
    shared_ptr<held_locks> _held;
#endif
protected:
    // Task destruction is performed by run_and_dispose() via a concrete type,
    // so no need for a virtual destructor here. Derived classes that implement
    // run_and_dispose() should be declared final to avoid losing concrete type
    // information via inheritance.
    ~task() {
#ifdef SEASTAR_DEADLOCK_DETECTION
        if (task_list_iterator != internal::task_list().end()) {
            internal::task_list().erase(task_list_iterator);
            task_list_iterator = internal::task_list().end();
        }
#endif
    }

public:
    explicit task(scheduling_group sg = current_scheduling_group()) noexcept: _sg(sg) {
#ifdef SEASTAR_DEADLOCK_DETECTION
        task_list_iterator = internal::task_list().insert(internal::task_list().end(), this);
#endif
    }
    void set_held_locks(shared_ptr<held_locks> held);
    shared_ptr<held_locks> get_held_locks();
    virtual void run_and_dispose() noexcept = 0;
    /// Returns the next task which is waiting for this task to complete execution, or nullptr.
    virtual task* waiting_task() noexcept = 0;
    scheduling_group group() const { return _sg; }
    shared_backtrace get_backtrace() const;
#ifdef SEASTAR_TASK_BACKTRACE
    void make_backtrace() noexcept;
#else
    void make_backtrace() noexcept {}
#endif


#ifdef SEASTAR_DEADLOCK_DETECTION
    virtual internal::promise_base *waiting_promise() const noexcept {
        return nullptr;
    }
#else
    internal::promise_base *waiting_promise() {
        return nullptr;
    }
#endif
};

inline
shared_backtrace task::get_backtrace() const {
#ifdef SEASTAR_TASK_BACKTRACE
    return _bt;
#else
    return {};
#endif
}

void schedule(task* t) noexcept;
void schedule_urgent(task* t) noexcept;
task* current_task() noexcept;
shared_ptr<held_locks> new_lock_level(const shared_ptr<held_locks>& current);
shared_ptr<held_locks> choose_newer_locks(const shared_ptr<held_locks> &lhs, const shared_ptr<held_locks> &rhs);

inline
void task::set_held_locks(shared_ptr<held_locks> held) {
#ifdef SEASTAR_DEADLOCK_DETECTION
    _held = std::move(held);
#endif
}
inline
shared_ptr<held_locks> task::get_held_locks() {
#ifdef SEASTAR_DEADLOCK_DETECTION
    return _held;
#else
    return shared_ptr<held_locks>();
#endif
}

#ifdef SEASTAR_DEADLOCK_DETECTION
void deadlock_debug(std::string_view);
#else
inline void deadlock_debug(std::string_view) {}
#endif

}
