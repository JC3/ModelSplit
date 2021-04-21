#include <cstdio>
#include <set>
#include <string>
#include <assimp/BaseImporter.h>
#include <assimp/Importer.hpp>
#include <assimp/version.h>
#include <assimp/postprocess.h>

using namespace std;

// doing a ReadFile with validation doesn't add new info and makes the output
// more verbose than needed; so i've optionified it and disabled it.
#define TRY_READ_VALIDATED 0 // non-zero to enable


static void tryItWithAFile (const char *extn) {

    string filename = "dummy.";
    filename += extn;

    FILE *dummy = fopen(filename.c_str(), "w");
    if (!dummy) {
        perror(extn);
        return;
    }
    fclose(dummy);

    printf("[created zero-length dummy file: %s]\n", filename.c_str());

    {
        printf("  ReadFile (pFlags=0):\n");
        Assimp::Importer importer;
        if (!importer.ReadFile(filename, 0))
            printf("    ReadFile:                 error: %s\n", importer.GetErrorString());
        else
            printf("    ReadFile:                 success\n");
        int index = importer.GetPropertyInteger("importerIndex", -1);
        printf("    importerIndex:            %d\n", index);
        const aiImporterDesc *desc = importer.GetImporterInfo(index);
        printf("    mName:                    %s\n", desc ? desc->mName : "(null info)");
    }

#if TRY_READ_VALIDATED
    {
        printf("  ReadFile (pFlags=aiProcess_ValidateDataStructure):\n");
        Assimp::Importer importer;
        if (!importer.ReadFile(filename, aiProcess_ValidateDataStructure))
            printf("    ReadFile:                 error: %s\n", importer.GetErrorString());
        else
            printf("    ReadFile:                 success\n");
        int index = importer.GetPropertyInteger("importerIndex", -1);
        printf("    importerIndex:            %d\n", index);
        const aiImporterDesc *desc = importer.GetImporterInfo(index);
        printf("    mName:                    %s\n", desc ? desc->mName : "(null info)");
    }
#endif

    printf("[deleting dummy file]\n");
    remove(filename.c_str());

}


static void showExtensionInfo (const char *extn) {

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

    tryItWithAFile(extn);

    printf("---------------------------------------------------------------\n");

}


int main (int argc, char **argv) {

    if (argc == 1) {
        fprintf(stderr, "Usage: %s <extension> [ <extensions> ... ]\n\n", argv[0]);
        fprintf(stderr, "   extension   One or more file extensions to test (no leading period).\n\n");
        return 1;
    }

    printf("assimp %d.%d.%d (%s @ %08x)\n", aiGetVersionMajor(), aiGetVersionMinor(),
           aiGetVersionPatch(), aiGetBranchName(), aiGetVersionRevision());

    for (int a = 1; a < argc; ++ a)
        showExtensionInfo(argv[a]);

    return 0;

}
