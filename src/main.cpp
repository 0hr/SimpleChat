#include "QApplication"
#include "Core/RingTcpTransport.h"
#include "UI/SimpleChatMainWindow.h"

int main(int argc, char** argv) {
    QApplication app(argc, argv);

    QString id;
    QString peers;
    quint16 port = 0;
    quint16 nextPort = 0;

    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i + 1 < args.size(); ++i) {
        if (args[i] == "--id") { id = args[i+1]; }
        if (args[i] == "--peers") { peers = args[i+1]; }
        if (args[i] == "--port") { port = args[i+1].toUShort(); }
        if (args[i] == "--next") { nextPort = args[i+1].toUShort(); }
    }

    auto* transport = new Core::RingTcpTransport();

    auto* controller = new Controllers::ChatController(transport);

    SimpleChatMainWindow simpleChatWindow(controller, nullptr);

    QObject::connect(controller, &Controllers::ChatController::logLine, &simpleChatWindow, &SimpleChatMainWindow::appendLine);
    QObject::connect(controller, &Controllers::ChatController::logLineWithTitle, &simpleChatWindow, &SimpleChatMainWindow::appendLineWithTitle);

    simpleChatWindow.show();

    qDebug() << "id:" + id << "port: "<< port << "next port: "<< nextPort;
    if (!id.isNull() && port > 0 && nextPort > 0) {
        simpleChatWindow.connectToPeer(id, port, nextPort, peers);
    }

    return app.exec();
}
