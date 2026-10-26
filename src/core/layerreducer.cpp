/************************************************************************
 **
 **  @file   layerreducer.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Cutting layers OUT of a PDF somebody else wrote.
 **
 **  @copyright
 **
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

#include "layerreducer.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStringList>
#include <zlib.h>

namespace
{
/// Inflate a PDF Flate stream: a zlib stream with no length to trust, so it is grown until it ends.
auto Inflate(const QByteArray &compressed, QByteArray &out) -> bool
{
    z_stream stream{};
    if (inflateInit(&stream) != Z_OK)
    {
        return false;
    }

    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(compressed.constData()));
    stream.avail_in = static_cast<uInt>(compressed.size());

    out.clear();
    QByteArray chunk(256 * 1024, Qt::Uninitialized);
    int status = Z_OK;
    do
    {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = inflate(&stream, Z_NO_FLUSH);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
        {
            inflateEnd(&stream);
            return false;
        }
        out.append(chunk.constData(), static_cast<int>(chunk.size() - stream.avail_out));
    } while (status != Z_STREAM_END && (stream.avail_in > 0 || stream.avail_out == 0));

    inflateEnd(&stream);

    // ⚠️ **A real PDF stream can stop short of its own checksum.** The A4 plan of a customer does
    // exactly that — two bytes missing, declared `/Length` included — and every reader draws what is
    // there; only a strict decoder refuses, which blocked the whole file over one page. The data is
    // therefore kept when the input ran out mid-stream: a corrupt stream still fails, a truncated one
    // does not.
    return status == Z_STREAM_END || (status != Z_DATA_ERROR && !out.isEmpty());
}

/// Deflate to a PDF Flate stream — a zlib stream, which is what /FlateDecode is.
auto Deflate(const QByteArray &data, QByteArray &out) -> bool
{
    z_stream stream{};
    if (deflateInit(&stream, 9) != Z_OK)
    {
        return false;
    }

    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    stream.avail_in = static_cast<uInt>(data.size());

    out.clear();
    QByteArray chunk(256 * 1024, Qt::Uninitialized);
    int status = Z_OK;
    do
    {
        stream.next_out = reinterpret_cast<Bytef *>(chunk.data());
        stream.avail_out = static_cast<uInt>(chunk.size());
        status = deflate(&stream, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END && status != Z_BUF_ERROR)
        {
            deflateEnd(&stream);
            return false;
        }
        out.append(chunk.constData(), static_cast<int>(chunk.size() - stream.avail_out));
    } while (status != Z_STREAM_END);

    deflateEnd(&stream);
    return true;
}

auto PdfText(const QByteArray &data) -> QString
{
    return QString::fromLatin1(data);
}

/// One definition of an object: where its `N 0 obj` head ends, and where its closing `endobj` starts.
struct ObjectRange
{
    int headEnd = -1;
    int endobj = -1;
};

/// Every definition of object @p number, in file order.
///
/// ⚠️ **`indexOf("12 0 obj")` matches inside `112 0 obj`**, and the range then belongs to somebody
/// else — a rewrite once pointed the cross-reference at the wrong object that way. The head is
/// therefore looked for where only a head can be: at the start of a line. @p text is the same
/// document as @p pdf, decoded by PdfText, so that indices and byte offsets agree.
///
/// ⚠️ **A stream's data is binary, and it may contain the bytes `endobj` itself.** Cutting at the
/// first `endobj` truncated such a stream by a few bytes, and it then refused to inflate — a tiled
/// A4 plan has thirty-four streams, and one of them is enough to block the whole file. A stream is
/// therefore delimited the way the format delimits it: by its `/Length`, checked against `endstream`.
auto ObjectRanges(const QByteArray &pdf, const QString &text, int number) -> QVector<ObjectRange>
{
    static const QRegularExpression lengthEntry(QStringLiteral("/Length\\s+(\\d+)"));

    QVector<ObjectRange> ranges;
    const QRegularExpression head(QStringLiteral("(?:^|[\\r\\n])%1\\s+0\\s+obj").arg(number));
    for (QRegularExpressionMatchIterator it = head.globalMatch(text); it.hasNext();)
    {
        const QRegularExpressionMatch match = it.next();
        ObjectRange range;
        range.headEnd = match.capturedEnd();

        const int endobj = text.indexOf(QLatin1String("endobj"), range.headEnd);
        const int streamAt = pdf.indexOf("stream", range.headEnd);
        if (streamAt >= 0 && (endobj < 0 || streamAt < endobj))
        {
            int dataAt = streamAt + 6;
            while (dataAt < pdf.size() && (pdf.at(dataAt) == '\r' || pdf.at(dataAt) == '\n'))
            {
                ++dataAt;
            }

            int endstreamAt = -1;
            const QRegularExpressionMatch length =
                lengthEntry.match(text.mid(range.headEnd, streamAt - range.headEnd));
            if (length.hasMatch())
            {
                const int after = dataAt + length.captured(1).toInt();
                const int declared = text.indexOf(QLatin1String("endstream"), after);
                if (declared >= 0 && declared - after <= 3)
                {
                    endstreamAt = declared;
                }
            }
            if (endstreamAt < 0)
            {
                endstreamAt = text.indexOf(QLatin1String("endstream"), dataAt);
            }
            if (endstreamAt < 0)
            {
                continue;
            }
            range.endobj = text.indexOf(QLatin1String("endobj"), endstreamAt + 9);
        }
        else
        {
            range.endobj = endobj;
        }

        if (range.endobj > range.headEnd)
        {
            ranges.append(range);
        }
    }
    return ranges;
}

/// The body of object @p number — everything its head opens, up to `endobj` — or empty.
auto ObjectBody(const QByteArray &pdf, const QString &text, int number) -> QByteArray
{
    const QVector<ObjectRange> ranges = ObjectRanges(pdf, text, number);
    if (ranges.isEmpty())
    {
        return {};
    }
    return pdf.mid(ranges.first().headEnd, ranges.first().endobj - ranges.first().headEnd);
}

/// The byte range of the `<< … >>` dictionary starting at or after @p from, nesting counted.
///
/// ⚠️ **A regex cannot find the end of /OCProperties.** The entry is followed by `/D << … >>` and a
/// nested `/AS [<< … >>]`, so the first `>>` is never the entry's own, and the value may name an
/// array out of line (`/Order 30 0 R`). Counting the nesting is the only shape that reads both. The
/// returned value is the dictionary's start, @p end the offset just past its final `>>`, or -1.
auto DictionaryBounds(const QString &text, int from, int &end) -> int
{
    const int begin = text.indexOf(QLatin1String("<<"), from);
    if (begin < 0)
    {
        return -1;
    }

    int depth = 0;
    for (int i = begin; i + 1 < text.size(); ++i)
    {
        if (text.at(i) == QLatin1Char('<') && text.at(i + 1) == QLatin1Char('<'))
        {
            ++depth;
            ++i;
            continue;
        }
        if (text.at(i) == QLatin1Char('>') && text.at(i + 1) == QLatin1Char('>'))
        {
            ++i;
            if (--depth == 0)
            {
                end = i + 1;
                return begin;
            }
        }
    }

    return -1;
}

/// Removes every reference to one of @p numbers from a PDF text, so the array that holds it can be
/// written back without them. The look-arounds are what keep object 21 apart from object 121.
void DropReferences(QString &text, const QVector<int> &numbers)
{
    for (const int number : numbers)
    {
        const QRegularExpression reference(QStringLiteral("(?<![0-9])%1(?![0-9])\\s+0\\s+R").arg(number));
        text.remove(reference);
    }
}

/// The object numbers of the document's optional content groups, in the order the file writes them.
auto OcgNumbers(const QByteArray &pdf) -> QVector<int>
{
    static const QRegularExpression ocg(QStringLiteral("(\\d+)\\s+0\\s+obj\\s*<<[^>]*?/Type\\s*/OCG"));
    QVector<int> numbers;
    QRegularExpressionMatchIterator it = ocg.globalMatch(PdfText(pdf));
    while (it.hasNext())
    {
        numbers.append(it.next().captured(1).toInt());
    }
    return numbers;
}

