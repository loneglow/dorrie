//
// Created by gsoldano on 11/10/26.
//

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H
#include <QDialog>
#include <QColor>
#include <QFont>

class QFontComboBox;
class QSpinBox;
class QLabel;
class QPushButton;

// Fonts and colors chosen by the user, persisted under "appearance/" in
// AppPaths::settingsFile() (~/.config/dorrie/settings.conf).
struct AppearanceSettings {
    QFont uiFont;       // whole application interface
    QFont responseFont; // body and headers in ResponseComponent

    // Text colors for JSON response bodies.
    QColor jsonKeyColor;    // object keys
    QColor jsonStringColor; // string values
    QColor jsonOtherColor;  // numbers, booleans and null

    // Defaults: the platform's UI font and the platform's fixed-width font.
    static AppearanceSettings defaults();
    // Stored values, falling back to defaults() for anything missing.
    static AppearanceSettings load();
    void save() const;
};

class SettingsDialog final : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(const AppearanceSettings &current, QWidget *parent = nullptr);

    AppearanceSettings settings() const;

signals:
    // Emitted on Apply and on OK, so the caller can apply the fonts live.
    void applied(const AppearanceSettings &settings);

private:
    void setValues(const AppearanceSettings &values);
    void updatePreviews();
    void pickColor(QColor &color, QPushButton *button, const QString &title);
    static void showColor(QPushButton *button, const QColor &color);

    QFontComboBox *uiFamily = nullptr;
    QSpinBox *uiSize = nullptr;
    QLabel *uiPreview = nullptr;

    QFontComboBox *responseFamily = nullptr;
    QSpinBox *responseSize = nullptr;
    QLabel *responsePreview = nullptr;

    QColor keyColor;
    QColor stringColor;
    QColor otherColor;
    QPushButton *keyColorButton = nullptr;
    QPushButton *stringColorButton = nullptr;
    QPushButton *otherColorButton = nullptr;
};

#endif //SETTINGSDIALOG_H
