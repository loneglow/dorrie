//
// Created by gsoldano on 11/10/26.
//

#ifndef REQUESTTREECOMPONENT_H
#define REQUESTTREECOMPONENT_H
#include <QHash>
#include <QTreeWidget>
#include <QWidget>

#include "RequestStore.h"

// QTreeWidget that reports when items have been moved by drag & drop.
class RequestTreeWidget final : public QTreeWidget {
    Q_OBJECT

public:
    using QTreeWidget::QTreeWidget;

signals:
    void itemsMoved();

protected:
    void dropEvent(QDropEvent *event) override {
        QTreeWidget::dropEvent(event);
        emit itemsMoved();
    }
};

// Left sidebar: saved requests organised in nested folders.
// Every change (create, rename, move, delete, save) is written immediately
// to ~/.config/dorrie/requests.json through RequestStore.
class RequestTreeComponent final : public QWidget {
    Q_OBJECT

public:
    explicit RequestTreeComponent(QWidget *parent = nullptr);

    bool contains(const QString &id) const { return requests.contains(id); }
    QString nameOf(const QString &id) const;

    // Overwrites a saved request with the editor's content.
    void updateRequest(const QString &id, const RequestData &data);

    // Asks for a name and a destination folder, then saves a new request.
    // Returns the new id, or an empty string if the user cancelled.
    // *name receives the chosen name.
    QString saveAsNew(const RequestData &data, const QString &suggestedName, QString *name);

signals:
    void openRequested(const QString &id, const QString &name, const RequestData &data);
    void requestRenamed(const QString &id, const QString &name);
    void requestsRemoved(const QStringList &ids);

private:
    enum Role { TypeRole = Qt::UserRole, IdRole };
    enum Column { NameColumn = 0, MethodColumn = 1 };

    static bool isFolder(const QTreeWidgetItem *item);
    static QString idOf(const QTreeWidgetItem *item);

    QTreeWidgetItem *makeFolderItem(const QString &id, const QString &name);
    QTreeWidgetItem *makeRequestItem(const QString &id, const QString &name, const QString &method);
    void populate(QTreeWidgetItem *parent, const std::vector<RequestNode> &nodes);
    std::vector<RequestNode> collect(const QTreeWidgetItem *parent) const;
    QTreeWidgetItem *findItem(const QString &id) const;
    // The folder new items go into for the given selection (the invisible
    // root item stands for the top level).
    QTreeWidgetItem *folderFor(QTreeWidgetItem *item) const;

    void createFolder(QTreeWidgetItem *parent);
    void createRequest(QTreeWidgetItem *parent);
    void duplicateRequest(QTreeWidgetItem *item);
    void removeItem(QTreeWidgetItem *item);
    void showContextMenu(const QPoint &pos);
    void persist();

    RequestTreeWidget *tree = nullptr;
    QHash<QString, RequestData> requests; // request content by id
};

#endif //REQUESTTREECOMPONENT_H
