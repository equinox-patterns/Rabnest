/************************************************************************
 **
 **  @file   printpdf.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   24/10/2026
 **
 **  @brief  The print run written as a layered PDF.
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

#include "core/print/printpdf.h"

#include "app/print/exportmarkpainter.h"
#include "core/layerreducer.h"
#include "core/print/exportartboards.h"
#include "core/print/exportprintmodel.h"
#include "core/print/exportsheets.h"
#include "core/print/printmetrics.h"

#include "pdf/SvgToOcgPdf.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QSize>
#include <QPrinter>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QXmlStreamWriter>

#include <algorithm>
#include <cmath>

namespace
{
//---------------------------------------------------------------------------------------------------------------------
/// The name the DRAWING gives a piece — Ostara's own `OSvg::SanitizeSvgId`, and not a rule of ours.
///
/// The same rule `pattern.cpp` reads the payload with: the drawing keeps letters, digits, `_` and
/// `-` and turns EVERY other character into an underscore. Written here rather than imported so a
/// file this module writes is named the way the file it was read from is.
auto Slug(const QString &name) -> QString
{
    QString slug;
    slug.reserve(name.size());
    for (const QChar character : name)
    {
        slug += (character.isLetterOrNumber() || character == QLatin1Char('_')
                 || character == QLatin1Char('-'))
                    ? character
                    : QLatin1Char('_');
    }
    if (!slug.isEmpty() && slug.front().isDigit())
    {
        slug.prepend(QLatin1Char('_'));
    }
    return slug;
}

//---------------------------------------------------------------------------------------------------------------------
/// The name the LAYOUT layer is written under, as the file this run came from names it.
///
/// Ostara lets the creator type it, so it is a label and never a key: what identifies the layout is
/// the `ostara:layout` marker. The label is still needed — it is what a reader lists and what this
/// file must put back unchanged. Empty when the payload carries no layout layer at all.
auto LayoutName(const QByteArray &payload) -> QString
{
    QDomDocument source;
    if (!source.setContent(payload))
    {
        return {};
    }

    for (QDomElement element = source.documentElement().firstChildElement(QStringLiteral("g"));
         !element.isNull(); element = element.nextSiblingElement(QStringLiteral("g")))
    {
        if (element.attribute(QStringLiteral("ostara:layout")) != QLatin1String("yes"))
        {
            continue;
        }

        QString label = element.attribute(QStringLiteral("inkscape:label"));
        if (label.isEmpty())
        {
            label = element.firstChildElement(QStringLiteral("title")).text();
        }
        if (label.isEmpty())
        {
            label = element.attribute(QStringLiteral("id"));
        }
        return label.isEmpty() ? QCoreApplication::translate("PrintPdf", "Layout") : label;
    }

    return {};
}

//---------------------------------------------------------------------------------------------------------------------
/// Whether the layout of @p job is one of the reader's layers, read where the answer lives.
///
/// **The source PDF and nothing else.** The choice exists only as an export box, and Ostara persists
/// it nowhere, so the file it produced is the only record: a layout the reader's panel can switch off
/// is one the author asked to be switchable, and a layout drawn into the page is one she asked not
/// to be. Anything else — a source that cannot be read, a payload with no layout at all — answers no,
/// which draws the decor where it stands and leaves nothing switchable.
auto LayoutIsALayer(const PrintPdf::Job &job) -> bool
{
    if (job.sourcePath.isEmpty())
    {
        return false;
    }

    const QString name = LayoutName(job.payload);
    if (name.isEmpty())
    {
        return false;
    }

    QString error;
    const QVector<PdfLayer> layers = LayerReducer::Read(job.sourcePath, &error);
    for (const PdfLayer &layer : layers)
    {
        if (layer.name == name)
        {
            return true;
        }
    }

    return false;
}

//---------------------------------------------------------------------------------------------------------------------
/// The paper this run takes, in the plan's own pixels — the one rectangle the whole file lives in.
///
/// A tiled run is its whole grid, the last row included whether or not the nesting filled it; a roll
/// and the A0 sheet have no grid at all and are the bin the run came out of.
auto PlanRect(const PrintPlan &plan) -> QRectF
{
    if (plan.grid.valid)
    {
        return plan.grid.planRect;
    }
    return QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx);
}

//---------------------------------------------------------------------------------------------------------------------
auto Number(qreal value) -> QString
{
    return QString::number(value, 'f', 4);
}

//---------------------------------------------------------------------------------------------------------------------
/// The transform that puts one piece where the plan put it: `(x, y) + R(rotation) · (p − origin)`.
///
/// The same arithmetic as `PlacedPoint`, written as a transform rather than baked into every
/// coordinate — the payload's paths are the creator's own figures, and re-plotting them one point at
/// a time would rewrite a drawing to place it.
auto PlacementTransform(const Placement &placement, const QPointF &origin) -> QString
{
    return QStringLiteral("translate(%1,%2) rotate(%3) translate(%4,%5)")
        .arg(Number(placement.x), Number(placement.y), Number(placement.rotation),
             Number(-origin.x()), Number(-origin.y()));
}

//---------------------------------------------------------------------------------------------------------------------
/// One polygon of ink as a path, closed.
auto PolygonPath(const QPolygonF &polygon) -> QString
{
    QString path;
    path.reserve(polygon.size() * 20);

    for (int index = 0; index < polygon.size(); ++index)
    {
        const QPointF &point = polygon.at(index);
        path += (index == 0 ? QStringLiteral("M %1,%2") : QStringLiteral(" L %1,%2"))
                    .arg(Number(point.x()), Number(point.y()));
    }

    if (!path.isEmpty())
    {
        path += QLatin1Char('Z');
    }
    return path;
}

//---------------------------------------------------------------------------------------------------------------------
/// What a size is DRAWN with in the file.
///
/// **The canvas's own floor is not here.** On screen a plan is seen zoomed out, and a line thinner
/// than a pixel is a grey smear, so the canvas holds a minimum; a sheet of paper has no such
/// problem, and a file drawn at the canvas's floor would print every pattern at one weight. The
/// weight is the pattern's, and the plain outline is the only exception — it is the user's answer to
/// making a single-size plan readable, and it applies to what leaves as much as to what is shown.
void SetInkStyle(QDomElement &path, const PrintLayer &layer, bool plain)
{
    path.setAttribute(QStringLiteral("fill"), QStringLiteral("none"));

    // ⚠️ **Le trait d'une projection n'est pas décidé ici.** La couche le porte (noir, plein, 7 pt,
    // voir `PrintPlan::Generate`), pour que le canevas et le fichier lisent la même valeur : un trait
    // décidé à l'écriture ne serait pas celui de l'écran, et c'est exactement ce qui se voyait.
    if (plain)
    {
        path.setAttribute(QStringLiteral("stroke"), QStringLiteral("#000000"));
    }
    else
    {
        path.setAttribute(QStringLiteral("stroke"), layer.colour.name());
        if (!layer.dashes.isEmpty())
        {
            QStringList pattern;
            pattern.reserve(layer.dashes.size());
            for (const qreal value : layer.dashes)
            {
                pattern.append(QString::number(value, 'f', 5));
            }
            path.setAttribute(QStringLiteral("stroke-dasharray"), pattern.join(QLatin1Char(' ')));
        }
    }

    // A payload that states no weight — an export from a writer older than the attribute — gets
    // Ostara's hairline, which is what such a pen was drawn with: 1,2 mm over three.
    const qreal width = layer.width > 0.0 ? layer.width : PrintMetrics::MmToPx(1.2 / 3.0);
    path.setAttribute(QStringLiteral("stroke-width"), QString::number(width, 'f', 5));
}

//---------------------------------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------------------------------
/// The `<metadata>` the file carries, read from the file the run came from and restated for THIS
/// run.
///
/// **The recipe is the one thing that cannot be copied.** It states the paper the plan was laid for
/// — the format, the columns, the margin, the space between pieces — and the run may have been laid
/// on other paper than the file it was imported from, in which case a copied recipe would have the
/// file claim a sheet it was never printed on. Everything else in it is the creator's and is copied
/// as it stands: the pieces say what may be DONE with a piece, which no drawing and no placement
/// can state, and the cover page is a document of its own.
///
/// @return false when there is nothing to carry and no way to state the paper.
auto CarryMetadata(QDomDocument &document, const QDomElement &source, const PrintPlan &plan,
                   bool projector) -> bool
{
    const ExportPrintModel *model = FindExportPrintModel(plan.modelToken);

    // **Une projection n'a pas de modèle de papier, et n'en a pas besoin.** Sa page est celle que la
    // recherche a trouvée, et la recette la porte telle quelle : c'est ce qui fait d'un fichier
    // projecteur un fichier qu'on relit, au lieu d'un fichier qu'on refuse.
    if (model == nullptr && !projector)
    {
        return false;
    }

    QDomElement metadata = source.firstChildElement(QStringLiteral("metadata"));
    if (!metadata.isNull())
    {
        metadata = document.importNode(metadata, true).toElement();
    }
    else
    {
        metadata = document.createElement(QStringLiteral("metadata"));
    }
    document.documentElement().appendChild(metadata);

    QDomElement recipe = metadata.firstChildElement(QStringLiteral("exportRecipe"));
    if (recipe.isNull())
    {
        recipe = document.createElement(QStringLiteral("exportRecipe"));
        recipe.setAttribute(QStringLiteral("version"), QStringLiteral("1"));
        metadata.insertBefore(recipe, metadata.firstChild());
    }

    QDomElement format = recipe.firstChildElement(QStringLiteral("format"));
    if (format.isNull())
    {
        format = document.createElement(QStringLiteral("format"));
        recipe.insertBefore(format, recipe.firstChild());
    }

    format.setAttribute(QStringLiteral("token"), plan.modelToken);
    format.setAttribute(QStringLiteral("framed"),
                         model != nullptr && model->hasFrame ? QStringLiteral("1")
                                                             : QStringLiteral("0"));
    format.setAttribute(QStringLiteral("columns"), QString::number(std::max(plan.columns, 1)));
    format.setAttribute(QStringLiteral("documentWidthMm"),
                         model != nullptr
                             ? QString::number(model->widthMm * std::max(plan.columns, 1), 'f', 4)
                             : QString::number(PrintMetrics::PxToMm(plan.binWidthPx), 'f', 4));

    // Le marqueur de destination : une projection le dit, et c'est ce qui l'empêche d'être relue
    // comme une impression — sans lui, Rabnest la re-nidifierait en écart zéro et en tuilé.
    if (projector)
    {
        format.setAttribute(QStringLiteral("destination"), QStringLiteral("projector"));
    }
    format.setAttribute(QStringLiteral("frameHeightMm"), QString::number(plan.sheetHeightMm, 'f', 4));
    format.setAttribute(QStringLiteral("marginMm"), QString::number(plan.marginCm * 10.0, 'f', 4));
    format.setAttribute(QStringLiteral("gapMm"), QString::number(plan.gapCm * 10.0, 'f', 4));

    // **A file that draws no decor does not claim one.** A roll and the A0 sheet cut no sheets, so
    // they have no frame, no numbering and no marks: the recipe says nothing about a decor, and a
    // reader that reopens the file supplies its own rather than reproducing one that is not there.
    if (!plan.grid.valid)
    {
        const QDomElement decor = recipe.firstChildElement(QStringLiteral("decor"));
        if (!decor.isNull())
        {
            recipe.removeChild(decor);
        }
    }

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/// The plan itself: one layer per size, the layout over them, all of it one document.
///
/// **`layered` is the same rule as Ostara's** — several sizes make layers, and one size makes none.
/// A layer set of one would put a single entry in the reader's panel, switching something that is
/// the whole drawing on and off; and the layers exist to tell sizes apart, which one size cannot.
///
/// @param layoutLayer the layout's own id and name, as the source PDF gave them; empty when it has
///                    none, and then the decor is written where it is drawn and named nowhere.
auto WritePlan(const PrintPdf::Job &job, const QString &path, bool withPayload, QString *error)
    -> bool
{
    const PrintPlan &plan = job.plan;
    const QRectF rect = PlanRect(plan);
    if (rect.width() <= 0.0 || rect.height() <= 0.0)
    {
        *error = QCoreApplication::translate("PrintPdf", "The plan has no paper to be written on.");
        return false;
    }

    QDomDocument source;
    if (!source.setContent(job.payload))
    {
        *error = QCoreApplication::translate(
            "PrintPdf", "What the plan was drawn from could not be read, so nothing can be "
                        "written beside it.");
        return false;
    }

    QDomDocument document;
    QDomElement root = document.createElement(QStringLiteral("svg"));
    root.setAttribute(QStringLiteral("version"), QStringLiteral("1.1"));
    root.setAttribute(QStringLiteral("xmlns"), QStringLiteral("http://www.w3.org/2000/svg"));
    root.setAttribute(QStringLiteral("xmlns:i"),
                      QStringLiteral("http://ns.adobe.com/AdobeIllustrator/10.0/"));
    root.setAttribute(QStringLiteral("xmlns:inkscape"),
                      QStringLiteral("http://www.inkscape.org/namespaces/inkscape"));
    root.setAttribute(QStringLiteral("xmlns:ostara"), QStringLiteral("urn:ostara"));
    root.setAttribute(QStringLiteral("width"),
                      QStringLiteral("%1mm").arg(PrintMetrics::PxToMm(rect.width()), 0, 'f', 4));
    root.setAttribute(QStringLiteral("height"),
                      QStringLiteral("%1mm").arg(PrintMetrics::PxToMm(rect.height()), 0, 'f', 4));
    root.setAttribute(QStringLiteral("viewBox"),
                      QStringLiteral("0 0 %1 %2").arg(Number(rect.width()), Number(rect.height())));
    document.appendChild(root);

    // ⚠️ **The metadata is the payload, and a payload is not a drawing.** `QSvgRenderer` draws the
    // children of every element it does not know — `<metadata>` included — and the payload carries
    // every piece's decor: rendered, those cartouches and fold lines land all over the plan, at
    // places that belong to no sheet. A file keeps it (it is what Rabnest reopens); a printer does
    // not, so a document made for rendering leaves it out.
    if (withPayload && !CarryMetadata(document, source.documentElement(), plan, job.projector))
    {
        *error = QCoreApplication::translate("PrintPdf", "The format this run was laid for is not "
                                                         "one Rabnest knows, so its paper cannot be "
                                                         "stated.");
        return false;
    }

    // ⚠️ **Une projection est TOUJOURS calquée**, même en taille unique : son fond et ses deux
    // grilles sont trois choses que l'utilisatrice doit pouvoir éteindre, et un document plat n'en
    // porterait aucun.
    const bool layered = job.projector || plan.layers.size() > 1;

    // ── Les calques du projecteur : SOUS les pièces ─────────────────────────────────────────────
    //
    // Écrits par le SOCLE (`io/GradedOverlays.h`), les mêmes qu'Ostara : un fond plein page puis les
    // grilles de calibration, chacun portant son rôle. Le fragment est produit par le même écrivain
    // puis adopté par ce document-ci — deux écritures des mêmes valeurs divergeraient au premier
    // changement de couleur ou de pas, et rien ne dirait laquelle est le projecteur.
    if (job.projector)
    {
        QByteArray fragmentXml;
        {
            QXmlStreamWriter writer(&fragmentXml);
            writer.writeStartElement(QStringLiteral("svg"));
            GradedOverlays::Write(writer, job.overlays, rect.width(), rect.height(),
                                  PrintMetrics::PtToPx(4.0), QStringLiteral("black"));
            writer.writeEndElement();
        }

        QDomDocument fragment;
        if (fragment.setContent(fragmentXml, false))
        {
            const QDomElement holder = fragment.documentElement();
            for (QDomNode child = holder.firstChild(); !child.isNull(); child = child.nextSibling())
            {
                root.appendChild(document.importNode(child, true));
            }
        }
    }

    // **The largest size is written FIRST and the smallest last.** Not an order of the run's own:
    // it is the order of the drawing. The sizes of a graded pattern all stand on the same pieces, so
    // the lines that fall on top of each other are the ones a reader sees — and Ostara writes them
    // from the largest down so that the smallest is drawn last, on top, where its finer line stays
    // readable. Writing them the other way would hide the smallest size under the largest.
    for (auto layer = plan.layers.crbegin(); layer != plan.layers.crend(); ++layer)
    {
        // **Une taille retenue pour asseoir le décor ne fait pas une couche.** Elle n'a pas d'encre :
        // écrire son groupe donnerait au lecteur un calque vide, qu'il proposerait d'éteindre alors
        // qu'il n'y a rien dedans. Ses enfants sont plus bas, dans la couche de mise en page.
        if (layer->hidden)
        {
            continue;
        }

        QDomElement group = root;
        if (layered)
        {
            const QString id = layer->sizeId.isEmpty() ? Slug(layer->label) : layer->sizeId;
            group = document.createElement(QStringLiteral("g"));
            group.setAttribute(QStringLiteral("id"), id);
            group.setAttribute(QStringLiteral("i:layer"), QStringLiteral("yes"));
            group.setAttribute(QStringLiteral("inkscape:groupmode"), QStringLiteral("layer"));
            group.setAttribute(QStringLiteral("inkscape:label"), layer->label);

            QDomElement caption = document.createElement(QStringLiteral("title"));
            caption.appendChild(document.createTextNode(layer->label));
            group.appendChild(caption);

            root.appendChild(group);
        }

        for (const PrintPiece &piece : layer->pieces)
        {
            if (piece.placementIndex < 0
                || piece.placementIndex >= static_cast<int>(plan.placements.size()))
            {
                continue;
            }

            if (piece.ink.isEmpty())
            {
                continue;
            }

            QDomElement pieceGroup = document.createElement(QStringLiteral("g"));
            pieceGroup.setAttribute(QStringLiteral("id"), Slug(piece.name));
            pieceGroup.setAttribute(
                QStringLiteral("transform"),
                PlacementTransform(plan.placements.at(static_cast<std::size_t>(piece.placementIndex)),
                                   plan.origins.at(piece.placementIndex)));
            group.appendChild(pieceGroup);

            for (const QPolygonF &polygon : piece.ink)
            {
                const QString data = PolygonPath(polygon);
                if (data.isEmpty())
                {
                    continue;
                }

                QDomElement path = document.createElement(QStringLiteral("path"));
                path.setAttribute(QStringLiteral("d"), data);
                SetInkStyle(path, *layer, plan.plainOutline);
                pieceGroup.appendChild(path);
            }
        }
    }

    // ── Le décor ────────────────────────────────────────────────────────────────────────────────
    //
    // **The children belong to ONE size, and never to all of them** — the same rule the canvas
    // draws by: a cartouche, an annotation or a grain line written once per size would be the same
    // words ten times over. The payload carries a copy per size all the same; the copy is a lookup
    // key, not a decision.
    // **Placed by the same frame as the ink.** The content is in the pattern's own pixels, so it
    // travels under the transform that takes the piece's own frame onto its placement.
    //
    // The layer's NAME is the creator's — Ostara lets her type it, so it is read from the file
    // rather than invented here.
    const QString layoutName = LayoutName(job.payload);

    QDomElement layout = root;
    if (layered)
    {
        layout = document.createElement(QStringLiteral("g"));
        layout.setAttribute(QStringLiteral("id"), Slug(layoutName));
        layout.setAttribute(QStringLiteral("i:layer"), QStringLiteral("yes"));
        layout.setAttribute(QStringLiteral("inkscape:groupmode"), QStringLiteral("layer"));
        layout.setAttribute(QStringLiteral("inkscape:label"), layoutName);
        layout.setAttribute(QStringLiteral("ostara:layout"), QStringLiteral("yes"));

        QDomElement caption = document.createElement(QStringLiteral("title"));
        caption.appendChild(document.createTextNode(layoutName));
        layout.appendChild(caption);

        root.appendChild(layout);
    }

    for (const PrintLayer &layer : plan.layers)
    {
        if (!layer.reference)
        {
            continue;
        }

        for (const PrintPiece &piece : layer.pieces)
        {
            if (piece.decor.content.isEmpty() || piece.placementIndex < 0
                || piece.placementIndex >= static_cast<int>(plan.placements.size()))
            {
                continue;
            }

            QDomElement wrapper = document.createElement(QStringLiteral("g"));
            wrapper.setAttribute(QStringLiteral("id"), Slug(piece.name));
            wrapper.setAttribute(
                QStringLiteral("transform"),
                PlacementTransform(plan.placements.at(static_cast<std::size_t>(piece.placementIndex)),
                                   plan.origins.at(piece.placementIndex)));

            // The children exactly as the payload writes them — the same fragment the canvas
            // renders, so the paper and the screen cannot show two drawings.
            QDomDocument fragment;
            if (fragment.setContent(QStringLiteral("<decor>%1</decor>").arg(piece.decor.content)))
            {
                for (QDomElement child = fragment.documentElement().firstChildElement(); !child.isNull();
                     child = child.nextSiblingElement())
                {
                    wrapper.appendChild(document.importNode(child, true));
                }
            }

            layout.appendChild(wrapper);
        }
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        *error = QCoreApplication::translate("PrintPdf", "The file %1 could not be written.")
                     .arg(QFileInfo(path).fileName());
        return false;
    }

    const QByteArray written = document.toByteArray(-1);
    const bool whole = file.write(written) == written.size();
    file.close();

    if (!whole)
    {
        *error = QCoreApplication::translate("PrintPdf", "The file %1 could not be written.")
                     .arg(QFileInfo(path).fileName());
    }
    return whole;
}

//---------------------------------------------------------------------------------------------------------------------
/// The paper's own frame, its numbering and its registration marks — a document of its own.
///
/// **Under the layers and outside every optional content group.** A sheet whose frame can be
/// switched off is a sheet that can be assembled wrongly, and it is the silent kind of wrong: the
/// print looks right. Written over the whole plan and not sheet by sheet, because a seam belongs to
/// the two sheets that share it and must carry one line from the plan's anchor, or the dashes
/// restart at every edge and the join shows.
auto WriteMarks(const PrintPdf::Job &job, const QString &path, QString *error) -> bool
{
    const PrintPlan &plan = job.plan;

    ExportMarkGrid grid;
    grid.columns = plan.grid.columns;
    grid.rows = plan.grid.rows;
    grid.sheetWidthPx = plan.grid.cellWidthPx;
    grid.sheetHeightPx = plan.grid.cellHeightPx;
    grid.pxPerMm = PrintMetrics::MmToPx(1.0);
    grid.sheets = plan.sheets;

    ExportMarkPainter painter;
    painter.SetFrameSettings(job.decor.frame);
    painter.SetMarkSettings(job.decor.marks);
    painter.SetPaginationSettings(job.decor.pagination);

    const QRectF rect = PlanRect(plan);

    QSvgGenerator generator;
    generator.setFileName(path);
    generator.setResolution(static_cast<int>(PrintMetrics::kPrintDpi));
    generator.setSize(QSize(static_cast<int>(std::lround(rect.width())),
                            static_cast<int>(std::lround(rect.height()))));
    generator.setViewBox(rect);

    {
        QPainter svgPainter(&generator);
        if (!svgPainter.isActive())
        {
            *error = QCoreApplication::translate("PrintPdf", "The sheet's marks could not be "
                                                             "drawn.");
            return false;
        }

        svgPainter.translate(-rect.topLeft());
        painter.Paint(svgPainter, rect.topLeft(), grid, job.framed, /*sheetGuide=*/false);
    }

    return true;
}

