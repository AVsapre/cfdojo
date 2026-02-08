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
        
        // Set a timeout so misbehaving clients don't hang forever
        QTimer *timeout = new QTimer(socket);
        timeout->setSingleShot(true);
        connect(timeout, &QTimer::timeout, this, [this, socket]() {
            buffers_.remove(socket);
            socket->abort();
            socket->deleteLater();
        });
        timeout->start(kSocketTimeoutMs);
    }
}

void CompanionListener::onReadyRead() {
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !buffers_.contains(socket)) {
        return;
    }
    
    buffers_[socket].append(socket->readAll());
    
    // Guard against unbounded memory growth
    if (buffers_[socket].size() > kMaxBufferSize) {
        buffers_.remove(socket);
        socket->abort();
        socket->deleteLater();
        return;
    }
    
    // Check if we have a complete HTTP request
    QByteArray &buffer = buffers_[socket];
    
    // Look for end of HTTP headers
    int headerEnd = buffer.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return;  // Headers not complete yet
    }
    
    // Parse Content-Length from headers
    int contentLength = -1;
    QByteArray headers = buffer.left(headerEnd);
    int clPos = headers.toLower().indexOf("content-length:");
    if (clPos != -1) {
        int lineEnd = headers.indexOf("\r\n", clPos);
        if (lineEnd == -1) {
            lineEnd = headers.size();
        }
        QByteArray clLine = headers.mid(clPos + 15, lineEnd - clPos - 15).trimmed();
        bool ok = false;
        contentLength = clLine.toInt(&ok);
        if (!ok || contentLength < 0) {
            contentLength = -1;
        }
    }
    
    // If no Content-Length header, we cannot know the body size — reject.
    if (contentLength < 0) {
        // Send an error response
        QByteArray response = "HTTP/1.1 411 Length Required\r\n"
                              "Content-Length: 0\r\n"
                              "Connection: close\r\n"
                              "\r\n";
        socket->write(response);
        socket->flush();
        buffers_.remove(socket);
        socket->disconnectFromHost();
        return;
    }
    
    // Check if we have the complete body
    int bodyStart = headerEnd + 4;
    if (buffer.size() < bodyStart + contentLength) {
        return;  // Body not complete yet
    }
    
    // Parse the request — only use exactly contentLength bytes
    parseRequest(socket, buffer.mid(0, bodyStart + contentLength));
    
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
    int headerEnd = data.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        return;
    }

    // Re-parse Content-Length to know exact body size
    QByteArray headers = data.left(headerEnd);
    int contentLength = 0;
    int clPos = headers.toLower().indexOf("content-length:");
    if (clPos != -1) {
        int lineEnd = headers.indexOf("\r\n", clPos);
        if (lineEnd == -1) {
            lineEnd = headers.size();
        }
        QByteArray clLine = headers.mid(clPos + 15, lineEnd - clPos - 15).trimmed();
        contentLength = clLine.toInt();
    }

    int bodyStart = headerEnd + 4;
    QByteArray body = data.mid(bodyStart, contentLength);
    
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
