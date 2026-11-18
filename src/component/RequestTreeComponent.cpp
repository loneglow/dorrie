//
// Created by gsoldano on 11/10/26.
//

#include "RequestTreeComponent.h"

#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStyle>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

namespace {
    constexpr int FolderType = 0;
    constexpr int RequestType = 1;

    // Copies the folder structure of the sidebar into the destination tree
    // of the save dialog, remembering which sidebar folder each entry is.
    void copyFolders(const QTreeWidgetItem *source, QTreeWidgetItem *target,
                     QHash<const QTreeWidgetItem *, QTreeWidgetItem *> &map,
                     const QIcon &icon) {
        for (int i = 0; i < source->childCount(); ++i) {
            QTreeWidgetItem *child = source->child(i);
            if (child->data(0, Qt::UserRole).toInt() != FolderType)
                continue;
            auto *copy = new QTreeWidgetItem(target, {child->text(0)});
            copy->setIcon(0, icon);
            map.insert(copy, child);
            copyFolders(child, copy, map, icon);
        }
    }
} // namespace

RequestTreeComponent::RequestTreeComponent(QWidget *parent)
    : QWidget(parent) {
    setMinimumWidth(190);
    setMaximumWidth(360);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel("REQUESTS");
    title->setObjectName("sectionTitle");
    auto *add = new QPushButton("+");
    add->setFixedWidth(28);
    add->setToolTip("New request or folder");
    titleRow->addWidget(title);
    titleRow->addStretch();
    titleRow->addWidget(add);
    layout->addLayout(titleRow);

    tree = new RequestTreeWidget;
    tree->setColumnCount(2);
    tree->setHeaderHidden(true);
    tree->setIndentation(16);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    tree->header()->setSectionResizeMode(MethodColumn, QHeaderView::ResizeToContents);
    tree->setDragDropMode(QAbstractItemView::InternalMove);
    tree->setDefaultDropAction(Qt::MoveAction);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    // Double-click opens a request: rename with F2 or the context menu.
    tree->setEditTriggers(QAbstractItemView::EditKeyPressed);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(tree, 1);

    QString error;
    const std::vector<RequestNode> nodes = RequestStore::load(&error);
    {
        const QSignalBlocker blocker(tree);
        populate(tree->invisibleRootItem(), nodes);
    }
    if (!error.isEmpty())
        QMessageBox::warning(this, "Saved requests", error);

    auto *addMenu = new QMenu(add);
    addMenu->addAction("New Request", this, [this] { createRequest(folderFor(tree->currentItem())); });
    addMenu->addAction("New Folder", this, [this] { createFolder(folderFor(tree->currentItem())); });
    connect(add, &QPushButton::clicked, this, [add, addMenu] {
        addMenu->popup(add->mapToGlobal(QPoint(0, add->height())));
    });

    connect(tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item, int) {
        if (!isFolder(item)) {
            const QString id = idOf(item);
            emit openRequested(id, item->text(NameColumn), requests.value(id));
        }
    });

    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column) {
        if (column != NameColumn)
            return;
        QString name = item->text(NameColumn).trimmed();
        if (name.isEmpty())
            name = isFolder(item) ? "New Folder" : "New Request";
        if (name != item->text(NameColumn)) {
            const QSignalBlocker blocker(tree);
            item->setText(NameColumn, name);
        }
        persist();
        if (!isFolder(item))
            emit requestRenamed(idOf(item), name);
    });

    connect(tree, &RequestTreeWidget::itemsMoved, this, [this] { persist(); });
    connect(tree, &QWidget::customContextMenuRequested, this, &RequestTreeComponent::showContextMenu);
}

bool RequestTreeComponent::isFolder(const QTreeWidgetItem *item) {
    return item && item->data(NameColumn, TypeRole).toInt() == FolderType;
}

QString RequestTreeComponent::idOf(const QTreeWidgetItem *item) {
    return item ? item->data(NameColumn, IdRole).toString() : QString();
}

QString RequestTreeComponent::nameOf(const QString &id) const {
    const QTreeWidgetItem *item = findItem(id);
    return item ? item->text(NameColumn) : QString();
}

QTreeWidgetItem *RequestTreeComponent::makeFolderItem(const QString &id, const QString &name) {
    auto *item = new QTreeWidgetItem({name});
    item->setData(NameColumn, TypeRole, FolderType);
    item->setData(NameColumn, IdRole, id);
    item->setIcon(NameColumn, style()->standardIcon(QStyle::SP_DirIcon));
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable
                   | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    return item;
}

