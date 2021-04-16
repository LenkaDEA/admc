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

#include "console_types/object.h"

#include "adldap.h"
#include "globals.h"
#include "settings.h"
#include "utils.h"
#include "status.h"
#include "central_widget.h"
#include "filter_dialog.h"
#include "filter_widget/filter_widget.h"
#include "object_actions.h"

#include <QStandardItemModel>
#include <QSet>
#include <QMenu>

int object_results_id;

enum DropType {
    DropType_Move,
    DropType_AddToGroup,
    DropType_None
};

void object_item_data_load(QStandardItem *item, const AdObject &object);
DropType object_get_drop_type(const QModelIndex &dropped, const QModelIndex &target);
void disable_drag_if_object_cant_be_moved(const QList<QStandardItem *> &items, const AdObject &object);
bool object_scope_and_results_add_check(ConsoleWidget *console, const QModelIndex &parent);

void object_results_load(const QList<QStandardItem *> row, const AdObject &object) {
    // Load attribute columns
    for (int i = 0; i < g_adconfig->get_columns().count(); i++) {
        const QString attribute = g_adconfig->get_columns()[i];

        if (!object.contains(attribute)) {
            continue;
        }

        const QString display_value =
        [attribute, object]() {
            if (attribute == ATTRIBUTE_OBJECT_CLASS) {
                const QString object_class = object.get_string(attribute);

                if (object_class == CLASS_GROUP) {
                    const GroupScope scope = object.get_group_scope(); 
                    const QString scope_string = group_scope_string(scope);

                    const GroupType type = object.get_group_type(); 
                    const QString type_string = group_type_string_adjective(type);

                    return QString("%1 - %2").arg(type_string, scope_string);
                } else {
                    return g_adconfig->get_class_display_name(object_class);
                }
            } else {
                const QByteArray value = object.get_value(attribute);
                return attribute_display_value(attribute, value, g_adconfig);
            }
        }();

        row[i]->setText(display_value);
    }

    object_item_data_load(row[0], object);

    disable_drag_if_object_cant_be_moved(row, object);
}

void object_item_data_load(QStandardItem *item, const AdObject &object) {
    item->setData(true, ConsoleRole_HasProperties);

    item->setData(ItemType_Object, ConsoleRole_Type);

    const QIcon icon = get_object_icon(object);
    item->setIcon(icon);
    
    item->setData(object.get_dn(), ObjectRole_DN);

    const QList<QString> object_classes = object.get_strings(ATTRIBUTE_OBJECT_CLASS);
    item->setData(QVariant(object_classes), ObjectRole_ObjectClasses);

    const bool cannot_move = object.get_system_flag(SystemFlagsBit_CannotMove);
    item->setData(cannot_move, ObjectRole_CannotMove);

    const bool cannot_rename = object.get_system_flag(SystemFlagsBit_CannotRename);
    item->setData(cannot_rename, ObjectRole_CannotRename);

    const bool cannot_delete = object.get_system_flag(SystemFlagsBit_CannotDelete);
    item->setData(cannot_delete, ObjectRole_CannotDelete);

    const bool account_disabled = object.get_account_option(AccountOption_Disabled);
    item->setData(account_disabled, ObjectRole_AccountDisabled);
}

QList<QString> object_header_labels() {
    QList<QString> out;

    for (const QString &attribute : g_adconfig->get_columns()) {
        const QString attribute_display_name = g_adconfig->get_column_display_name(attribute);

        out.append(attribute_display_name);
    }
    
    return out;
}

QList<int> object_default_columns() {
    // By default show first 3 columns: name, class and
    // description
    return {0, 1, 2};
}

QList<QString> object_search_attributes() {
    QList<QString> attributes;

    attributes += g_adconfig->get_columns();

    // NOTE: needed for loading group type/scope into "type"
    // column
    attributes += ATTRIBUTE_GROUP_TYPE;

    // NOTE: system flags are needed to disable
    // delete/move/rename for objects that can't do those
    // actions
    attributes += ATTRIBUTE_SYSTEM_FLAGS;

    return attributes;
}

