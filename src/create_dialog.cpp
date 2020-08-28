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

#include "create_dialog.h"
#include "ad_interface.h"
#include "utils.h"
#include "status.h"
#include "edits/attribute_edit.h"
#include "edits/string_edit.h"
#include "edits/group_scope_edit.h"
#include "edits/group_type_edit.h"
#include "edits/account_option_edit.h"
#include "edits/password_edit.h"
#include "utils.h"

#include <QDialog>
#include <QLineEdit>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QList>
#include <QComboBox>
#include <QMessageBox>
#include <QCheckBox>
#include <QDialogButtonBox>

// TODO: implement cannot change pass

QString create_type_to_string(const CreateType &type);

CreateDialog::CreateDialog(const QString &parent_dn_arg, CreateType type_arg, QWidget *parent)
: QDialog(parent)
{
    parent_dn = parent_dn_arg;
    type = type_arg;

    setAttribute(Qt::WA_DeleteOnClose);
    resize(600, 600);

    const QString type_string = create_type_to_string(type);
    const auto title_text = QString(CreateDialog::tr("Create %1 in \"%2\"")).arg(type_string, parent_dn);
    const auto title_label = new QLabel(title_text);
    
    const auto edits_layout = new QGridLayout();

    name_edit = new StringEdit(ATTRIBUTE_NAME);
    all_edits.append(name_edit);

    switch (type) {
        case CreateType_User: {
            make_user_edits();
            break;
        }
        case CreateType_Group: {
            make_group_edits();
            break;
        }
        default: {
            break;
        }
    }

    layout_attribute_edits(all_edits, edits_layout);

    auto button_box = new QDialogButtonBox(QDialogButtonBox::Ok |  QDialogButtonBox::Cancel, this);

    const auto top_layout = new QVBoxLayout();
    setLayout(top_layout);
    top_layout->addWidget(title_label);
    top_layout->addLayout(edits_layout);
    top_layout->addWidget(button_box);

    connect(
        button_box, &QDialogButtonBox::accepted,
        this, &CreateDialog::accept);
    connect(
        button_box, &QDialogButtonBox::rejected,
        this, &QDialog::reject);
}

void CreateDialog::accept() {
    const QString name = name_edit->edit->text();

    auto get_suffix =
    [](CreateType type_arg) {
        switch (type_arg) {
            case CreateType_User: return "CN";
            case CreateType_Computer: return "CN";
            case CreateType_OU: return "OU";
            case CreateType_Group: return "CN";
            case CreateType_COUNT: return "COUNT";
        }
        return "";
    };
    const QString suffix = get_suffix(type);

    const QString dn = suffix + "=" + name + "," + parent_dn;

    const bool verify_success = verify_attribute_edits(all_edits, this);
    if (!verify_success) {
        return;
    }

    auto get_classes =
    [](CreateType type_arg) {
        static const char *classes_user[] = {CLASS_USER, NULL};
        static const char *classes_group[] = {CLASS_GROUP, NULL};
        static const char *classes_ou[] = {CLASS_OU, NULL};
        static const char *classes_computer[] = {CLASS_TOP, CLASS_PERSON, CLASS_ORG_PERSON, CLASS_USER, CLASS_COMPUTER, NULL};

        switch (type_arg) {
            case CreateType_User: return classes_user;
            case CreateType_Computer: return classes_computer;
            case CreateType_OU: return classes_ou;
            case CreateType_Group: return classes_group;
            case CreateType_COUNT: return classes_user;
        }
        return classes_user;
    };
    const char **classes = get_classes(type);

    const int errors_index = Status::instance()->get_errors_size();
    AdInterface::instance()->start_batch();
    {   

        const bool add_success = AdInterface::instance()->object_add(dn, classes);

        bool apply_success = false;
        if (add_success) {
            apply_success = apply_attribute_edits(all_edits, dn, this);
        }

        const QString type_string = create_type_to_string(type);

        if (add_success && apply_success) {
            const QString message = QString(tr("Created %1 - \"%2\"")).arg(type_string, name);

            Status::instance()->message(message, StatusType_Success);

            QDialog::accept();
        } else {
            if (add_success) {
                AdInterface::instance()->object_delete(dn);
            }

            const QString message = QString(tr("Failed to create %1 - \"%2\"")).arg(type_string, name);
            Status::instance()->message(message, StatusType_Error);
        }
    }
    AdInterface::instance()->end_batch();
    Status::instance()->show_errors_popup(errors_index);
}

void CreateDialog::make_group_edits() {
    const auto sama_edit = new StringEdit(ATTRIBUTE_SAMACCOUNT_NAME);
    autofill_sama_name(sama_edit, name_edit);

    const auto group_scope = new GroupScopeEdit();
    const auto group_type = new GroupTypeEdit();

    all_edits = {
        sama_edit,
        group_scope,
        group_type
    };
}

void CreateDialog::make_user_edits() {
    const QList<QString> string_attributes = {
        ATTRIBUTE_FIRST_NAME,
        ATTRIBUTE_LAST_NAME,
        ATTRIBUTE_DISPLAY_NAME,
        ATTRIBUTE_INITIALS,
        ATTRIBUTE_USER_PRINCIPAL_NAME,
        ATTRIBUTE_SAMACCOUNT_NAME,
    };
    QMap<QString, StringEdit *> string_edits;
    make_string_edits(string_attributes, &string_edits);

    auto password_edit = new PasswordEdit();

    const QList<AccountOption> options = {
        AccountOption_PasswordExpired,
        AccountOption_DontExpirePassword,
        AccountOption_Disabled
        // TODO: AccountOption_CannotChangePass
    };
    QMap<AccountOption, AccountOptionEdit *> option_edits = make_account_option_edits(options, this);

    // NOTE: use keys from lists to get correct order
    for (auto attribute : string_attributes) {
        all_edits.append(string_edits[attribute]);
    }
    all_edits.append(password_edit);
    for (auto option : options) {
        all_edits.append(option_edits[option]);
    }

    autofill_sama_name(string_edits[ATTRIBUTE_SAMACCOUNT_NAME], name_edit);

    autofill_full_name(string_edits);
}

QString create_type_to_string(const CreateType &type) {
    switch (type) {
        case CreateType_User: return CreateDialog::tr("User");
        case CreateType_Computer: return CreateDialog::tr("Computer");
        case CreateType_OU: return CreateDialog::tr("Organization Unit");
        case CreateType_Group: return CreateDialog::tr("Group");
        case CreateType_COUNT: return "COUNT";
    }
    return "";
}
