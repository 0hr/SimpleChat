// You may need to build the project (run Qt uic code generator) to get "ui_SimpleChatMainWindow.h" resolved

#include "SimpleChatMainWindow.h"
#include "ui_SimpleChatMainWindow.h"

#include <QHostAddress>
#include <QSignalBlocker>

SimpleChatMainWindow::SimpleChatMainWindow(Controllers::ChatController *controller,
                                           QWidget *parent) : QMainWindow(parent), ui(new Ui::SimpleChatMainWindow),
                                                              controller(controller) {
    ui->setupUi(this);

    connect(ui->sendButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onSend);
    connect(ui->connectButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onConnect);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onDisconnect);
    connect(ui->addPeerButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onAddPeer);
    connect(controller, &Controllers::ChatController::peersUpdated, this, &SimpleChatMainWindow::onPeersUpdated);

    ui->destComboBox->setVisible(true);
    ui->destEdit->setVisible(true);
    ui->peerHostEdit->setText(QStringLiteral("127.0.0.1"));

    focusInput();
    disableChatInputs();
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
    ui->peersEdit->setEnabled(!state);

    ui->peerHostEdit->setEnabled(state);
    ui->peerPortSpin->setEnabled(state);
    ui->addPeerButton->setEnabled(state);
}

void SimpleChatMainWindow::enableChatInputs() {
    toogleInputs(true);
}

void SimpleChatMainWindow::disableChatInputs() {
    toogleInputs(false);
}

void SimpleChatMainWindow::appendLine(const QString &line) {
    ui->chatView->append(line.toHtmlEscaped());
}

void SimpleChatMainWindow::appendLineWithTitle(const QString &title, const QString &line) {
    ui->chatView->append(
        QString("<div><b style=\"color:red\">%1</b> <span style=\"white-space: pre-wrap;\">%2</span></div>").arg(title).
        arg(line.toHtmlEscaped()));
}

