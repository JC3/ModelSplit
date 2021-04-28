#include "printcolorstuff.h"

#if _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  ifdef min
#    undef min
#  endif
#endif

#include <cstdio>
#include <algorithm>
#include <set>
#include <stack>
#include <string>
#include <utility>

using namespace std;

static bool s_haveansi = false;
#if _WIN32
static DWORD s_initOutMode = 0;
#endif
static const char FORMAT_RESET[] = "0";
static const char FORMAT_L0[] = "38;5;228";
static const char FORMAT_L1[] = "38;5;155";
static const char FORMAT_L2[] = "38;5;152";
static const char FORMAT_ASSIMP[] = "38;5;38";


void AnsiLogStream::write (const char *message) {

    ansip(FORMAT_ASSIMP);
    printf("%s", message);

}


void ansip (const char *seq, bool reset) {

    if (reset && seq != FORMAT_RESET)
        ansip(FORMAT_RESET, false);
    if (s_haveansi && seq)
        printf("\x1b[%sm", seq);

}


void initTerminal () {

#if !NEVER_USE_ANSI
#  if _WIN32
    HANDLE hcon = GetStdHandle(STD_OUTPUT_HANDLE);
    s_haveansi = GetConsoleMode(hcon, &s_initOutMode) &&
            SetConsoleMode(hcon, s_initOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    ansip(FORMAT_RESET);
#  else
    // todo: query terminal caps on linux
#  endif
#endif

}


void doneTerminal () {

#if _WIN32
    ansip(FORMAT_RESET);
    if (s_haveansi) {
        HANDLE hcon = GetStdHandle(STD_OUTPUT_HANDLE);
        SetConsoleMode(hcon, s_initOutMode);
        s_haveansi = false;
    }
#endif

}


static string binstr (const char *data, unsigned size) {

    string str = "{";
    for (const char *b = data; b < data + std::min(PRINTED_DATA_CUTOFF, size); ++ b) {
        char buf[10];
        snprintf(buf, sizeof(buf), " %02X", (unsigned char)(*b));
        str += buf;
    }
    if (size > PRINTED_DATA_CUTOFF)
        str += " ...";
    str += " }";

    return str;

}


template <typename Index, typename Element, typename Length>
static Element * safeget (Index index, Element **arr, Length narr) {
    return (index >= 0 && index < (Index)narr) ? arr[index] : nullptr;
}


void printColorStuff (const aiScene *scene, bool verbose) {

    ansip(FORMAT_L0);
#ifdef ASSIMP_501_COMPAT
    printf("  <SCENE> meshes=%d materials=%d textures=%d\n",
           scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);
#else
    printf("  <SCENE> '%s' meshes=%d materials=%d textures=%d\n", scene->mName.C_Str(),
           scene->mNumMeshes, scene->mNumMaterials, scene->mNumTextures);
#endif

    for (unsigned k = 0; k < scene->mNumMaterials; ++ k) {
        auto m = scene->mMaterials[k];
        ansip(FORMAT_L1);
        printf("    <MATERIAL %d> '%s'\n", k, m->GetName().C_Str());
        ansip(FORMAT_L2);
        unsigned anytex = 0;
        for (int t = 0; t < (int)aiTextureType_UNKNOWN; ++ t) {
            unsigned n = m->GetTextureCount((aiTextureType)t);
            if (n)
                printf("      <TEXTURES> type=%d count=%d\n", t, n);
            anytex += n;
        }
        if (!anytex && verbose)
            printf("      <TEXTURES> none present\n");
        printf("      <PROPERTIES> count=%d capacity=%d\n", m->mNumProperties, m->mNumAllocated);
        for (unsigned p = 0; p < m->mNumProperties; ++ p) {
            auto prop = m->mProperties[p];
            if (!verbose || !PRINTED_DATA_CUTOFF) {
                printf("        <PROPERTY> '%s' type=%d len=%d\n", prop->mKey.C_Str(),
                       prop->mType, prop->mDataLength);
            } else {
                printf("        <PROPERTY> '%s' type=%d len=%d data=%s\n", prop->mKey.C_Str(),
                       prop->mType, prop->mDataLength, binstr(prop->mData, prop->mDataLength).c_str());
            }
        }
    }

    for (unsigned k = 0; k < scene->mNumMeshes; ++ k) {
        auto m = scene->mMeshes[k];
        ansip(FORMAT_L1);
        printf("    <MESH %d> '%s' verts=%d faces=%d\n", k, m->mName.C_Str(),
               m->mNumVertices, m->mNumFaces);
        auto meshmat = safeget(m->mMaterialIndex, scene->mMaterials, scene->mNumMaterials);
        ansip(FORMAT_L2);
        printf("      <MATERIAL> #%d (%s)\n", m->mMaterialIndex,
               meshmat ? meshmat->GetName().C_Str() : "<invalid material index>");
        bool anycolors = false, anyuv = false;
        for (unsigned i = 0; i < AI_MAX_NUMBER_OF_COLOR_SETS; ++ i) {
            if (m->HasVertexColors(i))
                printf("      <VCOLORS %d> present\n", i);
            anycolors |= m->HasVertexColors(i);
        }
        if (!anycolors && verbose)
            printf("      <VCOLORS> none present\n");
        for (unsigned i = 0; i < AI_MAX_NUMBER_OF_TEXTURECOORDS; ++ i) {
            if (m->HasTextureCoords(i))
                printf("      <TEXCOORDS %d> %dD present\n", i, m->mNumUVComponents[i]);
            anyuv |= m->HasTextureCoords(i);
        }
        if (!anyuv && verbose)
            printf("      <TEXCOORDS> none present\n");
    }

    // tests cycle detection
    //aiNode *test = new aiNode();
    //scene->mRootNode->addChildren(1, &test);
    //test->addChildren(1, const_cast<aiNode**>(&(scene->mRootNode)));

    stack<pair<int,const aiNode *> > nodes;
    set<const aiNode *> hit; // i just want to make sure there's no cycles
    ansip(FORMAT_L1);
    if (scene->mRootNode)
        nodes.push(make_pair(0, scene->mRootNode));
    else
        printf("    <NODES> none present -- null mRootnode.\n");
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
            printf("%s<NODES> !!!! scene graph cycle detected\n", indent);
            continue;
        }
        printf("%s<NODE> '%s' meshes=%d\n", indent, node->mName.C_Str(), node->mNumMeshes);
        ansip(FORMAT_L2);
        for (unsigned k = 0; k < node->mNumMeshes; ++ k) {
            auto mindex = node->mMeshes[k];
            auto mesh = safeget(mindex, scene->mMeshes, scene->mNumMeshes);
            printf("%s  <MESH> #%d (%s)\n", indent, mindex, mesh ? mesh->mName.C_Str() : "<invalid mesh index>");
        }
        ansip(FORMAT_L1);
        hit.insert(node);
        for (unsigned k = 0; k < node->mNumChildren; ++ k)
            nodes.push(make_pair(depth + 1, node->mChildren[k]));
    }

    ansip(FORMAT_RESET);

}

// ... and that ... was way more typing than i wanted to do for debug output.

