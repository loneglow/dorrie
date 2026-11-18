//
// Created by gsoldano on 11/10/26.
//

#ifndef APPPATHS_H
#define APPPATHS_H
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QString>

// Everything Dorrie stores lives in one folder:
// ~/.config/dorrie on Linux (or $XDG_CONFIG_HOME/dorrie when set).
//
// Preferences are read and written with
//     QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
// rather than a default-constructed QSettings, which would use
// ~/.config/<organization>/<application>.conf instead.
namespace AppPaths {
    inline QString configDirectory() {
        return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/dorrie";
    }

    // Preferences: window layout, fonts, colors.
    inline QString settingsFile() {
        return configDirectory() + "/settings.conf";
    }

    // Saved requests and folders.
    inline QString requestsFile() {
        return configDirectory() + "/requests.json";
    }

    // Tabs open when the application was last closed, with their responses.
    inline QString sessionFile() {
        return configDirectory() + "/session.json";
    }

    // Creates the folder and, the first time, copies the preferences from the
    // location used by older builds (~/.config/gsoldano/Dorrie.conf, the
    // default QSettings path for the names set in main()). The old file is
    // left in place. Call once at startup, after the application names are set.
    inline void prepare() {
        QDir().mkpath(configDirectory());
        if (QFile::exists(settingsFile()))
            return;
        const QString legacy = QSettings().fileName();
        if (QFile::exists(legacy))
            QFile::copy(legacy, settingsFile());
    }
} // namespace AppPaths

#endif //APPPATHS_H
