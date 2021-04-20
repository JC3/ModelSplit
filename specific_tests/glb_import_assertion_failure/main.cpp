#include <cstdio>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>

using namespace std;

int main (int argc, char **argv) {

    const char *modelfile = (argc > 1 ? argv[1] : nullptr);
    if (!modelfile) {
        fprintf(stderr, "no model file specified\n");
        return 1;
    }

    printf("assimp %d.%d.%d (%s @ %08x)\n", aiGetVersionMajor(), aiGetVersionMinor(),
           aiGetVersionPatch(), aiGetBranchName(), aiGetVersionRevision());

    printf("importing %s...\n", modelfile);
    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(modelfile, 0);
    if (!scene) {
        fprintf(stderr, "error: %s\n", importer.GetErrorString());
        return 1;
    }

    printf("exporting scene.glb...\n");
    Assimp::Exporter exporter;
    if (exporter.Export(scene, "glb", "scene.glb") != AI_SUCCESS) {
        fprintf(stderr, "error: %s\n", exporter.GetErrorString());
        return 1;
    }

    printf("importing scene.glb...\n");
    Assimp::Importer reimporter;
    scene = reimporter.ReadFile("scene.glb", 0);
    if (!scene) {
        fprintf(stderr, "error: %s\n", reimporter.GetErrorString());
        return 1;
    }

    printf("finished.\n");
    return 0;

}
