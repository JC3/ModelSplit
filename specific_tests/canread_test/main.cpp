// warning: this code is a mess.
// note to self: you'll never remember how this works.

#ifndef HTML_OUTPUT
#  define HTML_OUTPUT 1 // zero disables and also removes qt dependency
#endif

#ifndef PDF_OUTPUT
#  define PDF_OUTPUT 1 // zero disables; requires HTML_OUTPUT
#endif

#if HTML_OUTPUT
#  include <QDomDocument>
#  include <QDomNode>
#  include <QDomElement>
#  include <QFile>
#  include <QTextStream>
#  include <QMimeDatabase>
#  include <QResource>
#  if PDF_OUTPUT
#    include <QWebEnginePage>
#    include <QApplication>
#  else
#    include <QCoreApplication>
#  endif
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
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#ifdef _WIN32
#  define popen _popen
#  define pclose _pclose
#endif

#define EXIT_STD_EXCEPTION    80
#define EXIT_OTHER_EXCEPTION  81
#define EXIT_TEST_ERROR       82

#define TEST_CANREAD          0
#define TEST_CANREAD_CHECKSIG 1
#define TEST_READ             2
#define TEST_IMPORT           3

#define WHICH_TEST            TEST_CANREAD_CHECKSIG
#define VALIDATE_SCENES       1   // only relevant for TEST_IMPORT
#define VERBOSE_PROGRESS      1
#define FILTER_COFF_OBJ       1

// these are ignored if !HTML_OUTPUT
#define FLOATING_HEADER       1
#define EMBED_FONT            1
#define EMBED_FONT_FILE       ":/font/report"  // if EMBED_FONT
#define NOEMBED_FONT_FAMILY   "Bahnschrift Condensed"  // used whether embed_font or not.

using namespace std;
using path = filesystem::path;


// for debug *and* release configurations
#define always_assert(expr) do { if (!(expr)) { \
      fprintf(stderr, "always_assert(%s:%d): %s\n", __FILE__, __LINE__, #expr); \
      abort(); \
    } } while (0)


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
        printf("\n!@#$ %c %s\n", kind, detail.c_str());
    else
        printf("\n!@#$ %c\n", kind);

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
#if HTML_OUTPUT
    explicit test_error (const QString &message) : std::runtime_error(message.toStdString()) { }
    test_error (const QString &m1, const QString &m2) : std::runtime_error((m1 + ": " + m2).toStdString()) { }
#endif
};


