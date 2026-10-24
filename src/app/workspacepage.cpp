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
#include "core/print/exportprintmodel.h"
#include "core/print/printmetrics.h"
#include "core/print/printplan.h"

#include <QCoreApplication>
#include <QGraphicsItemGroup>
#include <QGraphicsPathItem>
#include <QGraphicsRectItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSizeF>
#include <QTimer>
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
#include <limits>

namespace
{
/// The band between the two cloths of a pair, in scene pixels — **one inch, and a FIXED one**
/// (`oexportoutputs.cpp:103`, `kLiningGapPx`). Fixed and not proportional: a gap that grew with the
/// plan would give two runs of the same width two different spacings, and two drawings that no
/// longer compare. An inch is enough to see at a glance that there are two plans, and not enough to
/// read anything between them.
constexpr double kLiningGapPx = 96.0;

constexpr double kCmPerPx = 2.54 / 96.0;
constexpr double kScale = 1000000.0;
constexpr double kMmPerPx = 25.4 / 96.0;

/// **Ostara's hairline, and the one width a printed plan is drawn with.** Every piece of her preview
/// carries the pen `WidthHairLine()` states — her pattern's line width (1.2 mm by default) divided by
/// three, at 96 dpi — whatever the paper, black or in its size's colour
/// (`vlayoutpiece.cpp:1710-1712`, `vcommonsettings.cpp:1914`). A pen that follows the ZOOM instead,
/// as the cutting plan's does, draws a print run four times too fat when the plan is fitted: one
/// device pixel, where Ostara draws four tenths of one, and every dash four times too long.
constexpr double kHairlinePx = 1.2 / 3.0 / 25.4 * 96.0;

/// **What a printed piece is drawn with on the SCREEN.** The hairline is the pattern's own width, and
/// it is the truth — but a plan fitted to the canvas is drawn about an eighth of its size, where a
/// hairline of four tenths of a device pixel is not a thin line, it is a pale one: a run of two or
/// three sizes reads as a heap of colour rather than as contours. So the canvas keeps a floor of
/// seven tenths of a device pixel, and only a floor — a plan zoomed in still reaches the pattern's
/// exact hairline.
///
/// ⚠️ **The screen's, never the file's.** A drawing goes out with the pen the pattern states
/// (`kHairlinePx`); `RenderPlan` puts the file's width back before it renders. Thickening the file
/// would thicken the pattern — the opposite of porting it.
constexpr double kPrintCanvasFloorPx = 0.7;

/// **And the DASHES are not drawn at all on the canvas — every size gets a plain line, its colour
/// kept.** A pattern writes its sizes apart with a colour AND a dash: size 32 of a real pattern is
/// 0,46 mm of ink every 2,82 mm, which is a fine dotted line at 1:1 on paper and a third of a device
/// pixel — a mist — at the scale a plan is shown. A legibility floor on the pattern made it a dotted
/// line again, still fine, still hard to read with a single size on the sheet; a plain line is what
/// is actually readable, and the colour still tells two sizes apart. The dashes are not lost: the
/// FILE is drawn with them, exactly as the pattern states (`RenderPlan`, file mode).
constexpr bool kCanvasKeepsDashes = false;

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

    // The fit is taken again whenever the viewport changes size — see `eventFilter`. A resize is
    // not a signal the view emits, so the one object that has the size is watched instead.
    m_view->viewport()->installEventFilter(this);

