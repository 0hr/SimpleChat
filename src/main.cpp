#include "QApplication"
#include "Core/RingTcpTransport.h"
#include "UI/SimpleChatMainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QString id;
    QString peers;
    QString testMessage;
    QString testPeer;
    quint16 port = 0;
    quint16 nextPort = 0;
    quint16 testWaitTime = 0;
    quint16 testCount = 0;

    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i + 1 < args.size(); ++i) {
        if (args[i] == "--id") { id = args[i+1]; }
        if (args[i] == "--peers") { peers = args[i+1]; }
        if (args[i] == "--port") { port = args[i+1].toUShort(); }
        if (args[i] == "--next") { nextPort = args[i+1].toUShort(); }
        if (args[i] == "--test_message") { testMessage = args[i+1]; }
        if (args[i] == "--test_peer") { testPeer = args[i+1]; }
        if (args[i] == "--test_wait_time") { testWaitTime = args[i+1].toUShort(); }
        if (args[i] == "--test_count") { testCount = args[i+1].toUShort(); }
    }

    auto* transport = new Core::RingTcpTransport();

    auto* controller = new Controllers::ChatController(transport);

    SimpleChatMainWindow simpleChatWindow(controller, nullptr);

    QObject::connect(controller, &Controllers::ChatController::logLine, &simpleChatWindow, &SimpleChatMainWindow::appendLine);
    QObject::connect(controller, &Controllers::ChatController::logLineWithTitle, &simpleChatWindow, &SimpleChatMainWindow::appendLineWithTitle);

    simpleChatWindow.show();

    qDebug() << "id:" + id << ", port: "<< port << ", next port: "<< nextPort;
    if (!id.isNull() && port > 0 && nextPort > 0) {
        simpleChatWindow.connectToPeer(id, port, nextPort, peers);
    }

    qDebug() << "test_message:" + testMessage << ", test_peer: "<< testPeer << ", test_wait_time: "<< testWaitTime << ", test_count: "<< testCount;
    if (!testMessage.isNull() && testWaitTime > 0 && testCount > 0 && !testPeer.isNull()) {
        QTimer::singleShot(testWaitTime, &simpleChatWindow, [&simpleChatWindow, &testPeer, testMessage, testCount]{
            simpleChatWindow.sendTestMessage(testPeer, testMessage, testCount);
        });
    }

    return app.exec();
}
