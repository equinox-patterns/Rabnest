/************************************************************************
 **
 **  @file   GroupOverlapDialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   25/10/2026
 **
 **  @brief  « These pieces make up a group: do you want them superimposed? », asked once per run.
 **
 **  @copyright
 **  Copyright (C) 2026 Sophie Denys <contact@sophiedenys.com>
 **
 **  This program is free software: you can redistribute it and/or modify
 **  it under the terms of the GNU General Public License as published by
 **  the Free Software Foundation, either version 3 of the License, or
 **  (at your option) any later version.
 **
 **  This program is distributed in the hope that it will be useful,
 **  but WITHOUT ANY WARRANTY; without even the implied warranty of
 **  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 **  GNU General Public License for more details.
 **
 **  You should have received a copy of the GNU General Public License
 **  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **
 *************************************************************************/

#include "ui/GroupOverlapDialog.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

//---------------------------------------------------------------------------------------------------------------------
GroupOverlapDialog::GroupOverlapDialog(const QVector<CompleteGroup> &groups, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Groups"));

    auto *layout = new QVBoxLayout(this);

    auto *intro = new QLabel(
        tr("These pieces are kept whole, so they are superimposed in the original PDF. "
           "Say for each group whether they should stay that way."),
        this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    m_answers.reserve(groups.size());
    for (const CompleteGroup &group : groups)
    {
        auto *box = new QGroupBox(group.name, this);
        auto *boxLayout = new QVBoxLayout(box);

        auto *question = new QLabel(
            tr("Pieces %1 make up group %2. These pieces are superimposed in the original PDF. "
               "Do you want them to stay superimposed?")
                .arg(group.MembersPhrase(), group.name),
            box);
        question->setWordWrap(true);
        boxLayout->addWidget(question);

        auto *yes = new QCheckBox(tr("Yes"), box);
        auto *no = new QCheckBox(tr("No"), box);
        // **Exclusive, and `Yes` first**: the two answers are one answer, and the one the application
        // already gave is the one that stays checked until a hand says otherwise.
        yes->setChecked(true);

        auto *answers = new QButtonGroup(box);
        answers->setExclusive(true);
        answers->addButton(yes);
        answers->addButton(no);

        boxLayout->addWidget(yes);
        boxLayout->addWidget(no);

        layout->addWidget(box);
        m_answers.append({group.id, box, no});
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    if (QPushButton *ok = buttons->button(QDialogButtonBox::Ok); ok != nullptr)
    {
        ok->setDefault(true);
    }
}

//---------------------------------------------------------------------------------------------------------------------
auto GroupOverlapDialog::Ungrouped() const -> QSet<QString>
{
    QSet<QString> out;
    for (const Answer &answer : m_answers)
    {
        // « No » ticked is the only way a group stops being one; `Yes` leaves the file as it is.
        if (answer.ungroup != nullptr && answer.ungroup->isChecked())
        {
            out.insert(answer.groupId);
        }
    }
    return out;
}