//---------------------------------------------------------------------------------------------------------------------
/// Gives @p path the page of @p planPath.
///
/// **A second document must be laid out on the plan's own page.** The marks are written through a
/// generator that states its own size, and a millimetre too many would have the writer refuse them
/// rather than place them on a guess — so the page is copied across, exactly as Ostara does
/// (`oexportunderlay.cpp:64`), instead of being asked to come out the same twice.
auto AdoptPlanPage(const QString &path, const QString &planPath, QString *error) -> bool
{
    const auto read = [](const QString &file, QDomDocument &document) {
        QFile input(file);
        if (!input.open(QIODevice::ReadOnly))
        {
            return false;
        }
        const QDomDocument::ParseResult result = document.setContent(&input);
        input.close();
        return static_cast<bool>(result);
    };

    QDomDocument plan;
    QDomDocument underlay;
    if (!read(planPath, plan) || !read(path, underlay))
    {
        *error = QCoreApplication::translate("PrintPdf", "The sheet's marks could not be placed on "
                                                         "the plan's page.");
        return false;
    }

    const QDomElement planRoot = plan.documentElement();
    QDomElement root = underlay.documentElement();
    for (const char *name : {"width", "height", "viewBox"})
    {
        const QString attribute = QString::fromLatin1(name);
        root.setAttribute(attribute, planRoot.attribute(attribute));
    }

    QFile output(path);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        *error = QCoreApplication::translate("PrintPdf", "The sheet's marks could not be written.");
        return false;
    }

    const QByteArray bytes = underlay.toByteArray(-1);
    const bool whole = output.write(bytes) == bytes.size();
    output.close();
    return whole;
}

