// warning: this code is a mess.
// note to self: you'll never remember how this works.

// Qt dependency will be removed if both HTML_OUTPUT and XML_OUTPUT are 0.

#ifndef HTML_OUTPUT
#  define HTML_OUTPUT 1 // zero disables
#endif

#ifndef XML_OUTPUT
#  define XML_OUTPUT 1 // zero disables
#endif

#ifndef PDF_OUTPUT
#  define PDF_OUTPUT 0 // zero disables; requires HTML_OUTPUT
#endif

#if HTML_OUTPUT || XML_OUTPUT
#  include <QDomDocument>
#  include <QDomNode>
#  include <QDomElement>
#  include <QFile>
#  include <QTextStream>
#  include <QMimeDatabase>
#  include <QResource>
#  include <QDateTime>
#  include <QProcess>
#  if PDF_OUTPUT && HTML_OUTPUT
#    include <QWebEnginePage>
#    include <QApplication>
#    include <QDesktopServices>
#    include <QUrl>
#    define GUI_SUPPORTED 1
#  else
#    include <QCoreApplication>
#    define GUI_SUPPORTED 0
#  endif
#  define QT_AVAILABLE 1
#else
#  define QT_AVAILABLE 0
#endif
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <io.h>
#endif
#include <cstdio>
#include <list>
#include <map>
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
#include <assimp/DefaultLogger.hpp>

#ifdef _WIN32
#  define popen  _popen
#  define pclose _pclose
#  define dup2   _dup2
#endif

#if QT_AVAILABLE
#  define ENABLE_CASEGEN 1
#  define USE_QPROCESS   1
#endif

#define EXIT_STD_EXCEPTION    80
#define EXIT_OTHER_EXCEPTION  81
#define EXIT_TEST_ERROR       82
#define EXIT_TEST_QPCRASHED   83  // only if USE_QPROCESS

#define TEST_CANREAD          0
#define TEST_CANREAD_CHECKSIG 1
#define TEST_READ             2
#define TEST_IMPORT           3

#define WHICH_TEST            TEST_READ
#define VALIDATE_SCENES       1   // only relevant for TEST_IMPORT
#define VERBOSE_PROGRESS      1
#define DUMP_LOGS_FROM_ASS    0
#define FILTER_COFF_OBJ       1
#define STRIP_FILE_EXTNS      0
#define MOVE_FILES_AWAY       0

// these are ignored if !HTML_OUTPUT
#define FLOATING_HEADER       1
#define EMBED_FONT            1
#define EMBED_FONT_FILE       ":/font/report"  // if EMBED_FONT
#define NOEMBED_FONT_FAMILY   "Bahnschrift Condensed"  // used whether embed_font or not.
#define LIST_MESSAGES         0
#define OPEN_HTML_AT_END      1

using namespace std;
using path = filesystem::path;


// for debug *and* release configurations
#define always_assert(expr) do { if (!(expr)) { \
      fprintf(stderr, "always_assert(%s:%d): %s\n", __FILE__, __LINE__, #expr); \
      abort(); \
    } } while (0)


// i know c++ has chrono stuff but it makes my head explode.
// todo: use gettimeofday on linux.
struct Timer {
    Timer ();
    void start ();
    double seconds () const;
private:
#ifdef _WIN32
    LARGE_INTEGER freq;
    LARGE_INTEGER t0;
#endif
};

#ifdef _WIN32
Timer::Timer () {
    QueryPerformanceFrequency(&freq);
    start();
}

void Timer::start () {
    QueryPerformanceCounter(&t0);
}

double Timer::seconds () const {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)(now.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
}
#endif


