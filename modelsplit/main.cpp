/* --------------------------------------------------------------------------
 *
 * MIT License
 *
 * Copyright (c) 2021 Jason C
 *
 * Permission is  hereby granted,  free of charge,  to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without  restriction, including without limitation
 * the rights to use,  copy, modify, merge, publish,  distribute, sublicense,
 * and/or sell  copies of  the Software,  and to permit  persons to  whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED,  INCLUDING BUT NOT LIMITED TO  THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A  PARTICULAR PURPOSE  AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY,  WHETHER IN AN ACTION OF CONTRACT,  TORT OR OTHERWISE,  ARISING
 * FROM,  OUT OF  OR IN  CONNECTION WITH  THE SOFTWARE  OR THE  USE OR  OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * ------------------------------------------------------------------------*/

#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressDialog>
#include <QSettings>
#include <QTranslator>
#include <optional>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/Exporter.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#ifdef Q_OS_WIN
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <shellapi.h>
#endif

#define WEIGHTED_UNION 0 // use weighting in vunion(); hurts more than it helps in testing
#define DEBUG_TIMES    0 // print some timing info

using std::exception;
using std::runtime_error;
using std::optional;

/** Options parsed from the command line. */
struct Options {
    QString input;             /**< Input filename. */
    bool unify;                /**< Unify vertices on input? */
    optional<bool> overwrite;  /**< Forced overwrite mode, if specified. */
    QString format;            /**< Output format ID, if specified. */
    bool pickFormat;           /**< Show output format picker dialog? */
};

/** An overly-complicated progress dialog. */
struct ComplicatedProgress {
    explicit ComplicatedProgress (int items) : subcounts(items, 1) { setProgress(0, 0); }
    void updateCount (int items);
    void updateSubCount (int item, int subcount);
    bool setProgress (int item, int subitem = 0);
    void setFinished ();
    void setEnabled (bool enabled) { dialog->setEnabled(enabled); }
private:
    QScopedPointer<QProgressDialog> dialog;
    QVector<int> subcounts;
    int curitem;
    int cursubitem;
};

/** Prompts to overwrite if needed; saves state if "Yes To All" / "No To All" chosen. */
struct OverwritePrompter {
    explicit OverwritePrompter (const Options &opts, ComplicatedProgress *progress)
        : overwrite(opts.overwrite), progress(progress) { }
    QString getFilename (QString filename);
private:
    optional<bool> overwrite;
    ComplicatedProgress *progress;
};

/** A split component; it's the source mesh and a subset of its vertex and face indices.
 *  Also handles exporting the component to an output file. */
struct Component {

    /** Options relevant to saving the ouput file. */
    struct SaveOptions {
        int sourceIndex;              /**< Index of source mesh (usually it's just 0). */
        int componentIndex;           /**< Index of component, in source mesh. */
        int globalIndex;              /**< Index of component, globally. */
        QString defaultFormat;        /**< Fallback format ID if Options::format not set. */
        OverwritePrompter *prompter;  /**< A shared OverwritePrompter. */
    };

    const aiMesh *source;    /**< The source mesh. */
    QVector<int> vertices;   /**< Indices of source vertices for this component. */
    QVector<int> faces;      /**< Indices of source faces for this component. */

    explicit Component (const aiMesh *source = nullptr) : source(source) { }

    void save (const Options &opts, const SaveOptions &sopts) const;
    aiMesh * buildSubMesh () const;

};

/** Splits a mesh into components. This doesn't really *need* to exist (it made more
 *  sense when I started), but it's handy to pass buffers around, I guess.
 *
 *  @todo Clean up implementation of split(), maybe make this struct disappear. It's
 *        working fine as-is, though, and it's plenty fast.
 */
struct Splitter {

    const aiMesh *mesh;             /**< The source mesh. */
    QVector<Component> components;  /**< Split components from source mesh. */

    explicit Splitter (const aiMesh *mesh) : mesh(mesh) { split(); }
    void split ();

private:

    static const int Unused = INT_MIN;

#if WEIGHTED_UNION
    QVector<int> treeSize;
#endif
    QVector<int> vertexComponent;   /**< Intermediate buffer for vertex component IDs. */
    QVector<int> faceComponent;     /**< Intermediate buffer for face component IDs. */

    // Ye olde disjoint set. Operates on vertexComponent.
    int vroot (int v);
    void vunion (int a, int b);

};