//---------------------------------------------------------------------------------------------------------------------
/// The cover page as its own document, laid out in the PLAN's coordinates.
///
/// **A cover page is a page, and the plan is what its own position is measured against.** The
/// payload carries it as a document of its own — 19 × 25.9 cm with its own viewBox — which is the
/// right thing to draw and the wrong thing to place: handed to the writer as it stands, its
/// coordinates would be read as the plan's, and a page meant for the corner of a sheet would land in
/// the middle of the drawing. So its content is re-laid into a document whose viewBox IS the
/// rectangle the page stands on, exactly as Ostara writes it (`oexportcoverpagesvg.cpp:44`), and the
/// writer's own placement correction then puts it back where the canvas showed it.
///
/// @param page the page's own rectangle, in the plan's pixels. Refused when empty: a page with
///             nowhere to stand is not a page, and writing it would put an empty sheet in the file.
auto WriteCover(const PrintPdf::Job &job, const QString &path, QString *error) -> bool
{
    const QRectF page = job.coverPage;
    if (page.width() <= 0.0 || page.height() <= 0.0)
    {
        *error = QCoreApplication::translate("PrintPdf", "The cover page has nowhere to stand.");
        return false;
    }

    QDomDocument source;
    if (!source.setContent(job.coverSvg))
    {
        *error = QCoreApplication::translate("PrintPdf", "The cover page could not be read.");
        return false;
    }

    const QDomElement sourceRoot = source.documentElement();
    const QStringList viewBox =
        sourceRoot.attribute(QStringLiteral("viewBox")).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    const QRectF box = viewBox.size() == 4 ? QRectF(viewBox.at(0).toDouble(), viewBox.at(1).toDouble(),
                                                    viewBox.at(2).toDouble(), viewBox.at(3).toDouble())
                                           : QRectF();

    // The page's own size said twice, as every document in the file says it: millimetres on the
    // root, print pixels in the viewBox.
    QDomDocument document;
    QDomElement root = document.createElement(QStringLiteral("svg"));
    root.setAttribute(QStringLiteral("xmlns"), QStringLiteral("http://www.w3.org/2000/svg"));
    root.setAttribute(QStringLiteral("xmlns:ostara"), QStringLiteral("urn:ostara"));
    root.setAttribute(QStringLiteral("width"),
                      QStringLiteral("%1mm").arg(PrintMetrics::PxToMm(page.width()), 0, 'f', 4));
    root.setAttribute(QStringLiteral("height"),
                      QStringLiteral("%1mm").arg(PrintMetrics::PxToMm(page.height()), 0, 'f', 4));
    root.setAttribute(QStringLiteral("viewBox"),
                      QStringLiteral("%1 %2 %3 %4")
                          .arg(Number(page.left()), Number(page.top()), Number(page.width()),
                               Number(page.height())));
    document.appendChild(root);

    QDomElement group = document.createElement(QStringLiteral("g"));
    if (!box.isEmpty())
    {
        group.setAttribute(QStringLiteral("transform"),
                            QStringLiteral("translate(%1,%2) scale(%3,%4) translate(%5,%6)")
                                .arg(Number(page.left()), Number(page.top()),
                                     Number(page.width() / box.width()),
                                     Number(page.height() / box.height()), Number(-box.left()),
                                     Number(-box.top())));
    }
    root.appendChild(group);

    for (QDomElement child = sourceRoot.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        group.appendChild(document.importNode(child, true));
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        *error = QCoreApplication::translate("PrintPdf", "The cover page could not be written.");
        return false;
    }

    const QByteArray bytes = document.toByteArray(-1);
    const bool whole = file.write(bytes) == bytes.size();
    file.close();
    return whole;
}

