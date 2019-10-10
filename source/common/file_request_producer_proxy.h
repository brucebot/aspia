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

#ifndef COMMON__FILE_REQUEST_PRODUCER_PROXY_H
#define COMMON__FILE_REQUEST_PRODUCER_PROXY_H

#include "base/macros_magic.h"
#include "common/file_request_producer.h"

namespace common {

class FileRequestProducerProxy : public FileRequestProducer
{
public:
    explicit FileRequestProducerProxy(FileRequestProducer* request_producer);
    ~FileRequestProducerProxy();

    void dettach();

    // FileRequestProducer implementation.
    void onReply(std::shared_ptr<FileRequest> request) override;

private:
    FileRequestProducer* request_producer_;

    DISALLOW_COPY_AND_ASSIGN(FileRequestProducerProxy);
};

} // namespace common

#endif // COMMON__FILE_REQUEST_PRODUCER_PROXY_H
