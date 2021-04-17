#include <cstdio>
#include <assimp/mesh.h>
#include <assimp/scene.h>
#include <assimp/Exporter.hpp>


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


static aiScene * buildScene (int nobjects = 3) {

    aiScene *scene = new aiScene();

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


int main (int argc, char * argv[]) {

    const char *action = (argc >= 2 ? argv[1] : "");

    if (!strcmp(action, "import_coverage")) {

        printf("todo\n");

    } else if (!*action) {

        for (unsigned k = 0; k < aiGetExportFormatCount(); ++ k) {
            const aiExportFormatDesc *desc = aiGetExportFormatDescription(k);
            char filename[100];
            snprintf(filename, sizeof(filename), "model-%02d-%s.%s", k, desc->id, desc->fileExtension);
            printf("[%-10s] %s...\n", desc->id, filename);
            Assimp::Exporter exporter;
            if (exporter.Export(buildScene(3), desc->id, filename) != AI_SUCCESS)
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
