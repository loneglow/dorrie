//
// Created by gsoldano on 11/10/26.
//

#include "SettingsDialog.h"
#include "AppPaths.h"

#include <QApplication>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace {
    constexpr int MinFontSize = 6;
    constexpr int MaxFontSize = 48;

    // Point size of a font, also when it was defined in pixels.
    int pointSizeOf(const QFont &font) {
        if (font.pointSize() > 0)
            return font.pointSize();
        return qMax(MinFontSize, qRound(font.pixelSize() * 0.75));
    }

    QFont readFont(const QSettings &settings, const QString &prefix, const QFont &fallback) {
        QFont font = fallback;
        const QString family = settings.value(prefix + "Family").toString();
        if (!family.isEmpty())
            font.setFamily(family);
        const int size = settings.value(prefix + "Size", 0).toInt();
        if (size >= MinFontSize && size <= MaxFontSize)
            font.setPointSize(size);
        return font;
    }

    QColor readColor(const QSettings &settings, const QString &key, const QColor &fallback) {
        const QColor color(settings.value(key).toString());
        return color.isValid() ? color : fallback;
    }

    QFont makeFont(const QFontComboBox *family, const QSpinBox *size) {
        QFont font = family->currentFont();
        font.setPointSize(size->value());
        return font;
    }

    // The interface font before any user setting is applied.
    // Captured on first use, which happens before the stored font is applied.
    const QFont &systemUiFont() {
        static const QFont font = QApplication::font();
        return font;
    }
} // namespace

AppearanceSettings AppearanceSettings::defaults() {
    return {
        systemUiFont(),
        QFontDatabase::systemFont(QFontDatabase::FixedFont),
        QColor("#0451a5"), // keys: blue
        QColor("#a31515"), // strings: dark red
        QColor("#098658"), // numbers, booleans, null: green
    };
}

AppearanceSettings AppearanceSettings::load() {
    const AppearanceSettings fallback = defaults();
    const QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    return {
        readFont(settings, "appearance/uiFont", fallback.uiFont),
        readFont(settings, "appearance/responseFont", fallback.responseFont),
        readColor(settings, "appearance/jsonKeyColor", fallback.jsonKeyColor),
        readColor(settings, "appearance/jsonStringColor", fallback.jsonStringColor),
        readColor(settings, "appearance/jsonOtherColor", fallback.jsonOtherColor),
    };
}

void AppearanceSettings::save() const {
    QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
    settings.setValue("appearance/uiFontFamily", uiFont.family());
    settings.setValue("appearance/uiFontSize", pointSizeOf(uiFont));
    settings.setValue("appearance/responseFontFamily", responseFont.family());
    settings.setValue("appearance/responseFontSize", pointSizeOf(responseFont));
    settings.setValue("appearance/jsonKeyColor", jsonKeyColor.name());
    settings.setValue("appearance/jsonStringColor", jsonStringColor.name());
    settings.setValue("appearance/jsonOtherColor", jsonOtherColor.name());
}

