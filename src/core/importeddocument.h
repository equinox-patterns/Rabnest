/************************************************************************
 **
 **  @file   importeddocument.h
 **  @author Sophie Denys <contact@sophiedenys.com>
 **  @date   22/10/2026
 **
 **  @brief  What the user just imported, and what Rabnest is allowed to do with it.
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

#ifndef IMPORTEDDOCUMENT_H
#define IMPORTEDDOCUMENT_H

#include <QString>
#include <QVector>

/// One layer of an imported PDF, as the user will see it in a list.
///
/// `decor` is the one Rabnest never shows: the layout layer — cartouche, labels, registration
/// marks — which belongs to the drawing and not to a choice. It is found by the marker Ostara
/// writes on it (`ostara:layout="yes"`), never by its name, because that name is the creator's
/// own and can be anything.
struct ImportedLayer
{
    QString id;     ///< the SVG id, which is what the payload is navigated by
    QString label;  ///< what a human reads: inkscape:label, else <title>, else the id
    bool decor = false;
};

/// A PDF the user picked, read far enough to know which journey they are on.
///
/// There are two, and only two. A PDF **Ostara wrote** carries the payload the plan was drawn
/// from, so Rabnest can offer sizes, pieces, outputs and a new nesting. Any other PDF carries
/// nothing Rabnest understands: the layers are still there — the reader's own optional content —
/// but there are no pieces to choose and nothing to re-nest, so the only gesture left is to write
/// the same document back with fewer layers.
///
/// Nothing is kept: this object lives as long as the window, and no file is written beside the
/// PDF the user chose.
class ImportedDocument
{
public:
    enum class Kind
    {
        Ostara,  ///< it carries the payload — the full journey
        Foreign  ///< it does not — the layer reduction, and nothing else
    };

    /// Reads @p path and says which kind of document it is.
    ///
    /// The file is never modified and nothing is copied: on the Ostara path the payload is held
    /// in memory, decompressed, because every later step — sizes, pieces, nesting — reads it.
    static ImportedDocument Read(const QString &path);

    /// The same document, built from a payload that never came from a PDF.
    ///
    /// A test bench needs to state a payload outright — a piece whose fold is known, a quantity
    /// nobody typed — and going through a PDF to say it would be testing the PDF as much as the
    /// plan. The layers are read from the SVG exactly as they are read from a real payload, so
    /// nothing else about the journey changes.
    static ImportedDocument FromPayload(const QByteArray &svg, const QString &path);

    Kind kind() const { return m_kind; }
    QString path() const { return m_path; }

    /// The file's own name, without its extension — what an export starts from.
    QString stem() const;

    /// The layers the user may choose, in the order the payload holds them. The layout layer is
    /// not among them: it is present in `decorLayerId()` instead.
    const QVector<ImportedLayer> &layers() const { return m_layers; }

    /// The layout layer's id, or empty when the document has none — a single-size run writes its
    /// decor flat, and a foreign PDF knows nothing of it.
    QString decorLayerId() const;

    /// Whether this file was exported FOR A PROJECTOR — a 1:1 projection, one page, no tiling.
    ///
    /// ⚠️ **Il faut le demander, parce que rien d'autre ne le dit.** Sans marqueur, un PDF
    /// projecteur se lit comme un PDF d'impression : même charge utile, mêmes pièces, même recette —
    /// et Rabnest le re-nidifierait en écart zéro et en tuilé, silencieusement. Le marqueur voyage
    /// dans le contenu de la charge utile (`exportRecipe/format@destination`), où l'espace est libre ;
    /// les clés de l'enveloppe PDF, elles, sont des constantes que ce côté-ci ne peut pas apprendre.
    ///
    /// Ce mode n'est pas encore implémenté ici : ce que l'application en fait aujourd'hui, c'est
    /// refuser le fichier en disant pourquoi — jamais produire un plan faux qui a l'air fini.
    bool isProjector() const { return m_projector; }

    /// The payload, as Ostara wrote it. Empty for a foreign document.
    QByteArray payload() const { return m_payload; }

    /// The pieces the chosen size layers hold, by name, in the order they first appear.
    ///
    /// The names are the PAYLOAD's own (`<piece name="…">`), never the drawing's ids: Ostara slugs
    /// those — « Manche poignet » is drawn `Manche_poignet` — and writes every member of a group
    /// twice in a size layer, the second copy suffixed by its per-layer id deduplication. A reader
    /// that listed the drawing therefore offered `Manche` AND `Manche_1`: the same piece twice, and
    /// the suffixed one matches no `<piece name>` at all, so its tick was inert.
    ///
    /// A union and not an intersection: a piece missing from one size — a variant, a graded block
    /// that does not exist there — is still a piece of the pattern, and hiding it would silently
    /// drop what the creator drew. The user ticks what they want; the sizes that lack it simply
    /// contribute nothing.
    QStringList Pieces(const QStringList &sizeLayerIds) const;

    /// Whether the chosen sizes hold a piece **cut in the lining** — which is the only thing that
    /// makes a lining plan possible, and therefore the only thing that may offer one.
    ///
    /// Read on the payload's own figures (`cutLining` on each piece), not on the drawing: the
    /// drawing carries no quantity anywhere, so a reader that looked for it there would answer
    /// « no lining » to a garment that has one. A piece nobody filled in reads 0, and 0 is not
    /// « once » — it is « not cut in that cloth at all ».
    ///
    /// The whole document is asked when no size is given: the sizes are the first step's answer, and
    /// a journey that has not asked it yet still has a pattern.
    bool HasLiningPieces(const QStringList &sizeLayerIds) const;

    /// Why the file could not be read, when it could not. Empty is a document Rabnest can use.
    QString error() const { return m_error; }
    bool isValid() const { return m_error.isEmpty(); }

private:
    void ParsePayload();

    Kind m_kind = Kind::Foreign;
    QString m_path;
    QByteArray m_payload;
    QVector<ImportedLayer> m_layers;
    QString m_decorLayerId;
    /// Voir isProjector().
    bool m_projector = false;
    QString m_error;
};

#endif  // IMPORTEDDOCUMENT_H
