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

#ifndef CREATE_QUERY_DIALOG_H
#define CREATE_QUERY_DIALOG_H

#include <QDialog>

class ConsoleWidget;
class EditQueryWidget;

class CreateQueryDialog : public QDialog {
Q_OBJECT

public:
    CreateQueryDialog(ConsoleWidget *console_arg);

    void accept() override;

private:
    ConsoleWidget *console;
    EditQueryWidget *edit_query_widget;
};

#endif /* CREATE_QUERY_DIALOG_H */
