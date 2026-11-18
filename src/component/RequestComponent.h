//
// Created by gsoldano on 11/9/26.
//

#ifndef REQUESTCOMPONENT_H
#define REQUESTCOMPONENT_H

#include <algorithm>
#include <functional>
#include <optional>

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QFormLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QHeaderView>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QUrlQuery>
#include <QHash>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTabWidget>

#include "EditorStyle.h"
#include "RequestStore.h"
#include "ResponseData.h"

class RequestComponent final : public QWidget {
    Q_OBJECT

    QVBoxLayout *layout = nullptr;
    QComboBox *methodCombo = nullptr;
    QLineEdit *urlEdit = nullptr;
    QPushButton *sendButton = nullptr;
    QTableWidget *paramsTable = nullptr;
    QTableWidget *headersTable = nullptr;
    QPlainTextEdit *bodyEditor = nullptr;
    QTabWidget *sections = nullptr;
    QTableWidget *pathTable = nullptr;

    // Auth tab: the selector switches the page shown below it. Every page
    // keeps its own fields, so credentials typed for one type are still
    // there after selecting another type and coming back.
    QComboBox *authTypeCombo = nullptr;
    QStackedWidget *authPages = nullptr;
    QLineEdit *basicUsernameEdit = nullptr;
    QLineEdit *basicPasswordEdit = nullptr;

    // Values typed in the Path tab, by variable name. Kept even when the
    // variable disappears from the URL, so that retyping it (or fixing a
    // typo elsewhere in the URL) does not lose the value.
    QHash<QString, QString> pathValues;

    // Id of the saved request this editor was opened from or saved to;
    // empty for a request that has never been saved.
    QString savedId;

    // Last response received for this tab; empty until it is first sent.
    std::optional<ResponseData> response;

    signals:
        void sendRequested();

public:
    explicit RequestComponent(QWidget* parent = nullptr) : QWidget(parent) {
        layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto *urlRow = new QHBoxLayout;
        methodCombo = new QComboBox;
        methodCombo->addItems({"GET", "POST", "PUT", "PATCH", "DELETE", "HEAD", "OPTIONS"});
        methodCombo->setFixedWidth(75);

        urlEdit = new QLineEdit;
        urlEdit->setPlaceholderText("http://localhost:8080/api");
        EditorStyle::applyWhiteBackground(urlEdit);

        sendButton = new QPushButton("Send");
        sendButton->setObjectName("sendButton");
        urlRow->addWidget(methodCombo);
        urlRow->addWidget(urlEdit, 1);
        urlRow->addWidget(sendButton);
        layout->addLayout(urlRow);

        sections = new QTabWidget;

        // Filled automatically from the ":name" segments of the URL.
        pathTable = new QTableWidget(0, 2);
        pathTable->setHorizontalHeaderLabels({"Name", "Value"});
        pathTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        pathTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        pathTable->verticalHeader()->hide();
        pathTable->setAlternatingRowColors(true);

        paramsTable = new QTableWidget(0, 3);
        paramsTable->setHorizontalHeaderLabels({"Enabled", "Key", "Value"});
        paramsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        paramsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        paramsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        paramsTable->verticalHeader()->hide();

        headersTable = new QTableWidget(0, 3);
        headersTable->setHorizontalHeaderLabels({"Enabled", "Key", "Value"});
        headersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        headersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        headersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        headersTable->verticalHeader()->hide();

        bodyEditor = new QPlainTextEdit;
        bodyEditor->setPlaceholderText("{\n  \"example\": true\n}");
        EditorStyle::applyWhiteBackground(bodyEditor);

        // A page with the "Add" button on top and the table below it.
        auto editablePage = [](QTableWidget *table, const QString &addLabel,
                               const QString &deleteLabel) {
            auto *page = new QWidget;
            auto *pageLayout = new QVBoxLayout(page);
            pageLayout->setContentsMargins(0, 6, 0, 0);
            pageLayout->setSpacing(6);

            auto *button = new QPushButton(addLabel);
            button->setMaximumWidth(140);
            QObject::connect(button, &QPushButton::clicked, table, [table] {
                addEditableRow(table);
            });

            pageLayout->addWidget(button, 0, Qt::AlignLeft);
            pageLayout->addWidget(table, 1);
            installDeleteMenu(table, deleteLabel);
            return page;
        };

        auto *paramsPage = editablePage(paramsTable, "+ Add parameter", "Delete parameter");
        auto *headersPage = editablePage(headersTable, "+ Add header", "Delete header");

        // Add the pages (table + "Add" button), not the bare tables:
        // adding the tables directly would pull them out of their pages
        // and the "Add" buttons would never be shown.
        sections->addTab(pathTable, "Path");
        sections->addTab(paramsPage, "Query");
        sections->addTab(createAuthPage(), "Auth");
        sections->addTab(headersPage, "Headers");
        sections->addTab(bodyEditor, "Body");
        layout->addWidget(sections, 1);

        connect(sendButton, &QPushButton::clicked,
            this, &RequestComponent::sendRequested);

        // Keep the Path tab in sync with the variables typed in the URL.
        connect(urlEdit, &QLineEdit::textChanged,
                this, [this] { syncPathVariables(); });

        // Remember each value as soon as it is edited.
        connect(pathTable, &QTableWidget::itemChanged,
                this, [this](QTableWidgetItem *item) {
            if (item->column() != 1)
                return;
            if (const auto *nameItem = pathTable->item(item->row(), 0))
                pathValues.insert(nameItem->text(), item->text());
        });
    }

