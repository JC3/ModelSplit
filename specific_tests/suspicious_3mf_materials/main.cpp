#include <cstdio>
#include <string>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/version.h>

using namespace std;


static void convert (string input, string assfile, string output, string format) {

    if (output != "")
        printf("[converting] %s -> %s\n", input.c_str(), output.c_str());
    else
        printf("[dumping] %s -> %s\n", input.c_str(), assfile.c_str());

    Assimp::Importer importer;
    auto scene = importer.ReadFile(input, 0);
    if (!scene)
        throw runtime_error(input + ": " + importer.GetErrorString());

    if (output != "") {
        Assimp::Exporter exporter;
        if (exporter.Export(scene, format, output) != AI_SUCCESS)
            throw runtime_error(output + ": " + exporter.GetErrorString());
    }

    if (output != "")
        printf("[dumping] %s -> %s\n", input.c_str(), assfile.c_str());

    {
        Assimp::Exporter exporter;
        if (exporter.Export(scene, "assxml", assfile) != AI_SUCCESS)
            throw runtime_error(assfile + ": " + exporter.GetErrorString());
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
