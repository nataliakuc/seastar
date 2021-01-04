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
#include <seastar/core/semaphore.hh>
#include <seastar/core/future.hh>
#include <seastar/core/shared_ptr.hh>
#include <seastar/core/timer.hh>
#include <chrono>
#include <unordered_set>
#include <queue>
#include <set>
#include <vector>
#include <list>

using namespace std::chrono_literals;

namespace seastar {
class mutex;
struct deadlockable_object;
struct hash_deadlock_object;
class mutex_activity {
private:
    using vertex_set = std::unordered_set<deadlockable_object, hash_deadlock_object>;
    using duration = typename timer<timer<>::clock>::duration;
    using time_point = typename timer<timer<>::clock>::time_point;
    using clock = typename timer<timer<>::clock>::clock;

    std::set<std::pair<time_point, mutex*>> _mutexes;
    std::unordered_map<mutex*, time_point> _last_activity;

    duration MAX_INACTIVE_PERIOD = 3s;

    static void graph_search(mutex *mutex, vertex_set &route, vertex_set &visited);
    static void graph_search(internal::promise_base *promise, vertex_set &route, vertex_set &visited);
    static void graph_search(task *mutex, vertex_set &route, vertex_set &visited);
    static void graph_search(held_locks *mutex, vertex_set &route, vertex_set &visited);

public:
    void delete_mutex(mutex* _mutex);
    void register_activity(mutex* _mutex);
    void find_inactive_mutexes();
};

mutex_activity &get_mutex_activity();

class lock_already_unlocked : public std::exception {
public:
    /// Reports the exception reason.
    virtual const char* what() const noexcept {
        return "Unlocked lock can't be unlocked";
    }
};

class lock_not_found : public std::exception {
public:
    /// Reports the exception reason.
    virtual const char* what() const noexcept {
        return "Given lock couldn't be found";
    }
};

class held_locks {
private:
    std::unordered_set<mutex*> _locks;
    shared_ptr <held_locks> _inherited_locks;
    size_t timestamp;

    static size_t counter;
public:
    held_locks() : timestamp(0) {}
    ~held_locks() {
        if (!_locks.empty()) {
            deadlock_debug("held_lock:dtor Freeing held locks with some locks left.");
        }
    }

    void add_lock(mutex* lock);
    void remove_lock(mutex* lock);

    friend shared_ptr <held_locks> new_lock_level(const shared_ptr <held_locks>& current);
    friend shared_ptr <held_locks> choose_newer_locks(const shared_ptr <held_locks>& lhs, const shared_ptr <held_locks>& rhs);
    friend mutex_activity;
};


class mutex {
private:
    bool _open;
    std::list<promise<>> _wait_list;

public:
    /// Constructs a mutex object.
    /// Initialzie the semaphore with a default value of 1 to
    /// simulate lock behaviour.
    mutex() : _open(true) {
        get_mutex_activity().register_activity(this);
    }

    ~mutex() {
        if (!_open) {
            deadlock_debug("mutex:dtor Deleting locked mutex");
        }
        get_mutex_activity().delete_mutex(this);
    }

    future<> wait() {
        if (_open) {
            get_mutex_activity().register_activity(this);
            _open = false;
            auto fut = make_ready_future<>();
#ifdef SEASTAR_DEADLOCK_DETECTION
            shared_ptr<held_locks> locks;
            if (auto* task = ::seastar::current_task()) {
                if (locks == nullptr) {
                    task->set_held_locks(new_lock_level(nullptr));
                }
                locks = task->get_held_locks();
            } else {
                assert(false);
            }
            locks->add_lock(this);
            fut.set_held_locks(locks);
#endif
            return fut;
        }
        promise<> pr;
        auto fut = pr.get_future();
        try {
            _wait_list.push_back(std::move(pr));
        } catch (...) {
            pr.set_exception(std::current_exception());
        }
        return fut;
    }

    void signal() {
        if (_open) {
            throw lock_already_unlocked();
        }

        get_mutex_activity().register_activity(this);

#ifdef SEASTAR_DEADLOCK_DETECTION
        if (auto* task = ::seastar::current_task()) {
            if (auto locks = task->get_held_locks()) {
                locks->remove_lock(this);
            } else {
                deadlock_debug("mutex:signal No task locks in signal");
            }
        } else {
            deadlock_debug("mutex:signal No current task in signal");
        }
#endif
        _open = true;

        if (!_wait_list.empty()) {
            auto& x = _wait_list.front();
            _open = false;
#ifdef SEASTAR_DEADLOCK_DETECTION
            if (auto locks = x.get_held_locks()) {
                locks->add_lock(this);
            }
#endif
            x.set_value();
            _wait_list.pop_front();
        }
    }

    friend class mutex_activity;
};

inline
void held_locks::add_lock(seastar::mutex* lock) {
    assert(_locks.find(lock) == _locks.end());
    _locks.insert(lock);
    timestamp = counter++;
}

inline
void held_locks::remove_lock(seastar::mutex* lock) {
    timestamp = counter++;
    auto it = _locks.find(lock);
    if (it != _locks.end()) {
        _locks.erase(it);
        return;
    }

    if (auto inherited_locks = _inherited_locks.get()) {
        inherited_locks->remove_lock(lock);
        return;
    }

    deadlock_debug("held_lock:remove_lock Removing not-existant lock");
}


shared_ptr <held_locks> new_lock_level(const shared_ptr <held_locks>& current);
}
