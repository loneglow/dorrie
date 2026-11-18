//
// Created by gsoldano on 11/9/26.
//

#include "ResponseComponent.h"
#include "EditorStyle.h"

#include <QLabel>
#include <QPlainTextEdit>
#include <qtabwidget.h>
#include <QVBoxLayout>
#include <QDateTime>
#include <QJsonDocument>
#include <QStringList>
#include <QUrl>
#include <QJsonParseError>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

#include <string>

namespace {
    // Re-indents JSON text without parsing it into QJsonObject, so the original
    // key order (and the exact spelling of numbers) is preserved.
    // Expects valid JSON: validity is checked beforehand with QJsonDocument.
    std::string indentJson(const std::string &in, int indentSize = 2) {
        std::string out;
        out.reserve(in.size() * 2);
        int depth = 0;
        bool inString = false;
        bool escaped = false;

        const auto newline = [&] {
            out += '\n';
            out.append(static_cast<std::size_t>(depth * indentSize), ' ');
        };
        const auto isSpace = [](char c) {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r';
        };

        for (std::size_t i = 0; i < in.size(); ++i) {
            const char c = in[i];

            if (inString) {
                out += c;
                if (escaped)
                    escaped = false;
                else if (c == '\\')
                    escaped = true;
                else if (c == '"')
                    inString = false;
                continue;
            }

            if (isSpace(c))
                continue; // original whitespace outside strings is dropped

            switch (c) {
            case '"':
                inString = true;
                out += c;
                break;
            case '{':
            case '[': {
                // Keep empty containers compact: {} and []
                std::size_t next = i + 1;
                while (next < in.size() && isSpace(in[next]))
                    ++next;
                const char closing = (c == '{') ? '}' : ']';
                if (next < in.size() && in[next] == closing) {
                    out += c;
                    out += closing;
                    i = next;
                    break;
                }
                out += c;
                ++depth;
                newline();
                break;
            }
            case '}':
            case ']':
                --depth;
                newline();
                out += c;
                break;
            case ',':
                out += c;
                newline();
                break;
            case ':':
                out += ": ";
                break;
            default:
                out += c; // numbers, true, false, null
                break;
            }
        }
        return out;
    }

    QString timestamp(const QDateTime &time) {
        return time.isValid() ? time.toString("yyyy-MM-dd HH:mm:ss.zzz") : QStringLiteral("—");
    }

    QString byteCount(qsizetype bytes) {
        return bytes == 1 ? QStringLiteral("1 byte") : QString("%1 bytes").arg(bytes);
    }

    // "Name: value\r\n" lines, as received, one per output line.
    QStringList headerLines(const QByteArray &headers) {
        QStringList lines;
        for (const QByteArray &line : headers.split('\n')) {
            const QByteArray trimmed = line.trimmed();
            if (!trimmed.isEmpty())
                lines.append(QString::fromUtf8(trimmed));
        }
        return lines;
    }

    // Plain-text report of the request and response, for the Console tab.
    // The response body is shown in the Body tab: here only its size.
    QString consoleText(const ResponseData &response) {
        QStringList out;
        const SentRequest &request = response.request;

        // Missing for responses restored from an older session file.
        if (!request.method.isEmpty()) {
            out << QString("▶ REQUEST   %1").arg(timestamp(request.sentAt));
            out << QString("%1 %2").arg(request.method,
                                        request.url.toString(QUrl::FullyEncoded));
            out << QString();
            out << "Headers";
            if (request.headers.isEmpty())
                out << "  (none)";
            for (const auto &header : request.headers)
                out << QString("  %1: %2").arg(QString::fromLatin1(header.first),
                                               QString::fromUtf8(header.second));
            out << QString();
            out << QString("Body (%1)").arg(byteCount(request.body.size()));
            if (!request.body.isEmpty())
                out << QString::fromUtf8(request.body);
            out << QString() << QString();
        }

        out << QString("◀ RESPONSE  %1  (%2 ms)")
                   .arg(timestamp(response.receivedAt))
                   .arg(response.elapsedMs);
        if (response.statusCode > 0)
            out << QString("%1 %2").arg(response.statusCode).arg(response.reasonPhrase).trimmed();
        else
            out << "No HTTP response";
        if (!response.error.isEmpty())
            out << QString("Error: %1").arg(response.error);
        if (response.finalUrl.isValid() && !response.finalUrl.isEmpty()
            && response.finalUrl != request.url)
            out << QString("Final URL (after redirect): %1")
                       .arg(response.finalUrl.toString(QUrl::FullyEncoded));
        out << QString();
        out << "Headers";
        const QStringList received = headerLines(response.headers);
        if (received.isEmpty())
            out << "  (none)";
        for (const QString &line : received)
            out << "  " + line;
        out << QString();
        out << QString("Body: %1").arg(byteCount(response.body.size()));

        return out.join('\n');
    }
} // namespace

// Colors keys, string values and other values (numbers, booleans, null)
// of the JSON shown in the body view. Works line by line, which is enough
// for JSON: a string can never span lines (newlines must be escaped).
// Active only while the body holds valid JSON.
class JsonHighlighter final : public QSyntaxHighlighter {
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

