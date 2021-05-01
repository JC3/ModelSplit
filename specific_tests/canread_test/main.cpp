// warning: this code is a mess.
// note to self: you'll never remember how this works.

#ifndef HTML_OUTPUT
#  define HTML_OUTPUT 1 // zero disables and also removes qt dependency
#endif

#if HTML_OUTPUT
#  include <QCoreApplication>
#  include <QDomDocument>
#  include <QDomNode>
#  include <QDomElement>
#  include <QFile>
#  include <QTextStream>
#endif
#include <cstdio>
#include <list>
#include <string>
#include <filesystem>
#include <optional>
#include <cstdarg>
#include <assimp/version.h>
#include <assimp/Importer.hpp>
#include <assimp/BaseImporter.h>
#include <assimp/cimport.h>
#include <assimp/DefaultIOSystem.h>

#ifdef _WIN32
#  define popen _popen
#  define pclose _pclose
#endif

#define EXIT_STD_EXCEPTION    80
#define EXIT_OTHER_EXCEPTION  81
#define EXIT_TEST_ERROR       82

#define CANREAD_CHECKSIG      true
#define VERBOSE_PROGRESS      1

using namespace std;
using path = filesystem::path;


static char * trim (char *str) {

    if (str) {
        while (*str && isspace(*str))
            ++ str;
        for (int n = (int)strlen(str) - 1; n >= 0 && isspace(str[n]); -- n)
            str[n] = 0;
    }

    return str;

}


static void writeMessage (char kind, const string &detail) {

    if (!detail.empty())
        printf("%c %s\n", kind, detail.c_str());
    else
        printf("%c\n", kind);

    fflush(stdout);

}


static void writeMessagef (char kind, const char *format, ...) {

    char buf[1000];

    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);

    writeMessage(kind, buf);

}


struct test_error : public std::runtime_error {
    explicit test_error (const string &message) : std::runtime_error(message) { }
    test_error (const string &m1, const string &m2) : std::runtime_error(m1 + ": " + m2) { }
};


static int runClient (const vector<path> &testfiles, int fileidx, int importeridx) {

    Assimp::DefaultIOSystem io;
    int importers = (int)aiGetImportFormatCount();

    for (int nfile = fileidx; nfile < testfiles.size(); ++ nfile) {
        for (int nimp = importeridx; nimp < importers; ++ nimp) {
            writeMessagef('s', "%d %d", nfile, nimp);
            bool readable = false;
            try {
                Assimp::Importer importer;
                Assimp::BaseImporter *loader = importer.GetImporter(nimp);
                if (!loader)
                    throw runtime_error("no loader");
                readable = loader->CanRead(testfiles[nfile].string(), &io, CANREAD_CHECKSIG);
            } catch (const std::exception &x) {
                writeMessage('e', x.what());
            } catch (...) {
                writeMessage('e', "unknown exception");
            }
            writeMessagef('f', "%d %d %d", nfile, nimp, (int)readable);
        }
        importeridx = 0;
    }

    return EXIT_SUCCESS;

}


static string makeCommand (const path &myself, int fileidx, int importeridx, const path &listfile, const path &basedir) {

    return (stringstream()
            << filesystem::relative(myself).string() << " --go " << fileidx << " " << importeridx
            << " \"" << listfile.string() << "\" \"" << basedir.string() << "\"").str();

}


struct Result {
    bool tested;
    bool canread;
    bool haderror;
    bool crashed;
    string message;
    Result () : tested(false), canread(false), haderror(false), crashed(false) { }
};

struct ModelResult {
    path modelfile;
    vector<Result> results;
    set<int> importersForExtension;
    optional<int> primaryImporter;
    ModelResult (const path &modelfile, int importers) : modelfile(modelfile), results(importers) { queryImporters(); }
private:
    void queryImporters ();
};

typedef shared_ptr<ModelResult> ModelResultPtr;

static string strtolower (string s) {
    transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return tolower(c); });
    return s;
}

