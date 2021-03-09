//
// Created by zhaochaochao on 2021/3/9.
//

#include <bthread/execution_queue.h>
#include <butil/logging.h>

int demo_execute(void* meta, bthread::TaskIterator<int>& iter) {
    auto eq_id = static_cast<bthread::ExecutionQueue<int>::id_t*>(meta);
    LOG(INFO) << "eq_id[" << eq_id->value << "] demo_execute ---- current bthread: " << bthread_self()
              << ", current pthread: " << pthread_self();
    if (iter.is_queue_stopped()) {
        // destroy meta and related resources
        return 0;
    }
    for (; iter; ++iter) {
        LOG(INFO) << "eq_id[" << eq_id->value << "] iter-val: " << *iter;
        usleep(10000); // Simulate time consuming function.
        // do_something(meta, *iter)
    }
    return 0;
}

int main(int argc, char** argv) {
    /* 第一个execution queue. */
    bthread::ExecutionQueue<int>::id_t eq_id;
    bthread::execution_queue_start(&eq_id, nullptr, demo_execute, &eq_id);
    bthread::execution_queue_execute(eq_id, 1, nullptr);
    bthread::execution_queue_execute(eq_id, 2, nullptr);
    bthread::execution_queue_execute(eq_id, 3, &bthread::TASK_OPTIONS_URGENT); // task设置为高优先级
    bthread::execution_queue_execute(eq_id, 4, &bthread::TASK_OPTIONS_URGENT); // task设置为高优先级
    bthread::TaskHandle task_handle;
    bthread::TaskNode task_node;
    task_node.high_priority = true; // 通过task handle方式设置高优先级
    task_handle.node = &task_node;
    bthread::execution_queue_execute(eq_id, 5, &bthread::TASK_OPTIONS_URGENT, &task_handle); // 设置task handler
    // bthread::execution_queue_cancel(task_handle); // 通过handler可以取消某个task

    bthread::ExecutionQueue<int>::id_t eq_id2;
    bthread::execution_queue_start(&eq_id2, nullptr, demo_execute, &eq_id2);
    bthread::execution_queue_execute(eq_id2, 1, nullptr);
    bthread::execution_queue_execute(eq_id2, 2, nullptr);
    usleep(10000); // 等待10ms，这样即使后面task是高优先级也会在前面task后执行
    bthread::execution_queue_execute(eq_id2, 3, &bthread::TASK_OPTIONS_URGENT);
    bthread::execution_queue_execute(eq_id2, 4, &bthread::TASK_OPTIONS_URGENT);
    bthread::TaskHandle task_handle2;
    bthread::TaskNode task_node2;
    task_node2.high_priority = true;
    task_handle2.node = &task_node2;
    bthread::execution_queue_execute(eq_id2, 5, &bthread::TASK_OPTIONS_URGENT, &task_handle2);
    bthread::execution_queue_cancel(task_handle2); // 取消task 5

    bthread::execution_queue_join(eq_id);
    bthread::execution_queue_join(eq_id2);

    return 0;
}