    void setColors(const QColor &key, const QColor &string, const QColor &other) {
        keyFormat.setForeground(key);
        stringFormat.setForeground(string);
        otherFormat.setForeground(other);
        rehighlight();
    }

    // Must be called before the text changes: setPlainText re-highlights.
    void setEnabled(bool on) { enabled = on; }

protected:
    void highlightBlock(const QString &text) override {
        if (!enabled)
            return;
        const qsizetype n = text.size();
        qsizetype i = 0;
        while (i < n) {
            const QChar c = text[i];
            if (c == '"') {
                const qsizetype start = i++;
                bool escaped = false;
                while (i < n) {
                    const QChar d = text[i++];
                    if (escaped)
                        escaped = false;
                    else if (d == '\\')
                        escaped = true;
                    else if (d == '"')
                        break;
                }
                // A string followed by ':' is an object key.
                qsizetype next = i;
                while (next < n && text[next].isSpace())
                    ++next;
                const bool isKey = next < n && text[next] == ':';
                setFormat(int(start), int(i - start), isKey ? keyFormat : stringFormat);
            } else if (c == '-' || c.isLetterOrNumber()) {
                // numbers (incl. -1.5e+3), true, false, null
                const qsizetype start = i;
                while (i < n && (text[i].isLetterOrNumber() || text[i] == '-'
                                 || text[i] == '+' || text[i] == '.'))
                    ++i;
                setFormat(int(start), int(i - start), otherFormat);
            } else {
                ++i;
            }
        }
    }

private:
    bool enabled = false;
    QTextCharFormat keyFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat otherFormat;
};

ResponseComponent::ResponseComponent(QWidget *parent)
    : QWidget(parent) {
    setMinimumWidth(260);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel("RESPONSE");
    title->setObjectName("sectionTitle");
    statusLabel = new QLabel("—");
    statusLabel->setObjectName("responseStatus");
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(statusLabel);
    layout->addLayout(titleRow);

    metaLabel = new QLabel("Waiting for a request");
    metaLabel->setObjectName("muted");
    layout->addWidget(metaLabel);

    auto *tabs = new QTabWidget;
    bodyView = new QPlainTextEdit;
    bodyView->setReadOnly(true);
    bodyView->setPlaceholderText("Response body will appear here.");
    EditorStyle::applyWhiteBackground(bodyView);
    jsonHighlighter = new JsonHighlighter(bodyView->document());

    headersView = new QPlainTextEdit;
    headersView->setReadOnly(true);
    headersView->setPlaceholderText("Response headers will appear here.");

    tabs->addTab(bodyView, "Body");
    tabs->addTab(headersView, "Headers");
    consoleView = new QPlainTextEdit;
    consoleView->setReadOnly(true);
    consoleView->setLineWrapMode(QPlainTextEdit::NoWrap);
    consoleView->setPlaceholderText("Details of the request sent and of the response received will appear here.");

    tabs->addTab(consoleView, "Console");
    layout->addWidget(tabs, 1);
}

void ResponseComponent::setResponseFont(const QFont &font) {
    bodyView->setFont(font);
    headersView->setFont(font);
    consoleView->setFont(font);
}

void ResponseComponent::setJsonColors(const QColor &key, const QColor &string, const QColor &other) {
    jsonHighlighter->setColors(key, string, other);
}

void ResponseComponent::clear() {
    statusLabel->setText("—");
    metaLabel->setText("Waiting for a request");
    jsonHighlighter->setEnabled(false);
    headersView->clear();
    bodyView->clear();
    consoleView->clear();
}

void ResponseComponent::showResponse(const ResponseData &response) {
    const int statusCode = response.statusCode;
    const qint64 elapsedMs = response.elapsedMs;
    const QByteArray &headers = response.headers;
    const QByteArray &body = response.body;
    const QString &networkError = response.error;

    if (statusCode > 0) {
        statusLabel->setText(QString::number(statusCode));
        metaLabel->setText(QString("%1 ms  ·  %2")
                               .arg(elapsedMs)
                               .arg(body.size()) + " bytes");
    } else {
        statusLabel->setText("Network error");
        metaLabel->setText(QString("%1 ms").arg(elapsedMs));
    }

    headersView->setPlainText(QString::fromUtf8(headers));
    consoleView->setPlainText(consoleText(response));

    // Pretty-print valid JSON; leave all other payloads untouched.
    // QJsonDocument is used only to validate: re-serializing it would sort
    // the keys alphabetically (QJsonObject is an ordered map), so the
    // original text is re-indented instead, keeping the server's key order.
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(body, &parseError);
    const bool isJson = parseError.error == QJsonParseError::NoError && !json.isNull();
    // A network error message is prepended to the text below, which would
    // no longer be pure JSON: highlight only clean JSON bodies.
    jsonHighlighter->setEnabled(isJson && networkError.isEmpty());
    if (isJson) {
        const std::string indented = indentJson(body.toStdString());
        bodyView->setPlainText(QString::fromUtf8(indented.data(),
                                                 static_cast<qsizetype>(indented.size())));
    } else {
        bodyView->setPlainText(QString::fromUtf8(body));
    }

    if (!networkError.isEmpty()) {
        QString current = bodyView->toPlainText();
        if (!current.isEmpty())
            current.prepend(networkError + "\n\n");
        else
            current = networkError;
        bodyView->setPlainText(current);
    }
}
