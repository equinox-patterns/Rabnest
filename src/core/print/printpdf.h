/************************************************************************
 **
 **  @file   printpdf.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   24/10/2026
 **
 **  @brief  The print run as a file: one PDF, one layer per size, one page per printed sheet.
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

#ifndef PRINTPDF_H
#define PRINTPDF_H

#include "core/print/exporttemplate.h"
#include "core/print/printplan.h"

#include "io/GradedOverlays.h"  // Trame — les calques d'une projection

#include <QByteArray>
#include <QRectF>
#include <QString>
#include <QVector>

class QPrinter;

/// The print run delivered as a PDF — Ostara's `OExportPdf::Write`, on Rabnest's side of the wall.
///
/// **A print is not a picture of a print.** Ostara delivers it as drawings stacked in layers — one
/// per size, so a printer can switch a size off — cut into the sheets the format asks for, with the
/// paper's own frame and marks under them and the cover page over them. This is that file.
///
/// Everything the file needs is already in the run: `PrintPlan` holds the placed geometry, the
/// per-size ink and its style, and the decor of the reference size; the paper's marks come from the
/// template the payload carried; the sheets from the run's own tiling. Nothing is measured again.
///
/// **What is written is what the canvas shows** — the same geometry, by the same rule, because the
/// two would otherwise be two drawings. What differs is the stroke: the canvas has an on-screen floor
/// so a plan zoomed out stays readable, and a file printed on paper has no such problem, so it draws
/// with the weight the pattern itself states.
///
/// Three documents go out, exactly as Ostara sends them:
/// - the plan, one `<g>` per size, plus the layout layer when the run has one;
/// - the paper, a second document drawn on every page *under* the layers and outside every optional
///   content group — a frame that can be switched off is a sheet that can be assembled wrongly;
/// - the cover page, a third one, outside every group for the same reason.
namespace PrintPdf
{
/// One delivery, as the file sees it.
struct Job
{
    /// The payload the source PDF carried. It is re-emitted into the file — with the run's own
    /// recipe — because a PDF that nothing can reopen is a delivery nobody can ask anything more of.
    QByteArray payload;

    PrintPlan plan;

    /// The template the plan was laid with: the frame, the assembly marks, the numbering.
    ExportTemplate decor;
    bool framed = false;

    /// The cover page, as the source carried it, and the two rectangles its format gives it.
    QByteArray coverSvg;
    QRectF coverPaper;
    QRectF coverPage;

    /// The PDF this run was read from — the file it is a print of.
    ///
    /// **The only record of whether the layout is one of the reader's layers.** One of Ostara's two
    /// export boxes says so, and it is persisted nowhere: not in the pattern, not in the settings.
    /// The file it produced is the whole of what remains of the answer, so the answer is read there
    /// — the layout is put back as a layer exactly when it was one.
    ///
    /// A plan whose information block can be switched off is a plan that can be printed without it
    /// and look perfectly normal, which is why this is never decided here: a file that says nothing,
    /// or that cannot be read, gets the decor drawn where it stands.
    QString sourcePath;

    /// What the reader shows in its information panel — this delivery's name.
    QString title;

    /// **Une projection n'est pas une impression, et le fichier doit le dire.**
    ///
    /// Trois choses en découlent : le document est TOUJOURS calqué (les trois calques du dessous sont
    /// trois choses que l'utilisatrice doit pouvoir éteindre), ses pièces sont écrites au trait du
    /// format — noir, plein, 7 pt — et sa recette porte `destination="projector"`, sans quoi le
    /// fichier se relirait comme une impression.
    bool projector = false;

    /// Les calques d'une projection : le fond et les grilles, écrits SOUS les pièces. Vides pour
    /// une impression, qui n'a rien à mettre dessous.
    GradedOverlays::Set overlays;
};

/// The rectangles the PDF's pages are, in the plan's own pixels.
///
/// **A page is a sheet, and only a sheet with something on it.** The tiling is read from the run, so
/// a sheet a hand emptied is not written, and one it filled is. A roll and the A0 sheet are cut into
/// nothing: their single page is the plan itself.
///
/// @param coverSheet  set to the rectangle the cover page's own sheet occupies, when its format gives
///                    it one. Empty otherwise, which leaves the page on the plan's own sheets.
auto Pages(const PrintPlan &plan, const QRectF &coverPaper, QRectF *coverSheet = nullptr)
    -> QVector<QRectF>;

/// Every page the run prints, in the order it comes off the printer — the cover page's own sheet
/// first when its format gives it one.
///
/// The same list the file is written from, so a printed run and an exported one have the same pages
/// in the same order.
auto AllPages(const PrintPlan &plan, const QRectF &coverPaper) -> QVector<QRectF>;

/// Writes @p job to @p path.
///
/// @param error why the file could not be written, when it could not — one sentence for the user.
auto Write(const Job &job, const QString &path, QString *error) -> bool;

/// Writes the plan's own document to @p path, as SVG, without the payload and without the paper.
///
/// **The bench's own door onto the print.** The PDF is made from the plan's document; writing that
/// document out as it stands is how the drawing can be *measured* — a `<g>` per piece, its
/// transform, its paths — instead of being looked at. What the file holds is what the PDF's first
/// document holds, minus the payload: the payload is embedded so the file can be reopened, and a
/// bench that reads geometry has no use for it.
///
/// @param error why the file could not be written, when it could not.
auto WriteSvg(const Job &job, const QString &path, QString *error) -> bool;

/// Prints @p job: one printer page per sheet, at 100 %, in the file's own drawing.
///
/// **What prints is what the file holds.** The pages are painted from the very documents the PDF is
/// written from — the paper's marks, the cover page, the sizes over them — so a printed sheet and an
/// exported one cannot come apart, and no screen rule (a floor under the stroke, a dashed line drawn
/// solid) can reach the paper.
///
/// **At 100 %, and nothing else.** A sheet of a tiled run is glued to the one beside it: printed at
/// 95 % it is a plan whose seams do not meet, and nothing on the paper says so. The page is centred
/// on what the printer can actually ink, which is the paper's own size for a run whose sheet is that
/// paper.
///
/// The printer must already be configured by the caller: its page size, its orientation, its
/// `fullPage`.
auto Paint(QPrinter &printer, const Job &job, QString *error) -> bool;
} // namespace PrintPdf

#endif // PRINTPDF_H
