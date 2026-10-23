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
    return status == Z_STREAM_END;
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

/// The object body, given its number: from "N 0 obj" to the matching "endobj".
auto ObjectBody(const QByteArray &pdf, int number) -> QByteArray
{
    const QByteArray head = QByteArray::number(number) + " 0 obj";
    const int at = pdf.indexOf(head);
    if (at < 0)
    {
        return {};
    }
    const int end = pdf.indexOf("endobj", at);
    if (end < 0)
    {
        return {};
    }
    return pdf.mid(at, end - at);
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

    static const QRegularExpression ocg(QStringLiteral("(\\d+)\\s+0\\s+obj\\s*<<[^>]*?/Type\\s*/OCG"));
    QRegularExpressionMatchIterator it = ocg.globalMatch(PdfText(pdf));
    QHash<int, QString> nameOf;
    QVector<int> found;
    QSet<QString> seen;
    while (it.hasNext())
    {
        const QRegularExpressionMatch match = it.next();
        const int number = match.captured(1).toInt();
        const QString name = LayerName(ObjectBody(pdf, number));
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
    // `/Order` names them by object number, and may nest arrays for grouped layers; whatever it does
    // not name keeps its place at the end, in the order it was found. A file that states no `/Order`
    // has no panel order to honour and keeps the order it was read in.
    QVector<int> order;
    static const QRegularExpression orderArray(QStringLiteral("/Order\\s*\\[([^\\]]*)\\]"));
    const QRegularExpressionMatch ordered = orderArray.match(PdfText(pdf));
    if (ordered.hasMatch())
    {
        static const QRegularExpression reference(QStringLiteral("(\\d+)\\s+0\\s+R"));
        QRegularExpressionMatchIterator refs = reference.globalMatch(ordered.captured(1));
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
    const int startxref = pdf.mid(static_cast<int>(startxrefAt) + 10).trimmed().split('\n').first().toInt();

    // The page: the first /Type/Page dictionary, with its stream and its property map.
    static const QRegularExpression pageObject(
        QStringLiteral("(\\d+)\\s+0\\s+obj\\s*<<((?:(?!/Type/Pages).)*?/Type\\s*/Page[\\s/>])"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch page = pageObject.match(PdfText(pdf));
    static const QRegularExpression contents(QStringLiteral("/Contents\\s+(\\d+)\\s+0\\s+R"));
    static const QRegularExpression length(QStringLiteral("/Length\\s+(\\d+)"));

    if (!page.hasMatch())
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF has no readable page.");
        }
        return false;
    }

    const QByteArray body = ObjectBody(pdf, page.captured(1).toInt());
    const QRegularExpressionMatch contentsMatch = contents.match(PdfText(body));
    if (!contentsMatch.hasMatch())
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This page carries no drawing stream.");
        }
        return false;
    }

    const int contentsObject = contentsMatch.captured(1).toInt();
    const QByteArray streamBody = ObjectBody(pdf, contentsObject);
    const QRegularExpressionMatch lengthMatch = length.match(PdfText(streamBody));
    const int streamAt = streamBody.indexOf("stream");
    if (!lengthMatch.hasMatch() || streamAt < 0 || streamBody.indexOf("FlateDecode") < 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("This PDF is not in a shape Rabnest can reduce.");
        }
        return false;
    }

    int lengthValue = lengthMatch.captured(1).toInt();
    int dataAt = streamAt + 6;
    while (dataAt < streamBody.size()
           && (streamBody.at(dataAt) == '\r' || streamBody.at(dataAt) == '\n'))
    {
        ++dataAt;
    }

    QByteArray stream;
    if (!Inflate(streamBody.mid(dataAt, lengthValue), stream))
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

    QSet<int> kept;
    for (int object : keep)
    {
        kept.insert(object);
    }

    QByteArray rebuilt;
    int position = 0;
    int removed = 0;
    for (const Section &section : sections)
    {
        rebuilt.append(stream.mid(position, section.begin - position));
        const int object = PropertyObject(body, section.mc);

        if (kept.contains(object))
        {
            rebuilt.append(stream.mid(section.begin, section.end - section.begin));
        }
        else
        {
            const int delta = DepthDelta(stream.mid(section.begin, section.end - section.begin));
            if (delta > 0)
            {
                rebuilt.append(QByteArray("q\n").repeated(delta));
            }
            else if (delta < 0)
            {
                rebuilt.append(QByteArray("Q\n").repeated(-delta));
            }
            ++removed;
        }
        position = section.end;
    }
    rebuilt.append(stream.mid(position));

    if (removed == 0)
    {
        if (error != nullptr)
        {
            *error = QObject::tr("There is no layer to remove.");
        }
        return false;
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

    // The incremental update: the original bytes stay, and only what changed is appended.
    QByteArray out = pdf;
    if (!out.endsWith('\n'))
    {
        out.append("\r\n");
    }

    QHash<int, int> offsets;
    offsets.insert(contentsObject, out.size());
    out.append(QByteArray::number(contentsObject) + " 0 obj\r<</Filter/FlateDecode/Length ");
    out.append(QByteArray::number(compressed.size()));
    out.append(">>stream\r\n");
    out.append(compressed);
    out.append("\r\nendstream\rendobj\r");

    // The catalogue: only the kept layers, so the panel stops offering the removed ones. The rest
    // of the dictionary is kept byte for byte — Rabnest rewrites one entry and nothing else.
    static const QRegularExpression ocProperties(
        QStringLiteral("(/OCProperties\\s*<<.*?/OCGs\\s*\\[[^\\]]*\\]\\s*>>)"),
        QRegularExpression::DotMatchesEverythingOption);
    const QRegularExpressionMatch catalogMatch = ocProperties.match(PdfText(pdf));
    if (catalogMatch.hasMatch())
    {
        const int objectAt = pdf.lastIndexOf(" obj", catalogMatch.capturedStart());
        int numberAt = objectAt;
        while (numberAt > 0 && pdf.at(numberAt - 1) != '\n' && pdf.at(numberAt - 1) != '\r')
        {
            --numberAt;
        }
        const int number = pdf.mid(numberAt, objectAt - numberAt).trimmed().toInt();

        QString array;
        for (int object : keep)
        {
            array += QStringLiteral("%1 0 R ").arg(object);
        }
        array = array.trimmed();

        const QString filtered = QStringLiteral("<</D<</ON[%1]/Order[%1]/RBGroups[]>>/OCGs[%1]>>")
                                     .arg(array);
        QByteArray body = pdf.mid(catalogMatch.capturedStart(),
                                  catalogMatch.capturedEnd() - catalogMatch.capturedStart());
        body = filtered.toLatin1();

        offsets.insert(number, out.size());
        out.append(QByteArray::number(number) + " 0 obj\r");
        out.append(body);
        out.append("\rendobj\r");
    }

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
    out.append("trailer\r<</Size ");
    out.append(QByteArray::number(numbers.last() + 1));
    static const QRegularExpression root(QStringLiteral("/Root\\s+(\\d+)\\s+0\\s+R"));
    const QRegularExpressionMatch rootMatch = root.match(PdfText(pdf));
    out.append("/Root ");
    out.append(QByteArray::number(rootMatch.hasMatch() ? rootMatch.captured(1).toInt() : 1));
    out.append("/Prev ");
    out.append(QByteArray::number(startxref));
    out.append(">>\rstartxref\r");
    out.append(QByteArray::number(xrefAt));
    out.append("\r%%EOF\r");

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
