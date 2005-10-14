/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          May 2005
 RCS:           $Id: uiattrsel.cc,v 1.7 2005-10-14 05:57:37 cvsnanne Exp $
________________________________________________________________________

-*/

#include "uiattrsel.h"
#include "attribdescset.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "attribsel.h"
#include "hilbertattrib.h"
#include "attribstorprovider.h"

#include "uimsg.h"
#include "ioman.h"
#include "iodir.h"
#include "ioobj.h"
#include "iopar.h"
#include "ctxtioobj.h"
#include "ptrman.h"
#include "seistrctr.h"
#include "linekey.h"
#include "cubesampling.h"
#include "survinfo.h"

#include "nlamodel.h"
#include "nladesign.h"

#include "uilistbox.h"
#include "uigeninput.h"
#include "datainpspec.h"
#include "uilabel.h"
#include "uibutton.h"
#include "uicombobox.h"

using namespace Attrib;


uiAttrSelDlg::uiAttrSelDlg( uiParent* p, const char* seltxt,
			    const uiAttrSelData& atd, 
			    Pol2D pol2d, DescID ignoreid )
	: uiDialog(p,Setup("",0,atd.nlamodel?"":"101.1.1"))
	, attrdata(atd)
	, selgrp(0)
	, storfld(0)
	, attrfld(0)
	, attr2dfld(0)
	, nlafld(0)
	, nlaoutfld(0)
	, in_action(false)
{
    attrinf = new SelInfo( atd.attrset, atd.nlamodel, pol2d, ignoreid );
    if ( !attrinf->ioobjnms.size() )
    {
	new uiLabel( this, "No seismic data available.\n"
			   "Please import data first" );
	setCancelText( "Ok" );
	setOkText( "" );
	return;
    }

    const bool havenlaouts = attrinf->nlaoutnms.size();
    const bool haveattribs = attrinf->attrnms.size();

    BufferString nm( "Select " ); nm += seltxt;
    setName( nm );
    setCaption( "Select" );
    setTitleText( nm );

    selgrp = new uiGroup( this, "Input selection" );

    srcfld = new uiRadioButton( selgrp, "Stored" );
    srcfld->activated.notify( mCB(this,uiAttrSelDlg,selDone) );
    srcfld->setSensitive( attrdata.shwcubes );

    calcfld = new uiRadioButton( selgrp, "Attributes" );
    calcfld->attach( alignedBelow, srcfld );
    calcfld->setSensitive( haveattribs );
    calcfld->activated.notify( mCB(this,uiAttrSelDlg,selDone) );

    if ( havenlaouts )
    {
	nlafld = new uiRadioButton( selgrp, attrdata.nlamodel->nlaType(false) );
	nlafld->attach( alignedBelow, calcfld );
	nlafld->setSensitive( havenlaouts );
	nlafld->activated.notify( mCB(this,uiAttrSelDlg,selDone) );
    }

    if ( attrdata.shwcubes )
    {
	storfld = new uiListBox( this, attrinf->ioobjnms );
	storfld->setHSzPol( uiObject::wide );
	storfld->selectionChanged.notify( mCB(this,uiAttrSelDlg,cubeSel) );
	storfld->doubleClicked.notify( mCB(this,uiAttrSelDlg,accept) );
	storfld->attach( rightOf, selgrp );
	attr2dfld = new uiGenInput( this, "Stored Attribute",
				    StringListInpSpec() );
	attr2dfld->attach( alignedBelow, storfld );
	filtfld = new uiGenInput( this, "Filter", "*" );
	filtfld->attach( alignedBelow, storfld );
	filtfld->valuechanged.notify( mCB(this,uiAttrSelDlg,filtChg) );
    }

    if ( haveattribs )
    {
	attrfld = new uiListBox( this, attrinf->attrnms );
	attrfld->setHSzPol( uiObject::wide );
	attrfld->doubleClicked.notify( mCB(this,uiAttrSelDlg,accept) );
	attrfld->attach( rightOf, selgrp );
    }

    if ( havenlaouts )
    {
	nlaoutfld = new uiListBox( this, attrinf->nlaoutnms );
	nlaoutfld->setHSzPol( uiObject::wide );
	nlaoutfld->doubleClicked.notify( mCB(this,uiAttrSelDlg,accept) );
	nlaoutfld->attach( rightOf, selgrp );
    }

    int seltyp = havenlaouts ? 2 : (haveattribs ? 1 : 0);
    int storcur = -1, attrcur = -1, nlacur = -1;
    if ( attrdata.nlamodel && attrdata.outputnr >= 0 )
    {
	seltyp = 2;
	nlacur = attrdata.outputnr;
    }
    else
    {
	const Desc* desc = attrdata.attribid < 0 ? 0 :
	    		attrdata.attrset->getDesc( attrdata.attribid );
	if ( desc )
	{
	    seltyp = desc->isStored() ? 0 : 1;
	    if ( seltyp == 1 )
		attrcur = attrinf->attrnms.indexOf( desc->userRef() );
	    else if ( storfld )
		storcur = attrinf->ioobjnms.indexOf( desc->userRef() );
	}
	else
	{
	    // Defaults are the last ones added to attrib set
	    for ( int idx=attrdata.attrset->nrDescs()-1; idx!=-1; idx-- )
	    {
		const DescID attrid = attrdata.attrset->getID( idx );
		const Desc& ad = *attrdata.attrset->getDesc( attrid );
		if ( ad.isStored() && storcur == -1 )
		    storcur = attrinf->ioobjnms.indexOf( ad.userRef() );
		else if ( !ad.isStored() && attrcur == -1 )
		    attrcur = attrinf->attrnms.indexOf( ad.userRef() );
		if ( storcur != -1 && attrcur != -1 ) break;
	    }
	}
    }

    if ( storcur == -1 )		storcur = 0;
    if ( attrcur == -1 )		attrcur = attrinf->attrnms.size()-1;
    if ( nlacur == -1 && havenlaouts )	nlacur = 0;

    if ( storfld  )			storfld->setCurrentItem( storcur );
    if ( attrfld && attrcur != -1 )	attrfld->setCurrentItem( attrcur );
    if ( nlaoutfld && nlacur != -1 )	nlaoutfld->setCurrentItem( nlacur );

    if ( seltyp == 0 )		srcfld->setChecked(true);
    else if ( seltyp == 1 )	calcfld->setChecked(true);
    else if ( nlafld )		nlafld->setChecked(true);

    finaliseStart.notify( mCB( this,uiAttrSelDlg,doFinalise) );
}


