/*+
________________________________________________________________________

 CopyRight:     (C) de Groot-Bril Earth Sciences B.V.
 Author:        Nanne Hemstra
 Date:          June 2001
 RCS:           $Id: uiconvpos.cc,v 1.5 2001-11-15 08:13:01 nanne Exp $
________________________________________________________________________

-*/

#include "uiconvpos.h"
#include "pixmap.h"
#include "survinfo.h"
#include "uibutton.h"
#include "uidialog.h"
#include "uigeninput.h"

uiConvertPos::uiConvertPos( uiParent* p, SurveyInfo* si )
	: uiDialog(p, "Position conversion")
	, survinfo(si)

{
    setTitleText( "Coordinates vs Inline/X-line" );

    inlfld = new uiGenInput( this, "In-line", IntInpSpec() );
    crlfld = new uiGenInput( this, "Cross-line", IntInpSpec() );
    xfld = new uiGenInput( this, "X-coordinate", DoubleInpSpec() );
    yfld = new uiGenInput( this, "Y-coordinate", DoubleInpSpec() );

    const ioPixmap right( GetDataFileName("rightarrow.png") );
    const ioPixmap left( GetDataFileName("leftarrow.png") );
    docoordbut = new uiToolButton( this, "", right );
    docoordbut->activated.notify( mCB(this,uiConvertPos, getCoord) );
    dobinidbut = new uiToolButton( this, "", left );
    dobinidbut->activated.notify( mCB(this,uiConvertPos, getBinID) );

    crlfld->attach( alignedBelow, inlfld );
    dobinidbut->attach( rightAlignedAbove, inlfld );
    docoordbut->attach( rightOf, dobinidbut, 0 );
    xfld->attach( rightTo, inlfld );
    yfld->attach( alignedBelow, xfld );
    setOkText("Quit");
    setCancelText("");
}

void uiConvertPos::getCoord()
{
    BinID binid( inlfld->getIntValue(), crlfld->getIntValue() );
    Coord coord( survinfo->transform( binid ) );
    xfld->setValue( coord.x );
    yfld->setValue( coord.y );
}

void uiConvertPos::getBinID()
{
    Coord coord( xfld->getValue(), yfld->getValue() );
    BinID binid( survinfo->transform( coord ) );
    inlfld->setValue( binid.inl );
    crlfld->setValue( binid.crl );
}