void ModelResult::queryImporters () {

    static map<string,set<int> > all;
    static map<string,optional<int> > primary;

    if (all.empty()) {
        printf("initializing file extension info...\n");
        Assimp::Importer importer;
        for (unsigned k = 0; k < importer.GetImporterCount(); ++ k) {
            auto info = importer.GetImporterInfo(k);
            istringstream iss(info->mFileExtensions);
            for (auto ext = istream_iterator<string>(iss); ext != istream_iterator<string>(); ++ ext) {
                string e = strtolower(*ext);
                all[e].insert(k);
                all["." + e].insert(k);
                printf("  %s: %d (%s)\n", e.c_str(), k, info->mName);
            }
        }
    }

    string extn = modelfile.extension().string();

    if (extn.empty()) {
        importersForExtension.clear();
        primaryImporter.reset();
    } else {
        extn = strtolower(extn);
        importersForExtension = all[extn];
        auto pit = primary.find(extn);
        if (pit == primary.end()) {
            printf("caching info for %s...\n", extn.c_str());
            Assimp::Importer imp;
            int index = (int)imp.GetImporterIndex(extn.c_str());
            primaryImporter = (index == -1 ? optional<int>() : optional<int>(index));
            primary[extn] = primaryImporter;
        } else {
            primaryImporter = pit->second;
        }
    }

}


#if HTML_OUTPUT

static QDomElement addChild (QDomNode addto, const QString &tag) {

    QDomElement node = addto.ownerDocument().createElement(tag);
    addto.appendChild(node);
    return node;

}

struct StringHack {
    StringHack (const QString &str) : s(str) { }
    StringHack (const string &str) : s(QString::fromStdString(str)) { }
    StringHack (const char *str) : s(str) { }
    operator QString () const { return s; }
    QString s;
};

static QDomElement addChild (QDomNode addto, const QString &tag, const StringHack &str) {

    QDomElement node = addChild(addto, tag);
    node.appendChild(addto.ownerDocument().createTextNode(str));
    return node;

}

static void writeHtmlReport (const path &reportfile, const vector<ModelResultPtr> &results, int importers) {

    printf("writing %s (html)...\n", reportfile.string().c_str());

    Assimp::Importer imp;

    QDomDocument doc("html");
    QDomElement html = addChild(doc, "html");
    html.setAttribute("lang", "en");
    QDomElement head = addChild(html, "head");
    addChild(head, "title", QString("CanRead Report (%1 checkSig)").arg(CANREAD_CHECKSIG ? "with" : "without"));
    QDomElement body = addChild(html, "body");
    QDomElement main = addChild(body, "main");

#if 0
    QDomElement csslink = addChild(head, "link");
    csslink.setAttribute("rel", "stylesheet");
    csslink.setAttribute("href", "report.css");
#else
    //QString stylesheet = QString::fromLatin1(QResource(":/html/css").uncompressedData());
    {
        QFile fstylesheet(":/html/css");
        fstylesheet.open(QFile::ReadOnly | QFile::Text);
        addChild(head, "style", QString::fromLatin1(fstylesheet.readAll())).setAttribute("type", "text/css");
    }
#endif

    QDomElement table = addChild(addChild(main, "article"), "table");
    QDomElement header = addChild(table, "tr");

    addChild(header, "th", "Extn").setAttribute("class", "extn");
    for (int k = 0; k < importers; ++ k)
        addChild(addChild(header, "th"), "div", imp.GetImporterInfo(k)->mFileExtensions);
    addChild(header, "th", "Model File").setAttribute("class", "filename");

    for (ModelResultPtr result : results) {
        QDomElement row = addChild(table, "tr");
        addChild(row, "td", result->modelfile.extension().string()).setAttribute("class", "extn");
        for (int k = 0; k < importers; ++ k) {
            QStringList classes;
            Result res = (k < result->results.size() ? result->results[k] : Result());
            classes.append("result");
            //
            if (!res.tested)
                classes.append("untested");
            else if (res.crashed)
                classes.append("crashed");
            else if (res.haderror)
                classes.append("haderror");
            else if (res.canread)
                classes.append("canread");
            else
                classes.append("cantread");
            //
            if (result->importersForExtension.find(k) != result->importersForExtension.end())
                classes.append("imp");
            if (result->primaryImporter == k)
                classes.append("pimp");
            QDomElement td = addChild(row, "td");
            if (!classes.empty())
                td.setAttribute("class", classes.join(' '));
            if (!res.message.empty())
                td.setAttribute("title", QString::fromStdString(res.message));
        }
        addChild(row, "td", filesystem::relative(result->modelfile).string()).setAttribute("class", "filename");
    }

    QString versionstr = QString().sprintf("assimp version: %d.%d.%d (%s @ %x)",
                                           aiGetVersionMajor(),
                                           aiGetVersionMinor(),
                                           aiGetVersionPatch(),
                                           aiGetBranchName(),
                                           aiGetVersionRevision());
    addChild(addChild(body, "footer"), "div", versionstr);

    QFile out(reportfile.string().c_str());
    if (!out.open(QFile::WriteOnly | QFile::Text))
        throw runtime_error(out.errorString().toStdString());
    else {
        QTextStream outs(&out);
        doc.save(outs, 1);
        out.close();
    }

}