/// The /Name of an /OCG object, or empty.
auto LayerName(const QByteArray &body) -> QString
{
    static const QRegularExpression name(QStringLiteral("/Name\\s*\\(([^)]*)\\)"));
    const QRegularExpressionMatch match = name.match(PdfText(body));
    return match.hasMatch() ? match.captured(1) : QString();
}

/// The object number a page's /Properties names as /MCn, or 0.
auto PropertyObject(const QByteArray &page, const QString &mc) -> int
{
    static const QRegularExpression properties(QStringLiteral("/Properties\\s*<<(.*?)>>"),
                                              QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch block = properties.match(PdfText(page));
    if (!block.hasMatch())
    {
        return 0;
    }

    static const QRegularExpression pattern(QStringLiteral("/(MC\\d+)\\s+(\\d+)\\s+0\\s+R"));
    QRegularExpressionMatchIterator it = pattern.globalMatch(block.captured(1));
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        if (match.captured(1) == mc)
        {
            return match.captured(2).toInt();
        }
    }

    return 0;
}

/// One top-level marked-content section of a page stream: where it starts, where it ends, and which
/// /MCn it belongs to.
struct Section
{
    int begin = 0;
    int end = 0;
    QString mc;
};

/// Splits a page stream into its top-level `/OC /MCn BDC … EMC` sections, by depth, and reports what
/// lies between them (so the caller can rebuild the stream in order).
auto Sections(const QByteArray &stream, QVector<Section> &out) -> bool
{
    const QString text = PdfText(stream);
    static const QRegularExpression token(QStringLiteral("\\b(BDC|EMC)\\b"));
    static const QRegularExpression marker(QStringLiteral("/OC\\s*/(MC\\d+)\\s*$"));

    QVector<QPair<int, QString>> stack;
    QRegularExpressionMatchIterator it = token.globalMatch(text);
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        if (match.captured(1) == QLatin1String("BDC"))
        {
            const QString before = text.left(match.capturedStart());
            const QRegularExpressionMatch found = marker.match(before);
            stack.append({match.capturedStart(), found.hasMatch() ? found.captured(1) : QString()});
            continue;
        }

        if (stack.isEmpty())
        {
            // A restore without a begin: the stream is not the shape this understands.
            return false;
        }

        const QPair<int, QString> begin = stack.takeLast();
        if (stack.isEmpty())
        {
            Section section;
            section.begin = begin.first;
            section.end = match.capturedEnd();
            section.mc = begin.second;
            out.append(section);
        }
    }

    return stack.isEmpty() && !out.isEmpty();
}

