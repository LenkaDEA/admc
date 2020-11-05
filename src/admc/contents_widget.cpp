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

#include "contents_widget.h"
#include "object_model.h"
#include "containers_widget.h"
#include "object_context_menu.h"
#include "details_dialog.h"
#include "settings.h"
#include "utils.h"
#include "ad_interface.h"
#include "ad_config.h"
#include "settings.h"
#include "filter.h"
#include "attribute_display.h"

#include <QTreeView>
#include <QLabel>
#include <QHeaderView>
#include <QDebug>
#include <QSortFilterProxyModel>
#include <QLineEdit>
#include <QGridLayout>

ContentsWidget::ContentsWidget(ContainersWidget *containers_widget, QWidget *parent)
: QWidget(parent)
{   
    // NOTE: dn is not one of ADUC's columns, but adding it here for convenience
    const QList<QString> base_columns = {
        ATTRIBUTE_NAME,
        ATTRIBUTE_OBJECT_CLASS,
        ATTRIBUTE_DESCRIPTION,
        ATTRIBUTE_DISTINGUISHED_NAME
    };
    const QList<QString> extra_columns = ADCONFIG()->get_extra_columns();
    columns = base_columns + extra_columns;

    model = new ObjectModel(columns.count(), column_index(ATTRIBUTE_DISTINGUISHED_NAME), parent);

    const QList<QString> header_labels =
    [this]() {
        QList<QString> out;
        for (const QString attribute : columns) {
            const QString attribute_name = ADCONFIG()->get_attribute_display_name(attribute, CLASS_DEFAULT);

            out.append(attribute_name);
        }
        return out;
    }();
    model->setHorizontalHeaderLabels(header_labels);

    auto proxy_name = new QSortFilterProxyModel(this);
    proxy_name->setFilterKeyColumn(column_index(ATTRIBUTE_NAME));

    view = new QTreeView(this);
    view->setAcceptDrops(true);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setRootIsDecorated(false);
    view->setItemsExpandable(false);
    view->setExpandsOnDoubleClick(false);
    view->setContextMenuPolicy(Qt::CustomContextMenu);
    view->setDragDropMode(QAbstractItemView::DragDrop);
    view->setAllColumnsShowFocus(true);
    view->setSortingEnabled(true);
    view->header()->setSectionsMovable(true);

    proxy_name->setSourceModel(model);
    view->setModel(proxy_name);

    DetailsDialog::connect_to_open_by_double_click(view, column_index(ATTRIBUTE_DISTINGUISHED_NAME));

    setup_column_toggle_menu(view, model, 
    {
        column_index(ATTRIBUTE_NAME),
        column_index(ATTRIBUTE_OBJECT_CLASS),
        column_index(ATTRIBUTE_DESCRIPTION)
    });

    label = new QLabel(this);

    const auto filter_name_label = new QLabel(tr("Filter: "), this);
    auto filter_name_edit = new QLineEdit(this);

    const auto layout = new QGridLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(label, 0, 0);
    layout->setColumnStretch(1, 1);
    layout->addWidget(filter_name_label, 0, 2, Qt::AlignRight);
    layout->addWidget(filter_name_edit, 0, 3);
    layout->addWidget(view, 1, 0, 2, 4);

    connect(
        containers_widget, &ContainersWidget::selected_changed,
        this, &ContentsWidget::on_containers_selected_changed);
    connect(
        AD(), &AdInterface::modified,
        this, &ContentsWidget::on_ad_modified);

    const BoolSettingSignal *advanced_view_setting = SETTINGS()->get_bool_signal(BoolSetting_AdvancedView);
    connect(
        advanced_view_setting, &BoolSettingSignal::changed,
        [this]() {
            change_target(target_dn);
        });

    connect(
        filter_name_edit, &QLineEdit::textChanged,
        [proxy_name](const QString &text) {
            proxy_name->setFilterRegExp(QRegExp(text, Qt::CaseInsensitive, QRegExp::FixedString));
        });
    filter_name_edit->setText("");

    QObject::connect(
        view, &QWidget::customContextMenuRequested,
        this, &ContentsWidget::on_context_menu);
}

void ContentsWidget::on_containers_selected_changed(const QString &dn) {
    change_target(dn);
}

void ContentsWidget::on_ad_modified() {
    change_target(target_dn);
}

void ContentsWidget::on_context_menu(const QPoint pos) {
    const QString dn =
    [this, pos]() {
        const int dn_column = column_index(ATTRIBUTE_DISTINGUISHED_NAME);
        QString out = get_dn_from_pos(pos, view, dn_column);
        
        // Interpret clicks on empty space as clicks on parent
        if (out.isEmpty() && !target_dn.isEmpty()) {
            out = target_dn;
        }

        return out;
    }();
    if (dn.isEmpty()) {
        return;
    }    

    ObjectContextMenu context_menu(dn);
    exec_menu_from_view(&context_menu, view, pos);
}

void ContentsWidget::change_target(const QString &dn) {
    target_dn = dn;

    // Load model
    model->removeRows(0, model->rowCount());

    const QList<QString> search_attributes = columns;
    const QString filter = current_advanced_view_filter();
    const QHash<QString, AdObject> search_results = AD()->search(filter, search_attributes, SearchScope_Children, target_dn);

    for (auto child_dn : search_results.keys()) {
        const AdObject object  = search_results[child_dn];
        
        const QList<QStandardItem *> row = make_item_row(columns.count());
        for (int i = 0; i < columns.count(); i++) {
            const QString attribute = columns[i];

            if (!object.contains(attribute)) {
                continue;
            }

            const QString display_value =
            [attribute, object]() {
                if (attribute == ATTRIBUTE_OBJECT_CLASS) {
                    const QString value_string = object.get_string(attribute);
                    return ADCONFIG()->get_class_display_name(value_string);
                } else {
                    const QByteArray value = object.get_value(attribute);
                    return attribute_display_value(attribute, value);
                }
            }();

            row[i]->setText(display_value);
        }

        const QIcon icon = object.get_icon();
        row[0]->setIcon(icon);

        model->appendRow(row);
    }

    view->sortByColumn(column_index(ATTRIBUTE_NAME), Qt::AscendingOrder);

    resize_columns();

    const QString target_name = dn_get_rdn(target_dn);

    QString label_text;
    if (target_name.isEmpty()) {
        label_text = "";
    } else {
        const QAbstractItemModel *view_model = view->model();
        const QModelIndex view_head = view_model->index(0, 0);
        const int object_count = view_model->rowCount(view_head);

        const QString objects_string = tr("%n object(s)", "", object_count);
        label_text = QString("%1: %2").arg(target_name, objects_string);
    }
    label->setText(label_text);
}

void ContentsWidget::resize_columns() {
    const int view_width = view->width();
    const int name_width = (int) (view_width * 0.4);
    const int category_width = (int) (view_width * 0.15);

    view->setColumnWidth(column_index(ATTRIBUTE_NAME), name_width);
    view->setColumnWidth(column_index(ATTRIBUTE_OBJECT_CLASS), category_width);
}

void ContentsWidget::showEvent(QShowEvent *event) {
    resize_columns();
}

int ContentsWidget::column_index(const QString &attribute) {
    if (!columns.contains(attribute)) {
        printf("ContentsWidget is missing column for %s\n", qPrintable(attribute));
    }

    return columns.indexOf(attribute);
}
