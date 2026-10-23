/************************************************************************
 **
 **  @file   workspacepage.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  The canvas the cutting plan is drawn on.
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

#include "app/workspacepage.h"

#include "app/print/printitems.h"
#include "core/print/exportcoverpage.h"
#include "core/print/printmetrics.h"
#include "core/print/printplan.h"

#include <QGraphicsPathItem>
#include <QGraphicsPolygonItem>
#include <QPainterPath>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSvgItem>
#include <QSvgRenderer>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPolygonF>
#include <QPushButton>
#include <QTransform>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kCmPerPx = 2.54 / 96.0;
constexpr double kScale = 1000000.0;
constexpr double kMmPerPx = 25.4 / 96.0;

/// The width of a piece's line, in scene pixels — the thickness it has at 100 %, and the thickness it
/// keeps when the view is enlarged. The floor is the SCREEN's; `WorkspacePage::kLineFloorPx` states it
/// once, because a page passes its own.
constexpr double kLinePx = WorkspacePage::kLineFloorPx;

//---------------------------------------------------------------------------------------------------------------------
/// The pen a piece is drawn with, **at the zoom the view is at.**
///
/// ⚠️ **A cosmetic pen is one DEVICE pixel whatever the zoom, and that is the defect it looks like a
/// fix for.** Zooming in enlarges the piece and leaves its line a single pixel wide, so the outline
/// gets thinner and thinner while the cartouche it carries — an SVG, which scales — keeps growing; a
/// pattern at 400 % reads as a hairline drawing in a thick frame. The line therefore follows the zoom,
/// as ink on paper would.
///
/// ⚠️ **With ONE floor, and the floor is a LENGTH on the paper.** A plan fitted to the window is
/// drawn at about a fifth of its size, and a line of a fifth of a pixel is not a thin line, it is no
/// line at all. What is drawn is therefore either the pattern's own thickness or the floor, never
/// less.
///
/// ⚠️ **And the floor is the CALLER's, because a screen and a printer do not measure alike.** A
/// device pixel is 0.26 mm on a screen at 96 dpi and 0.04 mm on a printer at 600 — a floor written in
/// pixels that keeps a plan visible on screen prints it as a ghost. A page passes a length instead.
///
/// ⚠️ **In SCENE units, never cosmetic.** A cosmetic pen's width is in device pixels, which the item
/// knows nothing of: `QGraphicsPathItem::boundingRect()` counts the stroke from the pen's width, so a
/// cosmetic pen leaves the stroke outside the area the scene repaints — trails behind a zoomed stroke.
/// The width is kept in scene units and refreshed whenever the zoom moves, which is what makes the
/// bounding rect — and the dashes, which count in pen widths — right on their own.
auto PatternPen(const QPen &base, double viewScale, double floor) -> QPen
{
    QPen pen = base;
    pen.setCosmetic(false);
    pen.setWidthF(viewScale > 0.0 ? std::max(1.0 / viewScale, floor) : floor);
    return pen;
}

//---------------------------------------------------------------------------------------------------------------------
/// The piece's own drawing as a document of its own, sized to its own box — so one unit is one pixel
/// and the placement of the piece can be applied to it as a transform, without a scaling of its own.
auto SvgDocument(const PieceDecor &decor, bool oneInk) -> QByteArray
{
    const QRectF box = decor.box;

    QString svg = QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" "
                                 "xmlns:i=\"http://ns.adobe.com/AdobeIllustrator/10.0/\" "
                                 "xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\" "
                                 "xmlns:ostara=\"urn:ostara\" "
                                 "width=\"%1\" height=\"%2\" viewBox=\"%3 %4 %1 %2\">")
                      .arg(QString::number(box.width(), 'f', 3),
                           QString::number(box.height(), 'f', 3),
                           QString::number(box.left(), 'f', 3),
                           QString::number(box.top(), 'f', 3));

    // ⚠️ **Une seule encre.** Le décor est recopié du dessin, et le dessin d'une taille porte la
    // couleur et les pointillés du STYLE de cette taille : un chemin interne y arrive en magenta
    // pointillé, le droit-fil en rouge. Sur une pièce qu'on coupe, ça donne un contour noir et des
    // traits de couleur — deux dessins superposés au lieu d'un. Le plan de coupe dessine donc la
    // pièce d'une seule encre, la sienne : on force les traits au noir, et l'épaisseur et les
    // pointillés du décor tombent avec eux.
    QString ink = decor.content;
    if (oneInk)
    {
        static const QRegularExpression colour(QStringLiteral("stroke=\"#[0-9A-Fa-f]{3,6}\""));
        static const QRegularExpression dashes(QStringLiteral("\\s*stroke-dasharray=\"[^\"]*\""));
        ink.replace(colour, QStringLiteral("stroke=\"#000000\""));
        ink.remove(dashes);
    }

    svg += ink;
    svg += QStringLiteral("</svg>");

    return svg.toUtf8();
}

//---------------------------------------------------------------------------------------------------------------------
/// A piece's line, whose thickness follows the ZOOM — see @ref PatternPen.
///
/// It keeps the pen it was written with and re-lays only its WIDTH: the pattern is the payload's (a
/// dash pattern counts in pen widths, so it scales with the line), the thickness is the view's.
class PatternPathItem : public QGraphicsPathItem
{
public:
    PatternPathItem(const QPainterPath &shape, const QPen &pen)
      : QGraphicsPathItem(shape), m_pen(pen)
    {
        QGraphicsPathItem::setBrush(Qt::NoBrush);
        QGraphicsPathItem::setPen(pen);
    }

    /// The stroke, sized for @p viewScale — the scene unit in device pixels.
    ///
    /// ⚠️ **Only when the width actually changes.** Setting a pen tears down the item's paint cache and
    /// asks for a repaint, and this is called on every step of the zoom over every piece of the plan.
    virtual void SetViewScale(double viewScale, double floor = WorkspacePage::kLineFloorPx)
    {
        const QPen next = PatternPen(m_pen, viewScale, floor);
        if (std::abs(pen().widthF() - next.widthF()) < 1e-6)
        {
            return;
        }
        QGraphicsPathItem::setPen(next);
    }

private:
    QPen m_pen;
};

//---------------------------------------------------------------------------------------------------------------------
/// The guide that says where the cloth is folded. **Big dashes, at half black, whatever the zoom.**
///
/// ⚠️ **A fold zone is not a piece and must not look like one.** It says where the cloth was folded
/// before cutting — one rectangle, or three when the cloth was folded from both selvedges and the
/// two bands sit above a column. Drawn as a light hairline it was invisible: a cosmetic pen is one
/// DEVICE pixel, a twentieth of a millimetre on a sheet at 1200 dpi, in a grey lighter than the
/// paper. She could not see it, so nobody could read the plan.
///
/// ⚠️ **The dashes are counted in SCENE units, never in pen widths.** Qt's own dash patterns are
/// multiples of the stroke, so a guide whose stroke follows the zoom would have dashes that shrink
/// with it — and a plan printed at 1:5,6 has a stroke four times thinner than on screen. Here the
/// dash measures the same slice of the DRAWING at every scale, which is what makes it readable both
/// on screen and on paper.
class FoldGuideItem : public PatternPathItem
{
public:
    /// @param pattern the dash and gap lengths, in scene pixels, in Qt's own order (on, off, …)
    FoldGuideItem(const QPainterPath &shape, const QPen &pen, QVector<qreal> pattern)
      : PatternPathItem(shape, pen), m_scenePattern(std::move(pattern))
    {
    }

    void SetViewScale(double viewScale, double floor = WorkspacePage::kLineFloorPx) override
    {
        PatternPathItem::SetViewScale(viewScale, floor);

        // Qt counts a dash in widths of the pen it is drawn with, so the figure has to be divided by
        // the width the pen actually got — one scene pixel at 100 %, five when the plan is fitted.
        const double width = QGraphicsPathItem::pen().widthF();
        if (width <= 0.0 || m_scenePattern.isEmpty())
        {
            return;
        }

        QVector<qreal> pattern;
        pattern.reserve(m_scenePattern.size());
        for (const qreal length : m_scenePattern)
        {
            pattern.append(length / width);
        }

        QPen pen = QGraphicsPathItem::pen();
        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(pattern);
        QGraphicsPathItem::setPen(pen);
    }

private:
    QVector<qreal> m_scenePattern;
};

//---------------------------------------------------------------------------------------------------------------------
auto Cm(double millimetres) -> QString
{
    return QObject::tr("%1 cm").arg(millimetres / 10.0, 0, 'f', 1);
}

//---------------------------------------------------------------------------------------------------------------------
/// The nearest sixteenth of an inch, as a mixed number — the way cloth is sold where it is sold in
/// inches, and the figure a reader there checks against her own tape.
auto Inch(double millimetres) -> QString
{
    long sixteenths = std::lround(millimetres / 25.4 * 16.0);

    const long whole = sixteenths / 16;
    long numerator = sixteenths % 16;
    long denominator = 16;

    while (numerator != 0 && numerator % 2 == 0)
    {
        numerator /= 2;
        denominator /= 2;
    }

    if (numerator == 0)
    {
        return QObject::tr("%1\"").arg(whole);
    }
    if (whole == 0)
    {
        return QObject::tr("%1/%2\"").arg(numerator).arg(denominator);
    }
    return QObject::tr("%1 %2/%3\"").arg(whole).arg(numerator).arg(denominator);
}

//---------------------------------------------------------------------------------------------------------------------
/// Yards rounded UP to the eighth: a length of cloth is bought, never measured to the exact inch.
auto Yards(double millimetres) -> QString
{
    long eighths = static_cast<long>(std::ceil(millimetres / 914.4 * 8.0 - 1e-9));

    const long whole = eighths / 8;
    long numerator = eighths % 8;
    long denominator = 8;

    while (numerator != 0 && numerator % 2 == 0)
    {
        numerator /= 2;
        denominator /= 2;
    }

    if (numerator == 0)
    {
        return QObject::tr("%1 yd").arg(whole);
    }
    if (whole == 0)
    {
        return QObject::tr("%1/%2 yd").arg(numerator).arg(denominator);
    }
    return QObject::tr("%1 %2/%3 yd").arg(whole).arg(numerator).arg(denominator);
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
WorkspacePage::WorkspacePage(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *right = layout;

    m_scene = new QGraphicsScene(this);
    m_view = new QGraphicsView(m_scene, this);
    m_view->setObjectName(QStringLiteral("canvas"));
    m_view->setRenderHint(QPainter::Antialiasing, true);
    m_view->setDragMode(QGraphicsView::ScrollHandDrag);
    m_view->setBackgroundBrush(QColor(0xEF, 0xEF, 0xEF));
    right->addWidget(m_view, 1);

    auto *panel = new QWidget(this);
    panel->setObjectName(QStringLiteral("outputsPanel"));
    auto *panelLayout = new QHBoxLayout(panel);
    panelLayout->setContentsMargins(16, 10, 16, 10);
    panelLayout->setSpacing(16);

    m_detail = new QLabel(panel);
    m_detail->setTextInteractionFlags(Qt::TextSelectableByMouse);
    panelLayout->addWidget(m_detail, 1);

    right->addWidget(panel);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::Show(const CutPlan &plan)
{
    Draw(plan);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::Clear()
{
    m_scene->clear();
    m_detail->clear();
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::ZoomIn()
{
    ZoomBy(1.25);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::ZoomOut()
{
    ZoomBy(1.0 / 1.25);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::ZoomBy(double factor)
{
    if (m_scene == nullptr || m_scene->items().isEmpty())
    {
        return;
    }

    // **Le centre de la vue reste le centre de la vue.** A step of zoom that also moved the drawing
    // would be a pan disguised as a zoom: the point looked at before the gesture has to be the point
    // looked at after it.
    const QPoint viewportCentre = m_view->viewport()->rect().center();
    const QPointF before = m_view->mapToScene(viewportCentre);

    const double scale = m_view->transform().m11();
    constexpr double kLargest = 50.0;
    constexpr double kSmallest = 0.002;
    if ((factor > 1.0 && scale >= kLargest) || (factor < 1.0 && scale <= kSmallest))
    {
        return;
    }

    m_view->scale(factor, factor);

    const QPointF after = m_view->mapToScene(viewportCentre);
    const QPointF delta = after - before;
    m_view->translate(delta.x(), delta.y());

    RefreshLineWidth();
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::RefreshLineWidth()
{
    if (m_view == nullptr || m_scene == nullptr)
    {
        return;
    }

    // The scene unit in device pixels. `m11`/`m12` and not `m11` alone, so a view turned by half a
    // circle is not read as a scale of zero.
    const QTransform transform = m_view->transform();
    RefreshLineWidth(std::hypot(transform.m11(), transform.m12()), WorkspacePage::kLineFloorPx);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::RefreshLineWidth(double scale, double floor)
{
    if (m_scene == nullptr)
    {
        return;
    }

    for (QGraphicsItem *item : m_scene->items())
    {
        if (auto *path = dynamic_cast<PatternPathItem *>(item); path != nullptr)
        {
            path->SetViewScale(scale, floor);
        }
    }
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::RenderPlan(QPainter *painter, const QRectF &sourcePx, const QRectF &targetPx,
                              double floorPx)
{
    if (painter == nullptr || m_scene == nullptr || sourcePx.isEmpty() || targetPx.isEmpty())
    {
        return;
    }

    // How many device pixels one scene pixel becomes in the file — the scale the strokes must be
    // sized for.
    const double scale = targetPx.width() / sourcePx.width();

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setFont(m_scene->font());

    RefreshLineWidth(scale, floorPx);
    m_scene->render(painter, targetPx, sourcePx, Qt::IgnoreAspectRatio);
    RefreshLineWidth();
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::Draw(const CutPlan &plan)
{
    m_printMode = false;
    m_scene->clear();

    if (!plan.ok)
    {
        m_detail->setText(plan.error);
        return;
    }

    // The cloth: what the pieces were laid into, at the width she paid for and the length the plan
    // actually needs.
    QPen border(QColor(0xD6, 0xD6, 0xD6));
    border.setCosmetic(true);
    // ⚠️ **Le fond est le TISSU, et il est coloré — d'un bout à l'autre.** Il était blanc avec des
    // pièces beiges par-dessus : deux fonds, une bande blanche autour du plan et des pièces qui se
    // découpent en clair sur du clair. Le tissu prend donc la couleur, et les pièces ne sont plus
    // remplies du tout : ce qu'on voit d'une pièce, c'est son trait.
    const QBrush clothBrush(QColor(0xF7, 0xF5, 0xF2));
    m_scene->addRect(QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx), border, clothBrush);

    // ── Le pli ──────────────────────────────────────────────────────────────────────────────────
    //
    // The one thing a folded plan has to show, because a piece glued to that edge is cut ON the fold
    // and not through it. **Black at half, in big dashes**, and drawn by an item that follows the
    // zoom: a light hairline said nothing on screen and nothing at all on paper — see FoldGuideItem.
    //
    // The dash lengths are the DRAWING's, in scene pixels: a plan of 6047 pixels is 1,6 m of cloth, so
    // 56 px is a centimetre and a half of real fabric — about ten screen pixels when the plan is
    // fitted to the window, and two and a half millimetres once it is shrunk onto a sheet.
    const QPen foldZonePen(QColor(0, 0, 0, 128));
    for (const FoldZone &zone : plan.zones)
    {
        QPainterPath outline;
        outline.addRect(zone.rect);
        m_scene->addItem(new FoldGuideItem(outline, foldZonePen, {56.0, 32.0}));

        const double x = zone.foldOnTheLeft ? zone.rect.left() : zone.rect.right();
        QPainterPath axis;
        axis.moveTo(x, zone.rect.top());
        axis.lineTo(x, zone.rect.bottom());
        m_scene->addItem(new FoldGuideItem(axis, foldZonePen, {32.0, 16.0, 2.0, 16.0}));
    }

    // Its WIDTH is not set here: the zoom decides it, and `RefreshLineWidth` writes it after the
    // view has been fitted. Setting it once would be setting it for one zoom only.
    QPen piecePen(QColor(0x2B, 0x2B, 0x2B));

    const int count =
        std::min(static_cast<int>(plan.placements.size()), static_cast<int>(plan.footprint.polygons.size()));
    for (int index = 0; index < count; ++index)
    {
        const QPointF origin = index < plan.origins.size() ? plan.origins.at(index) : QPointF();
        const Placement &placement = plan.placements.at(static_cast<size_t>(index));

        // ⚠️ **La pièce, c'est son ENCRE — pas l'empreinte du moteur.** L'empreinte est l'union des
        // contours, faite pour nidifier : elle n'a ni crans, ni ligne de couture, ni chemin interne.
        // Ce qu'on dessine est la pièce telle qu'Ostara la dessine, et son contenu vient avec elle —
        // le contour, ses crans, ses chemins internes, ses repères et son cartouche sont UN objet.
        // L'empreinte ne sert plus que de repli, pour une pose dont le payload ne portait pas l'encre.
        const CutPiece &pose = plan.poses.at(index);
        QPainterPath shape;
        if (!pose.ink.isEmpty())
        {
            for (const QPolygonF &polygon : pose.ink)
            {
                QPolygonF placed;
                placed.reserve(polygon.size());
                for (const QPointF &point : polygon)
                {
                    placed.append(PlacedPoint(point, placement, origin));
                }
                shape.addPolygon(placed);
            }
        }
        else
        {
            shape.addPolygon(PlacedOutline(
                plan.footprint.polygons.at(static_cast<size_t>(index)), placement, origin));
        }

        m_scene->addItem(new PatternPathItem(shape, piecePen));

        // ── Le décor de la pièce ────────────────────────────────────────────────────────────────
        //
        // **Its real drawing, not a pair of points.** Ostara writes the cartouche, the annotations,
        // the grain line with its arrows and the fold mark in the layer this file used to throw away;
        // they are in the pattern's frame with one placement matrix above them, and that matrix is
        // dropped at read time. So all that is left is to lay them with the piece: the same
        // translation, the same rotation, and the offset of their own box.
        const PieceDecor decor = DecorFor(plan.decor, pose);
        if (!decor.content.isEmpty())
        {
            auto *renderer = new QSvgRenderer(SvgDocument(decor, /*oneInk=*/true), m_scene);
            if (renderer->isValid())
            {
                auto *item = new QGraphicsSvgItem();
                item->setSharedRenderer(renderer);

                // ── Le placement du décor ─────────────────────────────────────────────────────
                //
                // ⚠️ **Aucun recentrage.** Le décor est écrit dans le repère de la pièce : la mesure
                // le montre, le trait de pli tombe sur l'axe du pli et le polygone commence au même
                // x. Il n'y a donc rien à recaler — seulement à le poser là où il a été dessiné, avec
                // exactement la transformation de la pièce :
                //
                //   scene(p) = R(θ) · (p − origin) + (x, y)
                //
                // et, l'item dessinant son viewBox à partir de (0,0), ses coordonnées locales sont
                // `p − box.topLeft`. D'où :
                //
                //   T(x, y) · R(θ) · T(box.topLeft − origin)
                //
                // Le recentrage sur un centre — de la boîte, puis de la pièce de référence — a été
                // essayé et retiré : il déplaçait le cartouche et le pli au lieu de les laisser où
                // ils sont, parce que la boîte du dessin est dominée par le trait du droit-fil.
                // ⚠️ **Nothing to shift and nothing to re-centre.** The payload writes the decor in
                // the piece's own frame — the same one the outline is written in — so the drawing and
                // the outline are placed by the SAME transform. The centre shift that used to be
                // computed here (the piece's centre at the current size minus its centre at the size
                // the decor was drawn for) existed only because the decor came from the smallest
                // size's layer; the payload now states a position for every size, and a position
                // that is stated is not re-derived.
                QTransform transform;
                transform.translate(placement.x, placement.y);
                transform.rotate(placement.rotation);
                transform.translate(decor.box.left() - origin.x(), decor.box.top() - origin.y());

                item->setTransform(transform);
                m_scene->addItem(item);
            }
        }
    }

    m_scene->setSceneRect(
        QRectF(-20.0, -20.0, plan.binWidthPx + 40.0, plan.usedHeightPx + 40.0));
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    RefreshLineWidth();

    // The same line Ostara writes under its cut plan: the cloth in the unit it is bought in, then the
    // same two figures in the units somebody else buys it in — inches for the width, yards for the
    // length, because a bolt is sold one way in one country and the other way in the next.
    const double widthMm = plan.fabricWidthCm * 10.0;
    const double lengthMm = plan.usedHeightPx * kMmPerPx;

    QString detail;
    if (plan.foldedFabric)
    {
        detail += tr("Canvas: half fabric width %1").arg(Cm(widthMm / 2.0)) + QStringLiteral(" · ");
    }

    detail += tr("Fabric width %1 · length %2").arg(Cm(widthMm), Cm(lengthMm));
    detail += QStringLiteral(" · ")
              + tr("Fabric width %1 · length %2").arg(Inch(widthMm), Yards(lengthMm));

    if (plan.seconds > 0.0)
    {
        detail += QStringLiteral(" · %1 s").arg(plan.seconds, 0, 'f', 1);
    }

    m_detail->setText(detail);
}

