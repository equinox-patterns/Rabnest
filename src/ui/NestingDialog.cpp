/************************************************************************
 **
 **  @file   NestingDialog.cpp
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

#include "ui/NestingDialog.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGroupBox>

#include <QSettings>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>

#include "io/NestedSvgWriter.h"
#include "io/SvgReader.h"
#include "nesting/Bin.h"
#include "nesting/Engine.h"
#include "nesting/Footprint.h"

#if defined(__APPLE__)
#include <pthread.h>
#include <sys/qos.h>
#endif

// ── Pipeline result (packed for background thread) ─────────────────────────

#if defined(__APPLE__)
/// Ask macOS to keep this thread on the performance cores (USER_INTERACTIVE).
/// Nesting is a single long CPU-bound thread and gets 2-4× slower when the
/// scheduler demotes it to an efficiency core while the machine is busy.
/// Must be called from the worker thread itself.
static void preferPerformanceCores()
{
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
}
#else
static void preferPerformanceCores() {}
#endif

struct NestingResult
{
    bool                     success       = false;
    QString                  error;
    Footprint::Result        fp;
    std::vector<Placement>   placements;
    double                   usedHeightMm  = 0;
    double                   elapsed       = 0;
    int                      totalPieces   = 0;
};

static NestingResult runPipeline(const QString &svgPath, double binWidthMm)
{
    NestingResult r;
    constexpr double kMmToPx = 96.0 / 25.4;

    // ── 1. Read SVG ───────────────────────────────────────────────────────
    QVector<SizeLayer> layers;
    SvgReader reader;
    if (!reader.read(svgPath, layers)) {
        r.error = QStringLiteral("Failed to parse SVG.");
        return r;
    }

    for (const auto &layer : layers)
        r.totalPieces += layer.pieces.size();

    // ── 2. Compute footprints ─────────────────────────────────────────────
    r.fp = Footprint::compute(layers);
    if (r.fp.polygons.empty()) {
        r.error = QStringLiteral("No footprints — nothing to nest.");
        return r;
    }

    // ── 3. Nesting ────────────────────────────────────────────────────────
    const double binWidthPx = binWidthMm * kMmToPx;
    const bool isA0 = (binWidthMm <= 841.0);
    const double obsWPx = isA0 ? 210.0 * kMmToPx : 0.0;
    const double obsHPx = isA0 ? 297.0 * kMmToPx : 0.0;
    Bin bin(binWidthPx, 5.0 * kMmToPx, obsWPx, obsHPx);

    Engine::Config cfg;
    cfg.gap = 5.0 * kMmToPx;
    Engine engine(cfg);

    const auto t0 = std::chrono::steady_clock::now();
    r.placements = engine.place(bin, r.fp.polygons);
    const auto t1 = std::chrono::steady_clock::now();
    r.elapsed = std::chrono::duration<double>(t1 - t0).count();

    // ── 4. Compute used height ────────────────────────────────────────────
    for (size_t i = 0; i < r.placements.size() && i < r.fp.polygons.size(); ++i) {
        int64_t xMin = (std::numeric_limits<int64_t>::max)();
        int64_t yMin = (std::numeric_limits<int64_t>::max)();
        for (const auto &pt : r.fp.polygons[i]) {
            if (pt.x < xMin) xMin = pt.x;
            if (pt.y < yMin) yMin = pt.y;
        }

        Clipper2Lib::Path64 local;
        local.reserve(r.fp.polygons[i].size());
        for (const auto &pt : r.fp.polygons[i])
            local.push_back({pt.x - xMin, pt.y - yMin});

        Clipper2Lib::Path64 rotated;
        if (r.placements[i].rotation != 0.0) {
            const double rad = r.placements[i].rotation * M_PI / 180.0;
            const double c   = std::cos(rad);
            const double s   = std::sin(rad);
            rotated.reserve(local.size());
            for (const auto &pt : local) {
                rotated.push_back({
                    static_cast<int64_t>(std::round(pt.x * c - pt.y * s)),
                    static_cast<int64_t>(std::round(pt.x * s + pt.y * c))
                });
            }
        } else {
            rotated = std::move(local);
        }

        int64_t ryMax = (std::numeric_limits<int64_t>::min)();
        for (const auto &pt : rotated) {
            if (pt.y > ryMax) ryMax = pt.y;
        }

        const double bottom = r.placements[i].y + ryMax / 1e6;
        if (bottom > r.usedHeightMm)
            r.usedHeightMm = bottom;
    }
    r.usedHeightMm += bin.marginMm();
    r.usedHeightMm *= 25.4 / 96.0;
    if (r.usedHeightMm < 100) r.usedHeightMm = 100;
    const double obstacleBottomMm = (bin.marginMm() + bin.obstacleHeightMm()) * 25.4 / 96.0;
    if (r.usedHeightMm < obstacleBottomMm) r.usedHeightMm = obstacleBottomMm;

    r.success = true;
    return r;
}

// ── Batch job descriptor ──────────────────────────────────────────────────

struct BatchJob {
    QString svgPath;
    double  binWidthMm;
    QString suffix;
};

// ── Batch worker (runs on the Qt thread pool) ─────────────────────────────
// Each job is an independent nest: same input, same bin, fresh Engine with a
// fixed per-run seed → a job's result is bit-identical whether jobs run
// sequentially or in parallel. Jobs are therefore mapped concurrently over the
// available cores (A1). A job is skipped when its output already exists and is
// not older than its input (A2) — the existing artefact is kept as-is.

struct BatchShared {
    std::atomic<int> *done   = nullptr;  // completed jobs (progress)
    int               total  = 0;
    QString          *latest = nullptr;  // status of the job that just finished
    QMutex           *mutex  = nullptr;
};

struct JobOutcome {
    bool    ok        = false;   // false → counted as a failure
    bool    skipped   = false;   // output already up to date, nothing recomputed
    QString label;               // "file.svg → 841 mm"
    QString error;               // failure reason (parse / no footprints / write)
    double  elapsed   = 0;
    double  heightMm  = 0;
    QString outputPath;
};

static JobOutcome processJob(const BatchJob &job, const QString &outputDir,
                             BatchShared *shared)
{
    JobOutcome o;
    const QFileInfo fi(job.svgPath);
    o.label = QStringLiteral("%1 → %2 mm")
                  .arg(fi.fileName()).arg(job.binWidthMm, 0, 'f', 0);
    o.outputPath = outputDir + QLatin1Char('/')
                 + fi.completeBaseName() + job.suffix + QStringLiteral(".svg");

    // A2 — skip if the artefact already exists and is not older than its input.
    if (QFileInfo::exists(o.outputPath)) {
        const QFileInfo in(job.svgPath), out(o.outputPath);
        if (out.lastModified() >= in.lastModified()) {
            o.ok      = true;
            o.skipped = true;
            return o;
        }
    }

    NestingResult r = runPipeline(job.svgPath, job.binWidthMm);
    if (!r.success) {
        o.error = r.error;   // parse / no-footprints failure text
        return o;
    }

    NestedSvgWriter writer;
    if (!writer.write(job.svgPath, o.outputPath, r.placements, r.fp,
                      job.binWidthMm, r.usedHeightMm)) {
        o.error = QStringLiteral("could not write SVG");
        return o;
    }

    o.ok       = true;
    o.elapsed  = r.elapsed;
    o.heightMm = r.usedHeightMm;
    return o;
}

/// Authoritative one-line summary for a job (used for the final error list,
/// in job order — mirrors the historical sequential messages).
static QString jobStatusLine(const JobOutcome &o, int pos, int total)
{
    if (o.ok && o.skipped)
        return QStringLiteral("[%1/%2] %3 — up to date, skipped")
                   .arg(pos).arg(total).arg(o.label);
    if (!o.ok)
        return QStringLiteral("[%1/%2] %3 — FAILED: %4")
                   .arg(pos).arg(total).arg(o.label, o.error);
    return QStringLiteral("[%1/%2] %3 — Done in %4 s, height %5 mm → %6")
               .arg(pos).arg(total).arg(o.label)
               .arg(o.elapsed, 0, 'f', 1)
               .arg(o.heightMm, 0, 'f', 0)
               .arg(o.outputPath);
}

// ── NestingDialog ───────────────────────────────────────────────────────────

NestingDialog::NestingDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Rabnest — Batch Nesting"));
    setMinimumWidth(560);

    QSettings settings;
    auto *mainLayout = new QVBoxLayout(this);

    // ── Input files ────────────────────────────────────────────────────────
    auto *fileGroup = new QGroupBox(QStringLiteral("Input — SVG files"), this);
    auto *fileGroupLayout = new QVBoxLayout(fileGroup);

    m_fileList = new QListWidget(this);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->setMaximumHeight(120);
    fileGroupLayout->addWidget(m_fileList);

    auto *fileBtnLayout = new QHBoxLayout();
    auto *browseInBtn = new QPushButton(QStringLiteral("Browse..."), this);
    auto *clearFilesBtn = new QPushButton(QStringLiteral("Clear selected"), this);
    fileBtnLayout->addWidget(browseInBtn);
    fileBtnLayout->addWidget(clearFilesBtn);
    fileBtnLayout->addStretch();
    fileGroupLayout->addLayout(fileBtnLayout);
    mainLayout->addWidget(fileGroup);

    // ── Output directory ────────────────────────────────────────────────────
    auto *outGroup = new QGroupBox(QStringLiteral("Output"), this);
    auto *outLayout = new QHBoxLayout(outGroup);
    m_outputDir = new QLineEdit(this);
    m_outputDir->setPlaceholderText(QStringLiteral("Export directory..."));
    m_outputDir->setText(settings.value(QStringLiteral("lastExportDir")).toString());
    auto *browseOutBtn = new QPushButton(QStringLiteral("Browse..."), this);
    outLayout->addWidget(m_outputDir, 1);
    outLayout->addWidget(browseOutBtn);
    mainLayout->addWidget(outGroup);

    // ── Bin selection ───────────────────────────────────────────────────────
    auto *binGroup = new QGroupBox(QStringLiteral("Bin"), this);
    auto *binLayout = new QHBoxLayout(binGroup);
    m_binA0 = new QCheckBox(QStringLiteral("841 mm (A0)"), this);
    m_binA0->setChecked(true);
    m_bin950 = new QCheckBox(QStringLiteral("950 mm"), this);
    m_bin950->setChecked(true);
    binLayout->addWidget(m_binA0);
    binLayout->addWidget(m_bin950);
    binLayout->addStretch();
    mainLayout->addWidget(binGroup);

    // ── Progress bar ────────────────────────────────────────────────────────
    m_progress = new QProgressBar(this);
    m_progress->setVisible(false);
    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    mainLayout->addWidget(m_progress);

    // ── Run button ──────────────────────────────────────────────────────────
    m_runButton = new QPushButton(QStringLiteral("Run Nesting"), this);
    mainLayout->addWidget(m_runButton);

    // ── Status ──────────────────────────────────────────────────────────────
    m_status = new QLabel(QStringLiteral("Ready."), this);
    m_status->setWordWrap(true);
    mainLayout->addWidget(m_status);

    mainLayout->addStretch();

    // ── Close ───────────────────────────────────────────────────────────────
    auto *closeBtn = new QPushButton(QStringLiteral("Close"), this);
    mainLayout->addWidget(closeBtn);

    // ── Progress timer ──────────────────────────────────────────────────────
    m_progressTimer = new QTimer(this);
    m_progressTimer->setInterval(150);
    connect(m_progressTimer, &QTimer::timeout, this, &NestingDialog::onProgressTick);

    // ── Connections ─────────────────────────────────────────────────────────
    connect(browseInBtn, &QPushButton::clicked, this, &NestingDialog::addFiles);
    connect(clearFilesBtn, &QPushButton::clicked, this, &NestingDialog::removeSelectedFiles);
    connect(browseOutBtn, &QPushButton::clicked, this, &NestingDialog::browseOutputDir);
    connect(m_runButton, &QPushButton::clicked, this, &NestingDialog::runNesting);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
}

void NestingDialog::addFiles()
{
    QSettings settings;
    const QString lastDir = settings.value(QStringLiteral("lastInputDir")).toString();
    const QStringList paths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("Select SVG files"),
        lastDir,
        QStringLiteral("SVG files (*.svg);;All files (*)"));
    if (paths.isEmpty())
        return;

    for (const QString &path : paths) {
        // Avoid duplicates
        bool found = false;
        for (int i = 0; i < m_fileList->count(); ++i) {
            if (m_fileList->item(i)->text() == path) {
                found = true;
                break;
            }
        }
        if (!found)
            m_fileList->addItem(path);
    }

    settings.setValue(QStringLiteral("lastInputDir"), QFileInfo(paths.last()).absolutePath());

    // Auto-fill output dir from first file's directory if not already set
    if (m_outputDir->text().isEmpty() && m_fileList->count() > 0) {
        const QString dir = QFileInfo(m_fileList->item(0)->text()).absolutePath();
        m_outputDir->setText(dir);
        settings.setValue(QStringLiteral("lastExportDir"), dir);
    }

    m_status->setText(QStringLiteral("%1 file(s) in list.").arg(m_fileList->count()));
}

void NestingDialog::removeSelectedFiles()
{
    QList<QListWidgetItem *> selected = m_fileList->selectedItems();
    for (QListWidgetItem *item : selected)
        delete item;
    m_status->setText(QStringLiteral("%1 file(s) in list.").arg(m_fileList->count()));
}

void NestingDialog::browseOutputDir()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("Select export directory"),
        m_outputDir->text());
    if (!dir.isEmpty())
        updateOutputDir(dir);
}

void NestingDialog::updateOutputDir(const QString &path)
{
    m_outputDir->setText(path);
    QSettings().setValue(QStringLiteral("lastExportDir"), path);
}

void NestingDialog::runNesting()
{
    // ── Validate input ──────────────────────────────────────────────────────
    if (m_fileList->count() == 0) {
        m_status->setText(QStringLiteral("Please add at least one SVG file."));
        return;
    }

    if (!m_binA0->isChecked() && !m_bin950->isChecked()) {
        m_status->setText(QStringLiteral("Please select at least one bin size."));
        return;
    }

    QString exportDir = m_outputDir->text();
    if (exportDir.isEmpty()) {
        m_status->setText(QStringLiteral("Please select an output directory."));
        return;
    }
    QSettings().setValue(QStringLiteral("lastExportDir"), exportDir);

    // ── Build job list ─────────────────────────────────────────────────────
    QVector<BatchJob> jobs;
    for (int i = 0; i < m_fileList->count(); ++i) {
        const QString &path = m_fileList->item(i)->text();
        if (!QFileInfo::exists(path)) {
            m_status->setText(QStringLiteral("File not found: ") + path);
            return;
        }
        if (m_binA0->isChecked())
            jobs.append({path, 841.0, QStringLiteral("_nestingA0")});
        if (m_bin950->isChecked())
            jobs.append({path, 950.0, QStringLiteral("_nesting")});
    }

    // ── Setup progress ─────────────────────────────────────────────────────
    m_jobIndex.store(0, std::memory_order_relaxed);
    m_jobStatus = QStringLiteral("Starting...");
    m_progress->setVisible(true);
    m_progress->setRange(0, jobs.size());
    m_progress->setValue(0);
    m_runButton->setEnabled(false);
    m_progressTimer->start();

    // ── Progress data shared with the concurrent workers ───────────────────
    auto shared = std::make_shared<BatchShared>();
    shared->done   = &m_jobIndex;
    shared->total  = jobs.size();
    shared->latest = &m_jobStatus;
    shared->mutex  = &m_statusMutex;

    // ── Run jobs in parallel on the global Qt thread pool ──────────────────
    const int totalJobs = jobs.size();
    auto *watcher = new QFutureWatcher<JobOutcome>(this);

    QFuture<JobOutcome> future =
        QtConcurrent::mapped(jobs, [exportDir, shared](const BatchJob &job) {
            preferPerformanceCores();  // keep the nest off the efficiency cores
            JobOutcome o = processJob(job, exportDir, shared.get());

            // Publish progress: number of finished jobs + the job's rich status
            // line (done in X s, height Y mm → output), like the historical UI.
            const int done = shared->done->fetch_add(1, std::memory_order_relaxed) + 1;
            QMutexLocker lock(shared->mutex);
            *shared->latest = jobStatusLine(o, done, shared->total);
            return o;
        });

    connect(watcher, &QFutureWatcher<JobOutcome>::finished, this,
        [this, watcher, shared]()
    {
        watcher->deleteLater();
        m_progressTimer->stop();
        m_progress->setVisible(false);
        m_runButton->setEnabled(true);

        // Final tick to show last status
        onProgressTick();

        // Recap of every job, in input order (results() preserves order):
        // one line per file/bin with its time, height and output path.
        const QList<JobOutcome> outcomes = watcher->future().results();
        const int total  = outcomes.size();
        int failed = 0;
        QStringList recap;
        recap.reserve(total + 1);
        int i = 0;
        for (const JobOutcome &o : outcomes) {
            if (!o.ok)
                ++failed;
            recap.append(jobStatusLine(o, i + 1, total));
            ++i;
        }

        const QString header = (failed == 0)
            ? QStringLiteral("All %1 job(s) completed successfully.").arg(total)
            : QStringLiteral("%1/%2 job(s) failed.").arg(failed).arg(total);
        recap.prepend(header);
        m_status->setText(recap.join(QLatin1Char('\n')));
    });
    watcher->setFuture(future);
}

void NestingDialog::onProgressTick()
{
    const int idx = m_jobIndex.load(std::memory_order_relaxed);
    m_progress->setValue(idx);

    QString status;
    {
        QMutexLocker lock(&m_statusMutex);
        status = m_jobStatus;
    }
    if (!status.isEmpty())
        m_status->setText(status);
}

void NestingDialog::closeEvent(QCloseEvent *event)
{
    if (m_progressTimer->isActive()) {
        event->ignore();
        return;
    }
    event->accept();
}