    static void addEditableRow(QTableWidget *table) {
        const int row = table->rowCount();
        table->insertRow(row);

        auto *enabled = new QTableWidgetItem;
        enabled->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        enabled->setCheckState(Qt::Checked);
        table->setItem(row, 0, enabled);
        table->setItem(row, 1, new QTableWidgetItem);
        table->setItem(row, 2, new QTableWidgetItem);
        table->editItem(table->item(row, 1));
    }

private:
    // Combo index == AuthData::Type == page index in authPages.
    QWidget *createAuthPage() {
        auto *page = new QWidget;
        auto *pageLayout = new QVBoxLayout(page);
        pageLayout->setContentsMargins(0, 6, 0, 0);
        pageLayout->setSpacing(10);

        auto *typeRow = new QHBoxLayout;
        typeRow->addWidget(new QLabel("Type"));
        authTypeCombo = new QComboBox;
        authTypeCombo->addItem("No Auth");
        authTypeCombo->addItem("Basic");
        typeRow->addWidget(authTypeCombo);
        typeRow->addStretch();
        pageLayout->addLayout(typeRow);

        authPages = new QStackedWidget;

        auto *noAuthPage = new QLabel("This request does not use any authorization.");
        noAuthPage->setObjectName("muted");
        noAuthPage->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        authPages->addWidget(noAuthPage);

        auto *basicPage = new QWidget;
        auto *basicForm = new QFormLayout(basicPage);
        basicForm->setContentsMargins(0, 0, 0, 0);
        basicUsernameEdit = new QLineEdit;
        basicPasswordEdit = new QLineEdit;
        basicPasswordEdit->setEchoMode(QLineEdit::Password);
        auto *showPassword = new QCheckBox("Show password");
        connect(showPassword, &QCheckBox::toggled, basicPasswordEdit, [this](bool on) {
            basicPasswordEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
        });
        basicForm->addRow("Username", basicUsernameEdit);
        basicForm->addRow("Password", basicPasswordEdit);
        basicForm->addRow(QString(), showPassword);
        authPages->addWidget(basicPage);

        pageLayout->addWidget(authPages);
        pageLayout->addStretch();

        connect(authTypeCombo, &QComboBox::currentIndexChanged,
                authPages, &QStackedWidget::setCurrentIndex);
        return page;
    }

    AuthData authData() const {
        AuthData auth;
        auth.type = static_cast<AuthData::Type>(authTypeCombo->currentIndex());
        auth.basicUsername = basicUsernameEdit->text();
        auth.basicPassword = basicPasswordEdit->text();
        return auth;
    }

    void setAuthData(const AuthData &auth) {
        basicUsernameEdit->setText(auth.basicUsername);
        basicPasswordEdit->setText(auth.basicPassword);
        authTypeCombo->setCurrentIndex(static_cast<int>(auth.type));
    }

    // Value of the Authorization header produced by the Auth tab, or empty
    // when it adds none. Basic (RFC 7617): base64 of "username:password",
    // encoded as UTF-8.
    QByteArray authorizationHeader() const {
        if (authTypeCombo->currentIndex() != static_cast<int>(AuthData::Type::Basic))
            return {};
        const QString username = basicUsernameEdit->text();
        const QString password = basicPasswordEdit->text();
        if (username.isEmpty() && password.isEmpty())
            return {};
        return "Basic " + (username + ':' + password).toUtf8().toBase64();
    }

