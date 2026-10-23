/************************************************************************
 **
 **  @file   exportimagedialog.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The four questions an image export asks — folder, format, size, resolution.
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

#include "app/exportimagedialog.h"
#include "app/factorywidgets.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

namespace
{
/// The millimetres one of @p unit is worth.
auto FactorOf(ExportImageDialog::Unit unit) -> double
{
    switch (unit)
    {
        case ExportImageDialog::Unit::Inch:
            return 25.4;
        case ExportImageDialog::Unit::Centimetre:
            return 10.0;
        default:
            return 1.0;
    }
}

/// The default resolution of a PNG. A pattern printed at 96 dpi is a thumbnail; 300 is what a
/// cartouche stays readable at, and it is Ostara's default for the same reason.
constexpr int kDefaultDpi = 300;
constexpr int kSmallestDpi = 72;
constexpr int kLargestDpi = 1200;
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
ExportImageDialog::ExportImageDialog(const PlanImage::Plan &plan, const QString &outputName,
                                     const QString &directory, QWidget *parent)
    : QDialog(parent), m_plan(plan), m_outputName(outputName)
{
    setWindowTitle(tr("Export the plan as an image"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    // ── Le dossier ─────────────────────────────────────────────────────────────────────────────
    //
    // Typed or browsed, and never guessed: the last one used is proposed, and Documents is what a
    // machine has when nothing was ever exported.
    const QString start = directory.isEmpty()
                              ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                              : directory;

    auto *folderRow = new QWidget(this);
    auto *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    m_directory = new QLineEdit(start, folderRow);
    folderLayout->addWidget(m_directory, 1);
    auto *browse = new QPushButton(tr("Browse..."), folderRow);
    browse->setCursor(Qt::PointingHandCursor);
    folderLayout->addWidget(browse);
    form->addRow(tr("Folder"), folderRow);

    connect(browse, &QPushButton::clicked, this, [this] {
        const QString chosen = QFileDialog::getExistingDirectory(this, tr("Folder"), m_directory->text());
        if (!chosen.isEmpty())
        {
            m_directory->setText(chosen);
        }
    });

    // ── Le format ──────────────────────────────────────────────────────────────────────────────
    //
    // SVG first, as in Ostara: an SVG carries the plan at its true size and stays editable, and a
    // PNG is the picture of it.
    m_format = MakeComboBox(this);
    m_format->addItem(tr("SVG"), static_cast<int>(PlanImageWriter::Format::Svg));
    m_format->addItem(tr("PNG"), static_cast<int>(PlanImageWriter::Format::Png));
    form->addRow(tr("Format"), m_format);

    // ── La taille ──────────────────────────────────────────────────────────────────────────────
    //
    // One side is given and the other follows from the plan's own shape. Asking for both would let
    // them contradict the plan, and the drawing would be stretched to hide it.
    m_axis = MakeComboBox(this);
    m_axis->addItem(tr("Width"), static_cast<int>(PlanImage::Axis::Width));
    m_axis->addItem(tr("Height"), static_cast<int>(PlanImage::Axis::Height));
    form->addRow(tr("Give"), m_axis);

    auto *sizeRow = new QWidget(this);
    auto *sizeLayout = new QHBoxLayout(sizeRow);
    sizeLayout->setContentsMargins(0, 0, 0, 0);
    m_size = new QLineEdit(sizeRow);
    sizeLayout->addWidget(m_size, 1);
    m_unit = MakeComboBox(sizeRow);
    m_unit->addItem(tr("cm"), static_cast<int>(Unit::Centimetre));
    m_unit->addItem(tr("mm"), static_cast<int>(Unit::Millimetre));
    m_unit->addItem(tr("in"), static_cast<int>(Unit::Inch));
    sizeLayout->addWidget(m_unit);
    form->addRow(tr("Size"), sizeRow);

    m_other = new QLabel(this);
    form->addRow(QString(), m_other);

    // ── La résolution ──────────────────────────────────────────────────────────────────────────
    //
    // A PNG question, and only a PNG question: an SVG is written at 96 dpi by construction, its
    // page being declared in millimetres. The row is hidden rather than disabled, so that nothing
    // on screen suggests a figure that will not be used.
    m_dpiLabel = new QLabel(tr("Resolution"), this);
    m_dpi = new QSpinBox(this);
    m_dpi->setRange(kSmallestDpi, kLargestDpi);
    m_dpi->setValue(kDefaultDpi);
    m_dpi->setSuffix(tr(" dpi"));
    form->addRow(m_dpiLabel, m_dpi);

    layout->addLayout(form);
    layout->addStretch(1);

    auto *buttons = new QDialogButtonBox(this);
    auto *exportButton = buttons->addButton(tr("Export"), QDialogButtonBox::AcceptRole);
    exportButton->setObjectName(QStringLiteral("primaryButton"));
    exportButton->setCursor(Qt::PointingHandCursor);
    auto *cancel = buttons->addButton(tr("Cancel"), QDialogButtonBox::RejectRole);
    cancel->setCursor(Qt::PointingHandCursor);
    layout->addWidget(buttons);

    connect(buttons, &QDialogButtonBox::accepted, this, &ExportImageDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &ExportImageDialog::reject);
    connect(m_format, &QComboBox::currentIndexChanged, this, &ExportImageDialog::UpdateFormat);
    connect(m_size, &QLineEdit::textEdited, this, &ExportImageDialog::UpdateSize);
    connect(m_unit, &QComboBox::currentIndexChanged, this, &ExportImageDialog::UpdateSize);
    connect(m_axis, &QComboBox::currentIndexChanged, this, &ExportImageDialog::UpdateSize);

    // The plan's width, in the unit on screen: what she is about to scale, not a round number.
    m_sizeMm = m_plan.widthMm;
    m_size->setText(QString::number(m_sizeMm / FactorOf(Unit::Centimetre), 'f', 1));

    UpdateFormat();
    UpdateSize();
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::directory() const -> QString
{
    return m_directory->text().trimmed();
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::format() const -> PlanImageWriter::Format
{
    return static_cast<PlanImageWriter::Format>(m_format->currentData().toInt());
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::axis() const -> PlanImage::Axis
{
    return static_cast<PlanImage::Axis>(m_axis->currentData().toInt());
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::targetMm() const -> double
{
    return m_sizeMm;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::dpi() const -> int
{
    return format() == PlanImageWriter::Format::Png ? m_dpi->value() : 96;
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::target() const -> PlanImage::Target
{
    return PlanImage::SizeFor(m_plan, m_sizeMm, axis(), dpi());
}

//---------------------------------------------------------------------------------------------------------------------
auto ExportImageDialog::fileName() const -> QString
{
    return QStringLiteral("%1.%2").arg(m_outputName, PlanImageWriter::Suffix(format()));
}

//---------------------------------------------------------------------------------------------------------------------
void ExportImageDialog::UpdateFormat()
{
    const bool png = format() == PlanImageWriter::Format::Png;
    m_dpiLabel->setVisible(png);
    m_dpi->setVisible(png);
}

//---------------------------------------------------------------------------------------------------------------------
void ExportImageDialog::UpdateSize()
{
    bool ok = false;
    const double typed = m_size->text().trimmed().replace(QLatin1Char(','), QLatin1Char('.')).toDouble(&ok);
    const auto unit = static_cast<Unit>(m_unit->currentData().toInt());
    m_sizeMm = ok ? typed * FactorOf(unit) : 0.0;

    if (m_sizeMm <= 0.0)
    {
        m_other->clear();
        return;
    }

    // The other side, read only: the plan keeps its shape, and a figure that follows is not a figure
    // to edit.
    const PlanImage::Target counted = target();
    m_other->setText(tr("%1 × %2 pixels").arg(counted.sizePx.width()).arg(counted.sizePx.height()));
}

//---------------------------------------------------------------------------------------------------------------------
void ExportImageDialog::accept()
{
    if (!QDir(directory()).exists())
    {
        QMessageBox::warning(this, tr("Export"), tr("That folder does not exist."));
        return;
    }

    if (m_sizeMm <= 0.0)
    {
        QMessageBox::warning(this, tr("Export"), tr("The size must be a number greater than zero."));
        return;
    }

    if (m_plan.widthMm <= 0.0 || m_plan.heightMm <= 0.0)
    {
        QMessageBox::warning(this, tr("Export"), tr("The plan cannot be written at that size."));
        return;
    }

    // A PNG is the only format whose pixels are a decision; an SVG carries its size in millimetres
    // and would be refused for a count it never uses.
    if (format() == PlanImageWriter::Format::Png && PlanImage::IsTooLarge(target()))
    {
        QMessageBox::warning(this, tr("Export"),
                             tr("That image would be too large. Lower the size or the resolution."));
        return;
    }

    QDialog::accept();
}