void object_scope_load(QStandardItem *item, const AdObject &object) {
    const QString name =
    [&]() {
        const QString dn = object.get_dn();
        return dn_get_name(dn);
    }();

    item->setText(name);

    object_item_data_load(item, object);
    disable_drag_if_object_cant_be_moved({item}, object);
}

void disable_drag_if_object_cant_be_moved(const QList<QStandardItem *> &items, const AdObject &object) {
    const bool cannot_move = object.get_system_flag(SystemFlagsBit_CannotMove);

    for (auto item : items) {
        item->setDragEnabled(!cannot_move);
    }
}

// NOTE: have to search instead of just using deleted
// index because can delete objects from query tree
void object_delete(ConsoleWidget *console, const QList<QString> &dn_list, const bool ignore_query_tree) {
    for (const QString &dn : dn_list) {
        // Delete in scope
        const QList<QPersistentModelIndex> scope_indexes = get_persistent_indexes(console->search_scope_by_role(ObjectRole_DN, dn, ItemType_Object));
        for (const QPersistentModelIndex &index : scope_indexes) {
            console->delete_item(index);
        }

        // Delete in results
        const QList<QPersistentModelIndex> results_indexes = get_persistent_indexes(console->search_results_by_role(ObjectRole_DN, dn, ItemType_Object));
        for (const QPersistentModelIndex &index : results_indexes) {
            // NOTE: don't touch query tree indexes, they
            // stay around and just go out of date
            const bool index_is_in_query_tree =
            [=]() {
                const QModelIndex scope_parent = console->get_scope_parent(index);
                const ItemType scope_parent_type = (ItemType) scope_parent.data(ConsoleRole_Type).toInt();

                return (scope_parent_type == ItemType_QueryItem);
            }();

            if (index_is_in_query_tree && ignore_query_tree) {
                continue;
            }

            console->delete_item(index);
        }
    }
}

void object_create(ConsoleWidget *console, AdInterface &ad, const QList<QString> &dn_list, const QModelIndex &parent) {
    if (!object_scope_and_results_add_check(console, parent)) {
        return;
    }

    const QList<AdObject> object_list =
    [&]() {
        QList<AdObject> out;

        for (const QString &dn : dn_list) {
            const AdObject object = ad.search_object(dn);
            out.append(object);
        }

        return out;
    }();

    object_create(console, object_list, parent);
}

void object_move(ConsoleWidget *console, AdInterface &ad, const QList<QString> &old_dn_list, const QList<QString> &new_dn_list, const QString &new_parent_dn) {
    // NOTE: delete old item AFTER adding new item because:
    // If old item is deleted first, then it's possible for
    // new parent to get selected (if they are next to each
    // other in scope tree). Then what happens is that due
    // to new parent being selected, it gets fetched and
    // loads new object. End result is that new object is
    // duplicated.
    const QModelIndex new_parent_index =
    [=]() {
        const QList<QModelIndex> search_results = console->search_scope_by_role(ObjectRole_DN, new_parent_dn, ItemType_Object);

        if (search_results.size() == 1) {
            return search_results[0];
        } else {
            return QModelIndex();
        }
    }();

    object_create(console, ad, new_dn_list, new_parent_index);
    object_delete(console, old_dn_list, true);

    console->sort_scope();
}

void object_move(ConsoleWidget *console, AdInterface &ad, const QList<QString> &old_dn_list, const QString &new_parent_dn) {
    const QList<QString> new_dn_list =
    [&]() {
        QList<QString> out;

        for (const QString &old_dn : old_dn_list) {
            const QString new_dn = dn_move(old_dn, new_parent_dn);
            out.append(new_dn);
        }

        return out;
    }();

    object_move(console, ad, old_dn_list, new_dn_list, new_parent_dn);
}

// Check parent index before adding objects to console
bool object_scope_and_results_add_check(ConsoleWidget *console, const QModelIndex &parent) {
    if (!parent.isValid()) {
        return false;
    }

    // NOTE: don't add if parent wasn't fetched yet. If that
    // is the case then the object will be added naturally
    // when parent is fetched.
    const bool parent_was_fetched = console->item_was_fetched(parent);
    if (!parent_was_fetched) {
        return false;
    }

    return true;
}

