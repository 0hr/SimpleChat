// You may need to build the project (run Qt uic code generator) to get "ui_SimpleChatMainWindow.h" resolved

#include "SimpleChatMainWindow.h"
#include "ui_SimpleChatMainWindow.h"

#include <QHostAddress>
#include <QSignalBlocker>
#include <QInputDialog>
#include <QShortcut>
#include <QListWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QDateTime>

SimpleChatMainWindow::SimpleChatMainWindow(
    Controllers::ChatController *controller,
    QWidget *parent
) : QMainWindow(parent), ui(new Ui::SimpleChatMainWindow), controller(controller) {
    ui->setupUi(this);

    connect(ui->sendButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onSend);
    connect(ui->connectButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onConnect);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onDisconnect);
    connect(ui->addPeerButton, &QPushButton::clicked, this, &SimpleChatMainWindow::onAddPeer);
    connect(controller, &Controllers::ChatController::peersUpdated, this, &SimpleChatMainWindow::onPeersUpdated);
    connect(controller, &Controllers::ChatController::routesDetailedUpdated, this, &SimpleChatMainWindow::onRoutesDetailedUpdated);

    ui->peerHostEdit->setText(QStringLiteral("127.0.0.1"));

    connect(ui->nodesListWidget, &QListWidget::itemActivated, this, [this](QListWidgetItem* item){
        if (!item) return;
        openPrivateMessageDialog();
    });

    connect(ui->nodesListWidget, &QListWidget::itemClicked, this, [this](QListWidgetItem* item){
        if (!item) return;
        openPrivateMessageDialog();
    });

    if (auto table = this->findChild<QTableWidget*>("routesTable")) {
        table->setColumnCount(6);
        QStringList headers;
        headers << tr("Dest") << tr("Next Hop") << tr("SeqNo") << tr("Direct") << tr("Endpoint") << tr("Updated");
        table->setHorizontalHeaderLabels(headers);
        table->horizontalHeader()->setStretchLastSection(true);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
    }

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
    if (text.isEmpty()) {
        QMessageBox::critical(nullptr, "Error", "You have to write message!");
        return;
    }

    QMetaObject::invokeMethod(
        controller,
        "sendChat",
        Qt::QueuedConnection,
        Q_ARG(QString, QStringLiteral("-1")),
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
}

void SimpleChatMainWindow::onTransportError(const QString &error) {
    qDebug() << ("Error: " + error);
}

SimpleChatMainWindow::~SimpleChatMainWindow() {
    delete ui;
}

void SimpleChatMainWindow::connectToPeer(const QString &id, const quint16 port, const QString &peers) {
    connectToPeer(id, port, peers, QHostAddress::LocalHost);
}

void SimpleChatMainWindow::connectToPeer(const QString &id, const quint16 port, const QString &peers, const QHostAddress& bindAddress) {
    ui->idEdit->setText(id);
    ui->portSpin->setValue(port);
    ui->peersEdit->setText(peers);
    transportConnect(id, bindAddress, port, peers);
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

    {
        QSignalBlocker blocker(ui->nodesListWidget);
        ui->nodesListWidget->clear();
        ui->nodesListWidget->addItems(filtered);
    }
}

void SimpleChatMainWindow::openPrivateMessageDialog() {
    QStringList options;
    for (int i = 0; i < ui->nodesListWidget->count(); ++i) {
        const auto *item = ui->nodesListWidget->item(i);
        if (!item) continue;
        const QString text = item->text().trimmed();
        if (text.isEmpty()) continue;
        options << text;
    }
    options.removeAll(ui->idEdit->text().trimmed());
    options.sort();
    if (options.isEmpty()) {
        QMessageBox::information(this, tr("Private Message"), tr("No destinations available."));
        return;
    }

    bool ok = false;
    QString currentTarget;
    if (auto* selItem = ui->nodesListWidget->currentItem()) {
        currentTarget = selItem->text();
    }
    int currentIndex = options.indexOf(currentTarget);
    if (currentIndex < 0) currentIndex = 0;
    const QString dest = QInputDialog::getItem(this, tr("Private Message"), tr("Destination:"), options, currentIndex, false, &ok);
    if (!ok || dest.isEmpty()) return;

    bool ok2 = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Private Message"), tr("Message:"), QString(), &ok2);
    if (!ok2 || text.trimmed().isEmpty()) return;

    QMetaObject::invokeMethod(
        controller,
        "sendChat",
        Qt::QueuedConnection,
        Q_ARG(QString, dest),
        Q_ARG(QString, text)
    );
}

static QString sinceString(qint64 ms) {
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    qint64 delta = qMax<qint64>(0, now - ms);
    if (delta < 2000) return QObject::tr("just now");
    if (delta < 60000) return QObject::tr("%1s ago").arg(delta/1000);
    if (delta < 3600000) return QObject::tr("%1m ago").arg(delta/60000);
    return QObject::tr("%1h ago").arg(delta/3600000);
}

void SimpleChatMainWindow::onRoutesDetailedUpdated(const QList<QVariantMap> &routes) {
    auto table = this->findChild<QTableWidget*>("routesTable");
    if (!table) return;
    table->setRowCount(routes.size());
    int row = 0;
    for (const QVariantMap &m : routes) {
        const QString dest = m.value("Dest").toString();
        const QString nextHop = m.value("NextHop").toString();
        const qulonglong seqNo = m.value("SeqNo").toULongLong();
        const bool direct = m.value("Direct").toBool();
        const QString ip = m.value("LastIP").toString();
        const quint16 port = static_cast<quint16>(m.value("LastPort").toUInt());
        const qint64 updated = m.value("LastUpdatedMs").toLongLong();

        auto setItem = [&](int col, const QString &text){
            auto *it = new QTableWidgetItem(text);
            table->setItem(row, col, it);
        };
        setItem(0, dest);
        setItem(1, nextHop);
        setItem(2, QString::number(seqNo));
        setItem(3, direct ? tr("Yes") : tr("No"));
        setItem(4, (ip.isEmpty() || port == 0) ? QString() : QStringLiteral("%1:%2").arg(ip, QString::number(port)));
        setItem(5, updated > 0 ? sinceString(updated) : QString());
        row++;
    }
    table->resizeColumnsToContents();
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