//---------------------------------------------------------------------------------------------------------------------
/// One piece of a print run, and the only thing of it a hand can take.
///
/// **It carries its placement and not its geometry**: the outline it was drawn with is the size's
/// own, and the frame it stands in belongs to the footprint the engine placed — the union of that
/// piece over the sizes being printed. Moving one size therefore moves every size of that piece,
/// which is what a graded plan means.
///
/// The move is applied on RELEASE, not while dragging: the sheets have to be read again when the
/// piece has landed, and reading them mid-gesture would rebuild the scene under the hand that is
/// still holding it.
class PrintPieceItem : public PatternPathItem
{
public:
    PrintPieceItem(const QPainterPath &shape, const QPen &pen, int placementIndex,
                   WorkspacePage *page)
      : PatternPathItem(shape, pen), m_placementIndex(placementIndex), m_page(page)
    {
        setFlag(QGraphicsItem::ItemIsMovable, true);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setCursor(Qt::OpenHandCursor);
    }

    /// **Grabbed by its SURFACE, never by its outline alone.** The item is drawn unfilled, and a
    /// path item's own shape follows the stroke it draws — so the piece could only be taken by the
    /// line, and a piece one has to click exactly on the line of is a piece one cannot move. The
    /// filled path is the same answer Ostara's own hit item gives.
    auto shape() const -> QPainterPath override { return path(); }

protected:
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override
    {
        QGraphicsPathItem::mouseReleaseEvent(event);

        // With ItemIsMovable the item is moved by its own POSITION, its path untouched — so the
        // gesture is read there, and put back to zero before the scene is drawn again.
        const QPointF delta = pos();
        if (delta.isNull())
        {
            return;
        }

        setPos(QPointF());
        m_page->MovePrintPiece(m_placementIndex, delta);
    }

private:
    int m_placementIndex{-1};
    WorkspacePage *m_page{nullptr};
};

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::ShowPrint(const PrintPlan &plan, const ExportTemplate &decor, bool framed,
                              const QByteArray &coverSvg, const QRectF &coverPaper,
                              const QRectF &coverPage)
{
    // What is shown is kept: a piece a hand moves changes the plan, and redrawing it must not mean
    // laying everything out again.
    m_printPlan = plan;
    m_printDecor = decor;
    m_printFramed = framed;
    m_coverSvg = coverSvg;
    m_coverPaper = coverPaper;
    m_coverPage = coverPage;
    m_printMode = true;

    RedrawPrint();
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::RedrawPrint()
{
    const PrintPlan &plan = m_printPlan;
    const ExportTemplate &decor = m_printDecor;
    const bool framed = m_printFramed;

    m_scene->clear();

    if (!plan.ok)
    {
        m_detail->setText(plan.error);
        return;
    }

    // ── Le papier ───────────────────────────────────────────────────────────────────────────────
    //
    // One white rectangle per PRINTED sheet, and none over a cell with nothing on it: a blank sheet
    // drawn white would be the canvas promising a page the file will never write.
    const QRectF planRect = plan.grid.valid ? plan.grid.planRect
                                            : QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx);

    if (plan.grid.valid)
    {
        MakeArtboardPaper(m_scene, plan.grid, plan.sheets);
    }
    else
    {
        // A roll and the A0 sheet: no sheets are cut, so the plan is one piece of paper.
        MakePaper(m_scene, planRect);
    }

    // The cover page's own sheet, for the formats that give it one. Paper only — the page is not a
    // cell, so it never enters ExportSheets and never takes a number.
    if (!m_coverPaper.isEmpty())
    {
        MakePaper(m_scene, m_coverPaper);
    }

    // ── Le décor ───────────────────────────────────────────────────────────────────────────────
    //
    // The tiling as the painter sees it: the grid's own cell size, and the sheets that carry a
    // piece. Nothing is decided here — the painter reads the same rules Ostara's canvas reads.
    if (plan.grid.valid)
    {
        ExportMarkGrid grid;
        grid.columns = plan.grid.columns;
        grid.rows = plan.grid.rows;
        grid.sheetWidthPx = plan.grid.cellWidthPx;
        grid.sheetHeightPx = plan.grid.cellHeightPx;
        grid.pxPerMm = PrintMetrics::MmToPx(1.0);
        grid.sheets = plan.sheets;

        m_scene->addItem(
            new AssemblyMarksItem(decor, plan.grid.planRect.topLeft(), grid, framed));
    }

    // ── La page de garde ───────────────────────────────────────────────────────────────────────
    //
    // At the size it will be printed, and never resized: the page is the payload's, at 19 × 25.9 cm,
    // wherever the format put it.
    if (!m_coverPage.isEmpty() && !m_coverSvg.isEmpty())
    {
        m_scene->addItem(new CoverPageItem(m_coverPage, m_coverSvg));
    }

    // ── Les pièces ─────────────────────────────────────────────────────────────────────────────
    //
    // **Each size draws its own form, at the placement its envelope took.** The engine placed one
    // footprint per piece — the union of that piece over the sizes being printed — so every size of
    // that piece stands where the envelope stands, and its own contour is mapped by the same frame:
    // the rotation about the envelope's own origin, and its translation.
    // **No fill.** A printed plan is a drawing of the pieces, not a plan of the cloth: Ostara's
    // print canvas paints the outlines and nothing inside them — the coloured fill belongs to the
    // cutting plan, where a piece is something to cut out.

    for (int layerIndex = 0; layerIndex < plan.layers.size(); ++layerIndex)
    {
        const PrintLayer &layer = plan.layers.at(layerIndex);

        // **The decor belongs to ONE size, and never to all of them.** Ostara's canvas draws it on the
        // reference layer — the smallest size of the range, the first the payload writes — and lets
        // every other size draw its own ink only; a cartouche, an annotation or a grain line
        // repeated once per size would be a drawing of the same words ten times over. The payload
        // carries a copy per size all the same: the copy is a lookup key, not a decision to draw ten
        // of them. The run's own order is largest first, so the flag comes from the payload.
        const bool reference = layer.reference;

        // The size's own line, as the payload drew it: its colour, and its dash pattern when it
        // has one. A Qt pattern counts in multiples of the pen width, so the pixels the SVG states
        // are divided by it — the same conversion Ostara's DashPen makes.
        // Its width follows the zoom — see `PatternPen`.
        QPen piecePen(layer.colour);
        if (!layer.dashes.isEmpty())
        {
            // The pen is one pixel wide, so the pattern's own pixel lengths are already the
            // multiples of the pen width Qt asks for.
            piecePen.setStyle(Qt::CustomDashLine);
            piecePen.setDashPattern(layer.dashes);
        }

        for (const PrintPiece &piece : layer.pieces)
        {
            if (piece.placementIndex < 0
                || piece.placementIndex >= static_cast<int>(plan.placements.size()))
            {
                continue;
            }

            const Placement &placement = plan.placements.at(static_cast<std::size_t>(piece.placementIndex));
            const QPointF origin = plan.origins.at(piece.placementIndex);

            // **The piece as it is DRAWN, and not the contour the engine unioned.** Its notches are
            // part of it — spliced into the contour by Ostara — and a printed piece without them
            // cannot be mounted. A subpath of its own is the seam allowance, which a print shows.
            QPainterPath shape;
            // **Nothing when the payload says nothing.** A piece whose main path is hidden and that
            // carries no seam allowance is not drawn at all by Ostara, and the payload carries an
            // empty ink for it; drawing its plain outline here would be drawing what the creator
            // chose not to show.
            for (const QPolygonF &polygon : piece.ink)
            {
                QPolygonF placed;
                placed.reserve(polygon.size());
                for (const QPointF &point : polygon)
                {
                    placed.append(PlacedPoint(point, placement, origin));
                }
                shape.addPolygon(placed);
            }

            auto *item = new PrintPieceItem(shape, piecePen, piece.placementIndex, this);
            m_scene->addItem(item);

            // The piece's own drawing, laid by the same frame as its outline — the same rule the
            // cutting plan's canvas applies, and the same transform.
            if (reference && !piece.decor.content.isEmpty())
            {
                auto *renderer = new QSvgRenderer(SvgDocument(piece.decor, /*oneInk=*/false), m_scene);
                if (renderer->isValid())
                {
                    auto *item = new QGraphicsSvgItem();
                    item->setSharedRenderer(renderer);

                    QTransform transform;
                    transform.translate(placement.x, placement.y);
                    transform.rotate(placement.rotation);
                    transform.translate(piece.decor.box.left() - origin.x(),
                                        piece.decor.box.top() - origin.y());

                    item->setTransform(transform);
                    m_scene->addItem(item);
                }
            }
        }
    }

    // ── Les traits de coupe ───────────────────────────────────────────────────────────────────
    //
    // The scene draws its own cut lines over the pieces — which is where they are readable — and
    // stands them down wherever the FRAME draws them: two lines in the same place, one of them the
    // user's at the opacity she set, is the defect Ostara's preview was cured of.
    if (plan.grid.valid && !framed)
    {
        m_scene->addItem(new ArtboardGridItem(plan.grid.planRect, plan.grid.cellWidthPx,
                                              plan.grid.cellHeightPx, plan.grid.columns,
                                              plan.grid.rows, plan.sheets));
    }

    QRectF sceneRect = planRect.adjusted(-20.0, -20.0, 20.0, 20.0);
    if (!m_coverPaper.isEmpty())
    {
        sceneRect = sceneRect.united(m_coverPaper.adjusted(-20.0, -20.0, 20.0, 20.0));
    }

    m_scene->setSceneRect(sceneRect);
    m_view->fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    RefreshLineWidth();

    // The same figures Ostara writes under a printed plan: the cloth the run takes, then the same
    // two in the units somebody else buys it in.
    const double widthMm = plan.binWidthPx * kMmPerPx;
    const double lengthMm = plan.usedHeightPx * kMmPerPx;

    QString detail = tr("Document width %1 · length %2").arg(Cm(widthMm), Cm(lengthMm));
    if (plan.grid.valid)
    {
        detail += QStringLiteral(" · ")
                  + tr("%n sheet(s)", nullptr, plan.sheets.count);
    }

    detail += QStringLiteral(" · ") + tr("Fabric width %1 · length %2").arg(Inch(widthMm), Yards(lengthMm));
    m_detail->setText(detail);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::MovePrintPiece(int placementIndex, const QPointF &delta)
{
    if (!m_printMode || placementIndex < 0
        || placementIndex >= static_cast<int>(m_printPlan.placements.size()))
    {
        return;
    }

    Placement &placement = m_printPlan.placements.at(static_cast<std::size_t>(placementIndex));
    placement.x += delta.x();
    placement.y += delta.y();

    // **The paper follows the ink.** A sheet the gesture emptied is not printed any more, and one
    // it put a piece back on is printed again — the same rule the layout ran, read again.
    m_printPlan.RefreshSheets();
    RedrawPrint();
}