/// The graphics state a section leaves behind: `q` opens one, `Q` closes one. A block that opens
/// one more than it closes must be replaced by a dummy `q`, or the stream would restore a state
/// nobody saved — which is exactly what poppler refused in the prototype.
auto DepthDelta(const QByteArray &block) -> int
{
    const QString text = PdfText(block);
    static const QRegularExpression open(QStringLiteral("(?<![\\w/])q(?=\\s)"));
    static const QRegularExpression close(QStringLiteral("(?<![\\w/])Q(?=\\s)"));
    return text.count(open) - text.count(close);
}
/// Writes the new bytes of @p bodies where the objects already stand, at CONSTANT length.
///
/// ⚠️ **An incremental update is not enough.** The original document stays whole ahead of it, and a
/// reader that stops at the first `%%EOF` — Illustrator does — sees none of it: it reopens the full
/// plan, removed layers and all. Writing the objects where they stand removes the old definition
/// from the file, keeps every offset of the cross-reference true, and makes a flat re-read and a
/// cross-reference read agree.
///
/// @returns false when a new definition is longer than the one it replaces — which happens when the
/// user keeps nearly every layer, and leaves the caller its incremental update.
auto PatchInPlace(const QByteArray &pdf, const QString &text, const QHash<int, QByteArray> &bodies,
                  QByteArray &out) -> bool
{
    out = pdf;
    for (auto it = bodies.constBegin(); it != bodies.constEnd(); ++it)
    {
        const QVector<ObjectRange> ranges = ObjectRanges(pdf, text, it.key());
        if (ranges.isEmpty())
        {
            return false;
        }

        for (const ObjectRange &range : ranges)
        {
            QByteArray bytes = it.value();
            bytes.append("endobj");

            const int span = range.endobj + 6 - range.headEnd;
            if (bytes.size() > span)
            {
                out = pdf;
                return false;
            }

            // The slack is blanks after `endobj`, which the format ignores: every offset after it
            // stays where the cross-reference says it is.
            bytes.append(QByteArray(span - bytes.size(), ' '));
            out.replace(range.headEnd, span, bytes);
        }
    }
    return true;
}

}  // namespace

