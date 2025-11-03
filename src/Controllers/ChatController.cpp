#include "ChatController.h"

#include <QHostAddress>
#include <QHostInfo>

namespace Controllers {

    ChatController::ChatController(Core::IChatTransport* transport, QObject *parent)
        : QObject(parent), transport(transport) {
        connect(transport, &Core::IChatTransport::messageReceived, this, &ChatController::onMessage);
        connect(transport, &Core::IChatTransport::connected, this, &ChatController::onTransportConnected);
        connect(transport, &Core::IChatTransport::errorOccurred, this, &ChatController::onTransportError);
        connect(transport, &Core::IChatTransport::peersChanged, this, &ChatController::onPeersChanged);
        connect(transport, &Core::IChatTransport::routesChanged, this, &ChatController::onRoutesChanged);
        connect(transport, &Core::IChatTransport::routesDetailedChanged, this, &ChatController::onRoutesDetailedChanged);
    }

    void ChatController::onTransportConnected() {
        emit logLine(QStringLiteral("UDP transport ready."));
    }

    void ChatController::onTransportError(const QString &err) {
        emit logLine(QStringLiteral("Transport error: ") + err);
    }

    void ChatController::setId(const QString &myId) {
        this->myId = myId;
    }

    Core::IChatTransport * ChatController::getTransport() const {
        return this->transport;
    }

    void ChatController::sendChat(const QString &destination, const QString &text) {
        if (destination.isEmpty() || text.isEmpty()) return;

        nextSeq += 1;
        QVariantMap m;
        m["ChatText"] = text;
        m["Origin"] = myId;
        m["Destination"] = destination;
        m["Seq"] = nextSeq;
        m["Type"] = QStringLiteral("Chat");

        // emit logLine(QString("Me (%1) → %2 [#%3]: %4").arg(myId, destination, QString::number(nextSeq), text));
        emit logLineWithTitle(QString("Me (%1) → %2 [#%3]: ").arg(myId, destination, QString::number(nextSeq)), text);

        seen.insert(key(myId, nextSeq));
        forward(m);
    }

    void ChatController::onMessage(const QVariantMap &msg) {
        const QString origin = msg.value("Origin").toString();
        const QString dest = msg.value("Destination").toString();
        const qulonglong seq = msg.value("Seq").toULongLong();
        const QString k = key(origin, seq);

        if (seen.contains(k)) return;
        seen.insert(k);

        if (dest == myId || dest == QStringLiteral("-1")) {
            deliverOrBuffer(msg);
        }
    }

    void ChatController::deliverOrBuffer(const QVariantMap &msg) {
        const QString origin = msg.value("Origin").toString();
        const qulonglong seq = msg.value("Seq").toULongLong();
        expectedSeq[origin] = seq;
        inboxBuffer[origin].insert(seq, msg);
        tryDeliver(origin);
    }

    void ChatController::tryDeliver(const QString &origin) {
        qulonglong &expect = expectedSeq[origin];
        auto &buf = inboxBuffer[origin];
        while (buf.contains(expect)) {
            QVariantMap msg = buf.take(expect);
            const QString text = msg.value("ChatText").toString();
            // emit logLine(QString("From %1 [#%2]: %3").arg(origin).arg(expect).arg(text));
            emit logLineWithTitle(QString("From %1 [#%2]: ").arg(origin).arg(expect), text);
        }
    }

    void ChatController::requestPeer(const QString &host, quint16 port) {
        if (!transport) return;
        if (port == 0) {
            emit logLine(QStringLiteral("Cannot add peer on port 0."));
            return;
        }

        QHostAddress address;
        if (address.setAddress(host)) {
            transport->addPeer(address, port);
            emit logLine(QStringLiteral("Attempting to reach peer %1:%2").arg(address.toString(), QString::number(port)));
            return;
        }

        QHostInfo::lookupHost(host, this, [this, port, host](const QHostInfo& info) {
            if (info.error() != QHostInfo::NoError) {
                emit logLine(QStringLiteral("DNS lookup failed for %1: %2").arg(host, info.errorString()));
                return;
            }
            if (info.addresses().isEmpty()) {
                emit logLine(QStringLiteral("No addresses resolved for %1").arg(host));
                return;
            }
            const QHostAddress resolved = info.addresses().first();
            transport->addPeer(resolved, port);
            emit logLine(QStringLiteral("Attempting to reach peer %1:%2").arg(resolved.toString(), QString::number(port)));
        });
    }

    void ChatController::onPeersChanged(const QStringList &peerIds) {
        latestPeers = peerIds;
        emitMergedDestinations();
    }

    void ChatController::onRoutesChanged(const QStringList &destinationIds) {
        latestDestinations = destinationIds;
        emitMergedDestinations();
    }

    void ChatController::emitMergedDestinations() {
        QSet<QString> merged;
        for (const auto &p : latestPeers) merged.insert(p);
        for (const auto &d : latestDestinations) merged.insert(d);
        QStringList out = merged.values();
        out.sort();
        emit peersUpdated(out);
    }

    void ChatController::onRoutesDetailedChanged(const QList<QVariantMap> &routes) {
        emit routesDetailedUpdated(routes);
    }
} // Controllers
