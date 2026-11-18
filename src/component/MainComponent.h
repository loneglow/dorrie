//
// Created by gsoldano on 11/9/26.
//

#ifndef MAINCOMPONENT_H
#define MAINCOMPONENT_H
#include <QApplication>
#include <QDockWidget>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QTabBar>
#include <QCloseEvent>
#include <QPointer>

#include "AppPaths.h"
#include "HttpClient.h"
#include "RequestComponent.h"
#include "RequestTreeComponent.h"
#include "ResponseComponent.h"
#include "SessionStore.h"
#include "SettingsDialog.h"


class MainComponent final : public QMainWindow {

    Q_OBJECT

    QSplitter* splitter = nullptr;
    QTabWidget* requestTabs = nullptr;
    QDockWidget* terminalDock = nullptr;
    HttpClient *httpClient = nullptr;
    ResponseComponent *responseComponent = nullptr;
    RequestTreeComponent *requestTree = nullptr;
    // Response font from Settings, also used by the request body editors.
    QFont editorFont;

    void createMenus() {
        auto *file = menuBar()->addMenu("&File");
        auto *newAction = file->addAction("New Request", this, [this] {
            addRequestTab();
        });
        newAction->setShortcut(QKeySequence::New);
        auto *saveAction = file->addAction("Save Request", this, [this] { saveCurrentRequest(false); });
        saveAction->setShortcut(QKeySequence::Save);
        auto *saveAsAction = file->addAction("Save Request As…", this, [this] { saveCurrentRequest(true); });
        saveAsAction->setShortcut(QKeySequence::SaveAs);
        file->addSeparator();
        auto *settingsAction = file->addAction("Settings…", this, [this] { openSettings(); });
        settingsAction->setShortcut(QKeySequence::Preferences);
        settingsAction->setMenuRole(QAction::PreferencesRole);
        file->addSeparator();
        file->addAction("Quit", this, &QWidget::close);

        auto *view = menuBar()->addMenu("&View");
        view->addAction("Toggle Terminal", this, [this] {
            if (terminalDock)
                terminalDock->setVisible(!terminalDock->isVisible());
        });
    }

    void createToolbar() {
        auto* toolbar = addToolBar("Main");
        toolbar->setMovable(false);
        toolbar->addAction("New Request", this, [this] { addRequestTab(); });
        toolbar->addAction("Terminal", this, [this] {
            if (terminalDock)
                terminalDock->setVisible(!terminalDock->isVisible());
        });
    }

    QWidget *createResponsePanel() {
        auto *panel = new QWidget;
        panel->setMinimumWidth(260);
        auto *layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(8);

        auto *titleRow = new QHBoxLayout;
        auto *title = new QLabel("RESPONSE");
        title->setObjectName("sectionTitle");
        auto *status = new QLabel("—");
        status->setObjectName("responseStatus");
        titleRow->addWidget(title);
        titleRow->addStretch();
        titleRow->addWidget(status);
        layout->addLayout(titleRow);

        auto *meta = new QLabel("Waiting for a request");
        meta->setObjectName("muted");
        layout->addWidget(meta);

        auto *tabs = new QTabWidget;
        auto *body = new QPlainTextEdit;
        body->setReadOnly(true);
        body->setPlaceholderText("Response body will appear here.");
        auto *headers = new QPlainTextEdit;
        headers->setReadOnly(true);
        headers->setPlaceholderText("Response headers will appear here.");
        tabs->addTab(body, "Body");
        tabs->addTab(headers, "Headers");
        tabs->addTab(new QLabel("Timing information will appear here."), "Timing");
        layout->addWidget(tabs, 1);
        return panel;
    }

    void createSidebarAndWorkspace() {
        splitter = new QSplitter(Qt::Horizontal);
        requestTree = new RequestTreeComponent;
        splitter->addWidget(requestTree);
        connect(requestTree, &RequestTreeComponent::openRequested,
                this, &MainComponent::openSavedRequest);
        connect(requestTree, &RequestTreeComponent::requestRenamed, this,
                [this](const QString &id, const QString &name) {
                    if (RequestComponent *editor = findOpenRequest(id))
                        requestTabs->setTabText(requestTabs->indexOf(editor), name);
                });
        connect(requestTree, &RequestTreeComponent::requestsRemoved, this,
                [this](const QStringList &ids) {
                    // Tabs stay open as unsaved requests: Save will ask where to put them.
                    for (const QString &id : ids)
                        if (RequestComponent *editor = findOpenRequest(id))
                            editor->setSavedRequestId({});
                });

        auto *center = new QWidget;
        auto *centerLayout = new QVBoxLayout(center);
        centerLayout->setContentsMargins(0, 0, 0, 0);
        centerLayout->setSpacing(0);

        requestTabs = new QTabWidget;
        requestTabs->setTabsClosable(true);
        requestTabs->setMovable(true);
        connect(requestTabs, &QTabWidget::tabCloseRequested,
                this, [this](int index) {
                    // removeTab() does not delete the page: destroy it explicitly.
                    QWidget *page = requestTabs->widget(index);
                    requestTabs->removeTab(index);
                    if (page)
                        page->deleteLater();
                });
        // Each tab has its own response: show it when switching tabs.
        connect(requestTabs, &QTabWidget::currentChanged,
                this, [this] { showCurrentResponse(); });
        // Middle click on a tab closes it (see eventFilter).
        requestTabs->tabBar()->installEventFilter(this);
        centerLayout->addWidget(requestTabs, 1);

        splitter->addWidget(center);

        responseComponent = new ResponseComponent;
        splitter->addWidget(responseComponent);

        //splitter->addWidget(createResponsePanel());
        splitter->setStretchFactor(0, 0);
        splitter->setStretchFactor(1, 3);
        splitter->setStretchFactor(2, 2);
        splitter->setSizes({230, 650, 520});

        setCentralWidget(splitter);
        // The first tabs are opened by restoreSession().
    }

