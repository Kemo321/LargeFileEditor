/**
 * @file Main.cc
 * @author Tomasz Okon
 * @brief Application entry point.
 */

#include <QApplication>
#include <QFileInfo>
#include <QStringList>

#include "gui/MainWindow.h"

/// Application entry point: starts the Qt event loop with the main window shown.
/// An optional path argument opens that file on startup.
auto main( int argc, char* argv[] ) -> int
{
    QApplication app( argc, argv );

    MainWindow main_window;
    main_window.show();

    const QStringList args = QApplication::arguments();
    if( args.size() > 1 ) {
        main_window.loadFile( QFileInfo( args.at( 1 ) ).absoluteFilePath() );
    }

    return QApplication::exec();
}
