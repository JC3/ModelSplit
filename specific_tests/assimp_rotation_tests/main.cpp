#include "util.hh"

#pragma warning (suppress: 4100)
int main (int argc, char *argv[]) {

    printver();

    /* assimp components to focus on:
     *   matrix3x3
     *   matrix4x4
     *   quaternion
     *   vector3
     *
     * first step is let's regen docs
     *   done: https://github.com/JC3/AssimpDocs
     *
     * remember: https://github.com/assimp/assimp/issues/3790
     *   aiQuaternion constructor takes (y, z, x)
     *
     * next i'd like to confirm that everything using euler angles is
     * using the same rotation sequence. specifically:
     *
     *   matrix3x3
     *   - nothing of interest
     *   matrix4x4
     *   - FromEulerAnglesXYZ (x, y, z)
     *   - FromEulerAnglexXYZ (vector3 xyz)
     *   quaternion
     *   - constructor from euler angles (known issue: param names incorrect; it's y,z,x not x,y,z)
     *   - GetMatrix
     *   vector3
     *   - nothing of interest
     *
     * for later: code review axis-angle math
     * for later: code review quaternion math
     */

    if (fabs(AI_DEG_TO_RAD(123.4) - (123.4 * 3.141592653 / 180.0)) > 1e-6) {
        fprintf(stderr, "AI_DEG_TO_RAD is broken.\n");
        abort();
    }

    const ai_real XROT = AI_DEG_TO_RAD(30);
    const ai_real YROT = AI_DEG_TO_RAD(-45);
    const ai_real ZROT = AI_DEG_TO_RAD(60);

    const aiQuaternion EXPECTEDQ(0.7233174f, 0.3919038f, -0.2005621f, 0.5319757f);
    const aiMatrix3x3 EXPECTEDM3(0.3535534f, -0.9267767f,  0.1268265f,
                                 0.6123725f,  0.1268265f, -0.7803301f,
                                 0.7071068f,  0.3535534f,  0.6123725f);
    const aiMatrix4x4 EXPECTEDM4(EXPECTEDM3);

    /* expected quaternions for different sequences:
     *   xyz [ 0.0222600, -0.4396797, 0.3604234, 0.8223632 ]
     *   xzy [ 0.3919038, -0.4396797, 0.3604234, 0.7233174 ]
     *   yxz [ 0.0222600, -0.4396797, 0.5319757, 0.7233174 ]
     *   yzx [ 0.0222600, -0.2005621, 0.5319757, 0.8223632 ]
     *   zxy [ 0.3919038, -0.2005621, 0.3604234, 0.8223632 ]
     *   zyx [ 0.3919038, -0.2005621, 0.5319757, 0.7233174 ]
     * so let's see what we get...
     */

    aiQuaternion quat(YROT, ZROT, XROT);
    print(quat, "quat", &EXPECTEDQ);

    /*  [     0.3919    -0.2006     0.5320     0.7233 ] (quaternion)
     * that's ZYX. now let's make sure transforms are equal.
     * expected rotation matrix for zyx is:
     *
     * [  0.3535534, -0.9267767,  0.1268265;
     *    0.6123725,  0.1268265, -0.7803301;
     *    0.7071068,  0.3535534,  0.6123725 ]
     */

    aiMatrix4x4 tFromQuat(aiVector3D(1,1,1), quat, aiVector3D(0,0,0));
    aiMatrix4x4 tFromQuatM(quat.GetMatrix());
    aiMatrix4x4 tFromEuler = aiMatrix4x4().FromEulerAnglesXYZ(XROT, YROT, ZROT);
    aiMatrix4x4 tFromEulerV = aiMatrix4x4().FromEulerAnglesXYZ(aiVector3D(XROT, YROT, ZROT));

    print(tFromQuat, "quaternion constructor", &EXPECTEDM4);
    print(tFromQuatM, "quaternion GetMatrix", &EXPECTEDM4);
    print(tFromEuler, "euler angle constructor", &EXPECTEDM4);
    print(tFromEulerV, "euler angle (vector) constructor", &EXPECTEDM4);

    aiMatrix4x4 tFromEulerX = aiMatrix4x4().FromEulerAnglesXYZ(XROT, 0, 0);
    aiMatrix4x4 tFromEulerY = aiMatrix4x4().FromEulerAnglesXYZ(0, YROT, 0);
    aiMatrix4x4 tFromEulerZ = aiMatrix4x4().FromEulerAnglesXYZ(0, 0, ZROT);
    aiMatrix4x4 tFromMul = tFromEulerZ * tFromEulerY * tFromEulerX;

    print(tFromMul, "z * y * x", &EXPECTEDM4);

    /* looks good; i'm satisfied; no need to test further. */

    return 0;

}
