/************************************************************************
 **
 **  @file   exportsheets.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  Ostara's sheet-presence rule, ported.
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

#include "exportsheets.h"

#include <algorithm>

//---------------------------------------------------------------------------------------------------------------------
auto ExportSheets::Set::IsPresent(int row, int col) const -> bool
{
    return RankOf(row, col) > 0;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportSheets::Set::RankOf(int row, int col) const -> int
{
    if (row < 0 || row >= rows || col < 0 || col >= columns)
    {
        return 0;
    }

    return ranks.at(row * columns + col);
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportSheets::Set::VerticalEdges(int boundary) const -> QVector<Run>
{
    QVector<Run> runs;
    int start = -1;

    for (int row = 0; row < rows; ++row)
    {
        // The boundary is the edge of a printed sheet wherever either of the two cells it
        // separates is printed. On the plan's own two sides one of them does not exist, and
        // IsPresent says so — which is what keeps a boundary from being drawn past the paper.
        const bool edge = IsPresent(row, boundary - 1) || IsPresent(row, boundary);

        if (edge && start < 0)
        {
            start = row;
        }
        else if (!edge && start >= 0)
        {
            runs.append({start, row - 1});
            start = -1;
        }
    }

    if (start >= 0)
    {
        runs.append({start, rows - 1});
    }

    return runs;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportSheets::Set::HorizontalEdges(int boundary) const -> QVector<Run>
{
    QVector<Run> runs;
    int start = -1;

    for (int col = 0; col < columns; ++col)
    {
        const bool edge = IsPresent(boundary - 1, col) || IsPresent(boundary, col);

        if (edge && start < 0)
        {
            start = col;
        }
        else if (!edge && start >= 0)
        {
            runs.append({start, col - 1});
            start = -1;
        }
    }

    if (start >= 0)
    {
        runs.append({start, columns - 1});
    }

    return runs;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportSheets::All(int columns, int rows) -> Set
{
    Set sheets;
    sheets.columns = std::max(columns, 0);
    sheets.rows = std::max(rows, 0);
    sheets.count = sheets.columns * sheets.rows;
    sheets.ranks.resize(sheets.count);

    for (int i = 0; i < sheets.count; ++i)
    {
        sheets.ranks[i] = i + 1;
    }

    return sheets;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportSheets::Compute(const ExportArtboards::Grid &grid, const QVector<QPolygonF> &ink,
                           const QRectF &reserved) -> Set
{
    Set sheets;

    if (!grid.valid || grid.columns < 1 || grid.rows < 1 || grid.cellWidthPx <= 0.0
        || grid.cellHeightPx <= 0.0)
    {
        return sheets;
    }

    sheets.columns = grid.columns;
    sheets.rows = grid.rows;
    sheets.ranks.fill(0, static_cast<qsizetype>(grid.columns) * grid.rows);

    // Row-major, so that the ranks are handed out in reading order — which is what the page number
    // means. The number is a rank and not a place: a sheet's number counts the sheets that are
    // printed, so dropping one renumbers the sheets that follow it.
    for (int row = 0; row < grid.rows; ++row)
    {
        for (int col = 0; col < grid.columns; ++col)
        {
            const QRectF cell(grid.planRect.left() + col * grid.cellWidthPx,
                              grid.planRect.top() + row * grid.cellHeightPx, grid.cellWidthPx,
                              grid.cellHeightPx);

            // The reserved block first, and for the one sheet it lands on: it is not ink the plan
            // carries, but the sheet it stands on is printed all the same — see the note in the
            // header. Tested before the placements so that the sheet is printed even when no piece
            // comes near it.
            if (!reserved.isEmpty() && cell.intersects(reserved))
            {
                sheets.ranks[row * grid.columns + col] = ++sheets.count;
                continue;
            }

            const QPolygonF sheet(cell);

            for (const QPolygonF &shape : ink)
            {
                // Meets, and also contains or is contained — a piece smaller than a sheet lies
                // wholly inside its polygon and would be missed by a test on the edges alone.
                if (sheet.intersects(shape))
                {
                    sheets.ranks[row * grid.columns + col] = ++sheets.count;
                    break;
                }
            }
        }
    }

    return sheets;
}
