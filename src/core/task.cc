#include <seastar/core/task.hh>

#ifdef SEASTAR_DEADLOCK_DETECTION
#include <list>
namespace seastar::internal {
std::list<seastar::task*>& task_list() {
    static thread_local std::list<seastar::task*> list{};
    return list;
}
}
#endif