QTreeWidgetItem *RequestTreeComponent::makeRequestItem(const QString &id, const QString &name,
                                                       const QString &method) {
    auto *item = new QTreeWidgetItem({name, method});
    item->setData(NameColumn, TypeRole, RequestType);
    item->setData(NameColumn, IdRole, id);
    item->setTextAlignment(MethodColumn, Qt::AlignRight | Qt::AlignVCenter);
    item->setForeground(MethodColumn, palette().color(QPalette::PlaceholderText));
    // No ItemIsDropEnabled: a request cannot contain other items.
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable
                   | Qt::ItemIsDragEnabled);
    return item;
}

void RequestTreeComponent::populate(QTreeWidgetItem *parent, const std::vector<RequestNode> &nodes) {
    for (const RequestNode &node : nodes) {
        if (node.type == RequestNode::Type::Folder) {
            QTreeWidgetItem *item = makeFolderItem(node.id, node.name);
            parent->addChild(item);
            populate(item, node.children);
        } else {
            parent->addChild(makeRequestItem(node.id, node.name, node.request.method));
            requests.insert(node.id, node.request);
        }
    }
}

std::vector<RequestNode> RequestTreeComponent::collect(const QTreeWidgetItem *parent) const {
    std::vector<RequestNode> nodes;
    for (int i = 0; i < parent->childCount(); ++i) {
        const QTreeWidgetItem *item = parent->child(i);
        RequestNode node;
        node.id = idOf(item);
        node.name = item->text(NameColumn);
        if (isFolder(item)) {
            node.type = RequestNode::Type::Folder;
            node.children = collect(item);
        } else {
            node.type = RequestNode::Type::Request;
            node.request = requests.value(node.id);
        }
        nodes.push_back(std::move(node));
    }
    return nodes;
}

QTreeWidgetItem *RequestTreeComponent::findItem(const QString &id) const {
    for (QTreeWidgetItemIterator it(tree); *it; ++it)
        if (idOf(*it) == id)
            return *it;
    return nullptr;
}

QTreeWidgetItem *RequestTreeComponent::folderFor(QTreeWidgetItem *item) const {
    if (!item)
        return tree->invisibleRootItem();
    if (isFolder(item))
        return item;
    return item->parent() ? item->parent() : tree->invisibleRootItem();
}

void RequestTreeComponent::persist() {
    QString error;
    if (!RequestStore::save(collect(tree->invisibleRootItem()), &error))
        QMessageBox::warning(this, "Saved requests", error);
}

void RequestTreeComponent::createFolder(QTreeWidgetItem *parent) {
    QTreeWidgetItem *item = makeFolderItem(RequestStore::newId(), "New Folder");
    {
        const QSignalBlocker blocker(tree);
        parent->addChild(item);
    }
    parent->setExpanded(true);
    persist();
    tree->setCurrentItem(item);
    tree->editItem(item, NameColumn); // renaming saves again
}

void RequestTreeComponent::createRequest(QTreeWidgetItem *parent) {
    const QString id = RequestStore::newId();
    const RequestData data;
    requests.insert(id, data);
    QTreeWidgetItem *item = makeRequestItem(id, "New Request", data.method);
    {
        const QSignalBlocker blocker(tree);
        parent->addChild(item);
    }
    parent->setExpanded(true);
    persist();
    tree->setCurrentItem(item);
    emit openRequested(id, item->text(NameColumn), data);
    tree->editItem(item, NameColumn);
}

void RequestTreeComponent::duplicateRequest(QTreeWidgetItem *item) {
    const QString id = RequestStore::newId();
    const RequestData data = requests.value(idOf(item));
    requests.insert(id, data);
    QTreeWidgetItem *copy = makeRequestItem(id, item->text(NameColumn) + " copy", data.method);
    QTreeWidgetItem *parent = folderFor(item);
    {
        const QSignalBlocker blocker(tree);
        parent->insertChild(parent->indexOfChild(item) + 1, copy);
    }
    persist();
    tree->setCurrentItem(copy);
}

