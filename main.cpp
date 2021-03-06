#include <QApplication>
#include <QCommandLineParser>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("inspectrum");
    MainWindow mainWin;

    QCommandLineParser parser;
    parser.setApplicationDescription("spectrum viewer");
    parser.addHelpOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("main", "File to view."));

    // Add options
    QCommandLineOption rateOption(QStringList() << "r" << "rate",
	    QCoreApplication::translate("main", "Set sample rate."),
	    QCoreApplication::translate("main", "Hz"));
    parser.addOption(rateOption);

    // Process the actual command line
    parser.process(a);

    if (parser.isSet(rateOption)){
	bool ok;
	// Use toDouble just for scientific notation support
	int rate = parser.value(rateOption).toDouble(&ok);
	if(!ok){
	    fputs("ERROR: could not parse rate\n", stderr);
	    return 1;
	}
	mainWin.changeSampleRate(rate);
    }

    const QStringList args = parser.positionalArguments();
    if (args.size()>=1)
	mainWin.openFile(args.at(0));

    mainWin.show();
    return a.exec();
}