uiAttrSelDlg::~uiAttrSelDlg()
{
    delete attrinf;
}


void uiAttrSelDlg::doFinalise( CallBacker* )
{
    selDone(0);
    in_action = true;
}


int uiAttrSelDlg::selType() const
{
    if ( calcfld->isChecked() )
	return 1;
    if ( nlafld && nlafld->isChecked() )
	return 2;
    return 0;
}


void uiAttrSelDlg::selDone( CallBacker* c )
{
    if ( !selgrp ) return;

    mDynamicCastGet(uiRadioButton*,but,c);
   
    bool dosrc, docalc, donla; 
    if ( but == srcfld )
    { dosrc = true; docalc = donla = false; }
    else if ( but == calcfld )
    { docalc = true; dosrc = donla = false; }
    else if ( but == nlafld )
    { donla = true; docalc = dosrc = false; }

    if ( but )
    {
	srcfld->setChecked( dosrc );
	calcfld->setChecked( docalc );
	if ( nlafld ) nlafld->setChecked( donla );
    }

    const int seltyp = selType();
    if ( attrfld ) attrfld->display( seltyp == 1 );
    if ( nlaoutfld ) nlaoutfld->display( seltyp == 2 );
    if ( storfld )
    {
	storfld->display( seltyp == 0 );
	filtfld->display( seltyp == 0 );
    }

    cubeSel(0);
}


