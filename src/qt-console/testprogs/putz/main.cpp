#include <QApplication>
#include "putz.h"

Putz *putz;
QApplication *app;

int main(int argc, char *argv[])
{
	app = new QApplication(argc, argv);
	app->setQuitOnLastWindowClosed(true);

	putz = new Putz;
	putz->show();

	return app->exec();
}
