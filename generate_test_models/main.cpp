#include <cassert>
#include <cstdio>
#include <string>
#include <list>
#include <memory>
#include <set>
#include <vector>
#include <filesystem>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

using namespace std;

typedef std::shared_ptr<aiScene> aiScenePtr;

#define TEST_LARGE_MODELS     1
#define LARGE_MODEL_FACES     1000000
#define PRESERVE_IMPORT_TEST_MODELS 0


static void setFace (aiFace &face, unsigned a, unsigned b, unsigned c) {

    delete[] face.mIndices;
    face.mNumIndices = 3;
    face.mIndices = new unsigned[3];
    face.mIndices[0] = a;
    face.mIndices[1] = b;
    face.mIndices[2] = c;

}


/** Makes a tetrahedron in bounding box (-1,-1,-1) -> ( 1, 1, 1), translated. */
static aiMesh * buildObject (bool normals, ai_real x, ai_real y = 0, ai_real z = 0) {

    aiMesh *mesh = new aiMesh();

    mesh->mNumVertices = 4;
    mesh->mVertices = new aiVector3D[4];
    mesh->mVertices[0].Set(1, 1, 1);
    mesh->mVertices[1].Set(-1, 1, -1);
    mesh->mVertices[2].Set(-1, -1, 1);
    mesh->mVertices[3].Set(1, -1, -1);

    if (normals)
        mesh->mNormals = new aiVector3D[4];

    mesh->mNumFaces = 4;
    mesh->mFaces = new aiFace[4];
    setFace(mesh->mFaces[0], 0, 1, 2);
    setFace(mesh->mFaces[1], 0, 2, 3);
    setFace(mesh->mFaces[2], 0, 3, 1);
    setFace(mesh->mFaces[3], 3, 2, 1);

    for (unsigned k = 0; k < mesh->mNumVertices; ++ k) {
        if (normals) {
            mesh->mNormals[k] = mesh->mVertices[k];
            mesh->mNormals[k].Normalize();
        }
        mesh->mVertices[k] += aiVector3D(x, y, z);
    }

    return mesh;

}


#if TEST_LARGE_MODELS
// makes a really long triangle strip
static aiMesh * buildLargeObject (unsigned faces) {

    printf("buildLargeObject: generating %d faces...\n", faces);
    aiMesh *mesh = new aiMesh();

    mesh->mNumVertices = faces + 2;
    mesh->mVertices = new aiVector3D[mesh->mNumVertices];

    unsigned bottomv = (faces + 1) / 2 + 1;
    for (unsigned v = 0; v < bottomv; ++ v)
        mesh->mVertices[v].Set((ai_real)v, -0.5f, 0);
    for (unsigned v = bottomv; v < mesh->mNumVertices; ++ v)
        mesh->mVertices[v].Set(0.5f + v - bottomv, 0.5f, 0);

    mesh->mNumFaces = faces;
    mesh->mFaces = new aiFace[faces];

    for (unsigned f = 0; f < faces; ++ f) {
        if (!(f % 2))
            setFace(mesh->mFaces[f], f / 2, f / 2 + 1, f / 2 + bottomv);
        else
            setFace(mesh->mFaces[f], f / 2 + bottomv, (f + 1) / 2, f / 2 + 1 + bottomv);
    }

    printf("buildLargeObject: done\n");
    return mesh;

}
#endif


// returned scene will have ownership of supplies meshes after this.
static aiScenePtr buildScene (const vector<aiMesh *> &meshes) {

    aiScenePtr scene(new aiScene());

    scene->mNumMeshes = (unsigned)meshes.size();
    scene->mMeshes = new aiMesh *[scene->mNumMeshes];
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial *[1];
    scene->mMaterials[0] = new aiMaterial();
    scene->mRootNode = new aiNode();
    scene->mRootNode->mNumMeshes = scene->mNumMeshes;
    scene->mRootNode->mMeshes = new unsigned[scene->mNumMeshes];

    for (unsigned k = 0; k < scene->mNumMeshes; ++ k) {
        scene->mMeshes[k] = meshes[k];
        scene->mRootNode->mMeshes[k] = k;
    }

    return scene;


}


static aiScenePtr buildScene (int nobjects = 3, bool normals = false) {

    vector<aiMesh *> objects;
    for (int k = 0; k < nobjects; ++ k)
        objects.push_back(buildObject(normals, (ai_real)2.5 * k));

    return buildScene(objects);

}


#if TEST_LARGE_MODELS
static aiScenePtr buildLargeScene () {

    vector<aiMesh *> objects;
    objects.push_back(buildLargeObject(LARGE_MODEL_FACES));

    return buildScene(objects);

}
#endif