/**
 * Split the source mesh into individual components. There is no error return; if
 * the source mesh is null or empty, it'll just have zero components.
 *
 * @pre   Source mesh is in 'mesh'.
 * @post  Components are in 'components'.
 * @todo  Clean this up, maybe.
 */
void Splitter::split () {

#if DEBUG_TIMES
    QElapsedTimer timer;
    timer.start();
#endif

    QVector<int> componentNames;
    vertexComponent.clear();
    faceComponent.clear();

    bool ok = false;
    if (!mesh)
        qWarning("splitter: skipping null mesh.");
    else if (!mesh->mNumVertices || !mesh->HasFaces())
        qWarning("splitter: skipping empty mesh.");
    else
        ok = true;
    if (!ok)
        return;

    QVector<bool> usedVertices(mesh->mNumVertices, false);
    vertexComponent.resize(mesh->mNumVertices);
    for (unsigned k = 0; k < mesh->mNumVertices; ++ k)
        vertexComponent[k] = k;

#if WEIGHTED_UNION
    treeSize.fill(1, mesh->mNumVertices);
#endif

    for (unsigned k = 0; k < mesh->mNumFaces; ++ k) {
        const aiFace &face = mesh->mFaces[k];
        Q_ASSERT(face.mNumIndices == 3); // aiProcess_Triangulate on load.
        vunion(face.mIndices[0], face.mIndices[1]);
        vunion(face.mIndices[0], face.mIndices[2]);
        usedVertices[face.mIndices[0]] = true;
        usedVertices[face.mIndices[1]] = true;
        usedVertices[face.mIndices[2]] = true;
    }

    int unusedVertices = 0;

    // gather roots
#if 1 // slightly optimized -- remove use of QSet
    QVector<bool> isRoot(mesh->mNumVertices, false);
    QVector<int> componentIndex(mesh->mNumVertices);
    for (int k = 0; k < vertexComponent.size(); ++ k) {
        if (usedVertices[k]) {
            int name = vroot(k);
            vertexComponent[k] = name;
            if (!isRoot[name]) {
                componentIndex[name] = componentNames.size();
                componentNames.append(name);
                isRoot[name] = true;
            }
        } else {
            vertexComponent[k] = Unused;
            ++ unusedVertices;
        }
    }
#else
    QSet<int> componentNameSet;
    for (int k = 0; k < vertexComponent.size(); ++ k) {
        if (usedVertices[k]) {
            vertexComponent[k] = vroot(k);
            componentNameSet.insert(vertexComponent[k]);
        } else {
            vertexComponent[k] = -1;
            ++ unusedVertices;
        }
    }
    foreach (int name, componentNameSet)
        componentNames.append(name);
#endif

    if (unusedVertices) {
        qDebug("splitter: also removed %d unused vert%s", unusedVertices,
               unusedVertices == 1 ? "ex" : "ices");
    }

    components.fill(Component(mesh), componentNames.size());

    // do not call vroot() / vunion() after this point, vertexComponent is about
    // to be repurposed to hold component IDs instead of vertex root IDs.

    // replace vertex equivalency classes with component ids; i don't know how to
    // optimize this but it seems to be doing fine as is.
    for (unsigned k = 0; k < mesh->mNumVertices; ++ k) {
        if (usedVertices[k]) {
            int name = componentIndex[vertexComponent[k]];
            Q_ASSERT(name >= 0 && name < components.size());
            vertexComponent[k] = name;
            components[name].vertices.append(k);
        } else {
            Q_ASSERT(vertexComponent[k] == Unused);
        }
    }

    // assign component ids to faces
    faceComponent.resize(mesh->mNumFaces);
    for (unsigned k = 0; k < mesh->mNumFaces; ++ k) {
        int name = vertexComponent[mesh->mFaces[k].mIndices[0]];
        Q_ASSERT(name != Unused);
        Q_ASSERT(name >= 0 && name < components.size());
        Q_ASSERT(name == vertexComponent[mesh->mFaces[k].mIndices[1]]);
        Q_ASSERT(name == vertexComponent[mesh->mFaces[k].mIndices[2]]);
        faceComponent[k] = name;
        components[name].faces.append(k);
    }

#if WEIGHTED_UNION
    treeSize.clear(); // don't need this any more
#endif

#if DEBUG_TIMES
    qint64 nsecs = timer.nsecsElapsed();
    qDebug() << ((double)nsecs / 1000000.0) << "msec";
#endif

    // todo: you can get rid of componentNames since you ended up storing
    // consecutive component ids in vertexComponent.
    // todo: consider dropping faceComponent since its easy to look up.

}


