/************************************************************************
 **
 **  @file   exportpaginationsettings.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's pagination-form table, ported.
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

#include "exportpaginationsettings.h"

namespace
{
const QVector<ExportPaginationFormModel> kForms{
    {QT_TRANSLATE_NOOP("OExportPaginationSettings", "Number"), "number"},
    {QT_TRANSLATE_NOOP("OExportPaginationSettings", "Number + letter"), "number+letter"},
};
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportPaginationForms() -> const QVector<ExportPaginationFormModel> &
{
    return kForms;
}
