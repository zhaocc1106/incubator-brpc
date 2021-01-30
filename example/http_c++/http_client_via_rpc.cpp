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

// 使用rpc方式建立http请求，以及发出http请求，接收http回复
// ./http_client "127.0.0.1:8010" -d '{"message": "hello"}'

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/channel.h>
#include "http.pb.h"

DEFINE_string(d, "", "POST this data to the http server");
DEFINE_string(load_balancer, "", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 2000, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)");
DEFINE_string(protocol, "http", "Client-side protocol");

namespace brpc {
DECLARE_bool(http_verbose);
}

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);

    if (argc != 2) {
        LOG(ERROR) << "Usage: ./http_client \"127.0.0.1:8010\" -d '{\"message\": \"hello\"}'";
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

    // Rpc stub.
    example::HttpService_Stub stub(&channel);

    // Build http request.
    brpc::Controller cntl;

    // Set uri.
    cntl.http_request().uri() = url;
    cntl.http_request().uri().SetQuery("name", "zhaocc");
    cntl.http_request().uri().SetQuery("pwd", "123456");
    // Post method.
    cntl.http_request().set_method(brpc::HTTP_METHOD_POST);
    // Content type.
    cntl.http_request().set_content_type("application/json");
    // Headers.
    cntl.http_request().AppendHeader("token", "123456789");
    cntl.http_request().AppendHeader("sessionid", "123456789");
    // Body.
    if (!FLAGS_d.empty()) {
        cntl.request_attachment().append(FLAGS_d);
    }

    // We will receive response synchronously, safe to put variables on stack.
    example::HttpResponse response;

    // Because `done'(last parameter) is NULL, this function waits until
    // the response comes back or error occurs(including timedout).
    stub.Echo(&cntl, NULL, &response, NULL);
    if (!cntl.Failed()) {
        LOG(INFO) << "Received response from " << cntl.remote_side()
                  << " to " << cntl.local_side()
                  << ": \n(\n" << cntl.response_attachment() << ")"
                  << " \nstatus=" << cntl.http_response().status_code()
                  << " \nlatency=" << cntl.latency_us() << "us";
    } else {
        LOG(WARNING) << cntl.ErrorText();
    }
    return 0;
}
