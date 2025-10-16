#ifndef SIMPLECHAT_UDPPEERTRANSPORT_H
#define SIMPLECHAT_UDPPEERTRANSPORT_H

#include "IChatTransport.h"
#include <QtNetwork/QUdpSocket>
#include <QQueue>
#include <QTimer>

namespace Core {
    class UdpPeerTransport : public IChatTransport {
        Q_OBJECT
    public:
        explicit UdpPeerTransport(QObject* parent = nullptr);
        void start(const QHostAddress& bindAddress,
                   const QString& myId,
                   quint16 myPort,
                   const QList<Peer>& initialPeers) override;
        void stop() override;
        void send(const QVariantMap& map) override;
        void addPeer(const QHostAddress& address, quint16 port) override;

    private slots:
        void onReadyRead();
        void onResendTimeout();
        void onAntiEntropyTimeout();
        void processDiscoveryQueue();

    private:
        struct PeerState {
            QString id;
            QHostAddress address;
            quint16 port = 0;
            QHash<QString, qulonglong> vectorClock;
            QSet<QString> pendingKeys;
        };

        struct PendingAck {
            QVariantMap payload;
            QString peerId;
            QString origin;
            qulonglong seq = 0;
            qint64 lastSent = 0;
            int attempts = 0;
        };

        QUdpSocket socket;
        QString myId;
        quint16 myPort = 0;
        QHostAddress bindAddress;
        bool running = false;

        QHash<QString, PeerState> peersById;
        QHash<QString, QString> endpointToId;
        QHash<QString, QVariantMap> historyByKey;
        QHash<QString, QSet<QString>> messageReceivers;
        QHash<QString, qulonglong> localVectorClock;
        QHash<QString, PendingAck> pendingAcks;
        QSet<QString> pendingEndpoints;
        QQueue<Peer> discoveryQueue;
        QSet<QString> queuedEndpoints;

        QTimer resendTimer;
        QTimer antiEntropyTimer;
        QTimer discoveryTimer;

        static QString endpointKey(const QHostAddress& address, quint16 port);
        static QString messageKey(const QString& origin, qulonglong seq);
        static QString pendingKey(const QString& peerId, const QString& origin, qulonglong seq);
        static qint64 currentMs();

        void discoverLocalPeers();
        void enqueueDiscovery(const QHostAddress& address, quint16 port);
        void sendHello(const QHostAddress& address, quint16 port, bool isReply = false);
        void addOrUpdatePeer(const QString& peerId, const QHostAddress& address, quint16 port, const QHash<QString, qulonglong>& remoteVector);
        void handleDatagram(const QVariantMap& payload, const QHostAddress& senderAddress, quint16 senderPort);
        void handleHello(const QVariantMap& payload, const QHostAddress& senderAddress, quint16 senderPort, bool isReply);
        void handleChat(const QVariantMap& payload, const QString& senderId);
        void handleAck(const QVariantMap& payload, const QString& senderId);
        void handleSummary(const QVariantMap& payload, const QString& senderId);
        void sendAck(const QString& senderId, const QString& origin, qulonglong seq);

        void sendSummary(const QString& peerId, bool isReply);
        void forwardBroadcast(const QString& excludePeerId, const QVariantMap& message);
        void sendChatToPeer(const QString& peerId, const QVariantMap& message);

        void storeMessage(const QVariantMap& message);
        QVariantMap vectorClockPayload() const;
        QStringList peerIdList() const;
        void updatePeerDirectory();
        void writeDatagram(const QVariantMap& payload, const QHostAddress& address, quint16 port);
        bool decodeDatagram(const QByteArray& data, QVariantMap& out) const;
        void trackPending(const QString& peerId, const QVariantMap& payload);
    };
}

#endif // SIMPLECHAT_UDPPEERTRANSPORT_H
