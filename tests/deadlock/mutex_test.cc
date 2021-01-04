#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/when_all.hh>
#include <seastar/core/sleep.hh>
#include <iostream>
#include <boost/iterator/counting_iterator.hpp>
#include <seastar/core/shared_mutex.hh>
#include <seastar/util/spinlock.hh>
#include <seastar/core/pipe.hh>
#include <seastar/core/rwlock.hh>
#include <seastar/core/gate.hh>
#include <seastar/core/mutex.hh>
#include <chrono>

using namespace std::chrono_literals;

seastar::mutex limit1;
seastar::mutex limit2;

seastar::future<> test(seastar::mutex& mutex1, seastar::mutex& mutex2) {
    return mutex2.wait().then([&mutex1] {
        return seastar::sleep(1s).then([&mutex1] {
            return mutex1.wait().then([&mutex1] {
                mutex1.signal();
                return mutex1.wait();
            });
        }).then([] {
            return seastar::sleep(1s);
        }).then([&mutex1] {
            mutex1.signal();
            return seastar::make_ready_future<>();
        });
    }).then([&mutex2] {
        mutex2.signal();
    });
}

seastar::future<> check_deadlock() {
    seastar::get_mutex_activity().find_inactive_mutexes();
    return seastar::sleep(1s).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    }).then([] {
        seastar::get_mutex_activity().find_inactive_mutexes();
        return seastar::sleep(1s);
    });
}

int main(int argc, char** argv) {
    seastar::app_template app;
    app.run(argc, argv, [] {
        return seastar::when_all(test(limit1, limit2), test(limit1, limit2), check_deadlock()).discard_result();
    });

}