    // The SPACE key belongs to the whole window, not to the canvas: it is held to pan, and the
    // focus may well be in the dock when it is pressed. It is therefore watched at the application's
    // own door — see `eventFilter`.
    qApp->installEventFilter(this);

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
void WorkspacePage::Show(const CutPlan &plan, const CutPlan *lining)
{
    Draw(plan, lining);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::Clear()
{
    m_scene->clear();
    m_detail->clear();
    m_zoomed = false;
    m_printMode = false;
    m_printItems.clear();
    m_dragItem = nullptr;
}

//---------------------------------------------------------------------------------------------------------------------
bool WorkspacePage::eventFilter(QObject *watched, QEvent *event)
{
    // ── La barre d'espace ──────────────────────────────────────────────────────────────────────
    //
    // **Held, the space bar means "move around", and a click then pans — it never takes a piece.**
    // The state is kept here because the press arrives on the viewport, where the key is long gone.
    // It is dropped when the window stops being active: a release that happens in another
    // application is a release this canvas never sees, and a stuck space bar would leave the plan
    // impossible to rearrange.
    if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
    {
        auto *key = static_cast<QKeyEvent *>(event);

        // Only a key pressed IN THIS WINDOW: a space typed in a dialogue belongs to the dialogue.
        auto *receiver = qobject_cast<QWidget *>(watched);
        const bool ours = receiver != nullptr && receiver->window() == window();

        if (ours && key->key() == Qt::Key_Space)
        {
            if (event->type() == QEvent::KeyPress)
            {
                m_spaceHeld = true;
            }
            else if (!key->isAutoRepeat())
            {
                m_spaceHeld = false;
            }
        }
    }
    else if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::ApplicationDeactivate)
    {
        m_spaceHeld = false;
    }

    if (watched == m_view->viewport() && event->type() == QEvent::Resize && !m_zoomed)
    {
        FitToScene();
    }

    // The gesture of a piece is taken here, below the view's own handling — see
    // `WorkspacePage::HandlePrintDrag`, defined with the item it moves.
    if (watched == m_view->viewport() && HandlePrintDrag(event))
    {
        return true;
    }

    return QWidget::eventFilter(watched, event);
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::FitToScene()
{
    if (m_view == nullptr || m_scene == nullptr || m_fitting)
    {
        return;
    }

    const QRectF rect = m_scene->sceneRect();
    if (rect.isEmpty())
    {
        return;
    }

    // ⚠️ **The fit can ask for the resize it is answering.** Fitting changes the scrollbars, a
    // scrollbar changes the viewport, and the resize comes back here — so the call is guarded, and
    // the second pass is skipped rather than run against a size that is still moving.
    m_fitting = true;
    m_view->fitInView(rect, Qt::KeepAspectRatio);
    RefreshLineWidth();
    m_fitting = false;
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

    m_zoomed = true;
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
void WorkspacePage::Draw(const CutPlan &plan, const CutPlan *lining)
{
    m_printMode = false;
    m_printItems.clear(); // the items of a print run are gone with its scene
    m_dragItem = nullptr;
    m_scene->clear();

    if (!plan.ok)
    {
        m_detail->setText(plan.error);
        return;
    }

    QRectF bounds = DrawCloth(plan, QPointF(0.0, 0.0));

    // ── La doublure, à droite du tissu, dans la MÊME scène ──────────────────────────────────────
    //
    // La scène ne se vide qu'une fois, et la seconde nappe s'ajoute à la première à la place qui lui
    // est réservée. Une seule scène pour les deux : c'est ce qui fait qu'on les lit ensemble, qu'un
    // seul zoom les cadre toutes les deux, et que la ligne de chiffres parle d'un run et non de deux
    // (oexportoutputs.cpp:1937-1950).
    if (lining != nullptr && lining->ok)
    {
        const QPointF seat(bounds.right() + kLiningGapPx, bounds.top());
        bounds = bounds.united(DrawCloth(*lining, seat));
    }

    m_scene->setSceneRect(bounds.adjusted(-20.0, -20.0, 20.0, 20.0));
    m_zoomed = false; // a plan put on screen is looked at WHOLE
    FitToScene();

    m_detail->setText(FiguresText(plan, lining));
}

//---------------------------------------------------------------------------------------------------------------------
auto WorkspacePage::DrawCloth(const CutPlan &plan, const QPointF &seat) -> QRectF
{
    // **Une nappe, et son siège.** Les deux étoffes sont dessinées par CETTE fonction : une pièce,
    // son encre, son décor et ses enfants ne peuvent donc pas exister d'un côté et manquer de
    // l'autre. Le siège est la seule chose qui les distingue.
    auto *group = new QGraphicsItemGroup();
    // ⚠️ **Le siège se pose APRÈS les items, jamais avant.** `addToGroup` remappe chaque item pour
    // que sa position à l'écran ne change pas : un groupe déjà déplacé est donc compensé item par
    // item, et la doublure se dessine à l'origine du tissu — deux nappes l'une sur l'autre, la
    // seconde recouvrant le haut de la première. Posé à la fin, le siège TRANSLATE le groupe et
    // tout ce qu'il porte, ce qui est exactement le décalage voulu.

    // The cloth: what the pieces were laid into, at the width she paid for and the length the plan
    // actually needs.
    QPen border(QColor(0xD6, 0xD6, 0xD6));
    border.setCosmetic(true);
    // ⚠️ **Le fond est le TISSU, et il est coloré — d'un bout à l'autre.** Il était blanc avec des
    // pièces beiges par-dessus : deux fonds, une bande blanche autour du plan et des pièces qui se
    // découpent en clair sur du clair. Le tissu prend donc la couleur, et les pièces ne sont plus
    // remplies du tout : ce qu'on voit d'une pièce, c'est son trait.
    const QBrush clothBrush(QColor(0xF7, 0xF5, 0xF2));
    auto *cloth = new QGraphicsRectItem(QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx));
    cloth->setPen(border);
    cloth->setBrush(clothBrush);
    group->addToGroup(cloth);

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
        group->addToGroup(new FoldGuideItem(outline, foldZonePen, {56.0, 32.0}));

        const double x = zone.foldOnTheLeft ? zone.rect.left() : zone.rect.right();
        QPainterPath axis;
        axis.moveTo(x, zone.rect.top());
        axis.lineTo(x, zone.rect.bottom());
        group->addToGroup(new FoldGuideItem(axis, foldZonePen, {32.0, 16.0, 2.0, 16.0}));
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

        group->addToGroup(new PatternPathItem(shape, piecePen));

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
                group->addToGroup(item);
            }
        }
    }

    group->setPos(seat);
    m_scene->addItem(group);

    // The cloth in the scene's own coordinates — the seat included, since that is where it is drawn.
    return QRectF(seat, QSizeF(plan.binWidthPx, plan.usedHeightPx));
}