void RequestTreeComponent::removeItem(QTreeWidgetItem *item) {
    const bool folder = isFolder(item);
    const QString question = folder
        ? QString("Delete the folder \"%1\" and everything it contains?").arg(item->text(NameColumn))
        : QString("Delete the request \"%1\"?").arg(item->text(NameColumn));
    if (QMessageBox::question(this, folder ? "Delete folder" : "Delete request", question)
        != QMessageBox::Yes)
        return;

    QStringList removed;
    const auto gather = [&](auto &&self, const QTreeWidgetItem *node) -> void {
        if (!isFolder(node)) {
            removed.append(idOf(node));
            return;
        }
        for (int i = 0; i < node->childCount(); ++i)
            self(self, node->child(i));
    };
    gather(gather, item);

    for (const QString &id : removed)
        requests.remove(id);
    delete item; // also deletes its children
    persist();
    if (!removed.isEmpty())
        emit requestsRemoved(removed);
}

void RequestTreeComponent::showContextMenu(const QPoint &pos) {
    QTreeWidgetItem *item = tree->itemAt(pos);
    QMenu menu(this);

    if (item && !isFolder(item)) {
        menu.addAction("Open", this, [this, item] {
            emit openRequested(idOf(item), item->text(NameColumn), requests.value(idOf(item)));
        });
        menu.addAction("Duplicate", this, [this, item] { duplicateRequest(item); });
        menu.addSeparator();
    } else {
        QTreeWidgetItem *parent = item ? item : tree->invisibleRootItem();
        menu.addAction("New Request", this, [this, parent] { createRequest(parent); });
        menu.addAction("New Folder", this, [this, parent] { createFolder(parent); });
        if (item)
            menu.addSeparator();
    }

    if (item) {
        menu.addAction("Rename", this, [this, item] { tree->editItem(item, NameColumn); });
        menu.addAction("Delete", this, [this, item] { removeItem(item); });
    }

    menu.exec(tree->viewport()->mapToGlobal(pos));
}

void RequestTreeComponent::updateRequest(const QString &id, const RequestData &data) {
    QTreeWidgetItem *item = findItem(id);
    if (!item)
        return;
    requests.insert(id, data);
    {
        const QSignalBlocker blocker(tree);
        item->setText(MethodColumn, data.method);
    }
    persist();
}

QString RequestTreeComponent::saveAsNew(const RequestData &data, const QString &suggestedName,
                                        QString *name) {
    QDialog dialog(this);
    dialog.setWindowTitle("Save Request");
    dialog.setMinimumSize(360, 380);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout;
    auto *nameEdit = new QLineEdit(suggestedName);
    nameEdit->selectAll();
    form->addRow("Name:", nameEdit);
    layout->addLayout(form);

    layout->addWidget(new QLabel("Folder:"));
    auto *folders = new QTreeWidget;
    folders->setHeaderHidden(true);
    const QIcon folderIcon = style()->standardIcon(QStyle::SP_DirIcon);
    auto *root = new QTreeWidgetItem(folders, {"(top level)"});
    root->setIcon(0, folderIcon);
    QHash<const QTreeWidgetItem *, QTreeWidgetItem *> map; // dialog item -> sidebar folder
    map.insert(root, tree->invisibleRootItem());
    copyFolders(tree->invisibleRootItem(), root, map, folderIcon);
    folders->expandAll();
    layout->addWidget(folders, 1);

    // Preselect the folder of the sidebar selection.
    const QTreeWidgetItem *preferred = folderFor(tree->currentItem());
    folders->setCurrentItem(root);
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        if (it.value() == preferred)
            folders->setCurrentItem(const_cast<QTreeWidgetItem *>(it.key()));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    const auto updateSave = [buttons, nameEdit] {
        buttons->button(QDialogButtonBox::Save)->setEnabled(!nameEdit->text().trimmed().isEmpty());
    };
    connect(nameEdit, &QLineEdit::textChanged, &dialog, updateSave);
    updateSave();
    nameEdit->setFocus();

    if (dialog.exec() != QDialog::Accepted)
        return {};

    QTreeWidgetItem *parent = map.value(folders->currentItem(), tree->invisibleRootItem());
    const QString id = RequestStore::newId();
    const QString chosen = nameEdit->text().trimmed();
    requests.insert(id, data);
    QTreeWidgetItem *item = makeRequestItem(id, chosen, data.method);
    {
        const QSignalBlocker blocker(tree);
        parent->addChild(item);
    }
    parent->setExpanded(true);
    tree->setCurrentItem(item);
    persist();

    if (name)
        *name = chosen;
    return id;
}