    void createTerminal() {
        terminalDock = new QDockWidget("Console", this);
        terminalDock->setObjectName("TerminalDock");
        terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea);
        auto *console = new QPlainTextEdit;
        console->setReadOnly(true);
        console->setMaximumBlockCount(2000);
        console->setPlaceholderText("Application output and request diagnostics will appear here.");
        terminalDock->setWidget(console);
        addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
        terminalDock->hide();
    }

    RequestComponent *addRequestTab(const QString &title = "New Request") {
        auto *editor = new RequestComponent();
        editor->setBodyFont(editorFont);
        const int index = requestTabs->addTab(editor, title);
        requestTabs->setCurrentIndex(index);

        connect(editor, &RequestComponent::sendRequested, this,
            [this, editor] { sendRequest(editor); });
        return editor;
    }

    // Open tab bound to a saved request, if any.
    RequestComponent *findOpenRequest(const QString &id) const {
        for (int i = 0; i < requestTabs->count(); ++i) {
            auto *editor = qobject_cast<RequestComponent *>(requestTabs->widget(i));
            if (editor && editor->savedRequestId() == id)
                return editor;
        }
        return nullptr;
    }

    // Switches to the request's tab if it is already open, otherwise opens it.
    void openSavedRequest(const QString &id, const QString &name, const RequestData &data) {
        if (RequestComponent *editor = findOpenRequest(id)) {
            requestTabs->setCurrentWidget(editor);
            return;
        }
        RequestComponent *editor = addRequestTab(name);
        editor->setData(data);
        editor->setSavedRequestId(id);
    }

    // Save: overwrites the saved request the tab is bound to; a tab never
    // saved before (or Save As) asks for a name and a folder.
    void saveCurrentRequest(bool asNew) {
        auto *editor = qobject_cast<RequestComponent *>(requestTabs->currentWidget());
        if (!editor)
            return;

        const QString id = editor->savedRequestId();
        if (!asNew && !id.isEmpty() && requestTree->contains(id)) {
            requestTree->updateRequest(id, editor->data());
            statusBar()->showMessage("Request saved", 3000);
            return;
        }

        const int index = requestTabs->indexOf(editor);
        QString name;
        const QString newId = requestTree->saveAsNew(editor->data(), requestTabs->tabText(index), &name);
        if (newId.isEmpty())
            return; // cancelled
        editor->setSavedRequestId(newId);
        requestTabs->setTabText(index, name);
        statusBar()->showMessage("Request saved", 3000);
    }

    // Interface font for the whole application, response font for the
    // body/headers views. The response font is set explicitly on those views,
    // so it is not overridden by the application-wide font.
    void applyAppearance(const AppearanceSettings &appearance) {
        QApplication::setFont(appearance.uiFont);

        // The request body editors share the response font.
        editorFont = appearance.responseFont;
        if (requestTabs) {
            for (int i = 0; i < requestTabs->count(); ++i)
                if (auto *editor = qobject_cast<RequestComponent *>(requestTabs->widget(i)))
                    editor->setBodyFont(editorFont);
        }

        if (responseComponent) {
            responseComponent->setResponseFont(appearance.responseFont);
            responseComponent->setJsonColors(appearance.jsonKeyColor,
                                             appearance.jsonStringColor,
                                             appearance.jsonOtherColor);
        }
    }

    void openSettings() {
        SettingsDialog dialog(AppearanceSettings::load(), this);
        connect(&dialog, &SettingsDialog::applied, this,
                [this](const AppearanceSettings &appearance) {
                    appearance.save();
                    applyAppearance(appearance);
                });
        dialog.exec();
    }

    void restoreLayout() {
        QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
        if (settings.contains("geometry"))
            restoreGeometry(settings.value("geometry").toByteArray());
        if (settings.contains("windowState"))
            restoreState(settings.value("windowState").toByteArray());
        if (splitter && settings.contains("workspaceSplitter"))
            splitter->restoreState(settings.value("workspaceSplitter").toByteArray());
    }

    void sendRequest(RequestComponent *requestComponent) {
        const QStringList missing = requestComponent->unresolvedPathVariables();
        if (!missing.isEmpty()) {
            requestComponent->showPathTab();
            QMessageBox::warning(this, "Missing path variables",
                                 QString("Enter a value in the Path tab for: %1")
                                     .arg(missing.join(", ")));
            return;
        }

        const QUrl url = requestComponent->requestUrl();

        if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
            QMessageBox::warning(this, "Invalid URL",
                                 "Enter a valid absolute URL, for example https://example.com/api.");
            return;
        }

        requestComponent->setSending(true);
        statusBar()->showMessage("Sending request…");

        // The response is kept by the tab that sent the request, and shown
        // only if that tab is the one on screen. QPointer: the tab may be
        // closed before the reply arrives.
        QPointer<RequestComponent> editor(requestComponent);
        httpClient->send(
            requestComponent->method(),
            url,
            requestComponent->requestHeaders(),
            requestComponent->requestBody(),
            [this, editor](const ResponseData &response) {
                if (!editor)
                    return;
                editor->setSending(false);
                editor->setLastResponse(response);
                if (requestTabs->currentWidget() == editor)
                    responseComponent->showResponse(response);

                statusBar()->showMessage(
                    response.statusCode > 0
                        ? QString("Request completed: HTTP %1 (%2 ms)")
                              .arg(response.statusCode).arg(response.elapsedMs)
                        : QString("Request failed: %1").arg(response.error),
                    7000
                );
            }
        );
    }

    // Shows the last response of the tab on screen, or an empty panel.
    void showCurrentResponse() {
        if (!responseComponent || !requestTabs)
            return;
        const auto *editor = qobject_cast<RequestComponent *>(requestTabs->currentWidget());
        if (editor && editor->lastResponse())
            responseComponent->showResponse(*editor->lastResponse());
        else
            responseComponent->clear();
    }

    // Open tabs (with their unsaved edits and last responses) are written
    // on close and reopened at the next start.
    void saveSession() const {
        Session session;
        session.currentIndex = requestTabs->currentIndex();
        for (int i = 0; i < requestTabs->count(); ++i) {
            const auto *editor = qobject_cast<RequestComponent *>(requestTabs->widget(i));
            if (!editor)
                continue;
            session.tabs.push_back({
                requestTabs->tabText(i),
                editor->savedRequestId(),
                editor->data(),
                editor->lastResponse(),
            });
        }

        QString error;
        if (!SessionStore::save(session, &error))
            qWarning("%s", qPrintable(error));
    }

    void restoreSession() {
        QString error;
        const Session session = SessionStore::load(&error);
        if (!error.isEmpty())
            statusBar()->showMessage(error, 10000);

        for (const SessionTab &tab : session.tabs) {
            RequestComponent *editor = addRequestTab(tab.title);
            editor->setData(tab.request);
            // The saved request may have been deleted in the meantime:
            // the tab then stays open as an unsaved request.
            if (!tab.savedId.isEmpty() && requestTree->contains(tab.savedId))
                editor->setSavedRequestId(tab.savedId);
            editor->setLastResponse(tab.response);
        }

        if (requestTabs->count() == 0)
            addRequestTab();
        else if (session.currentIndex >= 0 && session.currentIndex < requestTabs->count())
            requestTabs->setCurrentIndex(session.currentIndex);

        // setCurrentIndex() does not notify when the index is unchanged.
        showCurrentResponse();
    }

