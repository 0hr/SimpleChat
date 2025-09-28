#ifndef SIMPLECHAT_MESSAGESERIALIZER_H
#define SIMPLECHAT_MESSAGESERIALIZER_H
#include <QtCore>

namespace Core {
    class MessageSerializer {
    public:
        static QByteArray serialize(const QVariantMap& map);
        static bool unserialize(QByteArray& buffer, quint32& expected, QVariantMap& out);
    };
}


#endif //SIMPLECHAT_MESSAGESERIALIZER_H