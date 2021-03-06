/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 **/

#ifndef QUICKSTEP_CLI_TESTS_COMMAND_EXECUTOR_TEST_RUNNER_HPP_
#define QUICKSTEP_CLI_TESTS_COMMAND_EXECUTOR_TEST_RUNNER_HPP_

#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "cli/DefaultsConfigurator.hpp"
#include "parser/SqlParserWrapper.hpp"
#include "query_execution/ForemanSingleNode.hpp"
#include "query_execution/QueryExecutionTypedefs.hpp"
#include "query_execution/QueryExecutionUtil.hpp"
#include "query_execution/Worker.hpp"
#include "query_execution/WorkerDirectory.hpp"
#include "query_execution/WorkerMessage.hpp"
#include "query_optimizer/QueryProcessor.hpp"
#include "storage/StorageConstants.hpp"
#include "utility/Macros.hpp"
#include "utility/textbased_test/TextBasedTestDriver.hpp"

#include "tmb/id_typedefs.h"
#include "tmb/message_bus.h"

#include "glog/logging.h"

namespace quickstep {

/**
 * @brief TextBasedTestRunner for testing the ExecutionGenerator.
 */
class CommandExecutorTestRunner : public TextBasedTestRunner {
 public:
  /**
   * @brief Constructor.
   */
  explicit CommandExecutorTestRunner(const std::string &storage_path)
      : catalog_path_(storage_path + kCatalogFilename),
        storage_manager_(storage_path) {
    DefaultsConfigurator::InitializeDefaultDatabase(storage_path, catalog_path_);
    query_processor_ = std::make_unique<QueryProcessor>(std::string(catalog_path_));

    bus_.Initialize();

    main_thread_client_id_ = bus_.Connect();
    bus_.RegisterClientAsSender(main_thread_client_id_, kAdmitRequestMessage);
    bus_.RegisterClientAsSender(main_thread_client_id_, kPoisonMessage);
    bus_.RegisterClientAsReceiver(main_thread_client_id_, kWorkloadCompletionMessage);

    worker_.reset(new Worker(0, &bus_));

    std::vector<client_id> worker_client_ids;
    worker_client_ids.push_back(worker_->getBusClientID());

    // We don't use the NUMA aware version of foreman code.
    std::vector<int> numa_nodes;
    numa_nodes.push_back(-1);

    workers_.reset(new WorkerDirectory(1 /* number of workers */,
                                       worker_client_ids, numa_nodes));
    foreman_.reset(
        new ForemanSingleNode(main_thread_client_id_,
                              workers_.get(),
                              &bus_,
                              query_processor_->getDefaultDatabase(),
                              &storage_manager_));

    foreman_->start();
    worker_->start();
  }

  ~CommandExecutorTestRunner() {
    QueryExecutionUtil::BroadcastPoisonMessage(main_thread_client_id_, &bus_);
    worker_->join();
    foreman_->join();

    const std::string command = "rm -f " + catalog_path_;
    CHECK(!std::system(command.c_str()))
         << "Failed when attempting to remove catalog proto file: " << catalog_path_;
  }

  void runTestCase(const std::string &input,
                   const std::set<std::string> &options,
                   std::string *output) override;

 private:
  const std::string catalog_path_;

  SqlParserWrapper sql_parser_;
  StorageManager storage_manager_;
  std::unique_ptr<QueryProcessor> query_processor_;

  tmb::client_id main_thread_client_id_;

  MessageBusImpl bus_;
  std::unique_ptr<ForemanSingleNode> foreman_;
  std::unique_ptr<Worker> worker_;

  std::unique_ptr<WorkerDirectory> workers_;

  DISALLOW_COPY_AND_ASSIGN(CommandExecutorTestRunner);
};

}  // namespace quickstep

#endif  //  QUICKSTEP_CLI_TESTS_COMMAND_EXECUTOR_TEST_RUNNER_HPP_
