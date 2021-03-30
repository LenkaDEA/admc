/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020 BaseALT Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "find_results.h"
#include "object_actions.h"
#include "properties_dialog.h"
#include "utils.h"
#include "adldap.h"
#include "globals.h"
#include "object_model.h"
#include "settings.h"
#include "console_widget/customize_columns_dialog.h"
#include "console_widget/results_view.h"
#include "status.h"
#include "rename_dialog.h"
#include "move_dialog.h"
#include "select_container_dialog.h"
#include "create_dialog.h"
#include "select_dialog.h"
#include "password_dialog.h"

#include <QTreeView>
#include <QLabel>
#include <QHeaderView>
#include <QDebug>
#include <QVBoxLayout>
#include <QMenu>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHash>

FindResults::FindResults()
: QWidget()
{   
    object_actions = new ObjectActions(this);

    properties_action = new QAction("&Properties");

    model = new QStandardItemModel(this);

    const QList<QString> header_labels = object_model_header_labels();
    model->setHorizontalHeaderLabels(header_labels);

    view = new ResultsView(this);
    view->set_model(model);

    object_count_label = new QLabel();

    const auto layout = new QVBoxLayout();
    setLayout(layout);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    layout->addWidget(object_count_label);
    layout->addWidget(view);

    customize_columns_action = new QAction(tr("&Customize columns"), this);

    connect(
        customize_columns_action, &QAction::triggered,
        this, &FindResults::customize_columns);
    connect(
        view, &ResultsView::context_menu,
        this, &FindResults::on_context_menu);
    connect(
        view, &ResultsView::activated,
        this, &FindResults::properties);

    connect(
        properties_action, &QAction::triggered,
        this, &FindResults::properties);
    connect(
        object_actions->get(ObjectAction_NewUser), &QAction::triggered,
        this, &FindResults::create_user);
    connect(
        object_actions->get(ObjectAction_NewComputer), &QAction::triggered,
        this, &FindResults::create_computer);
    connect(
        object_actions->get(ObjectAction_NewOU), &QAction::triggered,
        this, &FindResults::create_ou);
    connect(
        object_actions->get(ObjectAction_NewGroup), &QAction::triggered,
        this, &FindResults::create_group);
    connect(
        object_actions->get(ObjectAction_Delete), &QAction::triggered,
        this, &FindResults::delete_objects);
    connect(
        object_actions->get(ObjectAction_Rename), &QAction::triggered,
        this, &FindResults::rename);
    connect(
        object_actions->get(ObjectAction_Move), &QAction::triggered,
        this, &FindResults::move);
    connect(
        object_actions->get(ObjectAction_AddToGroup), &QAction::triggered,
        this, &FindResults::add_to_group);
    connect(
        object_actions->get(ObjectAction_Enable), &QAction::triggered,
        this, &FindResults::enable);
    connect(
        object_actions->get(ObjectAction_Disable), &QAction::triggered,
        this, &FindResults::disable);
    connect(
        object_actions->get(ObjectAction_ResetPassword), &QAction::triggered,
        this, &FindResults::reset_password);

    connect(
        view, &ResultsView::selection_changed,
        this, &FindResults::update_actions_visibility);
}

void FindResults::add_actions_to_action_menu(QMenu *menu) {
    object_actions->add_to_menu(menu);

    menu->addSeparator();

    menu->addAction(properties_action);
}

void FindResults::add_actions_to_view_menu(QMenu *menu) {
    menu->addAction(customize_columns_action);
}

void FindResults::clear() {
    object_count_label->clear();
    model->removeRows(0, model->rowCount());
}

void FindResults::load(const QHash<QString, AdObject> &search_results) {
    for (const AdObject &object : search_results) {
        const QList<QStandardItem *> row = make_item_row(adconfig->get_columns().count());

        load_object_row(row, object);

        model->appendRow(row);
    }

    const QString label_text = tr("%n object(s)", "", model->rowCount());
    object_count_label->setText(label_text);
}

