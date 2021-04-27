#ifndef PRINTCOLORSTUFF_H
#define PRINTCOLORSTUFF_H

// this is in its own source file because it's like 10000x more code
// than the actual test program.

#include <assimp/scene.h>
#include <assimp/LogStream.hpp>

#define SLIGHTLY_LESS_VERBOSE 0    // non-zero just hides some extraneous stuff
#define PRINTED_DATA_CUTOFF   8U   // max number of property data bytes to print (0U = none)
#define NEVER_USE_ANSI        0    // non-zero to completely disable it

void initTerminal ();
void doneTerminal ();
void ansip (const char *seq, bool reset = true);
void printColorStuff (const aiScene *scene);

struct AnsiLogStream : public Assimp::LogStream {
    void write (const char *message) override;
};

#endif // PRINTCOLORSTUFF_H
