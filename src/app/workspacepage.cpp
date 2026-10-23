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

#include <QGraphicsScene>
#include <QGraphicsSvgItem>
#include <QSvgRenderer>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPolygonF>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kCmPerPx = 2.54 / 96.0;
constexpr double kScale = 1000000.0;
constexpr double kMmPerPx = 25.4 / 96.0;

//---------------------------------------------------------------------------------------------------------------------
/// The piece's own drawing as a document of its own, sized to its own box — so one unit is one pixel
/// and the placement of the piece can be applied to it as a transform, without a scaling of its own.
auto SvgDocument(const PieceDecor &decor) -> QByteArray
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

    svg += decor.content;
    svg += QStringLiteral("</svg>");

    return svg.toUtf8();
}

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
}

//---------------------------------------------------------------------------------------------------------------------
void WorkspacePage::Draw(const CutPlan &plan)
{
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
    m_scene->addRect(QRectF(0.0, 0.0, plan.binWidthPx, plan.usedHeightPx), border,
                     QBrush(Qt::white));

    // The fold, in dashes: the one thing a folded plan has to show, because a piece glued to that
    // edge is cut ON the fold and not through it.
    QPen fold(QColor(0x9A, 0x9A, 0x9A), 0.0, Qt::DashLine);
    fold.setCosmetic(true);
    for (const FoldZone &zone : plan.zones)
    {
        m_scene->addRect(zone.rect, fold, QBrush(Qt::NoBrush));

        const double x = zone.foldOnTheLeft ? zone.rect.left() : zone.rect.right();
        m_scene->addLine(QLineF(x, zone.rect.top(), x, zone.rect.bottom()),
                         QPen(QColor(0x7A, 0x7A, 0x7A), 0.0, Qt::DashDotLine));
    }

    QPen piecePen(QColor(0x2B, 0x2B, 0x2B));
    piecePen.setCosmetic(true);
    const QBrush pieceBrush(QColor(0xF7, 0xF5, 0xF2));

    const int count =
        std::min(static_cast<int>(plan.placements.size()), static_cast<int>(plan.footprint.polygons.size()));
    for (int index = 0; index < count; ++index)
    {
        const QPointF origin = index < plan.origins.size() ? plan.origins.at(index) : QPointF();
        const Placement &placement = plan.placements.at(static_cast<size_t>(index));
        m_scene->addPolygon(
            PlacedOutline(plan.footprint.polygons.at(static_cast<size_t>(index)), placement, origin),
            piecePen, pieceBrush);

        if (index >= plan.poses.size())
        {
            continue;
        }

        const CutPiece &pose = plan.poses.at(index);

        // ── Le décor de la pièce ────────────────────────────────────────────────────────────────
        //
        // **Its real drawing, not a pair of points.** Ostara writes the cartouche, the annotations,
        // the grain line with its arrows and the fold mark in the layer this file used to throw away;
        // they are in the pattern's frame with one placement matrix above them, and that matrix is
        // dropped at read time. So all that is left is to lay them with the piece: the same
        // translation, the same rotation, and the offset of their own box.
        const PieceDecor decor = plan.decor.value(pose.name);
        if (!decor.content.isEmpty())
        {
            auto *renderer = new QSvgRenderer(SvgDocument(decor), m_scene);
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
