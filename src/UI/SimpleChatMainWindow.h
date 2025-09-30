#ifndef SIMPLECHAT_SIMPLECHATMAINWINDOW_H
#define SIMPLECHAT_SIMPLECHATMAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include "../Controllers/ChatController.h"


QT_BEGIN_NAMESPACE

namespace Ui {
    class SimpleChatMainWindow;
}

QT_END_NAMESPACE

class SimpleChatMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit SimpleChatMainWindow(Controllers::ChatController *controller, QWidget *parent = nullptr);

    ~SimpleChatMainWindow() override;

    void connectToPeer(const QString &id, quint16 port, quint16 nextPort, const QString &peers);

    void sendTestMessage(const QString &testPeer, const QString &testMessage, quint16 testCount);

public slots:
    void appendLine(const QString &line);

    void appendLineWithTitle(const QString &title, const QString &line);

private slots:
    void onSend();

    void transportConnect(const QString &id, const QHostAddress &address, quint16 port, quint16 nextPort);

    void onConnect();

    void onDisconnect();

    void onTransportError(const QString &error);

private:
    Ui::SimpleChatMainWindow *ui;

    QPointer<Controllers::ChatController> controller;


    void focusInput();

    void toogleInputs(bool state);

    void enableChatInputs();

    void disableChatInputs();
};


#endif //SIMPLECHAT_SIMPLECHATMAINWINDOW_H
