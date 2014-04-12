#include <QCoreApplication>
#include <QCommandLineParser>

#include "qatemuploader.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    QCoreApplication::setApplicationName("qatemuploader");
    QCoreApplication::setApplicationVersion("0.1");

    QCommandLineParser parser;
    parser.setApplicationDescription("Uploads image files to a Blackmagic ATEM switcher.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("address", QCoreApplication::translate("main", "Address of the Blackmagic ATEM switcher"));
    parser.addPositionalArgument("position", QCoreApplication::translate("main", "Position in the still store"));
    parser.addPositionalArgument("source", QCoreApplication::translate("main", "Image file to upload"));

    parser.process(a);

    QStringList arguments = parser.positionalArguments();

    if (arguments.count() != 3)
    {
        parser.showHelp(-1);
    }

    QAtemUploader uploader;
    uploader.upload(arguments[2], arguments[0], arguments[1].toInt());

    return a.exec();
}