/**
 * Find root of vertexComponent node. Also does partial path compression. This
 * can only be called up to a certain point in split() without breaking. See
 * source comments there. Disjoint-set operation.
 *
 * @param   v Vertex index.
 * @return  Root ID i.e. equivalency class.
 */
int Splitter::vroot (int v) {

    while (v != vertexComponent[v]) {
        vertexComponent[v] = vertexComponent[vertexComponent[v]];
        v = vertexComponent[v];
    }

    return v;

}


/**
 * Mark two vertices as equivalent. Same deal re: split() as vroot(). This is
 * also a disjoint-set operation.
 *
 * @param a    Vertex index.
 * @param b    Vertex index.
 */
void Splitter::vunion (int a, int b) {

    int aroot = vroot(a);
    int broot = vroot(b);

#if WEIGHTED_UNION
    if (treeSize[aroot] < treeSize[broot]) {
        vertexComponent[aroot] = broot;
        treeSize[broot] += treeSize[aroot];
    } else {
        vertexComponent[broot] = aroot;
        treeSize[aroot] += treeSize[broot];
    }
#else
    vertexComponent[aroot] = broot;
#endif

}


/**
 * Given an assimp format ID, return the preferred file extension.
 *
 * @param  format   Format ID.
 * @return File extension without leading dot. "" if not found.
 */
static QString lookupExtensionForFormat (QString format) {

    static QString cachedFormat;
    static QString cachedExtn;

    if (format != cachedFormat) {
        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
            auto desc = aiGetExportFormatDescription(k);
            Q_ASSERT(desc);
            if (!format.compare(desc->id, Qt::CaseInsensitive)) {
                cachedFormat = format;
                cachedExtn = desc->fileExtension;
                break;
            }
        }
    }

    return cachedExtn;

}


/**
 * Build an aiMesh that represents this component. This will copy the appropriate
 * vertices and faces from the source mesh, remap indices as necessary and build
 * an output mesh. The output mesh will have vertices and faces only (no normals,
 * colors, materials, texture coords, etc.).
 *
 * @pre    The 'source', 'vertices', and 'faces' members are valid.
 * @return A new aiMesh with the component model in it. The caller should delete
 *         this when it is no longer needed.
 */
aiMesh * Component::buildSubMesh () const {

    aiMesh *mesh = new aiMesh();
    mesh->mNumVertices = vertices.size();
    mesh->mVertices = new aiVector3D[vertices.size()];
    mesh->mNumFaces = faces.size();
    mesh->mFaces = new aiFace[faces.size()];

    QVector<int> vertexMap(source->mNumVertices, -1);

    for (int subv = 0; subv < vertices.size(); ++ subv) {
        int sourcev = vertices[subv];
        Q_ASSERT(sourcev >= 0 && sourcev < (int)source->mNumVertices);
        Q_ASSERT(vertexMap[sourcev] == -1);
        mesh->mVertices[subv] = source->mVertices[sourcev];
        vertexMap[sourcev] = subv;
    }

    for (int subf = 0; subf < faces.size(); ++ subf) {
        aiFace *subface = mesh->mFaces + subf;
        Q_ASSERT(faces[subf] >= 0 && faces[subf] < (int)source->mNumFaces);
        const aiFace *sourceface = source->mFaces + faces[subf];
        subface->mNumIndices = sourceface->mNumIndices;
        subface->mIndices = new unsigned[subface->mNumIndices];
        for (unsigned k = 0; k < subface->mNumIndices; ++ k) {
            Q_ASSERT(vertexMap[sourceface->mIndices[k]] != -1);
            subface->mIndices[k] = vertexMap[sourceface->mIndices[k]];
        }
    }

    return mesh;

}


/**
 * Export this component to a file. The filename is determined from the
 * provided option parameters. An overwrite prompt may be displayed.
 *
 * @pre   All preconditions for buildSubMesh().
 * @param opts    Program options.
 * @param sopts   Component-specific save options.
 * @throw runtime_error if an error occurs.
 */
