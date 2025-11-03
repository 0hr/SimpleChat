#include "UdpPeerTransport.h"

#include <QDateTime>
#include <QVariantList>
#include <QRandomGenerator>


namespace Core {
    constexpr int kResendIntervalMs = 1500;
    constexpr int kMaxResendAttempts = 5;
    constexpr int kAntiEntropyIntervalMs = 2000;
    constexpr int kDiscoveryRadius = 10;
    constexpr int kDiscoveryMultiplier = 5;
    constexpr int kDiscoveryTimes = 10;
    constexpr int kDiscoveryIntervalMs = 200;
    constexpr int kRouteRumorIntervalMs = 6000;

    Core::IChatTransport::Peer parsePeerString(const QString& entry) {
        const QStringList idSplit = entry.split('@');
        QString hostPort = entry;
        if (idSplit.size() == 2) {
            hostPort = idSplit[1];
        }
        const QStringList parts = hostPort.split(':');
        if (parts.size() != 2) {
            return {QHostAddress(), 0};
        }
        QHostAddress address(parts[0]);
        bool ok = false;
        quint16 port = parts[1].toUShort(&ok);
        if (!ok || address.isNull()) {
            return {QHostAddress(), 0};
        }
        return {address, port};
    }

    UdpPeerTransport::UdpPeerTransport(QObject* parent) : IChatTransport(parent), socket(this) {
        connect(&socket, &QUdpSocket::readyRead, this, &UdpPeerTransport::onReadyRead);
        resendTimer.setInterval(kResendIntervalMs);
        resendTimer.setSingleShot(false);
        connect(&resendTimer, &QTimer::timeout, this, &UdpPeerTransport::onResendTimeout);

        antiEntropyTimer.setInterval(kAntiEntropyIntervalMs);
        antiEntropyTimer.setSingleShot(false);
        connect(&antiEntropyTimer, &QTimer::timeout, this, &UdpPeerTransport::onAntiEntropyTimeout);
        discoveryTimer.setInterval(kDiscoveryIntervalMs);
        discoveryTimer.setSingleShot(false);
        connect(&discoveryTimer, &QTimer::timeout, this, &UdpPeerTransport::processDiscoveryQueue);

        routeRumorTimer.setInterval(kRouteRumorIntervalMs);
        routeRumorTimer.setSingleShot(false);
        connect(&routeRumorTimer, &QTimer::timeout, this, &UdpPeerTransport::onRouteRumorTimeout);
    }

    void UdpPeerTransport::start(const QHostAddress& address, const QString& id, quint16 port, const QList<Peer>& initialPeers) {
        stop();

        bindAddress = address;
        myId = id;
        myPort = port;

        if (myId.isEmpty()) {
            emit errorOccurred(QStringLiteral("Cannot start transport without id."));
            return;
        }
        if (myPort == 0) {
            emit errorOccurred(QStringLiteral("Cannot start transport without port."));
            return;
        }

        QHostAddress bind = bindAddress;
        if (bind == QHostAddress::AnyIPv6) {
            bind = QHostAddress::Any;
        }
        if (bind.isNull()) {
            bind = QHostAddress::Any;
        }

        if (socket.state() != QAbstractSocket::UnconnectedState) {
            socket.abort();
            socket.close();
        }

        QAbstractSocket::BindMode bindMode = QAbstractSocket::ShareAddress | QAbstractSocket::ReuseAddressHint;
        if (!socket.bind(bind, myPort, bindMode)) {
            emit errorOccurred(QStringLiteral("Failed to bind UDP socket on %1:%2 -> %3")
                                   .arg(bind.toString(),
                                        QString::number(myPort),
                                        socket.errorString()));
            myId.clear();
            myPort = 0;
            bindAddress = QHostAddress();
            return;
        }

        running = true;
        localVectorClock.clear();
        localVectorClock.insert(myId, 0);
        historyByKey.clear();
        messageReceivers.clear();
        pendingAcks.clear();
        peersById.clear();
        endpointToId.clear();
        pendingEndpoints.clear();
        discoveryQueue.clear();
        queuedEndpoints.clear();
        discoveryTimer.stop();

        routes.clear();
        routeSeqByOrigin.clear();
        myRouteSeqNo = 0;

        resendTimer.start();
        antiEntropyTimer.start();
        routeRumorTimer.start();

        emit connected();
        emit peersChanged(QStringList());
        emit routesChanged(QStringList());
        emit routesDetailedChanged(QList<QVariantMap>());

        for (const Peer& peer : initialPeers) {
            if (peer.second == 0 || peer.first.isNull()) continue;
            addPeer(peer.first, peer.second);
        }
        if (initialPeers.isEmpty()) {
            discoverLocalPeers();
        }

        // Send an initial route rumor..
        QTimer::singleShot(500, this, [this]() { sendRouteRumor(true); });
    }

