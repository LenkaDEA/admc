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

#ifndef CREATE_POLICY_DIALOG_H
#define CREATE_POLICY_DIALOG_H

/**
 * Creates a GPO.
 */

#include <QDialog>

class QLineEdit;

namespace Ui {
    class CreatePolicyDialog;
}

class CreatePolicyDialog : public QDialog {
    Q_OBJECT

public:
    CreatePolicyDialog(QWidget *parent);

signals:
    void created_policy(const QString &dn);

public slots:
    void open() override;
    void accept() override;

private:
    Ui::CreatePolicyDialog *ui;
};

#endif /* CREATE_POLICY_DIALOG_H */
