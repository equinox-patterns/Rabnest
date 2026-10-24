/************************************************************************
 **
 **  @file   exportprintmodel.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's print-model table, ported row for row.
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

#include "exportprintmodel.h"

namespace
{
// The labels are the strings Ostara shows, under Ostara's own translation context, so that one
// language file serves both applications and a format is never called two different things.
//
// The cover-seat column, read down the table: the block is an A4's own rectangle, so a format
// keeps it in a corner when it has room for a plan BESIDE it, and gives the page a sheet of its
// own when it has not. A3 (297 × 420) and Tabloid (279.4 × 431.8) have that room; A4 does not —
// the block would be the whole sheet — Letter is too short for 297, and A4/Letter framed is
// narrower than 210. A roll has no sheet height at all, so the block simply sits at its corner.
//
// ⚠️ A0 is the one seat nobody dictated: it is not selectable today, so no run can reach it.
// `InSheet` because an A0 sheet does admit the block; to revisit if the row is ever enabled.
//
// ⚠️ The columns are in DECLARATION order — a Seat sits between the two flags, because `hasFrame`
// and `coverSeat` are the format's two properties and belong together. Aggregate initialisation is
// positional: a column moved without the struct moved is a silent reinterpretation.
const QVector<ExportPrintModel> kModels{
    //              label                    token              shape                    widthMm  heightMm frame  coverSeat           columns default selectable
    {QT_TRANSLATE_NOOP("OExportPrintModel", "A0 Roll"),           "A0Roll",          ExportPrintShape::Roll,   841.0,      0.0,  false, CoverSeat::InSheet,    ExportPaper::Family::A0,       false,   1,   true},
    {QT_TRANSLATE_NOOP("OExportPrintModel", "A0"),                "A0",              ExportPrintShape::Tiled,  841.0,   1189.0,  false, CoverSeat::InSheet,    ExportPaper::Family::A0,       false,   1,   false},
    {QT_TRANSLATE_NOOP("OExportPrintModel", "A3"),                "A3",              ExportPrintShape::Tiled,  297.0,    420.0,  false, CoverSeat::InSheet,    ExportPaper::Family::A3Tabloid, true,    3,   true},
    {QT_TRANSLATE_NOOP("OExportPrintModel", "Tabloid"),           "Tabloid",         ExportPrintShape::Tiled,  279.4,    431.8,  false, CoverSeat::InSheet,    ExportPaper::Family::A3Tabloid, true,    3,   true},
    {QT_TRANSLATE_NOOP("OExportPrintModel", "A4"),                "A4",              ExportPrintShape::Tiled,  210.0,    297.0,  false, CoverSeat::ExtraSheet, ExportPaper::Family::A4Letter, true,    5,   true},
    {QT_TRANSLATE_NOOP("OExportPrintModel", "Letter"),            "Letter",          ExportPrintShape::Tiled,  215.9,    279.4,  false, CoverSeat::ExtraSheet, ExportPaper::Family::A4Letter, true,    5,   true},
    // The two framed formats are the only ones whose dimensions are not a standard: the frame is
    // sized so the printable area stays inside the sheet's margins — ten millimetres a side —
    // which is exactly what lets one file come out on A4 OR on Letter, and the other on A3 OR on
    // Tabloid. They are listed after the standards, together: they are variants of them.
    {QT_TRANSLATE_NOOP("OExportPrintModel", "A4/Letter framed"),  "A4LetterFramed",  ExportPrintShape::Tiled,  190.0,    259.0,   true,  CoverSeat::ExtraSheet, ExportPaper::Family::A4Letter, true,    5,   true},
    {QT_TRANSLATE_NOOP("OExportPrintModel", "A3/Tabloid framed"), "A3TabloidFramed", ExportPrintShape::Tiled,  259.0,    400.0,   true,  CoverSeat::InSheet,    ExportPaper::Family::A3Tabloid, true,    3,   true},
};
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportPrintModels() -> const QVector<ExportPrintModel> &
{
    return kModels;
}

//---------------------------------------------------------------------------------------------------------------------
auto FindExportPrintModel(const QString &token) -> const ExportPrintModel *
{
    for (const ExportPrintModel &model : kModels)
    {
        if (token == QLatin1String(model.token))
        {
            return &model;
        }
    }
    return nullptr;
}
