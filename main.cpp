#include <QApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QDir>
#include <optional>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/Exporter.hpp>

#define WEIGHTED_UNION 0 // hurts more than it helps in testing
#define DEBUG_TIMES    0

using std::exception;
using std::runtime_error;
using std::optional;

struct Options {
    QString input;
    bool unify;
    optional<bool> overwrite;
    QString format;
};

struct OverwritePrompter {
    explicit OverwritePrompter (const Options &opts) : overwrite(opts.overwrite) { }
    QString getFilename (QString filename);
private:
    optional<bool> overwrite;
};

struct Component {

    struct SaveOptions {
        int sourceIndex;
        int componentIndex;
        int globalIndex;
        QString defaultFormat;
        OverwritePrompter *prompter;
    };

    const aiMesh *source;
    QVector<int> vertices;
    QVector<int> faces;

    explicit Component (const aiMesh *source = nullptr) : source(source) { }

    void save (const Options &opts, const SaveOptions &sopts) const;
    aiMesh * buildSubMesh () const;

};

struct Splitter {

    const aiMesh *mesh;
    QVector<Component> components;

    explicit Splitter (const aiMesh *mesh) : mesh(mesh) { split(); }
    void split ();

private:

    static const int Unused = INT_MIN;

#if WEIGHTED_UNION
    QVector<int> treeSize;
#endif
    QVector<int> vertexComponent;
    QVector<int> faceComponent;