    void UdpPeerTransport::stop() {
        running = false;

        socket.abort();
        if (socket.isOpen()) {
            socket.close();
        }

        resendTimer.stop();
        antiEntropyTimer.stop();
        discoveryTimer.stop();
        routeRumorTimer.stop();

        pendingAcks.clear();
        peersById.clear();
        endpointToId.clear();
        historyByKey.clear();
        messageReceivers.clear();
        localVectorClock.clear();
        pendingEndpoints.clear();
        discoveryQueue.clear();
        queuedEndpoints.clear();
        myId.clear();
        myPort = 0;
        bindAddress = QHostAddress();

        emit peersChanged(QStringList());
        emit routesChanged(QStringList());
        emit routesDetailedChanged(QList<QVariantMap>());
    }

    void UdpPeerTransport::send(const QVariantMap& map) {
        if (!running) return;
        QVariantMap message = map;
        if (message.value(QStringLiteral("Type")).toString().isEmpty()) {
            message.insert(QStringLiteral("Type"), QStringLiteral("Chat"));
        }
        if (!message.contains(QStringLiteral("Origin"))) {
            message.insert(QStringLiteral("Origin"), myId);
        }
        const QString origin = message.value(QStringLiteral("Origin")).toString();
        QString destination = message.value(QStringLiteral("Destination")).toString();
        if (destination.isEmpty()) {
            destination = message.value(QStringLiteral("Dest")).toString();
            if (!destination.isEmpty()) message.insert(QStringLiteral("Destination"), destination);
        }
        const qulonglong seq = message.value(QStringLiteral("Seq")).toULongLong();
        if (!message.contains(QStringLiteral("HopLimit"))) {
            message.insert(QStringLiteral("HopLimit"), 10u);
        }

        if (origin != myId) {
            message.insert(QStringLiteral("Origin"), myId);
        }
        if (seq == 0) {
            emit errorOccurred(QStringLiteral("Message missing sequence number."));
            return;
        }

        storeMessage(message);

        if (destination == myId) {
            emit messageReceived(message);
            return;
        }

        if (destination == QStringLiteral("-1")) {
            for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
                sendChatToPeer(it.key(), message);
            }
        } else {
            QString nextHopId = destination;
            if (!peersById.contains(destination)) {
                if (routes.contains(destination)) {
                    nextHopId = routes.value(destination).nextHopId;
                } else {
                    emit errorOccurred(QStringLiteral("No route to '%1'.").arg(destination));
                    return;
                }
            }
            sendChatToPeer(nextHopId, message);
        }
    }

    void UdpPeerTransport::addPeer(const QHostAddress& address, quint16 port) {
        if (!running) return;
        if (address.isNull() || port == 0) return;
        if (port == myPort && (address == bindAddress || address.isLoopback())) return;

        const QString key = endpointKey(address, port);
        if (endpointToId.contains(key)) return;
        if (pendingEndpoints.contains(key)) return;

        pendingEndpoints.insert(key);
        sendHello(address, port, false);
    }

    void UdpPeerTransport::discoverLocalPeers() {
        if (!running) return;
        if (myPort == 0) return;

        for (int step = 0; step < kDiscoveryTimes; ++step) {
            constexpr int curr = kDiscoveryRadius * kDiscoveryTimes * kDiscoveryMultiplier;
            int prev = 0;
            if (step == 0) {
                prev = 0;
            } else {
                prev = kDiscoveryRadius * kDiscoveryMultiplier * step;
            }
            for (int offset = prev + 1; offset <= curr; ++offset) {
                int candidate = static_cast<int>(myPort) + offset;
                if (candidate <= 0 || candidate > 65535) continue;
                enqueueDiscovery(QHostAddress::LocalHost, static_cast<quint16>(candidate));
                candidate = static_cast<int>(myPort) - offset;
                if (candidate <= 0 || candidate > 65535) continue;
                enqueueDiscovery(QHostAddress::LocalHost, static_cast<quint16>(candidate));
            }
        }
        if (!discoveryQueue.isEmpty() && !discoveryTimer.isActive()) {
            QMetaObject::invokeMethod(this, &UdpPeerTransport::processDiscoveryQueue, Qt::QueuedConnection);
            discoveryTimer.start();
        }
    }

    void UdpPeerTransport::enqueueDiscovery(const QHostAddress& address, quint16 port) {
        if (!running) return;
        if (address.isNull() || port == 0) return;
        if (port == myPort && (address == bindAddress || address.isLoopback())) return;

        const QString key = endpointKey(address, port);
        if (endpointToId.contains(key)) return;
        if (pendingEndpoints.contains(key)) return;
        if (queuedEndpoints.contains(key)) return;

        discoveryQueue.enqueue(Peer(address, port));
        queuedEndpoints.insert(key);
        if (!discoveryTimer.isActive()) {
            QMetaObject::invokeMethod(this, &UdpPeerTransport::processDiscoveryQueue, Qt::QueuedConnection);
            discoveryTimer.start();
        }
    }

    void UdpPeerTransport::processDiscoveryQueue() {
        if (!running) {
            discoveryQueue.clear();
            queuedEndpoints.clear();
            discoveryTimer.stop();
            return;
        }
        if (discoveryQueue.isEmpty()) {
            discoveryTimer.stop();
            return;
        }

        const Peer peer = discoveryQueue.dequeue();
        const QString key = endpointKey(peer.first, peer.second);
        queuedEndpoints.remove(key);
        addPeer(peer.first, peer.second);

        if (discoveryQueue.isEmpty()) {
            discoveryTimer.stop();
        }
    }

    void UdpPeerTransport::sendHello(const QHostAddress& address, quint16 port, bool isReply) {
        if (!running) return;
        if (address.isNull() || port == 0 || myId.isEmpty()) return;

        QVariantMap payload;
        payload.insert(QStringLiteral("Type"), isReply ? QStringLiteral("HelloReply") : QStringLiteral("Hello"));
        payload.insert(QStringLiteral("Origin"), myId);
        payload.insert(QStringLiteral("Sender"), myId);
        payload.insert(QStringLiteral("Port"), myPort);
        payload.insert(QStringLiteral("Vector"), vectorClockPayload());
        QStringList knownPeers;
        for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
            const PeerState& peer = it.value();
            knownPeers << QStringLiteral("%1@%2:%3").arg(peer.id,
                                                         peer.address.toString(),
                                                         QString::number(peer.port));
        }
        payload.insert(QStringLiteral("Peers"), knownPeers);

        writeDatagram(payload, address, port);
    }

    void UdpPeerTransport::onReadyRead() {
        if (!running) {
            while (socket.hasPendingDatagrams()) {
                socket.readDatagram(nullptr, 0);
            }
            return;
        }
        while (socket.hasPendingDatagrams()) {
            QByteArray buffer;
            buffer.resize(int(socket.pendingDatagramSize()));
            QHostAddress sender;
            quint16 port = 0;

            if (socket.readDatagram(buffer.data(), buffer.size(), &sender, &port) < 0) {
                continue;
            }

            QVariantMap payload;
            if (!decodeDatagram(buffer, payload)) {
                continue;
            }

            handleDatagram(payload, sender, port);
        }
    }

    void UdpPeerTransport::handleDatagram(const QVariantMap& payload,
                                          const QHostAddress& senderAddress,
                                          quint16 senderPort) {
        if (!running) return;
        const QString type = payload.value(QStringLiteral("Type")).toString();
        QString senderId = payload.value(QStringLiteral("Sender")).toString();
        if (senderId.isEmpty()) {
            senderId = payload.value(QStringLiteral("Origin")).toString();
        }

        const quint16 claimedPort = payload.value(QStringLiteral("Port")).toUInt();
        const quint16 peerPort = claimedPort != 0 ? claimedPort : senderPort;

        if ((senderId.isEmpty() || !peersById.contains(senderId))) {
            const QString endKey = endpointKey(senderAddress, peerPort);
            const QString mappedId = endpointToId.value(endKey);
            if (!mappedId.isEmpty()) {
                senderId = mappedId;
            }
        }

        if (senderId == myId) {
            return;
        }

        const QVariantMap vectorVariant = payload.value(QStringLiteral("Vector")).toMap();
        QHash<QString, qulonglong> remoteVector;
        for (auto it = vectorVariant.constBegin(); it != vectorVariant.constEnd(); ++it) {
            remoteVector.insert(it.key(), it.value().toULongLong());
        }

        if (!senderId.isEmpty()) {
            addOrUpdatePeer(senderId, senderAddress, peerPort, remoteVector);
        }

        if (type == QStringLiteral("Hello")) {
            handleHello(payload, senderAddress, peerPort, false);
            return;
        }
        if (type == QStringLiteral("HelloReply")) {
            handleHello(payload, senderAddress, peerPort, true);
            return;
        }
        if (type == QStringLiteral("Ack")) {
            handleAck(payload, senderId, senderAddress, peerPort);
            return;
        }
        if (type == QStringLiteral("Summary")) {
            handleSummary(payload, senderId);
            return;
        }
        if (type == QStringLiteral("Rumor") || type == QStringLiteral("RouteRumor")) {
            handleRumor(payload, senderId, senderAddress, senderPort);
            return;
        }
        if (type == QStringLiteral("Chat")) {
            handleChat(payload, senderId);
            return;
        }
    }

    void UdpPeerTransport::handleHello(const QVariantMap& payload, const QHostAddress& senderAddress, quint16 senderPort, bool isReply) {
        if (!running) return;
        Q_UNUSED(isReply);
        QStringList peers = payload.value(QStringLiteral("Peers")).toStringList();
        for (const QString& entry : peers) {
            const Peer candidate = parsePeerString(entry);
            if (candidate.second != 0 && !candidate.first.isNull() && candidate.second != myPort) {
                const QString key = endpointKey(candidate.first, candidate.second);
                if (key == endpointKey(senderAddress, senderPort)) {
                    continue;
                }
                enqueueDiscovery(candidate.first, candidate.second);
            }
        }

        if (!isReply) {
            sendHello(senderAddress, senderPort, true);
        }
    }

    void UdpPeerTransport::handleChat(const QVariantMap& payload, const QString& senderId) {
        if (!running) return;
        const QString origin = payload.value(QStringLiteral("Origin")).toString();
        const qulonglong seq = payload.value(QStringLiteral("Seq")).toULongLong();
        if (origin.isEmpty() || seq == 0) return;

        const QString key = messageKey(origin, seq);
        sendAck(senderId, origin, seq);

        auto peerIt = peersById.find(senderId);
        if (peerIt != peersById.end()) {
            peerIt->vectorClock[origin] = qMax(peerIt->vectorClock.value(origin, 0ULL), seq);
        }

        bool isNew = !historyByKey.contains(key);
        if (isNew) {
            storeMessage(payload);
        }

        messageReceivers[key].insert(senderId);

        QString destination = payload.value(QStringLiteral("Destination")).toString();
        if (destination.isEmpty()) destination = payload.value(QStringLiteral("Dest")).toString();
        if (isNew && (destination == myId || destination == QStringLiteral("-1"))) {
            emit messageReceived(payload);
        }

        if (isNew && destination == QStringLiteral("-1")) {
            forwardBroadcast(senderId, payload);
            return;
        }

        // Forward messages according to DSDV routing
        if (isNew && !destination.isEmpty() && destination != myId && destination != QStringLiteral("-1")) {
            forwardDirect(senderId, payload);
        }
    }

    void UdpPeerTransport::forwardBroadcast(const QString& excludePeerId, const QVariantMap& message) {
        if (!running) return;
        if (noForwardMode) return;
        const QString origin = message.value(QStringLiteral("Origin")).toString();
        const qulonglong seq = message.value(QStringLiteral("Seq")).toULongLong();
        const QString key = messageKey(origin, seq);
        for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
            const QString& peerId = it.key();
            if (peerId == excludePeerId || peerId == myId) continue;
            if (messageReceivers[key].contains(peerId)) continue;
            sendChatToPeer(peerId, message);
        }
    }

    void UdpPeerTransport::forwardDirect(const QString& excludePeerId, QVariantMap message) {
        if (!running) return;
        if (noForwardMode) return;
        const QString dest = message.value(QStringLiteral("Destination")).toString();
        if (dest.isEmpty() || dest == myId) return;

        quint32 hopLimit = message.value(QStringLiteral("HopLimit")).toUInt();
        if (hopLimit == 0) return;
        message.insert(QStringLiteral("HopLimit"), hopLimit - 1);

        QString nextHopId;
        if (peersById.contains(dest)) {
            nextHopId = dest; // direct neighbor
        } else if (routes.contains(dest)) {
            nextHopId = routes.value(dest).nextHopId;
        }
        if (nextHopId.isEmpty() || nextHopId == excludePeerId) return;

        sendChatToPeer(nextHopId, message);
    }

    void UdpPeerTransport::handleAck(const QVariantMap& payload, const QString& senderId, const QHostAddress& senderAddress, quint16 senderPort) {
        if (!running) return;
        const QString ackOrigin = payload.value(QStringLiteral("AckOrigin")).toString();
        const qulonglong seq = payload.value(QStringLiteral("Seq")).toULongLong();
        if (ackOrigin.isEmpty() || seq == 0) return;

        QString key = pendingKey(senderId, ackOrigin, seq);
        auto it = pendingAcks.find(key);
        if (it == pendingAcks.end()) {
            const QString endKey = endpointKey(senderAddress, senderPort);
            const QString mappedId = endpointToId.value(endKey);
            if (!mappedId.isEmpty() && mappedId != senderId) {
                key = pendingKey(mappedId, ackOrigin, seq);
                it = pendingAcks.find(key);
            }
        }
        if (it == pendingAcks.end()) {
            for (auto scan = pendingAcks.begin(); scan != pendingAcks.end(); ++scan) {
                if (scan->origin == ackOrigin && scan->seq == seq) { it = scan; key = scan.key(); break; }
            }
        }
        if (it == pendingAcks.end()) {
            return;
        }

        const QString messageKeyId = messageKey(ackOrigin, seq);
        messageReceivers[messageKeyId].insert(senderId);

        auto peerIt = peersById.find(senderId);
        if (peerIt != peersById.end()) {
            peerIt->vectorClock[ackOrigin] = qMax(peerIt->vectorClock.value(ackOrigin, 0ULL), seq);
            peerIt->pendingKeys.remove(key);
        }

        pendingAcks.erase(it);
    }

    void UdpPeerTransport::handleSummary(const QVariantMap& payload, const QString& senderId) {
        if (!running) return;
        auto peerIt = peersById.find(senderId);
        if (peerIt == peersById.end()) {
            return;
        }

        const QVariantMap vectorMap = payload.value(QStringLiteral("Vector")).toMap();
        for (auto it = vectorMap.constBegin(); it != vectorMap.constEnd(); ++it) {
            peerIt->vectorClock.insert(it.key(), it.value().toULongLong());
        }

        const bool isReply = payload.value(QStringLiteral("Reply")).toBool();

        for (auto clockIt = localVectorClock.constBegin(); clockIt != localVectorClock.constEnd(); ++clockIt) {
            const QString& origin = clockIt.key();
            const qulonglong haveSeq = clockIt.value();
            const qulonglong remoteSeq = peerIt->vectorClock.value(origin, 0ULL);
            if (haveSeq <= remoteSeq) continue;
            for (qulonglong seq = remoteSeq + 1; seq <= haveSeq; ++seq) {
                const QString key = messageKey(origin, seq);
                if (!historyByKey.contains(key)) continue;
                if (messageReceivers[key].contains(senderId)) continue;
                sendChatToPeer(senderId, historyByKey.value(key));
            }
        }

        if (!isReply) {
            sendSummary(senderId, true);
        }
    }

    void UdpPeerTransport::onRouteRumorTimeout() {
        if (!running) return;
        sendRouteRumor(false);
    }

    void UdpPeerTransport::sendRouteRumor(bool initialFanout) {
        if (!running) return;

        myRouteSeqNo += 1;

        QVariantMap rumor;
        rumor.insert(QStringLiteral("Type"), QStringLiteral("Rumor"));
        rumor.insert(QStringLiteral("Origin"), myId);
        rumor.insert(QStringLiteral("Sender"), myId);
        rumor.insert(QStringLiteral("SeqNo"), QVariant::fromValue(myRouteSeqNo));
        rumor.insert(QStringLiteral("Port"), myPort);
        rumor.insert(QStringLiteral("HopLimit"), 10u);

        if (initialFanout) {
            for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {

        QHostAddress last = bindAddress;
        if (last.isNull() || last == QHostAddress::Any || last == QHostAddress::AnyIPv6) last = QHostAddress::LocalHost;
        rumor.insert(QStringLiteral("LastIP"), last.toString());
        rumor.insert(QStringLiteral("LastPort"), QVariant::fromValue(myPort));
        writeDatagram(rumor, it->address, it->port);
            }
        } else {
            const QString neighbor = chooseRandomNeighbor();
            if (!neighbor.isEmpty()) {
                const PeerState& st = peersById.value(neighbor);
                QHostAddress last2 = bindAddress;
                if (last2.isNull() || last2 == QHostAddress::Any || last2 == QHostAddress::AnyIPv6) last2 = QHostAddress::LocalHost;
                rumor.insert(QStringLiteral("LastIP"), last2.toString());
                rumor.insert(QStringLiteral("LastPort"), QVariant::fromValue(myPort));
                writeDatagram(rumor, st.address, st.port);
            }
        }
    }

    void UdpPeerTransport::forwardRumorRandomNeighbor(const QString& excludePeerId, QVariantMap payload) {
        if (!running) return;
        quint32 hop = payload.value(QStringLiteral("HopLimit")).toUInt();
        if (hop == 0) return;
        payload.insert(QStringLiteral("HopLimit"), hop - 1);
        payload.insert(QStringLiteral("Sender"), myId);

        const QString neighbor = chooseRandomNeighbor(excludePeerId);
        if (neighbor.isEmpty()) return;
        const PeerState& st = peersById.value(neighbor);
        writeDatagram(payload, st.address, st.port);
    }

    QString UdpPeerTransport::chooseRandomNeighbor(const QString& excludePeerId) const {
        if (peersById.isEmpty()) return QString();
        QStringList ids;
        ids.reserve(peersById.size());
        for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
            if (!excludePeerId.isEmpty() && it.key() == excludePeerId) continue;
            if (it.key() == myId) continue;
            ids.append(it.key());
        }
        if (ids.isEmpty()) return QString();
        const int idx = QRandomGenerator::global()->bounded(ids.size());
        return ids.at(idx);
    }

    void UdpPeerTransport::handleRumor(const QVariantMap& payload, const QString& senderId, const QHostAddress& senderAddress, quint16 senderPort) {
        if (!running) return;
        const QString origin = payload.value(QStringLiteral("Origin")).toString();
        const qulonglong seqNo = payload.value(QStringLiteral("SeqNo")).toULongLong();
        if (origin.isEmpty() || seqNo == 0) return;


        QVariantMap forwarded = payload;
        forwarded.insert(QStringLiteral("LastIP"), senderAddress.toString());
        forwarded.insert(QStringLiteral("LastPort"), QVariant::fromValue(senderPort));

        bool shouldUpdate = false;
        const qulonglong known = routeSeqByOrigin.value(origin, 0ULL);
        if (seqNo > known) {
            shouldUpdate = true;
        }




        RouteEntry candidate;
        candidate.destinationId = origin;
        candidate.nextHopId = senderId;
        candidate.nextHopAddress = senderAddress;
        candidate.nextHopPort = senderPort;
        candidate.seqNo = seqNo;
        candidate.isDirect = (senderId == origin);
        candidate.lastUpdatedMs = currentMs();
        const QHostAddress advAddr(payload.value(QStringLiteral("LastIP")).toString());
        const quint16 advPort = payload.value(QStringLiteral("LastPort")).toUInt();
        if (!advAddr.isNull() && advPort != 0) {
            candidate.advertisedAddress = advAddr;
            candidate.advertisedPort = advPort;
        }

        auto itOld = routes.find(origin);
        if (itOld == routes.end()) {
            if (shouldUpdate) {
                routes.insert(origin, candidate);
                routeSeqByOrigin.insert(origin, seqNo);
                updateRoutesDirectory();
            }
        } else {
            if (shouldUpdate || isBetterRoute(itOld.value(), candidate)) {
                itOld.value() = candidate;
                routeSeqByOrigin.insert(origin, qMax(seqNo, known));
                updateRoutesDirectory();
            }
        }

        if (!candidate.advertisedAddress.isNull() && candidate.advertisedPort != 0) {
            addPeer(candidate.advertisedAddress, candidate.advertisedPort);
        }

        forwardRumorRandomNeighbor(senderId, forwarded);
    }

    void UdpPeerTransport::onResendTimeout() {
        if (!running) return;
        const qint64 now = currentMs();
        QList<QString> toRemove;

        for (auto it = pendingAcks.begin(); it != pendingAcks.end(); ++it) {
            PendingAck& pending = it.value();
            if (!peersById.contains(pending.peerId)) {
                toRemove.append(it.key());
                continue;
            }
            if (messageReceivers[messageKey(pending.origin, pending.seq)].contains(pending.peerId)) {
                toRemove.append(it.key());
                continue;
            }

            if (now - pending.lastSent < kResendIntervalMs) {
                continue;
            }

            if (pending.attempts >= kMaxResendAttempts) {
                emit errorOccurred(QStringLiteral("No acknowledgement from %1 for %2#%3")
                                       .arg(pending.peerId,
                                            pending.origin,
                                            QString::number(pending.seq)));
                toRemove.append(it.key());
                continue;
            }

            const auto peerIt = peersById.find(pending.peerId);
            if (peerIt == peersById.end()) {
                toRemove.append(it.key());
                continue;
            }

            writeDatagram(pending.payload, peerIt->address, peerIt->port);
            pending.lastSent = now;
            pending.attempts += 1;
        }

        for (const QString& key : toRemove) {
            PendingAck pending = pendingAcks.take(key);
            auto peerIt = peersById.find(pending.peerId);
            if (peerIt != peersById.end()) {
                peerIt->pendingKeys.remove(key);
            }
        }
    }

    void UdpPeerTransport::onAntiEntropyTimeout() {
        if (!running) return;
        for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
            sendSummary(it.key(), false);
        }
    }

    void UdpPeerTransport::sendSummary(const QString& peerId, bool isReply) {
        if (!running) return;
        auto it = peersById.find(peerId);
        if (it == peersById.end()) return;

        QVariantMap payload;
        payload.insert(QStringLiteral("Type"), QStringLiteral("Summary"));
        payload.insert(QStringLiteral("Origin"), myId);
        payload.insert(QStringLiteral("Sender"), myId);
        payload.insert(QStringLiteral("Destination"), peerId);
        payload.insert(QStringLiteral("Vector"), vectorClockPayload());
        payload.insert(QStringLiteral("Reply"), isReply);
        payload.insert(QStringLiteral("Port"), myPort);

        writeDatagram(payload, it->address, it->port);
    }

    void UdpPeerTransport::sendChatToPeer(const QString& peerId, const QVariantMap& message) {
        if (!running) return;

        if (noForwardMode && message.value(QStringLiteral("Origin")).toString() != myId) return;
        auto it = peersById.find(peerId);
        if (it == peersById.end()) return;

        const QString origin = message.value(QStringLiteral("Origin")).toString();
        const qulonglong seq = message.value(QStringLiteral("Seq")).toULongLong();
        if (origin.isEmpty() || seq == 0) return;

        const QString key = messageKey(origin, seq);
        if (messageReceivers[key].contains(peerId)) {
            return;
        }

        QVariantMap payload = message;
        payload.insert(QStringLiteral("Type"), QStringLiteral("Chat"));
        payload.insert(QStringLiteral("Sender"), myId);
        payload.insert(QStringLiteral("Recipient"), peerId);
        payload.insert(QStringLiteral("Port"), myPort);

        writeDatagram(payload, it->address, it->port);
        trackPending(peerId, payload);
    }

    void UdpPeerTransport::sendAck(const QString& senderId, const QString& origin, qulonglong seq) {
        if (!running) return;
        auto it = peersById.find(senderId);
        if (it == peersById.end()) return;

        QVariantMap ack;
        ack.insert(QStringLiteral("Type"), QStringLiteral("Ack"));
        ack.insert(QStringLiteral("Origin"), myId);
        ack.insert(QStringLiteral("Sender"), myId);
        ack.insert(QStringLiteral("Destination"), senderId);
        ack.insert(QStringLiteral("AckOrigin"), origin);
        ack.insert(QStringLiteral("Seq"), seq);
        ack.insert(QStringLiteral("Port"), myPort);

        writeDatagram(ack, it->address, it->port);
    }

    void UdpPeerTransport::storeMessage(const QVariantMap& message) {
        if (!running) return;
        const QString origin = message.value(QStringLiteral("Origin")).toString();
        const qulonglong seq = message.value(QStringLiteral("Seq")).toULongLong();
        if (origin.isEmpty() || seq == 0) return;

        const QString key = messageKey(origin, seq);
        if (!historyByKey.contains(key)) {
            QVariantMap copy = message;
            copy.insert(QStringLiteral("Type"), QStringLiteral("Chat"));
            historyByKey.insert(key, copy);
        }

        localVectorClock[origin] = qMax(localVectorClock.value(origin, 0ULL), seq);
        messageReceivers[key].insert(myId);
    }

    QVariantMap UdpPeerTransport::vectorClockPayload() const {
        QVariantMap vector;
        for (auto it = localVectorClock.constBegin(); it != localVectorClock.constEnd(); ++it) {
            vector.insert(it.key(), QVariant::fromValue(it.value()));
        }
        return vector;
    }

    QStringList UdpPeerTransport::peerIdList() const {
        QStringList ids;
        ids.reserve(peersById.size());
        for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
            ids << it.key();
        }
        ids.sort();
        return ids;
    }

    QStringList UdpPeerTransport::knownDestinationsList() const {
        QSet<QString> out;
        for (auto it = peersById.constBegin(); it != peersById.constEnd(); ++it) {
            out.insert(it.key());
        }
        for (auto it = routes.constBegin(); it != routes.constEnd(); ++it) {
            out.insert(it.key());
        }
        out.remove(myId);
        QStringList list = out.values();
        list.sort();
        return list;
    }

    void UdpPeerTransport::updatePeerDirectory() {
        emit peersChanged(peerIdList());
    }

    void UdpPeerTransport::updateRoutesDirectory() {
        emit routesChanged(knownDestinationsList());
        emit routesDetailedChanged(knownRoutesDetailedList());
    }

    bool UdpPeerTransport::isBetterRoute(const RouteEntry& oldRoute, const RouteEntry& newRoute) {
        if (newRoute.seqNo > oldRoute.seqNo) return true;
        if (newRoute.seqNo < oldRoute.seqNo) return false;

        if (newRoute.isDirect && !oldRoute.isDirect) return true;
        return false;
    }

    QList<QVariantMap> UdpPeerTransport::knownRoutesDetailedList() const {
        QList<QVariantMap> list;
        list.reserve(routes.size());
        for (auto it = routes.constBegin(); it != routes.constEnd(); ++it) {
            const RouteEntry &r = it.value();
            QVariantMap m;
            m.insert(QStringLiteral("Dest"), r.destinationId);
            m.insert(QStringLiteral("NextHop"), r.nextHopId);
            m.insert(QStringLiteral("SeqNo"), QVariant::fromValue(r.seqNo));
            m.insert(QStringLiteral("Direct"), r.isDirect);
            m.insert(QStringLiteral("LastIP"), r.advertisedAddress.toString());
            m.insert(QStringLiteral("LastPort"), QVariant::fromValue(r.advertisedPort));
            m.insert(QStringLiteral("LastUpdatedMs"), QVariant::fromValue(r.lastUpdatedMs));
            list.append(m);
        }
        return list;
    }

    void UdpPeerTransport::addOrUpdatePeer(const QString& peerId, const QHostAddress& address, quint16 port, const QHash<QString, qulonglong>& remoteVector) {
        if (peerId.isEmpty() || peerId == myId || port == 0 || address.isNull()) return;

        const QString endKey = endpointKey(address, port);
        const bool isKnown = peersById.contains(peerId);

        PeerState& state = peersById[peerId];
        state.id = peerId;
        state.address = address;
        state.port = port;
        for (auto it = remoteVector.constBegin(); it != remoteVector.constEnd(); ++it) {
            state.vectorClock.insert(it.key(), it.value());
        }

        endpointToId.insert(endKey, peerId);

        if (!isKnown) {
            updatePeerDirectory();
            QTimer::singleShot(0, this, [this]() { sendRouteRumor(true); });
        }

        const QString key = endpointKey(address, port);
        pendingEndpoints.remove(key);
        queuedEndpoints.remove(key);
    }

    void UdpPeerTransport::writeDatagram(const QVariantMap& payload, const QHostAddress& address, quint16 port) {
        if (address.isNull() || port == 0) return;
        QByteArray buffer;
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_0);
        stream << payload;
        socket.writeDatagram(buffer, address, port);
    }

    bool UdpPeerTransport::decodeDatagram(const QByteArray& data, QVariantMap& out) const {
        QDataStream stream(data);
        stream.setVersion(QDataStream::Qt_6_0);
        stream >> out;
        return stream.status() == QDataStream::Ok;
    }

    void UdpPeerTransport::trackPending(const QString& peerId, const QVariantMap& payload) {
        if (!running) return;
        const QString origin = payload.value(QStringLiteral("Origin")).toString();
        const qulonglong seq = payload.value(QStringLiteral("Seq")).toULongLong();
        if (origin.isEmpty() || seq == 0) return;

        const QString key = pendingKey(peerId, origin, seq);
        PendingAck pending;
        pending.payload = payload;
        pending.peerId = peerId;
        pending.origin = origin;
        pending.seq = seq;
        pending.lastSent = currentMs();
        pending.attempts = 1;

        pendingAcks.insert(key, pending);
        peersById[peerId].pendingKeys.insert(key);
    }

    QString UdpPeerTransport::endpointKey(const QHostAddress& address, quint16 port) {
        return QStringLiteral("%1:%2").arg(address.toString(), QString::number(port));
    }

    QString UdpPeerTransport::messageKey(const QString& origin, qulonglong seq) {
        return origin + QLatin1Char('#') + QString::number(seq);
    }

    QString UdpPeerTransport::pendingKey(const QString& peerId, const QString& origin, qulonglong seq) {
        return peerId + QLatin1Char('|') + origin + QLatin1Char('|') + QString::number(seq);
    }

    qint64 UdpPeerTransport::currentMs() {
        return QDateTime::currentMSecsSinceEpoch();
    }
}
