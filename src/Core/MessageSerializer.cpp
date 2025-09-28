#include "MessageSerializer.h"

namespace Core {
    QByteArray MessageSerializer::serialize(const QVariantMap& map) {
        QByteArray payload;
        QDataStream ds(&payload, QIODevice::WriteOnly);
        ds.setVersion(QDataStream::Qt_6_0);
        ds << map;

        QByteArray block;
        QDataStream bs(&block, QIODevice::WriteOnly);
        bs.setVersion(QDataStream::Qt_6_0);
        bs << quint32(payload.size());
        block.append(payload);

        return block;
    }

    bool MessageSerializer::unserialize(QByteArray &buffer, quint32 &expected, QVariantMap &out) {
        if (expected == 0) {
            if (buffer.size() < static_cast<int>(sizeof(quint32))) return false;
            QDataStream s(&buffer, QIODevice::ReadOnly);
            s.setVersion(QDataStream::Qt_6_0);
            quint32 sz = 0; s >> sz;
            buffer.remove(0, sizeof(quint32));
            expected = sz;
        }
        if (buffer.size() < static_cast<int>(expected)) return false;
        QByteArray payload = buffer.left(expected);
        buffer.remove(0, expected);
        expected = 0;
        QDataStream ps(&payload, QIODevice::ReadOnly);
        ps.setVersion(QDataStream::Qt_6_0);
        QVariantMap m; ps >> m; out = m;
        return true;
    }
}