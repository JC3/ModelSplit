#include <cstdio>
#include <assimp/scene.h>
#include <assimp/mesh.h>
#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>


static void setFace (aiFace &face, unsigned a, unsigned b, unsigned c) {

    delete[] face.mIndices;
    face.mNumIndices = 3;
    face.mIndices = new unsigned[3];
    face.mIndices[0] = a;
    face.mIndices[1] = b;
    face.mIndices[2] = c;

}


/** Makes a tetrahedron in bounding box (-1,-1,-1) -> ( 1, 1, 1). */
static aiMesh * buildObject () {

    aiMesh *mesh = new aiMesh();

    char name[100];
    snprintf(name, sizeof(name), "mesh_%p", (void *)mesh);
    mesh->mName.Set(name); // assimp #3814 workaround

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

    mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE; // assimp #3778 workaround

    return mesh;

}


static aiScene * buildScene () {

    aiScene *scene = new aiScene();

    scene->mNumMeshes = 1;
    scene->mMeshes = new aiMesh *[scene->mNumMeshes];
    scene->mMeshes[0] = buildObject();
    scene->mNumMaterials = 1;
    scene->mMaterials = new aiMaterial *[1];
    scene->mMaterials[0] = new aiMaterial();
    scene->mMetaData = new aiMetadata(); // assimp #3781 workaround
    scene->mRootNode = new aiNode();
    scene->mRootNode->mNumMeshes = scene->mNumMeshes;
    scene->mRootNode->mMeshes = new unsigned[scene->mNumMeshes];

    for (unsigned k = 0; k < scene->mNumMeshes; ++ k)
        scene->mRootNode->mMeshes[k] = k;

    return scene;

}


static void moveSceneNodesDown (aiScene *scene) {

    aiNode *oldRoot = scene->mRootNode;
    scene->mRootNode = new aiNode();
    scene->mRootNode->addChildren(1, &oldRoot);

}


// so we can verify we're operating on the expected tree structure
static void printSceneStructure (const aiScene *scene) {

    printf("scene");
    for (const aiNode *node = scene->mRootNode; node; node = (node->mChildren ? node->mChildren[0] : nullptr))
        printf(" > node");
    printf("\n");

}


static bool doExportCheck (const aiScene *scene, const char *filename, const char *format) {

    printf("----------------------------------------------------------------\n");
    printSceneStructure(scene);
    printf("writing %s...\n", filename);

    Assimp::Exporter exporter;
    if (exporter.Export(scene, format, filename) != AI_SUCCESS) {
        printf("export failed: %s\n", exporter.GetErrorString());
        return false;
    }

    Assimp::Importer importer;
    if (!importer.ReadFile(filename, aiProcess_ValidateDataStructure)) {
        printf("import failed: %s\n", importer.GetErrorString());
        return false;
    }

    // nothing fancy just see if it's still got meshes
    const aiScene *impscene = importer.GetScene();
    if (impscene->mNumMeshes != scene->mNumMeshes) {
        printf("check failed: wrote %d meshes, but read %d meshes.\n", scene->mNumMeshes, impscene->mNumMeshes);
        return false;
    }

    printf("seems ok.\n");
    return true;

}


int main () {

    aiScene *scene = buildScene();
    int failures = 0;

    failures += !doExportCheck(scene, "test_scene_output_depth_1.3mf", "3mf");
    moveSceneNodesDown(scene);
    failures += !doExportCheck(scene, "test_scene_output_depth_2.3mf", "3mf");
    moveSceneNodesDown(scene);
    failures += !doExportCheck(scene, "test_scene_output_depth_3.3mf", "3mf");

    if (!failures) {
        printf("----------------------------------------------------------------\n");
        printf("you're on the road to success!\n");
    }

    return failures;

}
