/************************************************************************
 **
 **  @file   GroupOverlapDialog.h
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

#ifndef GROUPOVERLAPDIALOG_H
#define GROUPOVERLAPDIALOG_H

#include "core/pattern.h"

#include <QDialog>
#include <QSet>
#include <QVector>

class QCheckBox;
class QGroupBox;

/// One dialogue for the whole run, one group box per group the user kept WHOLE.
///
/// **A group is one block in the source PDF**: its members are drawn on top of each other there, and
/// the group carries one cartouche, one grain line and one miniature for the lot. A print reproduces
/// that, unless the user says otherwise, which is why she is asked.
///
/// **Yes is the default**, and it is the behaviour the application already had: nothing changes for a
/// run that does not touch the dialogue. **No** ungroups the group: every member becomes an ordinary
/// piece, with its own drawing and its own footprint, laid apart from its fellows.
///
/// The answer is read as the set of groups to UNGROUP: the groups are the rare case, and a set that
/// is empty (the usual run) is the cheapest thing to pass down to the reader.
class GroupOverlapDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param groups every group the run keeps whole, which are the ones worth asking about
    explicit GroupOverlapDialog(const QVector<CompleteGroup> &groups, QWidget *parent = nullptr);

    /// The groups the user chose NOT to keep superimposed.
    auto Ungrouped() const -> QSet<QString>;

private:
    struct Answer
    {
        QString groupId;
        QGroupBox *box = nullptr;
        QCheckBox *ungroup = nullptr;  ///< « No » : the one answer this dialogue exists to read
    };

    QVector<Answer> m_answers;
};

#endif // GROUPOVERLAPDIALOG_H
