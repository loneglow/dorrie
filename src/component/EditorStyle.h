#ifndef EDITORSTYLE_H
#define EDITORSTYLE_H

#include <QColor>
#include <QPalette>
#include <QWidget>

namespace EditorStyle {
    // White background with dark text, whatever the system theme is.
    // The palette is set on the widget itself, so it is kept even when the
    // application palette changes (e.g. switching to a dark desktop theme).
    inline void applyWhiteBackground(QWidget *widget) {
        QPalette palette = widget->palette();
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Text, Qt::black);
        palette.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
        widget->setPalette(palette);
    }
}

#endif // EDITORSTYLE_H
