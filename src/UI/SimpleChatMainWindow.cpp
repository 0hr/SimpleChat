// You may need to build the project (run Qt uic code generator) to get "ui_SimpleChatMainWindow.h" resolved

#include "SimpleChatMainWindow.h"
#include "ui_SimpleChatMainWindow.h"
#include "../Core/RingTcpTransport.h"

SimpleChatMainWindow::SimpleChatMainWindow(Controllers::ChatController* controller, QWidget *parent) : QMainWindow(parent), ui(new Ui::SimpleChatMainWindow), controller(controller) {
    ui->setupUi(this);

    connect(ui->sendButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onSend);
    connect(ui->connectButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onConnect);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onDisconnect);
    focusInput();
    ui->destEdit->setVisible(true);
    ui->destComboBox->setVisible(false);
}

void SimpleChatMainWindow::focusInput() {
    ui->chatView->setFocus();
    ui->chatView->setFocusPolicy(Qt::StrongFocus);
    ui->messageEdit->setFocus();
}

void SimpleChatMainWindow::toogleInputs(bool state) {
    ui->chatView->setEnabled(state);
    //ui->destEdit->setEnabled(state);
    //ui->messageEdit->setEnabled(state);
    //ui->sendButton->setEnabled(state);
    ui->connectButton->setEnabled(!state);
    ui->disconnectButton->setEnabled(state);

    ui->hostEdit->setEnabled(!state);
    ui->idEdit->setEnabled(!state);
    ui->portSpin->setEnabled(!state);
    ui->nextPortSpin->setEnabled(!state);
}

void SimpleChatMainWindow::enableChatInputs() {
    toogleInputs(true);
}

void SimpleChatMainWindow::disableChatInputs() {
    toogleInputs(false);
}

void SimpleChatMainWindow::appendLine(const QString& line) {
    ui->chatView->append(line.toHtmlEscaped());
}

void SimpleChatMainWindow::appendLineWithTitle(const QString& title, const QString& line) {
    ui->chatView->append(QString("<div><b style=\"color:red\">%1</b> <span style=\"white-space: pre-wrap;\">%2</span></div>").arg(title).arg(line.toHtmlEscaped()));
}

void SimpleChatMainWindow::onSend() {
    const QString text = ui->messageEdit->toPlainText().trimmed();
    const QString dest = ui->destEdit->text().trimmed();
    if (dest.isEmpty()) {
        QMessageBox::critical(nullptr, "Error", "You have to choose peer!");
        return;
    }

    if (text.isEmpty()) {
        QMessageBox::critical(nullptr, "Error", "You have to write message!");
        return;
    }
    
    QMetaObject::invokeMethod(
        controller,
        "sendChat",
        Qt::QueuedConnection,
        Q_ARG(QString, dest),
        Q_ARG(QString, text)
    );

    ui->messageEdit->clear();
    // ui->destComboBox->setCurrentIndex(-1);
    focusInput();
}

void SimpleChatMainWindow::transportConnect(const QString &id, const QHostAddress &address, const quint16 port, const quint16 nextPort) {
    this->controller->setId(id);
    this->window()->setWindowTitle( "SimpleChat : Id:" + id + ", port: " + QString::number(port) + ", next port: "+ QString::number(nextPort));

    auto transport = this->controller->getTransport();
    connect(transport, &Core::IChatTransport::connected, this, &SimpleChatMainWindow::enableChatInputs);
    connect(transport, &Core::IChatTransport::errorOccurred, this, &SimpleChatMainWindow::onTransportError);

    transport->start(address, id, port, nextPort);
    this->focusInput();
}

void SimpleChatMainWindow::onConnect() {
    const QString host = ui->hostEdit->text().trimmed();
    const QString id = ui->idEdit->text().trimmed();
    const quint16 port = ui->portSpin->text().trimmed().toUShort();
    const quint16 nextPort = ui->nextPortSpin->text().trimmed().toUShort();

    QHostAddress address(host);

    transportConnect(id, address, port, nextPort);
}

void SimpleChatMainWindow::onDisconnect() {
    this->controller->getTransport()->stop();
    this->disableChatInputs();
}

void SimpleChatMainWindow::onTransportError(const QString& error) {
    qDebug() << ("Error: " + error);
}

SimpleChatMainWindow::~SimpleChatMainWindow() {
    delete ui;
}

void SimpleChatMainWindow::connectToPeer(const QString &id, const quint16 port, const quint16 nextPort, const QString &peers) {
    ui->idEdit->setText(id);
    ui->portSpin->setValue(port);
    ui->nextPortSpin->setValue(nextPort);
    ui->destEdit->setVisible(false);
    ui->destComboBox->setVisible(true);
    QStringList list = peers.split(",");
    ui->destComboBox->addItems(list);
    transportConnect(id, QHostAddress::LocalHost, port, nextPort);
    focusInput();
}
