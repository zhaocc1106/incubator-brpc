//
// bthread线程间同步，使用bthread互斥元和条件变量.
// https://github.com/zhaocc1106/incubator-brpc/blob/master/docs/cn/bthread.md
//

#include <bthread/bthread.h>
#include <brpc/controller.h>

bthread_mutex_t mutex;
bthread_cond_t cond;

void* func1(void* arg) {
    LOG(INFO) << "func1";
    bthread_mutex_lock(&mutex); // mutex上锁
    bthread_cond_wait(&cond, &mutex); // 等待条件变量
    bthread_mutex_unlock(&mutex); // mutex解锁
    LOG(INFO) << "wait finish.";
    return nullptr;
}

void* func2(void* arg) {
    LOG(INFO) << "func2";
    bthread_usleep(100000); // 睡100ms
    bthread_mutex_lock(&mutex); // mutex上锁
    bthread_cond_signal(&cond); // 通知条件变量唤醒
    LOG(INFO) << "signal finish.";
    bthread_mutex_unlock(&mutex); // mutex解锁
    return nullptr;
}

int main(int argc, char** argv) {
    bthread_mutex_init(&mutex, nullptr);
    bthread_cond_init(&cond, nullptr);

    bthread_t th1;
    bthread_t th2;
    bthread_start_background(&th1, nullptr, func1, nullptr);
    bthread_start_background(&th2, nullptr, func2, nullptr);

    bthread_join(th1, nullptr);
    bthread_join(th2, nullptr);

    bthread_mutex_destroy(&mutex);
    bthread_cond_destroy(&cond);
    return 0;
}