//---------------------------------------------------------------------------------------------------------------------
/// The documents a run is drawn from, on disk and ready to be read.
///
/// Three, as Ostara sends them: the plan with its layers, the paper's own marks, and the cover page.
/// A file writes them and a printer renders them, and both come through here — which is what keeps a
/// printed sheet and an exported one the same drawing.
struct Documents
{
    QString plan;   ///< the plan, its layers and the payload it carries
    QString marks;  ///< the sheet's frame, numbering and registration marks; empty when it has none
    QString cover;  ///< the cover page, laid out in the plan's coordinates; empty when there is none
};

auto Prepare(const PrintPdf::Job &job, QTemporaryDir &directory, Documents &documents,
             bool withPayload, QString *error) -> bool
{
    documents.plan = directory.filePath(QStringLiteral("plan.svg"));
    if (!WritePlan(job, documents.plan, withPayload, error))
    {
        return false;
    }

    if (job.plan.grid.valid)
    {
        documents.marks = directory.filePath(QStringLiteral("marks.svg"));
        if (!WriteMarks(job, documents.marks, error)
            || !AdoptPlanPage(documents.marks, documents.plan, error))
        {
            return false;
        }
    }

    if (!job.coverSvg.isEmpty() && !job.coverPage.isEmpty())
    {
        documents.cover = directory.filePath(QStringLiteral("cover.svg"));
        if (!WriteCover(job, documents.cover, error))
        {
            return false;
        }
    }

    return true;
}
} // namespace