SettingsDialog::SettingsDialog(const AppearanceSettings &current, QWidget *parent)
    : QDialog(parent) {
    setWindowTitle("Settings");
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);

    // Interface
    auto *uiGroup = new QGroupBox("Interface");
    auto *uiForm = new QFormLayout(uiGroup);
    uiFamily = new QFontComboBox;
    uiSize = new QSpinBox;
    uiSize->setRange(MinFontSize, MaxFontSize);
    uiSize->setSuffix(" pt");
    uiPreview = new QLabel("The quick brown fox jumps over the lazy dog");
    uiForm->addRow("Font:", uiFamily);
    uiForm->addRow("Size:", uiSize);
    uiForm->addRow("Preview:", uiPreview);
    layout->addWidget(uiGroup);

    // Response
    auto *responseGroup = new QGroupBox("Response");
    auto *responseForm = new QFormLayout(responseGroup);
    responseFamily = new QFontComboBox;
    responseSize = new QSpinBox;
    responseSize->setRange(MinFontSize, MaxFontSize);
    responseSize->setSuffix(" pt");
    responsePreview = new QLabel;
    responsePreview->setTextFormat(Qt::RichText);
    keyColorButton = new QPushButton;
    stringColorButton = new QPushButton;
    otherColorButton = new QPushButton;
    responseForm->addRow("Font:", responseFamily);
    responseForm->addRow("Size:", responseSize);
    responseForm->addRow("JSON key color:", keyColorButton);
    responseForm->addRow("JSON string value color:", stringColorButton);
    responseForm->addRow("JSON other values color:", otherColorButton);
    responseForm->addRow("Preview:", responsePreview);
    layout->addWidget(responseGroup);

    layout->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                         | QDialogButtonBox::Cancel
                                         | QDialogButtonBox::Apply
                                         | QDialogButtonBox::RestoreDefaults);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        emit applied(settings());
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this,
            [this] { emit applied(settings()); });
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this,
            [this] { setValues(AppearanceSettings::defaults()); });

    for (auto *combo : {uiFamily, responseFamily})
        connect(combo, &QFontComboBox::currentFontChanged, this, [this] { updatePreviews(); });
    connect(keyColorButton, &QPushButton::clicked, this,
            [this] { pickColor(keyColor, keyColorButton, "JSON key color"); });
    connect(stringColorButton, &QPushButton::clicked, this,
            [this] { pickColor(stringColor, stringColorButton, "JSON string value color"); });
    connect(otherColorButton, &QPushButton::clicked, this,
            [this] { pickColor(otherColor, otherColorButton, "JSON number/boolean/null color"); });

    for (auto *spin : {uiSize, responseSize})
        connect(spin, &QSpinBox::valueChanged, this, [this] { updatePreviews(); });

    setValues(current);
}

AppearanceSettings SettingsDialog::settings() const {
    return {
        makeFont(uiFamily, uiSize),
        makeFont(responseFamily, responseSize),
        keyColor,
        stringColor,
        otherColor,
    };
}

void SettingsDialog::setValues(const AppearanceSettings &values) {
    uiFamily->setCurrentFont(values.uiFont);
    uiSize->setValue(pointSizeOf(values.uiFont));
    responseFamily->setCurrentFont(values.responseFont);
    responseSize->setValue(pointSizeOf(values.responseFont));
    keyColor = values.jsonKeyColor;
    stringColor = values.jsonStringColor;
    otherColor = values.jsonOtherColor;
    showColor(keyColorButton, keyColor);
    showColor(stringColorButton, stringColor);
    showColor(otherColorButton, otherColor);
    updatePreviews();
}

void SettingsDialog::updatePreviews() {
    const AppearanceSettings values = settings();
    uiPreview->setFont(values.uiFont);
    responsePreview->setFont(values.responseFont);

    const auto span = [](const QColor &color, const QString &text) {
        return QString("<span style=\"color:%1\">%2</span>").arg(color.name(), text.toHtmlEscaped());
    };
    responsePreview->setText(
        "{ " + span(keyColor, R"("id")") + ": " + span(otherColor, "42") + ", "
        + span(keyColor, R"("name")") + ": " + span(stringColor, R"("Dorrie")") + ", "
        + span(keyColor, R"("active")") + ": " + span(otherColor, "true") + " }");
}

void SettingsDialog::pickColor(QColor &color, QPushButton *button, const QString &title) {
    const QColor chosen = QColorDialog::getColor(color, this, title);
    if (!chosen.isValid())
        return; // dialog cancelled
    color = chosen;
    showColor(button, color);
    updatePreviews();
}

void SettingsDialog::showColor(QPushButton *button, const QColor &color) {
    QPixmap swatch(16, 16);
    swatch.fill(color);
    button->setIcon(QIcon(swatch));
    button->setText(color.name());
}
