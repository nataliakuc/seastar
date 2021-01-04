#include <seastar/core/task.hh>

#include <iostream>
#include <list>

#ifdef SEASTAR_DEADLOCK_DETECTION
namespace seastar::internal {
std::list<seastar::task*>& task_list() {
    static thread_local std::list<seastar::task*> list{};
    return list;
}
}
#endif
