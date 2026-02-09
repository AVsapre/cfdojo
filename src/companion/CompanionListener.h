#pragma once

#include <QObject>
#include <QJsonObject>
#include <QList>

class QTcpServer;
class QTcpSocket;

// Listens for Competitive Companion browser extension
// Tries multiple ports used by various CP tools as fallbacks
// Protocol: HTTP POST with JSON body containing problem data
class CompanionListener : public QObject {
    Q_OBJECT

public:
    // Ports used by Competitive Companion compatible tools
    static constexpr quint16 kDefaultPorts[] = {
        10043, // Caide and AI Virtual Assistant (our preferred)
        10045, // CP Editor
        10042, // acmX
        6174,  // Mind Sport
        4244,  // Hightail
        1327,  // cpbooster
        27121  // Competitive Programming Helper
    };
    static constexpr int kDefaultPortsCount = 7;
    
    // Safety limits
    static constexpr qint64 kMaxBufferSize = 4 * 1024 * 1024;  // 4 MB
    static constexpr int kSocketTimeoutMs = 10000;              // 10 seconds

    explicit CompanionListener(QObject *parent = nullptr);
    ~CompanionListener();

    // Start listening, trying each port in the list until one works
    bool start();
    bool start(quint16 port);
    void stop();
    bool isListening() const;
    quint16 port() const;

signals:
    void problemReceived(const QJsonObject &problem);
    void errorOccurred(const QString &error);

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void parseRequest(const QByteArray &data);
    
    QTcpServer *server_ = nullptr;
    quint16 activePort_ = 0;
    QMap<QTcpSocket*, QByteArray> buffers_;
};
