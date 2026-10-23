/************************************************************************
 **
 **  @file   exporttemplate.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The three blocks that draw on the sheets of a tiled plan — Ostara's template, read
 **          from the payload.
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

#ifndef EXPORTTEMPLATE_H
#define EXPORTTEMPLATE_H

#include "exportframesettings.h"
#include "exportmarksettings.h"
#include "exportpaginationsettings.h"

#include <QString>

class QDomElement;

/// @brief What it is called, and everything it draws on the sheets of a tiled plan.
///
/// **Ostara's `OExportTemplate`, ported** — with one difference of circumstance, not of rule:
/// Ostara keeps a template as a file (`~/Ostara/PDFTemplate/*.oamt`) and reads it by name; Rabnest
/// has no such folder, so the template it draws is the one the imported PDF carried, read out of
/// the payload's `<exportRecipe><decor>`. The blocks and their attributes are the same, because it
/// is the same XML (`oexporttemplate.cpp:214-266`).
///
/// **It carries no sheet and no format.** One template decorates any tiled plan — A3, A4, Letter,
/// Tabloid, or either of the two framed formats — so nothing here names one.
struct ExportTemplate
{
    /// What the template is called. In Rabnest it is only ever the creator's name for it, kept so
    /// that the canvas and a log can say which decor is drawn.
    QString name;

    ExportFrameSettings frame;
    ExportMarkSettings marks;
    ExportPaginationSettings pagination;

    /// @brief Reads a template back off an XML element, and says whether it was one.
    ///
    /// Every attribute is read over a default-constructed template, so a payload written by an
    /// older build — or a hand-edited one missing a setting — opens with that setting at its
    /// default rather than at nothing.
    auto LoadFromXml(const QDomElement &root) -> bool;

    /// @brief The decor Rabnest draws when the imported PDF transmits none.
    ///
    /// A rouleau and the A0 sheet carry no `<decor>`: nothing was drawn there, so there is nothing
    /// to reproduce and the application decides. The values are the ones Ostara's own shipped
    /// template draws — a dashed frame at half opacity, the octagon, and a number at the centre of
    /// each sheet and in its corners — so a plan printed from Rabnest looks like a plan printed
    /// from Ostara even when the file said nothing.
    static auto RabnestDefault() -> ExportTemplate;
};

#endif // EXPORTTEMPLATE_H