    // Right-click on a row offers to delete it. If the clicked row is part
    // of a multi-row selection, the whole selection is deleted.
    static void installDeleteMenu(QTableWidget *table, const QString &label) {
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(table, &QWidget::customContextMenuRequested, table,
                         [table, label](const QPoint &pos) {
            const int clickedRow = table->rowAt(pos.y());
            if (clickedRow < 0)
                return;

            QList<int> rows;
            for (const QModelIndex &index : table->selectionModel()->selectedRows())
                rows.append(index.row());
            if (!rows.contains(clickedRow)) {
                rows = {clickedRow};
                table->selectRow(clickedRow);
            }

            QMenu menu(table);
            const QString text = rows.size() > 1
                ? QStringLiteral("%1 (%2 rows)").arg(label).arg(rows.size())
                : label;
            const QAction *deleteAction = menu.addAction(text);
            if (menu.exec(table->viewport()->mapToGlobal(pos)) != deleteAction)
                return;

            // Remove from the bottom so the remaining indexes stay valid.
            std::sort(rows.begin(), rows.end(), std::greater<>());
            for (const int row : rows)
                table->removeRow(row);
        });
    }

    // Rows with both key and value empty are skipped.
    static QList<KeyValueRow> readRows(const QTableWidget *table) {
        QList<KeyValueRow> rows;
        for (int row = 0; row < table->rowCount(); ++row) {
            const auto *enabled = table->item(row, 0);
            const auto *key = table->item(row, 1);
            const auto *value = table->item(row, 2);
            KeyValueRow entry{
                !enabled || enabled->checkState() == Qt::Checked,
                key ? key->text() : QString(),
                value ? value->text() : QString(),
            };
            if (!entry.key.isEmpty() || !entry.value.isEmpty())
                rows.append(entry);
        }
        return rows;
    }

    static void writeRows(QTableWidget *table, const QList<KeyValueRow> &rows) {
        table->setRowCount(0);
        for (const KeyValueRow &entry : rows) {
            const int row = table->rowCount();
            table->insertRow(row);
            auto *enabled = new QTableWidgetItem;
            enabled->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            enabled->setCheckState(entry.enabled ? Qt::Checked : Qt::Unchecked);
            table->setItem(row, 0, enabled);
            table->setItem(row, 1, new QTableWidgetItem(entry.key));
            table->setItem(row, 2, new QTableWidgetItem(entry.value));
        }
    }

    // Path variables are segments written as ":name" right after a '/',
    // e.g. https://host/api/users/:id/posts/:postId
    static const QRegularExpression &pathVariablePattern() {
        static const QRegularExpression pattern(QStringLiteral("/:([A-Za-z0-9_-]+)"));
        return pattern;
    }

    // Length of the path part of the URL text (query string and fragment
    // excluded), so that a ":" inside them is never treated as a variable.
    static qsizetype pathPartLength(const QString &text) {
        qsizetype end = text.size();
        for (const QChar c : {QChar('?'), QChar('#')}) {
            const qsizetype pos = text.indexOf(c);
            if (pos >= 0 && pos < end)
                end = pos;
        }
        return end;
    }

    // Variable names in order of appearance, without duplicates.
    static QStringList pathVariableNames(const QString &text) {
        QStringList names;
        auto matches = pathVariablePattern().globalMatch(text.left(pathPartLength(text)));
        while (matches.hasNext()) {
            const QString name = matches.next().captured(1);
            if (!names.contains(name))
                names.append(name);
        }
        return names;
    }

    void syncPathVariables() {
        const QStringList names = pathVariableNames(urlEdit->text());

        QStringList shown;
        for (int row = 0; row < pathTable->rowCount(); ++row)
            if (const auto *item = pathTable->item(row, 0))
                shown.append(item->text());

        // Rebuild only when the set of variables actually changed.
        if (names == shown)
            return;

        const QSignalBlocker blocker(pathTable);
        pathTable->setRowCount(static_cast<int>(names.size()));
        for (int row = 0; row < names.size(); ++row) {
            auto *nameItem = new QTableWidgetItem(names.at(row));
            nameItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); // read-only
            pathTable->setItem(row, 0, nameItem);
            pathTable->setItem(row, 1, new QTableWidgetItem(pathValues.value(names.at(row))));
        }
    }

    // URL text with every ":name" segment replaced by its percent-encoded
    // value. Variables without a value are left untouched.
    QString resolvedUrlText() const {
        const QString text = urlEdit->text().trimmed();
        const qsizetype pathLength = pathPartLength(text);
        const QString path = text.left(pathLength);

        QString result;
        qsizetype last = 0;
        auto matches = pathVariablePattern().globalMatch(path);
        while (matches.hasNext()) {
            const auto match = matches.next();
            const QString value = pathValues.value(match.captured(1)).trimmed();
            result += path.mid(last, match.capturedStart() - last);
            if (value.isEmpty())
                result += match.captured(0);
            else
                result += QStringLiteral("/") + QString::fromLatin1(QUrl::toPercentEncoding(value));
            last = match.capturedEnd();
        }
        result += path.mid(last);
        result += text.mid(pathLength);
        return result;
    }