static char * trim (char *str) {

    if (str) {
        while (*str && isspace((unsigned char)*str))
            ++ str;
        for (int n = (int)strlen(str) - 1; n >= 0 && isspace((unsigned char)str[n]); -- n)
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


static int runClient (const vector<path> &testfiles, const path &myself, int fileidx, int importeridx) {

#if WHICH_TEST != TEST_IMPORT
    Assimp::DefaultIOSystem io;
    int importers = (int)aiGetImportFormatCount();
#else
    (void)importeridx;
#endif

    for (int nfile = fileidx; nfile < (int)testfiles.size(); ++ nfile) {
        path testfile = testfiles[nfile];
#if STRIP_FILE_EXTNS
        path strippedfile = testfile.parent_path() / "~canread_test_model";
        if (filesystem::exists(strippedfile) && is_directory(strippedfile))
            throw test_error("please clean up " + strippedfile.string());
        try {
            filesystem::remove(strippedfile);
            filesystem::copy_file(testfile, strippedfile, filesystem::copy_options::overwrite_existing);
            testfile = strippedfile;
        } catch (const exception &x) {
            //throw test_error(string(x.what()));
            // oajdsflkjakjafdakj;fdlkajl;zz
            //writeMessagef('e', x.what());
            printf("[warn:strip] %s\n", x.what());
            continue;
        }
#endif
#if MOVE_FILES_AWAY
        path movedfile = filesystem::temp_directory_path() / testfile.filename();
        if (filesystem::exists(movedfile) && is_directory(movedfile))
            throw test_error("please clean up " + movedfile.string());
        try {
            filesystem::remove(movedfile);
            filesystem::copy_file(testfile, movedfile, filesystem::copy_options::overwrite_existing);
            testfile = movedfile;
        } catch (const exception &x) {
            printf("[warn:move] %s\n", x.what());
            continue;
        }
#endif
        //printf("[debug] %s\n", testfile.string().c_str());
#if WHICH_TEST == TEST_IMPORT
        writeMessagef('S', "%d", nfile);
        bool readable = false;
        bool emptyscene = false;
        Assimp::Importer importer;
        try {
#  if VALIDATE_SCENES
            readable = (importer.ReadFile(testfile.string(), aiProcess_ValidateDataStructure) != nullptr);
#  else
            readable = (importer.ReadFile(testfile.string(), 0) != nullptr);
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
                readable = loader->CanRead(testfile.string(), &io, false);
#  elif WHICH_TEST == TEST_CANREAD_CHECKSIG
                readable = loader->CanRead(testfile.string(), &io, true);
#  elif WHICH_TEST == TEST_READ
                aiScene *scene = loader->ReadFile(&importer, testfile.string(), &io);
                if (!scene) {
                    writeMessagef('e', "[ReadFile] %s", loader->GetErrorText().c_str());
                    // what a mess...
                    //printf("[ReadFile] %s\n", loader->GetErrorText().c_str());
                }
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
#if MOVE_FILES_AWAY
        filesystem::remove(movedfile);
#endif
#if STRIP_FILE_EXTNS
        filesystem::remove(strippedfile);
#endif
        if (filesystem::exists(myself.parent_path() / "STOP"))
            break;
    }

    return EXIT_SUCCESS;

}


#if USE_QPROCESS
static QStringList makeCommand (const path &myself, int fileidx, int importeridx, const path &listfile, const path &basedir) {
    QStringList c;
    c << filesystem::relative(myself).string().c_str()
      << "--go"
      << QString("%1").arg(fileidx)
      << QString("%1").arg(importeridx)
      << listfile.string().c_str()
      << basedir.string().c_str();
    return c;
}
#else
static string makeCommand (const path &myself, int fileidx, int importeridx, const path &listfile, const path &basedir) {

    return (stringstream()
            << filesystem::relative(myself).string() << " --go " << fileidx << " " << importeridx
            << " \"" << listfile.string() << "\" \"" << basedir.string() << "\"").str();

}
#endif


// 'using enum' is c++20, can't count on it with msvc
namespace NSResultFlags {
    enum ResultFlags : unsigned {
        None       = 0     ,
        Tested     = 1 << 0,
        CanRead    = 1 << 1,
        EmptyScene = 1 << 2,
        HadError   = 1 << 3,
        Crashed    = 1 << 4,
        HasMessage = 1 << 5,
        MiscOutput = 1 << 6,
        BadChars   = 1 << 7,
        Slow       = 1 << 8
    };
    constexpr ResultFlags operator | (ResultFlags a, ResultFlags b) {
        return (ResultFlags)((unsigned)a | (unsigned)b);
    }
    ResultFlags & operator |= (ResultFlags &a, ResultFlags b) {
        return (a = a | b);
    }
    static const constexpr ResultFlags DefaultMatchFlags = CanRead|EmptyScene|HadError|Crashed|HasMessage|MiscOutput|BadChars;
}


struct Result {
    bool tested;
    bool canread;
    bool emptyscene;
    bool haderror;
    bool crashed;
    int crashedret;
    string message;
    vector<string> miscmessages;
    bool miscoutput;
    bool badchars;
    double seconds;
    Result () : tested(false), canread(false), emptyscene(false), haderror(false), crashed(false), crashedret(0), miscoutput(false), badchars(false), seconds(0) { }
    // utilities for casegen
    using ResultFlags = NSResultFlags::ResultFlags;
    static const constexpr ResultFlags DefaultMatchFlags = NSResultFlags::DefaultMatchFlags;
    ResultFlags flags () const {
        using namespace NSResultFlags;
        ResultFlags f = None;
        if (tested) f |= Tested;
        if (canread) f |= CanRead;
        if (emptyscene) f |= EmptyScene;
        if (haderror) f |= HadError;
        if (crashed) f |= Crashed;
        if (!message.empty()) f |= HasMessage;
        if (miscoutput) f |= MiscOutput;
        if (badchars) f |= BadChars;
        if (seconds >= 5) f |= Slow;
        return f;
    }
    bool matches (ResultFlags checkflags, ResultFlags flagset = DefaultMatchFlags) const {
        return (flags() & flagset) == checkflags;
    }
};

struct ModelResult {
    path modelfile;
    vector<Result> results;
    set<int> importersForExtension;
    optional<int> primaryImporter;
#if WHICH_TEST == TEST_IMPORT
    optional<int> usedImporter;
    Result usedResult; // even if !usedImporter.hasValue
#endif
    ModelResult (const path &modelfile, int importers, bool qi = true) :
        modelfile(modelfile), results(importers) { if (qi) queryImporters(); }
    // for whatever
    optional<int> cacheKey;
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


#if HTML_OUTPUT || XML_OUTPUT

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

#endif

#if HTML_OUTPUT

static QDomElement addChildFromFile (QDomNode addto, const QString &tag, const QString &filename) {

    QFile f(filename);
    if (!f.open(QFile::ReadOnly | QFile::Text))
        throw test_error(filename, f.errorString());
    return addChild(addto, tag, QString::fromUtf8(f.readAll()));

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
        //always_assert(ok);
        if (!ok) {
            printf("    [pdf] an error occurred while rendering the html.\n");
            complete = true;
        } else {
            printf("    [pdf] html rendered, generating pdf...\n");
            QPageLayout layout(QPageSize(QPageSize::A4), QPageLayout::Landscape, QMarginsF());
            renderer.printToPdf(QString::fromStdString(reportfile.string()), layout);
        }
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


template <typename Container>
static string join (const Container &str_, const string &delim, bool removedupes = true) {
#if 1
    Container str;
    if (removedupes) {
        set<string> seen;
        for (string s : str_) {
            if (seen.find(s) == seen.end()) {
                seen.insert(s);
                str.push_back(s);
            }
        }
    } else {
        str = str_;
    }
#else
    const Container &str = str_;
#endif
    //
    string joined;
    auto iter = str.begin();
    if (iter != str.end())
        joined = *(iter ++);
    while (iter != str.end()) {
        joined += delim;
        joined += *(iter ++);
    }
    return joined;
}

static QString htmlPreEscape (const StringHack &str_) {

    QString str = str_; //.s.toHtmlEscaped();
    for (QChar &ch : str) {
        if (ch != '\r' && ch != '\n' && !ch.isPrint())
            ch = '?';
    }
    return str;

}


#if LIST_MESSAGES
template <typename V> class OrderedSet {
private:
    using set_iter = typename set<V>::iterator;
    using list_iter = typename list<set_iter>::iterator;
public:
    struct iterator {
    public:
        const V & operator * () { return **li_; }
        const V * operator -> () { return (*li_).operator -> (); }
        bool operator == (const iterator &rhs) const { return li_ == rhs.li_; }
        bool operator != (const iterator &rhs) const { return li_ != rhs.li_; }
        iterator & operator ++ () { ++ li_; return *this; }
        iterator operator ++ (int) { return iterator(li_ ++); }
    private:
        iterator (list_iter li) : li_(li) { }
        list_iter li_;
        friend class OrderedSet<V>;
    };
    bool insert (const V &v) {
        auto i = set_.insert(v);
        if (i.second) list_.push_back(i.first);
        return i.second;
    }
    bool insert_if (const V &v, bool cond) { return cond ? insert(v) : false; }
    iterator begin () { return iterator(list_.begin()); }
    iterator end () { return iterator(list_.end()); }
    size_t size () const { return list_.size(); }
private:
    set<V> set_;
    list<set_iter> list_;
};


static bool shouldIgnore (const string &msg) {

    if (msg.empty())
        return true;
    if (msg.find("e [ReadFile]") == 0)
        return true;
    //if (msg.find("[ReadFile]") != string::npos)
    //    return msg.find("C:\\") != string::npos || msg.find("c:\\") != string::npos;
    if (msg.find("[ReadFile]") == 0)
        return true;

    return false;

}

#endif


static void writeHtmlReport (const path &reportfile, const vector<ModelResultPtr> &results, int importers) {

    printf("writing %s (html)...\n", reportfile.string().c_str());

    Assimp::Importer imp;
    QList<QDomElement> removeForPdf; 

#if WHICH_TEST == TEST_CANREAD
    static const char PAGE_TITLE[] = "CanRead(false) Report";
    static const char TEST_CLASSES[] = "test-canread";
#elif WHICH_TEST == TEST_CANREAD_CHECKSIG
    static const char PAGE_TITLE[] = "CanRead(true) Report";
    static const char TEST_CLASSES[] = "test-canread test-checksig";
#elif WHICH_TEST == TEST_READ
    static const char PAGE_TITLE[] = "BaseImporter::Read Report";
    static const char TEST_CLASSES[] = "test-read";
#elif WHICH_TEST == TEST_IMPORT
#  if VALIDATE_SCENES
    static const char PAGE_TITLE[] = "Importer::Read+Validate Report";
    static const char TEST_CLASSES[] = "test-import test-validate";
#  else
    static const char PAGE_TITLE[] = "Importer::Read Report";
    static const char TEST_CLASSES[] = "test-import";
#  endif
#endif

#if LIST_MESSAGES
    OrderedSet<string> messagelist;
    int totalmessages = 0;
    for (ModelResultPtr m : results) {
        for (const Result &r : m->results) {
            messagelist.insert_if(r.message, !shouldIgnore(r.message));
            for (const string &s : r.miscmessages)
                messagelist.insert_if(s, !shouldIgnore(s));
            totalmessages += (int)r.miscmessages.size();
        }
    }
    printf("[html] %d unique messages, %d total\n", (int)messagelist.size(), totalmessages);
#endif

    QDomDocument doc("html");
    QDomElement html = addChild(doc, "html");
    html.setAttribute("lang", "en");
    QDomElement head = addChild(html, "head");
    addChild(head, "meta").setAttribute("charset", "utf-8");
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

    QDomElement article = addChild(main, "article");
    article.setAttribute("class", TEST_CLASSES);
    QDomElement table = addChild(article, "table");
    table.setAttribute("id", "report");
    QDomElement thead = addChild(table, "thead");
    QDomElement tbody = addChild(table, "tbody");
    QDomElement header = addChild(thead, "tr");

#if LIST_MESSAGES
    {
        QDomElement div = addChild(article, "div");
        div.setAttribute("id", "messages");
        QDomElement mlist = addChild(div, "ul");
        for (string m : messagelist)
            addChild(mlist, "li", htmlPreEscape(m));
    }
#endif

    addChild(header, "th", "Extn").setAttribute("class", "extn");
    for (int k = 0; k < importers; ++ k) {
        QString label = imp.GetImporterInfo(k)->mFileExtensions;
        addChild(addChild(header, "th"), "div", label).setAttribute("title", htmlPreEscape(label + " - " + imp.GetImporterInfo(k)->mName));
    }
    addChild(header, "th", "Model File").setAttribute("class", "filename");

    for (ModelResultPtr result : results) {
        QDomElement row = addChild(tbody, "tr");
#if WHICH_TEST == TEST_IMPORT
        if (!result->usedImporter.has_value()) {
            row.setAttribute("class", "noimp");
            if (result->usedResult.emptyscene)
                row.setAttribute("class", row.attribute("class") + " u_emptyscene");
            if (result->usedResult.canread)
                row.setAttribute("class", row.attribute("class") + " u_canread");
            string tooltip = join(result->usedResult.miscmessages, "\n");
            if (!tooltip.empty())
                row.setAttribute("title", htmlPreEscape(tooltip));
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
            if (res.badchars)
                classes.append("badchars");
            if (result->importersForExtension.find(k) != result->importersForExtension.end())
                classes.append("imp");
            if (result->primaryImporter == k)
                classes.append("pimp");
            if (res.seconds > 10.0)
                classes.append("vslow");
            else if (res.seconds > 5.0)
                classes.append("slow");
            //
            QDomElement td = addChild(row, "td");
            string tooltip = join(res.miscmessages, "\n");
            if (!tooltip.empty())
                td.setAttribute("title", htmlPreEscape(tooltip));
            if (!res.message.empty())
                classes.append("message");
            if (res.miscoutput)
                classes.append("miscmessage");
            if (res.crashed)
                td.setAttribute("data-crashedret", res.crashedret);
            if (res.tested)
                td.setAttribute("data-time", res.seconds);
            //
            if (!classes.empty())
                td.setAttribute("class", classes.join(' '));
        }
        try {
            addChild(row, "td", filesystem::relative(result->modelfile).string()).setAttribute("class", "filename");
        } catch (const std::exception &ex) {
            auto e = addChild(row, "td", result->modelfile.filename().string());
            e.setAttribute("class", "filename rel-path-failed");
            e.setAttribute("title", htmlPreEscape(ex.what()));
        }
    }

    QString versionstr = QString().sprintf("assimp version: %d.%d.%d (%s @ %x)",
                                           aiGetVersionMajor(),
                                           aiGetVersionMinor(),
                                           aiGetVersionPatch(),
                                           aiGetBranchName(),
                                           aiGetVersionRevision());
    QDomElement footer = addChild(body, "footer");
    addChild(footer, "div", PAGE_TITLE);
    addChild(footer, "div", versionstr);
    addChild(footer, "div", QDateTime::currentDateTimeUtc().toString());

    {
        QFile out(reportfile.string().c_str());
        if (!out.open(QFile::WriteOnly | QFile::Text))
            throw runtime_error(out.errorString().toStdString());
        else {
            QTextStream outs(&out);
            outs.setCodec("utf-8");
            doc.save(outs, 1);
            out.close();
        }
    }

#if OPEN_HTML_AT_END
#  if GUI_SUPPORTED
    QDesktopServices::openUrl(QUrl::fromLocalFile(reportfile.string().c_str()));
#  else
    system(reportfile.string().c_str());
#  endif
#endif

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


#if XML_OUTPUT

static QDomElement addXmlConfig (QDomElement config, QString key, QVariant value) {
    QDomElement prop = addChild(config, "var");
    prop.setAttribute("name", key);
    prop.setAttribute("value", value.toString());
    return prop;
}


template <typename Container>
static QString setstr (const Container &values, char delim = ' ') {
    QStringList strs;
    for (auto v : values)
        strs.append(QString("%1").arg(v));
    return strs.join(delim);
}


#define XML_REDUCE_SIZE 1

/*
static QString xmlEscape (const StringHack &str_) {

    QString str = str_;
    str.replace();
    return str;

}
*/
#define xmlEscape(s) s
/*static QString xmlEscape (const StringHack &str_) {

    QString str = str_;
    for (QChar &ch : str) {
        if (!ch.isPrint())
            ch = '?';
    }
    return str;

}*/

static void addXmlResult (QDomElement addto, const Result &result, int importer) {

    if (!result.tested)
        return;

    QDomElement e = addChild(addto, "importer");
    e.setAttribute("id", importer);
    if (result.canread) e.setAttribute("canread", true);
    if (result.emptyscene) e.setAttribute("emptyscene", true);
    if (result.haderror) e.setAttribute("haderror", true);
    if (result.crashed) e.setAttribute("crashed", true);
    if (result.crashed) e.setAttribute("crashedret", QString().sprintf("0x%08x", result.crashedret));
    if (result.badchars) e.setAttribute("badchars", true);

    if (!result.message.empty())
        addChild(e, "message", xmlEscape(result.message));

    if (result.miscoutput || result.miscmessages.size() > 1) {
        QDomElement emm = addChild(e, "miscmessages");
        if (result.miscoutput) emm.setAttribute("std", true);
        for (const string &message : result.miscmessages)
            addChild(emm, "message", xmlEscape(message));
    }

#if XML_REDUCE_SIZE
    if (!e.hasChildNodes() && e.attributes().size() == 1 /* id */) {
        addto.removeChild(e);
        return;
    }
#endif

    e.setAttribute("time", result.seconds);

}


static void writeXmlReport (const path &reportfile, const vector<ModelResultPtr> &results, int importers,
                            const vector<path> &testfiles, const path &listfile, const path &basedir)
{

    printf("writing %s (xml)...\n", reportfile.string().c_str());

    QDomDocument doc;
    doc.appendChild(doc.createProcessingInstruction("xml", "version=\"1.1\" encoding=\"utf-8\""));

    QDomElement root = addChild(doc, "canread_test_report");
    root.setAttribute("version", 1);
    root.setAttribute("escapes", "html");

    QDomElement meta = addChild(root, "meta");
    meta.setAttribute("timestamp", QDateTime::currentMSecsSinceEpoch());
    meta.appendChild(doc.createComment(QDateTime::currentDateTime().toString()));

    QDomElement config = addChild(meta, "config");
    addXmlConfig(config, "WHICH_TEST", WHICH_TEST);
    addXmlConfig(config, "VALIDATE_SCENES", VALIDATE_SCENES);
    addXmlConfig(config, "DUMP_LOGS_FROM_ASS", DUMP_LOGS_FROM_ASS);
    addXmlConfig(config, "FILTER_COFF_OBJ", FILTER_COFF_OBJ);
    addXmlConfig(config, "STRIP_FILE_EXTNS", STRIP_FILE_EXTNS);
    addXmlConfig(config, "MOVE_FILES_AWAY", MOVE_FILES_AWAY);

    QDomElement assimp = addChild(meta, "assimp");
    addChild(assimp, "version", QString().sprintf("%d.%d.%d", aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionPatch()));
    addChild(assimp, "branch", aiGetBranchName());
    addChild(assimp, "commit", QString("%1").arg(aiGetVersionRevision(), 0, 16));
    assimp.appendChild(doc.createComment("flags value is base 10:"));
    addChild(assimp, "flags", QString("%1").arg(aiGetCompileFlags()));

    QDomElement imps = addChild(assimp, "importers");
    {
        Assimp::Importer ai;
        for (int k = 0; k < importers; ++ k) {
            auto info = ai.GetImporterInfo(k);
            QDomElement imp = addChild(imps, "importer", QString(info->mName).replace("\n", " "));
            imp.setAttribute("id", k);
            imp.setAttribute("exts", info->mFileExtensions);
        }
    }

    QDomElement files = addChild(root, "files");
    files.setAttribute("list", filesystem::relative(listfile, basedir).generic_string().c_str());
    files.setAttribute("base", filesystem::relative(basedir).generic_string().c_str());
    for (int k = 0; k < (int)testfiles.size(); ++ k) {
        try {
            addChild(files, "file", filesystem::relative(testfiles[k], basedir).generic_string())
                    .setAttribute("id", k);
        } catch (const std::exception &ex) {
            auto e = addChild(files, "file", testfiles[k].generic_string());
            e.setAttribute("id", k);
            e.setAttribute("rel-path-failed", ex.what());
        }
    }

    QDomElement eresults = addChild(root, "results");
    eresults.setAttribute("compressed", XML_REDUCE_SIZE);

    QDomElement defaultrefs = addChild(eresults, "defaults");
    defaultrefs.appendChild(doc.createComment("Really, this just serves as a reference for the Result fields, in case they change, since it wouldn't otherwise be apparent here."));
    QDomElement defaultref = addChild(defaultrefs, "default");
    defaultref.setAttribute("for", "model/importers/importer");
    addChild(defaultref, "attr", "0").setAttribute("name", "canread");
    addChild(defaultref, "attr", "0").setAttribute("name", "emptyscene");
    addChild(defaultref, "attr", "0").setAttribute("name", "haderror");
    addChild(defaultref, "attr", "0").setAttribute("name", "crashed");
    addChild(defaultref, "attr", "unset").setAttribute("name", "crashedret");
    addChild(defaultref, "attr", "0").setAttribute("name", "badchars");
    addChild(defaultref, "attr", "unset").setAttribute("name", "time");

    for (int k = 0; k < (int)results.size(); ++ k) {
        const ModelResultPtr &mr = results[k];
        QDomElement emr = addChild(eresults, "model");
        emr.setAttribute("file", k);
        QDomElement eimps = addChild(emr, "importers");
        eimps.setAttribute("all", setstr(mr->importersForExtension));
        if (mr->primaryImporter.has_value())
            eimps.setAttribute("default", mr->primaryImporter.value());
#if XML_REDUCE_SIZE
        {
            vector<int> tested, untested;
            for (int j = 0; j < (int)mr->results.size(); ++ j)
                (mr->results[j].tested ? tested : untested).push_back(j);
            if (tested.size() <= untested.size())
                addChild(eimps, "tested").setAttribute("ids", setstr(tested));
            else
                addChild(eimps, "untested").setAttribute("ids", setstr(untested));
        }
#endif
#if WHICH_TEST == TEST_IMPORT
        if (mr->usedImporter.has_value())
            eimps.setAttribute("actual", mr->usedImporter.value());
        else
            addXmlResult(eimps, mr->usedResult, -1);
#endif
        for (int j = 0; j < (int)mr->results.size(); ++ j)
            addXmlResult(eimps, mr->results[j], j);
    }

    {
        QFile out(reportfile.string().c_str());
        if (!out.open(QFile::WriteOnly | QFile::Text))
            throw runtime_error(out.errorString().toStdString());
        else {
            QTextStream outs(&out);
            outs.setCodec("utf-8");
            doc.save(outs, 1);
            out.close();
        }
    }

}

#endif // XML_OUTPUT


#if ENABLE_CASEGEN

static QDomElement xmlFirstChild (QDomElement elem, QString tag, const function<bool(const QDomElement &)> &pred) {
    for (QDomElement c = elem.firstChildElement(tag); !c.isNull(); c = c.nextSiblingElement(tag))
        if (pred(c))
            return c;
    return QDomElement();
}


static QDomElement xmlForEach (QDomElement elem, QString tag, const function<void(QDomElement)> &func) {
    for (QDomElement c = elem.firstChildElement(tag); !c.isNull(); c = c.nextSiblingElement(tag))
        func(c);
    return elem;
}


template <typename Container>
static bool consecutive (const Container &c) {
    for (int k = 0; k < (int)c.size(); ++ k)
        if (c.find(k) == c.end())
            return false;
    return true;
}


static int strhash (const string &s) {
    return (int)std::hash<string>()(s);
}


struct ByResultProfile {
private:
    static map<int,vector<int> > rpcache_;
    static bool checkmsgs_;
public:
    static void setCheckMessages (bool v) {
        checkmsgs_ = v;
        clearCache();
    }
    static void clearCache () {
        rpcache_.clear();
    }
    template <typename Container>
    static void debugrp (const Container &ms) {
        for (const ModelResultPtr m : ms) {
            printf("  rpcache: ");
            if (!m->cacheKey.has_value()) {
                printf("no cache key\n");
                continue;
            }
            printf("%4d: ", m->cacheKey.value());
            auto entry = rpcache_.find(m->cacheKey.value());
            if (entry == rpcache_.end()) {
                printf("not in cache\n");
                continue;
            }
            const vector<int> &rp = entry->second;
            for (int k = 0; k < rp.size(); k += 2)
                printf("%c", rp[k] ? 'U' : '.');
            printf("\n");
        }
    }
    static vector<int> makerp (const ModelResult &m) {
        if (m.cacheKey.has_value()) {
            auto cached = rpcache_.find(m.cacheKey.value());
            if (cached != rpcache_.end()) return cached->second;
        }
        vector<int> rp;
        using namespace NSResultFlags;
        map<int,bool> as_expected;
        // see if one of the expected importers *did* work
        optional<int> working;
        for (int i : m.importersForExtension) {
            if (m.results[i].matches(CanRead, DefaultMatchFlags | Slow)) {
                as_expected[i] = true;
                working = i;
                break;
            }
        }
        // if so, make sure any other expected importers gracefully failed,
        // otherwise; none of them are working as expected.
        if (working.has_value()) {
            for (int i : m.importersForExtension) {
                if (i != working.value()) {
                    bool ok = m.results[i].matches(HadError | HasMessage, DefaultMatchFlags | Slow);
                    as_expected[i] = ok;
                }
            }
        } else {
            for (int i : m.importersForExtension)
                as_expected[i] = false;
        }
        // and... now i've confused myself so let's just try random crap and hope it works.
        for (int k = 0; k < (int)m.results.size(); ++ k) {
            bool expected_result;
            if (as_expected.find(k) != as_expected.end()) {
                expected_result = as_expected[k];
            } else {
                // expected result is a fast graceful failure
                expected_result = m.results[k].matches(HadError | HasMessage, DefaultMatchFlags | Slow);
            }
            // now give it some hash-y thing
            if (expected_result) {
                rp.push_back(0);
                rp.push_back(0);
                if (checkmsgs_)
                    rp.push_back(0);
            } else {
                rp.push_back(m.results[k].flags());
                rp.push_back(m.results[k].crashed ? m.results[k].crashedret : 0);
                if (checkmsgs_)
                    rp.push_back(strhash(m.results[k].message));
            }
        }
        if (m.cacheKey.has_value())
            rpcache_[m.cacheKey.value()] = rp;
        return rp;
    }
    bool operator () (ModelResultPtr a, ModelResultPtr b) const {
        if (!a)
            return true;
        else if (a == b)
            return false;
        else {
            auto va = makerp(*a);
            auto vb = makerp(*b);
            return std::lexicographical_compare(va.begin(), va.end(), vb.begin(), vb.end());
        }
    }
};

map<int,vector<int> > ByResultProfile::rpcache_;
bool ByResultProfile::checkmsgs_ = false;


static bool allfast (const ModelResultPtr &m, double threshold = -1) {
    using namespace NSResultFlags;
    for (const Result &r : m->results) {
        if (threshold < 0) {
            if (r.flags() & Slow)
                return false;
        } else {
            if (r.seconds >= threshold)
                return false;
        }
    }
    return true;
}


static int casegenMain (int argc, char **argv) {

    always_assert(argc > 1);
    always_assert(!strcmp(argv[1], "--casegen"));

    if (argc != 4)
        return EXIT_FAILURE;

    path reportfile(argv[2]);
    path outdir = filesystem::absolute(argv[3]);

    printf("[casegen] loading %s...\n", reportfile.string().c_str());

    QDomDocument report;
    {
        QFile freport(reportfile.string().c_str());
        if (!freport.open(QFile::ReadOnly | QFile::Text))
            throw runtime_error(freport.errorString().toStdString());
        QString message;
        if (!report.setContent(&freport, &message))
            throw runtime_error(message.toStdString());
    }

    printf("[casegen] processing report...\n");

    QDomElement root = report.documentElement();
    if (root.tagName() != "canread_test_report")
        throw runtime_error("not a report file");
    else if (root.attribute("version") != "1")
        throw runtime_error("unsupported report version");
    if (root.attribute("escapes") != "html")
        printf("[warning] unexpected string escape flavor");

    QDomElement meta = root.firstChildElement("meta");
    QDomElement config = meta.firstChildElement("config");
    QDomElement config_WHICH_TEST = xmlFirstChild(config, "var", [](QDomElement e){ return e.attribute("name") == "WHICH_TEST"; });
    if (config_WHICH_TEST.attribute("value") != "2")
        throw runtime_error("report is not from a TEST_READ test");

    map<int,QString> importerNames;
    QDomElement assimp = meta.firstChildElement("assimp");
    QDomElement importers = assimp.firstChildElement("importers");
    xmlForEach(importers, "importer", [&importerNames] (QDomElement imp) {
        bool ok;
        importerNames[imp.attribute("id").toInt(&ok)] = imp.text().trimmed();
        if (!ok) throw runtime_error("missing importer id attribute");
    });
    if (importerNames.empty())
        throw runtime_error("missing importer metadata");
    if (!consecutive(importerNames))
        throw runtime_error("gaps in importer ids");
    int nimporters = (int)importerNames.size();
    printf("[casegen] %d importers found in report.\n", nimporters);

    map<int,path> filePaths;
    QDomElement files = root.firstChildElement("files");
    if (files.isNull())
        throw runtime_error("missing file list");
    path filebasedir = files.attribute("base", ".").toStdString();
    xmlForEach(files, "file", [&] (QDomElement fel) {
        bool ok;
        int id = fel.attribute("id").toInt(&ok);
        if (!ok) throw runtime_error("missing file id attribute");
        QString pstr = fel.text().trimmed();
        if (pstr == "") throw runtime_error("missing file name");
        path filepath = filesystem::absolute(filebasedir / pstr.toStdString());
        filePaths[id] = filepath;
    });
    if (!consecutive(filePaths))
        throw runtime_error("gaps in file ids");
    printf("[casegen] %d test files found in report.\n", (int)filePaths.size());

    list<ModelResultPtr> modelResults;
    QDomElement results = root.firstChildElement("results");
    bool compressed = (results.attribute("compressed").toInt() != 0);
    xmlForEach(results, "model", [&] (QDomElement model) {
        bool ok;
        int fid = model.attribute("file").toInt(&ok);
        if (!ok) throw runtime_error("missing model file id");
        path modelfile = filePaths[fid];
        if (modelfile.empty()) throw runtime_error("invalid model file id");
        if (!filesystem::exists(modelfile)) {
            printf("[warning] file does not exist: %s\n", modelfile.string().c_str());
            return;
        }
        ModelResultPtr mres(new ModelResult(modelfile, nimporters, true));
        mres->cacheKey = fid;
        modelResults.push_back(mres);
        QDomElement mimps = model.firstChildElement("importers");
        if (compressed) {
            QDomElement utcheck = mimps.firstChildElement("untested");
            if (utcheck.isNull() || utcheck.attribute("ids") != "")
                throw runtime_error("fixme: handle compression completely");
            for (Result &r : mres->results)
                r.tested = true;
        }
        xmlForEach(mimps, "importer", [&] (QDomElement imp) {
            int id = imp.attribute("id").toInt(&ok);
            if (!ok) throw runtime_error("missing result importer id");
            if (id < 0 || id >= nimporters) throw runtime_error("invalid result importer id");
            Result &r = mres->results[id];
            r.canread = imp.attribute("canread").toInt();
            r.emptyscene = imp.attribute("emptyscene").toInt();
            r.haderror = imp.attribute("haderror").toInt();
            r.crashed = imp.attribute("crashed").toInt();
            r.crashedret = imp.attribute("crashedret").toInt(nullptr, 0);
            r.message = imp.firstChildElement("message").text().trimmed().toStdString();
            QDomElement mmsgs = imp.firstChildElement("miscmessages");
            r.miscoutput = mmsgs.attribute("std").toInt();
            xmlForEach(mmsgs, "message", [&] (QDomElement mmsg) {
                r.miscmessages.push_back(mmsg.text().trimmed().toStdString());
            });
            if (r.miscmessages.empty() && r.message != "")
                r.miscmessages.push_back(r.message);
            r.badchars = imp.attribute("badchars").toInt();
            r.seconds = imp.attribute("time").toDouble();
        });
    });
    printf("[casegen] %d result sets loaded from report.\n", (int)modelResults.size());
    printf("[casegen] report loaded.\n");

    //--------------------

    ByResultProfile::setCheckMessages(false);
    set<ModelResultPtr,ByResultProfile> cases1;
    for (ModelResultPtr mres : modelResults)
        cases1.insert(mres);
    printf("[casegen] by profile: generated %d test cases\n", (int)cases1.size());

    {
        FILE *f = fopen("cases1.txt", "wt");
        for (const ModelResultPtr &mr : cases1) {
            try {
                fprintf(f, "%s\n", filesystem::relative(mr->modelfile).generic_string().c_str());
            } catch (...) {
                fprintf(f, "%s\n", mr->modelfile.generic_string().c_str());
            }
        }
        fclose(f);
        printf("[casesgen] wrote cases1.txt\n");
    }

    // --------------------

    ByResultProfile::setCheckMessages(true);
    set<ModelResultPtr,ByResultProfile> cases2;
    for (ModelResultPtr mres : modelResults)
        cases2.insert(mres);
    printf("[casegen] by profile + message: generated %d test cases\n", (int)cases2.size());

    {
        FILE *f = fopen("cases2.txt", "wt");
        for (const ModelResultPtr &mr : cases2) {
            try {
                fprintf(f, "%s\n", filesystem::relative(mr->modelfile).generic_string().c_str());
            } catch (...) {
                fprintf(f, "%s\n", mr->modelfile.generic_string().c_str());
            }
        }
        fclose(f);
        printf("[casesgen] wrote cases2.txt\n");
    }

    // --------------------

    ByResultProfile::setCheckMessages(true);
    set<ModelResultPtr,ByResultProfile> cases3;
    for (ModelResultPtr mres : modelResults)
        if (allfast(mres, 10))
            cases3.insert(mres);
    printf("[casegen] fast only, by profile + message: generated %d test cases\n", (int)cases3.size());

    {
        FILE *f = fopen("cases3.txt", "wt");
        for (const ModelResultPtr &mr : cases3) {
            try {
                fprintf(f, "%s\n", filesystem::relative(mr->modelfile).generic_string().c_str());
            } catch (...) {
                fprintf(f, "%s\n", mr->modelfile.generic_string().c_str());
            }
        }
        fclose(f);
        printf("[casesgen] wrote cases3.txt\n");
    }

    return EXIT_SUCCESS;

}


#endif // ENABLE_CASEGEN


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
        try {
            fprintf(f, "\"%s\"", filesystem::relative(result->modelfile).string().c_str());
        } catch (...) {
            fprintf(f, "\"%s\"", result->modelfile.filename().string().c_str());
        }
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


static bool safe_isprint (char c) {
    return isprint((unsigned char)c);
}


template <typename Container>
static bool hasUnprintableChars (const Container &strs) {

    for (auto str : strs)
        if (!all_of(str.begin(), str.end(), safe_isprint))
            return true;

    return false;

}


static void setResult (vector<ModelResultPtr> &results, int fileidx, int importeridx,
                       bool readable, bool emptyscene, bool haderror, bool crashed,
                       const string &message, const vector<string> &miscmessages,
                       bool miscoutput, int crashedRetCode, double seconds)
{

    if (fileidx < 0 || fileidx >= (int)results.size())
        throw runtime_error("setResult: bad file index");
    ModelResultPtr modelresult = results[fileidx];

    Result result;
    result.tested = true;
    result.canread = readable;
    result.emptyscene = emptyscene;
    result.haderror = haderror;
    result.crashed = crashed;
    result.crashedret = crashedRetCode;
    result.message = message;
    result.miscmessages = miscmessages;
    result.miscoutput = miscoutput;
    result.badchars = hasUnprintableChars(miscmessages);
    result.seconds = seconds;

    if (result.badchars)
        OutputDebugStringA("**** BAD CHARS! ****");

#if WHICH_TEST == TEST_IMPORT
    modelresult->usedResult = result;
    if (importeridx != -1) {
        modelresult->usedImporter = importeridx;
#endif
        if (importeridx < 0 || importeridx >= (int)modelresult->results.size())
            throw runtime_error("setResult: bad importer index");
        else if (modelresult->results[importeridx].tested)
            throw runtime_error("setResult: test already performed");
        else
            modelresult->results[importeridx] = result;
#if WHICH_TEST == TEST_IMPORT
    } else {
        modelresult->usedImporter.reset();
    }
#endif

#if VERBOSE_PROGRESS
    static int lastfileidx = -1;
    if (fileidx != lastfileidx) {
        printf("\n[%3d/%3d] %48s: ", fileidx + 1, (int)results.size(), rclip(/*filesystem::relative*/(modelresult->modelfile).string(), 48).c_str());
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

    Timer esttime;

    while (fileidx < (int)testfiles.size() && (WHICH_TEST == TEST_IMPORT || importeridx < importers)) {

#if USE_QPROCESS
        QStringList command = makeCommand(myself, fileidx, importeridx, listfile, basedir);
        always_assert(!command.empty());
        QProcess proc;
        proc.setReadChannel(QProcess::StandardOutput);
        proc.setProgram(command.takeFirst());
        proc.setArguments(command);
        proc.start(QIODevice::ReadOnly);
        if (!proc.waitForStarted())
            throw runtime_error(proc.errorString().toStdString());
#else
        string command = makeCommand(myself, fileidx, importeridx, listfile, basedir);
        FILE *p = popen(command.c_str(), "rt");
        if (!p)
            throw runtime_error(string("popen: ") + strerror(errno));
#endif

        char buf[1000], *message, type, *data;
        int mfileidx, mimporteridx, mreadable, memptyscene, curfileidx = -1, curimporteridx = -1;
        string curmessage;
        vector<string> miscmessages;
        bool intest = false, haderror = false, hadmisc = false;
#if USE_QPROCESS
        int readresult;
        while ((readresult = proc.readLine(buf, sizeof(buf))) >= 0) {
            if (readresult == 0) {
                if (!proc.waitForReadyRead(100)) {
                    static const char *OGspinner = "-\\|/";
                    static int anim = 0;
                    printf("%c\b", OGspinner[anim++]);
                    if (!OGspinner[anim]) anim = 0;
                    if (esttime.seconds() > 11) {
                        printf("#\b");
                        proc.kill();
                    }
                    fflush(stdout);
                }
                continue;
            }
#else
        while (fgets(buf, sizeof(buf), p)) {
#endif
            message = trim(buf);
            if (!*message)
                continue;
            //
            if (strncmp(message, "!@#$ ", 5)) {
#ifdef _WIN32
                OutputDebugStringA(message);
#endif
                miscmessages.push_back(message);
                hadmisc = true;
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
                miscmessages.clear();
                hadmisc = false;
                esttime.start();
                if (curfileidx < 0 || curfileidx >= (int)testfiles.size())
                    throw runtime_error("bad S index received");
            } else if (type == 'F' && intest && data && sscanf(data, "%d %d %d %d", &mfileidx, &mimporteridx, &mreadable, &memptyscene) == 4) {
                if (mfileidx != curfileidx)
                    throw runtime_error("bad F index received");
                curimporteridx = mimporteridx;
                setResult(results, curfileidx, curimporteridx, mreadable, memptyscene, haderror, false, curmessage, miscmessages, hadmisc, 0);
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
                miscmessages.clear();
                hadmisc = false;
                esttime.start();
                if (curfileidx < 0 || curfileidx >= (int)testfiles.size() || curimporteridx < 0 || curimporteridx >= importers)
                    throw runtime_error("bad s index received");
            } else if (type == 'f' && intest && data && sscanf(data, "%d %d %d %d", &mfileidx, &mimporteridx, &mreadable, &memptyscene) == 4) {
                if (mfileidx != curfileidx || mimporteridx != curimporteridx)
                    throw runtime_error("bad f index received");
                setResult(results, curfileidx, curimporteridx, mreadable, memptyscene, haderror, false, curmessage, miscmessages, hadmisc, 0, esttime.seconds());
                intest = false;
#endif
            } else if ((type == 'e' || type == 'x') && intest && data) {
                curmessage = data;
                miscmessages.push_back(message /* yes, including type */);
                haderror = true;
            } else {
                fprintf(stderr, "weird message: %s\n", message);
                throw runtime_error("weirdness");
            }
        }

#if USE_QPROCESS
        proc.waitForFinished();
        if (proc.exitStatus() == QProcess::NormalExit)
            lastretcode = proc.exitCode();
        else
            lastretcode = EXIT_TEST_QPCRASHED;
#else
        lastretcode = pclose(p);
#endif
        if (lastretcode == EXIT_TEST_ERROR)
            throw runtime_error("unrecoverable: " + curmessage);

        if (intest)
            setResult(results, curfileidx, curimporteridx, false, false, true, true, curmessage, miscmessages, hadmisc, lastretcode, esttime.seconds());

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

        if (filesystem::exists(myself.parent_path() / "STOP")) {
            printf("STOPPED\n");
            filesystem::remove(myself.parent_path() / "STOP");
            break;
        }

    }

#if VERBOSE_PROGRESS
    printf("\n");
#endif

    if (lastretcode != EXIT_SUCCESS)
        printf("warning: unexpected return code %d (0x%x)\n", lastretcode, lastretcode);

    writeReport(filesystem::absolute("report.csv"), results, importers);
#if XML_OUTPUT
    writeXmlReport(filesystem::absolute("report.xml"), results, importers, testfiles, listfile, basedir);
#endif
#if HTML_OUTPUT
    writeHtmlReport(filesystem::absolute("report.html"), results, importers); // also writes pdf
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

#if HTML_OUTPUT || XML_OUTPUT
#  if PDF_OUTPUT && HTML_OUTPUT
    QApplication app(argc, argv);
#  else
    QCoreApplication app(argc, argv);
#  endif
#endif

    // -------- command line -------------------------------------------------

    // canread_test listfile [ basedir ]
    // canread_test --go start_file_index start_importer_index listfile [ basedir ]
    // canread_test --casegen report.xml outdir

    bool runner = (argc > 1 && !strcmp(argv[1], "--go"));
#if ENABLE_CASEGEN
    bool casegen = (argc > 1 && !strcmp(argv[1], "--casegen"));
#endif
    int fileidx = -1, importeridx = -1;
    path listfile, basedir, myself = filesystem::absolute(argv[0]);

    if (runner && (argc == 5 || argc == 6)) {
        fileidx = atoi(argv[2]);
        importeridx = atoi(argv[3]);
        listfile = filesystem::absolute(argv[4]);
        basedir = filesystem::absolute(argc > 5 ? argv[5] : ".");
#if ENABLE_CASEGEN
    } else if (casegen) {
        try {
            return casegenMain(argc, argv);
        } catch (const std::exception &x) {
            printf("[error] %s\n", x.what());
            return EXIT_FAILURE;
        }
#endif
    } else if (!runner && (argc == 2 || argc == 3)) {
        listfile = filesystem::absolute(argv[1]);
        basedir = filesystem::absolute(argc > 2 ? argv[2] : ".");
    } else {
        fprintf(stderr, "\nUsage: %s <listfile> [ <basedir> ]\n\n", myself.filename().string().c_str());
        fprintf(stderr, "  listfile  File containing a list of 3D model files, one per line.\n");
        fprintf(stderr, "  basedir   Optional: Path that everything in <listfile> is relative to.\n\n");
        return EXIT_FAILURE;
    }

    // -------- output tweaks ------------------------------------------------

    if (runner) {
#if _WIN32
        if (!IsDebuggerPresent()) {
            _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
            _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDOUT);
            _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_DEBUG);
            _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDOUT);
        }
#endif
        dup2(1, 2); // stderr -> stdout, since we're just using popen()
    }

#if DUMP_LOGS_FROM_ASS
    Assimp::DefaultLogger::create("", Assimp::DefaultLogger::VERBOSE, aiDefaultLogStream_STDERR);
#endif

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

    filesystem::remove(myself.parent_path() / "STOP");

    int result = EXIT_FAILURE;
    if (runner) {
        try {
            result = runClient(testfiles, myself, fileidx, importeridx);
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
