#include "util.hh"
#include <cstdio>
#include <string>
#include <assimp/version.h>
#include <assimp/Importer.hpp>
#include <assimp/BaseImporter.h>
#include <assimp/DefaultIOSystem.h>

using namespace std;

#define DEBUG_EXPORT_MODELS 0


static void bye (const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}


int main (int argc, char **argv) {

    Options opts(argc, argv);

    printf("assimp version: %d.%d.%d (%s @ %8x)\n", aiGetVersionMajor(),
           aiGetVersionMinor(), aiGetVersionPatch(), aiGetBranchName(),
           aiGetVersionRevision());

    bool showinfo = true;
    for (string filename : opts.filenames) {

        Assimp::Importer importer;
        Assimp::BaseImporter *loader;
        Assimp::DefaultIOSystem io;

        if (opts.extension.has_value())
            loader = importer.GetImporter(opts.extension.value().c_str());
        else if (opts.index.has_value())
            loader = importer.GetImporter(opts.index.value());
        if (!loader)
            bye("the specified importer could not be found.");

        if (showinfo) {
            auto info = loader->GetInfo();
            printf("-----------------------------------------------------------------\n");
            printf("Importer:\n");
            printf("  mName:       %s\n", sanitize(info->mName).c_str());
            printf("  mFileExtns:  %s\n", info->mFileExtensions);
            printf("  mAuthor:     %s\n", info->mAuthor);
            printf("  mMaintainer: %s\n", info->mMaintainer);
            printf("  mComments:   %s\n", info->mComments);
            printf("-----------------------------------------------------------------\n");
            showinfo = false;
        }

        bool readable = false;
        string readableStr;
        try {
            readable = loader->CanRead(filename, &io, false);
            readableStr = (readable ? "yes" : "no");
        } catch (const std::exception &x) {
            readableStr = x.what();
        }

        bool readableSig = false;
        string readableSigStr;
        try {
            readableSig = loader->CanRead(filename, &io, true);
            readableSigStr = (readableSig ? "yes" : "no");
        } catch (const std::exception &x) {
            readableSigStr = x.what();
        }

        const aiScene *scene = loader->ReadFile(&importer, filename, &io);

#if DEBUG_EXPORT_MODELS
        if (scene)
            debugExport(filename.c_str(), scene);
#endif

        if (opts.compact) {
            printf("[%s|%c%c] %s: %s\n", scene ? "OK" : "  ", readable ? 'R' : ' ',
                   readableSig ? 'S' : ' ', filename.c_str(),
                   scene ? "loaded" : loader->GetErrorText().c_str());
        } else {
            printf("File: %s\n", filename.c_str());
            printf("  CanRead(checkSig=false): %s\n", readableStr.c_str());
            printf("  CanRead(checkSig=true):  %s\n", readableSigStr.c_str());
            printf("  ReadFile:                %s\n", scene ? "success" : loader->GetErrorText().c_str());
            printf("-----------------------------------------------------------------\n");
        }

    }


}
