#include "printcolorstuff.h"

#include <cstdio>
#include <algorithm>
#include <set>
#include <stack>
#include <string>
#include <utility>

using namespace std;


static string binstr (const char *data, unsigned size) {

    const constexpr unsigned CUTOFF = 8;

    string str = "[";
    for (const char *b = data; b < data + std::min(CUTOFF, size); ++ b) {
        char buf[10];
        snprintf(buf, sizeof(buf), " %02X", (unsigned char)(*b));
        str += buf;
    }
    if (size > CUTOFF)
        str += " ...";
    str += " ]";

    return str;

}


template <typename Index, typename Element, typename Length>
static Element * safeget (Index index, Element **arr, Length narr) {
    return (index >= 0 && index < (Index)narr) ? arr[index] : nullptr;
}


void printColorStuff (const aiScene *scene) {

    printf("  [scene] '%s' meshes=%d materials=%d textures=%d\n", scene->mName.C_Str(),
           scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);

    for (unsigned k = 0; k < scene->mNumMaterials; ++ k) {
        auto m = scene->mMaterials[k];
        printf("    [material %d] '%s'\n", k, m->GetName().C_Str());
        unsigned anytex = 0;
        for (int t = 0; t < (int)aiTextureType_UNKNOWN; ++ t) {
            unsigned n = m->GetTextureCount((aiTextureType)t);
            if (n)
                printf("      [textures] type=%d count=%d\n", t, n);
            anytex += n;
        }
#if !SLIGHTLY_LESS_VERBOSE
        if (!anytex)
            printf("      [textures] none present\n");
#endif
        printf("      [properties] count=%d capacity=%d\n", m->mNumProperties, m->mNumAllocated);
        for (unsigned p = 0; p < m->mNumProperties; ++ p) {
            auto prop = m->mProperties[p];
#if SLIGHTLY_LESS_VERBOSE
            printf("        [property] '%s' type=%d len=%d\n", prop->mKey.C_Str(),
                   prop->mType, prop->mDataLength);
#else
            printf("        [property] '%s' type=%d len=%d data=%s\n", prop->mKey.C_Str(),
                   prop->mType, prop->mDataLength, binstr(prop->mData, prop->mDataLength).c_str());
#endif
        }
    }

    for (unsigned k = 0; k < scene->mNumMeshes; ++ k) {
        auto m = scene->mMeshes[k];
        printf("    [mesh %d] '%s' verts=%d faces=%d\n", k, m->mName.C_Str(),
               m->mNumVertices, m->mNumFaces);
        auto meshmat = safeget(m->mMaterialIndex, scene->mMaterials, scene->mNumMaterials);
        printf("      [material] #%d (%s)\n", m->mMaterialIndex,
               meshmat ? meshmat->GetName().C_Str() : "<invalid material index>");
        bool anycolors = false, anyuv = false;
        for (unsigned i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++ i) {
            if (m->HasVertexColors(i))
                printf("      [vcolors %d] present\n", i);
            anycolors |= m->HasVertexColors(i);
        }
#if !SLIGHTLY_LESS_VERBOSE
        if (!anycolors)
            printf("      [vcolors] none present\n");
#endif
        for (unsigned i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++ i) {
            if (m->HasTextureCoords(i))
                printf("      [texcoords %d] %dD present\n", i, m->mNumUVComponents[i]);
            anyuv |= m->HasTextureCoords(i);
        }
#if !SLIGHTLY_LESS_VERBOSE
        if (!anyuv)
            printf("      [texcoords] none present\n");
#endif
    }

    stack<pair<int,const aiNode *> > nodes;
    set<const aiNode *> hit; // i just want to make sure there's no cycles
    if (scene->mRootNode)
        nodes.push(make_pair(0, scene->mRootNode));
    else
        printf("    [nodes] none present -- null mRootnode.\n");
    while (!nodes.empty()) {
        auto thingy = nodes.top();
        nodes.pop();
        int depth = thingy.first;
        const aiNode *node = thingy.second;
        if (!node)
            continue;
        string indentstr;
        for (int ind = 0; ind < 4 + 2 * depth; ++ ind)
            indentstr += " ";
        const char *indent = indentstr.c_str();
        if (hit.find(node) != hit.end()) {
            printf("%s[nodes] !!!! scene graph cycle detected\n", indent);
            continue;
        }
        printf("%s[node] '%s' meshes=%d\n", indent, node->mName.C_Str(), node->mNumMeshes);
        for (unsigned k = 0; k < node->mNumMeshes; ++ k) {
            auto mindex = node->mMeshes[k];
            auto mesh = safeget(mindex, scene->mMeshes, scene->mNumMeshes);
            printf("%s  [mesh] #%d (%s)\n", indent, mindex, mesh ? mesh->mName.C_Str() : "<invalid mesh index>");
        }
        hit.insert(node);
        for (unsigned k = 0; k < node->mNumChildren; ++ k)
            nodes.push(make_pair(depth + 1, node->mChildren[k]));
    }

}
// ... and that ... was way more typing than i wanted to do for debug output.

