/************************************************************************
 **
 **  @file   layerreducer.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Cutting layers OUT of a PDF somebody else wrote.
 **
 **  @copyright
 **
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

#ifndef LAYERREDUCER_H
#define LAYERREDUCER_H

#include "core/importeddocument.h"

#include <QString>
#include <QStringList>
#include <QVector>

/// A layer of a PDF written by another application, as its optional content declares it.
struct PdfLayer
{
    int object = 0;  ///< the /OCG object number, which the panel and the content both name
    QString name;    ///< its /Name — what the reader shows
};

/// The two things Rabnest does with a foreign PDF: read its layers, and write it back with fewer.
///
/// **Switching layers off is not reducing.** A PDF whose unwanted layers are merely hidden still
/// contains them, and a printer — who receives a file and prints it — can turn them back on, or his
/// RIP can draw them. So the layers are REMOVED from the page's own drawing, and the file is written
/// as an incremental update: the bytes of the original stay exactly as they were, and only the
/// page's stream, the catalogue and the cross-reference table are appended. Nothing else is
/// rewritten, so nothing else can be broken.
///
/// Deliberately narrow. It understands the shape Illustrator and Inkscape produce — marked content
/// `/OC /MCn BDC … EMC` inside a Flate page stream — and refuses anything else with a sentence
/// rather than guessing.
namespace LayerReducer
{
/// The layers of @p path, in panel order. Empty on failure, with @p error set.
auto Read(const QString &path, QString *error) -> QVector<PdfLayer>;

/// Writes @p path to @p target without the layers whose object number is not in @p keep.
auto Reduce(const QString &path, const QString &target, const QVector<int> &keep, QString *error)
    -> bool;
}  // namespace LayerReducer

#endif  // LAYERREDUCER_H