void object_create(ConsoleWidget *console, const QList<AdObject> &object_list, const QModelIndex &parent) {
    if (!object_scope_and_results_add_check(console, parent)) {
        return;
    }

    for (const AdObject &object : object_list) {
        const bool should_be_in_scope =
        [&]() {
            // NOTE: "containers" referenced here don't mean
            // objects with "container" object class.
            // Instead it means all the objects that can
            // have children(some of which are not
            // "container" class).
            const bool is_container =
            [=]() {
                const QList<QString> filter_containers = g_adconfig->get_filter_containers();
                const QString object_class = object.get_string(ATTRIBUTE_OBJECT_CLASS);

                return filter_containers.contains(object_class);
            }();

            const bool show_non_containers_ON = g_settings->get_bool(BoolSetting_ShowNonContainersInConsoleTree);

            return (is_container || show_non_containers_ON);
        }();

        if (should_be_in_scope) {
            QStandardItem *scope_item;
            QList<QStandardItem *> results_row;
            console->add_buddy_scope_and_results(object_results_id, ScopeNodeType_Dynamic, parent, &scope_item, &results_row);

            object_scope_load(scope_item, object);
            object_results_load(results_row, object);
        } else {
            const QList<QStandardItem *> results_row = console->add_results_row(parent);
            object_results_load(results_row, object);
        }
    }
}

// Load children of this item in scope tree
// and load results linked to this scope item
void object_fetch(ConsoleWidget *console, FilterDialog *filter_dialog, const QModelIndex &index) {
    // TODO: handle connect/search failure
    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    show_busy_indicator();

    const bool dev_mode = g_settings->get_bool(BoolSetting_DevMode);

    //
    // Search object's children
    //
    const QString filter =
    [=]() {
        const QString user_filter = filter_dialog->filter_widget->get_filter();

        const QString is_container = is_container_filter();

        // NOTE: OR user filter with containers filter so that container objects are always shown, even if they are filtered out by user filter
        QString out = filter_OR({user_filter, is_container});

        // Hide advanced view only" objects if advanced view
        // setting is off
        const bool advanced_features_OFF = !g_settings->get_bool(BoolSetting_AdvancedFeatures);
        if (advanced_features_OFF) {
            const QString advanced_features = filter_CONDITION(Condition_NotEquals, ATTRIBUTE_SHOW_IN_ADVANCED_VIEW_ONLY, "true");
            out = filter_OR({out, advanced_features});
        }

        // OR filter with some dev mode object classes, so that they show up no matter what when dev mode is on
        if (dev_mode) {
            const QList<QString> schema_classes = {
                "classSchema",
                "attributeSchema",
                "displaySpecifier",
            };

            QList<QString> class_filters;
            for (const QString &object_class : schema_classes) {
                const QString class_filter = filter_CONDITION(Condition_Equals, ATTRIBUTE_OBJECT_CLASS, object_class);
                class_filters.append(class_filter);
            }

            out = filter_OR({out, filter_OR(class_filters)});
        }

        return out;
    }();

    const QList<QString> search_attributes = object_search_attributes();

    const QString dn = index.data(ObjectRole_DN).toString();

    QHash<QString, AdObject> search_results = ad.search(filter, search_attributes, SearchScope_Children, dn);

    // Dev mode
    // NOTE: configuration and schema objects are hidden so that they don't show up in regular searches. Have to use search_object() and manually add them to search results.
    if (dev_mode) {
        const QString search_base = g_adconfig->domain_head();
        const QString configuration_dn = g_adconfig->configuration_dn();
        const QString schema_dn = g_adconfig->schema_dn();

        if (dn == search_base) {
            search_results[configuration_dn] = ad.search_object(configuration_dn);
        } else if (dn == configuration_dn) {
            search_results[schema_dn] = ad.search_object(schema_dn);
        }
    }

    object_create(console, search_results.values(), index);
    console->sort_scope();

    hide_busy_indicator();
}

