#ifndef UTIL_HH
#define UTIL_HH

#include <assimp/matrix3x3.inl>
#include <assimp/matrix4x4.inl>
#include <assimp/quaternion.inl>
#include <assimp/vector3.inl>

void printver ();
void print (const aiMatrix4x4 &m, const char *description = nullptr, const aiMatrix4x4 *expected = nullptr);
void print (const aiVector3D &v, const char *description = nullptr, const aiVector3D *expected = nullptr);
void print (const aiQuaternion &q, const char *description = nullptr, const aiQuaternion *expected = nullptr);
void printhr ();

#endif // UTIL_HH