protected:
    // Saved here rather than in the destructor, while every tab still exists.
    void closeEvent(QCloseEvent *event) override {
        saveSession();
        QMainWindow::closeEvent(event);
    }

    // Middle click on a request tab closes it, as is usual on Linux.
    // Acting on release (like the close button) lets the user cancel by
    // moving the pointer off the tab before letting go.
    bool eventFilter(QObject *watched, QEvent *event) override {
        if (requestTabs && watched == requestTabs->tabBar()
            && event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::MiddleButton) {
                const int index = requestTabs->tabBar()->tabAt(mouse->position().toPoint());
                if (index >= 0) {
                    emit requestTabs->tabCloseRequested(index);
                    return true;
                }
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

public:
    MainComponent(QWidget *parent = nullptr)
        : QMainWindow(parent),
          httpClient(new HttpClient(this)) {
        setWindowTitle("Dorrie");
        resize(1280, 720);

        createMenus();
        createSidebarAndWorkspace();
        createTerminal();
        applyAppearance(AppearanceSettings::load());

        statusBar()->showMessage("Ready");
        restoreSession();
        restoreLayout();
    }

    ~MainComponent() override {
        QSettings settings(AppPaths::settingsFile(), QSettings::IniFormat);
        settings.setValue("geometry", saveGeometry());
        settings.setValue("windowState", saveState());
        if (splitter)
            settings.setValue("workspaceSplitter", splitter->saveState());
    }


};



#endif //MAINCOMPONENT_H
