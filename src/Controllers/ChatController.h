#ifndef SIMPLECHAT_CHATCONTROLLER_H
#define SIMPLECHAT_CHATCONTROLLER_H
#include <QtCore>
#include "../Core/IChatTransport.h"

namespace Controllers {
    class ChatController : public QObject {
        Q_OBJECT
    public:
        ChatController(Core::IChatTransport* transport, QObject* parent=nullptr);
        void setId(const QString& myId);
        Core::IChatTransport* getTransport() const;

    public slots:
        void sendChat(const QString& destination, const QString& text);

    private slots:
        void onMessage(const QVariantMap& msg);
        void onTransportConnected();
        void onTransportError(const QString& err);

    signals:
        void logLine(const QString& line);
        void logLineWithTitle(const QString& title, const QString& line);

    private:
        QString myId;

        QSet<QString> seen;
        QHash<QString, qulonglong> expectedSeq;
        QHash<QString, QMap<qulonglong, QVariantMap>> inboxBuffer;
        qulonglong nextSeq = 0;

        static QString key(const QString& origin, qulonglong seq) { return origin + "#" + QString::number(seq); }
        void deliverOrBuffer(const QVariantMap& msg);
        void tryDeliver(const QString& origin);
        void forward(const QVariantMap& msg) { transport->send(msg); }
        Core::IChatTransport* transport = nullptr;
    };
} // Controllers

#endif //SIMPLECHAT_CHATCONTROLLER_H