void SimpleChatMainWindow::onSend() {
    const QString text = ui->messageEdit->toPlainText().trimmed();
    const QString dest = ui->destEdit->text().trimmed();
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

void SimpleChatMainWindow::transportConnect(const QString &id, const QHostAddress &address, const quint16 port,
                                            const QString &peers) {
    this->controller->setId(id);
    this->window()->setWindowTitle(
        "SimpleChat : Id:" + id + ", port: " + QString::number(port));

    auto transport = this->controller->getTransport();
    connect(transport, &Core::IChatTransport::connected, this, &SimpleChatMainWindow::enableChatInputs, Qt::UniqueConnection);
    connect(transport, &Core::IChatTransport::errorOccurred, this, &SimpleChatMainWindow::onTransportError, Qt::UniqueConnection);

    const QList<Core::IChatTransport::Peer> initialPeers = parsePeers(peers);
    transport->start(address, id, port, initialPeers);

    const QStringList entries = peers.split(',', Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QString trimmed = entry.trimmed();
        if (trimmed.isEmpty()) continue;
        const int sep = trimmed.lastIndexOf(':');
        if (sep <= 0) continue;
        const QString host = trimmed.left(sep);
        const QString portPart = trimmed.mid(sep + 1);
        bool ok = false;
        const quint16 peerPort = portPart.toUShort(&ok);
        if (!ok) continue;
        QHostAddress resolved;
        if (!resolved.setAddress(host)) {
            QMetaObject::invokeMethod(controller.data(), [this, host, peerPort]() {
                if (controller) controller->requestPeer(host, peerPort);
            }, Qt::QueuedConnection);
        }
    }

    this->focusInput();
    ui->destEdit->setText(QStringLiteral("-1"));
}

void SimpleChatMainWindow::onConnect() {
    const QString host = ui->hostEdit->text().trimmed();
    const QString id = ui->idEdit->text().trimmed();
    const quint16 port = static_cast<quint16>(ui->portSpin->value());
    const QString peers = ui->peersEdit->text().trimmed();

    if (id.isEmpty()) {
        QMessageBox::warning(this, tr("Connect"), tr("Provide a unique peer ID before connecting."));
        ui->idEdit->setFocus();
        return;
    }
    if (port == 0) {
        QMessageBox::warning(this, tr("Connect"), tr("Select a non-zero UDP port."));
        ui->portSpin->setFocus();
        return;
    }

    QHostAddress address;
    if (!address.setAddress(host)) {
        address = QHostAddress::LocalHost;
    }

    transportConnect(id, address, port, peers);
}

void SimpleChatMainWindow::onDisconnect() {
    this->controller->getTransport()->stop();
    if (controller) {
        controller->setId(QString());
    }
    this->disableChatInputs();
    onPeersUpdated(QStringList());
    ui->destEdit->clear();
    ui->destComboBox->clear();
}

void SimpleChatMainWindow::onTransportError(const QString &error) {
    qDebug() << ("Error: " + error);
}

SimpleChatMainWindow::~SimpleChatMainWindow() {
    delete ui;
}

void SimpleChatMainWindow::connectToPeer(const QString &id, const quint16 port, const QString &peers) {
    ui->idEdit->setText(id);
    ui->portSpin->setValue(port);
    ui->peersEdit->setText(peers);
    transportConnect(id, QHostAddress::LocalHost, port, peers);
    ui->destEdit->setText(QStringLiteral("-1"));
    focusInput();
}

void SimpleChatMainWindow::sendTestMessage(const QString &testPeer, const QString &testMessage, const quint16 testCount) {
    for (quint16 i = 0; i < testCount; i++) {
        QMetaObject::invokeMethod(
            controller,
            "sendChat",
            Qt::QueuedConnection,
            Q_ARG(QString, testPeer),
            Q_ARG(QString, testMessage)

        );
    }
}

void SimpleChatMainWindow::onAddPeer() {
    const QString host = ui->peerHostEdit->text().trimmed();
    const quint16 port = static_cast<quint16>(ui->peerPortSpin->value());
    if (host.isEmpty() || port == 0) {
        QMessageBox::warning(this, tr("Peer"), tr("Specify host and port to add a peer."));
        return;
    }

    QMetaObject::invokeMethod(controller.data(), [this, host, port]() {
        if (controller) controller->requestPeer(host, port);
    }, Qt::QueuedConnection);
}

void SimpleChatMainWindow::onPeersUpdated(const QStringList &peerIds) {
    const QString myId = ui->idEdit->text().trimmed();
    QStringList filtered;
    for (const QString &peer : peerIds) {
        if (peer == myId) continue;
        filtered << peer;
    }
    filtered.sort();

    const QString previous = ui->destComboBox->currentText();
    {
        QSignalBlocker blocker(ui->destComboBox);
        ui->destComboBox->clear();
        ui->destComboBox->addItem(QStringLiteral("-1"));
        ui->destComboBox->addItems(filtered);
    }
    if (filtered.contains(previous) || previous == QStringLiteral("-1")) {
        ui->destComboBox->setCurrentText(previous);
    } else {
        ui->destComboBox->setCurrentIndex(-1);
    }
}

QList<Core::IChatTransport::Peer> SimpleChatMainWindow::parsePeers(const QString &peersText) const {
    QList<Core::IChatTransport::Peer> peers;
    const QStringList entries = peersText.split(',', Qt::SkipEmptyParts);
    for (const QString &entry : entries) {
        const QString trimmed = entry.trimmed();
        if (trimmed.isEmpty()) continue;
        const int sep = trimmed.lastIndexOf(':');
        if (sep <= 0) continue;
        const QString host = trimmed.left(sep);
        const QString portPart = trimmed.mid(sep + 1);
        bool ok = false;
        const quint16 port = portPart.toUShort(&ok);
        if (!ok) continue;
        QHostAddress address;
        if (!address.setAddress(host)) continue;
        Core::IChatTransport::Peer peer(address, port);
        peers.append(peer);
    }
    return peers;
}
