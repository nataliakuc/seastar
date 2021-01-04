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

#include <seastar/core/mutex.hh>
#include <seastar/core/future.hh>
#include <seastar/core/shared_ptr.hh>
#include <functional>
namespace seastar {

void mutex_activity::delete_mutex(mutex* mutex) {
    auto it = _last_activity.find(mutex);

    if (it == _last_activity.end()) {
        deadlock_debug("mutex_activity:delete_mutex Deleting nonexisting mutex");
    } else {
        time_point timestamp = it->second;

        _mutexes.erase(std::make_pair(timestamp, mutex));
        _last_activity.erase(it);
    }
}
void mutex_activity::register_activity(mutex* mutex) {

    auto it = _last_activity.find(mutex);

    if (it != _last_activity.end()) {
        delete_mutex(mutex);
    }

    time_point timestamp = clock::now();
    _last_activity[mutex] = timestamp;
    _mutexes.insert(std::make_pair(timestamp, mutex));
}

struct deadlockable_object {
    enum class object_type {
        Task,
        Promise,
        Mutex,
        Held_Locks
    };

    object_type type;
    void* _ptr;

    deadlockable_object(task* ptr) : type(object_type::Task), _ptr(ptr) {}
    deadlockable_object(internal::promise_base* ptr) : type(object_type::Promise), _ptr(ptr) {}
    deadlockable_object(mutex* ptr) : type(object_type::Mutex), _ptr(ptr) {}
    deadlockable_object(held_locks* ptr) : type(object_type::Held_Locks), _ptr(ptr) {}

    friend bool operator==(const deadlockable_object& lhs, const deadlockable_object& rhs) {
        if (lhs.type != rhs.type) {
            return false;
        }

        return lhs._ptr == rhs._ptr;
    }

    friend struct hash_deadlockable_object;
};

struct deadlock_found_backtrace : public std::exception {
public:
    const deadlockable_object _initiator;

    deadlock_found_backtrace(deadlockable_object object) : _initiator(object) {}

    const char* what() const noexcept override {
        return "Found a deadlock.";
    }
};

struct deadlock_found : public std::exception {
    deadlock_found() = default;

    const char* what() const noexcept override {
        return "Found a deadlock.";
    }
};

struct hash_deadlockable_object {
    size_t operator()(const deadlockable_object& rhs) const {
        return reinterpret_cast<size_t>(rhs._ptr);
    }
};

void mutex_activity::graph_search(mutex* mutex, vertex_set& route, vertex_set& visited) {
    if (!mutex) {
        return;
    }

    if (route.count(mutex)) {
        deadlock_debug("DEADLOCK");
        throw deadlock_found_backtrace(mutex);
    }

    if (visited.count(mutex)) {
        return;
    }

    route.insert(mutex);
    visited.insert(mutex);
    try {
        for (auto& promise : mutex->_wait_list) {
            graph_search((internal::promise_base*) &promise, route, visited);
        }
    }
    catch (const deadlock_found_backtrace& e) {
        deadlock_debug("deadlocked mutex at ", mutex);
        if (e._initiator == mutex) {
            throw deadlock_found();
        }
        throw e;
    }
    route.erase(mutex);
}

void mutex_activity::graph_search(internal::promise_base* promise, vertex_set& route, vertex_set& visited) {
    if (!promise) {
        return;
    }

    if (route.count(promise)) {
        deadlock_debug("DEADLOCK");
        throw deadlock_found_backtrace(promise);
    }

    if (visited.count(promise)) {
        return;
    }

    route.insert(promise);
    visited.insert(promise);

    try {
        if (auto locks = promise->get_held_locks().get()) {
            graph_search(locks, route, visited);
        }

        if (auto task = promise->waiting_task()) {
            graph_search(task, route, visited);
        }
    }
    catch (const deadlock_found_backtrace& e) {
        deadlock_debug("deadlocked promise at ", promise);
        if (e._initiator == promise) {
            throw deadlock_found();
        }
        throw e;
    }

    route.erase(promise);
}

void mutex_activity::graph_search(task* task, vertex_set& route, vertex_set& visited) {
    if (!task) {
        return;
    }

    if (route.count(task)) {
        deadlock_debug("DEADLOCK");
        throw deadlock_found_backtrace(task);
    }

    if (visited.count(task)) {
        return;
    }

    route.insert(task);
    visited.insert(task);

    try {
        if (auto locks = task->get_held_locks().get()) {
            graph_search(locks, route, visited);
        }

        if (auto promise = task->waiting_promise()) {
            graph_search(promise, route, visited);
        }

        if (auto next_task = task->waiting_task()) {
            graph_search(next_task, route, visited);
        }
    }
    catch (const deadlock_found_backtrace& e) {
        deadlock_debug("deadlocked task at ", task);
        if (e._initiator == task) {
            throw deadlock_found();
        }
        throw e;
    }

    route.erase(task);
}

void mutex_activity::graph_search(held_locks* locks, vertex_set& route, vertex_set& visited) {
    if (!locks) {
        return;
    }

    if (route.count(locks)) {
        deadlock_debug("DEADLOCK");
        throw deadlock_found_backtrace(locks);
    }

    if (visited.count(locks)) {
        return;
    }

    route.insert(locks);
    visited.insert(locks);

    try {
        for (auto* mutex : locks->_locks) {
            graph_search(mutex, route, visited);
        }

        if (auto* inherited_locks = locks->_inherited_locks.get()) {
            graph_search(inherited_locks, route, visited);
        }
    }
    catch (const deadlock_found_backtrace& e) {
        if (e._initiator == locks) {
            throw deadlock_found();
        }
        throw e;
    }

    route.erase(locks);
}

void mutex_activity::find_inactive_mutexes() {
    time_point now = clock::now();
    vertex_set visited;
    vertex_set route;
    try {
        for (auto[time, mutex] : _mutexes) {
            if (now - time < MAX_INACTIVE_PERIOD) {
                break;
            }

            if (mutex->_open) {
                continue;
            }

            graph_search(mutex, route, visited);
        }
    } catch (const deadlock_found &e) {
        // skip
    } catch (const deadlock_found_backtrace &e) {
        assert(false);
    }
}

shared_ptr<held_locks> new_lock_level(const shared_ptr<held_locks>& current) {
    auto new_level = make_shared<held_locks>();
    new_level->_inherited_locks = current;
    if (auto* cur = current.get()) {
        new_level->timestamp = cur->timestamp;
    }
    return new_level;
}

shared_ptr<held_locks> choose_newer_locks(const shared_ptr<held_locks>& lhs, const shared_ptr<held_locks>& rhs) {
    if (auto* l = lhs.get()) {
        if (auto* r = rhs.get()) {
            if (l->timestamp < r->timestamp) {
                return rhs;
            } else {
                return lhs;
            }
        } else {
            return lhs;
        }
    } else {
        return rhs;
    }
}

size_t held_locks::counter = 1;

mutex_activity& get_mutex_activity() {
    static thread_local mutex_activity activity;
    return activity;
}

}

