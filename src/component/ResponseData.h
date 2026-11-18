#ifndef RESPONSEDATA_H
#define RESPONSEDATA_H

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrl>

// The request exactly as HttpClient handed it to Qt.
struct SentRequest {
    QString method;
    QUrl url;                                     // query string included
    QList<QPair<QByteArray, QByteArray>> headers; // headers set by Dorrie
    QByteArray body;
    QDateTime sentAt;
};

// Outcome of one HTTP request, as shown in the response panel.
struct ResponseData {
    SentRequest request;

    int statusCode = 0;     // 0 when no HTTP response was received
    QString reasonPhrase;   // e.g. "OK", "Not Found"
    QUrl finalUrl;          // differs from request.url after a redirect
    QDateTime receivedAt;
    qint64 elapsedMs = 0;
    QByteArray headers;     // "Name: value\r\n" lines
    QByteArray body;
    QString error;          // network error message, empty on success
};

#endif // RESPONSEDATA_H
