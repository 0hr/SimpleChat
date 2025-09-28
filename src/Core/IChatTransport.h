#ifndef SIMPLECHAT_ICHATTRANSPORT_H
#define SIMPLECHAT_ICHATTRANSPORT_H
#include <QtCore>
#include <QTcpSocket>

namespace Core {
    class IChatTransport : public QObject {
        Q_OBJECT
    public:
        using QObject::QObject;
        virtual ~IChatTransport() = default;
        virtual void start(QHostAddress ipAddress, QString myId, quint16 myPort, quint16 nextPort) = 0;
        virtual void stop() = 0;
        virtual void send(const QVariantMap& map) = 0;
        signals:
            void connected();
        void errorOccurred(const QString& error);
        void messageReceived(const QVariantMap& msg);
    };
}
#endif //SIMPLECHAT_ICHATTRANSPORT_H