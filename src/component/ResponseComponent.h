//
// Created by gsoldano on 11/9/26.
//

#ifndef RESPONSECOMPONENT_H
#define RESPONSECOMPONENT_H
#include <QWidget>

#include "ResponseData.h"


class QPlainTextEdit;
class QLabel;
class QFont;
class QColor;
class JsonHighlighter;

class ResponseComponent final : public QWidget {
    Q_OBJECT

public:
    explicit ResponseComponent(QWidget *parent = nullptr);

    void showResponse(const ResponseData &response);

    // Back to the empty state, for a request that was never sent.
    void clear();

    // Font used for the response body and headers.
    void setResponseFont(const QFont &font);

    // Text colors used to highlight JSON response bodies.
    void setJsonColors(const QColor &key, const QColor &string, const QColor &other);

private:
    QLabel *statusLabel = nullptr;
    QLabel *metaLabel = nullptr;
    QPlainTextEdit *bodyView = nullptr;
    QPlainTextEdit *headersView = nullptr;
    QPlainTextEdit *consoleView = nullptr; // request + response details
    JsonHighlighter *jsonHighlighter = nullptr;
};



#endif //RESPONSECOMPONENT_H