void uiAttrSelDlg::filtChg( CallBacker* c )
{
    if ( !storfld || !filtfld ) return;

    attrinf->fillStored( filtfld->text() );
    storfld->empty();
    storfld->addItems( attrinf->ioobjnms );
    if ( attrinf->ioobjnms.size() )
	storfld->setCurrentItem( 0 );

    cubeSel( c );
}


void uiAttrSelDlg::cubeSel( CallBacker* c )
{
    const int seltyp = selType();
    if ( seltyp )
    {
	attr2dfld->display( false );
	return;
    }

    int selidx = storfld->currentItem();
    bool is2d = false;
    BufferString ioobjkey;
    if ( selidx >= 0 )
    {
	ioobjkey = attrinf->ioobjids.get( storfld->currentItem() );
	is2d = SelInfo::is2D( ioobjkey.buf() );
    }
    attr2dfld->display( is2d );
    filtfld->display( !is2d );
    if ( is2d )
    {
	BufferStringSet nms;
	SelInfo::getAttrNames( ioobjkey.buf(), nms );
	attr2dfld->newSpec( StringListInpSpec(nms), 0 );
    }
}


bool uiAttrSelDlg::getAttrData( bool needattrmatch )
{
    attrdata.attribid = DescID::undef();
    attrdata.outputnr = -1;
    if ( !selgrp || !in_action ) return true;

    const int seltyp = selType();
    int selidx = (seltyp ? (seltyp == 2 ? nlaoutfld : attrfld) : storfld)
		    ->currentItem();
    if ( selidx < 0 )
	return false;

    if ( seltyp == 1 )
	attrdata.attribid = attrinf->attrids[selidx];
    else if ( seltyp == 2 )
	attrdata.outputnr = selidx;
    else
    {
	DescSet& as = const_cast<DescSet&>( *attrdata.attrset );

	const char* ioobjkey = attrinf->ioobjids.get(selidx);
	LineKey linekey( ioobjkey );
	if ( SelInfo::is2D(ioobjkey) )
	{
	    attrdata.outputnr = attr2dfld->getIntValue();
	    BufferStringSet nms;
	    SelInfo::getAttrNames( ioobjkey, nms );
	    const char* attrnm = attrdata.outputnr >= nms.size() ? 0
				    : nms.get(attrdata.outputnr).buf();
	    if ( needattrmatch )
		linekey.setAttrName( attrnm );
	}

	attrdata.attribid = as.getStoredID( linekey, 0, true );
	if ( needattrmatch && attrdata.attribid < 0 )
	{
	    BufferString msg( "Could not find the seismic data " );
	    msg += attrdata.attribid == DescID::undef() ? "in object manager"
							: "on disk";	
	    uiMSG().error( msg );
	    return false;
	}
    }

    return true;
}


bool uiAttrSelDlg::acceptOK( CallBacker* )
{
    return getAttrData(true);
}


uiAttrSel::uiAttrSel( uiParent* p, const DescSet* ads,
		      const char* txt, DescID curid )
    : uiIOSelect(p,mCB(this,uiAttrSel,doSel),txt?txt:"Input Data")
    , attrdata(ads)
    , ignoreid(DescID::undef())
    , is2d(false)
{
    attrdata.attribid = curid;
    updateInput();
}


uiAttrSel::uiAttrSel( uiParent* p, const char* txt, const uiAttrSelData& ad )
    : uiIOSelect(p,mCB(this,uiAttrSel,doSel),txt?txt:"Input Data")
    , attrdata(ad)
    , ignoreid(DescID::undef())
    , is2d(false)
{
    updateInput();
}


void uiAttrSel::setDescSet( const DescSet* ads )
{
    attrdata.attrset = ads;
    update2D();
}


void uiAttrSel::setNLAModel( const NLAModel* mdl )
{
    attrdata.nlamodel = mdl;
}


