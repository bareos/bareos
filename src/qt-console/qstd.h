#ifndef QSTD_H
#define QSTD_H

#include <QTextStream>
#include <QFile>
#include <QString>

/** @short helper objects and functions which help reduce the
       need for char[] and the standard library.  

    defines three @ref QTextStream instances
    which behave like the c++ standard iostreams, bound to the
    standard in/out/error. 
    
    Also provided, some helper functions for writing
    interactive stdin/stdout applications.
*/
//start
namespace qstd {

    /** @short  An alias for standard input
     */
    extern QTextStream cin;  /* declared only, defined in the .cpp file */
    /** @short  An alias for standard output
     */
    extern QTextStream cout;
    /** @short  An alias for standard error
     */
    extern QTextStream cerr;
    /** yes/no prompt
        interactive stdin UI - prompts user with
        a yes/no question. Repeatedly-asks
        until user supplies a valid answer.
 
        @param yesNoQuestion the yes/no question
        @return true/false depending on what the
        user responded.
    */
    bool yes(QString yesNoQuestion);
    /** Convenience function that feeds a  specific question
        to the yes() function.
        @usage do {.....} while(more ("foobar"));
        so that user sees the question: "Another foobar (y/n)? "
        @param name of the item being handled by the loop.
    */
    bool more(QString prompt);
    /** A function for safely taking an int from the keyboard.
        Takes data into a  QString and tests to make sure it
        can be converted to int before returning.
        @param base allows choice of number base.
        @return returns validated int.
    */
    int promptInt(int base = 10);
    /** A function for safely taking a double from the keyboard.
        Takes data into a  QString and tests to make sure it
        can be converted to double before returning.
        @return returns validated int.
    */
    double promptDouble();
    /** Complete dialog for opening a file for output.
        Asks user for file name, checks to see if
        file already exists and, if so, asks the user if
        it is ok to overwrite.
        @param Reference QFile parameter is set to point
        to the (eventually) opened file.
    */
    /** @short Dialog for a output file prompt
     */
    void promptOutputFile(QFile& outfile);

    /** @short Dialog for input file prompt */
    void promptInputFile(QFile& infile);
    
    
//end
}

#endif

