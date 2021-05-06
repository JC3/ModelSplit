#include <cstdio>
#include <cstring>
#include <set>
#include <map>
#include <vector>
#include <functional>
#include <filesystem>
#include <cassert>
#include <assimp/cimport.h>
#include <zip.h>

#ifdef _WIN32
#  ifdef MAX_PATH
#    undef MAX_PATH
#  endif
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <io.h>
#  undef min
#  undef max
#endif

using namespace std;
using path = filesystem::path;

const constexpr int DEFAULT_LENGTH = 100000; //00;
const constexpr int DEFAULT_CORRUPT_COUNT = 5;
using generator = function<void(FILE*&)>;
using generators = vector<generator>;

generators & operator += (generators &v, const generator &n) {
    v.push_back(n);
    return v;
}

static path gimme (FILE *&f) {
    char filename[MAX_PATH];
    GetFinalPathNameByHandleA((HANDLE)_get_osfhandle(_fileno(f)), filename, sizeof(filename), 0);
    fclose(f);
    f = nullptr;
    return filesystem::weakly_canonical(filename);
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

struct g_zip_empty {
    vector<string> fns_;
    g_zip_empty (const vector<string> &fns = vector<string>()) : fns_(fns) { }
    void operator () (FILE *&f) const {
        zip_t *zip = zip_open(gimme(f).string().c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
        for (string fn : fns_) {
            zip_entry_open(zip, fn.c_str());
            zip_entry_close(zip);
        }
        zip_close(zip);
    }
};

void g_zip_dir (FILE *&f, const path &dir) {
    zip_t *zip = zip_open(gimme(f).string().c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
    for (auto &ent : filesystem::recursive_directory_iterator(dir)) {
        if (!ent.is_directory()) {
            path epath = ent.path();
            path zpath = filesystem::relative(epath, dir);
            zip_entry_open(zip, zpath.generic_string().c_str());
            zip_entry_fwrite(zip, epath.string().c_str());
            zip_entry_close(zip);
        }
    }
    zip_close(zip);
}

void g_rand (FILE *f, unsigned seed) {
    srand(seed);
    for (int k = 0; k < DEFAULT_LENGTH; ++ k)
        fputc(rand() & 0xff, f);
}

void g_trand (FILE *f, unsigned seed) {
    srand(seed);
    for (int k = 0; k < DEFAULT_LENGTH; ++ k) {
        // lazy...
        unsigned char ch;
        for (ch = rand() & 0xff; ch == 0x0b || ch == 0x0c || (!isalnum(ch) && !isspace(ch)); ch = rand() & 0xff)
            ;
        fputc(ch, f);
    }
}

enum CorruptMode { None, Replace, Erase, Duplicate, Zero };

void g_corrupt (FILE *f, const path &srcpath, unsigned seed = 0, CorruptMode mode = Replace) {
    FILE *src = fopen(srcpath.string().c_str(), "rb");
    long length = filelength(_fileno(src)), pos = 0;
    srand(seed);
    set<long> locs;
    for (int k = 0; k < DEFAULT_CORRUPT_COUNT; ++ k)
        locs.insert(std::max(length / 50, (long)((double)length * (double)rand() / (double)RAND_MAX)));
    if (!src) throw runtime_error(strerror(errno));
    int ch;
    while ((ch = fgetc(src)) != EOF) {
        if (locs.find(pos) != locs.end()) {
            if (mode == None) {
                fputc(ch, f);
            } else if (mode == Replace) {
                int newch = rand() & 0xff;
                if (newch == ch) newch = ~ch;
                fputc(newch & 0xff, f);
            } else if (mode == Erase) {
            } else if (mode == Duplicate) {
                fputc(ch, f);
                fputc(ch, f);
            } else if (mode == Zero) {
                fputc(0, f);
            }
        } else {
            fputc(ch, f);
        }
        ++ pos;
    }
    fclose(src);
}

void generate (FILE *filelist, const path &out, const generator &g) {
    printf("%s...\n", out.string().c_str());
    FILE *f = fopen(out.string().c_str(), "wb");
    if (f) {
        g(f); // might change f!
        if (f) fclose(f);
        fprintf(filelist, "%s\n", out.string().c_str());
        fflush(filelist);
    } else {
        throw runtime_error(strerror(errno));
    }
}

int main () {

    using namespace std::placeholders;

    path templates = filesystem::absolute("templates/");
    map<string,generators> magic;

    magic["generic"] += bind(g_rand, _1, 12345);
    magic["generic"] += bind(g_rand, _1, 100);
    magic["generic"] += bind(g_rand, _1, 101);
    magic["generic"] += bind(g_rand, _1, 102);
    magic["generic"] += bind(g_rand, _1, 0);
    magic["generic"] += bind(g_trand, _1, 0);
    magic["generic"] += bind(g_trand, _1, 1);
    magic["generic"] += g_fill({ 0x00 });
    magic["generic"] += g_rept({ ' ' });
    magic["generic"] += g_rept({ '\n' });
    magic["generic"] += g_rept({ '\r', '\n' });
    magic["generic"] += g_rept({ 'x' });
    magic["generic"] += g_rept({ 'x', ' ' });
    magic["generic"] += g_rept({ 'x', '\n' });
    magic["generic"] += g_rept({ 'x', '\r', '\n' });
    magic["generic"] += g_zip_empty({ "a", "b", "c" });
    magic["generic"] += g_only({ 'P', 'K' });
    magic["generic"] += g_fill({ 'P', 'K' }, 0, 100000);
    magic["generic"] += g_only({ '{' });
    magic["generic"] += g_only({ '{', '}' });
    magic["generic"] += g_only({ '}' });
    magic["generic"] += g_rept({ '{', ' ' });
    magic["generic"] += g_rept({ '{', '}' });
    magic["generic"] += g_rept({ '{', '}', ' ' });
    magic["generic"] += g_only({ '<' });
    magic["generic"] += g_only({ '<', '>' });
    magic["generic"] += g_only({ '>' });
    magic["generic"] += g_rept({ '<', ' ' });
    magic["generic"] += g_rept({ '<', '>' });
    magic["generic"] += g_rept({ '<', '>', ' ' });
    magic["obj"] += g_rept({ 'v' });
    magic["obj"] += g_rept({ 'v', ' ' });
    magic["obj"] += g_rept({ 'v', '\n' });
    magic["obj"] += g_rept({ 'v', '\r', '\n' });
    magic["obj"] += g_rept({ 'o', '\r', '\n' });
    magic["obj"] += g_rept({ 'g', '\r', '\n' });
    magic["3ds"] += g_only({ 0x4d });
    magic["3ds"] += g_only({ 0x4d,0x4d });
    magic["3ds"] += g_fill({ 0x4d,0x4d, 0x00,0x00,0x01,0x00 });
    magic["3ds"] += g_fill({ 0x4d,0x4d, 0x06,0x00,0x00,0x00 });
    magic["3ds"] += g_fill({ 0x4d,0x4d, 0xff,0xff,0xff,0xff });
    magic["3ds"] += g_rept({ 0x4d,0x4d, 0x06,0x00,0x00,0x00 });
    magic["3ds"] += g_3ds_deep({ 0x4d,0x4d });
    magic["3ds"] += g_3ds_deep({ 0x3d,0x3d });
    magic["3mf"] += g_zip_empty();
    magic["3mf"] += g_zip_empty({ "3D/3DModel.model" });
    magic["3mf"] += g_zip_empty({ "_rels/.rels" });
    magic["3mf"] += g_zip_empty({ "_rels/.rels", "3D/3DModel.model" });
    magic["3mf"] += g_zip_empty({ "[Content_Types].xml" });
    magic["3mf"] += g_zip_empty({ "[Content_Types].xml", "3D/3DModel.model" });
    magic["3mf"] += g_zip_empty({ "[Content_Types].xml", "_rels/.rels" });
    magic["3mf"] += g_zip_empty({ "[Content_Types].xml", "_rels/.rels", "3D/3DModel.model" });
    magic["3mf"] += g_zip_empty({ "[Content_Types].xml", "_rels/.rels", "3D/3DModel.model", "a", "b", "c" });
    magic["3mf"] += bind(g_corrupt, _1, templates / "box.3mf", 10001, Replace);
    magic["3mf"] += bind(g_corrupt, _1, templates / "box.3mf", 10002, Erase);
    magic["3mf"] += bind(g_corrupt, _1, templates / "box.3mf", 10003, Duplicate);
    magic["3mf"] += bind(g_corrupt, _1, templates / "box.3mf", 10004, Zero);
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badcontenttypes/");
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badrelpath1/");
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badrelpath2/");
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badreltype/");
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badxmlct/");
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badxmlrel/");
    magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.badxmlmodel/");
    //magic["3mf"] += bind(g_zip_dir, _1, templates / "box.3mf.differentmodel/");

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
