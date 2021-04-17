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

using namespace std;

typedef std::shared_ptr<aiScene> aiScenePtr;


static void setFace (aiFace &face, unsigned a, unsigned b, unsigned c) {

    delete[] face.mIndices;
    face.mNumIndices = 3;
    face.mIndices = new unsigned[3];
    face.mIndices[0] = a;
    face.mIndices[1] = b;
    face.mIndices[2] = c;

}


/** Makes a tetrahedron in bounding box (-1,-1,-1) -> ( 1, 1, 1), translated. */
static aiMesh * buildObject (ai_real x, ai_real y = 0, ai_real z = 0) {

    aiMesh *mesh = new aiMesh();

    mesh->mNumVertices = 4;
    mesh->mVertices = new aiVector3D[4];
    mesh->mVertices[0].Set(1, 1, 1);
    mesh->mVertices[1].Set(-1, 1, -1);
    mesh->mVertices[2].Set(-1, -1, 1);
    mesh->mVertices[3].Set(1, -1, -1);

    mesh->mNumFaces = 4;
    mesh->mFaces = new aiFace[4];
    setFace(mesh->mFaces[0], 0, 1, 2);
    setFace(mesh->mFaces[1], 0, 2, 3);
    setFace(mesh->mFaces[2], 0, 3, 1);
    setFace(mesh->mFaces[3], 3, 2, 1);

    for (unsigned k = 0; k < mesh->mNumVertices; ++ k)
        mesh->mVertices[k] += aiVector3D(x, y, z);

    return mesh;

}


static aiScenePtr buildScene (int nobjects = 3) {

    aiScenePtr scene(new aiScene());

    scene->mNumMeshes = nobjects;
    scene->mMeshes = new aiMesh *[nobjects];
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial *[1];
    scene->mMaterials[0] = new aiMaterial();
    scene->mRootNode = new aiNode();
    scene->mRootNode->mNumMeshes = nobjects;
    scene->mRootNode->mMeshes = new unsigned[nobjects];

    for (int k = 0; k < nobjects; ++ k) {
        scene->mMeshes[k] = buildObject((ai_real)2.5 * k);
        scene->mRootNode->mMeshes[k] = k;
    }

    return scene;

}


static string nonull (const char *str) {
    return str ? str : "<null>";
}


struct ImportResult {

    string testDescription;

    int exporterIndex;
    const aiExportFormatDesc *exporter;
    aiScenePtr exported; // will be non-null even on failure so...
    bool exportSuccess;  // ...this will tell if it failed or not.
    string exportError;

    int importerIndex;
    const aiImporterDesc *importer;
    unsigned importPP;
    aiScenePtr imported; // null on failure.
    string importError;

    explicit ImportResult (string desc) :
        testDescription(desc),
        exporterIndex(-1), exporter(nullptr), exportSuccess(false),
        importerIndex(-1), importer(nullptr), importPP(0) { }

};


static ImportResult testExportImport (string description, aiScenePtr scene, int exporterIndex, unsigned importPP) {

    static const char EXPORT_TEST_PATH[] = "~import_coverage_dir";

    const aiExportFormatDesc *exporterDesc = aiGetExportFormatDescription(exporterIndex);
    Assimp::Exporter exporter;
    Assimp::Importer importer;
    ImportResult result(description);

    filesystem::remove_all(EXPORT_TEST_PATH);
    filesystem::create_directory(EXPORT_TEST_PATH);

    string filename = string(EXPORT_TEST_PATH) + "/model." + exporterDesc->fileExtension;

    result.exporterIndex = exporterIndex;
    result.exporter = exporterDesc;
    result.exportSuccess = (exporter.Export(scene.get(), exporterDesc->id, filename, 0) == AI_SUCCESS);
    result.exportError = nonull(exporter.GetErrorString());

    if (result.exportSuccess) {
        importer.ReadFile(filename, importPP);
        result.importError = nonull(importer.GetErrorString());
        result.importerIndex = importer.GetPropertyInteger("importerIndex", -1);
        result.importer = aiGetImportFormatDescription(result.importerIndex);
        result.importPP = importPP;
        result.imported.reset(importer.GetOrphanedScene());
        printf("exporter: %10s  importer: %s (%s)\n", exporterDesc->id, result.importer ? result.importer->mName : "none", result.imported ? "ok" : result.importError.c_str());
    } else {
        printf("exporter: %10s (failed)\n", exporterDesc->id);
    }

    filesystem::remove_all(EXPORT_TEST_PATH);
    return result;

}


