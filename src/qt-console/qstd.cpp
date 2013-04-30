//start id=namespace
#include "qstd.h"

/* QTextStreams look a lot like iostreams,
we just have to point them to the right place. */

//start id=streamdefs
QTextStream qstd::cin(stdin, QIODevice::ReadOnly);
QTextStream qstd::cout(stdout, QIODevice::WriteOnly);
QTextStream qstd::cerr(stderr, QIODevice::WriteOnly);
//end


/* Namespace members are like static class members */
bool qstd::yes(QString question) {
    QString ans;
    cout << QString(" %1 [y/n]? ").arg(question);
    cout.flush();
    ans = cin.readLine();
    return (ans.toUpper().startsWith("Y", Qt::CaseInsensitive));
}
//end

bool qstd::more(QString s) {
    return yes(QString("Another %1").arg(s));
}


int qstd::promptInt(int base /* =10 */) { /* Usage: int n = promptInt(); */
    QString numstr;
    int result;
    bool ok;
    cout << ": " << flush;
    while (1) {
        numstr = cin.readLine();
        result = numstr.toInt(&ok, base);
        if (!ok) {
            cout << "Invalid number. Try again: ";
            cout.flush();
        }
        else
            return result;
    }
}


double qstd::promptDouble() { /* Usage: double d = promptDouble(); */
    QString numstr;
    double result;
    bool ok;
    while (1) {
        numstr = cin.readLine();
        result = numstr.toDouble(&ok);
        if (!ok) {
            cout << "Invalid number. Try again: ";
            cout.flush();
        }
        else
            return result;
    }
}


void qstd::promptOutputFile(QFile& outfile) {
    QString filename;
    while (1) {
        cout << "Please enter the file name for saving this data: ";
        cout.flush();
        filename = cin.readLine();
        outfile.setFileName(filename);
        bool fileExists = outfile.open(QIODevice::ReadOnly);
        if (!fileExists)
            break;
        if (yes("File already exists ... Ok to overwrite"))
            break;
        outfile.close();
        outfile.reset();
    }
    outfile.close();
    outfile.reset();
    outfile.open(QIODevice::WriteOnly);
    cout << filename << " open for writing ...\n";
    cout.flush();
}


void qstd::promptInputFile(QFile& infile) {
    QString filename;
    while (1) {
        cout << "Name of the file to be read:  ";
        cout.flush();
        filename = cin.readLine();
        infile.setFileName(filename);
        bool fileExists = infile.open(QIODevice::ReadOnly);
        if (fileExists)
            break;
        cout << "File does not exist ... Please try again. \n";
        cout.flush();
        infile.reset();
    }
    cout << filename << " open for reading ...\n";
    cout.flush();
}