QModelIndex object_tree_init(ConsoleWidget *console, AdInterface &ad) {
    // Create tree head
    const QString head_dn = g_adconfig->domain_head();
    const AdObject head_object = ad.search_object(head_dn);

    QStandardItem *head_item = console->add_scope_item(object_results_id, ScopeNodeType_Dynamic, QModelIndex());

    object_scope_load(head_item, head_object);

    const QString domain_text =
    [&]() {
        const QString name = head_item->text();
        const QString host = ad.host();

        return QString("%1 [%2]").arg(name, host);
    }();
    head_item->setText(domain_text);

    return head_item->index();
}

void object_can_drop(const QList<QModelIndex> &dropped_list, const QModelIndex &target, const QSet<ItemType> &dropped_types, bool *ok) {
    const bool dropped_are_all_objects = (dropped_types.size() == 1 && dropped_types.contains(ItemType_Object));
    if (!dropped_are_all_objects) {
        return;
    }

    // NOTE: always allow dropping when dragging multiple
    // objects. This way, whatever objects can drop will be
    // dropped and if others fail to drop it's not a big
    // deal.
    if (dropped_list.size() != 1) {
        *ok = true;
    } else {
        const QModelIndex dropped = dropped_list[0];

        const DropType drop_type = object_get_drop_type(dropped, target);
        const bool can_drop = (drop_type != DropType_None);

        *ok = can_drop;
    }
}

void object_drop(ConsoleWidget *console, const QList<QModelIndex> &dropped_list, const QModelIndex &target) {
    const QString target_dn = target.data(ObjectRole_DN).toString();

    AdInterface ad;
    if (ad_failed(ad)) {
        return;
    }

    show_busy_indicator();

    for (const QModelIndex &dropped : dropped_list) {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const DropType drop_type = object_get_drop_type(dropped, target);

        switch (drop_type) {
            case DropType_Move: {
                const bool move_success = ad.object_move(dropped_dn, 
                    target_dn);

                if (move_success) {
                    object_move(console, ad, QList<QString>({dropped_dn}), target_dn);
                }

                break;
            }
            case DropType_AddToGroup: {
                ad.group_add_member(target_dn, dropped_dn);

                break;
            }
            case DropType_None: {
                break;
            }
        }
    }

    console->sort_scope();

    hide_busy_indicator();

    g_status()->display_ad_messages(ad, console);
}

// Determine what kind of drop type is dropping this object
// onto target. If drop type is none, then can't drop this
// object on this target.
DropType object_get_drop_type(const QModelIndex &dropped, const QModelIndex &target) {
    const bool dropped_is_target =
    [&]() {
        const QString dropped_dn = dropped.data(ObjectRole_DN).toString();
        const QString target_dn = target.data(ObjectRole_DN).toString();

        return (dropped_dn == target_dn);
    }();

    const QList<QString> dropped_classes = dropped.data(ObjectRole_ObjectClasses).toStringList();
    const QList<QString> target_classes = target.data(ObjectRole_ObjectClasses).toStringList();

    const bool dropped_is_user = dropped_classes.contains(CLASS_USER);
    const bool dropped_is_group = dropped_classes.contains(CLASS_GROUP);
    const bool target_is_group = target_classes.contains(CLASS_GROUP);

    if (dropped_is_target) {
        return DropType_None;
    } else if (dropped_is_user && target_is_group) {
        return DropType_AddToGroup;
    } else if (dropped_is_group && target_is_group) {
        return DropType_AddToGroup;
    } else {
        const QList<QString> dropped_superiors = g_adconfig->get_possible_superiors(dropped_classes);

        const bool target_is_valid_superior =
        [&]() {
            for (const auto &object_class : dropped_superiors) {
                if (target_classes.contains(object_class)) {
                    return true;
                }
            }

            return false;
        }();

        if (target_is_valid_superior) {
            return DropType_Move;
        } else {
            return DropType_None;
        }
    }
}

