#include "util.hh"
#include <cstdio>
#include <filesystem>
#include <assimp/cimport.h>
#include <assimp/importerdesc.h>

using namespace std;


Options::Options (int argc, char **argv) : compact(false) {

    int state = 0;

    for (int a = 1; a < argc && state >= 0; ++ a) {
        switch (state) {
        case 0:
            if (!strcmp(argv[a], "--"))
                state = 1;
            else if (!strcmp(argv[a], "-x"))
                state = 2;
            else if (!strcmp(argv[a], "-n"))
                state = 3;
            else if (!strcmp(argv[a], "-f"))
                state = 4;
            else if (!strcmp(argv[a], "-b"))
                compact = true;
            else if (argv[a][0] != '-')
                filenames.push_back(argv[a]);
            else
                state = -1;
            break;
        case 1: filenames.push_back(argv[a]); break;
        case 2: extension = argv[a]; state = 0; break;
        case 3: index = atoi(argv[a]); state = 0; break;
        case 4: loadlist(argv[a]); state = 0; break;
        }
    }

    if (state || filenames.empty() || (!extension.has_value() && !index.has_value())) {
        string me = filesystem::path(argv[0]).filename().string();
        printf("\nUsage: %s ( -x <extn> | -n <index> ) [ -b ] [ -- ] <filename> [ <filenames> ... ]\n", me.c_str());
        printf("       %s ( -x <extn> | -n <index> ) [ -b ] -f <listfile>\n\n", me.c_str());
        printf("    -x extn      Force use of loader registered to handle <extn>.\n");
        printf("    -n index     Force use of loader at index <index>.\n");
        printf("    -b           More compact output.\n");
        printf("    filename(s)  One or more files to try and load.\n");
        printf("    listfile     A file containing a list of files to load, one per line.\n\n");
        printf("    Note: Option parser is pretty loose so you can specify multiple -f's if you\n");
        printf("          want, and even combine with individual filenames. It's not a bug, it's\n");
        printf("          a feature, right?\n\n");
        printf("    %-6s %-25s IMPORTER\n\n", "INDEX", "EXTN");
        for (unsigned k = 0; k < aiGetImportFormatCount(); ++ k) {
            auto info = aiGetImportFormatDescription(k);
            printf("    %-6u %-26s %s\n", k, info->mFileExtensions, sanitize(info->mName).c_str());
        }
        printf("\n");
        exit(1);
    }

}


void Options::loadlist (const char *filename) {

    FILE *f = fopen(filename, "rt");
    if (!f) {
        perror(filename);
        exit(1);
    }

    char buffer[5000];
    while (fgets(buffer, sizeof(buffer), f)) {
        char *trimmed;
        for (trimmed = buffer; *trimmed && isspace(*trimmed); ++ trimmed)
            ;
        for (int n = (int)strlen(trimmed) - 1; n >= 0 && isspace(trimmed[n]); -- n)
            trimmed[n] = 0;
        if (*trimmed)
            filenames.push_back(trimmed);
    }

    fclose(f);

}


// workaround for https://github.com/assimp/assimp/issues/3797
string sanitize (const char *name) {

    string clean;
    for (const char *ch = name; *ch; ++ ch)
        clean += isspace(*ch) ? ' ' : *ch;

    return clean;

}

