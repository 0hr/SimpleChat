#include "ChatController.h"

namespace Controllers {
    ChatController::ChatController(Core::IChatTransport* transport, QObject *parent)
        : QObject(parent), transport(transport) {
        connect(transport, &Core::IChatTransport::messageReceived, this, &ChatController::onMessage);
        connect(transport, &Core::IChatTransport::connected, this, &ChatController::onTransportConnected);
        connect(transport, &Core::IChatTransport::errorOccurred, this, &ChatController::onTransportError);
    }

    void ChatController::onTransportConnected() {
        emit logLine(QStringLiteral("Connected to right neighbor."));
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

        if (dest == myId) {
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
} // Controllers
