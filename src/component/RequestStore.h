//
// Created by gsoldano on 11/10/26.
//

#ifndef REQUESTSTORE_H
#define REQUESTSTORE_H
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <vector>

// One row of the Query or Headers table.
struct KeyValueRow {
    bool enabled = true;
    QString key;
    QString value;
};

// Auth tab. The credentials of every type are kept even when another type
// is selected, so switching back finds them as they were typed.
struct AuthData {
    enum class Type { None, Basic };

    Type type = Type::None;
    QString basicUsername;
    QString basicPassword;
};

// Everything the request editor needs to restore a request.
struct RequestData {
    AuthData auth;
    QString method = "GET";
    QString url;
    QHash<QString, QString> pathValues; // values of the ":name" URL segments
    QList<KeyValueRow> params;
    QList<KeyValueRow> headers;
    QString body;
};

// A node of the saved-requests tree: a folder (with children) or a request.
struct RequestNode {
    enum class Type { Folder, Request };

    Type type = Type::Request;
    QString id;   // stable UUID, used to link open tabs to saved requests
    QString name;
    RequestData request;               // used only by requests
    std::vector<RequestNode> children; // used only by folders
};

// Reads and writes the saved-requests tree as a single JSON file:
// ~/.config/dorrie/requests.json (honours $XDG_CONFIG_HOME).
class RequestStore {
public:
    static QString directory();
    static QString filePath();

    // Returns the stored tree; empty when the file does not exist yet.
    // If the file cannot be parsed it is moved aside (requests.json.corrupt-…)
    // so the next save does not destroy it, and *error describes what happened.
    static std::vector<RequestNode> load(QString *error = nullptr);

    // Writes atomically (temporary file + rename): a crash never leaves a
    // half-written file behind.
    static bool save(const std::vector<RequestNode> &nodes, QString *error = nullptr);

    static QString newId();

    // JSON form of a single request, as written inside requests.json
    // (also used for the open tabs in session.json).
    static QJsonObject requestToJson(const RequestData &request);
    static RequestData requestFromJson(const QJsonObject &object);
};

#endif //REQUESTSTORE_H
