/**
 * @file Main.cc
 * @author Tomasz Okon
 * @brief Application entry point.
 */

#include <QApplication>

#include "gui/MainWindow.h"

/// Application entry point: starts the Qt event loop with the main window shown.
auto main( int argc, char* argv[] ) -> int
{
    QApplication app( argc, argv );

    MainWindow main_window;
    main_window.show();

    return QApplication::exec();
}
