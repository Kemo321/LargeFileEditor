/**
 * Author: Tomasz Okon
 * Description: Application entry point.
 */

#include <QApplication>

#include "gui/MainWindow.h"

using namespace std;

int main( int argc, char* argv[] )
{
    QApplication app( argc, argv );

    MainWindow main_window;
    main_window.show();

    return app.exec();
}
