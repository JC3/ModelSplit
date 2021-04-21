#include <iostream>
#include <cstdio> // #OccupyOStream
#include <assimp/matrix4x4.h>
#include <assimp/matrix4x4.inl>
#include <assimp/vector3.h>
#include <assimp/vector3.inl>
#include <assimp/quaternion.h>
#include <assimp/quaternion.inl>

using namespace std;

static istream & operator >> (istream &in, aiMatrix4x4 &m) {
    m = aiMatrix4x4();
    for (int r = 0; r < 3; ++ r)
        for (int c = 0; c < 4; ++ c)
            in >> m[r][c];
    return in;
}

// please file your complaints directly into the trash can.
static void print (const aiMatrix4x4 &m, const char *description = nullptr) {
    if (description)
        printf("%s =\n", description);
    for (int r = 0; r < 4; ++ r)
        printf("  [ %10.4f %10.4f %10.4f %10.4f ]\n", m[r][0], m[r][1], m[r][2], m[r][3]);
}

static void print (const aiVector3D &v, const char *description = nullptr) {
    if (description)
        printf("%s =\n", description);
    printf("  [ %10.4f %10.4f %10.4f ] (vector)\n", v.x, v.y, v.z);
}

static void print (const aiQuaternion &q, const char *description = nullptr) {
    if (description)
        printf("%s =\n", description);
    printf("  [ %10.4f %10.4f %10.4f %10.4f ] (quaternion)\n", q.x, q.y, q.z, q.w);
}

static void printhr () {
    printf("----------------------------------------------------------------------\n");
}


// this isn't built into assimp? well, i couldn't find it, anyways...
static aiMatrix4x4 recompose (const aiVector3D &scale, const aiVector3D &rotateEuler, const aiVector3D &translate) {

    // the api here is weird and inconsistent
    aiMatrix4x4 m_scale, m_rotate, m_translate;
    aiMatrix4x4::Scaling(scale, m_scale);
    m_rotate = aiMatrix4x4().FromEulerAnglesXYZ(rotateEuler);
    aiMatrix4x4::Translation(translate, m_translate);

    return m_translate * m_rotate * m_scale;

}


static void checkMatrix (const aiMatrix4x4 &aTb) {

    aiMatrix4x4 aTb_recomposed;
    aiVector3D aTb_scale, aTb_translate, aTb_rotateEuler;
    aiQuaternion aTb_rotate;

    printhr();
    print(aTb, "input");

    aTb.Decompose(aTb_scale, aTb_rotate, aTb_translate);
    aTb_recomposed = aiMatrix4x4(aTb_scale, aTb_rotate, aTb_translate);

    printhr();
    print(aTb_translate, "input translation (w/ quaternion rotation)");
    print(aTb_rotate, "input rotation (w/ quaternion rotation)");
    print(aTb_scale, "input scale (w/ quaternion rotation)");
    print(aTb_recomposed, "recomposed (w/ quaternion rotation");
    printf("recomposed == input? %s\n", aTb_recomposed.Equal(aTb) ? "YES" : "NO");

    aTb.Decompose(aTb_scale, aTb_rotateEuler, aTb_translate);
    aTb_recomposed = recompose(aTb_scale, aTb_rotateEuler, aTb_translate);

    printhr();
    print(aTb_translate, "input translation (w/ euler angle rotation)");
    print(aTb_rotateEuler * (ai_real)(180.0 / AI_MATH_PI), "input rotation in degrees (w/ euler angle rotation)");
    print(aTb_scale, "input scale (w/ euler angle rotation)");
    print(aTb_recomposed, "recomposed (w/ euler angle rotation");
    printf("recomposed == input? %s\n", aTb_recomposed.Equal(aTb) ? "YES" : "NO");
    printhr();

}



int main (int argc, char **argv) {

    aiMatrix4x4 aTb;

    bool readMatrix = (argc > 1 && !strcmp(argv[1], "-r"));
    if (readMatrix) {
        cin >> aTb;
    } else {
        aTb[0][0] = aTb[1][3] = -1; // scale = [-1, 1, 1], translate = [0, -1, 0].
        aTb *= aiMatrix4x4().FromEulerAnglesXYZ(45.0, 23.0, -15.0); // shake it up a little!
    }

    checkMatrix(aTb);

}