int main (int argc, char * argv[]) {

    const char *action = (argc >= 2 ? argv[1] : "");

    if (!strcmp(action, "import_coverage")) {

        aiScenePtr scene = buildScene(1);
        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
            testExportImport("", scene, k, aiProcess_ValidateDataStructure);
        }

/*
        struct ImportResult {
            bool success;
            string errorMessage;
            int importerIndex;
            unsigned sceneFlags;
            int meshes;
        };

        struct Result {
            int exporterIndex;
            ImportResult loose1, loose3;
            ImportResult strict1, strict3;
        };

        list<Result> results;

        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
            const aiExportFormatDesc *desc = aiGetExportFormatDescription(k);
            char filename[100];
            snprintf(filename, sizeof(filename), "~model_coverage_test.%s", desc->fileExtension);
            fprintf(stderr, "[%-10s] testing...\n", desc->id);
            Result result;
            result.exporterIndex = k;
            {
                Assimp::Exporter exporter;
                if (exporter.Export(buildScene(1), desc->id, filename) != AI_SUCCESS) {
                    fprintf(stderr, "  export(1) error (%s): %s\n", desc->id, exporter.GetErrorString());
                } else {
                    unique_ptr<Assimp::Importer> importer;
                    const aiScene *scene;
                    importer.reset(new Assimp::Importer());
                    scene = importer->ReadFile(filename, aiProcess_ValidateDataStructure);
                    result.strict1.success = (scene != nullptr);
                    result.strict1.errorMessage = nonull(importer->GetErrorString());
                    result.strict1.importerIndex = importer->GetPropertyInteger("importerIndex", -1);
                    result.strict1.sceneFlags = (scene ? scene->mFlags : 0);
                    result.strict1.meshes = (scene ? scene->mNumMeshes : -1);
                    importer.reset(new Assimp::Importer());
                    scene = importer->ReadFile(filename, 0);
                    result.loose1.success = (scene != nullptr);
                    result.loose1.errorMessage = nonull(importer->GetErrorString());
                    result.loose1.importerIndex = importer->GetPropertyInteger("importerIndex", -1);
                    result.loose1.sceneFlags = (scene ? scene->mFlags : 0);
                    result.loose1.meshes = (scene ? scene->mNumMeshes : -1);
                }
            }
            {
                Assimp::Exporter exporter;
                if (exporter.Export(buildScene(3), desc->id, filename) != AI_SUCCESS) {
                    fprintf(stderr, "  export(3) error (%s): %s\n", desc->id, exporter.GetErrorString());
                } else {
                    unique_ptr<Assimp::Importer> importer;
                    const aiScene *scene;
                    importer.reset(new Assimp::Importer());
                    scene = importer->ReadFile(filename, aiProcess_ValidateDataStructure);
                    result.strict3.success = (scene != nullptr);
                    result.strict3.errorMessage = nonull(importer->GetErrorString());
                    result.strict3.importerIndex = importer->GetPropertyInteger("importerIndex", -1);
                    result.strict3.sceneFlags = (scene ? scene->mFlags : 0);
                    result.strict3.meshes = (scene ? scene->mNumMeshes : -1);
                    importer.reset(new Assimp::Importer());
                    scene = importer->ReadFile(filename, 0);
                    result.loose3.success = (scene != nullptr);
                    result.loose3.errorMessage = nonull(importer->GetErrorString());
                    result.loose3.importerIndex = importer->GetPropertyInteger("importerIndex", -1);
                    result.loose3.sceneFlags = (scene ? scene->mFlags : 0);
                    result.loose3.meshes = (scene ? scene->mNumMeshes : -1);
                }
            }
            results.push_back(result);
            remove(filename);
        }

        vector<vector<int> > exportersByImporter;
        exportersByImporter.resize(aiGetImportFormatCount());

        for (auto result = results.begin(); result != results.end(); ++ result) {
            Result &r = *result;
            const aiExportFormatDesc *exdesc = aiGetExportFormatDescription(r.exporterIndex);
            printf("exporter: [%s] %s (.%s)\n", exdesc->id, exdesc->description, exdesc->fileExtension);
            printf("   1 mesh, no validation: success=%s meshes=%d flags=%08x error='%s' importer=%d\n",
                   r.loose1.success ? "yes" : "no", r.loose1.meshes, r.loose1.sceneFlags, r.loose1.errorMessage.c_str(), r.loose1.importerIndex);
            printf("   1 mesh, validation   : success=%s meshes=%d flags=%08x error='%s' importer=%d\n",
                   r.strict1.success ? "yes" : "no", r.strict1.meshes, r.strict1.sceneFlags, r.strict1.errorMessage.c_str(), r.strict1.importerIndex);
            printf("   3 mesh, no validation: success=%s meshes=%d flags=%08x error='%s' importer=%d\n",
                   r.loose3.success ? "yes" : "no", r.loose3.meshes, r.loose3.sceneFlags, r.loose3.errorMessage.c_str(), r.loose3.importerIndex);
            printf("   3 mesh, validation   : success=%s meshes=%d flags=%08x error='%s' importer=%d\n",
                   r.strict3.success ? "yes" : "no", r.strict3.meshes, r.strict3.sceneFlags, r.strict3.errorMessage.c_str(), r.strict3.importerIndex);
            set<int> importerIndices;
            importerIndices.insert(r.loose1.importerIndex);
            importerIndices.insert(r.strict1.importerIndex);
            importerIndices.insert(r.loose3.importerIndex);
            importerIndices.insert(r.strict3.importerIndex);
            importerIndices.erase(-1);
            if (importerIndices.size() > 1)
                printf("   *** more than one importer used? ***\n");
            else if (importerIndices.empty())
                printf("   *** all imports failed ***\n");
            else
                exportersByImporter[*importerIndices.begin()].push_back(r.exporterIndex);
        }


        for (int k = 0; k < aiGetImportFormatCount(); ++ k) {
            const aiImporterDesc *imdesc = aiGetImportFormatDescription(k);
            const vector<int> &exporters = exportersByImporter[k];
            if (exporters.empty()) {
                printf("--- %s: no matching exporters\n", imdesc->mName);
            } else {
                for (int j = 0; j < exporters.size(); ++ j) {
                    const aiExportFormatDesc *exdesc = aiGetExportFormatDescription(exporters[j]);
                    printf("+++ %s: imports from %s (%s)\n", imdesc->mName, exdesc->id, exdesc->description);
                }
            }
        }
*/
    } else if (!*action) {

        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
            const aiExportFormatDesc *desc = aiGetExportFormatDescription(k);
            char filename[100];
            snprintf(filename, sizeof(filename), "model-%02d-%s.%s", k, desc->id, desc->fileExtension);
            printf("[%-10s] %s...\n", desc->id, filename);
            Assimp::Exporter exporter;
            if (exporter.Export(buildScene(3).get(), desc->id, filename) != AI_SUCCESS)
                fprintf(stderr, "  error: %s\n", exporter.GetErrorString());
        }

    } else {

        fprintf(stderr, "usage: %s [ action ]\n\n", argv[0]);
        fprintf(stderr, "actions:\n\n"
                        "  (default)         generate test models\n"
                        "  import_coverage   find importer / exporter associations\n"
                        "\n");
        return 1;

    }

    return 0;

}