static int runClient (const vector<path> &testfiles, int fileidx, int importeridx) {

#if WHICH_TEST != TEST_IMPORT
    Assimp::DefaultIOSystem io;
    int importers = (int)aiGetImportFormatCount();
#else
    (void)importeridx;
#endif

    for (int nfile = fileidx; nfile < (int)testfiles.size(); ++ nfile) {
#if WHICH_TEST == TEST_IMPORT
        writeMessagef('S', "%d", nfile);
        bool readable = false;
        bool emptyscene = false;
        Assimp::Importer importer;
        try {
#  if VALIDATE_SCENES
            readable = (importer.ReadFile(testfiles[nfile].string(), aiProcess_ValidateDataStructure) != nullptr);
#  else
            readable = (importer.ReadFile(testfiles[nfile].string(), 0) != nullptr);
#  endif
            if (!readable)
                writeMessage('E', importer.GetErrorString());
            else
                emptyscene = !importer.GetScene()->HasMeshes();
        } catch (const std::exception &x) {
            writeMessage('e', x.what());
        } catch (...) {
            writeMessage('e', "unknown exception");
        }
        int nimp = importer.GetPropertyInteger("importerIndex", -1);
        writeMessagef('F', "%d %d %d %d", nfile, nimp, (int)readable, (int)emptyscene);
#else
        for (int nimp = importeridx; nimp < importers; ++ nimp) {
            writeMessagef('s', "%d %d", nfile, nimp);
            bool readable = false;
            bool emptyscene = false;
            try {
                Assimp::Importer importer;
                Assimp::BaseImporter *loader = importer.GetImporter(nimp);
                if (!loader)
                    throw runtime_error("no loader");
#  if WHICH_TEST == TEST_CANREAD
                readable = loader->CanRead(testfiles[nfile].string(), &io, false);
#  elif WHICH_TEST == TEST_CANREAD_CHECKSIG
                readable = loader->CanRead(testfiles[nfile].string(), &io, true);
#  elif WHICH_TEST == TEST_READ
                aiScene *scene = loader->ReadFile(&importer, testfiles[nfile].string(), &io);
                readable = (scene != nullptr);
                emptyscene = (scene && !scene->HasMeshes());
                delete scene;
#  endif
            } catch (const std::exception &x) {
                writeMessage('e', x.what());
            } catch (...) {
                writeMessage('e', "unknown exception");
            }
            writeMessagef('f', "%d %d %d %d", nfile, nimp, (int)readable, (int)emptyscene);
        }
        importeridx = 0;
#endif
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
    bool emptyscene;
    bool haderror;
    bool crashed;
    string message;
    Result () : tested(false), canread(false), emptyscene(false), haderror(false), crashed(false) { }
};

struct ModelResult {
    path modelfile;
    vector<Result> results;
    set<int> importersForExtension;
    optional<int> primaryImporter;
#if WHICH_TEST == TEST_IMPORT
    optional<int> usedImporter;
    string usedMessage;
    bool usedEmptyScene;
    bool usedReadable;
#endif
    ModelResult (const path &modelfile, int importers) :  modelfile(modelfile), results(importers) {
#if WHICH_TEST == TEST_IMPORT
        usedEmptyScene = false;
        usedReadable = false;
#endif
        queryImporters();
    }
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

static QDomElement addChildFromFile (QDomNode addto, const QString &tag, const QString &filename) {

    QFile f(filename);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        throw test_error(filename, f.errorString());
    return addChild(addto, tag, QString::fromLatin1(f.readAll()));

}

static QDomElement htmlhack (QDomElement node) {

    node.appendChild(node.ownerDocument().createTextNode(""));
    return node;

}

template <typename Value>
static QDomElement withAttribute (QDomElement e, QString k, Value v) {
    e.setAttribute(k, v);
    return e;
}

#if PDF_OUTPUT
static void writePdfReport (const path &reportfile, const QDomDocument &html) {

    printf("writing %s (pdf)...\n", reportfile.string().c_str());

    QWebEnginePage renderer;
    bool complete = false;

    QObject::connect(&renderer, &QWebEnginePage::loadFinished, [&] (bool ok) {
        always_assert(ok);
        printf("    [pdf] html rendered, generating pdf...\n");
        QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF());
        renderer.printToPdf(QString::fromStdString(reportfile.string()), layout);
    });

    QObject::connect(&renderer, &QWebEnginePage::pdfPrintingFinished, [&] (QString, bool ok) {
        if (!ok)
            printf("    [pdf] an error occurred.\n");
        else
            printf("    [pdf] pdf written ok.\n");
        complete = true;
    });

    printf("    [pdf] rendering html...\n");
    renderer.setHtml(html.toString(-1));

    while (!complete) {
        QApplication::processEvents(QEventLoop::AllEvents | QEventLoop::WaitForMoreEvents, 100);
    }

}
#endif


#if EMBED_FONT
static QString embeddedFontCSS (const QString &fontfile, const QString &family) {

    QMimeType mimetype = QMimeDatabase().mimeTypeForFile(fontfile);

    QString data;
    if (fontfile.startsWith(":")) {
        data = QString::fromLatin1(QResource(fontfile).uncompressedData().toBase64());
    } else {
        QFile f(fontfile);
        if (!f.open(QFile::ReadOnly))
            throw test_error(fontfile, f.errorString());
        data = QString::fromLatin1(f.readAll().toBase64());
    }

    QString datauri = "data:" + mimetype.name() + ";base64," + data;

    /*
    QString format;
    if (mimetype.name() == "font/ttf")
        format = "truetype";
    else if (mimetype.name() == "font/otf")
        format = "opentype";
    else if (mimetype.name() == "font/woff")
        format = "woff";
    else if (mimetype.name() == "font/woff2")
        format = "woff2";
    else if (mimetype.preferredSuffix() == "svg")
        format = "svg";
    else if (mimetype.preferredSuffix() == "eot")
        format = "embedded-opentype";
    if (format != "")
        format = "format(\"" + format + "\")";
    */

    return "@font-face {\n"
           "  font-family: \"" + family + "\";\n"
           "  font-weight: 300 700;\n"
           "  font-stretch: semi-condensed;\n"
           "  src: url(\"" + datauri + "\");\n"
           "}";

}
#endif


static void writeHtmlReport (const path &reportfile, const vector<ModelResultPtr> &results, int importers) {

    printf("writing %s (html)...\n", reportfile.string().c_str());

    Assimp::Importer imp;
    QList<QDomElement> removeForPdf; 

#if WHICH_TEST == TEST_CANREAD
    static const char PAGE_TITLE[] = "CanRead(false) Report";
#elif WHICH_TEST == TEST_CANREAD_CHECKSIG
    static const char PAGE_TITLE[] = "CanRead(true) Report";
#elif WHICH_TEST == TEST_READ
    static const char PAGE_TITLE[] = "BaseImporter::Read Report";
#elif WHICH_TEST == TEST_IMPORT
#  if VALIDATE_SCENES
    static const char PAGE_TITLE[] = "Importer::Read+Validate Report";
#  else
    static const char PAGE_TITLE[] = "Importer::Read Report";
#  endif
#endif

    QDomDocument doc("html");
    QDomElement html = addChild(doc, "html");
    html.setAttribute("lang", "en");
    QDomElement head = addChild(html, "head");
    addChild(head, "title", PAGE_TITLE);
    QDomElement body = addChild(html, "body");
    QDomElement main = addChild(body, "main");

#if FLOATING_HEADER
    QDomElement ftiemeta = addChild(head, "meta");
    ftiemeta.setAttribute("http-equiv", "X-UA-Compatible");
    ftiemeta.setAttribute("content", "IE=10; IE=9; IE=8; IE=7; IE=EDGE");
    removeForPdf += ftiemeta;
    removeForPdf += withAttribute(htmlhack(addChild(head, "script")), "src", "https://code.jquery.com/jquery-3.6.0.min.js");
    removeForPdf += withAttribute(htmlhack(addChild(head, "script")), "src", "https://code.jquery.com/ui/1.12.1/jquery-ui.min.js");
    removeForPdf += withAttribute(htmlhack(addChild(head, "script")), "src", "https://cdnjs.cloudflare.com/ajax/libs/floatthead/2.2.1/jquery.floatThead.min.js");
    removeForPdf += addChildFromFile(head, "script", ":/js/report");
#endif

#if EMBED_FONT
    addChild(head, "style", embeddedFontCSS(EMBED_FONT_FILE, "ReportFont")).setAttribute("type", "text/css");
    addChild(head, "style", ":root{font-family:'" NOEMBED_FONT_FAMILY "','ReportFont',sans-serif;}").setAttribute("type", "text/css");
#else
    addChild(head, "style", ":root{font-family:'" NOEMBED_FONT_FAMILY "',sans-serif;}").setAttribute("type", "text/css");
#endif

#if 0
    QDomElement csslink = addChild(head, "link");
    csslink.setAttribute("rel", "stylesheet");
    csslink.setAttribute("href", "report.css");
#else
    addChildFromFile(head, "style", ":/css/report").setAttribute("type", "text/css");
#endif

    QDomElement table = addChild(addChild(main, "article"), "table");
    table.setAttribute("id", "report");
    QDomElement thead = addChild(table, "thead");
    QDomElement tbody = addChild(table, "tbody");
    QDomElement header = addChild(thead, "tr");

    addChild(header, "th", "Extn").setAttribute("class", "extn");
    for (int k = 0; k < importers; ++ k) {
        QString label = imp.GetImporterInfo(k)->mFileExtensions;
        addChild(addChild(header, "th"), "div", label).setAttribute("title", label);
    }
    addChild(header, "th", "Model File").setAttribute("class", "filename");

    for (ModelResultPtr result : results) {
        QDomElement row = addChild(tbody, "tr");
#if WHICH_TEST == TEST_IMPORT
        if (!result->usedImporter.has_value()) {
            row.setAttribute("class", "noimp");
            if (result->usedEmptyScene)
                row.setAttribute("class", row.attribute("class") + " u_emptyscene");
            if (result->usedReadable)
                row.setAttribute("class", row.attribute("class") + " u_canread");
            if (!result->usedMessage.empty())
                row.setAttribute("title", QString::fromStdString(result->usedMessage));
        }
#endif
        addChild(row, "td", result->modelfile.extension().string()).setAttribute("class", "extn");
        for (int k = 0; k < importers; ++ k) {
            QStringList classes;
            Result res = (k < (int)result->results.size() ? result->results[k] : Result());
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
            if (res.emptyscene)
                classes.append("emptyscene");
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

    {
        QFile out(reportfile.string().c_str());
        if (!out.open(QFile::WriteOnly | QFile::Text))
            throw runtime_error(out.errorString().toStdString());
        else {
            QTextStream outs(&out);
            doc.save(outs, 1);
            out.close();
        }
    }

#if PDF_OUTPUT
    for (QDomElement &e : removeForPdf)
        e.parentNode().removeChild(e);
    path pdffile = reportfile;
    writePdfReport(pdffile.replace_extension("pdf"), doc);
#else
    Q_UNUSED(removeForPdf);
#endif

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

    if ((int)str.length() <= maxwidth)
        return str;
    else
        return "..." + str.substr(str.length() - (maxwidth - 3));

}
#endif


static void setResult (vector<ModelResultPtr> &results, int fileidx, int importeridx,
                       bool readable, bool emptyscene, bool haderror, bool crashed,
                       const string &message)
{

    if (fileidx < 0 || fileidx >= (int)results.size())
        throw runtime_error("setResult: bad file index");
    ModelResultPtr modelresult = results[fileidx];

#if WHICH_TEST == TEST_IMPORT
    modelresult->usedMessage = message;
    modelresult->usedEmptyScene = emptyscene;
    modelresult->usedReadable = readable;
    if (importeridx != -1) {
        modelresult->usedImporter = importeridx;
#endif
        if (importeridx < 0 || importeridx >= (int)modelresult->results.size())
            throw runtime_error("setResult: bad importer index");
        Result &result = modelresult->results[importeridx];

        if (result.tested)
            throw runtime_error("setResult: test already performed");

        result.tested = true;
        result.canread = readable;
        result.emptyscene = emptyscene;
        result.haderror = haderror;
        result.crashed = crashed;
        result.message = message;
#if WHICH_TEST == TEST_IMPORT
    } else {
        modelresult->usedImporter.reset();
    }
#endif

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


#ifdef _WIN32
extern "C" void __stdcall OutputDebugStringA (const char *);
#endif

static int runServer (const vector<path> &testfiles, const path &myself, const path &listfile, const path &basedir) {

    int fileidx = 0, importeridx = 0;
    int importers = (int)aiGetImportFormatCount(), lastretcode = -1;

    vector<ModelResultPtr> results;
    for (path modelfile : testfiles)
        results.push_back(ModelResultPtr(new ModelResult(modelfile, importers)));

    while (fileidx < (int)testfiles.size() && (WHICH_TEST == TEST_IMPORT || importeridx < importers)) {

        string command = makeCommand(myself, fileidx, importeridx, listfile, basedir);
        FILE *p = popen(command.c_str(), "rt");
        if (!p)
            throw runtime_error(string("popen: ") + strerror(errno));

        char buf[1000], *message, type, *data;
        int mfileidx, mimporteridx, mreadable, memptyscene, curfileidx = -1, curimporteridx = -1;
        string curmessage;
        bool intest = false, haderror = false;
        while (fgets(buf, sizeof(buf), p)) {
            message = trim(buf);
            if (!*message)
                continue;
            //
            if (strncmp(message, "!@#$ ", 5)) {
#ifdef _WIN32
                OutputDebugStringA(message);
#endif
                continue;
            } else {
                message += 5;
            }
            type = message[0];
            data = (message[0] && message[1]) ? (message + 2) : nullptr;
            //
#if WHICH_TEST == TEST_IMPORT
            if (type == 'E') type = 'e'; // for now, until implemented better
            if (type == 'S' && !intest && data && sscanf(data, "%d", &mfileidx) == 1) {
                curfileidx = mfileidx;
                curimporteridx = -1;
                curmessage = "";
                intest = true;
                haderror = false;
                mreadable = false;
                memptyscene = false;
                if (curfileidx < 0 || curfileidx >= (int)testfiles.size())
                    throw runtime_error("bad S index received");
            } else if (type == 'F' && intest && data && sscanf(data, "%d %d %d %d", &mfileidx, &mimporteridx, &mreadable, &memptyscene) == 4) {
                if (mfileidx != curfileidx)
                    throw runtime_error("bad F index received");
                curimporteridx = mimporteridx;
                setResult(results, curfileidx, curimporteridx, mreadable, memptyscene, haderror, false, curmessage);
                intest = false;
#else
            if (type == 's' && !intest && data && sscanf(data, "%d %d", &mfileidx, &mimporteridx) == 2) {
                curfileidx = mfileidx;
                curimporteridx = mimporteridx;
                curmessage = "";
                intest = true;
                haderror = false;
                mreadable = false;
                memptyscene = false;
                if (curfileidx < 0 || curfileidx >= (int)testfiles.size() || curimporteridx < 0 || curimporteridx >= importers)
                    throw runtime_error("bad s index received");
            } else if (type == 'f' && intest && data && sscanf(data, "%d %d %d %d", &mfileidx, &mimporteridx, &mreadable, &memptyscene) == 4) {
                if (mfileidx != curfileidx || mimporteridx != curimporteridx)
                    throw runtime_error("bad f index received");
                setResult(results, curfileidx, curimporteridx, mreadable, memptyscene, haderror, false, curmessage);
                intest = false;
#endif
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
            setResult(results, curfileidx, curimporteridx, false, false, true, true, curmessage);

#if WHICH_TEST == TEST_IMPORT
        fileidx = curfileidx + 1;
#else
        importeridx = curimporteridx + 1;
        if (importeridx >= importers) {
            fileidx = curfileidx + 1;
            importeridx = 0;
        } else {
            fileidx = curfileidx;
        }
#endif

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


#if FILTER_COFF_OBJ
static bool objLooksLikeCOFF (const path &thefile) {

    static const uint8_t MX86[2] = { 0x4C, 0x01 };
    static const uint8_t MX64[2] = { 0x64, 0x86 };
    static const uint8_t MITA[2] = { 0x00, 0x02 };

    uint8_t machine[2];
    bool readok = false;

    FILE *f = fopen(thefile.string().c_str(), "rb");
    if (f) {
        readok = (fread(machine, 2, 1, f) == 1);
        fclose(f);
    }

    return (readok && (!memcmp(machine, MX86, 2)
                       || !memcmp(machine, MX64, 2)
                       || !memcmp(machine, MITA, 2)));

}
#endif


int main (int argc, char **argv) {

#if HTML_OUTPUT
#  if PDF_OUTPUT
    QApplication app(argc, argv);
#  else
    QCoreApplication app(argc, argv);
#  endif
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
        if (*(trimmed = trim(line))) {
            path modelfile = filesystem::absolute(basedir / trimmed);
#if FILTER_COFF_OBJ
            if (modelfile.extension().string() == ".obj" && objLooksLikeCOFF(modelfile)) {
                if (!runner)
                    printf("skipping coff file: %s\n", filesystem::relative(modelfile).string().c_str());
                continue;
            }
#endif
            testfiles.push_back(modelfile);
        }
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