void uiAttrSel::setDesc( const Desc* ad )
{
    attrdata.attrset = ad ? ad->descSet() : 0;
    const char* inp = ad->userRef();
    if ( inp[0] == '_' || (ad->isStored() && ad->dataType() == Seis::Dip) )
	return;

    attrdata.attribid = !attrdata.attrset ? DescID::undef() : ad->id();
    updateInput();
    update2D();
}


void uiAttrSel::setIgnoreDesc( const Desc* ad )
{
    ignoreid = DescID::undef();
    if ( !ad ) return;
    if ( !attrdata.attrset ) attrdata.attrset = ad->descSet();
    ignoreid = ad->id();
}


void uiAttrSel::updateInput()
{
    BufferString bs;
    bs = attrdata.attribid.asInt();
    bs += ":";
    bs += attrdata.outputnr;
    setInput( bs );
}


const char* uiAttrSel::userNameFromKey( const char* txt ) const
{
    if ( !attrdata.attrset || !txt || !*txt ) return "";

    BufferString buf( txt );
    char* outnrstr = strchr( buf.buf(), ':' );
    if ( outnrstr ) *outnrstr++ = '\0';
    const DescID attrid( atoi(buf.buf()), true );
    const int outnr = outnrstr ? atoi( outnrstr ) : 0;
    if ( attrid < 0 )
    {
	if ( !attrdata.nlamodel || outnr < 0 )
	    return "";
	if ( outnr >= attrdata.nlamodel->design().outputs.size() )
	    return "<error>";

	const char* nm = attrdata.nlamodel->design().outputs[outnr]->buf();
	return IOObj::isKey(nm) ? IOM().nameOf(nm) : nm;
    }

    const Desc* ad = attrdata.attrset->getDesc( attrid );
    usrnm = ad ? ad->userRef() : "";
    return usrnm.buf();
}


void uiAttrSel::getHistory( const IOPar& iopar )
{
    uiIOSelect::getHistory( iopar );
    updateInput();
}


bool uiAttrSel::getRanges( CubeSampling& cs ) const
{
    if ( attrdata.attribid < 0 || !attrdata.attrset )
	return false;

    const Desc* desc = attrdata.attrset->getDesc( attrdata.attribid );
    if ( !desc->isStored() ) return false;

    const ValParam* keypar = 
		(ValParam*)desc->getParam( StorageProvider::keyStr() );
    const MultiID mid( keypar->getStringValue() );
    return SeisTrcTranslator::getRanges( mid, cs );
}


void uiAttrSel::doSel( CallBacker* )
{
    if ( !attrdata.attrset ) return;

    uiAttrSelDlg dlg( this, inp_->label()->text(), attrdata, 
	    	      is2d ? Only2D : Both2DAnd3D, ignoreid );
    if ( dlg.go() )
    {
	attrdata.attribid = dlg.attribID();
	attrdata.outputnr = dlg.outputNr();
	updateInput();
	update2D();
	selok_ = true;
    }
}


void uiAttrSel::processInput()
{
    if ( !attrdata.attrset ) return;

    BufferString inp = getInput();
    char* attr2d = strchr( inp, '|' );
    DescSet& as = const_cast<DescSet&>( *attrdata.attrset );
    attrdata.attribid = as.getID( inp, true );
    attrdata.outputnr = -1;
    if ( attrdata.attribid >= 0 && attr2d )
    {
	const Desc& ad = *attrdata.attrset->getDesc(attrdata.attribid);
	BufferString defstr; ad.getDefStr( defstr );
	BufferStringSet nms;
	SelInfo::getAttrNames( defstr, nms );
	attrdata.outputnr = nms.indexOf( attr2d );
    }
    else if ( attrdata.attribid < 0 && attrdata.nlamodel )
    {
	const BufferStringSet& outnms( attrdata.nlamodel->design().outputs );
	const BufferString nodenm = IOObj::isKey(inp) ? IOM().nameOf(inp)
	    						: inp.buf();
	for ( int idx=0; idx<outnms.size(); idx++ )
	{
	    const BufferString& outstr = *outnms[idx];
	    const char* desnm = IOObj::isKey(outstr) ? IOM().nameOf(outstr)
						     : outstr.buf();
	    if ( nodenm == desnm )
		{ attrdata.outputnr = idx; break; }
	}
    }

    updateInput();
}


