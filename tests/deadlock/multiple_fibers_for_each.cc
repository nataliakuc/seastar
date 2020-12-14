#include <iostream>
#include <chrono>

#include <seastar/core/app-template.hh>
#include <seastar/core/when_all.hh>
#include <seastar/core/fstream.hh>
#include <seastar/core/loop.hh>
#include <seastar/core/sleep.hh>
#include <seastar/util/tmp_file.hh>
#include <seastar/core/reactor.hh>

using namespace std::chrono_literals;
static thread_local seastar::semaphore limit_concurrent(5);

static seastar::future<> simulate_work(size_t milliseconds) {
    if (milliseconds == 0) {
        return seastar::make_ready_future<>();
    }
    size_t val = rand() % (milliseconds + 1);
    milliseconds -= val;
    return seastar::sleep(val * 1ms).then([milliseconds]() mutable {
        size_t val = rand() % (milliseconds / 10 + 1);
        milliseconds -= val;
        std::this_thread::sleep_for(val * 1ms);
        return simulate_work(milliseconds);
    });
}

static seastar::future<> run_2() {
    return seastar::with_semaphore(limit_concurrent, 1, [] {
        return simulate_work(10);
    });
}

static seastar::future<> run_1(int i) {
    return seastar::with_semaphore(limit_concurrent, 1, [i] {
        return simulate_work(10).then([i] {
            std::cout << i << " finished first work" << std::endl;
            return run_2().then([i] {
                std::cout << i << " finished second work" << std::endl;
            });
        });
    });
}

static seastar::future<> test() {
    return seastar::parallel_for_each(boost::irange(5), run_1);
}

int main(int ac, char **av) {
    namespace bpo = boost::program_options;
    seastar::app_template app;
    return app.run(ac, av, test);
}