    int vroot (int v);
    void vunion (int a, int b);

};


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

    // replace vertex equivalency classes with component ids; i don't know how to
    // optimize this but it seems to be doing fine as is.
    for (unsigned k = 0; k < mesh->mNumVertices; ++ k) {
        if (usedVertices[k]) {
            int name = componentIndex[vroot(k)];
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


int Splitter::vroot (int v) {

    while (v != vertexComponent[v]) {
        vertexComponent[v] = vertexComponent[vertexComponent[v]];
        v = vertexComponent[v];
    }

    return v;

}


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


aiMesh * Component::buildSubMesh () const {

#if 1

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

#else

    aiVector3D *vertices = new aiVector3D [] {          // deleted: mesh.h:758
        { -1, -1, 0 },
        { 0, 1, 0 },
        { 1, -1, 0 }
    };

    aiFace *faces = new aiFace[1];                      // deleted: mesh.h:784
    faces[0].mNumIndices = 3;
    faces[0].mIndices = new unsigned [] { 0, 1, 2 };    // deleted: mesh.h:149

    aiMesh *mesh = new aiMesh();                        // deleted: Version.cpp:150
    mesh->mNumVertices = 3;
    mesh->mVertices = vertices;
    mesh->mNumFaces = 1;
    mesh->mFaces = faces;

    return mesh;

#endif

}


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
        throw runtime_error("Aborted. No more meshes will be written.");

#ifndef QT_NO_DEBUG
    qDebug().noquote() << "    --> " << outFilePath;
#endif

    // all right; do it!

    QScopedPointer<aiScene> scene(new aiScene());
    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh * [] { buildSubMesh() };
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial * [] { new aiMaterial() };
    scene->mRootNode = new aiNode();
    scene->mRootNode->mNumMeshes = 1;
    scene->mRootNode->mMeshes = new unsigned [] { 0 };

    Assimp::Exporter exporter;
    if (exporter.Export(scene.get(), outFormat.toLatin1().constData(), outFilePath.toLatin1().constData()) != AI_SUCCESS)
        throw runtime_error(QString("Export: %1").arg(exporter.GetErrorString()).toStdString());

#ifndef QT_NO_DEBUG
    // hack; just leak memory for now in debug builds, mismatched new[] and delete[]
    // in msvc debug vs. release libs makes destructors crash in debug mode. :(
    // todo: fix this... somehow.
    scene.take();
#endif

}


static bool looksLikeAsciiSTL (QString filename) {

    QFile in(filename);
    if (!in.open(QFile::ReadOnly | QFile::Text))
        return false;

    // limit line length in case it's binary, but make it longish in case there's
    // some spaces before the word "solid" (dunno if that's allowed but...).
    return QString::fromLatin1(in.readLine(32)).trimmed().startsWith("solid");

}


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


static void splitModel (const Options &opts) {

    // -------- load input ---------------------------------------------------

    Assimp::Importer importer;
    importer.SetPropertyInteger(AI_CONFIG_PP_RVC_FLAGS, ~aiComponent_MESHES);
    const aiScene *inputScene = importer.ReadFile(opts.input.toStdString(),
                                                  aiProcess_Triangulate |
                                                  aiProcess_PreTransformVertices |
                                                  aiProcess_ValidateDataStructure |
                                                  aiProcess_FindInvalidData |
                                                  aiProcess_FindDegenerates |
                                                  aiProcess_RemoveComponent |
                                                  (opts.unify ? aiProcess_JoinIdenticalVertices : 0));

    if (!inputScene)
        throw runtime_error(QString("Import Error: %1").arg(importer.GetErrorString()).toStdString());
    else if (!inputScene->HasMeshes())
        throw runtime_error("Import Error: No mesh data found in file.");

    QString inputFormat = guessInputFormat(importer, opts.input);

    // -------- process each mesh --------------------------------------------

    OverwritePrompter prompter(opts);

    unsigned n = 0;
    for (unsigned k = 0; k < inputScene->mNumMeshes; ++ k) {
        Splitter split(inputScene->mMeshes[k]);
        qDebug("split mesh %d: %d vertices, %d faces, %d components",
               k + 1, inputScene->mMeshes[k]->mNumVertices,
               inputScene->mMeshes[k]->mNumFaces,
               split.components.size());
        if (split.components.size() > 50) {
            int r = QMessageBox::warning(nullptr, QString(),
                                         QString("Warning (input mesh #%1): About to write %2 output files. Is this OK?")
                                         .arg(k+1).arg(split.components.size()),
                                         QMessageBox::Abort | QMessageBox::Ok);
            if (r != QMessageBox::Ok)
                throw runtime_error("Aborted. No more meshes will be written.");
        }
        for (int j = 0; j < split.components.size(); ++ j) {
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

}


QString OverwritePrompter::getFilename (QString filename) {

    QFileInfo info(filename);
    if (!info.exists())
        return filename;

    bool overwriteThisTime;
    if (overwrite.has_value()) {
        overwriteThisTime = overwrite.value();
    } else {
        int choice = QMessageBox::warning(nullptr, QString(), filename + "\n\nFile exists. Overwrite?",
                                          QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No |
                                          QMessageBox::NoToAll | QMessageBox::Abort, QMessageBox::No);
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
        case QMessageBox::Abort:
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


int main (int argc, char *argv[]) {

    QApplication a(argc, argv);
    QApplication::setApplicationName("Model Splitter");
    QApplication::setApplicationVersion("0.0");
    QApplication::setOrganizationName("Jason C");

    QCommandLineParser p;
    QCommandLineOption help = p.addHelpOption();
    QCommandLineOption version = p.addVersionOption();
    p.addPositionalArgument("input", "Input file name.");
    p.addOptions({
                     { "no-unify", "Do not unify duplicate vertices." },
                     { "y", "Always overwrite, don't prompt." },
                     { "n", "Never overwrite, don't prompt (overrides -y)." },
                     { "f", "Output format (default is same as input, or OBJ if unsupported).", "format" }
                 });
    bool parsed = p.parse(a.arguments());

    if (p.isSet(help) || p.isSet("help-all") || p.isSet(version))
        p.process(a);
    else if (!parsed || p.positionalArguments().size() != 1)
        p.showHelp();

    Options opts;
    opts.input = p.positionalArguments()[0];
    opts.unify = !p.isSet("no-unify");
    opts.overwrite = (p.isSet("n") ? false : (p.isSet("y") ? true : optional<bool>()));
    opts.format = p.value("f");

    bool success = false;
    try {
        splitModel(opts);
        success = true;
    } catch (const exception &x) {
        QMessageBox::critical(nullptr, QString(), x.what());
    }

    return success ? 0 : 1; //a.exec();

}
