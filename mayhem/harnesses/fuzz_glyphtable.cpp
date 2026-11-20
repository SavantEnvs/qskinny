// Fuzz QskGlyphTable: an (icon) font file given as bytes is loaded with
// QRawFont, then QSkinny's own 'post' / 'CFF ' table parsers build the glyph
// name table, and glyphs are converted to QskGraphic and rasterized.
#include "qsk_fuzz_init.h"

#include <QskGlyphTable.h>
#include <QskGraphic.h>

#include <QRawFont>
#include <QImage>
#include <QHash>
#include <QString>

extern "C" int LLVMFuzzerInitialize( int*, char*** )
{
    qskFuzzInitApp();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
    const QByteArray bytes( reinterpret_cast< const char* >( data ), int( size ) );

    QRawFont font( bytes, 16 );
    if ( !font.isValid() )
        return -1;

    QskGlyphTable table( font );
    const uint count = table.glyphCount();

    const auto names = table.nameTable();
    for ( auto it = names.constBegin(); it != names.constEnd(); ++it )
        (void)table.nameToIndex( it.key() );
    (void)table.nameToIndex( QStringLiteral( "home" ) );
    (void)table.codeToIndex( 0xe000 );
    (void)table.codeToIndex( 'A' );

    const uint n = count < 8 ? count : 8;
    for ( uint i = 0; i < n; i++ )
    {
        const auto graphic = table.glyphGraphic( i );
        if ( !graphic.isNull() )
            (void)graphic.toImage( QSize( 32, 32 ) );
    }

    return 0;
}