void uiAttrSel::fillSelSpec( SelSpec& as ) const
{
    const bool isnla = attrdata.attribid < 0 && attrdata.outputnr >= 0;
    if ( isnla )
	as.set( 0, DescID(attrdata.outputnr,true), true, "" );
    else
	as.set( 0, attrdata.attribid, false, "" );

    if ( isnla && attrdata.nlamodel )
	as.setRefFromID( *attrdata.nlamodel );
    else
	as.setRefFromID( *attrdata.attrset );
}


const char* uiAttrSel::getAttrName() const
{
    static BufferString ret;

    ret = getInput();
    if ( is2d )
    {
	const Desc& ad = *attrdata.attrset->getDesc( attrdata.attribid );
	if ( ad.isStored() || strchr(ret.buf(),'|') )
	    ret = LineKey( ret ).attrName();
    }

    return ret.buf();
}


bool uiAttrSel::checkOutput( const IOObj& ioobj ) const
{
    if ( attrdata.attribid < 0 && attrdata.outputnr < 0 )
    {
	uiMSG().error( "Please select the input" );
	return false;
    }

    if ( is2d && !SeisTrcTranslator::is2D(ioobj) )
    {
	uiMSG().error( "Can only store this in a 2D line set" );
	return false;
    }

    //TODO this is pretty difficult to get right
    if ( attrdata.attribid < 0 )
	return true;

    const Desc& ad = *attrdata.attrset->getDesc( attrdata.attribid );
    bool isdep = false;
/*
    if ( !is2d )
	isdep = ad.isDependentOn(ioobj,0);
    else
    {
	// .. and this too
	if ( ad.isStored() )
	{
	    LineKey lk( ad.defStr() );
	    isdep = ioobj.key() == lk.lineName();
	}
    }
    if ( isdep )
    {
	uiMSG().error( "Cannot output to an input" );
	return false;
    }
*/

    return true;
}


void uiAttrSel::update2D()
{
    processInput();
    if ( attrdata.attrset && attrdata.attrset->nrDescs() > 0 )
	is2d = attrdata.attrset->is2D();
}


// **** uiImagAttrSel ****

DescID uiImagAttrSel::imagID() const
{
    if ( !attrdata.attrset )
    {
	pErrMsg( "No attribdescset set");
	return DescID::undef();
    }

    const DescID selattrid = attribID();
    TypeSet<DescID> attribids;
    attrdata.attrset->getIds( attribids );
    for ( int idx=0; idx<attribids.size(); idx++ )
    {
	const Desc* desc = attrdata.attrset->getDesc( attribids[idx] );

	if ( strcmp(desc->attribName(),Hilbert::attribName()) )
	    continue;

	const Desc* inputdesc = desc->getInput( 0 );
	if ( !inputdesc || inputdesc->id() != selattrid )
	    continue;

	return attribids[idx];
    }

    DescSet* descset = const_cast<DescSet*>(attrdata.attrset);

    Desc* inpdesc = descset->getDesc( selattrid );
    Desc* newdesc = PF().createDescCopy( Hilbert::attribName() );
    if ( !newdesc || !inpdesc ) return DescID::undef();

    newdesc->selectOutput( 0 );
    newdesc->setInput( 0, inpdesc );
    newdesc->setHidden( true );

    BufferString usrref = "_"; usrref += inpdesc->userRef(); usrref += "_imag";
    newdesc->setUserRef( usrref );

    return descset->addDesc( newdesc );
}
