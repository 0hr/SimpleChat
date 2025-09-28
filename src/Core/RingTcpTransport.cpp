#include "RingTcpTransport.h"

#include "IChatTransport.h"
#include "MessageSerializer.h"

namespace Core {
    RingTcpTransport::RingTcpTransport(QObject* parent)
        : IChatTransport(parent) {
        connect(&server, &QTcpServer::newConnection, this, &RingTcpTransport::onIncomingConnection);

        connect(&outgoing, &QTcpSocket::connected, this, &RingTcpTransport::onOutgoingConnected);
        connect(&outgoing, &QTcpSocket::readyRead, this, &RingTcpTransport::onOutgoingReadyRead);
        connect(&outgoing, &QTcpSocket::errorOccurred, this, &RingTcpTransport::onOutgoingError);
        connect(&reconnectTimer, &QTimer::timeout, this, &RingTcpTransport::ensureOutgoingConnected);
        reconnectTimer.setInterval(1000);
        reconnectTimer.setSingleShot(false);
    }

    void RingTcpTransport::start(QHostAddress ipAddress, QString myId, quint16 myPort, quint16 nextPort) {
        this->ipAddress = ipAddress;
        this->myId = myId;
        this->myPort = myPort;
        this->nextPort = nextPort;

        if (!server.isListening()) {
            if (!server.listen(ipAddress, myPort)) {
                emit errorOccurred(QStringLiteral("Listen failed on %1: %2").arg(myPort).arg(server.errorString()));
            }
        }
        ensureOutgoingConnected();
        reconnectTimer.start();
    }

    void RingTcpTransport::stop() {
        reconnectTimer.stop();
        server.close();
        if (incoming) {
            incoming->close();
            incoming->deleteLater();
        }
        outgoing.close();
    }
    void RingTcpTransport::ensureOutgoingConnected() {
        if (outgoing.state() == QAbstractSocket::ConnectedState ||
            outgoing.state() == QAbstractSocket::ConnectingState) return;
        outgoing.abort();
        outgoing.connectToHost(ipAddress, nextPort);
    }

    void RingTcpTransport::onOutgoingConnected() {
        emit connected();
    }

    void RingTcpTransport::onOutgoingError(QAbstractSocket::SocketError) {
        emit errorOccurred(outgoing.errorString());
    }

    void RingTcpTransport::onIncomingConnection() {
        if (incoming) {
            incoming->disconnect(this);
            incoming->deleteLater();
        }
        incoming = server.nextPendingConnection();
        connect(incoming, &QTcpSocket::readyRead, this, &RingTcpTransport::onIncomingReadyRead);
        connect(incoming, &QTcpSocket::disconnected, this, &RingTcpTransport::onIncomingDisconnected);
    }

    void RingTcpTransport::onIncomingDisconnected() {
        if (incoming) {
            incoming->deleteLater();
        }
        incoming.clear();
    }

    void RingTcpTransport::processSocket(QTcpSocket* sock) {
        QVariantMap msg;
        QByteArray& buf = buffers[sock];
        buf.append(sock->readAll());
        quint32& expect = expected[sock];
        while (MessageSerializer::unserialize(buf, expect, msg)) {
            if (msg.value("Destination").toString() == myId) {
                emit messageReceived(msg);
            } else {
                send(msg);
            }
        }
    }
    void RingTcpTransport::onIncomingReadyRead() {
        if (!incoming) return;
        processSocket(incoming);
    }

    void RingTcpTransport::onOutgoingReadyRead() {
        processSocket(&outgoing);
    }

    void RingTcpTransport::send(const QVariantMap& m) {
        if (outgoing.state() != QAbstractSocket::ConnectedState) {
            emit errorOccurred(QStringLiteral("Not connected to right neighbor"));
            return;
        }
        outgoing.write(MessageSerializer::serialize(m));
        outgoing.flush();
    }
} // Core