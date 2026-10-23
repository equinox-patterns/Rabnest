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
 **  All rights reserved.
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
