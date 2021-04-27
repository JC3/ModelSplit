#include "printcolorstuff.h"
#include <cstdio>
#include <string>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>
#include <assimp/DefaultLogger.hpp>

using namespace std;

#define ENABLE_ASSIMP_LOGGING 0


static void convert (string input, string assfile, string output, string format) {

    printf("[import] %s\n", input.c_str());
    Assimp::Importer importer;
    auto scene = importer.ReadFile(input, 0);
    if (!scene)
        throw runtime_error(input + ": " + importer.GetErrorString());

    printColorStuff(scene);

    if (assfile != "") {
        printf("  [export] %s\n", assfile.c_str());
        Assimp::Exporter exporter;
        if (exporter.Export(scene, "assxml", assfile) != AI_SUCCESS)
            throw runtime_error(assfile + ": " + exporter.GetErrorString());
    }

    if (output != "") {
        printf("  [export] %s\n", output.c_str());
        Assimp::Exporter exporter;
        if (exporter.Export(scene, format, output) != AI_SUCCESS)
            throw runtime_error(output + ": " + exporter.GetErrorString());
    }

}


int main (int argc, char **argv) {

    const char *input = (argc > 1 ? argv[1] : nullptr);
    const char *format = "3mf";

    if (!input) {
        fprintf(stderr, "\nUsage: %s <filename>\n\n", argv[0]);
        fprintf(stderr, "  Imports <filename>, exports to 3MF, imports that, re-exports, and\n");
        fprintf(stderr, "  one more time for good luck.\n\n");
        fprintf(stderr, "  Produces output-[123].3mf, input.assxml, and output-[123].assxml.\n\n");
        return 1;
    }

    printf("assimp version %d.%d.%d (%s @ %x)\n", aiGetVersionMajor(), aiGetVersionMinor(),
           aiGetVersionPatch(), aiGetBranchName(), aiGetVersionRevision());

#if ENABLE_ASSIMP_LOGGING
    Assimp::DefaultLogger::create("", Assimp::DefaultLogger::VERBOSE, aiDefaultLogStream_STDOUT);
#endif

    try {
        convert(input, "input.assxml", "output-1.3mf", format);
        convert("output-1.3mf", "output-1.assxml", "output-2.3mf", format);
        convert("output-2.3mf", "output-2.assxml", "output-3.3mf", format);
        convert("output-3.3mf", "output-3.assxml", "", "");
        printf("[finished]\n");
    } catch (const exception &x) {
        fprintf(stderr, "[error] %s\n", x.what());
        return 1;
    }

}
