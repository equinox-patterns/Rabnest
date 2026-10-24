/************************************************************************
 **
 **  @file   exportpaper.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   24/10/2026
 **
 **  @brief  Which sheets of paper a print run can be printed on.
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

#include "core/print/exportpaper.h"

#include <QCoreApplication>

#include <algorithm>

namespace
{
/// The papers a home or a studio printer has, in millimetres and portrait.
///
/// The five this application can lay a plan out for, and no more: a list of everything a driver
/// knows would offer papers no format of ours produces, and choosing one of those is exactly the
/// mistake this exists to prevent.
struct Candidate
{
    QPageSize::PageSizeId id;
    double widthMm;
    double heightMm;
};

using PageSizeId = QPageSize::PageSizeId;

const Candidate kA4{PageSizeId::A4, 210.0, 297.0};
const Candidate kLetter{PageSizeId::Letter, 215.9, 279.4};
const Candidate kA3{PageSizeId::A3, 297.0, 420.0};
const Candidate kTabloid{PageSizeId::Tabloid, 279.4, 431.8};
const Candidate kA0{PageSizeId::A0, 841.0, 1189.0};

/// The members of one pair, smallest first — the order a choice is offered in.
auto Members(ExportPaper::Family family) -> QVector<Candidate>
{
    switch (family)
    {
        case ExportPaper::Family::A0:
            return {kA0};
        case ExportPaper::Family::A3Tabloid:
            return {kA3, kTabloid};
        default:
            return {kA4, kLetter};
    }
}

/// A third of a millimetre: enough that a sheet stated at 297,0 mm is held by a paper stated at
/// 297,0 mm, and far too little to let a sheet be printed on a paper it overhangs.
constexpr double kToleranceMm = 0.3;
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto ExportPaper::Fitting(Family family, const QSizeF &sheetMm) -> QVector<Paper>
{
    QVector<Paper> papers;

    if (sheetMm.width() <= 0.0 || sheetMm.height() <= 0.0)
    {
        return papers;
    }

    for (const Candidate &candidate : Members(family))
    {
        // **Whole, not merely big enough.** A sheet printed on a paper that is narrower is a sheet
        // with a column of its drawing missing — and a plan whose edge is missing is not a plan
        // that can be taped together.
        if (sheetMm.width() > candidate.widthMm + kToleranceMm
            || sheetMm.height() > candidate.heightMm + kToleranceMm)
        {
            continue;
        }

        papers.append(Paper{candidate.id, QSizeF(candidate.widthMm, candidate.heightMm)});
    }

    // The smallest that holds it first — Letter before A4, Tabloid before A3 — which is the order
    // the pairs are named in, and the order of the choice a framed run offers.
    std::sort(papers.begin(), papers.end(), [](const Paper &one, const Paper &other) {
        const double oneArea = one.sizeMm.width() * one.sizeMm.height();
        const double otherArea = other.sizeMm.width() * other.sizeMm.height();
        return oneArea < otherArea;
    });

    return papers;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportPaper::Name(QPageSize::PageSizeId id) -> QString
{
    // **The five names she uses, and not the driver's.** Qt calls the American sheets « Letter /
    // ANSI A » and « Tabloid / ANSI B » — true, and three words where one says it. A paper is named
    // the way the format table names the format, so a run and its paper read alike.
    switch (id)
    {
        case QPageSize::A0:
            return QCoreApplication::translate("ExportPaper", "A0");
        case QPageSize::A3:
            return QCoreApplication::translate("ExportPaper", "A3");
        case QPageSize::Tabloid:
            return QCoreApplication::translate("ExportPaper", "Tabloid");
        case QPageSize::A4:
            return QCoreApplication::translate("ExportPaper", "A4");
        case QPageSize::Letter:
            return QCoreApplication::translate("ExportPaper", "Letter");
        default:
            return QPageSize(id).name();
    }
}
