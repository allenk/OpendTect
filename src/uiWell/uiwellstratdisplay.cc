/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bruno
 Date:          Mar 2010
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiwellstratdisplay.cc,v 1.8 2010-05-11 14:22:11 cvsbruno Exp $";

#include "uiwellstratdisplay.h"

#include "stratlevel.h"
#include "uigraphicsscene.h"
#include "uistrattreewin.h"
#include "welld2tmodel.h"
#include "wellmarker.h"
#include "welldisp.h"

uiWellStratDisplay::uiWellStratDisplay( uiParent* p, bool nobg,
					const Well::Well2DDispData& dd)
    : uiAnnotDisplay(p,"")
    , dispdata_(dd)
    , uidatagather_(uiStratAnnotGather(data_,StratTWin().mgr()))
{
    if ( nobg )
    {
	setNoSytemBackGroundAttribute();
	uisetBackgroundColor( Color( 255, 255, 255, 255 )  );
	scene().setBackGroundColor( Color( 255, 255, 255, 255 )  );
    }
    drawer_.yAxis() = *new uiAxisHandler( scene_, 
    	uiAxisHandler::Setup(uiRect::Left).noborderspace(true)
					  .border(uiBorder(0))
					  .nogridline(true));
    drawer_.xAxis() = *new uiAxisHandler( scene_, 
    	uiAxisHandler::Setup(uiRect::Top).noborderspace(true)
					 .border(uiBorder(0))
					 .nogridline(true));
    drawer_.xAxis().setBounds( StepInterval<float>( 0, 100, 10 ) );
   
    uidatagather_.newtreeRead.notify( mCB(this,uiWellStratDisplay,dataChanged));
    dataChanged(0);
}


void uiWellStratDisplay::dataChanged( CallBacker* )
{
    for ( int colidx=0; colidx<nrCols(); colidx++ )
    {
	for ( int idx=0; idx<nrUnits( colidx ); idx++ )
	{
	    AnnotData::Unit* unit = getUnit( idx, colidx );
	    if ( unit )
		setUnitPos( *unit );
	}
    }
    setZRange( Interval<float>( (dispData().zrg_.stop/1000), 
				(dispData().zrg_.start )/1000) );
}


void uiWellStratDisplay::setUnitPos( AnnotData::Unit& unit )  
{
    if ( !dispdata_.markers_ ) return;
    float& toppos = unit.zpos_; 
    float& botpos = unit.zposbot_;
    const Well::Marker* topmrk = 0;
    const Well::Marker* basemrk = 0;
    for ( int idx=0; idx<dispdata_.markers_->size(); idx++ )
    {
	const Strat::Level* lvl = (*dispdata_.markers_)[idx]->level();
	if ( lvl && !strcmp( lvl->name(), unit.annots_[0]->buf() ) )
	    topmrk = (*dispdata_.markers_)[idx];
	if ( lvl && !strcmp( lvl->name(), unit.annots_[2]->buf() ) )
	    basemrk = (*dispdata_.markers_)[idx];
    }
    if ( !topmrk || !basemrk ) 
    { 
	toppos = mUdf(float); 
	botpos = mUdf(float); 
    }
    else
    {
	toppos = topmrk->dah();
	botpos = basemrk->dah();
	if ( dispdata_.zistime_ && dispdata_.d2tm_ ) 
	{ 
	    toppos = dispdata_.d2tm_->getTime( toppos ); 
	    botpos = dispdata_.d2tm_->getTime( botpos ); 
	}
    }
}