QList<QList<QStandardItem *>> FindResults::get_selected_rows() const {
    const QList<QModelIndex> selected_rows = view->current_view()->selectionModel()->selectedRows();

    QList<QList<QStandardItem *>> out;

    for (const QModelIndex row_index : selected_rows) {
        const int row = row_index.row();

        QList<QStandardItem *> row_copy;

        for (int col = 0; col < model->columnCount(); col++) {
            QStandardItem *item = model->item(row, col);
            QStandardItem *item_copy = item->clone();
            row_copy.append(item_copy);
        }

        out.append(row_copy);
    }

    return out;
}

void FindResults::delete_objects() {
    const QList<QString> targets = get_selected_dns();

    object_delete(targets, this);
}

void FindResults::properties() {
    const QList<QString> targets = get_selected_dns();
    if (targets.size() != 1) {
        return;
    }

    const QString dn = targets[0];

    PropertiesDialog *dialog = PropertiesDialog::open_for_target(dn);
    dialog->open();
}

void FindResults::rename() {
    const QList<QString> targets = get_selected_dns();
    auto dialog = new RenameDialog(targets, this);
    dialog->open();
}

void FindResults::create_helper(const QString &object_class) {
    const QList<QString> targets = get_selected_dns();
    auto dialog = new CreateDialog(targets, object_class, this);
    dialog->open();
}

void FindResults::move() {
    const QList<QString> targets = get_selected_dns();
    auto dialog = new MoveDialog(targets, this);
    dialog->open();
}

void FindResults::add_to_group() {
    const QList<QString> targets = get_selected_dns();
    object_add_to_group(targets, this);
}

void FindResults::enable() {
    enable_disable_helper(false);
}

void FindResults::disable() {
    enable_disable_helper(true);
}

void FindResults::reset_password() {
    const QList<QString> targets = get_selected_dns();
    const auto dialog = new PasswordDialog(targets, this);
    dialog->open();
}

void FindResults::create_user() {
    create_helper(CLASS_USER);
}

void FindResults::create_computer() {
    create_helper(CLASS_COMPUTER);
}

void FindResults::create_ou() {
    create_helper(CLASS_OU);
}

void FindResults::create_group() {
    create_helper(CLASS_GROUP);
}

void FindResults::customize_columns() {
    auto dialog = new CustomizeColumnsDialog(view->detail_view(), object_model_default_columns(), this);
    dialog->open();
}

void FindResults::on_context_menu(const QPoint pos) {
    const QPoint global_pos = view->mapToGlobal(pos);

    emit context_menu(global_pos);
}

void FindResults::enable_disable_helper(const bool disabled) {
    const QList<QString> targets = get_selected_dns();
    const QList<QString> changed_objects = object_enable_disable(targets, disabled, this);

    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();

    for (const QString &dn : changed_objects) {
        const QPersistentModelIndex index = selected[dn];

        model->setData(index, disabled, ObjectRole_AccountDisabled);
    }
    
    update_actions_visibility();
}

// First, hide all actions, then show whichever actions are
// appropriate for current console selection
void FindResults::update_actions_visibility() {
    const QList<QModelIndex> selected_indexes = view->current_view()->selectionModel()->selectedRows();
    object_actions->update_actions_visibility(selected_indexes);

    // Always hide find action because opening a find dialog
    // from another find dialog is weird
    object_actions->get(ObjectAction_Find)->setVisible(false);
}

QHash<QString, QPersistentModelIndex> FindResults::get_selected_dns_and_indexes() {
    QHash<QString, QPersistentModelIndex> out;

    const QList<QModelIndex> indexes = view->current_view()->selectionModel()->selectedRows();
    for (const QModelIndex &index : indexes) {
        const QString dn = index.data(ObjectRole_DN).toString();
        out[dn] = QPersistentModelIndex(index);
    }

    return out;
}

QList<QString> FindResults::get_selected_dns() {
    const QHash<QString, QPersistentModelIndex> selected = get_selected_dns_and_indexes();

    return selected.keys();
}
