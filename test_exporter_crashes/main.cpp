#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/cimport.h>
#include <assimp/cexport.h>
#include <assimp/scene.h>
#include <assimp/version.h>

using namespace std;

#define MAGIC_SUCCESS_RETURN 88


// double quotes in modelfile will break this. not worth handling it.
static int runAllTests (string myself, const char *modelfile) {

    // ------- log some info

    printf("assimp version: %d.%d.%d @ %s %08x\n", aiGetVersionMajor(), aiGetVersionMinor(),
           aiGetVersionPatch(), aiGetBranchName(), aiGetVersionRevision());

    if (modelfile) {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(modelfile, 0);
        if (scene) {
            const aiImporterDesc *info = aiGetImportFormatDescription(importer.GetPropertyInteger("importerIndex", -1));
            printf("test scene info:\n");
            printf("   filename:    %s\n", modelfile);
            printf("   format:      %s\n", info ? info->mName : "(null)");
            printf("   flags:       0x%08x\n", scene->mFlags);
            printf("   animations:  %d\n", scene->mNumAnimations);
            printf("   cameras:     %d\n", scene->mNumCameras);
            printf("   lights:      %d\n", scene->mNumLights);
            printf("   materials:   %d\n", scene->mNumMaterials);
            printf("   meshes:      %d\n", scene->mNumMeshes);
            for (unsigned k = 0; k < scene->mNumMeshes; ++ k) {
                auto m = scene->mMeshes[k];
                printf("      (%d) %d vertices, %d faces, %snormals, %scolors, %suv coords\n", k,
                       m->mNumVertices, m->mNumFaces, m->HasNormals() ? "" : "no ",
                       m->HasVertexColors(0) ? "" : "no ", m->HasTextureCoords(0) ? "" : "no ");
            }
            printf("   textures:    %d\n", scene->mNumTextures);
            printf("-----------------------------------------------------\n");
        }
    } else {
        printf("no model file specified, will generate a scene.\n");
    }

    // ------- end of logging stuff

    int nformats = (int)aiGetExportFormatCount(), fails = 0;
    string modelparam = (modelfile ? (string(" \"") + modelfile + "\"") : "");

    for (int k = 0; k < nformats; ++ k) {
        auto desc = aiGetExportFormatDescription(k);
        bool success;
        string params = string("-t ") + desc->id + modelparam;
#if _WIN32
        // system() is good enough. on windows:
        //   -1 = system() return code; command couldn't be executed
        //    0 = system() return code; command interpreter not found
        //    1 = process return code; export failed gracefully
        //    2 = process return code; import failed gracefully (if modelfile)
        //   88 = process return code; export success
        // else = process terminated abnormally; probably an exception code
        success = (system((myself + " " + params).c_str()) == MAGIC_SUCCESS_RETURN);
#else
        // todo: exec + wait + WIFEXITED on linux
        printf("implement me\n");
        abort();
#endif
        fails += !success;
    }

    printf("tests: %d, passes: %d, fails: %d\n", nformats, nformats - fails, fails);
    return fails != 0;

}


// it's a bird... it's a plane... it's... a unit tetrahedron centered at the origin :(
static aiMesh * buildObject () {

    static const ai_real V[4][3] = {{1,1,1},{-1,1,-1},{-1,-1,1},{1,-1,-1}};
    static const unsigned F[4][3] = {{0,1,2},{0,2,3},{0,3,1},{3,2,1}};

    aiMesh *mesh = new aiMesh();
    mesh->mNumVertices = 4;
    mesh->mVertices = new aiVector3D[4];
    mesh->mNumFaces = 4;
    mesh->mFaces = new aiFace[4];
    for (int k = 0; k < 4; ++ k) {
        mesh->mVertices[k].Set(V[k][0], V[k][1], V[k][2]);
        mesh->mFaces[k].mNumIndices = 3;
        mesh->mFaces[k].mIndices = new unsigned [3];
        memcpy(mesh->mFaces[k].mIndices, F[k], 3 * sizeof(unsigned));
    }

    return mesh;

}


static int runTest (const char *format, const char *modelfile) {

    printf("[%-10s] ", format);

    const aiExportFormatDesc *desc = nullptr;
    for (unsigned k = 0; k < aiGetExportFormatCount() && !desc; ++ k) {
        auto d = aiGetExportFormatDescription(k);
        desc = (strcmp(d->id, format) ? nullptr : d);
    }

    if (!desc) {
        printf("unknown format\n");
        return 1;
    }

    static const char EXPORT_TEMP_PATH[] = "~test_exporter_crashes_dir";
    filesystem::remove_all(EXPORT_TEMP_PATH);
    filesystem::create_directory(EXPORT_TEMP_PATH);

    aiScene *scene;
    if (modelfile) {
        Assimp::Importer importer;
        if (!importer.ReadFile(modelfile, 0)) {
            printf("import failed: %s\n", importer.GetErrorString());
            return 2;
        }
        scene = importer.GetOrphanedScene();
    } else {
        scene = new aiScene();
        scene->mNumMeshes = 1;
        scene->mMeshes = new aiMesh *[1];
        scene->mMeshes[0] = buildObject();
        scene->mNumMaterials = 1;
        scene->mMaterials = new aiMaterial *[1];
        scene->mMaterials[0] = new aiMaterial();
        scene->mRootNode = new aiNode();
        scene->mRootNode->mNumMeshes = 1;
        scene->mRootNode->mMeshes = new unsigned[1];
        scene->mRootNode->mMeshes[0] = 0;
    }

    string filename = string(EXPORT_TEMP_PATH) + "/scene." + desc->fileExtension;
    Assimp::Exporter exporter;
    bool success = (exporter.Export(scene, format, filename, 0) == AI_SUCCESS);
    if (!success)
        printf("export failed: %s\n", exporter.GetErrorString());

    delete scene;
    filesystem::remove_all(EXPORT_TEMP_PATH);


    if (success)
        printf("pass\n");
    return success ? MAGIC_SUCCESS_RETURN : 1;

}


static int showUsage (const char *myself) {

    printf("\nUsage: %s -r [ <modelfile> ]\n", myself);
    printf("       %s -t <format_id> [ <modelfile> ]\n\n", myself);
    printf("  -r         Run test for every exporter.\n");
    printf("  -t         Run test for exporter <format_id>.\n");
    printf("  modelfile  Use as test model, otherwise generates tetrahedron.\n");
    printf("             Make sure model file name doesn't contain double quotes!\n\n");

    printf("  format_id can be:\n\n");
    for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
        auto desc = aiGetExportFormatDescription(k);
        printf("    %-10s  %s\n", desc->id, desc->description);
    }
    printf("\n");

    return 1;

}


int main (int argc, char **argv) {

    const char *myself = argv[0];
    const char *action = (argc > 1 ? argv[1] : "");
    const char *param2 = (argc > 2 ? argv[2] : nullptr);
    const char *param3 = (argc > 3 ? argv[3] : nullptr);
    int status;

    if (!strcmp(action, "-r"))
        status = runAllTests(myself, param2);
    else if (!strcmp(action, "-t") && param2)
        status = runTest(param2, param3);
    else
        status = showUsage(myself);

    return status;

}