//---------------------------------------------------------------------------------------------------------------------
auto WorkspacePage::FiguresText(const CutPlan &plan, const CutPlan *lining) const -> QString
{
    // ⚠️ **Un seul plan se lit exactement comme avant** : ni le nom de l'étoffe, ni le gras, ni un
    // séparateur de plus. Une paire, elle, nomme ses deux moitiés — deux laizes et deux métrages à
    // la suite seraient un rébus — et le temps ferme la ligne une seule fois, puisqu'il est celui du
    // run (oexportoutputs.cpp:305-338).
    if (lining == nullptr || !lining->ok)
    {
        QString detail = ClothFigures(plan);
        if (plan.seconds > 0.0)
        {
            detail += QStringLiteral(" · ") + DurationText(plan.seconds);
        }
        return detail;
    }

    // ── Les deux nappes, une par ligne ────────────────────────────────────────────────────────
    //
    // **Décidé par Sophie le 24-10-2026.** Sur une ligne, la paire ne tenait pas : quatre chiffres,
    // deux unités et deux systèmes de mesure, et la fin de la phrase sortait de l'écran. Une nappe
    // par ligne, et le temps sur la seconde — il est celui du run, donc il ferme la lecture.
    //
    // ⚠️ `&lt;br&gt;` et non « \n » : l'étiquette est en texte riche (le nom des étoffes est en gras), et le
    // texte riche avale un saut de ligne comme un espace.
    const QString fabric = QStringLiteral("<b>%1</b> : ").arg(tr("Fabric")) + ClothFigures(plan);
    const QString cloth = QStringLiteral("<b>%1</b> : ").arg(tr("Lining")) + ClothFigures(*lining);

    QString detail = fabric + QStringLiteral("<br>") + cloth;
    if (plan.seconds > 0.0)
    {
        detail += QStringLiteral(" · ") + DurationText(plan.seconds);
    }
    return detail;
}

