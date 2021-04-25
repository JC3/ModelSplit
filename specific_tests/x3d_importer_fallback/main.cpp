#include <cstdio>
#include <set>
#include <string>
#include <assimp/BaseImporter.h>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/DefaultIOSystem.h>

using namespace std;

// doing a ReadFile with validation doesn't add new info and makes the output
// more verbose than needed; so i've optionified it and disabled it.
#define TRY_READ_VALIDATED 0 // non-zero to enable

// also try forcing use of the BaseImporter that assimp says should handle
// the extension, in case of mismatch.
#define TRY_FORCING_LOADER 1 // zero to disable


static void dumpModel (const aiScene *scene, const char *name, const char *format) {

    if (!scene || !format)
        return;

    Assimp::Exporter exporter;

    const aiExportFormatDesc *desc = nullptr;
    for (unsigned k = 0; k < exporter.GetExportFormatCount() && !desc; ++ k) {
        desc = exporter.GetExportFormatDescription(k);
        desc = (strcmp(format, desc->id) ? nullptr : desc);
    }

    string filename = "debug_";
    for (const char *ch = name; *ch; ++ ch)
        filename += isalnum(*ch) ? *ch : '_';
    if (desc)
        filename += string(".") + desc->fileExtension;

    if (exporter.Export(scene, format, filename) != AI_SUCCESS) {
        fprintf(stderr, "[export failed: %s]\n", exporter.GetErrorString());
    } else {
        printf("[wrote: %s]\n", filename.c_str());
    }

}


static void tryItWithAFile (const char *extn, const char *file, const char *dumpformat) {

    string filename;

    if (file) {
        filename = file;
        printf("[using specified file: %s]\n", filename.c_str());
    } else {
        filename = string("dummy.") + extn;
        FILE *dummy = fopen(filename.c_str(), "w");
        if (!dummy) {
            perror(extn);
            return;
        }
        fclose(dummy);
        printf("[created zero-length dummy file: %s]\n", filename.c_str());
    }

    {
        printf("  ReadFile (pFlags=0):\n");
        Assimp::Importer importer;
        const aiScene *scene;
        if (!(scene = importer.ReadFile(filename, 0)))
            printf("    ReadFile:                 error: %s\n", importer.GetErrorString());
        else
            printf("    ReadFile:                 success\n");
        int index = importer.GetPropertyInteger("importerIndex", -1);
        printf("    importerIndex:            %d\n", index);
        const aiImporterDesc *desc = importer.GetImporterInfo(index);
        printf("    mName:                    %s\n", desc ? desc->mName : "(null info)");
        if (scene)
            printf("    scene->mFlags:            0x%08x\n", scene->mFlags);
    }

#if TRY_READ_VALIDATED
    {
        printf("  ReadFile (pFlags=aiProcess_ValidateDataStructure):\n");
        Assimp::Importer importer;
        const aiScene *scene;
        if (!(scene = importer.ReadFile(filename, aiProcess_ValidateDataStructure)))
            printf("    ReadFile:                 error: %s\n", importer.GetErrorString());
        else
            printf("    ReadFile:                 success\n");
        int index = importer.GetPropertyInteger("importerIndex", -1);
        printf("    importerIndex:            %d\n", index);
        const aiImporterDesc *desc = importer.GetImporterInfo(index);
        printf("    mName:                    %s\n", desc ? desc->mName : "(null info)");
        if (scene)
            printf("    scene->mFlags:            0x%08x\n", scene->mFlags);
    }
#endif

#if TRY_FORCING_LOADER
    {
        printf("  ReadFile via forced use of BaseImporter:\n");
        Assimp::Importer importer;
        Assimp::BaseImporter *base = importer.GetImporter(extn);
        if (base) {
            Assimp::DefaultIOSystem io;
            bool readable = base->CanRead(filename, &io, false);
            bool readableSig = base->CanRead(filename, &io, true);
            printf("    mName:                    %s\n", base->GetInfo()->mName);
            printf("    CanRead(checkSig=false):  %s\n", readable ? "yes" : "no");
            printf("    CanRead(checkSig=true):   %s\n", readableSig ? "yes" : "no");
            const aiScene *scene = base->ReadFile(&importer, filename, &io);
            if (!scene)
                printf("    ReadFile:                 error: %s\n", base->GetErrorText().c_str());
            else
                printf("    ReadFile:                 success\n");
            if (scene)
                printf("    scene->mFlags:            0x%08x\n", scene->mFlags);
            dumpModel(scene, filename.c_str(), dumpformat);
        } else {
            printf("    GetImporter:              null\n");
        }
    }
#endif

    if (!file) {
        printf("[deleting dummy file]\n");
        remove(filename.c_str());
    }

}


static void showExtensionInfo (const char *extn, const char *file, const char *dumpformat) {

    Assimp::Importer importer;

    printf("Extension: \"%s\"\n", extn);
    printf("  IsExtensionSupported:       %s\n", importer.IsExtensionSupported(extn) ? "true" : "false");
    printf("  GetImporterIndex:           %d\n", (int)importer.GetImporterIndex(extn));

    Assimp::BaseImporter *plugin = importer.GetImporter(extn);
    printf("  GetImporter:                plugin=0x%p\n", (void *)plugin);

    if (plugin) {
        set<string> extnset;
        plugin->GetExtensionList(extnset);
        string extns;
        for (auto e = extnset.begin(); e != extnset.end(); ++ e)
            extns += (*e + " ");
        const aiImporterDesc *desc = plugin->GetInfo();
        printf("  ->GetExtensionList:         %s\n", extns.c_str());
        printf("  ->GetInfo->mName:           %s\n", desc ? desc->mName : "(null info)");
        printf("  ->GetInfo->mFileExtensions: %s\n", desc ? desc->mFileExtensions : "(null info)");
    }

    tryItWithAFile(extn, file, dumpformat);

    printf("---------------------------------------------------------------\n");

}


int main (int argc, char **argv) {

    if (argc == 1) {
        fprintf(stderr, "Usage: %s [ -x[:format] ] <extension[:file]> [ <extensions[:file]> ... ]\n\n", argv[0]);
        fprintf(stderr, "   extension        One or more file extensions to test (no leading period).\n");
        fprintf(stderr, "   extension:file   Use actual file <file> instead of a dummy.\n");
        fprintf(stderr, "   -x[:format]      Export forced-import scenes for debugging (default format assxml).\n\n");
        return 1;
    }

    printf("assimp %d.%d.%d (%s @ %08x)\n", aiGetVersionMajor(), aiGetVersionMinor(),
           aiGetVersionPatch(), aiGetBranchName(), aiGetVersionRevision());

    string dumpformat;
    for (int a = 1; a < argc; ++ a) {
        char *work = strdup(argv[a]);
        char *extn = work;
        char *file = strchr(work, ':');
        if (file) *(file ++) = 0;
        if (!strcmp(extn, "-x")) // haaaaaaaaaaaaaaaaack
            dumpformat = (file ? file : "assxml");
        else
            showExtensionInfo(extn, file, dumpformat == "" ? nullptr : dumpformat.c_str());
        free(work);
    }

    return 0;

}
