//
// Created by gsoldano on 11/9/26.
//

#include "HttpClient.h"

#include <QDateTime>
#include <QElapsedTimer>
#include <QNetworkRequest>
#include <QUrl>
#include <QNetworkReply>

HttpClient::HttpClient(QObject *parent)
    : QObject(parent), manager(this) {
}

QNetworkReply *HttpClient::send(const QString &method,
                      const QUrl &url,
                      const QList<QPair<QByteArray, QByteArray>> &headers,
                      const QByteArray &body,
                      std::function<void(const ResponseData &)> onFinished) {
    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    request.setAttribute(
        QNetworkRequest::RedirectPolicyAttribute,
        QNetworkRequest::NoLessSafeRedirectPolicy
    );

    for (const auto &header : headers)
        request.setRawHeader(header.first, header.second);

    // A generic request also supports less common methods and DELETE bodies.
    QNetworkReply *reply = manager.sendCustomRequest(
        request, method.toLatin1(), body
    );

    QElapsedTimer timer;
    timer.start();

    // Kept with the response, for the Console tab.
    SentRequest sent{method, url, headers, body, QDateTime::currentDateTime()};

    connect(reply, &QNetworkReply::finished, this,
            [reply, timer, sent = std::move(sent), onFinished = std::move(onFinished)] {
        ResponseData response;
        response.request = sent;
        response.receivedAt = QDateTime::currentDateTime();
        response.elapsedMs = timer.elapsed();
        response.statusCode = reply->attribute(
            QNetworkRequest::HttpStatusCodeAttribute
        ).toInt();
        response.reasonPhrase = reply->attribute(
            QNetworkRequest::HttpReasonPhraseAttribute
        ).toString();
        response.finalUrl = reply->url();

        const auto rawHeaders = reply->rawHeaderPairs();
        for (const auto &header : rawHeaders) {
            response.headers += header.first;
            response.headers += ": ";
            response.headers += header.second;
            response.headers += "\r\n";
        }

        response.body = reply->readAll();
        if (reply->error() != QNetworkReply::NoError)
            response.error = reply->errorString();

        if (onFinished)
            onFinished(response);

        reply->deleteLater();
    });

    return reply;
}
