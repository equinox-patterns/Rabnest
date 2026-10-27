/************************************************************************
 **
 **  @file   NestingDialog.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   26/05/2026
 **
 **  @brief  Batch modal UI — import multiple SVGs, choose bins, run nesting,
 **          export all SVGs automatically.
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

#ifndef NESTINGDIALOG_H
#define NESTINGDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMutex>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>

#include <atomic>

class NestingDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NestingDialog(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void addFiles();
    void removeSelectedFiles();
    void browseOutputDir();
    void runNesting();
    void onProgressTick();

private:
    void updateOutputDir(const QString &path);

    QListWidget *m_fileList = nullptr;
    QLineEdit   *m_outputDir = nullptr;
    QCheckBox   *m_binA0     = nullptr;
    QCheckBox   *m_bin950    = nullptr;
    QPushButton *m_runButton = nullptr;
    QProgressBar *m_progress = nullptr;
    QLabel      *m_status    = nullptr;

    // Batch progress (written by worker thread, read by UI timer)
    std::atomic<int> m_jobIndex;
    QString      m_jobStatus;
    QMutex       m_statusMutex;
    QTimer      *m_progressTimer = nullptr;
};

#endif // NESTINGDIALOG_H
