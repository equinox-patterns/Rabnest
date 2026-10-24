/************************************************************************
 **
 **  @file   busydialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   23/10/2026
 **
 **  @brief  The dialogue a run shows while it works: what it is doing, and how long for.
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

#ifndef BUSYDIALOG_H
#define BUSYDIALOG_H

#include <QDialog>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>

class QLabel;
class QProgressBar;

/// **The moment of waiting, shown and counted.**
///
/// A run takes seconds — half a minute on a real pattern — and a window that says nothing for
/// thirty seconds is thirty seconds of somebody clicking twice. It is a small modal dialogue: it
/// takes the focus, it cannot be missed, and it says what is happening and how long it has been
/// going on.
///
/// ⚠️ **The seconds tick from a timer, not from the progress.** A figure that only moves when a
/// size finishes sits still for exactly as long as somebody doubts it, so the clock runs on its
/// own, ten times a second, from the moment the dialogue opens — and the work it counts has to
/// be off this thread, or nothing repaints at all (see the callers).
class BusyDialog : public QDialog
{
    Q_OBJECT

public:
    /// @param title        the window's title — what the run is, in one word
    /// @param text         what is happening, written on the label
    /// @param progressText what the label says when a step count becomes known: `%1` is the step
    ///                     reached and `%2` the number of steps. Empty for a run that reports no
    ///                     steps at all, in which case the label never changes.
    BusyDialog(QWidget *parent, const QString &title, const QString &text,
               const QString &progressText = QString());

    /// How far the run has got: one step per unit of work. A @p total of 0 leaves the bar
    /// spinning, which is the honest state of a run that counts nothing.
    void SetProgress(int done, int total);

private:
    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
    QLabel *m_elapsed = nullptr;
    QString m_text;
    QString m_progressText;
    QElapsedTimer m_clock;
    QTimer m_tick;
};

#endif // BUSYDIALOG_H
