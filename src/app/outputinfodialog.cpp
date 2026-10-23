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
auto CutText(const CutPiece &piece) -> QString
{
    return piece.onFold ? QObject::tr("Cut %1 on the fold").arg(piece.cutFabric)
                        : QObject::tr("Cut %1").arg(piece.cutFabric);
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
OutputInfoDialog::OutputInfoDialog(const CutPlan &plan, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("%1 - Cut").arg(plan.sizeLabel));
    setModal(true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(12);

    // ── Ce qui a été demandé ───────────────────────────────────────────────────────────────────
    auto *asked = new QGroupBox(tr("What this output was asked for"), this);
    auto *askedForm = new QFormLayout(asked);
    AddRow(askedForm, tr("Fabric width"), Cm(plan.fabricWidthCm));
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
    if (plan.seconds > 0.0)
    {
        AddRow(madeForm, tr("Time"), tr("%1 s").arg(plan.seconds, 0, 'f', 1));
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

    auto *piecesBox = new QGroupBox(tr("Pieces"), this);
    auto *piecesLayout = new QVBoxLayout(piecesBox);
    piecesLayout->setSpacing(6);

    piecesLayout->addWidget(new QLabel(tr("Pieces: %1").arg(pieces.size()), piecesBox));

    auto *pieceRows = new QFormLayout();
    pieceRows->setLabelAlignment(Qt::AlignLeft);
    pieceRows->setHorizontalSpacing(24);
    for (const CutPiece &piece : pieces)
    {
        auto *count = new QLabel(CutText(piece), piecesBox);
        count->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pieceRows->addRow(piece.name, count);
    }
    piecesLayout->addLayout(pieceRows);

    layout->addWidget(piecesBox);

    if (!plan.ok)
    {
        auto *error = new QLabel(plan.error, this);
        error->setWordWrap(true);
        error->setObjectName(QStringLiteral("importError"));
        layout->addWidget(error);
    }

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}
