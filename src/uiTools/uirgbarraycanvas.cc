/*+
 ________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Feb 2007
 RCS:           $Id: uirgbarraycanvas.cc,v 1.11 2008-10-27 11:21:08 cvssatyaki Exp $
 ________________________________________________________________________

-*/

#include "uirgbarraycanvas.h"
#include "uirgbarray.h"
#include "uigraphicsscene.h"
#include "uigraphicsitemimpl.h"
#include "iodrawtool.h"
#include "pixmap.h"

uiRGBArrayCanvas::uiRGBArrayCanvas( uiParent* p,
				    const uiRGBArrayCanvas::Setup& setup,
				    uiRGBArray& a )
    	: uiGraphicsView( p,"RGB Array view" )
	, pixmapitm_( 0 )
	, rgbarr_( a ) 
	, newFillNeeded( this )
	, border_( 0,0,0,0 )
	, bgcolor_( Color::NoColor ) 
	, dodraw_( true )
	, pixmap_( 0 )
	, arrarea_( uiRect(0,0,0,0) )
	, width_( setup.width_ )
	, height_( setup.height_ )
{
    setStretch( 2, 2 );
    setScrollBar( setup.scrollbar_ );
}


uiRGBArrayCanvas::~uiRGBArrayCanvas()
{
    delete pixmap_;
    delete pixmapitm_;
}


void uiRGBArrayCanvas::setBorder( const uiBorder& newborder )
{
    if ( border_ != newborder )
	border_ = newborder;
}


void uiRGBArrayCanvas::setBGColor( const Color& c )
{
    if ( bgcolor_ != c )
    {
	bgcolor_ = c;
	setBackgroundColor( bgcolor_ );
    }
}


void uiRGBArrayCanvas::setBackgroundQpaque( bool withalpha )
{
    scene().useBackgroundPattern( withalpha );
}


void uiRGBArrayCanvas::setDrawArr( bool yn )
{
    if ( dodraw_ != yn )
	dodraw_ = yn;
}


void uiRGBArrayCanvas::beforeDraw()
{
    const uiSize totsz( width(), height() ); 
    const int unusedpix = 1;
    arrarea_ = border_.getRect( totsz, unusedpix );
    const int xsz = arrarea_.width() + 1;
    const int ysz = arrarea_.height() + 1;
    if ( xsz < 1 || ysz < 1 )
    {
	rgbarr_.setSize( 1, 1 );
	pixmap_ = new ioPixmap( 1, 1 );
	return;
    }

    if ( rgbarr_.getSize(true) != xsz || rgbarr_.getSize(false) != ysz )
    {
	rgbarr_.setSize( xsz, ysz );
	delete pixmap_; pixmap_ = 0;
    }
}


void uiRGBArrayCanvas::setPixmap( const ioPixmap& pixmap )
{
    if ( pixmap_ )
	delete pixmap_;
    pixmap_ = new ioPixmap( pixmap );
}


void uiRGBArrayCanvas::draw()
{
    if ( !dodraw_ )
	return;

    if ( !pixmap_ )
    {
	rgbarr_.clear( bgcolor_ );
	newFillNeeded.trigger();
	mkNewFill();
	if ( !createPixmap() )
	    return;
    }

    const uiRect pixrect( 0, 0, pixmap_->width()-1, pixmap_->height()-1 );
    if ( pixrect.right() < 1 && pixrect.bottom() < 1 )
	return;

    if ( !pixmapitm_ )
	pixmapitm_ = scene().addPixmap(*pixmap_);
    else
	pixmapitm_->setPixmap( *pixmap_ );
    pixmapitm_->setOffset( arrarea_.left(), arrarea_.top() );
}


void uiRGBArrayCanvas::setPixMapPos( int x, int y )
{
    pixmapitm_->setOffset( x, y );
}


void uiRGBArrayCanvas::rubberBandHandler( uiRect r )
{
    CBCapsule<uiRect> caps( r, this );
}


bool uiRGBArrayCanvas::createPixmap()
{
    const int xsz = rgbarr_.getSize( true );
    const int ysz = rgbarr_.getSize( false );
    delete pixmap_; pixmap_ = 0;
    if ( xsz < 1 || ysz < 1 )
	return false;

    pixmap_ = new ioPixmap( rgbarr_ );
    return true;
}
