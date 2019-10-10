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

#include "client/file_transfer.h"

#include "base/logging.h"
#include "client/file_request_factory.h"
#include "client/file_transfer_proxy.h"
#include "client/file_transfer_queue_builder.h"
#include "client/file_transfer_window_proxy.h"
#include "common/file_request_consumer_proxy.h"
#include "common/file_packet.h"
#include "common/file_request_producer_proxy.h"

namespace client {

namespace {

struct ActionsMap
{
    FileTransfer::Error::Type type;
    uint32_t available_actions;
    FileTransfer::Error::Action default_action;
} static const kActions[] =
{
    {
        FileTransfer::Error::Type::CREATE_DIRECTORY,
        FileTransfer::Error::ACTION_ABORT | FileTransfer::Error::ACTION_SKIP |
            FileTransfer::Error::ACTION_SKIP_ALL,
        FileTransfer::Error::ACTION_ASK
    },
    {
        FileTransfer::Error::Type::CREATE_FILE,
        FileTransfer::Error::ACTION_ABORT | FileTransfer::Error::ACTION_SKIP |
            FileTransfer::Error::ACTION_SKIP_ALL,
        FileTransfer::Error::ACTION_ASK
    },
    {
        FileTransfer::Error::Type::OPEN_FILE,
        FileTransfer::Error::ACTION_ABORT | FileTransfer::Error::ACTION_SKIP |
            FileTransfer::Error::ACTION_SKIP_ALL,
        FileTransfer::Error::ACTION_ASK
    },
    {
        FileTransfer::Error::Type::ALREADY_EXISTS,
        FileTransfer::Error::ACTION_ABORT | FileTransfer::Error::ACTION_SKIP |
            FileTransfer::Error::ACTION_SKIP_ALL | FileTransfer::Error::ACTION_REPLACE |
            FileTransfer::Error::ACTION_REPLACE_ALL,
        FileTransfer::Error::ACTION_ASK
    },
    {
        FileTransfer::Error::Type::WRITE_FILE,
        FileTransfer::Error::ACTION_ABORT | FileTransfer::Error::ACTION_SKIP |
            FileTransfer::Error::ACTION_SKIP_ALL,
        FileTransfer::Error::ACTION_ASK
    },
    {
        FileTransfer::Error::Type::READ_FILE,
        FileTransfer::Error::ACTION_ABORT | FileTransfer::Error::ACTION_SKIP |
            FileTransfer::Error::ACTION_SKIP_ALL,
        FileTransfer::Error::ACTION_ASK
    },
    {
        FileTransfer::Error::Type::OTHER,
        FileTransfer::Error::ACTION_ABORT,
        FileTransfer::Error::ACTION_ASK
    }
};

} // namespace

FileTransfer::FileTransfer(std::shared_ptr<base::TaskRunner> io_task_runner,
                           std::shared_ptr<FileTransferWindowProxy> transfer_window_proxy,
                           std::shared_ptr<common::FileRequestConsumerProxy> request_consumer_proxy,
                           Type type)
    : io_task_runner_(io_task_runner),
      transfer_proxy_(std::make_shared<FileTransferProxy>(io_task_runner, this)),
      transfer_window_proxy_(transfer_window_proxy),
      request_consumer_proxy_(request_consumer_proxy),
      request_producer_proxy_(std::make_shared<common::FileRequestProducerProxy>(this)),
      type_(type)
{
    // Nothing
}

FileTransfer::~FileTransfer()
{
    request_producer_proxy_->dettach();
    transfer_proxy_->dettach();
}

void FileTransfer::start(const std::string& source_path,
                         const std::string& target_path,
                         const std::vector<Item>& items,
                         const FinishCallback& finish_callback)
{
    finish_callback_ = finish_callback;

    std::unique_ptr<FileRequestFactory> request_factory_local =
        std::make_unique<FileRequestFactory>(
            request_producer_proxy_, common::FileTaskTarget::LOCAL);

    std::unique_ptr<FileRequestFactory> request_factory_remote =
        std::make_unique<FileRequestFactory>(
            request_producer_proxy_, common::FileTaskTarget::REMOTE);

    if (type_ == Type::DOWNLOADER)
    {
        request_factory_source_ = std::move(request_factory_remote);
        request_factory_target_ = std::move(request_factory_local);
    }
    else
    {
        DCHECK_EQ(type_, Type::UPLOADER);

        request_factory_source_ = std::move(request_factory_local);
        request_factory_target_ = std::move(request_factory_remote);
    }

    // Asynchronously start UI.
    transfer_window_proxy_->start(transfer_proxy_);

    queue_builder_ = std::make_unique<FileTransferQueueBuilder>(
        request_consumer_proxy_, request_factory_source_->target());

    // Start building a list of objects for transfer.
    queue_builder_->start(source_path, target_path, items, [this](proto::FileError error_code)
    {
        if (error_code == proto::FILE_ERROR_SUCCESS)
        {
            tasks_ = queue_builder_->takeQueue();
            total_size_ = queue_builder_->totalSize();

            if (tasks_.empty())
            {
                onFinished();
            }
            else
            {
                doFrontTask(false);
            }
        }
        else
        {
            onError(Error::Type::QUEUE, proto::FILE_ERROR_UNKNOWN);
        }

        queue_builder_.reset();
    });
}

void FileTransfer::stop()
{
    if (queue_builder_)
    {
        queue_builder_.reset();
        onFinished();
    }
    else
    {
        is_canceled_ = true;
        cancel_timer_.start(
            io_task_runner_, std::chrono::seconds(5), std::bind(&FileTransfer::onFinished, this));
    }
}

void FileTransfer::setActionForErrorType(Error::Type error_type, Error::Action action)
{
    actions_.insert_or_assign(error_type, action);
}

void FileTransfer::onReply(std::shared_ptr<common::FileRequest> request)
{
    if (type_ == Type::DOWNLOADER)
    {
        if (request->target() == common::FileTaskTarget::LOCAL)
        {
            targetReply(request->request(), request->reply());
        }
        else
        {
            DCHECK_EQ(request->target(), common::FileTaskTarget::REMOTE);

            sourceReply(request->request(), request->reply());
        }
    }
    else
    {
        DCHECK_EQ(type_, Type::UPLOADER);

        if (request->target() == common::FileTaskTarget::LOCAL)
        {
            sourceReply(request->request(), request->reply());
        }
        else
        {
            DCHECK_EQ(request->target(), common::FileTaskTarget::REMOTE);

            targetReply(request->request(), request->reply());
        }
    }
}

FileTransferTask& FileTransfer::frontTask()
{
    return tasks_.front();
}

void FileTransfer::targetReply(const proto::FileRequest& request, const proto::FileReply& reply)
{
    if (tasks_.empty())
        return;

    if (request.has_create_directory_request())
    {
        if (reply.error_code() == proto::FILE_ERROR_SUCCESS ||
            reply.error_code() == proto::FILE_ERROR_PATH_ALREADY_EXISTS)
        {
            doNextTask();
            return;
        }

        onError(Error::Type::CREATE_DIRECTORY, reply.error_code(), frontTask().targetPath());
    }
    else if (request.has_upload_request())
    {
        if (reply.error_code() != proto::FILE_ERROR_SUCCESS)
        {
            Error::Type error_type = Error::Type::CREATE_FILE;

            if (reply.error_code() == proto::FILE_ERROR_PATH_ALREADY_EXISTS)
                error_type = Error::Type::ALREADY_EXISTS;

            onError(error_type, reply.error_code(), frontTask().targetPath());
            return;
        }

        request_consumer_proxy_->doRequest(
            request_factory_source_->packetRequest(proto::FilePacketRequest::NO_FLAGS));
    }
    else if (request.has_packet())
    {
        if (reply.error_code() != proto::FILE_ERROR_SUCCESS)
        {
            onError(Error::Type::WRITE_FILE, reply.error_code(), frontTask().targetPath());
            return;
        }

        const int64_t full_task_size = frontTask().size();
        if (full_task_size && total_size_)
        {
            int64_t packet_size = common::kMaxFilePacketSize;

            task_transfered_size_ += packet_size;

            if (task_transfered_size_ > full_task_size)
            {
                packet_size = task_transfered_size_ - full_task_size;
                task_transfered_size_ = full_task_size;
            }

            total_transfered_size_ += packet_size;

            const int task_percentage = task_transfered_size_ * 100 / full_task_size;
            const int total_percentage = total_transfered_size_ * 100 / total_size_;

            if (task_percentage != task_percentage_ || total_percentage != total_percentage_)
            {
                task_percentage_ = task_percentage;
                total_percentage_ = total_percentage;

                transfer_window_proxy_->setCurrentProgress(total_percentage_, task_percentage_);
            }
        }

        if (request.packet().flags() & proto::FilePacket::LAST_PACKET)
        {
            doNextTask();
            return;
        }

        uint32_t flags = proto::FilePacketRequest::NO_FLAGS;
        if (is_canceled_)
            flags = proto::FilePacketRequest::CANCEL;

        request_consumer_proxy_->doRequest(request_factory_source_->packetRequest(flags));
    }
    else
    {
        onError(Error::Type::OTHER, proto::FILE_ERROR_UNKNOWN);
    }
}

void FileTransfer::sourceReply(const proto::FileRequest& request, const proto::FileReply& reply)
{
    if (tasks_.empty())
        return;

    if (request.has_download_request())
    {
        FileTransferTask& front_task = frontTask();

        if (reply.error_code() != proto::FILE_ERROR_SUCCESS)
        {
            onError(Error::Type::OPEN_FILE, reply.error_code(), front_task.sourcePath());
            return;
        }

        request_consumer_proxy_->doRequest(
            request_factory_target_->uploadRequest(
                front_task.targetPath(), front_task.overwrite()));
    }
    else if (request.has_packet_request())
    {
        if (reply.error_code() != proto::FILE_ERROR_SUCCESS)
        {
            onError(Error::Type::READ_FILE, reply.error_code(), frontTask().sourcePath());
            return;
        }

        request_consumer_proxy_->doRequest(request_factory_target_->packet(reply.packet()));
    }
    else
    {
        onError(Error::Type::OTHER, proto::FILE_ERROR_UNKNOWN);
    }
}

void FileTransfer::setAction(Error::Type error_type, Error::Action action)
{
    switch (action)
    {
        case Error::ACTION_ABORT:
            onFinished();
            break;

        case Error::ACTION_REPLACE:
        case Error::ACTION_REPLACE_ALL:
        {
            if (action == Error::ACTION_REPLACE_ALL)
                setActionForErrorType(error_type, action);

            doFrontTask(true);
        }
        break;

        case Error::ACTION_SKIP:
        case Error::ACTION_SKIP_ALL:
        {
            if (action == Error::ACTION_SKIP_ALL)
                setActionForErrorType(error_type, action);

            doNextTask();
        }
        break;

        default:
            NOTREACHED();
            break;
    }
}

void FileTransfer::doFrontTask(bool overwrite)
{
    task_percentage_ = 0;
    task_transfered_size_ = 0;

    FileTransferTask& front_task = frontTask();
    front_task.setOverwrite(overwrite);

    transfer_window_proxy_->setCurrentItem(front_task.sourcePath(), front_task.targetPath());

    if (front_task.isDirectory())
    {
        request_consumer_proxy_->doRequest(
            request_factory_target_->createDirectoryRequest(front_task.targetPath()));
    }
    else
    {
        request_consumer_proxy_->doRequest(
            request_factory_source_->downloadRequest(front_task.sourcePath()));
    }
}

void FileTransfer::doNextTask()
{
    if (is_canceled_)
    {
        while (!tasks_.empty())
            tasks_.pop_front();
    }

    if (!tasks_.empty())
    {
        // Delete the task only after confirmation of its successful execution.
        tasks_.pop_front();
    }

    if (tasks_.empty())
    {
        if (cancel_timer_.isActive())
            cancel_timer_.stop();

        onFinished();
        return;
    }

    doFrontTask(false);
}

void FileTransfer::onError(Error::Type type, proto::FileError code, const std::string& path)
{
    auto default_action = actions_.find(type);
    if (default_action != actions_.end())
    {
        setAction(type, default_action->second);
        return;
    }

    transfer_window_proxy_->errorOccurred(Error(type, code, path));
}

void FileTransfer::onFinished()
{
    FinishCallback callback;
    callback.swap(finish_callback_);

    if (callback)
    {
        transfer_window_proxy_->stop();
        callback();
    }
}

uint32_t FileTransfer::Error::availableActions() const
{
    for (size_t i = 0; i < sizeof(kActions) / sizeof(kActions[0]); ++i)
    {
        if (kActions[i].type == type_)
            return kActions[i].available_actions;
    }

    return 0;
}

FileTransfer::Error::Action FileTransfer::Error::defaultAction() const
{
    for (size_t i = 0; i < sizeof(kActions) / sizeof(kActions[0]); ++i)
    {
        if (kActions[i].type == type_)
            return kActions[i].default_action;
    }

    return Action::ACTION_ABORT;
}

} // namespace client