void Component::save (const Options &opts, const SaveOptions &sopts) const {

    // figure out the filename; there's two options (todo: command line param)

    QFileInfo finfo(opts.input);
    QDir baseDir = finfo.dir();
    QString baseName = finfo.completeBaseName();
    QString baseExtn = finfo.suffix();

    // if we're changing the output format, ask assimp what the extension
    // should be (otherwise just use the same as the input file). note: if
    // format is invalid, whatever, let the export fail later.

    QString outExtn, outFormat;
    if (opts.format != "") {
        outExtn = lookupExtensionForFormat(opts.format);
        outFormat = opts.format;
    } else {
        outExtn = baseExtn;
        outFormat = sopts.defaultFormat;
    }

    // option 1 is use the global index; option 2 is source+component.
    // we're gonna do option 2 for now i guess, with 2 digits each.

    QString outFileName = QString("%3-%1%2.%4")
            .arg(sopts.sourceIndex, 2, 10, QChar('0'))
            .arg(sopts.componentIndex + 1, 2, 10, QChar('0'))
            .arg(baseName, outExtn);
    QString outFilePath = baseDir.absoluteFilePath(outFileName);

    if ((outFilePath = sopts.prompter->getFilename(outFilePath)) == "")
        throw runtime_error(QApplication::tr("Aborted. No more meshes will be written.").toStdString());

#ifndef QT_NO_DEBUG
    qDebug().noquote() << "    --> " << outFilePath;
#endif

    // all right; do it!

    QScopedPointer<aiScene> scene(new aiScene());
    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh * [1];
    scene->mMeshes[0] = buildSubMesh();
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial * [1];
    scene->mMaterials[0] = new aiMaterial();
    scene->mRootNode = new aiNode();
    scene->mRootNode->mNumMeshes = 1;
    scene->mRootNode->mMeshes = new unsigned [1];
    scene->mRootNode->mMeshes[0] = 0;

    Assimp::Exporter exporter;
    if (exporter.Export(scene.get(), outFormat.toLatin1().constData(), outFilePath.toLatin1().constData()) != AI_SUCCESS)
        throw runtime_error(QApplication::tr("Export: %1").arg(exporter.GetErrorString()).toStdString());

#ifndef QT_NO_DEBUG
    // hack; just leak memory for now in debug builds, mismatched new[] and delete[]
    // in msvc debug vs. release libs makes destructors crash in debug mode. :(
    // todo: fix this... somehow.
    scene.take();
    qWarning("  debug build: leaking memory as a workaround");
#endif

}


/**
 * Update the number of items that we're handling. May show/update the
 * progress dialog as a result.
 *
 * @param items Number of items.
 */
void ComplicatedProgress::updateCount (int items) {

    if (items >= 0) {
        int oldcount = subcounts.size();
        subcounts.resize(items);
        for (int k = oldcount; k < subcounts.size(); ++ k)
            subcounts[k] = 1;
        setProgress(curitem, cursubitem);
    }

}


/**
 * Update the number of subitems that we've got for a given item. May
 * show/update the progress dialog as a result.
 *
 * @param item      Item index. No-op if invalid.
 * @param subcount  Number of subitems. No-op if negative.
 */
void ComplicatedProgress::updateSubCount (int item, int subcount) {

    if (item >= 0 && item < subcounts.size() && subcount >= 0) {
        subcounts[item] = subcount;
        setProgress(curitem, cursubitem);
    }

}


/**
 * Set the progress indicator to 100%. May show the progress dialog.
 */
void ComplicatedProgress::setFinished () {

    setProgress(subcounts.size(), 0);

}


/**
 * Set the progress indicator. May show the progress dialog.
 *
 * @param item    Index of item currently being processed.
 * @param subitem Index of subitem currently being processed.
 * @return Normally returns true; will return false if user pressed 'Cancel'.
 */
bool ComplicatedProgress::setProgress (int item, int subitem) {

    curitem = item;
    cursubitem = subitem;

    int current = 0, max = 0;

    for (int k = 0; k < subcounts.size(); ++ k) {
        if (k < item)
            current += subcounts[k];
        max += subcounts[k];
    }

    if (item < subcounts.size())
        current += subitem;

    if (!dialog) {
        dialog.reset(new QProgressDialog(QApplication::tr("Splitting model..."), QApplication::tr("Cancel"), 0, max));
        dialog->setWindowModality(Qt::ApplicationModal);
        dialog->setMinimumDuration(0);
        dialog->setAutoClose(false);
        dialog->setAutoReset(false);
        dialog->show();
        QApplication::processEvents();
    } else {
        dialog->setMaximum(max);
    }

    dialog->setValue(current);
    return !dialog->wasCanceled();

}


/**
 * Check if a file looks more like STLA than STLB. Current algorithm just
 * looks to see if the file starts with the string "solid". Utility function
 * for guessInputFormat().
 *
 * @param filename Name of file.
 * @return True if it might be STLA. False if it might not be.
 */