//---------------------------------------------------------------------------------------------------------------------
auto WorkspacePage::ClothFigures(const CutPlan &plan) const -> QString
{
    // The same line Ostara writes under its cut plan: the cloth in the unit it is bought in, then the
    // same two figures in the units somebody else buys it in — inches for the width, yards for the
    // length, because a bolt is sold one way in one country and the other way in the next.
    const double widthMm = plan.fabricWidthCm * 10.0;
    const double lengthMm = plan.usedHeightPx * kMmPerPx;

    QString figures;
    if (plan.foldedFabric)
    {
        figures += tr("Canvas: half fabric width %1").arg(Cm(widthMm / 2.0)) + QStringLiteral(" · ");
    }

    figures += tr("Fabric width %1 · length %2").arg(Cm(widthMm), Cm(lengthMm));
    figures += QStringLiteral(" · ")
               + tr("Fabric width %1 · length %2").arg(Inch(widthMm), Yards(lengthMm));
    return figures;
}

//---------------------------------------------------------------------------------------------------------------------
/// **Ostara's holder, and the one ink a hand's gesture shows.** The frame drawn around a footprint
/// being moved is hers to the pixel: `QColor(0, 122, 204)`, a DASHED cosmetic pen — width 0, so one
/// device pixel whatever the zoom — and nothing filled (`oexportholder.cpp:48`, `:205-207`).
const QColor kHolderColour(0, 122, 204);

/// One size's drawing, with the line it is drawn by: its own colour, its own dashes.
struct PrintInk
{
    QPainterPath shape;
    QColor colour;
    QVector<qreal> dashes;
};

/// **One FOOTPRINT, and every size it prints.** The engine places one footprint per piece — the
/// union of that piece over the sizes being printed — so the sizes of one piece stand at ONE
/// placement, and a hand that takes one takes them all.
///
/// ⚠️ **They therefore move together DURING the gesture, and not only once it is over.** A single
/// item per size moved on its own would drag one outline out of a graded drawing and put the others
/// back on release — the gesture would show one size and commit ten. The item carries the whole
/// footprint, so what the hand moves is what the plan is.
///
/// The move is applied to the PLAN on release, and only then: the sheets are read again from where
/// the pieces stand, and reading them mid-gesture would rebuild the scene under the hand holding it.
class PrintPlacementItem : public QGraphicsItem
{
public:
    PrintPlacementItem(QVector<PrintInk> inks, int placementIndex, bool plain)
      : m_inks(std::move(inks)), m_placementIndex(placementIndex), m_plain(plain)
    {
        // ⚠️ **Not `ItemIsMovable`, and the gesture is not the item's.** A view in `ScrollHandDrag`
        // keeps the drags it starts, and whether it hands one to an item is a detail of the view's
        // own state — a drag that answers "sometimes" is a drag that does not answer at all. The
        // page takes the gesture on the viewport, where it cannot be swallowed, and moves the item
        // (see `WorkspacePage::eventFilter`).
        setFlag(QGraphicsItem::ItemIsMovable, false);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setAcceptedMouseButtons(Qt::NoButton);
        setCursor(Qt::OpenHandCursor);

        for (const PrintInk &ink : m_inks)
        {
            m_bounds = m_bounds.united(ink.shape.boundingRect());
        }

        // Half the hairline, twice: the frame and the strokes must not be shaved by the item's own
        // rectangle.
        m_bounds.adjust(-2.0, -2.0, 2.0, 2.0);
    }

    /// **Grabbed by its SURFACE, never by its outline alone.** The item is drawn unfilled, and a
    /// path item's own shape follows the stroke it draws — so the piece could only be taken by the
    /// line, and a piece one has to click exactly on the line of is a piece one cannot move. The
    /// filled path is the same answer Ostara's own hit item gives.
    auto shape() const -> QPainterPath override
    {
        // ⚠️ **`WindingFill`, and the drag depends on it.** Qt's default rule is `OddEvenFill`: a
        // point covered by an EVEN number of outlines is OUTSIDE the shape. The sizes of one
        // footprint are drawn almost on top of each other, so a point in the middle of a piece is
        // covered by every one of them — two sizes gave a surface of nothing at all, and the piece
        // could not be taken by the hand, while one size worked and three sometimes did. Winding
        // fills every contour the same way round, which is what "the piece's surface" means.
        QPainterPath all;
        all.setFillRule(Qt::WindingFill);
        for (const PrintInk &ink : m_inks)
        {
            all.addPath(ink.shape);
        }
        return all;
    }

    auto boundingRect() const -> QRectF override { return m_bounds; }

