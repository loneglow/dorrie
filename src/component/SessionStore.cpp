#include "SessionStore.h"
#include "AppPaths.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {
    constexpr int FormatVersion = 1;

    QString toBase64(const QByteArray &bytes) {
        return QString::fromLatin1(bytes.toBase64());
    }

    QByteArray fromBase64(const QJsonValue &value) {
        return QByteArray::fromBase64(value.toString().toLatin1());
    }

    // ISO 8601 with milliseconds; the Console tab shows them.
    QString timeToJson(const QDateTime &time) {
        return time.toString(Qt::ISODateWithMs);
    }

    QDateTime timeFromJson(const QJsonValue &value) {
        return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
    }

    // Header values and bodies are raw bytes (a body may be binary, e.g. an
    // image), so they are stored as base64 to survive the JSON round trip.
    QJsonObject sentRequestToJson(const SentRequest &request) {
        QJsonArray headers;
        for (const auto &header : request.headers)
            headers.append(QJsonObject{
                {"name", toBase64(header.first)},
                {"value", toBase64(header.second)},
            });
        return QJsonObject{
            {"method", request.method},
            {"url", request.url.toString(QUrl::FullyEncoded)},
            {"headers", headers},
            {"body", toBase64(request.body)},
            {"sentAt", timeToJson(request.sentAt)},
        };
    }

    SentRequest sentRequestFromJson(const QJsonObject &object) {
        SentRequest request;
        request.method = object.value("method").toString();
        request.url = QUrl(object.value("url").toString(), QUrl::StrictMode);
        for (const QJsonValue &value : object.value("headers").toArray()) {
            const QJsonObject header = value.toObject();
            request.headers.append({fromBase64(header.value("name")),
                                    fromBase64(header.value("value"))});
        }
        request.body = fromBase64(object.value("body"));
        request.sentAt = timeFromJson(object.value("sentAt"));
        return request;
    }

    QJsonObject responseToJson(const ResponseData &response) {
        return QJsonObject{
            {"request", sentRequestToJson(response.request)},
            {"statusCode", response.statusCode},
            {"reasonPhrase", response.reasonPhrase},
            {"finalUrl", response.finalUrl.toString(QUrl::FullyEncoded)},
            {"receivedAt", timeToJson(response.receivedAt)},
            {"elapsedMs", static_cast<double>(response.elapsedMs)},
            {"headers", toBase64(response.headers)},
            {"body", toBase64(response.body)},
            {"error", response.error},
        };
    }

    // Sessions written before the Console tab have no "request": the
    // Console then shows only the response part.
    ResponseData responseFromJson(const QJsonObject &object) {
        ResponseData response;
        response.request = sentRequestFromJson(object.value("request").toObject());
        response.statusCode = object.value("statusCode").toInt();
        response.reasonPhrase = object.value("reasonPhrase").toString();
        response.finalUrl = QUrl(object.value("finalUrl").toString(), QUrl::StrictMode);
        response.receivedAt = timeFromJson(object.value("receivedAt"));
        response.elapsedMs = static_cast<qint64>(object.value("elapsedMs").toDouble());
        response.headers = fromBase64(object.value("headers"));
        response.body = fromBase64(object.value("body"));
        response.error = object.value("error").toString();
        return response;
    }
} // namespace

Session SessionStore::load(QString *error) {
    Session session;
    QFile file(AppPaths::sessionFile());
    if (!file.exists())
        return session;
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString("Cannot read %1: %2").arg(file.fileName(), file.errorString());
        return session;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        // Not worth keeping: it is rewritten on the next close anyway.
        if (error)
            *error = QString("Previous session could not be restored (%1).")
                         .arg(parseError.errorString());
        return session;
    }

    const QJsonObject root = document.object();
    for (const QJsonValue &value : root.value("tabs").toArray()) {
        const QJsonObject object = value.toObject();
        SessionTab tab;
        tab.title = object.value("title").toString("New Request");
        tab.savedId = object.value("savedId").toString();
        tab.request = RequestStore::requestFromJson(object.value("request").toObject());
        if (object.value("response").isObject())
            tab.response = responseFromJson(object.value("response").toObject());
        session.tabs.push_back(std::move(tab));
    }
    session.currentIndex = root.value("currentIndex").toInt(-1);
    return session;
}

bool SessionStore::save(const Session &session, QString *error) {
    if (!QDir().mkpath(AppPaths::configDirectory())) {
        if (error)
            *error = QString("Cannot create %1").arg(AppPaths::configDirectory());
        return false;
    }

    QJsonArray tabs;
    for (const SessionTab &tab : session.tabs) {
        QJsonObject object{
            {"title", tab.title},
            {"savedId", tab.savedId},
            {"request", RequestStore::requestToJson(tab.request)},
        };
        if (tab.response)
            object.insert("response", responseToJson(*tab.response));
        tabs.append(object);
    }

    const QJsonObject root{
        {"version", FormatVersion},
        {"currentIndex", session.currentIndex},
        {"tabs", tabs},
    };

    QSaveFile file(AppPaths::sessionFile());
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    return true;
}
