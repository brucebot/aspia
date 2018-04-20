//
// PROJECT:         Aspia
// FILE:            host/host_server.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#ifndef _ASPIA_HOST__HOST_SERVER_H
#define _ASPIA_HOST__HOST_SERVER_H

#include <QObject>
#include <QPointer>

#include "host/user_list.h"
#include "network/network_server.h"
#include "protocol/authorization.pb.h"

namespace aspia {

class Host;
class HostUserAuthorizer;

class HostServer : public QObject
{
    Q_OBJECT

public:
    HostServer(QObject* parent = nullptr);
    ~HostServer();

    bool start(int port);
    void stop();
    void setSessionChanged(quint32 event, quint32 session_id);

signals:
    void sessionChanged(quint32 event, quint32 session_id);

private slots:
    void onNewConnection();
    void onAuthorizationFinished(HostUserAuthorizer* authorizer);
    void onHostFinished(Host* host);

private:
    UserList user_list_;
    QPointer<NetworkServer> network_server_;

    Q_DISABLE_COPY(HostServer)
};

} // namespace aspia

#endif // _ASPIA_HOST__HOST_SERVER_H