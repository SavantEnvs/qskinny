// Fuzz the svg2qvg conversion path (tools/svg2qvg): SVG bytes are rendered by
// QSvgRenderer into a QskGraphic (QskGraphicPaintEngine records every paint
// operation), which is written as .qvg, read back and rasterized.
#include "qsk_fuzz_init.h"

#include <QskGraphic.h>
#include <QskGraphicIO.h>

#include <QSvgRenderer>
#include <QPainter>
#include <QBuffer>
#include <QImage>

extern "C" int LLVMFuzzerInitialize( int*, char*** )
{
    qskFuzzInitApp();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
    const QByteArray bytes( reinterpret_cast< const char* >( data ), int( size ) );

    QSvgRenderer renderer;
    if ( !renderer.load( bytes ) )
        return -1;

    QskGraphic graphic;
    graphic.setViewBox( renderer.viewBoxF() );
    {
        QPainter painter( &graphic );
        renderer.render( &painter );
        painter.end();
    }

    QByteArray qvg;
    {
        QBuffer out( &qvg );
        out.open( QIODevice::WriteOnly );
        QskGraphicIO::write( graphic, &out );
    }

    QBuffer in( &qvg );
    in.open( QIODevice::ReadOnly );
    const QskGraphic copy = QskGraphicIO::read( &in );
    if ( !copy.isNull() )
        (void)copy.toImage( QSize( 64, 64 ) );

    return 0;
}
