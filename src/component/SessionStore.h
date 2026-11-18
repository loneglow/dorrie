#ifndef SESSIONSTORE_H
#define SESSIONSTORE_H

#include <QString>

#include <optional>
#include <vector>

#include "RequestStore.h"
#include "ResponseData.h"

// One request tab as it was when the application was closed.
struct SessionTab {
    QString title;
    QString savedId;                      // empty for a never-saved request
    RequestData request;                  // current content, unsaved edits included
    std::optional<ResponseData> response; // last response, if it was sent
};

struct Session {
    std::vector<SessionTab> tabs;
    int currentIndex = -1;
};

// Reads and writes the open tabs as ~/.config/dorrie/session.json.
class SessionStore {
public:
    // Empty session when the file does not exist or cannot be read;
    // *error is set only for an unreadable or invalid file.
    static Session load(QString *error = nullptr);

    // Writes atomically (temporary file + rename).
    static bool save(const Session &session, QString *error = nullptr);
};

#endif // SESSIONSTORE_H
