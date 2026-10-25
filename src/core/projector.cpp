/************************************************************************
 **
 **  @file   projector.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   26/10/2026
 **
 **  @brief  The Projector run — one page, at 1:1, and never more than 500 × 500 cm.
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

#include "core/projector.h"

#include "core/importeddocument.h"
#include "core/pattern.h"

#include <QCoreApplication>
#include <QDomDocument>
#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPolygonF>
#include <QRectF>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
/// Les paliers de largeur, en millimètres — **150, 200, … 500 cm**, et pas un de plus : le dernier
/// est le plafond de la page, donc une largeur au-delà ne pourrait de toute façon pas être publiée.
constexpr std::array<int, 8> kWidthPaliersMm{1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000};

/// La tolérance des comparaisons de longueur : un demi-pixel. Le plafond se mesure en points dans le
/// PDF et en pixels ici ; refuser un plan qui le dépasse de 0,2 px serait refuser un plan qui tient.
constexpr double kPageTolerancePx = 0.5;

constexpr double kPxPerCm = 96.0 / 2.54;

auto Tr(const char *text) -> QString
{
    return QCoreApplication::translate("Projector", text);
}

/// Un nombre de centimètres : **entier quand il l'est**, sinon à un dixième.
auto Cm(double millimetres) -> QString
{
    const double centimetres = millimetres / 10.0;
    if (std::abs(centimetres - std::round(centimetres)) < 1e-6)
    {
        return QString::number(centimetres, 'f', 0);
    }

    return QString::number(centimetres, 'f', 1);
}

/// Un nombre de mètres carrés, à un dixième.
auto SquareMetres(double millimetresSquared) -> QString
{
    return QString::number(millimetresSquared / 1000000.0, 'f', 1);
}

/// « The plan does not fit in 500 × 500 cm » — la première ligne, la même dans les deux refus.
auto Headline(double pageMaxMm) -> QString
{
    return Tr("The plan does not fit in %1 × %1 cm").arg(Cm(pageMaxMm));
}

/// L'aire d'un contour, en pixels carrés.
///
/// **Le lacet, et pas Clipper.** Ce n'est pas la forme exacte qui compte ici mais un MINORANT : le
/// contour d'une pièce est simple, et son aire se lit sur ses points sans rien construire.
auto RingArea(const QVector<QPointF> &points) -> double
{
    if (points.size() < 3)
    {
        return 0.0;
    }

    double sum = 0.0;
    for (int index = 0, previous = points.size() - 1; index < points.size();
         previous = index, ++index)
    {
        sum += (points.at(previous).x() * points.at(index).y())
               - (points.at(index).x() * points.at(previous).y());
    }

    return std::abs(sum) / 2.0;
}

/// L'aire que les pièces occupent au minimum, en pixels carrés.
///
/// ⚠️ **C'est celle du plus grand exemplaire de chaque pose**, et pas la somme de toutes les tailles :
/// `Footprint::compute` fait l'union des tailles d'une pièce, donc additionner chaque taille donnerait
/// sept fois la surface d'une seule — et le format refuserait des plans parfaitement possibles.
auto PieceAreaPx(const QVector<CutSize> &sizes) -> double
{
    QHash<QString, double> largest;
    for (const CutSize &size : sizes)
    {
        for (const CutPiece &pose : size.poses)
        {
            const double area = RingArea(pose.outline);
            if (area > largest.value(pose.key, 0.0))
            {
                largest.insert(pose.key, area);
            }
        }
    }

    double total = 0.0;
    for (const double area : largest)
    {
        total += area;
    }

    return total;
}

/// Every group the payload names, as the set a projection DOES NOT KEEP.
///
/// **Pour le projecteur, les groupes n'existent pas** (`Projector.md` §8) : chaque membre est une
/// pièce classique, posée à côté de l'autre et portant son propre dessin. Le lecteur sait déjà faire
/// ça — un membre « dégroupé » reprend son empreinte et ses enfants — il suffit donc de le lui dire,
/// et de le lui dire pour TOUS les groupes, pas seulement ceux dont tous les membres sont retenus.
auto GroupIds(const ImportedDocument &document) -> QSet<QString>
{
    QSet<QString> ids;

    QDomDocument payload;
    if (!payload.setContent(document.payload()))
    {
        return ids;
    }

    const QDomElement pieces = payload.documentElement().firstChildElement(QStringLiteral("metadata"))
                                   .firstChildElement(QStringLiteral("pieces"));
    for (QDomElement group = pieces.firstChildElement(QStringLiteral("group")); !group.isNull();
         group = group.nextSiblingElement(QStringLiteral("group")))
    {
        const QString id = group.attribute(QStringLiteral("id"));
        if (!id.isEmpty())
        {
            ids.insert(id);
        }
    }

    return ids;
}

/// Le refus d'avant toute nidification : la surface des pièces ne tient dans aucun bac.
auto AreaRefusal(const Projector::Limits &limits, double areaPx, double marginMm) -> QString
{
    const double capacityMm = limits.pageMaxMm - 2.0 * marginMm;
    const double aMinMm2 = areaPx * std::pow(10.0 / kPxPerCm, 2.0);

    QStringList lines;
    lines << Headline(limits.pageMaxMm);
    lines << Tr("The pieces take %1 m². With an edge margin of %2 cm, only %3 m² remain in a "
                "%4 × %4 cm plan.")
                 .arg(SquareMetres(aMinMm2), Cm(marginMm), SquareMetres(capacityMm * capacityMm),
                      Cm(limits.pageMaxMm));

    // **La marge qui suffirait, et elle se calcule.** La surface utilisable d'un plan carré de côté
    // maximal est `(C − 2m)²` : elle doit couvrir l'aire des pièces, donc `m ≤ (C − √A) / 2`. C'est
    // une estimation — la forme du plan n'est pas carrée — et le message le dit (« would be enough »).
    const double needed = (limits.pageMaxMm - std::sqrt(aMinMm2)) / 2.0;
    const double rounded = std::max(0.0, std::floor(needed / 5.0) * 5.0);

    if (needed > 0.0 && rounded < marginMm)
    {
        lines << Tr("Reducing the edge margin to %1 cm would be enough.").arg(Cm(rounded));
    }
    else
    {
        lines << Tr("No edge margin can make it fit: the pieces themselves fill the largest plan this "
                    "format can hold.");
    }

    return lines.join(QStringLiteral("\n\n"));
}

/// Le refus d'après le dernier palier : la surface tient, la forme non.
auto ShapeRefusal(const Projector::Limits &limits, const QRectF &pagePx, double marginMm,
                  double gapCm) -> QString
{
    const double mmPerPx = 10.0 / kPxPerCm;
    const double widthMm = pagePx.width() * mmPerPx;
    const double heightMm = pagePx.height() * mmPerPx;
    const double overflow = std::max(widthMm, heightMm) - limits.pageMaxMm;

    QStringList lines;
    lines << Headline(limits.pageMaxMm);
    lines << Tr("The most compact plan obtained is %1 × %2 cm, with an edge margin of %3 cm and a "
                "space between pieces of %4 cm.")
                 .arg(Cm(widthMm), Cm(heightMm), Cm(marginMm), Cm(gapCm * 10.0));

    const double needed = marginMm - overflow / 2.0;
    const double rounded = std::max(0.0, std::floor(needed / 5.0) * 5.0);
    if (needed >= 0.0 && rounded < marginMm)
    {
        lines << Tr("Reducing the edge margin to %1 cm would be enough.").arg(Cm(rounded));
    }
    else
    {
        lines << Tr("Reduce the space between pieces, or split the plan.");
    }

    return lines.join(QStringLiteral("\n\n"));
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
auto Projector::Nest(const ImportedDocument &document, const QStringList &sizeIds,
                     const QStringList &pieceNames, double marginCm, const Limits &limits)
    -> Outcome
{
    Outcome outcome;

    QElapsedTimer clock;
    clock.start();

    const double pageMaxPx = limits.pageMaxMm * kPxPerCm / 10.0;
    const double marginPx = marginCm * kPxPerCm;
    const double marginMm = marginCm * 10.0;
    const double usableMaxPx = pageMaxPx - 2.0 * marginPx;

    if (usableMaxPx <= 0.0)
    {
        outcome.error = AreaRefusal(limits, 0.0, marginMm);
        outcome.seconds = clock.elapsed() / 1000.0;
        return outcome;
    }

    const QSet<QString> ungrouped = GroupIds(document);

    // ── La surface, mesurée sans nidifier ────────────────────────────────────────────────────────
    //
    // Une seule question, et elle se répond sans poser une pièce : les pièces tiennent-elles, nues,
    // dans le plus grand plan que ce format sache publier ? Si non, aucun palier ne les fera tenir, et
    // faire tourner le moteur huit fois pour l'apprendre coûterait des minutes.
    double areaPx = 0.0;
    {
        QVector<CutSize> sizes;
        QString error;
        if (!Pattern::Load(document, sizeIds, pieceNames, Cloth::Flat, sizes, &error,
                           Material::Fabric, ungrouped))
        {
            outcome.error = error;
            return outcome;
        }

        areaPx = PieceAreaPx(sizes);
    }

    if (areaPx > usableMaxPx * usableMaxPx)
    {
        outcome.error = AreaRefusal(limits, areaPx, marginMm);
        outcome.seconds = clock.elapsed() / 1000.0;
        return outcome;
    }

    // ── L'espacement, puis le plancher ───────────────────────────────────────────────────────────
    //
    // 10 cm sur TOUS les paliers d'abord, 5 cm seulement si rien ne tient : un plan plus large à
    // 10 cm vaut mieux qu'un plan étroit à 5 cm. Le plancher est tenu ICI aussi : une valeur venue
    // d'ailleurs ne descend jamais sous lui, quel que soit celui qui la demande.
    const double askedGapCm = std::max(limits.gapCm, limits.gapFloorCm);

    QVector<double> gaps{askedGapCm};
    if (limits.gapFloorCm > 0.0 && limits.gapFloorCm < askedGapCm)
    {
        gaps.append(limits.gapFloorCm);
    }

    QRectF best;
    double bestGapCm = gaps.first();
    QString engineError;

    for (const double gapCm : gaps)
    {
        for (const int widthMm : kWidthPaliersMm)
        {
            const double binWidthPx = widthMm * kPxPerCm / 10.0;

            // ⚠️ Un palier dont la surface utile ne couvre pas les pièces ne peut rien recevoir :
            // le sauter est une déduction, pas un pari — l'aire mesurée est un minorant.
            const double usableWidthPx = binWidthPx - 2.0 * marginPx;
            if (usableWidthPx <= 0.0 || areaPx > usableWidthPx * usableMaxPx)
            {
                continue;
            }

            // Le banc mesure une tentative seule : l'application n'a pas de palier imposé.
            if (limits.onlyWidthMm > 0 && widthMm != limits.onlyWidthMm)
            {
                continue;
            }

            ++outcome.attempts;

            PrintPlan plan = PrintPlan::Generate(
                document, sizeIds, pieceNames, marginCm, gapCm, binWidthPx, /*columns=*/0,
                /*sheetHeightMm=*/0.0, QRectF(), QString(), /*plainOutline=*/false, ungrouped,
                Cloth::Flat, /*perInstance=*/true, /*centred=*/true, /*grainPinned=*/true,
                /*projector=*/true);


            if (!plan.ok)
            {
                // **Un refus du moteur n'arrête pas la recherche** : c'est le palier qui est trop
                // étroit, et le suivant a le droit d'exister. Le message est gardé au cas où aucun
                // ne passerait.
                if (engineError.isEmpty())
                {
                    engineError = plan.error;
                }
                continue;
            }

            const QRectF box = plan.InkBounds();
            if (box.isNull())
            {
                continue;
            }

            // **La page, c'est la boîte serrée plus l'écart au bord** — jamais le bac. Un palier de
            // largeur n'est pas une taille de page, et c'est justement ce qui permet à un palier de
            // 300 de sortir une page de 287.
            const QRectF page(box.left() - marginPx, box.top() - marginPx, box.width() + 2.0 * marginPx,
                              box.height() + 2.0 * marginPx);
            if (page.width() <= pageMaxPx + kPageTolerancePx
                && page.height() <= pageMaxPx + kPageTolerancePx)
            {
                // ── Retenu : le plan est TRANSLATÉ dans le repère de sa page ─────────────────────
                for (Placement &placement : plan.placements)
                {
                    placement.x -= box.left() - marginPx;
                    placement.y -= box.top() - marginPx;
                }

                plan.binWidthPx = page.width();
                plan.usedHeightPx = page.height();
                plan.gapCm = gapCm;
                plan.marginCm = marginCm;

                outcome.plan = plan;
                outcome.ok = true;
                outcome.seconds = clock.elapsed() / 1000.0;
                outcome.widthMm = widthMm;
                outcome.gapCm = gapCm;
                return outcome;
            }

            // Le plus compact jusqu'ici : celui dont le plus grand côté dépasse le moins.
            if (best.isNull()
                || std::max(page.width(), page.height()) < std::max(best.width(), best.height()))
            {
                best = page;
                bestGapCm = gapCm;
            }
        }
    }

    // ── Le refus d'après le dernier palier ────────────────────────────────────────────────────────
    if (!best.isNull())
    {
        outcome.error = ShapeRefusal(limits, best, marginMm, bestGapCm);
        outcome.seconds = clock.elapsed() / 1000.0;
        return outcome;
    }

    // ── Et quand rien n'a même pu être posé ──────────────────────────────────────────────────────
    if (!engineError.isEmpty())
    {
        outcome.error = Headline(limits.pageMaxMm) + QStringLiteral("\n\n") + engineError;
        outcome.seconds = clock.elapsed() / 1000.0;
        return outcome;
    }

    outcome.error = Headline(limits.pageMaxMm) + QStringLiteral("\n\n")
                    + Tr("The pieces are wider than the largest plan this format can hold, with the "
                         "edge margin asked for. Reduce the edge margin.");
    outcome.seconds = clock.elapsed() / 1000.0;
    return outcome;
}

//---------------------------------------------------------------------------------------------------------------------
auto Projector::Overlays() -> GradedOverlays::Set
{
    GradedOverlays::Set overlays;

    // **Pas de fond blanc.** Le fond d'un PDF est blanc : un calque blanc ne servirait à rien.
    overlays.background = QStringLiteral("#e6007e");
    overlays.backgroundName = Tr("Magenta background");
    overlays.backgroundId = overlays.backgroundName;

    const QString gridCm = Tr("5cm grid");
    overlays.grids.append({gridCm, gridCm, 50.0 * kPxPerCm / 10.0});

    const QString gridIn = Tr("3in grid");
    overlays.grids.append({gridIn, gridIn, 76.2 * kPxPerCm / 10.0});

    return overlays;
}
