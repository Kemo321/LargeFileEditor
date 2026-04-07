/**
 * Author: Tomasz Okon
 * Description: Application entry point.
 */

#include <QApplication>

#include "gui/MainWindow.h"

using namespace std;

auto main( int argc, char* argv[] ) -> int
{
    QApplication app( argc, argv );

    MainWindow main_window;
    main_window.show();

    return QApplication::exec();
}