    /// The frame is what says a gesture is on: the page raises it when a hand takes the piece and
    /// drops it when the hand lets go.
    void SetMoving(bool moving)
    {
        if (m_moving == moving)
        {
            return;
        }

        m_moving = moving;
        update();
    }

    /// What the gesture moves: the item itself, and everything laid with it — the drawing of the
    /// piece, its cartouche, its annotations, its miniature, its grain line.
    void DragBy(const QPointF &step)
    {
        moveBy(step.x(), step.y());

        for (QGraphicsItem *follower : m_followers)
        {
            follower->moveBy(step.x(), step.y());
        }
    }

    /// The one pen of a run drawn without the pattern's own line: black, solid. What the user asked
    /// for when the run prints a SINGLE size, where a size's colour and dashes distinguish nothing
    /// and only make the drawing hard to read.
    static auto PlainPen(double width) -> QPen
    {
        QPen pen(Qt::black);
        pen.setWidthF(width);
        pen.setStyle(Qt::SolidLine);
        pen.setCapStyle(Qt::SquareCap);
        return pen;
    }

    /// The width for the zoom the CANVAS is at: the pattern's hairline, and never thinner than the
    /// floor that keeps it visible on a fitted plan.
    void SetCanvasScale(double viewScale)
    {
        m_canvas = true;
        SetLineWidth(viewScale > 0.0 ? std::max(kHairlinePx, kPrintCanvasFloorPx / viewScale)
                                     : kHairlinePx);
    }

    /// The width a FILE is drawn with — the pattern's own, whatever the canvas looks like, and with
    /// the dashes the pattern states rather than the canvas's legibility floor.
    void SetFileWidth(double width)
    {
        m_canvas = false;
        SetLineWidth(width);
    }

    auto PlacementIndex() const -> int { return m_placementIndex; }

    void SetFollowers(const QVector<QGraphicsItem *> &followers) { m_followers = followers; }

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *) override
    {
        painter->setBrush(Qt::NoBrush);
        for (const PrintInk &ink : m_inks)
        {
            // The canvas never dashes: the pattern's line is drawn plain, in its own colour. The
            // file gets the pattern's own dashes back — see `kCanvasKeepsDashes`.
            const QVector<qreal> dashes = (m_canvas && !kCanvasKeepsDashes) ? QVector<qreal>()
                                                                            : ink.dashes;
            painter->setPen(m_plain ? PlainPen(m_lineWidth)
                                    : PrintPen(ink.colour, dashes, m_lineWidth));
            painter->drawPath(ink.shape);
        }

        if (!m_moving)
        {
            return;
        }

        // Cosmetic, so the frame stays a hairline at every zoom — Ostara's own reason for a width of
        // zero (`oexportholder.cpp:203-205`).
        painter->setPen(QPen(kHolderColour, 0.0, Qt::DashLine));
        painter->drawRect(m_bounds);
    }

    /// The pen of a printed piece: Ostara's hairline, the size's colour, and the dashes divided by
    /// the width — a Qt pattern counts in WIDTHS, so a pixel length has to be divided by the pen
    /// that draws it. Her own conversion, to the letter (`ogratedsizestyles.cpp:148-159`).
    static auto PrintPen(const QColor &colour, const QVector<qreal> &dashes, double width) -> QPen
    {
        QPen pen(colour);
        pen.setWidthF(width);

        if (dashes.isEmpty())
        {
            pen.setStyle(Qt::SolidLine);
            pen.setCapStyle(Qt::SquareCap);
            return pen;
        }

        QVector<qreal> pattern;
        pattern.reserve(dashes.size());
        for (const qreal length : dashes)
        {
            pattern.append(length / width);
        }

        pen.setStyle(Qt::CustomDashLine);
        pen.setDashPattern(pattern);
        pen.setCapStyle(Qt::FlatCap);
        return pen;
    }

    void SetLineWidth(double width)
    {
        if (std::abs(m_lineWidth - width) < 1e-6)
        {
            return;
        }

        m_lineWidth = width;
        update();
    }

    QVector<PrintInk> m_inks;
    QVector<QGraphicsItem *> m_followers;
    double m_lineWidth = kHairlinePx;
    bool m_canvas = true;

    /// Black and solid, whatever the size's own line says — a run of ONE size, when the user asked
    /// for it.
    bool m_plain = false;
    QRectF m_bounds;
    int m_placementIndex{-1};
    bool m_moving{false};
};

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

    // A printed piece is not a `PatternPathItem` — it draws several inks, each with its own pen —
    // so its width is stated here, for the same reason and at the same moment.
    for (QGraphicsItem *item : m_printItems)
    {
        static_cast<PrintPlacementItem *>(item)->SetCanvasScale(scale);
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

    // ⚠️ **The FILE is drawn with the pattern's hairline, not with the screen's floor.** The canvas
    // is allowed a little more so a fitted plan stays readable; a drawing is not.
    for (QGraphicsItem *item : m_printItems)
    {
        static_cast<PrintPlacementItem *>(item)->SetFileWidth(kHairlinePx);
    }

    m_scene->render(painter, targetPx, sourcePx, Qt::IgnoreAspectRatio);

    for (QGraphicsItem *item : m_printItems)
    {
        static_cast<PrintPlacementItem *>(item)->SetCanvasScale(scale);
    }

    RefreshLineWidth();
}