static string nonull (const char *str) {
    return str ? str : "<null>";
}


// this is a hacky drop-in replacement for aiScenePtr in ImportResult meant as
// a quick memory optimization after large model tests were added. it just
// lets the imported scene be freed and was able to be added without me having
// to really mess with any other code.
struct SceneGhost {

    bool nullScene;
    unsigned mFlags;
    unsigned mNumMeshes;

    SceneGhost () : nullScene(true) { }
    const SceneGhost * operator -> () const { return this; }
    operator bool () const { return !nullScene; }

    void reset (aiScene *scene) {
        nullScene = !scene;
        mFlags = scene ? scene->mFlags : 0;
        mNumMeshes = scene ? scene->mNumMeshes : 0;
        delete scene;
    }

};


struct ImportResult {

    string testDescription;

    int exporterIndex;
    const aiExportFormatDesc *exporter;
    aiScenePtr exported; // will be non-null even on failure so...
    bool exportSuccess;  // ...this will tell if it failed or not.
    string exportError;
    list<string> exportedFiles;

    int importerIndex;
    const aiImporterDesc *importer;
    unsigned importPP;
    //aiScenePtr imported; // null on failure.
    SceneGhost imported;
    string importError;

    explicit ImportResult (string desc) :
        testDescription(desc),
        exporterIndex(-1), exporter(nullptr), exportSuccess(false),
        importerIndex(-1), importer(nullptr), importPP(0) { }

};


struct ImporterInfo {

    int importerIndex;
    const aiImporterDesc *importer;
    list<ImportResult> results;
    set<int> exporters;

    explicit ImporterInfo (int index) :
        importerIndex(index), importer(aiGetImportFormatDescription(index)) { }

};


static ImportResult testExportImport (string description, aiScenePtr scene, int exporterIndex, unsigned importPP) {

    static const char EXPORT_TEST_PATH_[] = "~import_coverage_dir";

    const aiExportFormatDesc *exporterDesc = aiGetExportFormatDescription(exporterIndex);
    Assimp::Exporter exporter;
    Assimp::Importer importer;
    ImportResult result(description);

#if PRESERVE_IMPORT_TEST_MODELS
    string descriptiondir;
    for (auto ch = description.cbegin(); ch != description.cend(); ++ ch)
        descriptiondir += isalnum(*ch) ? *ch : '_';
    string EXPORT_TEST_PATH = string(EXPORT_TEST_PATH_) + "/" + descriptiondir + "/" + exporterDesc->id;
#else
    static const char *EXPORT_TEST_PATH = EXPORT_TEST_PATH_;
#endif

    filesystem::remove_all(EXPORT_TEST_PATH);
    filesystem::create_directories(EXPORT_TEST_PATH);

    string filename = string(EXPORT_TEST_PATH) + "/scene." + exporterDesc->fileExtension;

    result.exporterIndex = exporterIndex;
    result.exporter = exporterDesc;
    result.exportSuccess = (exporter.Export(scene.get(), exporterDesc->id, filename, 0) == AI_SUCCESS);
    result.exportError = nonull(exporter.GetErrorString());
    std::transform(filesystem::directory_iterator(EXPORT_TEST_PATH), filesystem::directory_iterator(),
                   std::back_inserter(result.exportedFiles),
                   [] (auto &d) { return d.path().filename().string(); });

    if (result.exportSuccess) {
        importer.ReadFile(filename, importPP);
        result.importError = nonull(importer.GetErrorString());
        result.importerIndex = importer.GetPropertyInteger("importerIndex", -1);
        result.importer = aiGetImportFormatDescription(result.importerIndex);
        result.importPP = importPP;
        result.imported.reset(importer.GetOrphanedScene());
        printf("exporter: %10s %s importer: %s (%s)\n", exporterDesc->id,
               description.c_str(), result.importer ? result.importer->mName : "none",
               result.imported ? "ok" : result.importError.c_str());
    } else {
        printf("exporter: %10s %s (failed: %s)\n", exporterDesc->id,
               description.c_str(), result.exportError.c_str());
    }

#if !PRESERVE_IMPORT_TEST_MODELS
    filesystem::remove_all(EXPORT_TEST_PATH);
#endif
    return result;

}


struct TestMeshes {
    aiScenePtr oneMesh;
    aiScenePtr manyMeshes;
#if TEST_LARGE_MODELS
    aiScenePtr largeMesh;
#endif
};


