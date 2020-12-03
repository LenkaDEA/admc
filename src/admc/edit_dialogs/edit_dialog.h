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

#ifndef EDIT_DIALOG_H
#define EDIT_DIALOG_H

#include <QString>
#include <QDialog>
#include <QList>
#include <QByteArray>

class QVBoxLayout;
class QLabel;
class QDialogButtonBox;

/**
 * Gets input from user, which can be obtained through
 * get_new_values(). Different from AttributeEdit because it
 * is opened as a separate dialog and parent object is
 * responsible for actually applying the changes.
 */

class EditDialog : public QDialog {
Q_OBJECT

public:
    // Makes a dialog by picking the appropriate type of
    // edit dialog for given attribute. If attribute is not
    // supported, returns nullptr.
    static EditDialog *make(const QString attribute, const QList<QByteArray> values, QWidget *parent);

    virtual QList<QByteArray> get_new_values() const = 0;

protected:
    using QDialog::QDialog;
    
    static QLabel *make_attribute_label(const QString &attribute);
    QDialogButtonBox *make_button_box(const QString attribute);
};

#endif /* EDIT_DIALOG_H */