//---------------------------------------------------------------------------------------------------------------------
bool WorkspacePage::HandlePrintDrag(QEvent *event)
{
    if (!m_printMode || m_printItems.isEmpty())
    {
        return false;
    }

    if (event->type() == QEvent::MouseButtonPress)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);

        // A hand holding space is moving the drawing, not taking a piece: the press falls through to
        // the view, which pans with it.
        if (mouse->button() != Qt::LeftButton || m_dragItem != nullptr || m_spaceHeld)
        {
            return false;
        }

        const QPointF scenePos = m_view->mapToScene(mouse->position().toPoint());
        for (QGraphicsItem *candidate : m_printItems)
        {
            auto *item = static_cast<PrintPlacementItem *>(candidate);
            if (!item->shape().contains(item->mapFromScene(scenePos)))
            {
                continue;
            }

            m_dragItem = item;
            m_dragLast = scenePos;
            m_dragTotal = QPointF();
            item->SetMoving(true);
            return true;
        }

        return false;
    }

    if (m_dragItem == nullptr)
    {
        return false;
    }

    auto *item = static_cast<PrintPlacementItem *>(m_dragItem);

    if (event->type() == QEvent::MouseMove)
    {
        auto *mouse = static_cast<QMouseEvent *>(event);
        const QPointF scenePos = m_view->mapToScene(mouse->position().toPoint());
        const QPointF step = scenePos - m_dragLast;
        m_dragLast = scenePos;
        m_dragTotal += step;

        if (!step.isNull())
        {
            item->DragBy(step);
        }
        return true;
    }

    if (event->type() == QEvent::MouseButtonRelease)
    {
        m_dragItem = nullptr;
        item->SetMoving(false);

        const QPointF delta = m_dragTotal;
        m_dragTotal = QPointF();

        // ⚠️ **The item is left where the hand put it.** The scene is drawn again one turn later,
        // from the placement — putting the item back here would make the piece jump home for a
        // frame, under the hand that has just let go.
        if (!delta.isNull())
        {
            MovePrintPiece(item->PlacementIndex(), delta);
        }
        return true;
    }

    return false;
}

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
void WorkspacePage::RedrawPrint(bool refit)
{
    const PrintPlan &plan = m_printPlan;
    const ExportTemplate &decor = m_printDecor;
    const bool framed = m_printFramed;

    // ⚠️ **A redraw is not a new drawing.** When the plan is put on screen the view is fitted to it;
    // when a hand has moved a piece, the plan is only redrawn — and re-fitting there would undo the
    // zoom the hand had just taken, and slide the drawing out from under the gesture's own result.
    const QTransform viewTransform = m_view->transform();
    const int scrollX = m_view->horizontalScrollBar()->value();
    const int scrollY = m_view->verticalScrollBar()->value();

    m_scene->clear();
    m_printFollowers.clear();
    m_printItems.clear();

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
    //
    // One ITEM per footprint, and not one per size: the gesture that takes a piece takes its
    // placement, so ten sizes of one piece are one thing to the hand (see `PrintPlacementItem`).
    QHash<int, QVector<PrintInk>> inks;

    for (const PrintLayer &layer : plan.layers)
    {
        // **The children belong to ONE size, and never to all of them.** Ostara's canvas draws them
        // on the first layer of the range — its smallest size — and lets every other size draw its
        // own ink only; a cartouche, an annotation or a grain line repeated once per size would be
        // a drawing of the same words ten times over. The payload carries a copy per size all the
        // same: the copy is a lookup key, not a decision to draw ten of them.
        const bool reference = layer.reference;

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

            if (shape.isEmpty())
            {
                continue;
            }

            // The same rule when one piece is read alone: its seam allowance is a ring inside its
            // contour, and an even-odd reading would make the inside of the piece a hole.
            shape.setFillRule(Qt::WindingFill);

            PrintInk ink;
            ink.shape = shape;
            ink.colour = layer.colour;
            ink.dashes = layer.dashes;
            inks[piece.placementIndex].append(ink);

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
                    m_printFollowers[piece.placementIndex].append(item);
                }
            }
        }
    }

    for (int index = 0; index < static_cast<int>(plan.placements.size()); ++index)
    {
        if (!inks.contains(index))
        {
            continue;
        }

        auto *item = new PrintPlacementItem(inks.value(index), index, plan.plainOutline);
        item->SetFollowers(m_printFollowers.value(index));
        m_scene->addItem(item);
        m_printItems.append(item);
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

    if (refit)
    {
        m_zoomed = false; // a run put on screen is looked at WHOLE
        FitToScene();
    }
    else
    {
        // The same view of the same run, where the hand left it — the zoom it chose, and the corner
        // it was looking at.
        m_view->setTransform(viewTransform);
        m_view->horizontalScrollBar()->setValue(scrollX);
        m_view->verticalScrollBar()->setValue(scrollY);

        // ⚠️ **And the strokes for that zoom, or the trace changes weight for no reason.** The scene
        // is drawn again from scratch, so its items start at the pattern's hairline; without this
        // the line was thin after a move and thick again after the next zoom — the same run, the same
        // zoom, two different drawings. A redraw is not a new drawing.
        RefreshLineWidth();
    }

    // ── Ce que le run dit de lui-même ───────────────────────────────────────────────────────────
    //
    // **The FORMAT, the PAGES, the TIME.** A printed plan is paper: it has no cloth width and no
    // cloth length, and writing them under it was writing the cutting plan's sentence over the
    // print's — a laize the run never chose. What it does have is a paper format (A4, Letter,
    // A4/Letter framed, A0 roll…), a number of pages, and the time the laying out cost.
    const ExportPrintModel *model = FindExportPrintModel(plan.modelToken);

    QString detail = model != nullptr
                         ? QCoreApplication::translate("OExportPrintModel", model->label)
                         : tr("Unknown format");

    // The run's own count of its paper: a sheet per sheet, plus the cover page when it takes one —
    // and SAID, in brackets, because the count is higher than the sheets the grid shows and a
    // figure one cannot reconcile with the drawing is a figure one does not trust.
    const bool coverOnItsOwnSheet = !m_coverPaper.isEmpty();
    detail += QStringLiteral(" · ")
              + (coverOnItsOwnSheet ? tr("%n page(s) (cover page included)", nullptr,
                                         plan.Pages(true))
                                    : tr("%n page(s)", nullptr, plan.Pages(false)));

    if (plan.seconds > 0.0)
    {
        detail += QStringLiteral(" · ") + DurationText(plan.seconds);
    }

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

    // ⚠️ **The redraw is deferred by one turn, and it is not a detail.** Redrawing clears the scene,
    // and the scene is cleared HERE, from the mouse release of the very item it holds — deleting the
    // object whose event handler is still on the stack, and reading its members after the return.
    // One turn of the event loop later the gesture has ended, and there is nothing left to unmake.
    QTimer::singleShot(0, this, [this] { RedrawPrint(/*refit=*/false); });

    // **The run is announced at once, not with the redraw.** Whoever keeps the row takes it back
    // from here — the file and the printer are made from the row — and a deferred announcement would
    // be a turn in which the two disagree.
    emit PrintPlanEdited();
}