static void testExporter (vector<ImportResult> &appendTo, const TestMeshes &tm, int exporterIndex) {

    appendTo.push_back(testExportImport("meshes=1, validation=no ", tm.oneMesh, exporterIndex, 0));
    appendTo.push_back(testExportImport("meshes=5, validation=no ", tm.manyMeshes, exporterIndex, 0));
#if TEST_LARGE_MODELS
    appendTo.push_back(testExportImport("large   , validation=no ", tm.largeMesh, exporterIndex, 0));
#endif
    appendTo.push_back(testExportImport("meshes=1, validation=yes", tm.oneMesh, exporterIndex, aiProcess_ValidateDataStructure));
    appendTo.push_back(testExportImport("meshes=5, validation=yes", tm.manyMeshes, exporterIndex, aiProcess_ValidateDataStructure));
#if TEST_LARGE_MODELS
    appendTo.push_back(testExportImport("large   , validation=yes ", tm.largeMesh, exporterIndex, aiProcess_ValidateDataStructure));
#endif

}


static string quoteCSV (string value) {

    string quoted;

    for (int k = 0; k < value.size(); ++ k) {
        if (value[k] == '\"')
            quoted += "\"\"";
        else
            quoted += value[k];
    }

    return "\"" + quoted + "\"";

}


static string quoteCSV (const char *value) {

    return quoteCSV(nonull(value));

}


static string numberOr (int number, string ifNegativeOne) {

    if (number == -1)
        return ifNegativeOne;
    else {
        char buf[100];
        snprintf(buf, sizeof(buf), "%d", number);
        return buf;
    }

}


static bool isInteresting (const ImportResult &result) {

    return
            result.exporterIndex == -1 ||
            !result.exportSuccess ||
            result.importerIndex == -1 ||
            !result.imported ||
            result.imported->mFlags ||
            !result.imported->mNumMeshes;

}


static bool isInteresting (const ImporterInfo &importer) {

    for (auto pr = importer.results.cbegin(); pr != importer.results.cend(); ++ pr)
        if (isInteresting(*pr))
            return true;

    return importer.exporters.empty();

}


/*
static bool ImporterNameAscending (const ImporterInfo &a, const ImporterInfo &b) {

    if (a.importerIndex == b.importerIndex)
        return false;
    else if (a.importerIndex == -1 || !a.importer)
        return true;
    else {
        assert(a.importer);
        assert(b.importer);
        return strcmp(a.importer->mName, b.importer->mName) < 0;
    }

}
*/


static bool writeReport (const char *filename, const vector<ImportResult> &results, const vector<ImporterInfo> &importers_) {

    printf("saving report to %s...\n", filename);

    FILE *csv = fopen(filename, "wt");
    if (!csv) {
        perror("import_coverage.csv");
        return false;
    }

    // we'll just put all tables in the same csv i can format it by
    // hand afterwards, whatever.

    fprintf(csv, ",assimp,%d.%d.%d\n\n",
            aiGetVersionMajor(),
            aiGetVersionMinor(),
            aiGetVersionRevision());

    fprintf(csv, ",exporter,,,,export operation,,,,importer,,,import operation,,,,\n");
    fprintf(csv, ",index,id,description,extension,test,success?,message,files,index,description,extensions,success?,sceneflags,meshes,message,\n");

    for (auto pr = results.begin(); pr != results.end(); ++ pr) {
        string filenames;
        for (auto pfn = pr->exportedFiles.begin(); pfn != pr->exportedFiles.end(); ++ pfn)
            filenames += (pfn == pr->exportedFiles.begin() ? "" : ", ") + *pfn;
        fprintf(csv, "%s,",
                isInteresting(*pr) ? "*" : "");
        fprintf(csv, "%d,%s,%s,%s,",
                pr->exporterIndex,
                quoteCSV(pr->exporter->id).c_str(),
                quoteCSV(pr->exporter->description).c_str(),
                quoteCSV(pr->exporter->fileExtension).c_str());
        fprintf(csv, "%s,%s,%s,%s,",
                quoteCSV(pr->testDescription).c_str(),
                pr->exportSuccess ? "ok" : "failed",
                quoteCSV(pr->exportError).c_str(),
                quoteCSV(filenames).c_str());
        fprintf(csv, "%d,%s,%s,",
                pr->importerIndex /*numberOr(pr->importerIndex, "none").c_str() */,
                pr->importer ? quoteCSV(pr->importer->mName).c_str() : "",
                pr->importer ? quoteCSV(pr->importer->mFileExtensions).c_str() : "");
        fprintf(csv, "%s,0x%08x,%d,%s,",
                pr->imported ? "ok" : "failed",
                pr->imported ? pr->imported->mFlags : 0,
                pr->imported ? pr->imported->mNumMeshes : 0,
                quoteCSV(pr->importError).c_str());
        fprintf(csv, "\n");
    }

    fprintf(csv, "\n\n");
    fprintf(csv, ",importer,,,exporter,,,,\n");
    fprintf(csv, ",index,name,extensions,index,id,name,extensions,\n");

    vector<ImporterInfo> importers = importers_;
    //stable_sort(importers.begin(), importers.end(), ImporterNameAscending);

    for (auto pi = importers.begin(); pi != importers.end(); ++ pi) {
        set<int> exporters = pi->exporters;
        if (exporters.empty()) // make sure importers with no exporters are printed
            exporters.insert(-1);
        for (auto px = exporters.begin(); px != exporters.end(); ++ px) {
            const aiExportFormatDesc *x = aiGetExportFormatDescription(*px);
            fprintf(csv, "%s,",
                    pi->exporters.empty() ? "-" : (isInteresting(*pi) ? "*" : ""));
            fprintf(csv, "%d,%s,%s,",
                    pi->importerIndex,
                    quoteCSV(pi->importer->mName).c_str(),
                    quoteCSV(pi->importer->mFileExtensions).c_str());
            fprintf(csv, "%s,%s,%s,%s,",
                    numberOr(*px, "").c_str(),
                    x ? quoteCSV(x->id).c_str() : "",
                    x ? quoteCSV(x->description).c_str() : "",
                    x ? quoteCSV(x->fileExtension).c_str() : "");
            fprintf(csv, "\n");
        }
    }

    fclose(csv);
    return true;

}


