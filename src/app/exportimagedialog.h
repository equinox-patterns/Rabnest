/************************************************************************
 **
 **  @file   exportimagedialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The questions an image export asks before it writes anything.
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

#ifndef EXPORTIMAGEDIALOG_H
#define EXPORTIMAGEDIALOG_H

#include "core/export/planimage.h"
#include "core/export/planimagewriter.h"

#include <QDialog>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;

/// Where a plan goes, how big, and in which syntax — the whole of what an image export asks.
///
/// **One output, one dialogue.** Ostara asks the same four things and then a second dialogue for the
/// list of outputs, because a run there can carry several; a Rabnest output is one row of the dock,
/// and the row is what the icon was pressed on. The four questions and their defaults are its own.
class ExportImageDialog : public QDialog
{
    Q_OBJECT

public:
    /// The unit the size is typed in. Each carries its own factor to the millimetre the file needs.
    enum class Unit
    {
        Millimetre,
        Centimetre,
        Inch
    };

    ExportImageDialog(const PlanImage::Plan &plan, const QString &outputName, const QString &directory,
                      QWidget *parent = nullptr);

    auto directory() const -> QString;
    auto format() const -> PlanImageWriter::Format;

    /// The side the size pins, and that size in millimetres.
    auto axis() const -> PlanImage::Axis;
    auto targetMm() const -> double;
    auto dpi() const -> int;

    /// The size the plan takes at the answers given, for the caller to write.
    auto target() const -> PlanImage::Target;

    /// What the file will be called: the output's name, and the suffix its format imposes.
    ///
    /// ⚠️ **The name is the code's, never a file dialogue's.** Ostara imposes the suffix the same
    /// way, and the reason is the same: a `QFileDialog` lets a name keep a `.png` while the file
    /// inside is an SVG, and the file the user has then opened in her viewer is the wrong one.
    auto fileName() const -> QString;

protected:
    /// Refuses a size or a folder that cannot be honoured, in the same sentences Ostara uses.
    void accept() override;

private:
    void UpdateFormat();
    void UpdateSize();

    PlanImage::Plan m_plan;
    double m_sizeMm = 0.0;

    QLineEdit *m_directory = nullptr;
    QComboBox *m_format = nullptr;
    QComboBox *m_axis = nullptr;
    QLineEdit *m_size = nullptr;
    QComboBox *m_unit = nullptr;
    QLabel *m_other = nullptr;
    QLabel *m_dpiLabel = nullptr;
    QSpinBox *m_dpi = nullptr;

    QString m_outputName;
};

#endif  // EXPORTIMAGEDIALOG_H