static bool looksLikeAsciiSTL (QString filename) {

    QFile in(filename);
    if (!in.open(QFile::ReadOnly | QFile::Text))
        return false;

    // limit line length in case it's binary, but make it longish in case there's
    // some spaces before the word "solid" (dunno if that's allowed but...).
    return QString::fromLatin1(in.readLine(32)).trimmed().startsWith("solid");

}


/**
 * Check if a file looks more like PLYA than PLYB. Current algorithm looks
 * for the PLY format header and checks if it contains the word "ascii".
 * Utility function for guessInputFormat().
 *
 * @param filename Name of file.
 * @return True if it might be PLYA. False if it might not be.
 */
static bool looksLikeAsciiPLY (QString filename) {

    QFile in(filename);
    if (!in.open(QFile::ReadOnly | QFile::Text))
        return false;

    if (QString::fromLatin1(in.readLine(128)).trimmed() != "ply")
        return false;

    // limit # of lines read, for sanity
    for (int k = 0; k < 10; ++ k) {
        QStringList line = QString::fromLatin1(in.readLine(128)).split(" ", Qt::SkipEmptyParts);
        if (line.value(0) == "format")
            return line.value(1).contains("ascii");
    }

    return false;

}


/**
 * Try to guess which assimp export format ID corresponds to the format of the
 * given imported file.
 *
 * @param usedImporter   Importer that was used to import file.
 * @param inputFilename  Name of imported file.
 * @return A valid, non-empty export format ID, which may or may not have
 *         anything to do with the actual input format. It tries, though.
 */
static QString guessInputFormat (const Assimp::Importer &usedImporter,
                                 QString inputFilename)
{

    int imIndex = usedImporter.GetPropertyInteger("importerIndex", -1);
    const aiImporterDesc *imDesc = usedImporter.GetImporterInfo(imIndex);
    QStringList imExtns;
    if (imDesc)
        imExtns = QString(imDesc->mFileExtensions).split(" ", Qt::SkipEmptyParts);

    QString baseExtn = QFileInfo(inputFilename).suffix();

    // heuristic guess:
    //   choice #1) based on other choices + file content for certain types
    //   choice #2) exporter whose extensions match importer *and* id matches extension
    //   choice #3) exporter whose extensions match importer
    //   choice #4) exporter whose extensions match base file name
    //   choice #5) binary stl, i guess
    QStringList choices;

    // here we'll fill in choices 2, 3, and 4 (from the above list)
    QString exChoices[3];
    for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
        const aiExportFormatDesc *exDesc = aiGetExportFormatDescription(k);
        bool matchImExtn = imExtns.contains(exDesc->fileExtension, Qt::CaseInsensitive);
        bool matchImID = imExtns.contains(exDesc->id, Qt::CaseInsensitive);
        bool matchExtn = !baseExtn.compare(exDesc->fileExtension, Qt::CaseInsensitive);
        if (matchImExtn && matchImID)
            exChoices[0] = exDesc->id;
        if (matchImExtn)
            exChoices[1] = exDesc->id;
        if (matchExtn)
            exChoices[2] = exDesc->id;
    }
    for (int k = 0; k < 3; ++ k)
        if (exChoices[k] != "")
            choices.append(exChoices[k]);

    // certain types may let us find a better choice
    QString bestChoice = choices.value(0);
    if (bestChoice == "stl" || bestChoice == "stlb")
        choices.prepend(looksLikeAsciiSTL(inputFilename) ? "stl" : "stlb");
    else if (bestChoice == "ply" || bestChoice == "plyb")
        choices.prepend(looksLikeAsciiPLY(inputFilename) ? "ply" : "plyb");
    else if (bestChoice == "obj")
        choices.prepend("objnomtl"); // preferred because we've discarded materials
    else if (bestChoice == "fbx" || bestChoice == "gltf" || bestChoice == "glb")
        qWarning("Better output format choices may exist; but the decision isn't implemented for this type yet.");

    // and a fallback plan
    choices.append("stlb");

    Q_ASSERT(!choices.empty());
    return choices.front();

}


/**
 * Show a dialog allowing the user to choose from a list of output file
 * formats supported by assimp.
 *
 * @param  initial  Format ID of initial selection (ignored if invalid).
 * @return Selected export format ID, or "" if user cancelled.
 */