//---------------------------------------------------------------------------------------------------------------------
auto PrintPdf::Pages(const PrintPlan &plan, const QRectF &coverPaper, QRectF *coverSheet)
    -> QVector<QRectF>
{
    QVector<QRectF> pages;

    if (plan.grid.valid)
    {
        // Row by row, left to right — the order the sheets come off the printer, and the order the
        // numbers on them were given in. A cell with nothing on it is not a page.
        for (int row = 0; row < plan.grid.rows; ++row)
        {
            for (int column = 0; column < plan.grid.columns; ++column)
            {
                if (!plan.sheets.IsPresent(row, column))
                {
                    continue;
                }

                pages.append(QRectF(plan.grid.planRect.left() + column * plan.grid.cellWidthPx,
                                    plan.grid.planRect.top() + row * plan.grid.cellHeightPx,
                                    plan.grid.cellWidthPx, plan.grid.cellHeightPx));
            }
        }
    }
    else
    {
        // A roll and the A0 sheet are cut into nothing: the plan is one piece of paper.
        pages.append(QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx));
    }

    if (coverSheet != nullptr)
    {
        *coverSheet = coverPaper;
    }

    return pages;
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPdf::Write(const Job &job, const QString &path, QString *error) -> bool
{
    QString reason;

    // **A run that did not come out is not delivered.** The plan carries its own refusal, and writing
    // an empty file under the name of a real output would be the sort of answer that looks like one.
    if (!job.plan.ok)
    {
        *error = job.plan.error;
        return false;
    }

    // Every document the writer reads is a file: the module takes paths, never buffers. A directory
    // of its own, removed when it goes out of scope, so nothing is left beside her export.
    QTemporaryDir directory;
    if (directory.isValid())
    {
        Documents documents;
        if (Prepare(job, directory, documents, /*withPayload=*/true, &reason))
        {
            QRectF coverSheet;
            const QVector<QRectF> pages = Pages(job.plan, job.coverPaper, &coverSheet);
            if (!pages.isEmpty())
            {
                SvgToOcgPdf converter;

                SvgToOcgPdf::DocumentInfo info;
                info.title = job.title;
                info.author = QStringLiteral("Rabnest");
                info.creator = info.author;

                // **The plan's own document IS the payload**, as it is in Ostara: one file, written
                // once, that the reader renders and that Rabnest can reopen. A second copy of the
                // same drawing would be twice the bytes and two things to keep in agreement.
                if (converter.convert(documents.plan, pages, path, info, documents.marks,
                                      job.projector || LayoutIsALayer(job), documents.cover,
                                      coverSheet, documents.plan))
                {
                    return true;
                }

                *error = QCoreApplication::translate("PrintPdf", "The PDF could not be written: %1")
                             .arg(QFileInfo(path).fileName());
                return false;
            }

            *error = QCoreApplication::translate("PrintPdf", "The run has no sheet to print.");
            return false;
        }
    }
    else
    {
        *error = QCoreApplication::translate("PrintPdf", "A temporary folder could not be made, so "
                                                         "the file cannot be assembled.");
    }

    if (reason.isEmpty())
    {
        reason = QCoreApplication::translate("PrintPdf", "The run could not be assembled.");
    }
    *error = reason;
    return false;
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPdf::WriteSvg(const Job &job, const QString &path, QString *error) -> bool
{
    if (!job.plan.ok)
    {
        *error = job.plan.error;
        return false;
    }

    return WritePlan(job, path, /*withPayload=*/false, error);
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPdf::AllPages(const PrintPlan &plan, const QRectF &coverPaper) -> QVector<QRectF>
{
    QVector<QRectF> pages;

    // **The cover page's own sheet comes first**, as it does in the file: it is the page the run
    // opens on, so it is the sheet that comes off the printer first.
    if (!coverPaper.isEmpty())
    {
        pages.append(coverPaper);
    }

    pages.append(Pages(plan, QRectF()));
    return pages;
}

//---------------------------------------------------------------------------------------------------------------------
auto PrintPdf::Paint(QPrinter &printer, const Job &job, QString *error) -> bool
{
    QString reason;

    if (!job.plan.ok)
    {
        *error = job.plan.error;
        return false;
    }

    QTemporaryDir directory;
    if (!directory.isValid())
    {
        *error = QCoreApplication::translate("PrintPdf", "A temporary folder could not be made, so "
                                                         "the run cannot be printed.");
        return false;
    }

    Documents documents;
    if (!Prepare(job, directory, documents, /*withPayload=*/false, &reason))
    {
        *error = reason;
        return false;
    }

    const QVector<QRectF> pages = AllPages(job.plan, job.coverPaper);
    if (pages.isEmpty())
    {
        *error = QCoreApplication::translate("PrintPdf", "The run has no sheet to print.");
        return false;
    }

    // The very documents the file is written from, rendered instead of referenced: a printed sheet
    // holds the file's drawing, and not a second one drawn for paper.
    QSvgRenderer plan(documents.plan);
    QSvgRenderer marks(documents.marks);
    QSvgRenderer cover(documents.cover);

    if (!plan.isValid())
    {
        *error = QCoreApplication::translate("PrintPdf", "The run's drawing could not be read.");
        return false;
    }

    const QRectF planRect = PlanRect(job.plan);

    // **A plan pixel is a ninety-sixth of an inch**, and the paper is measured in millimetres: this
    // is the whole of the « 100 % » rule, and there is no other.
    const double devicePerMm = printer.resolution() / 25.4;
    const double scale = devicePerMm * 25.4 / PrintMetrics::kPrintDpi;

    QPainter painter;
    if (!painter.begin(&printer))
    {
        *error = QCoreApplication::translate("PrintPdf", "The printer could not be opened.");
        return false;
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    for (int index = 0; index < pages.size(); ++index)
    {
        if (index > 0 && !printer.newPage())
        {
            painter.end();
            *error = QCoreApplication::translate("PrintPdf", "The printer stopped after page %1.")
                         .arg(index);
            return false;
        }

        const QRectF page = pages.at(index);

        // ⚠️ **The painter's origin is where the machine can start inking, not the paper's corner.**
        // With `fullPage` false, Qt lays the painter out on the printable area: in painter
        // coordinates that area is `(0, 0, pageRect.size())`, and the paper reaches out beyond it by
        // the driver's own margins. Both rectangles are centred on the paper, so their centres read
        // the same — and using either as if it were the paper is what shifted every sheet by the
        // margin.
        const QRectF printable = printer.pageRect(QPrinter::DevicePixel);
        const QRectF paper = printer.paperRect(QPrinter::DevicePixel).translated(-printable.topLeft());

        // **Centred on the PAPER.** The sheet is where the file says it is: a framed sheet is the
        // paper less ten millimetres all round, and that margin is drawn around the frame on purpose.
        // Centring on what the machine can ink would put the frame nearer one edge than the other by
        // the driver's own margins, and the sheet would sit askew on the paper it was made for. What
        // the machine cannot reach it crops, and the sheet's own margin is the room for it.
        const QSizeF size(page.width() * scale, page.height() * scale);
        const QPointF origin(paper.center().x() - size.width() / 2.0,
                             paper.center().y() - size.height() / 2.0);

        painter.save();

        // ⚠️ **The clip is the SHEET, and not the paper it lands on.** A framed run's sheet is
        // 190 × 259 mm and the paper it fits is A4 or Letter: clipping to the printable area would
        // leave a border of paper around the frame in which the NEIGHBOURING sheets' drawing shows —
        // a plan carrying the pieces of the sheets beside it, which is exactly what a page of a
        // tiled run must not do. Each page of the file is its sheet and nothing else; a printed page
        // is the same page, so it is cut the same way.
        painter.setClipRect(QRectF(origin, size), Qt::IntersectClip);
        painter.translate(origin);
        painter.scale(scale, scale);
        painter.translate(-page.topLeft());

        // **A cover page's sheet holds the cover page.** Nothing of the plan is invoked there, and
        // neither is the paper's frame: the sheet sits above row one, so the plan's own top edge runs
        // along its bottom and would print half a line of a sheet that is not on it.
        const bool coverOnly = !job.coverPaper.isEmpty() && page == job.coverPaper;

        if (!coverOnly && marks.isValid())
        {
            marks.render(&painter, planRect);
        }
        if (cover.isValid())
        {
            cover.render(&painter, job.coverPage);
        }
        if (!coverOnly)
        {
            plan.render(&painter, planRect);
        }

        painter.restore();
    }

    painter.end();
    return true;
}
