/**
 * @file QtTestApp.h
 * @author Jan Szwagierczak
 * @brief Provides a single process-wide QGuiApplication for the headless GUI unit tests.
 *
 * The presentation-logic classes are QObjects: CursorManager owns a QTimer and the blink test
 * spins a real event loop via QSignalSpy::wait(). The EditorController tests also synthesise
 * QKeyEvents, which are QtGui input events backed by QInputDevice statics — those are only
 * cleaned up correctly when a QGuiApplication owns them, so a bare QCoreApplication crashes on
 * exit. The application therefore uses the "offscreen" platform, which needs no real display.
 *
 * Because only one application may exist per process, the instance lives in the local static of
 * an inline function, which the standard guarantees is shared across every translation unit that
 * includes this header.
 */
#pragma once

#include <QByteArray>
#include <QGuiApplication>

namespace test_support {

/// Returns the process-wide QGuiApplication, constructing it (headless) on first use.
///
/// The instance is intentionally leaked: destroying a QGuiApplication via a static destructor at
/// process exit races Qt's own QInputDevice global statics (created when synthesising QKeyEvents)
/// and segfaults. Never running the application's destructor sidesteps that teardown ordering.
inline auto qtApp() -> QGuiApplication&
{
    static QGuiApplication* app = [] {
        static int argc = 1;
        static char appName[] = "unit_tests";
        static char* argv[] = { appName, nullptr };
        // Force the windowless platform so the tests never touch a display server.
        qputenv( "QT_QPA_PLATFORM", QByteArray( "offscreen" ) );
        return new QGuiApplication( argc, argv );
    }();
    return *app;
}

}  // namespace test_support
