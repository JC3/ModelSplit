#include <cstdio>
#include <cstring>
#include <map>
#include <vector>
#include <functional>
#include <filesystem>
#include <cassert>
#include <assimp/cimport.h>
#include <zip.h>

using namespace std;
using path = filesystem::path;

const constexpr int DEFAULT_LENGTH = 10000000;
using generator = function<void(FILE*)>;
using generators = vector<generator>;

generators & operator += (generators &v, const generator &n) {
    v.push_back(n);
    return v;
}

struct g_only {
    vector<uint8_t> bytes_;
    g_only (const vector<uint8_t> &bytes) : bytes_(bytes) { }
    void operator () (FILE *f) const {
        fwrite(&(bytes_[0]), bytes_.size(), 1, f);
    }
};

struct g_fill {
    vector<uint8_t> bytes_;
    int len_;
    char fc_;
    g_fill (const vector<uint8_t> &bytes, char fc = 0, int len = DEFAULT_LENGTH) : bytes_(bytes), len_(len), fc_(fc) { }
    void operator () (FILE *f) const {
        fwrite(&(bytes_[0]), bytes_.size(), 1, f);
        for (int k = (int)bytes_.size(); k < len_; ++ k)
            fputc(fc_, f);
    }
};

struct g_rept {
    vector<uint8_t> bytes_;
    int len_;
    g_rept (const vector<uint8_t> &bytes, int len = DEFAULT_LENGTH) : bytes_(bytes), len_(len) { }
    void operator () (FILE *f) const {
        fwrite(&(bytes_[0]), bytes_.size(), 1, f);
        for (int k = (int)bytes_.size(); k < len_; k += (int)bytes_.size())
            fwrite(&(bytes_[0]), bytes_.size(), 1, f);
    }
};

struct g_3ds_deep {
    const uint8_t id_[2];
    int len_;
    g_3ds_deep (uint8_t id0, uint8_t id1, int len = DEFAULT_LENGTH) : id_{id0,id1}, len_(len) { }
    void operator () (FILE *f) const {
        int32_t remaining = (int32_t)((len_ + 5) / 6) * 6;
        while (remaining > 0) {
            fwrite(id_, 2, 1, f);
            fwrite(&remaining, 4, 1, f);
            remaining -= 6;
        }
    }
};

void g_rand (FILE *f, unsigned seed) {
    srand(seed);
    for (int k = 0; k < DEFAULT_LENGTH; ++ k)
        fputc(rand() & 0xff, f);
}

void generate (FILE *filelist, const path &out, const generator &g) {
    printf("%s...\n", out.string().c_str());
    FILE *f = fopen(out.string().c_str(), "wb");
    if (f) {
        g(f);
        fclose(f);
        fprintf(filelist, "%s\n", out.string().c_str());
        fflush(filelist);
    } else {
        throw runtime_error(strerror(errno));
    }
}

int main () {

    using namespace std::placeholders;

    map<string,generators> magic;
    magic["generic"] += bind(g_rand, _1, 12345);
    magic["generic"] += g_fill({ 0x00 });
    magic["3ds"] += g_only({ 0x4d });
    magic["3ds"] += g_only({ 0x4d,0x4d });
    magic["3ds"] += g_fill({ 0x4d,0x4d, 0x00,0x00,0x01,0x00 });
    magic["3ds"] += g_fill({ 0x4d,0x4d, 0x06,0x00,0x00,0x00 });
    magic["3ds"] += g_fill({ 0x4d,0x4d, 0xff,0xff,0xff,0xff });
    magic["3ds"] += g_rept({ 0x4d,0x4d, 0x06,0x00,0x00,0x00 });
    magic["3ds"] += g_3ds_deep({ 0x4d,0x4d });
    magic["3ds"] += g_3ds_deep({ 0x3d,0x3d });

    FILE *filelist = nullptr;
    int result = EXIT_SUCCESS;

    try {
        if (!(filelist = fopen("garbage-files.txt", "wt")))
            throw runtime_error(string("garbage-files.txt: ") + strerror(errno));
        aiString extns;
        aiGetExtensionList(&extns);
        extns.Append(";*.generic");
        for (char *extn = strtok(extns.data, " *.;"); extn; extn = strtok(nullptr, " *.;")) {
            transform(extn, extn + strlen(extn), extn, [](unsigned char c){return tolower(c);});
            const generators &gens = magic[extn];
            if (gens.empty()) {
                fprintf(stderr, "todo: need magic for '%s'\n", extn);
            } else {
                if (gens.size() == 1) {
                    path outfile = path("garbage").replace_extension(extn);
                    generate(filelist, outfile, gens[0]);
                } else {
                    for (int k = 0; k < gens.size(); ++ k) {
                        char filename[100];
                        snprintf(filename, sizeof(filename), "garbage-%d", k + 1);
                        path outfile = path(filename).replace_extension(extn);
                        generate(filelist, outfile, gens[k]);
                    }
                }
            }
        }
    } catch (const std::exception &x) {
        fprintf(stderr, "error: %s\n", x.what());
        result = EXIT_FAILURE;
    }

    fclose(filelist);

    return result;

}