#endif


static void writeReport (const path &reportfile, const vector<ModelResultPtr> &results, int importers) {

    printf("writing %s (csv)...\n", reportfile.string().c_str());

    FILE *f = fopen(reportfile.string().c_str(), "wt");
    if (!f)
        throw runtime_error(strerror(errno));

    Assimp::Importer imp;

    fprintf(f, ",");
    for (int k = 0; k < importers; ++ k) {
        string extns = imp.GetImporterInfo(k)->mFileExtensions;
        replace(extns.begin(), extns.end(), ' ', '\n');
        fprintf(f, ",\"%s\"", extns.c_str());
    }
    fprintf(f, "\n");

    fprintf(f, "model,extn");
    for (int k = 0; k < importers; ++ k)
        fprintf(f, ",%d", k);
    fprintf(f, "\n");

    for (ModelResultPtr result : results) {
        fprintf(f, "\"%s\"", filesystem::relative(result->modelfile).string().c_str());
        fprintf(f, ",%s", result->modelfile.extension().string().c_str());
        for (Result &r : result->results) {
            if (!r.tested)
                fprintf(f, ",?");
            else if (r.crashed)
                fprintf(f, ",!");
            else if (r.haderror)
                fprintf(f, ",e");
            else if (r.canread)
                fprintf(f, ",Y");
            else
                fprintf(f, ",");
        }
        fprintf(f, "\n");
    }

    fclose(f);

}


#if VERBOSE_PROGRESS
static string rclip (string str, int maxwidth) {

    if (str.length() <= maxwidth)
        return str;
    else
        return "..." + str.substr(str.length() - (maxwidth - 3));

}
#endif


static void setResult (vector<ModelResultPtr> &results, int fileidx, int importeridx,
                       bool readable, bool haderror, bool crashed, const string &message)
{

    if (fileidx < 0 || fileidx >= results.size())
        throw runtime_error("setResult: bad file index");
    ModelResultPtr modelresult = results[fileidx];

    if (importeridx < 0 || importeridx >= modelresult->results.size())
        throw runtime_error("setResult: bad importer index");
    Result &result = modelresult->results[importeridx];

    if (result.tested)
        throw runtime_error("setResult: test already performed");

    result.tested = true;
    result.canread = readable;
    result.haderror = haderror;
    result.crashed = crashed;
    result.message = message;

#if VERBOSE_PROGRESS
    static int lastfileidx = -1;
    if (fileidx != lastfileidx) {
        printf("\n[%3d/%3d] %48s: ", fileidx + 1, (int)results.size(), rclip(filesystem::relative(modelresult->modelfile).string(), 48).c_str());
        lastfileidx = fileidx;
    }
    if (crashed)
        putchar('!');
    else if (haderror)
        putchar('x');
    else if (readable)
        putchar('Y');
    else
        putchar('.');
    fflush(stdout);
#else
    if (crashed)
        printf("%s (%d): crashed\n", filesystem::relative(modelresult->modelfile).string().c_str(), importeridx);
    else if (haderror)
        printf("%s (%d): exception: %s\n", filesystem::relative(modelresult->modelfile).string().c_str(), importeridx, message.c_str());
#endif

}


