#ifndef SIMPLECHAT_ICHATTRANSPORT_H
#define SIMPLECHAT_ICHATTRANSPORT_H
#include <QtCore>
#include <QtNetwork>

namespace Core {
    class IChatTransport : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;
        virtual ~IChatTransport() = default;
        using Peer = QPair<QHostAddress, quint16>;
        virtual void start(const QHostAddress& bindAddress, const QString& myId, quint16 myPort, const QList<Peer>& initialPeers) = 0;
        virtual void stop() = 0;
        virtual void send(const QVariantMap& map) = 0;
        virtual void addPeer(const QHostAddress& address, quint16 port) = 0;
    signals:
        void connected();
        void errorOccurred(const QString& error);
        void messageReceived(const QVariantMap& msg);
        void peersChanged(const QStringList& peerIds);
        void routesChanged(const QStringList& destinationIds);
        void routesDetailedChanged(const QList<QVariantMap>& routes);
    };
}
#endif //SIMPLECHAT_ICHATTRANSPORT_H
