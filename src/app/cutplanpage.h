/************************************************************************
 **
 **  @file   cutplanpage.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Which sizes to lay out, and the cloth they will be laid on.
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

#ifndef CUTPLANPAGE_H
#define CUTPLANPAGE_H

#include "core/cutplanrequest.h"

#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWidget>

class QCheckBox;
class QFormLayout;
class QGroupBox;
class QTabWidget;
class PrintPanel;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QVBoxLayout;

/// Which sizes get a plan, and the cloth they are laid on.
///
/// **A cutting plan is laid for ONE size** — that is what a plan is, and Ostara's own tab says so.
/// Asking for three gives three plans, not one plan over three sizes. So the sizes are ticked here
/// and not assumed: a size kept in the first step is not necessarily a size she wants to lay out.
class CutPlanPage : public QWidget
{
    Q_OBJECT

public:
    explicit CutPlanPage(QWidget *parent = nullptr);

    /// The sizes the first step kept, as (id, label). Every one starts ticked — the common case is
    /// laying out all of them, and asking her to tick four sizes to say « all » is a step nobody
    /// needed.
    void SetSizes(const QVector<QPair<QString, QString>> &sizes);

    /// The ids of the sizes to lay out, in list order.
    QStringList CheckedSizeIds() const;

    /// What the page currently says about the cloth.
    CutPlanRequest Request() const;

    /// Ce que le PDF d'où vient le document EST — une projection, ou autre chose.
    ///
    /// **Ce n'est pas de la décoration.** Le cartouche d'une pièce est un DESSIN : ses mots sont des
    /// contours, donc Rabnest ne peut pas changer « Cut 2 » en « Cut 1 » — il ne saurait même pas où
    /// est le chiffre. Le nombre de coupes écrit sur la page est celui du format d'où le PDF vient, et
    /// la seule chose honnête est de le dire là où il ne correspond pas au run demandé.
    void SetDocumentKind(bool projector);

    /// Says why a plan could not be laid out, under the list.
    void ShowError(const QString &message);
    void ClearError();

    /// Which generation the button will make: the cutting plan, the print, or the projection.
    /// The tabs are the user's choice, and the button above them obeys it.
    ///
    /// ⚠️ **`Projector` remplace le `Plotter`**, qui n'a jamais existé : le plan pour plotter
    /// domestique ne sera pas porté dans Rabnest, et sa place est prise par la projection.
    enum class Tab
    {
        Cutting,
        Print,
        Projector
    };

    auto ActiveTab() const -> Tab;

    /// The print settings of this run, when the Print tab is the one being generated.
    auto PrintSettings() const -> PrintPanel *;

    /// Les deux écarts d'une projection, en centimètres : celui du bord, et celui entre deux pièces.
    ///
    /// **Pas de laize, pas de matière, pas de page de garde** — une projection n'a aucun des trois :
    /// sa page est celle que la recherche trouve, et le tissu n'existe pas. Ce qui reste à décider est
    /// donc ce qui se mesure sur la page, et rien d'autre.
    struct ProjectorSettings
    {
        double marginCm = 0.0;
        double gapCm = 10.0;
    };

    auto Projector() const -> ProjectorSettings;

    /// Whether this run also lays out the **lining** — the second plan of a pair.
    ///
    /// A lined garment is cut twice: once in the fabric, once in the lining. The run makes both
    /// plans, and nothing separates them but the piece set and the width — the fold, the direction,
    /// the margin and the gap are the run's own and belong to both. So the page asks for ONE thing
    /// more, the lining's width, and only when the pattern has a piece cut in the lining.
    auto IsLining() const -> bool;

    /// The lining's own width, in centimetres. **It is the fabric's until she gives it one**: a
    /// lining is bought on its own bolt, and until that bolt is named it is the same one.
    auto LiningWidthCm() const -> double;

    /// Says whether a lining plan is at all possible — the pattern has a piece cut in the lining.
    ///
    /// The box is ticked the moment it becomes possible, and only then: a box that re-ticked itself
    /// on every visit would put back a cross she has just taken off. When it cannot be, it is off
    /// and disabled — offering a lining plan for a garment that has none is a question with no
    /// answer in it.
    void SetLiningAvailable(bool available);

signals:
    void Continue();
    void Back();

protected:
    /// The interface has changed language: every word of the three tabs is set again — headings,
    /// the field labels the form layouts made, the two format warnings, the tab titles, the two
    /// suffixes of the width fields.
    ///
    /// **The form labels are found back through their field**, because the layouts made them: they
    /// were never in a member, and a label nobody holds is a label nobody can re-say.
    void changeEvent(QEvent *event) override;

private:
    void Retranslate();

    void UpdateContinue();
    void UpdateGenerateText();
    void UpdateLiningRow();

    QVector<QPair<QString, QCheckBox *>> m_boxes;  // id -> box, in list order
    QVBoxLayout *m_boxesLayout = nullptr;
    QWidget *m_boxesBlock = nullptr;
    QFormLayout *m_form = nullptr;
    QLabel *m_error = nullptr;
    QDoubleSpinBox *m_width = nullptr;
    QDoubleSpinBox *m_margin = nullptr;
    QDoubleSpinBox *m_gap = nullptr;
    QCheckBox *m_folded = nullptr;
    QCheckBox *m_oneWay = nullptr;
    QCheckBox *m_lining = nullptr;
    QDoubleSpinBox *m_liningWidth = nullptr;
    QPushButton *m_continue = nullptr;

    /// Whether the lining width was ever given a value of her own. Until it is, the field follows
    /// the fabric's, so that ticking « Lining » on the same bolt asks nothing twice.
    bool m_liningEdited = false;

    /// Whether a lining plan was possible the last time the page looked. The box is ticked when it
    /// BECOMES possible, not while it is — see SetLiningAvailable.
    bool m_liningWasAvailable = false;

    QTabWidget *m_tabs = nullptr;
    PrintPanel *m_print = nullptr;
    QDoubleSpinBox *m_projectorMargin = nullptr;
    QDoubleSpinBox *m_projectorGap = nullptr;

    /// The projector's form, for the two labels it made.
    QFormLayout *m_projectorForm = nullptr;

    // ── The words, kept so they can be said again ────────────────────────────────────────────────
    //
    // A label a language change must move has to be reachable: a local built once in the constructor
    // is a word nobody can ever write again.
    QLabel *m_again = nullptr;
    QLabel *m_cutTitle = nullptr;
    QLabel *m_cutSubtitle = nullptr;
    QLabel *m_foldedHint = nullptr;
    QLabel *m_oneWayHint = nullptr;
    QLabel *m_printTitle = nullptr;
    QLabel *m_printSubtitle = nullptr;
    QLabel *m_projectorTitle = nullptr;
    QLabel *m_projectorSubtitle = nullptr;
    QLabel *m_projectorNote = nullptr;

    /// The bold warning inside each format box, kept to be re-written.
    QLabel *m_printSourceNote = nullptr;
    QLabel *m_projectorSourceNote = nullptr;

    QPushButton *m_back = nullptr;

    /// Les deux avertissements de format, chacun dans sa boîte : celui de l'onglet Projecteur quand
    /// le PDF n'a pas été exporté pour une projection, celui de l'onglet Print quand il l'a été.
    ///
    /// **Une boîte et du gras, parce que c'est une chose qu'on ne doit pas manquer.** Le cartouche
    /// d'une pièce est un dessin : son chiffre de coupe ne se réécrit pas, et un lecteur qui ne voit
    /// pas l'avertissement croit le plan faux alors qu'il est seulement d'un autre format.
    QGroupBox *m_projectorSourceBox = nullptr;
    QGroupBox *m_printSourceBox = nullptr;
};

#endif  // CUTPLANPAGE_H
