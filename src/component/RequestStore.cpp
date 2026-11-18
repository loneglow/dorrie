//
// Created by gsoldano on 11/10/26.
//

#include "RequestStore.h"
#include "AppPaths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

namespace {
    constexpr int FormatVersion = 1;

    QJsonArray rowsToJson(const QList<KeyValueRow> &rows) {
        QJsonArray array;
        for (const KeyValueRow &row : rows)
            array.append(QJsonObject{
                {"enabled", row.enabled},
                {"key", row.key},
                {"value", row.value},
            });
        return array;
    }

    QList<KeyValueRow> rowsFromJson(const QJsonArray &array) {
        QList<KeyValueRow> rows;
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            rows.append({
                object.value("enabled").toBool(true),
                object.value("key").toString(),
                object.value("value").toString(),
            });
        }
        return rows;
    }

    // Credentials are stored in plain text, like the rest of the request.
    QJsonObject authToJson(const AuthData &auth) {
        return QJsonObject{
            {"type", auth.type == AuthData::Type::Basic ? "basic" : "none"},
            {"basic", QJsonObject{
                {"username", auth.basicUsername},
                {"password", auth.basicPassword},
            }},
        };
    }

    // A request saved before the Auth tab existed has no "auth": No Auth.
    AuthData authFromJson(const QJsonObject &object) {
        AuthData auth;
        if (object.value("type").toString() == "basic")
            auth.type = AuthData::Type::Basic;
        const QJsonObject basic = object.value("basic").toObject();
        auth.basicUsername = basic.value("username").toString();
        auth.basicPassword = basic.value("password").toString();
        return auth;
    }

    QJsonArray nodesToJson(const std::vector<RequestNode> &nodes);

    QJsonObject nodeToJson(const RequestNode &node) {
        QJsonObject object{
            {"id", node.id},
            {"name", node.name},
        };
        if (node.type == RequestNode::Type::Folder) {
            object.insert("type", "folder");
            object.insert("items", nodesToJson(node.children));
            return object;
        }

        object.insert("type", "request");
        const QJsonObject request = RequestStore::requestToJson(node.request);
        for (auto it = request.constBegin(); it != request.constEnd(); ++it)
            object.insert(it.key(), it.value());
        return object;
    }

    QJsonArray nodesToJson(const std::vector<RequestNode> &nodes) {
        QJsonArray array;
        for (const RequestNode &node : nodes)
            array.append(nodeToJson(node));
        return array;
    }

    std::vector<RequestNode> nodesFromJson(const QJsonArray &array) {
        std::vector<RequestNode> nodes;
        nodes.reserve(static_cast<std::size_t>(array.size()));
        for (const QJsonValue &value : array) {
            const QJsonObject object = value.toObject();
            RequestNode node;
            node.id = object.value("id").toString();
            if (node.id.isEmpty())
                node.id = RequestStore::newId();
            node.name = object.value("name").toString();

            if (object.value("type").toString() == "folder") {
                node.type = RequestNode::Type::Folder;
                node.children = nodesFromJson(object.value("items").toArray());
            } else {
                node.type = RequestNode::Type::Request;
                node.request = RequestStore::requestFromJson(object);
            }
            nodes.push_back(std::move(node));
        }
        return nodes;
    }
} // namespace

QJsonObject RequestStore::requestToJson(const RequestData &request) {
    QJsonObject pathValues;
    for (auto it = request.pathValues.cbegin(); it != request.pathValues.cend(); ++it)
        pathValues.insert(it.key(), it.value());

    return QJsonObject{
        {"method", request.method},
        {"url", request.url},
        {"pathVariables", pathValues},
        {"params", rowsToJson(request.params)},
        {"headers", rowsToJson(request.headers)},
        {"body", request.body},
        {"auth", authToJson(request.auth)},
    };
}

RequestData RequestStore::requestFromJson(const QJsonObject &object) {
    RequestData request;
    request.method = object.value("method").toString("GET");
    request.url = object.value("url").toString();
    const QJsonObject pathValues = object.value("pathVariables").toObject();
    for (auto it = pathValues.constBegin(); it != pathValues.constEnd(); ++it)
        request.pathValues.insert(it.key(), it.value().toString());
    request.params = rowsFromJson(object.value("params").toArray());
    request.headers = rowsFromJson(object.value("headers").toArray());
    request.body = object.value("body").toString();
    request.auth = authFromJson(object.value("auth").toObject());
    return request;
}

QString RequestStore::directory() {
    return AppPaths::configDirectory();
}

QString RequestStore::filePath() {
    return AppPaths::requestsFile();
}

QString RequestStore::newId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

std::vector<RequestNode> RequestStore::load(QString *error) {
    QFile file(filePath());
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QString("Cannot read %1: %2").arg(file.fileName(), file.errorString());
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        const QString backup = file.fileName() + ".corrupt-"
                               + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
        QFile::rename(file.fileName(), backup);
        if (error)
            *error = QString("%1 is not valid JSON (%2). It was moved to %3.")
                         .arg(file.fileName(), parseError.errorString(), backup);
        return {};
    }

    return nodesFromJson(document.object().value("items").toArray());
}

bool RequestStore::save(const std::vector<RequestNode> &nodes, QString *error) {
    if (!QDir().mkpath(directory())) {
        if (error)
            *error = QString("Cannot create %1").arg(directory());
        return false;
    }

    const QJsonObject root{
        {"version", FormatVersion},
        {"items", nodesToJson(nodes)},
    };

    QSaveFile file(filePath());
    if (!file.open(QIODevice::WriteOnly)
        || file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0
        || !file.commit()) {
        if (error)
            *error = QString("Cannot write %1: %2").arg(file.fileName(), file.errorString());
        return false;
    }
    return true;
}