public:
    // Path variables present in the URL that still have no value.
    QStringList unresolvedPathVariables() const {
        QStringList missing;
        for (const QString &name : pathVariableNames(urlEdit->text()))
            if (pathValues.value(name).trimmed().isEmpty())
                missing.append(name);
        return missing;
    }

    void showPathTab() {
        sections->setCurrentWidget(pathTable);
    }

    QString method() const {
        return methodCombo->currentText();
    }

    QUrl requestUrl() const {
        QUrl url = QUrl::fromUserInput(resolvedUrlText());
        QUrlQuery query(url);

        for (int row = 0; row < paramsTable->rowCount(); ++row) {
            const auto *enabled = paramsTable->item(row, 0);
            const auto *key = paramsTable->item(row, 1);
            const auto *value = paramsTable->item(row, 2);

            if (!enabled || enabled->checkState() != Qt::Checked || !key)
                continue;

            const QString name = key->text().trimmed();
            if (!name.isEmpty())
                query.addQueryItem(name, value ? value->text() : QString());
        }

        url.setQuery(query);
        return url;
    }

    // Headers table plus the Authorization header from the Auth tab.
    // When the Auth tab adds one, it replaces any Authorization row of the
    // table, so the request never carries two of them.
    QList<QPair<QByteArray, QByteArray>> requestHeaders() const {
        QList<QPair<QByteArray, QByteArray>> result;
        const QByteArray authorization = authorizationHeader();

        for (int row = 0; row < headersTable->rowCount(); ++row) {
            const auto *enabled = headersTable->item(row, 0);
            const auto *key = headersTable->item(row, 1);
            const auto *value = headersTable->item(row, 2);

            if (!enabled || enabled->checkState() != Qt::Checked || !key)
                continue;

            const QByteArray name = key->text().trimmed().toLatin1();
            if (name.isEmpty())
                continue;
            if (!authorization.isEmpty() && name.compare("Authorization", Qt::CaseInsensitive) == 0)
                continue;
            result.append({name, value ? value->text().toUtf8() : QByteArray()});
        }

        if (!authorization.isEmpty())
            result.append({"Authorization", authorization});
        return result;
    }

    // Set explicitly, so it is not overridden by the application-wide font.
    void setBodyFont(const QFont &font) {
        bodyEditor->setFont(font);
    }

    QByteArray requestBody() const {
        return bodyEditor->toPlainText().toUtf8();
    }

    QString savedRequestId() const { return savedId; }
    void setSavedRequestId(const QString &id) { savedId = id; }

    const std::optional<ResponseData> &lastResponse() const { return response; }
    void setLastResponse(const std::optional<ResponseData> &value) { response = value; }

    // Current content of the editor, as it is saved to disk.
    RequestData data() const {
        RequestData result;
        result.method = methodCombo->currentText();
        result.url = urlEdit->text();
        // Only the variables still present in the URL are worth saving.
        for (const QString &name : pathVariableNames(result.url))
            result.pathValues.insert(name, pathValues.value(name));
        result.params = readRows(paramsTable);
        result.headers = readRows(headersTable);
        result.body = bodyEditor->toPlainText();
        result.auth = authData();
        return result;
    }

    // Replaces the content of the editor with a saved request.
    void setData(const RequestData &request) {
        int index = methodCombo->findText(request.method);
        if (index < 0) {
            methodCombo->addItem(request.method);
            index = methodCombo->count() - 1;
        }
        methodCombo->setCurrentIndex(index);

        // Set the values before the URL, then force the Path table to be
        // rebuilt: syncPathVariables() skips the rebuild when the variable
        // names are unchanged, which would keep the old values on screen.
        pathValues = request.pathValues;
        {
            const QSignalBlocker blocker(pathTable);
            pathTable->setRowCount(0);
        }
        urlEdit->setText(request.url);
        syncPathVariables();

        writeRows(paramsTable, request.params);
        writeRows(headersTable, request.headers);
        bodyEditor->setPlainText(request.body);
        setAuthData(request.auth);
    }

    void setSending(bool sending) {
        sendButton->setEnabled(!sending);
        sendButton->setText(sending ? "Sending…" : "Send");
    }

};



#endif //REQUESTCOMPONENT_H
