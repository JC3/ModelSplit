#include "printcolorstuff.h"
#include <cstdio>
#include <string>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>
#include <assimp/DefaultLogger.hpp>
#include <assimp/LogStream.hpp>

using namespace std;

static const char FORMAT_MESSAGE[] = "1;37";
static const char FORMAT_ERROR[] = "1;31";


static void convert (string input, string assfile, string output, string format, bool verbose) {

    ansip(FORMAT_MESSAGE);
    printf("\n[IMPORT] %s\n", input.c_str());
    Assimp::Importer importer;
    auto scene = importer.ReadFile(input, 0);
    if (!scene)
        throw runtime_error(input + ": " + importer.GetErrorString());

    printColorStuff(scene, verbose);

    if (assfile != "") {
        ansip(FORMAT_MESSAGE);
        printf("  [EXPORT] %s\n", assfile.c_str());
        Assimp::Exporter exporter;
        if (exporter.Export(scene, "assxml", assfile) != AI_SUCCESS)
            throw runtime_error(assfile + ": " + exporter.GetErrorString());
    }

    if (output != "") {
        ansip(FORMAT_MESSAGE);
        printf("  [EXPORT] %s\n", output.c_str());
        Assimp::Exporter exporter;
        if (exporter.Export(scene, format, output) != AI_SUCCESS)
            throw runtime_error(output + ": " + exporter.GetErrorString());
    }

}


int main (int argc, char **argv) {

    const char *input = nullptr;
    const char *format = "3mf";
    bool verbose = false, logging = false;
    int a;

    for (a = 1; a < argc; ++ a) {
        if (!strcmp(argv[a], "-v"))
            verbose = true;
        else if (!strcmp(argv[a], "-vv"))
            verbose = logging = true;
        else if (argv[a][0] != '-' && !input)
            input = argv[a];
        else
            break;
    }

    if (a < argc || !input) {
        fprintf(stderr, "\nUsage: %s [ -v | -vv ] <filename>\n\n", argv[0]);
        fprintf(stderr, "  Imports <filename>, exports to 3MF, imports that, re-exports, and\n");
        fprintf(stderr, "  one more time for good luck.\n\n");
        fprintf(stderr, "  Produces output-[123].3mf, input.assxml, and output-[123].assxml.\n\n");
        fprintf(stderr, "  -v    Slightly more verbose output.\n");
        fprintf(stderr, "  -vv   Slightly more verbose output + print assimp logs.\n\n");
        return 1;
    }

    initTerminal();
    printf("assimp version %d.%d.%d (%s @ %x)\n", aiGetVersionMajor(), aiGetVersionMinor(),
           aiGetVersionPatch(), aiGetBranchName(), aiGetVersionRevision());

    if (logging)
        Assimp::DefaultLogger::create("", Assimp::DefaultLogger::VERBOSE, 0)->attachStream(new AnsiLogStream());

    int result = 0;
    try {
        convert(input, "input.assxml", "output-1.3mf", format, verbose);
        convert("output-1.3mf", "output-1.assxml", "output-2.3mf", format, verbose);
        convert("output-2.3mf", "output-2.assxml", "output-3.3mf", format, verbose);
        convert("output-3.3mf", "output-3.assxml", "", "", verbose);
        ansip(FORMAT_MESSAGE);
        printf("\n[FINISHED]\n");
    } catch (const exception &x) {
        ansip(FORMAT_ERROR);
        fprintf(stderr, "[ERROR] %s\n", x.what());
        result = 1;
    }

    doneTerminal();
    return result;

}
