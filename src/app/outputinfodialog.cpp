/************************************************************************
 **
 **  @file   outputinfodialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  What one output was asked for, and what it cost.
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

#include "app/outputinfodialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
constexpr double kCmPerPx = 2.54 / 96.0;

//---------------------------------------------------------------------------------------------------------------------
auto Cm(double centimetres) -> QString
{
    return QObject::tr("%1 cm").arg(centimetres, 0, 'f', 1);
}

//---------------------------------------------------------------------------------------------------------------------
auto YesNo(bool value) -> QString
{
    return value ? QObject::tr("Yes") : QObject::tr("No");
}

//---------------------------------------------------------------------------------------------------------------------
auto AddRow(QFormLayout *form, const QString &label, const QString &value) -> void
{
    auto *field = new QLabel(value);
    field->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(label, field);
}

//---------------------------------------------------------------------------------------------------------------------
/// Combien de fois la pièce se coupe, et si elle se coupe dans le pli.
///
/// ⚠️ **Ce sont les chiffres de la PIÈCE, pas ceux du plan.** « Cut 1 on the fold » se dit même sur
/// du tissu déplié : c'est une propriété du patron — cette pièce-là se coupe dans le pli, un point
/// c'est tout — et non la façon dont ce plan-ci l'a posée. Le plan, lui, déplie la pièce, la double
/// ou en pose la moitié selon le tissu ; rien de cela ne change ce que la pièce EST.
auto CutText(const CutPiece &piece, Material material) -> QString
{
    // The figure of the cloth this list is about: a piece lined twice is « Cut 2 » in the lining's
    // list and « Cut 1 » in the fabric's, and neither is the other's.
    const int cut = material == Material::Lining ? piece.cutLining : piece.cutFabric;
    return piece.onFold ? QObject::tr("Cut %1 on the fold").arg(cut)
                        : QObject::tr("Cut %1").arg(cut);
}

//---------------------------------------------------------------------------------------------------------------------
/// The pieces of one cloth, in the order the plan lays them, one entry per NAME — a piece cut twice
/// is one line saying « Cut 2 », not two lines.
auto PieceList(const CutPlan &plan) -> QVector<CutPiece>
{
    QVector<CutPiece> pieces;
    for (const CutPiece &pose : plan.poses)
    {
        const bool known =
            std::any_of(pieces.cbegin(), pieces.cend(),
                        [&pose](const CutPiece &seen) { return seen.name == pose.name; });
        if (!known)
        {
            pieces.append(pose);
        }
    }
    return pieces;
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
OutputInfoDialog::OutputInfoDialog(const CutPlan &plan, const CutPlan *lining, QWidget *parent)
    : QDialog(parent)
{
    const bool pair = lining != nullptr && lining->ok;

    setWindowTitle(tr("%1 - Cut").arg(plan.sizeLabel));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(12);

    // ── Ce qui a été demandé ───────────────────────────────────────────────────────────────────
    auto *asked = new QGroupBox(tr("What this output was asked for"), this);
    auto *askedForm = new QFormLayout(asked);
    AddRow(askedForm, tr("Fabric width"), Cm(plan.fabricWidthCm));
    // The lining's width is the one setting the pair does not share — so it is the one line added.
    if (pair)
    {
        AddRow(askedForm, tr("Lining width"), Cm(lining->fabricWidthCm));
    }
    AddRow(askedForm, tr("Margin from the edges"), Cm(plan.marginCm));
    AddRow(askedForm, tr("Gap between two pieces"), Cm(plan.gapCm));
    AddRow(askedForm, tr("The fabric is folded"), YesNo(plan.foldedFabric));
    AddRow(askedForm, tr("All pieces the same way round"), YesNo(plan.oneWay));
    layout->addWidget(asked);

    // ── Ce qu'il a coûté ───────────────────────────────────────────────────────────────────────
    auto *made = new QGroupBox(tr("What it produced"), this);
    auto *madeForm = new QFormLayout(made);
    AddRow(madeForm, tr("Canvas width"), Cm(plan.binWidthPx * kCmPerPx));
    AddRow(madeForm, tr("Fabric length"), Cm(plan.usedHeightPx * kCmPerPx));
    if (pair)
    {
        AddRow(madeForm, tr("Lining length"), Cm(lining->usedHeightPx * kCmPerPx));
    }
    if (plan.seconds > 0.0)
    {
        AddRow(madeForm, tr("Time"), DurationText(plan.seconds));
    }
    layout->addWidget(made);

    // ── Les pièces ─────────────────────────────────────────────────────────────────────────────
    //
    // **Une pièce par ligne, et combien de fois la couper.** « Shapes laid » a été retiré : le
    // nombre de formes posées sur le tissu est un chiffre de dessinateur — le pli en double la
    // moitié, la quantité en ajoute — et il ne dit rien à qui va couper.
    //
    // ⚠️ **La quantité et le pli sont ceux de la PIÈCE, pas du plan.** Une pièce coupée dans le pli
    // s'annonce « cut 1 on the fold » que le tissu soit plié ou non : c'est ce que la pièce EST, pas
    // la façon dont elle a été posée. Les poses du plan, elles, sont déjà expansées (quantité sur
    // tissu plat, moitié sur tissu plié, jumelle du pli), donc on ne compte pas les poses : on lit
    // la PREMIÈRE pose de chaque nom, qui porte les chiffres du patron.
    // ⚠️ **Deux listes pour une paire, une seule pour un plan.** Les deux étoffes ne portent pas les
    // mêmes pièces : la doublure en compte quatre là où le tissu en compte seize, et une liste unique
    // additionnerait deux choses que la coupe sépare.
    QVector<QPair<QString, const CutPlan *>> cloths;
    cloths.append({QString(), &plan});
    if (pair)
    {
        cloths.append({tr("Lining"), lining});
    }

    auto *piecesBox = new QGroupBox(tr("Pieces"), this);
    auto *piecesLayout = new QVBoxLayout(piecesBox);
    piecesLayout->setSpacing(6);

    for (const auto &cloth : cloths)
    {
        const Material material = cloth.second == lining ? Material::Lining : Material::Fabric;
        const QVector<CutPiece> pieces = PieceList(*cloth.second);

        if (!cloth.first.isEmpty())
        {
            // The cloth's name opens its half, in the same words the figure line uses.
            auto *title = new QLabel(QStringLiteral("<b>%1</b>").arg(cloth.first), piecesBox);
            piecesLayout->addWidget(title);
        }

        piecesLayout->addWidget(new QLabel(tr("Pieces: %1").arg(pieces.size()), piecesBox));

        auto *pieceRows = new QFormLayout();
        pieceRows->setLabelAlignment(Qt::AlignLeft);
        pieceRows->setHorizontalSpacing(24);
        for (const CutPiece &piece : pieces)
        {
            auto *count = new QLabel(CutText(piece, material), piecesBox);
            count->setTextInteractionFlags(Qt::TextSelectableByMouse);
            pieceRows->addRow(piece.name, count);
        }
        piecesLayout->addLayout(pieceRows);
    }

    layout->addWidget(piecesBox);

    Finish(layout, plan.ok ? QString() : plan.error);
}

//---------------------------------------------------------------------------------------------------------------------
OutputInfoDialog::OutputInfoDialog(const PrintInfo &print, QWidget *parent) : QDialog(parent)
{
    setWindowTitle(print.title);
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(12);

    // ── Ce qui a été demandé ───────────────────────────────────────────────────────────────────
    //
    // **The paper's own settings, and nothing of the cloth.** A print run is asked for a format, how
    // many sheets side by side, and two spacings — the margin from the sheet's edge and the space
    // between two pieces. There is no fabric width, no fold and no direction here: those questions
    // belong to the cutting plan, and answering them with a zero would be answering about a plan
    // that does not exist.
    auto *asked = new QGroupBox(tr("What this output was asked for"), this);
    auto *askedForm = new QFormLayout(asked);
    AddRow(askedForm, tr("Print model"), print.format);

    // A roll — and the A0 sheet — is one column wide by definition: it has no count, so it gets no
    // line. A count that can only be one is a question with no answer in it.
    if (print.hasColumns)
    {
        AddRow(askedForm, tr("Columns"), QString::number(print.columns));
    }

    AddRow(askedForm, tr("Document margin"), Cm(print.marginCm));
    AddRow(askedForm, tr("Space between pieces"), Cm(print.gapCm));
    layout->addWidget(asked);

    // ── Ce qu'il a produit ─────────────────────────────────────────────────────────────────────
    auto *made = new QGroupBox(tr("What it produced"), this);
    auto *madeForm = new QFormLayout(made);

    // **The pages, and the cover page said out loud.** It is printed, so it counts — and a count
    // that quietly includes a page the run never laid out on the grid would be a figure she could
    // not reconcile with the canvas, which draws that page on a sheet of its own.
    AddRow(madeForm, tr("Pages"),
           print.coverPage ? tr("%n page(s), cover page included", nullptr, print.pages)
                           : tr("%n page(s)", nullptr, print.pages));

    if (print.seconds > 0.0)
    {
        AddRow(madeForm, tr("Time"), tr("%1 s").arg(print.seconds, 0, 'f', 1));
    }
    layout->addWidget(made);

    // ── Les pièces ─────────────────────────────────────────────────────────────────────────────
    //
    // **One line per piece, and how many sizes it is printed at.** A print run lays every piece at
    // every size of the range, so the piece count is the pattern's, never the number of shapes put
    // on the paper.
    auto *piecesBox = new QGroupBox(tr("Pieces"), this);
    auto *piecesLayout = new QVBoxLayout(piecesBox);
    piecesLayout->setSpacing(6);
    piecesLayout->addWidget(new QLabel(tr("Pieces: %1").arg(print.pieces.size()), piecesBox));

    if (print.sizes > 0)
    {
        piecesLayout->addWidget(new QLabel(tr("Printed for %n size(s)", nullptr, print.sizes),
                                           piecesBox));
    }

    for (const QString &name : print.pieces)
    {
        auto *row = new QLabel(name, piecesBox);
        row->setTextInteractionFlags(Qt::TextSelectableByMouse);
        piecesLayout->addWidget(row);
    }

    layout->addWidget(piecesBox);

    Finish(layout, QString());
}

//---------------------------------------------------------------------------------------------------------------------
void OutputInfoDialog::Finish(QVBoxLayout *layout, const QString &error)
{
    if (!error.isEmpty())
    {
        auto *label = new QLabel(error, this);
        label->setWordWrap(true);
        label->setObjectName(QStringLiteral("importError"));
        layout->addWidget(label);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}
