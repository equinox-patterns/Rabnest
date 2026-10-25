/************************************************************************
 **
 **  @file   print.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The print run's figures without a click — it writes nothing, it prints a tiling.
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

#include "core/importeddocument.h"
#include "core/print/exportcoverpage.h"
#include "core/print/printmetrics.h"
#include "core/print/printplan.h"
#include "core/print/printpdf.h"
#include "core/print/printrecipe.h"
#include "app/print/exportmarkpainter.h"
#include "app/print/printpanel.h"

#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QSvgGenerator>

#include <QFontDatabase>
#include <QApplication>
#include <QGuiApplication>
#include <QFile>
#include <QStringList>
#include <QTextStream>

#include <algorithm>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QFontDatabase::addApplicationFont(QStringLiteral(":/fonts/inter/Inter-VariableFont.ttf"));
    const QStringList arguments = app.arguments();

    // `--panel` builds the panel on the recipe's model and prints what it says — the fields read
    // through the same calls the window makes, without a screen to look at them on.
    if (arguments.contains(QStringLiteral("--panel")))
    {
        PrintPanel panel;
        panel.SetModel(QStringLiteral("A4LetterFramed"), 5);
        panel.SetSizesText(QStringLiteral("Size range 32–46"));

        QTextStream out(stdout);
        const ExportPrintModel *chosen = panel.Model();
        out << "panel: model=" << (chosen != nullptr ? chosen->token : QStringLiteral("none"))
            << " columns=" << panel.Columns()
            << " marginMm=" << panel.MarginMm()
            << " gapMm=" << panel.GapMm()
            << " unit=" << panel.Unit()
            << "\n";
        return 0;
    }

    // `--decor=<file.svg>` writes the decor alone, as Ostara's underlay writes it: one document over
    // the whole plan, painted by the same painter the canvas uses. It is how the port is checked
    // without a screen.
    QString decorPath;
    QString gridSpec;
    QString coverPng;
    QString moveSpec;
    QString svgPath;
    QString sizesSpec;
    for (const QString &argument : arguments)
    {
        if (argument.startsWith(QStringLiteral("--decor=")))
        {
            decorPath = argument.mid(8);
        }
        else if (argument.startsWith(QStringLiteral("--svg=")))
        {
            svgPath = argument.mid(6);
        }
        else if (argument.startsWith(QStringLiteral("--sizes=")))
        {
            sizesSpec = argument.mid(8);
        }
        else if (argument.startsWith(QStringLiteral("--grid=")))
        {
            gridSpec = argument.mid(7);
        }
        else if (argument.startsWith(QStringLiteral("--cover-png=")))
        {
            coverPng = argument.mid(12);
        }
        else if (argument.startsWith(QStringLiteral("--move=")))
        {
            moveSpec = argument.mid(7);
        }
    }

    if (arguments.size() < 2)
    {
        QTextStream(stderr) << "usage: rabnest_print <pdf|svg> [columns] [margin_mm] [gap_mm]\n";
        return 2;
    }

    ImportedDocument document;
    if (arguments.at(1).endsWith(QStringLiteral(".svg")))
    {
        QFile file(arguments.at(1));
        if (!file.open(QIODevice::ReadOnly))
        {
            QTextStream(stderr) << "cannot open " << arguments.at(1) << "\n";
            return 1;
        }
        document = ImportedDocument::FromPayload(file.readAll(), arguments.at(1));
    }
    else
    {
        document = ImportedDocument::Read(arguments.at(1));
    }

    if (!document.isValid() || document.kind() != ImportedDocument::Kind::Ostara)
    {
        QTextStream(stderr) << "not an Ostara PDF: " << document.error() << "\n";
        return 1;
    }

    QTextStream out(stdout);

    const PrintRecipe recipe = ReadPrintRecipe(document.payload());
    out << "recipe: " << (recipe.format.present ? "yes" : "no")
        << " token=" << recipe.format.token << " framed=" << (recipe.format.framed ? 1 : 0)
        << " columns=" << recipe.format.columns
        << " document=" << recipe.format.documentWidthMm << "mm"
        << " sheetH=" << recipe.format.frameHeightMm << "mm"
        << " margin=" << recipe.format.marginMm << "mm"
        << " gap=" << recipe.format.gapMm << "mm"
        << " decor=" << (recipe.hasDecor ? recipe.decor.name : QStringLiteral("none"))
        << " cover=" << recipe.coverSvg.size() << "B\n";

    const ExportPrintModel *model = FindExportPrintModel(recipe.format.token);
    if (model == nullptr)
    {
        model = FindExportPrintModel(QStringLiteral("A0Roll"));
    }

    // The positional figures are read only where a figure stands: a flag in their place is not a
    // column count, and reading it as one quietly asked for a single column.
    const QStringList plain = [&arguments] {
        QStringList out;
        for (int index = 1; index < arguments.size(); ++index)
        {
            if (!arguments.at(index).startsWith(QStringLiteral("--")))
            {
                out.append(arguments.at(index));
            }
        }
        return out;
    }();

    int columns = plain.size() > 1 ? plain.at(1).toInt() : recipe.format.columns;
    if (columns < 1)
    {
        columns = 1;
    }
    const double marginMm = plain.size() > 2 ? plain.at(2).toDouble() : recipe.format.marginMm;
    const double gapMm = plain.size() > 3 ? plain.at(3).toDouble() : recipe.format.gapMm;

    const double documentWidthMm = model->widthMm * static_cast<double>(columns);
    const double binWidthPx = PrintMetrics::MmToPx(documentWidthMm);

    // `--grid=5x4` paints the decor on a demonstration grid and stops: nothing is nested, so the
    // port is checked in seconds rather than in the minutes a full run costs.
    if (!decorPath.isEmpty() && !gridSpec.isEmpty())
    {
        const QStringList parts = gridSpec.split(QLatin1Char('x'));
        const int gridColumns = parts.value(0).toInt();
        const int gridRows = parts.value(1).toInt();
        if (gridColumns < 1 || gridRows < 1)
        {
            QTextStream(stderr) << "bad grid: " << gridSpec << "\n";
            return 2;
        }

        ExportMarkGrid grid;
        grid.columns = gridColumns;
        grid.rows = gridRows;
        grid.sheetWidthPx = PrintMetrics::MmToPx(model->widthMm);
        grid.sheetHeightPx = PrintMetrics::MmToPx(model->heightMm > 0.0 ? model->heightMm : 297.0);
        grid.pxPerMm = PrintMetrics::MmToPx(1.0);
        grid.sheets = ExportSheets::All(gridColumns, gridRows);

        const ExportTemplate fallback = ExportTemplate::RabnestDefault();
        const ExportTemplate &decor = recipe.hasDecor ? recipe.decor : fallback;

        ExportMarkPainter painter;
        painter.SetFrameSettings(decor.frame);
        painter.SetMarkSettings(decor.marks);
        painter.SetPaginationSettings(decor.pagination);

        const QRectF rect(0.0, 0.0, grid.sheetWidthPx * gridColumns,
                          grid.sheetHeightPx * gridRows);

        QSvgGenerator generator;
        generator.setFileName(decorPath);
        generator.setResolution(96);
        generator.setSize(QSize(static_cast<int>(rect.width()), static_cast<int>(rect.height())));
        generator.setViewBox(rect);

        QPainter svgPainter(&generator);
        painter.Paint(svgPainter, QPointF(0.0, 0.0), grid, model->hasFrame, false);
        svgPainter.end();

        out << "decor written: " << decorPath << " (" << gridColumns << "x" << gridRows << ")\n";
        return 0;
    }

    // `--cover-png=…` draws the payload's cover page exactly as the canvas draws it — Qt's own SVG
    // renderer, no Trame in between — so the one question that matters is answered by the code
    // that will actually be on screen.
    if (!coverPng.isEmpty())
    {
        if (recipe.coverSvg.isEmpty())
        {
            QTextStream(stderr) << "no cover page in this payload\n";
            return 2;
        }

        QSvgRenderer renderer(recipe.coverSvg);
        out << "svg renderer valid: " << (renderer.isValid() ? "yes" : "no")
            << " defaultSize=" << renderer.defaultSize().width() << "x"
            << renderer.defaultSize().height() << "\n";

        if (!renderer.isValid())
        {
            return 1;
        }

        QImage image(760, 1036, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::white);
        QPainter painter(&image);
        renderer.render(&painter, QRectF(0, 0, image.width(), image.height()));
        painter.end();
        image.save(coverPng);
        out << "cover written: " << coverPng << "\n";
        return 0;
    }

    const CoverSeat seat = ExportCoverPage::SeatOf(model);
    const double marginPx = PrintMetrics::MmToPx(marginMm);
    const QRectF coverBlock =
        seat == CoverSeat::InSheet ? ExportCoverPage::BlockPx(seat, marginPx) : QRectF();

    // `--sizes=Taille_40,Taille_46` : le run d'un sous-ensemble de tailles, tel qu'un panneau le
    // demande. C'est le cas qui compte pour l'ancrage du décor — un run qui ne contient pas la taille
    // de référence n'a rien pour asseoir ses enfants.
    const QStringList wanted = sizesSpec.isEmpty()
                                   ? QStringList()
                                   : sizesSpec.split(QLatin1Char(','), Qt::SkipEmptyParts);

    QStringList sizeIds;
    QStringList pieces;
    for (const ImportedLayer &layer : document.layers())
    {
        if (layer.decor)
        {
            continue;
        }
        if (!wanted.isEmpty() && !wanted.contains(layer.id))
        {
            continue;
        }
        sizeIds.append(layer.id);
        for (const QString &piece : document.Pieces({layer.id}))
        {
            if (!pieces.contains(piece))
            {
                pieces.append(piece);
            }
        }
    }

    out << "model: " << model->token << " sheet=" << model->widthMm << "x" << model->heightMm
        << "mm framed=" << (model->hasFrame ? 1 : 0) << " columns=" << columns
        << " document=" << documentWidthMm << "mm" << " seat=" << static_cast<int>(seat) << "\n";
    out << "run: sizes=" << sizeIds.join(QStringLiteral(",")) << " pieces=" << pieces.size()
        << " window=" << binWidthPx << "px\n";

    PrintPlan plan = PrintPlan::Generate(document, sizeIds, pieces, marginMm / 10.0,
                                               gapMm / 10.0, binWidthPx, columns, model->heightMm,
                                               coverBlock, QString::fromLatin1(model->token));

    if (!plan.ok)
    {
        out << "REFUSED: " << plan.error << "\n";
        return 1;
    }

    out << "plan: usedHeight=" << PrintMetrics::PxToMm(plan.usedHeightPx) << "mm"
        << " grid=" << plan.grid.columns << "x" << plan.grid.rows
        << " sheets=" << plan.sheets.count << " (of " << (plan.grid.columns * plan.grid.rows) << ")"
        << "\n";
    out << "pages: "
        << plan.Pages(seat == CoverSeat::ExtraSheet && !recipe.coverSvg.isEmpty())
        << " (cover " << (seat == CoverSeat::ExtraSheet ? "on its own sheet" : "on the plan") << ")\n";

    out << "placements: " << plan.placements.size() << "\n";
    for (int index = 0; index < static_cast<int>(plan.placements.size()) && index < 3; ++index)
    {
        const Placement &placement = plan.placements.at(static_cast<std::size_t>(index));
        out << "  #" << index << " " << plan.footprint.keys.at(static_cast<std::size_t>(index))
            << " at " << placement.x << "," << placement.y << " rot " << placement.rotation << "\n";
    }

    // `--svg=…` écrit le document du plan tel que le PDF l'assemble : un `<g>` par pièce, sa
    // transformation, ses tracés, puis le décor. C'est le seul moyen de MESURER l'impression —
    // sans lui, un décor décalé à l'impression ne peut être ni situé ni chiffré.
    if (!svgPath.isEmpty())
    {
        PrintPdf::Job job;
        job.payload = document.payload();
        job.plan = plan;
        job.decor = recipe.hasDecor ? recipe.decor : ExportTemplate::RabnestDefault();
        job.framed = model->hasFrame;
        job.coverSvg = recipe.coverSvg;
        job.sourcePath = arguments.at(1);
        job.title = QStringLiteral("print");

        if (seat != CoverSeat::None && !recipe.coverSvg.isEmpty() && plan.grid.valid)
        {
            job.coverPaper = ExportCoverPage::PaperPx(seat, marginPx, plan.grid.planRect,
                                                     plan.grid.cellWidthPx, plan.grid.cellHeightPx);
            job.coverPage = ExportCoverPage::PagePx(seat, marginPx, plan.grid.planRect,
                                                   plan.grid.cellWidthPx, plan.grid.cellHeightPx);
        }

        QString svgError;
        if (!PrintPdf::WriteSvg(job, svgPath, &svgError))
        {
            QTextStream(stderr) << "svg: " << svgError << "\n";
            return 1;
        }
        out << "svg written: " << svgPath << "\n";
    }

    if (!decorPath.isEmpty() && plan.grid.valid)
    {
        ExportMarkGrid grid;
        grid.columns = plan.grid.columns;
        grid.rows = plan.grid.rows;
        grid.sheetWidthPx = plan.grid.cellWidthPx;
        grid.sheetHeightPx = plan.grid.cellHeightPx;
        grid.pxPerMm = PrintMetrics::MmToPx(1.0);
        grid.sheets = plan.sheets;

        ExportMarkPainter painter;
        painter.SetFrameSettings(recipe.hasDecor ? recipe.decor.frame : ExportTemplate::RabnestDefault().frame);
        painter.SetMarkSettings(recipe.hasDecor ? recipe.decor.marks : ExportTemplate::RabnestDefault().marks);
        painter.SetPaginationSettings(recipe.hasDecor ? recipe.decor.pagination
                                                      : ExportTemplate::RabnestDefault().pagination);

        QSvgGenerator generator;
        generator.setFileName(decorPath);
        generator.setResolution(96);
        generator.setSize(QSize(static_cast<int>(plan.grid.planRect.width()),
                                static_cast<int>(plan.grid.planRect.height())));
        generator.setViewBox(QRectF(0.0, 0.0, plan.grid.planRect.width(),
                                    plan.grid.planRect.height()));
        generator.setTitle(QStringLiteral("decor"));

        QPainter svgPainter(&generator);
        painter.Paint(svgPainter, QPointF(0.0, 0.0), grid, model->hasFrame, false);
        svgPainter.end();

        out << "decor written: " << decorPath << "\n";
    }

    // `--move=i,dx,dy` moves one placement and reads the sheets again — the gesture the canvas
    // makes, without a canvas. It is how « the paper follows the ink » is checked.
    if (!moveSpec.isEmpty())
    {
        const QStringList parts = moveSpec.split(QLatin1Char(','));
        const int index = parts.value(0).toInt();
        const double dx = parts.value(1).toDouble();
        const double dy = parts.value(2).toDouble();
        if (index < 0 || index >= static_cast<int>(plan.placements.size()))
        {
            QTextStream(stderr) << "bad placement index\n";
            return 2;
        }

        out << "move #" << index << " by " << dx << "," << dy
            << ": sheets before=" << plan.sheets.count;
        plan.placements[static_cast<std::size_t>(index)].x += dx;
        plan.placements[static_cast<std::size_t>(index)].y += dy;
        plan.RefreshSheets();
        out << " after=" << plan.sheets.count << "\n";
    }

    for (const PrintLayer &layer : plan.layers)
    {
        out << "  layer " << layer.label << " pieces=" << layer.pieces.size()
            << " colour=" << layer.colour.name() << " dashes=" << layer.dashes.size() << "\n";
    }

    // **Ce que le décor dessine, contre la pièce qu'il décore — dans le repère du CONTENU.**
    // Le plan applique la même transformation aux deux, donc elle s'annule : ce qui reste est
    // l'écart véritable entre le dessin et sa pièce, et c'est là qu'un décor mal assis se voit.
    for (const PrintLayer &layer : plan.layers)
    {
        if (!layer.reference)
        {
            continue;
        }

        // Chaque taille DESSINÉE, et son contour : c'est contre eux que l'assise du décor se juge.
        for (const PrintLayer &other : plan.layers)
        {
            if (other.hidden)
            {
                continue;
            }
            for (const PrintPiece &drawn : other.pieces)
            {
                if (drawn.outline.isEmpty())
                {
                    continue;
                }
                double ox0 = 1e18, oy0 = 1e18, ox1 = -1e18, oy1 = -1e18;
                for (const QPointF &point : drawn.outline)
                {
                    ox0 = std::min(ox0, point.x()); ox1 = std::max(ox1, point.x());
                    oy0 = std::min(oy0, point.y()); oy1 = std::max(oy1, point.y());
                }
                out << "      PIECE " << other.label << " " << drawn.name
                    << " x[" << ox0 << "," << ox1 << "] y[" << oy0 << "," << oy1 << "]\n";
            }
        }

        for (const PrintPiece &piece : layer.pieces)
        {
            if (piece.decor.content.isEmpty() || piece.outline.isEmpty())
            {
                continue;
            }

            double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
            for (const QPointF &point : piece.outline)
            {
                x0 = std::min(x0, point.x()); x1 = std::max(x1, point.x());
                y0 = std::min(y0, point.y()); y1 = std::max(y1, point.y());
            }

            const QRectF &box = piece.decor.box;
            const QRectF inside = QRectF(x0, y0, x1 - x0, y1 - y0).intersected(box);
            const double ratio = inside.isEmpty() ? 0.0 : inside.width() * inside.height()
                                                          / (box.width() * box.height());

            out << "      decor " << piece.name
                << " : piece x[" << x0 << "," << x1 << "] y[" << y0 << "," << y1 << "]"
                << " | dessin x[" << box.left() << "," << box.right()
                << "] y[" << box.top() << "," << box.bottom() << "]"
                << " | dedans " << int(ratio * 100.0) << "%\n";
        }
    }

    return plan.sheets.count > 0 ? 0 : 1;
}
