/************************************************************************
 **  @file   reduce.cpp
 **  @brief  The layer reducer, exercisable outside the window.
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

#include "core/layerreducer.h"

#include <QCoreApplication>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QStringList args = app.arguments().mid(1);
    if (args.size() < 3)
    {
        out << "Usage: reduce <in.pdf> <out.pdf> <layer name>...\n";
        return 1;
    }

    QString error;
    const QVector<PdfLayer> layers = LayerReducer::Read(args.at(0), &error);
    if (layers.isEmpty())
    {
        out << "ERR " << error << "\n";
        return 1;
    }

    QVector<int> keep;
    for (const PdfLayer &layer : layers)
    {
        const bool wanted = args.mid(2).contains(layer.name);
        out << (wanted ? "keep " : "cut  ") << layer.name << " (" << layer.object << ")\n";
        if (wanted)
        {
            keep.append(layer.object);
        }
    }

    if (!LayerReducer::Reduce(args.at(0), args.at(1), keep, &error))
    {
        out << "ERR " << error << "\n";
        return 1;
    }

    out << "Written " << args.at(1) << "\n";
    return 0;
}
