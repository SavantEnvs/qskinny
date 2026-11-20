// Fuzz the QSkinny vector graphic format (.qvg): QskGraphicIO::read parses
// the QDataStream of painter commands, QskGraphic replays them onto a QImage,
// and the result is serialized again with QskGraphicIO::write.
#include "qsk_fuzz_init.h"

#include <QskGraphic.h>
#include <QskGraphicIO.h>
#include <QskColorFilter.h>

#include <QBuffer>
#include <QImage>
#include <QPainter>

extern "C" int LLVMFuzzerInitialize( int*, char*** )
{
    qskFuzzInitApp();
    return 0;
}

extern "C" int LLVMFuzzerTestOneInput( const uint8_t* data, size_t size )
{
    QByteArray bytes( reinterpret_cast< const char* >( data ), int( size ) );

    // read( const QByteArray& ) does not open its buffer; use the QIODevice overload.
    QBuffer in( &bytes );
    in.open( QIODevice::ReadOnly );

    const QskGraphic graphic = QskGraphicIO::read( &in );
    if ( graphic.isNull() )
        return 0;

    (void)graphic.boundingRect();
    (void)graphic.controlPointRect();
    (void)graphic.toImage( QSize( 64, 64 ) );

    QskColorFilter filter;
    filter.addColorSubstitution( Qt::black, Qt::red );
    {
        QImage image( 48, 48, QImage::Format_ARGB32_Premultiplied );
        image.fill( Qt::transparent );
        QPainter painter( &image );
        graphic.render( &painter, QRectF( 0, 0, 48, 48 ), filter, Qt::KeepAspectRatio );
    }

    QByteArray outBytes;
    QBuffer out( &outBytes );
    out.open( QIODevice::WriteOnly );
    QskGraphicIO::write( graphic, &out );

    return 0;
}
