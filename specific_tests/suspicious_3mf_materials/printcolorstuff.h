#ifndef PRINTCOLORSTUFF_H
#define PRINTCOLORSTUFF_H

// this is in its own source file because it's like 10000x more code
// than the actual test program.

#include <assimp/scene.h>

#define SLIGHTLY_LESS_VERBOSE 0    // non-zero just hides some extraneous stuff
#define PRINTED_DATA_CUTOFF   8U   // max number of property data bytes to print (0U = none)

void printColorStuff (const aiScene *scene);

#endif // PRINTCOLORSTUFF_H
