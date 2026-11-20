// Functional self-test for QSkinny's untrusted-input code (QskGraphicIO, QskGraphic,
// QskGraphicPaintEngine, QskGlyphTable), on files shipped in the repo.
// Known-answer checks: every check prints "PASS <name>" or "FAIL <name> ...".
// Run from the repo root: build/test/qsk_selftest
#include <QskGlyphTable.h>
#include <QskGraphic.h>
#include <QskGraphicIO.h>
#include <QskPainterCommand.h>

#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QRawFont>
#include <QSvgRenderer>

#include <cstdio>

// Known answers (measured on the upstream sources at the integration commit).
#define EXP_TUX_COMMANDS 21
#define EXP_TUX_BOUNDS "0,0,219,254"
#define EXP_TUX_PIXELS 1322
#define EXP_TUX_WRITE_SIZE 22861
#define EXP_PLUS_COMMANDS 3
#define EXP_PLUS_REREAD_COMMANDS 2
#define EXP_PLUS_PIXELS 240
#define EXP_OCT_COUNT 180
#define EXP_OCT_NAMES 179
#define EXP_OCT_NAME "rocket"
#define EXP_OCT_NAME_INDEX 42
#define EXP_OCT_GRAPHIC 2
#define EXP_ICO_COUNT 591
#define EXP_ICO_NAMES 0
#define EXP_ICO_CODE 99
#define EXP_FA_COUNT 215
#define EXP_FA_NAMES 207

static int passed = 0, failed = 0;

static void check( const char* name, bool ok, const QString& detail = QString() )
{
    if ( ok ) { passed++; std::printf( "PASS %s\n", name ); }
    else { failed++; std::printf( "FAIL %s %s\n", name, qPrintable( detail ) ); }
}

template< typename T >
static void checkEq( const char* name, const T& actual, const T& expected )
{
    check( name, actual == expected,
        QStringLiteral( "actual=%1 expected=%2" ).arg( actual ).arg( expected ) );
}

static QByteArray readFile( const QString& path )
{
    QFile f( path );
    return f.open( QIODevice::ReadOnly ) ? f.readAll() : QByteArray();
}

static QByteArray writeQvg( const QskGraphic& g )
{
    QByteArray bytes;
    QBuffer b( &bytes );
    b.open( QIODevice::WriteOnly );
    QskGraphicIO::write( g, &b );
    return bytes;
}

static int opaquePixels( const QImage& img )
{
    int n = 0;
    for ( int y = 0; y < img.height(); y++ )
        for ( int x = 0; x < img.width(); x++ )
            if ( qAlpha( img.pixel( x, y ) ) > 0 ) n++;
    return n;
}

int main( int argc, char* argv[] )
{
    qputenv( "QT_QPA_PLATFORM", "offscreen" );
    QGuiApplication app( argc, argv );

    // --- .qvg reader / writer / renderer
    const QByteArray tuxBytes = readFile( "examples/qvgviewer/qvg/Tux.qvg" );
    QByteArray tmp = tuxBytes;
    QBuffer in( &tmp );
    in.open( QIODevice::ReadOnly );
    const QskGraphic tux = QskGraphicIO::read( &in );

    checkEq( "qvg_tux_commands", int( tux.commands().size() ), EXP_TUX_COMMANDS );
    const QRectF br = tux.boundingRect();
    checkEq( "qvg_tux_bounds", QStringLiteral( "%1,%2,%3,%4" )
        .arg( qRound( br.x() ) ).arg( qRound( br.y() ) )
        .arg( qRound( br.width() ) ).arg( qRound( br.height() ) ), QStringLiteral( EXP_TUX_BOUNDS ) );
    // write -> read -> write is stable
    QByteArray gen1 = writeQvg( tux );
    checkEq( "qvg_tux_write_size", int( gen1.size() ), EXP_TUX_WRITE_SIZE );
    QBuffer g1b( &gen1 );
    g1b.open( QIODevice::ReadOnly );
    const QskGraphic tux2 = QskGraphicIO::read( &g1b );
    checkEq( "qvg_tux_reread_commands", int( tux2.commands().size() ), EXP_TUX_COMMANDS );
    check( "qvg_tux_roundtrip_bytes", writeQvg( tux2 ) == writeQvg( tux ) );
    checkEq( "qvg_tux_render", opaquePixels( tux.toImage( QSize( 64, 64 ) ) ), EXP_TUX_PIXELS );

    // garbage is rejected
    QByteArray junk( "QSKX not a graphic" );
    QBuffer jb( &junk );
    jb.open( QIODevice::ReadOnly );
    check( "qvg_bad_magic", QskGraphicIO::read( &jb ).isNull() );

    // --- svg -> QskGraphic (svg2qvg path)
    QSvgRenderer renderer( QStringLiteral( "examples/gallery/icons/plus.svg" ) );
    QskGraphic plus;
    plus.setViewBox( renderer.viewBoxF() );
    {
        QPainter p( &plus );
        renderer.render( &p );
    }
    checkEq( "svg_plus_commands", int( plus.commands().size() ), EXP_PLUS_COMMANDS );
    QByteArray plusQvg = writeQvg( plus );
    QBuffer pb( &plusQvg );
    pb.open( QIODevice::ReadOnly );
    const QskGraphic plus2 = QskGraphicIO::read( &pb );
    checkEq( "svg_plus_reread_commands", int( plus2.commands().size() ), EXP_PLUS_REREAD_COMMANDS );
    checkEq( "svg_plus_render", opaquePixels( plus2.toImage( QSize( 32, 32 ) ) ), EXP_PLUS_PIXELS );

    // --- icon fonts / QskGlyphTable
    {
        QRawFont font( readFile( "playground/iconbrowser/fonts/Octicons.ttf" ), 16 );
        QskGlyphTable t( font );
        checkEq( "glyph_octicons_count", int( t.glyphCount() ), EXP_OCT_COUNT );
        checkEq( "glyph_octicons_names", int( t.nameTable().size() ), EXP_OCT_NAMES );
        checkEq( "glyph_octicons_name_lookup", int( t.nameToIndex( QStringLiteral( EXP_OCT_NAME ) ) ), EXP_OCT_NAME_INDEX );
        checkEq( "glyph_octicons_graphic", int( t.glyphGraphic( EXP_OCT_NAME_INDEX ).commands().size() ), EXP_OCT_GRAPHIC );
    }
    {
        QRawFont font( readFile( "playground/iconbrowser/fonts/IcoMoon-Free.ttf" ), 16 );
        QskGlyphTable t( font );
        checkEq( "glyph_icomoon_count", int( t.glyphCount() ), EXP_ICO_COUNT );
        checkEq( "glyph_icomoon_names", int( t.nameTable().size() ), EXP_ICO_NAMES );
        checkEq( "glyph_icomoon_code", int( t.codeToIndex( 0xe900 ) ), EXP_ICO_CODE );
    }
    {
        QRawFont font( readFile( "playground/iconbrowser/fonts/Font Awesome 6 Free-Regular-400.otf" ), 16 );
        QskGlyphTable t( font );
        checkEq( "glyph_fa_cff_count", int( t.glyphCount() ), EXP_FA_COUNT );
        checkEq( "glyph_fa_cff_names", int( t.nameTable().size() ), EXP_FA_NAMES );
    }

    std::printf( "SUMMARY passed=%d failed=%d\n", passed, failed );
    return failed ? 1 : 0;
}
