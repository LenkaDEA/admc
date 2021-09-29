/*
 * ADMC - AD Management Center
 *
 * Copyright (C) 2020-2021 BaseALT Ltd.
 * Copyright (C) 2020-2021 Dmitry Degtyarev
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

#include "find_object_dialog.h"
#include "ui_find_object_dialog.h"

#include "ad_config.h"
#include "find_widget.h"
#include "globals.h"
#include "settings.h"

#include <QMenuBar>

FindObjectDialog::FindObjectDialog(const QList<QString> classes, const QString default_base, QWidget *parent)
: QDialog(parent) {
    ui = new Ui::FindObjectDialog();
    ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);

    auto menubar = new QMenuBar();
    layout()->setMenuBar(menubar);
    auto action_menu = menubar->addMenu(tr("&Action"));
    auto view_menu = menubar->addMenu(tr("&View"));

    ui->find_widget->init(classes, default_base);
    ui->find_widget->setup_action_menu(action_menu);
    ui->find_widget->setup_view_menu(view_menu);

    settings_setup_dialog_geometry(SETTING_find_object_dialog_geometry, this);

    const QVariant state = settings_get_variant(SETTING_find_object_state);
    ui->find_widget->restore_state(state);
}

FindObjectDialog::~FindObjectDialog() {
    const QVariant state = ui->find_widget->save_state();
    settings_set_variant(SETTING_find_object_state, state);
}

void FindObjectDialog::set_buddy_console(ConsoleWidget *buddy_console) {
    ui->find_widget->set_buddy_console(buddy_console);
}
