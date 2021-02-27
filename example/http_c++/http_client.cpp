// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// - Access pb services via HTTP
//   ./http_client http://www.foo.com:8765/EchoService/Echo -d '{"message":"hello"}'
// - Access builtin services
//   ./http_client http://www.foo.com:8765/vars/rpc_server*
// - Access www.foo.com
//   ./http_client www.foo.com

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/channel.h>
#include <mutex>
#include <condition_variable>
#include <memory>

DEFINE_string(d, "", "POST this data to the http server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 2000, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)"); 
DEFINE_string(protocol, "http", "Client-side protocol");

namespace brpc {
DECLARE_bool(http_verbose);
}

std::mutex progressive_reader_lock; // progressive reader互斥元
std::condition_variable progressive_reader_done; // 条件变量等待progressive reader结束

class MyProgressiveReader : public brpc::ProgressiveReader {
public:
    // Called when one part was read.
    // Error returned is treated as *permenant* and the socket where the
    // data was read will be closed.
    // A temporary error may be handled by blocking this function, which
    // may block the HTTP parsing on the socket.
    butil::Status OnReadOnePart(const void* data, size_t length) override {
        LOG(INFO) << "OnReadOnePart data: " << (char*) data << " length: " << length;
        return butil::Status::OK();
    }

    // Called when there's nothing to read anymore. The `status' is a hint for
    // why this method is called.
    // - status.ok(): the message is complete and successfully consumed.
    // - otherwise: socket was broken or OnReadOnePart() failed.
    // This method will be called once and only once. No other methods will
    // be called after. User can release the memory of this object inside.
    void OnEndOfMessage(const butil::Status& status) override {
        LOG(INFO) << "OnEndOfMessage status: " << status.ok();
        std::lock_guard<std::mutex> lock(progressive_reader_lock);
        progressive_reader_done.notify_all();
    }

    ~MyProgressiveReader() override {
        LOG(INFO) << "Destructor function of MyProgressiveReader";
    }
};

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    if (argc != 2) {
        LOG(ERROR) << "Usage: ./http_client \"http(s)://www.foo.com\"";
        return -1;
    }
    char* url = argv[1];
    
    // A Channel represents a communication line to a Server. Notice that 
    // Channel is thread-safe and can be shared by all threads in your program.
    brpc::Channel channel;
    brpc::ChannelOptions options;
    options.protocol = FLAGS_protocol;
    options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
    options.max_retry = FLAGS_max_retry;

    // Initialize the channel, NULL means using default options. 
    // options, see `brpc/channel.h'.
    if (channel.Init(url, FLAGS_load_balancer.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return -1;
    }

    // We will receive response synchronously, safe to put variables
    // on stack.
    brpc::Controller cntl;

    cntl.http_request().uri() = url;
    if (!FLAGS_d.empty()) {
        cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
        cntl.request_attachment().append(FLAGS_d);
    }

    bool need_progressive_reader = cntl.http_request().uri().path() == "/example.FileService/default_method/largefile";
    if (need_progressive_reader) {
        cntl.response_will_be_read_progressively(); // 这告诉baidu-rpc在读取http response时只要读完header部分RPC就可以结束了。
    }

    // Because `done'(last parameter) is NULL, this function waits until
    // the response comes back or error occurs(including timedout).
    channel.CallMethod(NULL, &cntl, NULL, NULL, NULL);
    if (cntl.Failed()) {
        std::cerr << cntl.ErrorText() << std::endl;
        return -1;
    }

    if (need_progressive_reader) {
        std::shared_ptr<MyProgressiveReader> my_progressive_reader_guard(new MyProgressiveReader);
        cntl.ReadProgressiveAttachmentBy(my_progressive_reader_guard.get());
        std::unique_lock<std::mutex> lock(progressive_reader_lock);
        progressive_reader_done.wait(lock); // 等待progressive reader结束
    }

    // If -http_verbose is on, brpc already prints the response to stderr.
    if (!brpc::FLAGS_http_verbose) {
        std::cout << cntl.response_attachment() << std::endl;
    }
    return 0;
}
