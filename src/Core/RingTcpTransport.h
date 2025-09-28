#ifndef SIMPLECHAT_RINGTCPTRANSPORT_H
#define SIMPLECHAT_RINGTCPTRANSPORT_H
#include "IChatTransport.h"
#include <QtCore>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

namespace Core {
    class RingTcpTransport : public IChatTransport {
        Q_OBJECT
    public:
        RingTcpTransport(QObject* parent=nullptr);
        void start(QHostAddress ipAddress, QString myId, quint16 myPort, quint16 nextPort) override;
        void stop() override;
        void send(const QVariantMap& m) override;
    private slots:
        void onIncomingConnection();
        void onIncomingReadyRead();
        void onIncomingDisconnected();
        void onOutgoingConnected();
        void onOutgoingReadyRead();
        void onOutgoingError(QAbstractSocket::SocketError);
        void ensureOutgoingConnected();
    private:
        QHostAddress ipAddress;
        QString myId;
        quint16 myPort = 0;
        quint16 nextPort = 0;
        QTcpServer server;
        QPointer<QTcpSocket> incoming;
        QTcpSocket outgoing;
        QTimer reconnectTimer;
        QHash<QTcpSocket*, QByteArray> buffers;
        QHash<QTcpSocket*, quint32> expected;
        void processSocket(QTcpSocket* sock);
    };
} // Core

#endif //SIMPLECHAT_RINGTCPTRANSPORT_H