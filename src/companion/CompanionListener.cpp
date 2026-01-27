#include "companion/CompanionListener.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

CompanionListener::CompanionListener(QObject *parent)
    : QObject(parent), server_(new QTcpServer(this)) {
    connect(server_, &QTcpServer::newConnection,
            this, &CompanionListener::onNewConnection);
}

CompanionListener::~CompanionListener() {
    stop();
}

bool CompanionListener::start() {
    // Try each port until one works
    for (int i = 0; i < kDefaultPortsCount; ++i) {
        if (start(kDefaultPorts[i])) {
            return true;
        }
    }
    return false;
}

bool CompanionListener::start(quint16 port) {
    if (server_->isListening()) {
        return true;
    }
    
    if (!server_->listen(QHostAddress::LocalHost, port)) {
        // Don't emit error for fallback attempts
        return false;
    }
    
    activePort_ = port;
    return true;
}

void CompanionListener::stop() {
    if (server_->isListening()) {
        server_->close();
    }
    
    // Close all pending connections
    for (auto it = buffers_.begin(); it != buffers_.end(); ++it) {
        it.key()->deleteLater();
    }
    buffers_.clear();
}

bool CompanionListener::isListening() const {
    return server_->isListening();
}

quint16 CompanionListener::port() const {
    return activePort_;
}

void CompanionListener::onNewConnection() {
    while (server_->hasPendingConnections()) {
        QTcpSocket *socket = server_->nextPendingConnection();
        
        connect(socket, &QTcpSocket::readyRead,
                this, &CompanionListener::onReadyRead);
        connect(socket, &QTcpSocket::disconnected,
                this, &CompanionListener::onDisconnected);
        
        buffers_[socket] = QByteArray();
    }
}

void CompanionListener::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !buffers_.contains(socket)) {
        return;
    }
    
    buffers_[socket].append(socket->readAll());
    
    // Check if we have a complete HTTP request
    QByteArray &buffer = buffers_[socket];
    
    // Look for end of HTTP headers
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return;  // Headers not complete yet
    }
    
    // Parse Content-Length from headers
    int contentLength = 0;
    QByteArray headers = buffer.left(headerEnd);
    int clPos = headers.toLower().indexOf("content-length:");
    if (clPos != -1) {
        int lineEnd = headers.indexOf("\r\n", clPos);
        QByteArray clLine = headers.mid(clPos + 15, lineEnd - clPos - 15).trimmed();
        contentLength = clLine.toInt();
    }
    
    // Check if we have the complete body
    int bodyStart = headerEnd + 4;
    if (buffer.size() < bodyStart + contentLength) {
        return;  // Body not complete yet
    }
    
    // Parse the request
    parseRequest(socket, buffer);
    
    // Send HTTP response
    QByteArray response = "HTTP/1.1 200 OK\r\n"
                          "Content-Type: text/plain\r\n"
                          "Content-Length: 2\r\n"
                          "Connection: close\r\n"
                          "\r\n"
                          "OK";
    socket->write(response);
    socket->flush();
    socket->disconnectFromHost();
}

void CompanionListener::onDisconnected() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        buffers_.remove(socket);
        socket->deleteLater();
    }
}

void CompanionListener::parseRequest(QTcpSocket *socket, const QByteArray &data) {
    // Find body start (after \r\n\r\n)
    int bodyStart = data.indexOf("\r\n\r\n");
    if (bodyStart == -1) {
        return;
    }
    bodyStart += 4;
    
    QByteArray body = data.mid(bodyStart);
    
    // Parse JSON
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(body, &error);
    
    if (error.error != QJsonParseError::NoError) {
        emit errorOccurred(QString("JSON parse error: %1").arg(error.errorString()));
        return;
    }
    
    if (!doc.isObject()) {
        emit errorOccurred("Expected JSON object");
        return;
    }
    
    // Defer emission to avoid blocking socket handling with modal dialogs
    QJsonObject problem = doc.object();
    QTimer::singleShot(0, this, [this, problem]() {
        emit problemReceived(problem);
    });
}
