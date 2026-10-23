/************************************************************************
 **
 **  @file   exportmarksettings.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's mark-shape table, ported.
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

#include "exportmarksettings.h"

namespace
{
// Hand-written entries, as the print models are: nothing else names a file, so the day a custom
// shape arrives it joins this list and nothing else moves.
//
// The macro is QT_TRANSLATE_NOOP so that lupdate sees the context; the translation itself happens
// at display time, with QCoreApplication::translate on the same context.
const QVector<ExportMarkShapeModel> kShapes{
    {QT_TRANSLATE_NOOP("OExportMarkSettings", "Circle"), "circle", "circle.svg"},
    {QT_TRANSLATE_NOOP("OExportMarkSettings", "Square"), "square", "square.svg"},
    {QT_TRANSLATE_NOOP("OExportMarkSettings", "Octagon"), "octagon", "octogon.svg"},
};
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportMarkShapes() -> const QVector<ExportMarkShapeModel> &
{
    return kShapes;
}