static int runServer (const vector<path> &testfiles, const path &myself, const path &listfile, const path &basedir) {

    int fileidx = 0, importeridx = 0;
    int importers = (int)aiGetImportFormatCount(), lastretcode = -1;

    vector<ModelResultPtr> results;
    for (path modelfile : testfiles)
        results.push_back(ModelResultPtr(new ModelResult(modelfile, importers)));

    while (fileidx < testfiles.size() && importeridx < importers) {

        string command = makeCommand(myself, fileidx, importeridx, listfile, basedir);
        FILE *p = popen(command.c_str(), "rt");
        if (!p)
            throw runtime_error(string("popen: ") + strerror(errno));

        char buf[1000], *message, type, *data;
        int mfileidx, mimporteridx, mreadable, curfileidx = -1, curimporteridx = -1;
        string curmessage;
        bool intest = false, haderror = false;
        while (fgets(buf, sizeof(buf), p)) {
            message = trim(buf);
            type = message[0];
            data = (message[0] && message[1]) ? (message + 2) : nullptr;
            if (type == 's' && !intest && data && sscanf(data, "%d %d", &mfileidx, &mimporteridx) == 2) {
                curfileidx = mfileidx;
                curimporteridx = mimporteridx;
                curmessage = "";
                intest = true;
                haderror = false;
                mreadable = false;
                if (curfileidx < 0 || curfileidx >= testfiles.size() || curimporteridx < 0 || curimporteridx >= importers)
                    throw runtime_error("bad s index received");
            } else if (type == 'f' && intest && data && sscanf(data, "%d %d %d", &mfileidx, &mimporteridx, &mreadable) == 3) {
                if (mfileidx != curfileidx || mimporteridx != curimporteridx)
                    throw runtime_error("bad f index received");
                setResult(results, curfileidx, curimporteridx, mreadable, haderror, false, curmessage);
                intest = false;
            } else if ((type == 'e' || type == 'x') && intest && data) {
                curmessage = data;
                haderror = true;
            } else {
                fprintf(stderr, "weird message: %s\n", message);
                throw runtime_error("weirdness");
            }
        }

        lastretcode = pclose(p);
        if (lastretcode == EXIT_TEST_ERROR)
            throw runtime_error("unrecoverable: " + curmessage);

        if (intest)
            setResult(results, curfileidx, curimporteridx, false, true, true, curmessage);

        importeridx = curimporteridx + 1;
        if (importeridx >= importers) {
            fileidx = curfileidx + 1;
            importeridx = 0;
        } else {
            fileidx = curfileidx;
        }

    }

#if VERBOSE_PROGRESS
    printf("\n");
#endif

    if (lastretcode != EXIT_SUCCESS)
        printf("warning: unexpected return code %d (0x%x)\n", lastretcode, lastretcode);

    writeReport(filesystem::absolute("report.csv"), results, importers);
#if HTML_OUTPUT
    writeHtmlReport(filesystem::absolute("report.html"), results, importers);
#endif

    return EXIT_SUCCESS;

}


int main (int argc, char **argv) {

#if HTML_OUTPUT
    QCoreApplication app(argc, argv);
#endif

    // -------- command line -------------------------------------------------

    // canread_test listfile [ basedir ]
    // canread_test --go start_file_index start_importer_index listfile [ basedir ]

    bool runner = (argc > 1 && !strcmp(argv[1], "--go"));
    int fileidx = -1, importeridx = -1;
    path listfile, basedir, myself = filesystem::absolute(argv[0]);

    if (runner && (argc == 5 || argc == 6)) {
        fileidx = atoi(argv[2]);
        importeridx = atoi(argv[3]);
        listfile = filesystem::absolute(argv[4]);
        basedir = filesystem::absolute(argc > 5 ? argv[5] : ".");
    } else if (!runner && (argc == 2 || argc == 3)) {
        listfile = filesystem::absolute(argv[1]);
        basedir = filesystem::absolute(argc > 2 ? argv[2] : ".");
    } else {
        fprintf(stderr, "\nUsage: %s <listfile> [ <basedir> ]\n\n", myself.filename().string().c_str());
        fprintf(stderr, "  listfile  File containing a list of 3D model files, one per line.\n");
        fprintf(stderr, "  basedir   Optional: Path that everything in <listfile> is relative to.\n\n");
        return EXIT_FAILURE;
    }

    // -------- file list ----------------------------------------------------

    vector<path> testfiles;

    FILE *f = fopen(listfile.string().c_str(), "rt");
    if (!f) {
        if (runner) {
            writeMessage('x', listfile.filename().string() + ": " + strerror(errno));
            return EXIT_TEST_ERROR;
        } else {
            perror(listfile.filename().string().c_str());
            return EXIT_FAILURE;
        }
    }

    char line[5000], *trimmed;
    while (fgets(line, sizeof(line), f)) {
        if (*(trimmed = trim(line)))
            testfiles.push_back(filesystem::absolute(basedir / trimmed));
    }

    fclose(f);

    // -------- run ----------------------------------------------------------

    int result = EXIT_FAILURE;
    if (runner) {
        try {
            result = runClient(testfiles, fileidx, importeridx);
        } catch (const test_error &te) {
            writeMessage('x', te.what());
            result = EXIT_TEST_ERROR;
        } catch (const std::exception &x) {
            writeMessage('x', x.what());
            result = EXIT_STD_EXCEPTION;
        } catch (...) {
            writeMessage('x', "unknown exception");
            result = EXIT_OTHER_EXCEPTION;
        }
    } else {
        try {
            result = runServer(testfiles, myself, listfile, basedir);
        } catch (const std::exception &x) {
            fprintf(stderr, "error: %s\n", x.what());
        }
    }

    return result;

}
