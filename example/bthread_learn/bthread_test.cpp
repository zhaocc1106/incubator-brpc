//
// 基本bthread用法，与pthread（worker）的关系.
// https://github.com/zhaocc1106/incubator-brpc/blob/master/docs/cn/bthread.md
//

#include <vector>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>

#include <bthread/bthread.h>
#include <butil/logging.h>

std::unordered_map<pthread_t, int> pths; // 存储所有的pthread运行bthread的次数
bthread_mutex_t pths_mutex; // 保护pths
std::atomic<int> count(0); // func1运行的计数

static void* func1(void* arg) {
    // pthread对应bthread的一个worker，bthread放到pthread中运行，所以一个pthread id会被打印多次。
    LOG(INFO) << "func1 ---- current bthread: " << bthread_self()
              << ", current pthread: " << pthread_self();
    bthread_mutex_lock(&pths_mutex); // 须用bthread mutex
    pths[pthread_self()]++;
    count++;
    if ((count & 0x01) == 0) { // 偶数
        LOG(INFO) << "sleep ---- current bthread: " << bthread_self()
                  << ", current pthread: " << pthread_self();
        bthread_usleep(1000); // sleep 100ms，能发现bthread阻塞不会影响所在pthread运行其他的bthread
    }
    bthread_mutex_unlock(&pths_mutex);
    return nullptr;
}

int main(int argc, char** argv) {
    LOG(INFO) << "Hardware Concurrency: " << std::thread::hardware_concurrency() << ", bthread concurrency: "
              << bthread_getconcurrency();
    bthread_setconcurrency((int) std::thread::hardware_concurrency()); // 设置当前bthread的worker（pthread）数量
    LOG(INFO) << "After set, bthread concurrency: " << bthread_getconcurrency();

    bthread_mutex_init(&pths_mutex, nullptr);

    std::vector<bthread_t> ths(30, bthread_t());
    for (auto& th : ths) {
        bthread_start_background(&th, nullptr, func1, nullptr);
    }

    for (auto& th : ths) {
        bthread_join(th, nullptr);
    }

    for (auto& pth: pths) {
        LOG(INFO) << "pthread[" << pth.first << "] had been run " << pth.second << "th.";
    }

    bthread_mutex_destroy(&pths_mutex);
    return 0;
}
