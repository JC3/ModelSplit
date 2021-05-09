#include <QCoreApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QStringList>
#include <QString>
#include <QTextStream>
#include <QDomDocument>
#include <QDebug>
#include <QDateTime>
#include <filesystem>
#include <optional>
#include <string>

using namespace std;
using path = std::filesystem::path;


struct Options {
    path file1;
    path file2;
    path outfile;
    path basedir;
    optional<path> base1;
    optional<path> base2;
};

template <typename O, typename V>
static void set_if (optional<O> &opt, bool cond, const V &val) {
    if (cond)
        opt = val;
    else
        opt.reset();
}

template <typename V>
static QString qstring (const V &val) {
    QString s;
    QTextStream(&s) << val;
    return s;
}

template <>
static QString qstring<QString> (const QString &val) {
    return val;
}

template <>
static QString qstring<string> (const string &val) {
    return QString::fromStdString(val);
}

template <>
static QString qstring<path> (const path &val) {
    return QString::fromStdString(val.string());
}

template <typename O>
static QString qstring_if (const optional<O> &opt, const QString &def = "<not set>") {
    return opt.has_value() ? qstring(opt.value()) : def;
}

#define latin1(val) (qstring((val)).toLatin1().constData())

static QDomElement xml_foreach (QDomElement elem, QString tag, const function<void(QDomElement)> &func) {
    for (QDomElement c = elem.firstChildElement(tag); !c.isNull(); c = c.nextSiblingElement(tag))
        func(c);
    return elem;
}

static QDomElement xml_find (QDomElement elem, QString tag, const function<bool(QDomElement)> &func) {
    for (QDomElement c = elem.firstChildElement(tag); !c.isNull(); c = c.nextSiblingElement(tag))
        if (func(c))
            return c;
    return QDomElement();
}

static void xml_save (const QDomDocument &doc, const path &out) {

    printf("saving %s...\n", latin1(out.filename()));

    QFile f(qstring(out));
    if (!f.open(QFile::WriteOnly | QFile::Text))
        throw runtime_error(f.errorString().toStdString());

    QTextStream t(&f);
    doc.save(t, 2);

}


static QDomDocument load (const path &thepath, const optional<path> &newbase, const Options &opts) {

    printf("loading %s...\n", latin1(thepath.filename()));

    // -------- parse

    QDomDocument report;
    {
        QFile thefile(qstring(thepath));
        if (!thefile.open(QFile::ReadOnly | QFile::Text))
            throw runtime_error(thefile.errorString().toStdString());
        QString message;
        if (!report.setContent(&thefile, false, &message))
            throw runtime_error(message.toStdString());
    }

    // -------- validate

    QDomElement root = report.documentElement();
    if (root.tagName() != "canread_test_report")
        throw runtime_error("not a report file");
    else if (root.attribute("version") != "1")
        throw runtime_error("unsupported report version");
    if (root.attribute("escapes") != "html")
        printf("[warning] unexpected string escape flavor");

    QDomElement meta = root.firstChildElement("meta");
    QDomElement config = meta.firstChildElement("config");
    if (xml_find(config, "var", [] (QDomElement var) {
          return (var.attribute("name") == "WHICH_TEST" && var.attribute("value") == "2");
        }).isNull())
        throw runtime_error("not a read test");

    // -------- normalize paths

    QDomElement files = root.firstChildElement("files");
    if (files.isNull())
        throw runtime_error("missing file list");
    QDomElement results = root.firstChildElement("results");
    if (results.isNull())
        throw runtime_error("missing results list");

    path basedir;
    if (newbase.has_value())
        basedir = newbase.value();
    else
        basedir = files.attribute("base").toStdString();
    basedir = opts.basedir / basedir;

    printf("scanning results (basedir=%s)...\n", latin1(filesystem::relative(basedir)));

    QMap<int,QString> cmpkeys;
    xml_foreach(files, "file", [&] (QDomElement testfile) {
        path filepath;
        try{ filepath = filesystem::weakly_canonical(basedir / testfile.text().toStdString()); }catch(...){}
        if (filepath.empty())
            try{ filepath = filesystem::absolute(basedir / testfile.text().toStdString()); }catch(...){}
        if (filepath.empty())
            filepath = filesystem::relative(basedir / testfile.text().toStdString());
        int fileid = testfile.attribute("id", "-1").toInt();
        if (fileid < 0)
            throw runtime_error("missing file id");
        if (cmpkeys.contains(fileid))
            throw runtime_error("duplicate file id");
        cmpkeys[fileid] = qstring(filepath.generic_string());
    });

    xml_foreach(results, "model", [&] (QDomElement model) {
        int fileid = model.attribute("file", "-1").toInt();
        if (fileid < 0)
            throw runtime_error("missing model file id");
        if (!cmpkeys.contains(fileid))
            throw runtime_error("unknown model file id");
        model.setAttribute("data-cmpkey", cmpkeys[fileid]);
    });

    // --------

    return report;

}


static QDomDocument diff (const QDomDocument &report1, const QDomDocument &report2, const Options &opts) {

    QDomDocument report;

    QDomElement root = report.createElement("compare_reports");
    report.appendChild(root);
    root.setAttribute("version", 1);
    root.appendChild(report.createComment(QDateTime::currentDateTime().toString()));
    root.appendChild(report.createComment("Report 1: " + qstring(filesystem::relative(opts.file1).generic_string())));
    root.appendChild(report.createComment("Report 2: " + qstring(filesystem::relative(opts.file2).generic_string())));

    return report;

}


int main (int argc, char *argv[]) {

    QCoreApplication app(argc, argv);

    QCommandLineParser p;
    p.addPositionalArgument("file1", "First report XML file.");
    p.addPositionalArgument("file2", "Second report XML file.");
    p.addOption({ "base1", "Override basedir of file1 for path comparisons.", "basedir1" });
    p.addOption({ "base2", "Override basedir of file2 for path comparisons.", "basedir2" });
    p.addOption({ { "b", "base" }, "Set general base directory for relative paths.", "basedir", "." });
    p.addOption({ { "o", "out" }, "Output file.", "outfile" });
    p.addHelpOption();

    p.process(app);
    if (p.positionalArguments().size() != 2)
        p.showHelp(EXIT_FAILURE);
    else if (!p.isSet("o"))
        p.showHelp(EXIT_FAILURE);

    Options opts;
    opts.file1 = p.positionalArguments()[0].toStdString();
    opts.file2 = p.positionalArguments()[1].toStdString();
    opts.outfile = p.value("o").toStdString();
    opts.basedir = filesystem::absolute(p.value("b").toStdString());
    set_if(opts.base1, p.isSet("base1"), p.value("base1").toStdString());
    set_if(opts.base2, p.isSet("base2"), p.value("base2").toStdString());

    printf("file1:    %s\n", latin1(opts.file1));
    printf("file2:    %s\n", latin1(opts.file2));
    printf("basedir1: %s\n", latin1(qstring_if(opts.base1)));
    printf("basedir2: %s\n", latin1(qstring_if(opts.base2)));
    printf("basedir:  %s\n", latin1(opts.basedir));
    printf("outfile:  %s\n", latin1(opts.outfile));

    int retcode = EXIT_FAILURE;
    try {
        QDomDocument file1 = load(opts.file1, opts.base1, opts);
        QDomDocument file2 = load(opts.file2, opts.base2, opts);
        QDomDocument report = diff(file1, file2, opts);
        xml_save(report, opts.outfile);
        retcode = EXIT_SUCCESS;
    } catch (const std::exception &x) {
        fprintf(stderr, "error: %s\n", x.what());
    }

    return retcode;

}