void object_add_actions_to_menu(ObjectActions *actions, QMenu *menu) {
    // Container
    menu->addAction(actions->get(ObjectAction_Find));

    menu->addSeparator();

    // User
    menu->addAction(actions->get(ObjectAction_AddToGroup));
    menu->addAction(actions->get(ObjectAction_Enable));
    menu->addAction(actions->get(ObjectAction_Disable));
    menu->addAction(actions->get(ObjectAction_ResetPassword));

    // Other
    menu->addAction(actions->get(ObjectAction_EditUpnSuffixes));

    menu->addSeparator();

    // General object
    menu->addAction(actions->get(ObjectAction_Delete));
    menu->addAction(actions->get(ObjectAction_Rename));
    menu->addAction(actions->get(ObjectAction_Move));
}

void object_show_hide_actions(ObjectActions *actions, const QList<QModelIndex> &indexes) {
    if (!indexes_are_of_type(indexes, QSet<int>({ItemType_Object}))) {
        return;
    }

    // Get info about selected objects from view
    const QList<QString> targets =
    [=]() {
        QList<QString> out;

        for (const QModelIndex index : indexes) {
            const QString dn = index.data(ObjectRole_DN).toString();

            out.append(dn);
        }

        return out;  
    }();

    const QSet<QString> target_classes =
    [=]() {
        QSet<QString> out;
        
        for (const QModelIndex index : indexes) {
            const QList<QString> object_classes = index.data(ObjectRole_ObjectClasses).toStringList();
            const QString main_object_class = object_classes.last();

            out.insert(main_object_class);
        }

        return out;
    }();

    const bool single_object = (targets.size() == 1);

    if (single_object) {
        const QModelIndex index = indexes[0];
        const QString target = targets[0];
        const QString target_class = target_classes.values()[0];

        // Get info about object that will determine which
        // actions are present/enabled
        const bool is_container =
        [=]() {
            const QList<QString> container_classes = g_adconfig->get_filter_containers();

            return container_classes.contains(target_class);
        }();

        const bool is_user = (target_class == CLASS_USER);
        const bool is_domain = (target_class == CLASS_DOMAIN);

        const bool cannot_move = index.data(ObjectRole_CannotMove).toBool();
        const bool cannot_rename = index.data(ObjectRole_CannotRename).toBool();
        const bool cannot_delete = index.data(ObjectRole_CannotDelete).toBool();
        const bool account_disabled = index.data(ObjectRole_AccountDisabled).toBool();

        if (is_container) {
            actions->show(ObjectAction_NewUser);
            actions->show(ObjectAction_NewComputer);
            actions->show(ObjectAction_NewOU);
            actions->show(ObjectAction_NewGroup);

            actions->show(ObjectAction_Find);
        }

        if (is_user) {
            actions->show(ObjectAction_AddToGroup);
            actions->show(ObjectAction_ResetPassword);

            if (account_disabled) {
                actions->show(ObjectAction_Enable);
            } else {
                actions->show(ObjectAction_Disable);
            }
        }

        if (is_domain) {
            actions->get(ObjectAction_EditUpnSuffixes)->setVisible(true);
        }

        actions->show(ObjectAction_Move);
        actions->show(ObjectAction_Delete);
        actions->show(ObjectAction_Rename);

        actions->get(ObjectAction_Move)->setDisabled(cannot_move);
        actions->get(ObjectAction_Delete)->setDisabled(cannot_delete);
        actions->get(ObjectAction_Rename)->setDisabled(cannot_rename);
    } else if (targets.size() > 1) {
        const bool all_users = (target_classes.contains(CLASS_USER) && target_classes.size() == 1);

        if (all_users) {
            actions->show(ObjectAction_AddToGroup);

            // NOTE: show both enable/disable for multiple
            // users because some users might be disabled,
            // some enabled and we want to provide a way to
            // disable all or enable all
            actions->show(ObjectAction_Enable);
            actions->show(ObjectAction_Disable);
        }

        actions->show(ObjectAction_Move);
        actions->show(ObjectAction_Delete);
    }
}
