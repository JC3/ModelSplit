#include "util.hh"
#include <cstdio>
#include <assimp/version.h>

using namespace std;


void printver () {
    printf("assimp version: %d.%d.%d (%s @ %08x)\n", aiGetVersionMajor(),
           aiGetVersionMinor(), aiGetVersionPatch(), aiGetBranchName(),
           aiGetVersionRevision());
    printhr();
}


void print (const aiMatrix4x4 &m, const char *description, const aiMatrix4x4 *expected) {
    if (description)
        printf("%s =\n", description);
    for (int r = 0; r < 4; ++ r)
        printf("  [ %10.4f %10.4f %10.4f %10.4f ]\n", m[r][0], m[r][1], m[r][2], m[r][3]);
    if (expected)
        printf("  *** expected value? %s\n", m.Equal(*expected) ? "YES" : "NO");
}


void print (const aiVector3D &v, const char *description, const aiVector3D *expected) {
    if (description)
        printf("%s =\n", description);
    printf("  [ %10.4f %10.4f %10.4f ] (vector)\n", v.x, v.y, v.z);
    if (expected)
        printf("  *** expected value? %s\n", v.Equal(*expected) ? "YES" : "NO");
}


void print (const aiQuaternion &q, const char *description, const aiQuaternion *expected) {
    if (description)
        printf("%s =\n", description);
    printf("  [ %10.4f %10.4f %10.4f %10.4f ] (quaternion)\n", q.x, q.y, q.z, q.w);
    if (expected)
        printf("  *** expected value? %s\n", q.Equal(*expected) ? "YES" : "NO");
}


void printhr () {
    printf("----------------------------------------------------------------------\n");
}