//---------------------------------------------------------------------------------------------------------------------
auto LayerReducer::Read(const QString &path, QString *error) -> QVector<PdfLayer>
{
    QVector<PdfLayer> layers;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QObject::tr("The PDF cannot be opened.");
        }
        return layers;
    }

    const QByteArray pdf = file.readAll();
    file.close();
    const QString text = PdfText(pdf);

    static const QRegularExpression ocg(QStringLiteral("(\\d+)\\s+0\\s+obj\\s*<<[^>]*?/Type\\s*/OCG"));
    QRegularExpressionMatchIterator it = ocg.globalMatch(text);
    QHash<int, QString> nameOf;
    QVector<int> found;
    QSet<QString> seen;
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const int number = match.captured(1).toInt();
        const QString name = LayerName(ObjectBody(pdf, text, number));
        if (name.isEmpty() || seen.contains(name))
        {
            continue;
        }
        seen.insert(name);
        nameOf.insert(number, name);
        found.append(number);
    }

    // ── L'ordre du PANNEAU, et pas celui du fichier ─────────────────────────────────────────────
    //
    // ⚠️ **An OCG's object number says when it was WRITTEN, never where a reader shows it.** The
    // panel is drawn from the catalogue's `/Order` array, and the two disagree the moment a writer
    // creates the objects in one direction and lists them in the other — which is exactly what
    // happens here: the objects come out largest-first, `/Order` reads smallest-first, and reading
    // them in file order listed the sizes backwards on screen.
    //
    // ⚠️ **`/Order` is often written OUT OF LINE** (`/Order 163 0 R`, which is what Illustrator and
    // Acrobat write), and a pattern that only reads `/Order[…]` then finds nothing and silently falls
    // back to file order — which put « Mise en page » at the bottom of the list instead of the top.
    // The array is therefore resolved either way, and its own `/Order` object is read as an object.
    //
    // `/Order` names them by object number, and may nest arrays for grouped layers; whatever it does
    // not name keeps its place at the end, in the order it was found. A file that states no `/Order`
    // has no panel order to honour and keeps the order it was read in.
    static const QRegularExpression orderArray(QStringLiteral("/Order\\s*\\[([^\\]]*)\\]"));
    static const QRegularExpression orderReference(QStringLiteral("/Order\\s+(\\d+)\\s+0\\s+R"));

    QString ordered;
    const QRegularExpressionMatch direct = orderArray.match(text);
    if (direct.hasMatch())
    {
        ordered = direct.captured(1);
    }
    else
    {
        const QRegularExpressionMatch indirect = orderReference.match(text);
        if (indirect.hasMatch())
        {
            ordered = PdfText(ObjectBody(pdf, text, indirect.captured(1).toInt()));
        }
    }

    QVector<int> order;
    if (!ordered.isEmpty())
    {
        static const QRegularExpression reference(QStringLiteral("(\\d+)\\s+0\\s+R"));
        QRegularExpressionMatchIterator refs = reference.globalMatch(ordered);
        while (refs.hasNext())
        {
            const int number = refs.next().captured(1).toInt();
            if (nameOf.contains(number) && !order.contains(number))
            {
                order.append(number);
            }
        }
    }
    for (const int number : found)
    {
        if (!order.contains(number))
        {
            order.append(number);
        }
    }

    for (const int number : order)
    {
        layers.append({number, nameOf.value(number)});
    }

    if (layers.isEmpty() && error != nullptr)
    {
        *error = QObject::tr("This PDF carries no layer.");
    }

    return layers;
}

