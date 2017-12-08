/*
Tencent is pleased to support the open source community by making PhxQueue available.
Copyright (C) 2017 THL A29 Limited, a Tencent company. All rights reserved.
Licensed under the BSD 3-Clause License (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at

<https://opensource.org/licenses/BSD-3-Clause>

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License for the specific language governing permissions and limitations under the License.
*/



/* lock_main.cpp

 Generated by phxrpc_pb2server from lock.proto

*/

#include <iostream>
#include <memory>
#include <signal.h>
#include <unistd.h>

#include "phxrpc/file.h"
#include "phxrpc/http.h"
#include "phxrpc/rpc.h"

#include "lock_server_config.h"
#include "lock_service_impl.h"
#include "phxrpc_lock_dispatcher.h"

#include "phxqueue/comm.h"
#include "phxqueue/plugin.h"
#include "phxqueue/lock.h"

#include "phxqueue_phxrpc/plugin.h"


using namespace std;


extern char *program_invocation_short_name;

static phxqueue::comm::LogFunc g_log_func = nullptr;


static int MakeArgs(LockServerConfig &config, ServiceArgs_t &args) {
    args.config = &config;

    phxqueue::lock::LockOption opt;

    opt.topic = config.GetTopic();

    NLInfo("topic %s", opt.topic.c_str());

    opt.data_dir_path = config.GetDataDirPath();

    opt.ip = config.GetHshaServerConfig().GetBindIP();
    opt.port = config.GetHshaServerConfig().GetPort();
    opt.paxos_port = config.GetPaxosPort();
    opt.log_func = g_log_func;

    string phxqueue_global_config_path(config.GetPhxQueueGlobalConfigPath());
    opt.config_factory_create_func = [phxqueue_global_config_path]()->unique_ptr<phxqueue::plugin::ConfigFactory> {
        return unique_ptr<phxqueue::plugin::ConfigFactory>(new phxqueue_phxrpc::plugin::ConfigFactory(phxqueue_global_config_path));
    };

    args.lock = new phxqueue::lock::Lock(opt);
    if (phxqueue::comm::RetCode::RET_OK != args.lock->Init()) {
        NLErr("init phxqueue lock err");

        return -1;
    }

    return 0;
}


void Dispatch(const phxrpc::BaseRequest *const req,
              phxrpc::BaseResponse *const resp,
              phxrpc::DispatcherArgs_t *const args) {
    ServiceArgs_t *service_args{(ServiceArgs_t *)(args->service_args)};

    LockServiceImpl service(*service_args);
    LockDispatcher dispatcher(service, args);

    phxrpc::BaseDispatcher<LockDispatcher> base_dispatcher(
            dispatcher, LockDispatcher::GetURIFuncMap());
    if (!base_dispatcher.Dispatch(req, resp)) {
        resp->DispatchErr();
    }
}

void ShowUsage(const char *program) {
    printf("\n");
    printf("Usage: %s [-c <config>] [-d] [-l <log level>] [-v]\n", program);
    printf("\n");

    exit(0);
}


int main(int argc, char *argv[]) {
    const char *config_file{nullptr};
    bool daemonize{false};
    int log_level{-1};
    extern char *optarg;
    int c ;
    while ((c = getopt(argc, argv, "c:vl:d")) != EOF) {
        switch (c) {
            case 'c': config_file = optarg; break;
            case 'd': daemonize = true; break;
            case 'l': log_level = atoi(optarg); break;

            case 'v':
            default: ShowUsage(argv[0]); break;
        }
    }

    if (daemonize) phxrpc::ServerUtils::Daemonize();

    assert(signal(SIGPIPE, SIG_IGN) != SIG_ERR);

    // set customize log / monitor
    phxrpc::setvlog(phxqueue::comm::LogFuncForPhxRpc);

    if (nullptr == config_file) ShowUsage(argv[0]);

    LockServerConfig config;
    if (!config.Read(config_file)) ShowUsage(argv[0]);

    if (0 < log_level) config.GetHshaServerConfig().SetLogLevel(log_level);

    //phxqueue::plugin::LoggerSys::GetLogger(program_invocation_short_name, config.GetHshaServerConfig().GetLogLevel(), daemonize, g_log_func);  // syslog
    phxqueue::plugin::LoggerGoogle::GetLogger(program_invocation_short_name, config.GetHshaServerConfig().GetLogDir(), config.GetHshaServerConfig().GetLogLevel(), g_log_func);  // glog

    ServiceArgs_t service_args;
    int ret{MakeArgs(config, service_args)};
    if (0 != ret) {
        printf("ERR: MakeArgs ret %d\n", ret);

        exit(-1);
    }

    phxrpc::HshaServer server(config.GetHshaServerConfig(), Dispatch, &service_args);
    server.RunForever();

    phxrpc::closelog();

    return 0;
}