static QString pickOutputFormat (QString initial) {

    QMap<QString,QString> choicemap;
    QString initialText;

    for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
        const aiExportFormatDesc *desc = aiGetExportFormatDescription(k);
        QString text = QString(".%2 - %1 [%3]").arg(desc->description, desc->fileExtension, desc->id);
        choicemap[text] = desc->id;
        if (desc->id == initial)
            initialText = text;
    }

    QStringList choices = choicemap.keys();
    std::sort(choices.begin(), choices.end());
    int initialIndex = choices.indexOf(initialText);

    bool ok;
    QString choice = QInputDialog::getItem(nullptr, QString(), QApplication::tr("Choose output format:"), choices, initialIndex, false, &ok);

    return ok ? choicemap[choice] : QString();

}


/**
 * Does most of the work. Given a set of command line options, do whatever it
 * is this program is supposed to do. I'm not entirely sure how this function
 * ended up at some random spot in the middle of this source file. I could've
 * sworn it was closer to the bottom.
 *
 * @param opts_ Command line options.
 * @throw runtime_error if an error occurs.
 */
static void splitModel (const Options &opts_) {

    Options opts = opts_;
    ComplicatedProgress progress(1);

    // -------- load input ---------------------------------------------------

    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, ~aiComponent_MESHES);
    unsigned int preprocessFlags =
            aiProcess_Triangulate |
            aiProcess_PreTransformVertices |
            aiProcess_ValidateDataStructure |
            aiProcess_FindInvalidData |
            aiProcess_FindDegenerates |
            aiProcess_RemoveComponent;
    if (opts.unify)
        preprocessFlags |= aiProcess_JoinIdenticalVertices;
    const aiScene *inputScene = importer.ReadFile(opts.input.toStdString(), preprocessFlags);

    if (!inputScene)
        throw runtime_error(QApplication::tr("Import Error: %1").arg(importer.GetErrorString()).toStdString());
    else if (!inputScene->HasMeshes())
        throw runtime_error(QApplication::tr("Import Error: No mesh data found in file.").toStdString());

    QString inputFormat = guessInputFormat(importer, opts.input);

    progress.updateCount(inputScene->mNumMeshes + 1);

    // -------- show format picker if requested ------------------------------

    if (opts.pickFormat) {
        progress.setEnabled(false);
        opts.format = pickOutputFormat(opts.format == "" ? inputFormat : opts.format);
        if (opts.format == "")
            return;
        progress.setEnabled(true);
    }

    // -------- process each mesh --------------------------------------------

    OverwritePrompter prompter(opts, &progress);

    unsigned n = 0;
    for (unsigned k = 0; k < inputScene->mNumMeshes; ++ k) {
        if (!progress.setProgress(k + 1))
            throw runtime_error(QApplication::tr("Aborted. No more meshes will be written.").toStdString());
        Splitter split(inputScene->mMeshes[k]);
        progress.updateSubCount(k + 1, split.components.size());
        qDebug("split mesh %d: %d vertices, %d faces, %d components",
               k + 1, inputScene->mMeshes[k]->mNumVertices,
               inputScene->mMeshes[k]->mNumFaces,
               split.components.size());
        if (split.components.size() > 50) {
            int r = QMessageBox::warning(nullptr, QString(),
                                         QApplication::tr("Warning (input mesh #%1): About to write %2 output files. Is this OK?")
                                         .arg(k+1).arg(split.components.size()),
                                         QMessageBox::Abort | QMessageBox::Ok);
            if (r != QMessageBox::Ok)
                throw runtime_error(QApplication::tr("Aborted. No more meshes will be written.").toStdString());
        }
        for (int j = 0; j < split.components.size(); ++ j) {
            if (!progress.setProgress(k, j))
                throw runtime_error(QApplication::tr("Aborted. No more meshes will be written.").toStdString());
            const Component &c = split.components[j];
            qDebug("  component %d.%d: %d vertices, %d faces", k + 1, j + 1,
                   c.vertices.size(), c.faces.size());
            Component::SaveOptions sopts;
            sopts.sourceIndex = k;
            sopts.componentIndex = j;
            sopts.globalIndex = (n ++);
            sopts.defaultFormat = inputFormat;
            sopts.prompter = &prompter;
            c.save(opts, sopts);
        }
    }
    qDebug("total meshes created: %d", n);

    progress.setFinished();

}


/**
 * Given a proposed output filename, ask the user about overwriting it (if needed)
 * and decide on the actual output filename. If a prompt is displayed it'll be the
 * typical "yes" / "yes to all" / "no" / "no to all" / "cancel" dialog. If the user
 * makes a "... to all" choice, it'll be remembered for next time and they won't
 * be asked again.
 *
 * @param  filename  Proposed output filename.
 * @return Actual output filename, or "" if user cancelled.
 */