int main (int argc, char * argv[]) {

    const char *action = (argc >= 2 ? argv[1] : "");

    if (!strcmp(action, "import_coverage")) {

        TestMeshes testdata;
        testdata.oneMesh = buildScene(1);
        testdata.manyMeshes = buildScene(5);
#if TEST_LARGE_MODELS
        testdata.largeMesh = buildLargeScene();
#endif

        //Assimp::Exporter().Export(testdata.largeMesh.get(), "plyb", "large_scene.ply");

        vector<ImportResult> results;
        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k)
            testExporter(results, testdata, k);

        vector<ImporterInfo> importers;
        for (unsigned k = 0; k < aiGetImportFormatCount(); ++ k)
            importers.push_back(ImporterInfo(k));

        for (auto presult = results.begin(); presult != results.end(); ++ presult) {
            if (presult->importerIndex != -1) {
                assert(presult->importerIndex >= 0 && presult->importerIndex < importers.size());
                assert(presult->exporter != nullptr);
                importers[presult->importerIndex].results.push_back(*presult);
                importers[presult->importerIndex].exporters.insert(presult->exporterIndex);
            }
        }

        for (auto pimporter = importers.begin(); pimporter != importers.end(); ++ pimporter) {
            if (pimporter->exporters.empty())
                printf("--- importer: %s (no matching exporters)\n", pimporter->importer->mName);
            else {
                for (auto pexporter = pimporter->exporters.begin(); pexporter != pimporter->exporters.end(); ++ pexporter) {
                    const aiExportFormatDesc *exporter = aiGetExportFormatDescription(*pexporter);
                    printf("+++ importer: %s <- [%s] %s (.%s)\n", pimporter->importer->mName,
                           exporter->id, exporter->description, exporter->fileExtension);
                }
            }
        }

        if (!writeReport("importer_coverage.csv", results, importers))
            return 1;

    } else if (!strcmp(action, "dummy_files")) {

        aiString extns;
        aiGetExtensionList(&extns);

        for (char *token = strtok(extns.data, " ;*"); token; token = strtok(nullptr, " ;*")) {
            string filename = "dummy";
            filename += token;
            printf("[%-10s]: %s\n", token, filename.c_str());
            fclose(fopen(filename.c_str(), "a"));
        }

    } else if (!*action) {

        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
            const aiExportFormatDesc *desc = aiGetExportFormatDescription(k);
            char filename[100];
            snprintf(filename, sizeof(filename), "model-%02d-%s.%s", k, desc->id, desc->fileExtension);
            printf("[%-10s] %s...\n", desc->id, filename);
            Assimp::Exporter exporter;
            if (exporter.Export(buildScene(3, true).get(), desc->id, filename) != AI_SUCCESS)
                fprintf(stderr, "  error: %s\n", exporter.GetErrorString());
        }

    } else {

        fprintf(stderr, "usage: %s [ action ]\n\n", argv[0]);
        fprintf(stderr, "actions:\n\n"
                        "  (default)         generate test models\n"
                        "  import_coverage   find importer / exporter associations\n"
                        "  dummy_files       generate dummy files for all extensions\n"
                        "\n");
        return 1;

    }

    return 0;

}
