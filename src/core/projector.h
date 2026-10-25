/************************************************************************
 **
 **  @file   projector.h
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

#ifndef PROJECTOR_H
#define PROJECTOR_H

#include "core/print/printplan.h"

#include "io/GradedOverlays.h"  // Trame — les calques d'une projection

#include <QString>

/**
 * @brief La recherche de largeur du format Projecteur, portée d'Ostara.
 *
 * The engine needs a WIDTH and produces a height; a projection has no laize, so its width is
 * **searched**. This is that search and nothing else: it lays the run out on a ladder of widths,
 * measures what came back, and keeps the first plan whose page fits.
 *
 * **Why a ladder and not a formula.** The height does not decrease monotonically with the width —
 * the engine is a heuristic — so a dichotomy could step over a width that works. The ladder is
 * climbed in order, and the first success is the smallest width that fits.
 *
 * **Why the page is measured, not the bin.** A width palier is not the page: the engine keeps the
 * margin on both sides and the pieces do not fill the bin, so the page is the **tight box of the
 * placements, plus the edge margin on four sides**. That box becomes the plan's own width and
 * height, which is what the canvas draws and what the PDF page measures.
 *
 * ⚠️ **The cap is held, not patched up afterwards.** Past 5080 mm (14400 pt) a PDF page has to carry
 * a `/UserUnit`, and Trame writes none: the plan would come out **shrunk, and looking right**. So a
 * plan that does not fit is refused with its numbers, and no file is written.
 *
 * @see Modifications/Ostara/ExportMode/Projector.md §10 (la recherche), §11 (le refus)
 */
namespace Projector
{
/// @brief Ce que le format supporte, et ce que l'utilisatrice a demandé.
struct Limits
{
    /// Le plafond de la page, en millimètres — **5000**, le plus grand entier sous les deux limites
    /// d'Acrobat (200 in = 5080 mm) et de Qt (14400 pt), qui tombent au même endroit.
    double pageMaxMm{5000.0};

    /// L'espace demandé entre deux pièces, en centimètres — **10 cm par défaut**.
    double gapCm{10.0};

    /// Le plancher absolu de cet espace — **5 cm**. En dessous, ce n'est plus un plan projecteur,
    /// c'est un plan de coupe.
    double gapFloorCm{5.0};

    /// **Un seul palier, pour MESURER.** Zéro — le cas de l'application — les essaie tous. Une
    /// recherche coûte une nidification par palier, et le chiffre du banc est ce qui permet de dire
    /// ce que coûte une tentative au lieu de le supposer.
    int onlyWidthMm{0};
};

/// @brief Ce qu'une recherche a donné.
struct Outcome
{
    bool ok{false};

    /// Le plan retenu, **déjà translaté dans le repère de sa page** et portant la page dans
    /// `binWidthPx` × `usedHeightPx` : tout ce qui le lit ensuite n'a rien à savoir de la recherche.
    PrintPlan plan{};

    /// Le refus, déjà rédigé et chiffré. Vide quand @ref ok.
    QString error{};

    /// La largeur du palier retenu, en millimètres.
    int widthMm{0};

    /// L'espace entre pièces du plan retenu, en centimètres.
    double gapCm{0.0};

    /// Combien de nidifications la recherche a coûtées — la promesse « 1 à 3 » tenue ou non.
    int attempts{0};

    /// Ce que la recherche a duré, en secondes.
    double seconds{0.0};
};

/**
 * @brief Cherche la plus petite largeur dont la page tient sous le plafond.
 *
 * L'échelle des paliers est 150, 200, … 500 cm. L'espace demandé est tenté **sur tous les paliers
 * d'abord**, puis le plancher de 5 cm sur tous les paliers : l'espacement passe avant la compacité.
 *
 * @param marginCm l'écart au bord demandé — zéro par défaut
 */
auto Nest(const ImportedDocument &document, const QStringList &sizeIds, const QStringList &pieceNames,
          double marginCm, const Limits &limits) -> Outcome;

/**
 * @brief Les trois calques que la page porte SOUS les pièces : le fond magenta, la grille de 5 cm,
 * la grille de 3 pouces.
 *
 * **Les mêmes valeurs qu'Ostara, et c'est le format qui les décide** : `#E6007E`, un pas de 50 mm,
 * un pas de 76,2 mm. Les noms sont ceux que le lecteur PDF lit dans son panneau ; ils n'ont pas à
 * être traduits pour être justes, et Rabnest n'a pas de table de traduction — ils restent en anglais
 * comme tout ce que cette application écrit.
 */
auto Overlays() -> GradedOverlays::Set;
}  // namespace Projector

#endif  // PROJECTOR_H