//---------------------------------------------------------------------------------------------------------------------
auto LayerReducer::Reduce(const QString &path, const QString &target, const QVector<int> &keep,
                          QString *error) -> bool
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        if (error != nullptr)
        {
            *error = QObject::tr("The PDF cannot be opened.");
        }
        return false;
    }
    QByteArray pdf = file.readAll();
    file.close();

    const int startxrefAt = pdf.lastIndexOf("startxref");
    if (startxrefAt < 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF has no readable cross-reference table.");
        }
        return false;
    }
    // ⚠️ **A PDF may end its lines with `\r` alone**, and `split('\n')` then swallows the `%%EOF`
    // marker with the offset: `/Prev 0` pointed at the beginning of the file, which no reader
    // accepts. The digits are read as digits.
    int startxrefCursor = startxrefAt + 9;
    while (startxrefCursor < pdf.size()
           && (pdf.at(startxrefCursor) == '\r' || pdf.at(startxrefCursor) == '\n'
               || pdf.at(startxrefCursor) == ' '))
    {
        ++startxrefCursor;
    }
    int startxrefDigits = startxrefCursor;
    while (startxrefDigits < pdf.size() && pdf.at(startxrefDigits) >= '0' && pdf.at(startxrefDigits) <= '9')
    {
        ++startxrefDigits;
    }
    const int startxref = pdf.mid(startxrefCursor, startxrefDigits - startxrefCursor).toInt();

    if (startxref <= 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF has no readable cross-reference table.");
        }
        return false;
    }

    // ── Toutes les pages, et pas la première ────────────────────────────────────────────────────
    //
    // ⚠️ **Un plan tuilé est un PDF de plusieurs dizaines de pages**, chacune avec son propre flux :
    // réduire la seule page trouvée en premier laissait toutes les autres intactes, calques retirés
    // compris — le panneau perdait bien le calque, mais le papier le portait encore. Chaque page est
    // donc réduite, et un flux n'est réémis que s'il y perdait réellement quelque chose.
    const QString text = PdfText(pdf);

    static const QRegularExpression objectHead(QStringLiteral("(?:^|[\\r\\n])(\\d+)\\s+0\\s+obj"));
    static const QRegularExpression pageType(QStringLiteral("/Type\\s*/Page[\\s/>]"));
    static const QRegularExpression pagesType(QStringLiteral("/Type\\s*/Pages"));
    static const QRegularExpression contentsEntry(QStringLiteral("/Contents\\s*(\\[[^\\]]*\\]|\\d+\\s+0\\s+R)"));
    static const QRegularExpression reference(QStringLiteral("(\\d+)\\s+0\\s+R"));
    static const QRegularExpression length(QStringLiteral("/Length\\s+(\\d+)"));

    QSet<int> kept;
    for (int object : keep)
    {
        kept.insert(object);
    }

    QVector<QPair<int, QByteArray>> reduced;  // the content object, and its new stream, compressed
    QHash<int, QByteArray> bodies;           // every object this file rewrites, new body by number
    int removed = 0;

    for (QRegularExpressionMatchIterator heads = objectHead.globalMatch(text); heads.hasNext();)
    {
        const QRegularExpressionMatch head = heads.next();
        const QByteArray page = ObjectBody(pdf, text, head.captured(1).toInt());
        const QString pageText = PdfText(page);
        if (!pageType.match(pageText).hasMatch() || pagesType.match(pageText).hasMatch())
        {
            continue;
        }

        // ── L'artwork natif d'Illustrator ────────────────────────────────────────────────────────
        //
        // ⚠️ **Illustrator ne relit pas les pages : il restaure son propre dessin.** Un PDF enregistré
        // avec « Preserve Illustrator Editing Capabilities » porte à chaque page
        // `/PieceInfo << /Illustrator N 0 R >>`, et cet objet range l'artwork complet dans des flux
        // `AIPDFPrivateData`. Tant que cette entrée est là, Illustrator rouvre les calques retirés —
        // quoi qu'on fasse aux flux de page. Elle est donc retirée : le fichier ne garde qu'un PDF,
        // et les autres lecteurs, qui l'ignoraient, ne voient aucune différence.
        QString plainPage = pageText;
        for (int at = plainPage.indexOf(QLatin1String("/PieceInfo")); at >= 0;)
        {
            int gap = at + 10;
            while (gap < plainPage.size() && plainPage.at(gap).isSpace())
            {
                ++gap;
            }
            int entryEnd = 0;
            if (plainPage.mid(gap, 2) != QLatin1String("<<")
                || DictionaryBounds(plainPage, gap, entryEnd) < 0)
            {
                break;
            }
            plainPage.remove(at, entryEnd - at);
        }
        if (plainPage != pageText)
        {
            bodies.insert(head.captured(1).toInt(), plainPage.toLatin1());
        }

        // The drawing of a page: one stream, or an array of them. A page without any is a blank page
        // and holds no layer.
        const QRegularExpressionMatch contents = contentsEntry.match(pageText);
        if (!contents.hasMatch())
        {
            continue;
        }

        QVector<int> streams;
        for (QRegularExpressionMatchIterator refs = reference.globalMatch(contents.captured(1));
             refs.hasNext();)
        {
            streams.append(refs.next().captured(1).toInt());
        }

        for (const int contentsObject : streams)
        {
            const QByteArray streamBody = ObjectBody(pdf, text, contentsObject);
            const QRegularExpressionMatch lengthMatch = length.match(PdfText(streamBody));
            const int streamAt = streamBody.indexOf("stream");
            const bool flate = streamBody.indexOf("FlateDecode") >= 0;
            if (!lengthMatch.hasMatch() || streamAt < 0)
            {
                if (error != nullptr)
                {
                    *error = QObject::tr("This PDF is not in a shape Rabnest can reduce.");
                }
                return false;
            }

            int dataAt = streamAt + 6;
            while (dataAt < streamBody.size()
                   && (streamBody.at(dataAt) == '\r' || streamBody.at(dataAt) == '\n'))
            {
                ++dataAt;
            }

            const QByteArray raw = streamBody.mid(dataAt, lengthMatch.captured(1).toInt());
            QByteArray stream = raw;
            if (flate && !Inflate(raw, stream))
            {
                if (error != nullptr)
                {
                    *error = QObject::tr("The stream of this page could not be read.");
                }
                return false;
            }

            QVector<Section> sections;
            if (!Sections(stream, sections))
            {
                if (error != nullptr)
                {
                    *error = QObject::tr("This PDF is not in a shape Rabnest can reduce.");
                }
                return false;
            }

            QByteArray rebuilt;
            int position = 0;
            int lost = 0;
            for (const Section &section : sections)
            {
                rebuilt.append(stream.mid(position, section.begin - position));

                if (kept.contains(PropertyObject(page, section.mc)))
                {
                    rebuilt.append(stream.mid(section.begin, section.end - section.begin));
                }
                else
                {
                    // ⚠️ Un bloc qui ouvre un état graphique sans le refermer emporte le reste de la
                    // page : le `q`/`Q` de compensation rend au flux l'équilibre qu'il avait.
                    const int delta = DepthDelta(stream.mid(section.begin, section.end - section.begin));
                    if (delta > 0)
                    {
                        rebuilt.append(QByteArray("q\n").repeated(delta));
                    }
                    else if (delta < 0)
                    {
                        rebuilt.append(QByteArray("Q\n").repeated(-delta));
                    }
                    ++lost;
                }
                position = section.end;
            }
            rebuilt.append(stream.mid(position));

            if (lost == 0)
            {
                continue;
            }

            QByteArray compressed;
            if (!Deflate(rebuilt, compressed))
            {
                if (error != nullptr)
                {
                    *error = QObject::tr("The reduced stream could not be written.");
                }
                return false;
            }

            removed += lost;
            reduced.append({contentsObject, compressed});
        }
    }

    if (removed == 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("There is no layer to remove.");
        }
        return false;
    }

    // ── Les octets des objets réécrits ──────────────────────────────────────────────────────────
    //
    // A page's drawing, then the catalogue, then `/Order` — one new body each, written at the end,
    // either in place or as an incremental update.
    for (const QPair<int, QByteArray> &entry : reduced)
    {
        QByteArray body = "<</Filter/FlateDecode/Length ";
        body.append(QByteArray::number(entry.second.size()));
        body.append(">>stream\r\n");
        body.append(entry.second);
        body.append("\r\nendstream\r");
        bodies.insert(entry.first, body);
    }

    // ── Le catalogue : le panneau cesse d'offrir les calques retirés ─────────────────────────────
    //
    // ⚠️ **Un calque retiré du dessin reste offert dans le panneau tant que le catalogue le nomme
    // dans /OCProperties** : le lecteur dessine sa liste depuis `/OCGs`, jamais depuis ce que la page
    // contient. Cette entrée est donc réécrite aussi — c'est tout l'objet de cette mise à jour, donc
    // un document dont on ne sait pas lire le catalogue est REFUSÉ plutôt qu'écrit à moitié réduit.
    int dictionaryAt = -1;
    int dictionaryEnd = 0;

    // ⚠️ `/OCProperties` est aussi un mot que les métadonnées XMP peuvent citer, et un document peut
    // l'écrire hors ligne (`/OCProperties 12 0 R`). L'entrée retenue est donc la seule qui SOIT un
    // dictionnaire : le premier `<<` après le mot, sans rien d'autre que des blancs entre les deux.
    for (int at = text.indexOf(QLatin1String("/OCProperties")); at >= 0;
         at = text.indexOf(QLatin1String("/OCProperties"), at + 1))
    {
        int gap = at + 13;
        while (gap < text.size() && text.at(gap).isSpace())
        {
            ++gap;
        }
        if (text.mid(gap, 2) != QLatin1String("<<"))
        {
            continue;
        }
        dictionaryAt = DictionaryBounds(text, gap, dictionaryEnd);
        break;
    }

    if (dictionaryAt < 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF does not say which layers it holds.");
        }
        return false;
    }

    // L'objet qui porte l'entrée est le catalogue. Sa tête est réécrite octet pour octet, si bien que
    // la mise à jour REMPLACE cet objet : le document garde un catalogue, pas deux.
    // ⚠️ L'entrée n'ouvre pas toujours le dictionnaire (`1 0 obj<</OCProperties<<…`) : la tête est donc
    // la DERNIÈRE `N 0 obj` qui précède l'entrée, et non celle qui la touche.
    static const QRegularExpression catalogueHead(QStringLiteral("(\\d+)\\s+0\\s+obj"));
    const int catalogueEnd = pdf.indexOf("endobj", dictionaryEnd);
    QRegularExpressionMatch owner;
    QRegularExpressionMatchIterator heads = catalogueHead.globalMatch(text.left(dictionaryAt));
    while (heads.hasNext())
    {
        owner = heads.next();
    }

    if (!owner.hasMatch() || catalogueEnd < 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF does not say which layers it holds.");
        }
        return false;
    }

    const int catalogueObject = owner.captured(1).toInt();
    const int catalogueAt = owner.capturedEnd();

    // Seuls les calques gardés subsistent, partout où l'entrée en nomme un : `/OCGs` pour le panneau,
    // `/ON`, `/OFF`, `/Order` et `/RBGroups` pour les états et l'ordre.
    QVector<int> dropped;
    for (const int object : OcgNumbers(pdf))
    {
        if (!kept.contains(object))
        {
            dropped.append(object);
        }
    }

    QString properties = text.mid(dictionaryAt, dictionaryEnd - dictionaryAt);
    DropReferences(properties, dropped);
    properties.replace(QRegularExpression(QStringLiteral("[ \\t]{2,}")), QStringLiteral(" "));
    properties.replace(QRegularExpression(QStringLiteral("\\[\\s*\\]")), QStringLiteral("[]"));

    QByteArray catalogue = pdf.mid(catalogueAt, dictionaryAt - catalogueAt);
    catalogue.append(properties.toLatin1());
    // ⚠️ Le corps s'arrête AVANT `endobj` : c'est `PatchInPlace` qui referme l'objet, et le compter
    // deux fois laissait un `endobjendobj` et un objet plus long que le texte qu'il remplaçait.
    catalogue.append(pdf.mid(dictionaryEnd, catalogueEnd - dictionaryEnd));
    bodies.insert(catalogueObject, catalogue);

    // `/Order` écrit hors ligne (`/Order 30 0 R`) est un objet tableau à part : un lecteur qui dessine
    // son panneau depuis `/Order` continuerait d'offrir les calques retirés. Il est réécrit pareil.
    static const QRegularExpression indirectOrder(QStringLiteral("/Order\\s+(\\d+)\\s+0\\s+R"));
    const QRegularExpressionMatch orderMatch = indirectOrder.match(properties);
    if (orderMatch.hasMatch() && !dropped.isEmpty())
    {
        const int orderObject = orderMatch.captured(1).toInt();
        QString order = PdfText(ObjectBody(pdf, text, orderObject));
        if (!order.isEmpty())
        {
            DropReferences(order, dropped);
            bodies.insert(orderObject, order.toLatin1());
        }
    }

    // ── Écrire ──────────────────────────────────────────────────────────────────────────────────
    QByteArray out;
    if (!PatchInPlace(pdf, text, bodies, out))
    {
        // La place a manqué : la mise à jour incrémentale reste correcte pour les lecteurs qui suivent
        // `/Prev`, et c'est le seul cas où le fichier garde l'ancienne définition.
        out = pdf;
        if (!out.endsWith('\n'))
        {
            out.append("\r\n");
        }

        QHash<int, int> offsets;
        for (auto it = bodies.constBegin(); it != bodies.constEnd(); ++it)
        {
            offsets.insert(it.key(), out.size());
            out.append(QByteArray::number(it.key()) + " 0 obj");
            out.append(it.value());
            out.append("endobj\r\n");
        }

        // La table de références : uniquement les objets que cette mise à jour écrit, l'originale
        // restant joignable par `/Prev`. `/Root` est une RÉFÉRENCE et `/Size` couvre tout le fichier.
        const int xrefAt = out.size();
        out.append("xref\r\n");
        QList<int> numbers = offsets.keys();
        std::sort(numbers.begin(), numbers.end());
        for (int number : numbers)
        {
            out.append(QByteArray::number(number) + " 1\r\n");
            out.append(QByteArray::number(offsets.value(number)).rightJustified(10, '0'));
            out.append(" 00000 n\r\n");
        }

        const int trailerAt = text.lastIndexOf(QLatin1String("trailer"));
        const QString trailer = trailerAt < 0 ? QString() : text.mid(trailerAt);

        static const QRegularExpression sizeEntry(QStringLiteral("/Size\\s+(\\d+)"));
        const QRegularExpressionMatch sizeMatch = sizeEntry.match(trailer);
        int size = numbers.last() + 1;
        if (sizeMatch.hasMatch())
        {
            size = std::max(size, sizeMatch.captured(1).toInt());
        }

        static const QRegularExpression root(QStringLiteral("/Root\\s+(\\d+)\\s+0\\s+R"));
        const QRegularExpressionMatch rootMatch = root.match(trailer);

        out.append("trailer\r<</Size ");
        out.append(QByteArray::number(size));
        out.append("/Root ");
        out.append(QByteArray::number(rootMatch.hasMatch() ? rootMatch.captured(1).toInt() : catalogueObject));
        out.append(" 0 R/Prev ");
        out.append(QByteArray::number(startxref));
        out.append(">>\rstartxref\r");
        out.append(QByteArray::number(xrefAt));
        out.append("\r%%EOF\r");
    }

    QSaveFile output(target);
    if (!output.open(QIODevice::WriteOnly))
    {
        if (error != nullptr)
        {
            *error = QObject::tr("Cannot write %1.").arg(target);
        }
        return false;
    }
    output.write(out);
    return output.commit();
}
