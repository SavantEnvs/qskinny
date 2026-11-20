// Shared one-time Qt setup for the QSkinny fuzz harnesses.
// QSkinny's graphic code paints through QPainter/QRawFont, which need a
// QGuiApplication; the offscreen platform plugin runs headless.
#pragma once
#include <QGuiApplication>
#include <QByteArray>

static void qskFuzzInitApp()
{
    static int argc = 1;
    static char arg0[] = "qsk_fuzz";
    static char* argv[] = { arg0, nullptr };
    qputenv( "QT_QPA_PLATFORM", "offscreen" );
    qputenv( "QT_LOGGING_RULES", "*.debug=false;*.warning=false" );
    static QGuiApplication* app = new QGuiApplication( argc, argv );
    (void)app;
}
