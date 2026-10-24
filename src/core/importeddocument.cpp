/************************************************************************
 **
 **  @file   importeddocument.cpp
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  Reading an imported PDF far enough to know which journey it opens.
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

#include "importeddocument.h"

#include "core/layerreducer.h"
#include "pdf/PdfPayload.h"  // Trame — the envelope Ostara writes

#include <QDomDocument>
#include <QDomElement>
#include <QFileInfo>
#include <QSet>
#include <QStringList>

#include <algorithm>

namespace
{
/// Sorts the size layers the way the PDF's own panel shows them.
///
/// ⚠️ **The payload's order is the DRAWING's order, and the panel's is the reverse of it.** Ostara
/// writes its size layers from the largest down — the smallest size is drawn last, on top of the
/// others — and the PDF's `/Order` lists them from the top down, so a reader's panel shows the
/// smallest first. Rabnest read the drawing and called it the list, so the sizes came out backwards
/// against the very file she had just exported.
///
/// The panel's order is READ FROM THE PDF, never guessed by reversing the payload: two files can
/// disagree, and only one of them is on screen. A layer the panel does not name keeps its place at
/// the end, in the order it was read.
auto InPanelOrder(QVector<ImportedLayer> *layers, const QStringList &order) -> void
{
    if (layers == nullptr || order.isEmpty())
    {
        return;
    }

    QHash<QString, int> rank;
    for (int index = 0; index < order.size(); ++index)
    {
        rank.insert(order.at(index), index);
    }
    const int unknown = order.size();

    std::stable_sort(layers->begin(), layers->end(),
                     [&rank, unknown](const ImportedLayer &one, const ImportedLayer &other) {
                         return rank.value(one.label, unknown) < rank.value(other.label, unknown);
                     });
}

/// The names the panel shows, in the order it shows them.
auto PanelOrder(const QString &path) -> QStringList
{
    QStringList names;
    for (const PdfLayer &layer : LayerReducer::Read(path, nullptr))
    {
        names.append(layer.name);
    }
    return names;
}
/// The namespace the markers live in, and the two attribute names Rabnest reads.
///
/// Only two of Ostara's own attributes matter here, and both are read as PLAIN names: the payload
/// is read by QDom without namespace processing, so `inkscape:label` arrives whole and comparing
/// the prefixed name is both simpler and truer to what is in the file than resolving a namespace
/// that nothing else in this application uses.
constexpr auto kAttrInkscapeLabel = "inkscape:label";
constexpr auto kAttrGroupMode = "inkscape:groupmode";
constexpr auto kAttrIllustratorLayer = "i:layer";
constexpr auto kAttrOstaraLayout = "ostara:layout";
constexpr auto kValueLayer = "layer";
constexpr auto kValueYes = "yes";

/// The human name of a layer: what the creator called it, else its title, else its id.
///
/// The chain is Ostara's own, and the order matters: `inkscape:label` is what every editor shows and
/// what the creator typed; `<title>` is what a layered export carries when the label is missing; the
/// id is the machine's fallback and reads as one.
auto LayerLabel(const QDomElement &layer) -> QString
{
    const QString label = layer.attribute(QLatin1String(kAttrInkscapeLabel));
    if (!label.isEmpty())
    {
        return label;
    }

    for (QDomElement child = layer.firstChildElement(); !child.isNull();
         child = child.nextSiblingElement())
    {
        if (child.tagName() == QLatin1String("title"))
        {
            const QString title = child.text();
            if (!title.isEmpty())
            {
                return title;
            }
        }
    }

    return layer.attribute(QLatin1String("id"));
}

/// Whether an element is a layer — the two spellings that mean it, Illustrator's and Inkscape's.
auto IsLayer(const QDomElement &element) -> bool
{
    if (element.tagName() != QLatin1String("g"))
    {
        return false;
    }

    return element.attribute(QLatin1String(kAttrGroupMode)) == QLatin1String(kValueLayer)
           || element.attribute(QLatin1String(kAttrIllustratorLayer)) == QLatin1String(kValueYes);
}
}  // namespace

//---------------------------------------------------------------------------------------------------------------------
/// The payload is the whole answer: a file that carries one was written by Ostara, and a file that
/// does not was written by anybody else. Nothing else is looked at, and no guess is made from the
/// producer string — the payload is the contract, and a contract that can be imitated is still the
/// only one Rabnest promised to honour.
auto ImportedDocument::Read(const QString &path) -> ImportedDocument
{
    ImportedDocument document;
    document.m_path = path;

    if (!QFileInfo::exists(path))
    {
        document.m_error = QStringLiteral("The file does not exist.");
        return document;
    }

    int version = 0;
    QString error;
    document.m_payload = PdfPayload::read(path, &version, &error);

    if (document.m_payload.isEmpty())
    {
        // Not an error: a PDF written by another application is a document Rabnest can still work
        // with — fewer layers, no new nesting. The error is the reader's own, and it describes a
        // payload that was there and could not be read; a file with no payload has none.
        document.m_kind = Kind::Foreign;

        // A foreign PDF still has layers — its own optional content — and they are the whole of
        // what Rabnest can offer for it. They are read from the file's /OCG objects, because that is
        // where a reader's layer panel comes from, and their object number is their identity: two
        // layers may share a name, none share a number.
        QString reason;
        for (const PdfLayer &layer : LayerReducer::Read(path, &reason))
        {
            ImportedLayer entry;
            entry.id = QString::number(layer.object);
            entry.label = layer.name;
            document.m_layers.append(entry);
        }

        document.m_error.clear();
        return document;
    }

    document.m_kind = Kind::Ostara;
    document.ParsePayload();

    // The payload says which layers there are; the PDF says in which ORDER a reader sees them.
    InPanelOrder(&document.m_layers, PanelOrder(path));

    return document;
}

//---------------------------------------------------------------------------------------------------------------------
auto ImportedDocument::FromPayload(const QByteArray &svg, const QString &path) -> ImportedDocument
{
    ImportedDocument document;
    document.m_path = path;
    document.m_payload = svg;
    document.m_kind = Kind::Ostara;
    document.ParsePayload();
    return document;
}

//---------------------------------------------------------------------------------------------------------------------
void ImportedDocument::ParsePayload()
{
    QDomDocument svg;
    if (!svg.setContent(m_payload))
    {
        m_error = QStringLiteral("The payload Ostara wrote could not be read.");
        m_payload.clear();
        return;
    }

    const QDomElement root = svg.documentElement();
    for (QDomElement layer = root.firstChildElement(); !layer.isNull();
         layer = layer.nextSiblingElement())
    {
        if (!IsLayer(layer))
        {
            continue;
        }

        ImportedLayer entry;
        entry.id = layer.attribute(QLatin1String("id"));
        entry.label = LayerLabel(layer);
        entry.decor = layer.attribute(QLatin1String(kAttrOstaraLayout)) == QLatin1String(kValueYes);

        // The decor layer is remembered and not offered: it carries the cartouche, the labels and
        // the registration marks, which belong to the drawing rather than to a choice. Letting it be
        // unticked would deliver a plan that still looks like a plan and has lost its information.
        if (entry.decor)
        {
            m_decorLayerId = entry.id;
            continue;
        }

        m_layers.append(entry);
    }
}

//---------------------------------------------------------------------------------------------------------------------
auto ImportedDocument::stem() const -> QString
{
    return QFileInfo(m_path).completeBaseName();
}

//---------------------------------------------------------------------------------------------------------------------
auto ImportedDocument::decorLayerId() const -> QString
{
    return m_decorLayerId;
}

//---------------------------------------------------------------------------------------------------------------------
auto ImportedDocument::Pieces(const QStringList &sizeLayerIds) const -> QStringList
{
    QStringList pieces;
    if (m_payload.isEmpty() || sizeLayerIds.isEmpty())
    {
        return pieces;
    }

    QDomDocument svg;
    if (!svg.setContent(m_payload))
    {
        return pieces;
    }

    const QDomElement root = svg.documentElement();
    for (QDomElement layer = root.firstChildElement(); !layer.isNull();
         layer = layer.nextSiblingElement())
    {
        if (!IsLayer(layer) || !sizeLayerIds.contains(layer.attribute(QLatin1String("id"))))
        {
            continue;
        }

        // Every direct child of a size layer is a piece: Ostara writes one wrapper per object of
        // the pattern, named after it, whether or not the size drew anything inside. The wrapper's
        // id is the piece's identity across every size, which is what makes the union meaningful.
        for (QDomElement piece = layer.firstChildElement(); !piece.isNull();
             piece = piece.nextSiblingElement())
        {
            if (piece.tagName() != QLatin1String("g"))
            {
                continue;
            }

            // ⚠️ **A GROUP'S MEMBERS HANG ONE LEVEL LOWER, and they are the pieces — the wrapper is
            // not.** Ostara draws a group of two or more as one object that nests each member
            // (`<g id="Manche__Groupe_"><g id="Manche"/><g id="Manche_poignet"/></g>`), while the
            // payload names every member on its own, carrying the group's key so the nesting lays
            // them as one block. A reader that stopped at the direct children saw the group's name,
            // which matches no piece, and quietly planned the pattern without those pieces. The
            // wrapper's own id is kept only when it nests nothing — that is what a lone piece is.
            bool isGroup = false;
            for (QDomElement member = piece.firstChildElement(QStringLiteral("g")); !member.isNull();
                 member = member.nextSiblingElement(QStringLiteral("g")))
            {
                isGroup = true;
                const QString memberId = member.attribute(QLatin1String("id"));
                if (!memberId.isEmpty() && !pieces.contains(memberId))
                {
                    pieces.append(memberId);
                }
            }

            const QString id = piece.attribute(QLatin1String("id"));
            if (!isGroup && !id.isEmpty() && !pieces.contains(id))
            {
                pieces.append(id);
            }
        }
    }

    return pieces;
}

//---------------------------------------------------------------------------------------------------------------------
auto ImportedDocument::HasLiningPieces(const QStringList &sizeLayerIds) const -> bool
{
    if (m_payload.isEmpty())
    {
        return false;
    }

    QDomDocument svg;
    if (!svg.setContent(m_payload))
    {
        return false;
    }

    // The pieces are named by their SIZE's label, and the sizes the user kept are named by their
    // layer's id: the two vocabularies meet here, once.
    QSet<QString> labels;
    for (const ImportedLayer &layer : m_layers)
    {
        if (sizeLayerIds.isEmpty() || sizeLayerIds.contains(layer.id))
        {
            labels.insert(layer.label);
        }
    }

    const QDomNodeList pieces = svg.documentElement()
                                    .firstChildElement(QStringLiteral("metadata"))
                                    .firstChildElement(QStringLiteral("pieces"))
                                    .elementsByTagName(QStringLiteral("piece"));
    for (int index = 0; index < pieces.count(); ++index)
    {
        const QDomElement piece = pieces.at(index).toElement();
        if (!labels.contains(piece.attribute(QStringLiteral("size"))))
        {
            continue;
        }

        if (piece.attribute(QStringLiteral("cutLining")).toInt() > 0)
        {
            return true;
        }
    }

    return false;
}
