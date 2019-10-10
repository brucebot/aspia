//
// Aspia Project
// Copyright (C) 2019 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#ifndef CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H
#define CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H

#include "client/file_transfer.h"

namespace client {

// The class prepares the task queue to perform the downloading/uploading.
class FileTransferQueueBuilder : public common::FileRequestProducer
{
public:
    FileTransferQueueBuilder(
        std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy,
        common::FileTaskTarget target);
    ~FileTransferQueueBuilder();

    using FinishCallback = std::function<void(proto::FileError)>;

    // Starts building of the task queue.
    void start(const std::string& source_path,
               const std::string& target_path,
               const std::vector<FileTransfer::Item>& items,
               const FinishCallback& callback);

    FileTransfer::TaskList takeQueue();
    int64_t totalSize() const;

protected:
    // FileRequestProducer implementation.
    void onReply(std::shared_ptr<common::FileRequest> request) override;

private:
    void addPendingTask(const std::string& source_dir,
                        const std::string& target_dir,
                        const std::string& item_name,
                        bool is_directory,
                        int64_t size);
    void doPendingTasks();
    void onAborted(proto::FileError error_code);

    std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy_;
    std::shared_ptr<common::FileRequestProducerProxy> request_producer_proxy_;
    std::unique_ptr<FileRequestFactory> request_factory_;

    FinishCallback callback_;

    FileTransfer::TaskList pending_tasks_;
    FileTransfer::TaskList tasks_;
    int64_t total_size_ = 0;

    DISALLOW_COPY_AND_ASSIGN(FileTransferQueueBuilder);
};

} // namespace client

#endif // CLIENT__FILE_TRANSFER_QUEUE_BUILDER_H