QString OverwritePrompter::getFilename (QString filename) {

    QFileInfo info(filename);
    if (!info.exists())
        return filename;

    bool overwriteThisTime;
    if (overwrite.has_value()) {
        overwriteThisTime = overwrite.value();
    } else {
        progress->setEnabled(false);
        int choice = QMessageBox::warning(nullptr, QString(), filename + "\n\n" + QApplication::tr("File exists. Overwrite?"),
                                          QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No |
                                          QMessageBox::NoToAll | QMessageBox::Abort, QMessageBox::No);
        progress->setEnabled(true);
        switch (choice) {
        case QMessageBox::Yes:
            overwriteThisTime = true;
            break;
        case QMessageBox::YesToAll:
            overwriteThisTime = true;
            overwrite = true;
            break;
        case QMessageBox::No:
            overwriteThisTime = false;
            break;
        case QMessageBox::NoToAll:
            overwriteThisTime = false;
            overwrite = false;
            break;
        default:
            return QString();
        }
    }

    if (!overwriteThisTime) {
        QDir basePath = info.dir();
        QString baseName = info.completeBaseName();
        QString baseExtn = info.suffix();
        int number = 2;
        do {
            filename = baseName + QString(" (%1).").arg(number) + baseExtn;
            filename = basePath.absoluteFilePath(filename);
            ++ number;
        } while (QFileInfo(filename).exists());
    }

    return filename;

}


#ifdef Q_OS_WIN

/**
 * Update the system registry to enable or disable the "Split 3D Model..." shell context
 * menus. All file types that assimp can import will be updated here.
 *
 * @param install  True to enable menus, false to disable them.
 * @param verbcl   Extra command line params to add to verb command. Ignored if !install.
 * @param elevate  True to attempt to run as admin if registry edits fail.
 * @param showok   True to show a dialog on success, false to be quiet about it.
 * @return Process exit code.
 * @bug   If the menus are enabled, then assimp drops support for an extension and the
 *        application is updated, then the menus are disabled, then the no-longer-supported
 *        extensions won't be cleaned up. I probably won't fix this. The installer will
 *        unregister the context menus before reinstall anyways, which should mitigate
 *        this if it *does* happen.
 */
static int setupShellMenus (bool install, QString verbcl, bool elevate, bool showok) {

    std::string extnstrs;
    Assimp::Importer().GetExtensionList(extnstrs);
    QStringList extns = QString::fromStdString(extnstrs).replace("*", ";").split(";", Qt::SkipEmptyParts);

    QString executable = QFileInfo(QApplication::applicationFilePath()).canonicalFilePath();
    executable = QDir::toNativeSeparators(executable);
    QString command = QString("\"%1\" \"%2\" %3").arg(executable, "%1", verbcl).trimmed();
    qint64 pid = QApplication::applicationPid(); // for logging

    if (install)
        qDebug().noquote() << pid << "setupShellMenus: install:" << command;
    else
        qDebug().noquote() << pid << "setupShellMenus: uninstall";

    QSettings cls("HKEY_LOCAL_MACHINE\\SOFTWARE\\Classes\\SystemFileAssociations", QSettings::NativeFormat);

    bool attemptedChanges = false;
    foreach (QString key, extns) {
        key = key.trimmed();
        if (!key.startsWith(".") || key.length() <= 1 || key.contains("/") || key.contains("\\")) {
            qWarning() << "skipping strange extension" << key;
            continue;
        }
        if (install) {
            cls.beginGroup(key);
            cls.beginGroup("shell");
            cls.beginGroup("ModelSplit.Split");
            cls.setValue(".", QApplication::tr("Split 3D Model..."));
            cls.beginGroup("command");
            if (cls.value(".").toString() != command) {
                qDebug().noquote() << pid << "setupShellMenus: registering menu for" << key;
                cls.setValue(".", command);
                attemptedChanges = true;
            }
            cls.endGroup();
            cls.endGroup();
            cls.endGroup();
            cls.endGroup();
        } else {
            cls.beginGroup(key);
            cls.beginGroup("shell");
            if (cls.childGroups().contains("ModelSplit.Split", Qt::CaseInsensitive)) {
                qDebug().noquote() << pid << "setupShellMenus: unregistering menu for" << key;
                cls.beginGroup("ModelSplit.Split");
                cls.remove("");
                cls.endGroup();
                attemptedChanges = true;
            }
            cls.endGroup();
            cls.endGroup();
        }
    }

    cls.sync();
    qDebug() << pid << "setupShellMenus:" << cls.status() << "(attempted changes:" << attemptedChanges << ")";

    if (cls.status() == QSettings::AccessError && elevate && attemptedChanges) {
        qWarning() << pid << "setupShellMenus: access denied, attempting to elevate...";
        //if (QProcess::execute("runas", { executable, install ? "--register" : "--unregister" }) < 0)
        //    QMessageBox::critical(nullptr, QString(), "Could not update registry: Failed elevation attempt.");
        QString parameters = QString("%1 --regopts \"%2\"").arg(install ? "--register" : "--unregister", verbcl);
        if (PtrToInt(ShellExecuteW(NULL, L"runas", executable.toStdWString().c_str(), parameters.toStdWString().c_str(), NULL, SW_SHOW)) <= 32)
            QMessageBox::critical(nullptr, QString(), QApplication::tr("Could not update registry: Failed to run elevated process."));
    } else {
        if (cls.status() == QSettings::NoError || !attemptedChanges) {
            if (showok)
                QMessageBox::information(nullptr, QString(), QApplication::tr("Shell context menus updated."));
        } else if (cls.status() == QSettings::AccessError) {
            QMessageBox::critical(nullptr, QString(), QApplication::tr("Could not update registry: Access denied."));
        } else {
            QMessageBox::critical(nullptr, QString(), QApplication::tr("Could not update registry."));
        }
    }

    qDebug() << pid << "setupShellMenus: finished";

    return 0;

}

