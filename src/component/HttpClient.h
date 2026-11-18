//
// Created by gsoldano on 11/9/26.
//

#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H
#include <QObject>
#include <QNetworkAccessManager>

#include <functional>

#include "ResponseData.h"

class QNetworkReply;

class HttpClient final : public QObject {
    Q_OBJECT

public:
    explicit HttpClient(QObject *parent = nullptr);

    // Starts an asynchronous request; callers must not block waiting for
    // the response. onFinished is called once with the outcome of this
    // specific request, so each caller gets its own response back.
    // The returned reply is deleted automatically once finished.
    QNetworkReply *send(const QString &method,
              const QUrl &url,
              const QList<QPair<QByteArray, QByteArray>> &headers,
              const QByteArray &body,
              std::function<void(const ResponseData &)> onFinished);

private:
    QNetworkAccessManager manager;
};



#endif //HTTPCLIENT_H
