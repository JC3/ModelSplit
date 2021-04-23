#ifndef UTIL_H
#define UTIL_H

#include <optional>
#include <list>
#include <string>

struct aiScene;

// somehow this is more code than the actual test program, so i put it in
// its own source file so i didn't have to look at it.

struct Options {
    std::optional<std::string> extension;
    std::optional<unsigned> index;
    std::list<std::string> filenames;
    bool compact;
    Options (int argc, char **argv);
private:
    void loadlist (const char *filename);
};

std::string sanitize (const char *name);
void debugExport (const char *name, const aiScene *scene);

#endif // UTIL_H