#endif


/**
 * Main entry point, does the usual -- parses command line parameters, etc.
 * @param argc  Arrrrrrgh
 * @param argv  ARRRRRRRRRRRRRGH
 * @return      0 on success, 1 on error.
 */
int main (int argc, char *argv[]) {

    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    QApplication::setApplicationName("Model Splitter");
    QApplication::setOrganizationName("Jason C");

    QTranslator t;
    t.load(":/i18n/modelsplit_" + QLocale::system().name());
    a.installTranslator(&t);

    QCommandLineParser p;
    QCommandLineOption help = p.addHelpOption();
    QCommandLineOption version = p.addVersionOption();
    p.addPositionalArgument(QApplication::tr("input"), QApplication::tr("Input file name."));
    p.addOptions({
                     { "no-unify", QApplication::tr("Do not unify duplicate vertices.") }
                    ,{ "y", QApplication::tr("Always overwrite, don't prompt.") }
                    ,{ "n", QApplication::tr("Never overwrite, don't prompt (overrides -y).") }
                    ,{ "f", QApplication::tr("Output format (default is same as input, or OBJ if unsupported)."), "format" }
                    ,{ "F", QApplication::tr("Pick output format from dialog (overrides -f).") }
#ifdef Q_OS_WIN
                    ,{ "register", QApplication::tr("Register shell context menus for model file types.") }
                    ,{ "regopts", QApplication::tr("Specify extra modelsplit command line params for --register."), "regopts" }
                    ,{ "unregister", QApplication::tr("Deregister shell context menus created by --register.") }
                    ,{ "elevate", QApplication::tr("Try to elevate to admin if [un]register fails.") }
                    ,{ "quieter", QApplication::tr("Don't show message box on success (for --[un]register).") }
#endif
                 });
    bool parsed = p.parse(a.arguments());

    if (p.isSet(help) || p.isSet("help-all") || p.isSet(version))
        p.process(a);
#ifdef Q_OS_WIN
    else if (parsed && (p.isSet("register") || p.isSet("unregister")))
        return setupShellMenus(p.isSet("register"), p.value("regopts"), p.isSet("elevate"), !p.isSet("quieter"));
#endif
    else if (!parsed || p.positionalArguments().size() != 1)
        p.showHelp(1);

    Options opts;
    opts.input = p.positionalArguments()[0];
    opts.unify = !p.isSet("no-unify");
    opts.overwrite = (p.isSet("n") ? false : (p.isSet("y") ? true : optional<bool>()));
    opts.format = p.value("f");
    opts.pickFormat = p.isSet("F");

    bool success = false;
    try {
        splitModel(opts);
        success = true;
    } catch (const exception &x) {
        QMessageBox::critical(nullptr, QString(), x.what());
    }

    return success ? 0 : 1;

}
