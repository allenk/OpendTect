/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          May 2005
 RCS:           $Id: uisteeringsel.cc,v 1.11 2005-08-24 10:01:54 cvsnanne Exp $
________________________________________________________________________

-*/


#include "uisteeringsel.h"
#include "attribdesc.h"
#include "attribdescset.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "paramsetget.h"
#include "attribsel.h"
#include "attribstorprovider.h"
#include "ctxtioobj.h"
#include "ioobj.h"
#include "ioman.h"
#include "iopar.h"
#include "survinfo.h"
#include "seistrctr.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uimsg.h"


using namespace Attrib;

IOPar& uiSteeringSel::inpselhist = *new IOPar( "Steering selection history" );

uiSteeringSel::uiSteeringSel( uiParent* p, const Attrib::DescSet* ads )
    : uiGroup(p,"Steering selection")
    , ctio_(*mMkCtxtIOObj(SeisTrc))
    , descset_(ads)
    , typfld(0)
    , inpfld(0)
    , dirfld(0)
    , dipfld(0)
    , is2d_(false)
    , notypechange_(false)
{
    RefMan<Desc> desc = PF().createDescCopy( "FullSteering" );
    if ( !desc )
    {
	nosteerlbl_ = new uiLabel( this, "<Steering unavailable>" );
	setHAlignObj( nosteerlbl_ );
	return;
    }

    static const char* steertyps[] = { "None", "Central", "Full",
				       "Constant direction", 0 };
    typfld = new uiGenInput( this, "Steering", StringListInpSpec(steertyps) );
    typfld->valuechanged.notify( mCB(this,uiSteeringSel,typeSel));

    ctio_.ctxt.forread = true;
    inpfld = new uiSteerCubeSel( this, ctio_, ads );
    inpfld->getHistory( inpselhist );
    inpfld->attach( alignedBelow, typfld );

    dirfld = new uiGenInput( this, "Azimuth", FloatInpSpec() );
    dirfld->attach( alignedBelow, typfld );
    BufferString dipstr( "Apparent dip " );
    dipstr += SI().zIsTime() ? "(us/m)" : "(degrees)";
    dipfld = new uiGenInput( this, dipstr, FloatInpSpec() );
    dipfld->attach( alignedBelow, dirfld );

    setHAlignObj( inpfld );
    mainObject()->finaliseStart.notify( mCB(this,uiSteeringSel,doFinalise) );
}


uiSteeringSel::~uiSteeringSel()
{
    delete ctio_.ioobj; delete &ctio_;
}


void uiSteeringSel::doFinalise(CallBacker*)
{
    typeSel(0);
}


void uiSteeringSel::typeSel( CallBacker* )
{
    if ( !typfld ) return; 

    int typ = typfld->getIntValue();
    inpfld->display( typ > 0 && typ < 3 );
    dirfld->display( typ == 3 && !is2d_ );
    dipfld->display( typ == 3 );
}


bool uiSteeringSel::willSteer() const
{
    return typfld && typfld->getIntValue() > 0;
}


void uiSteeringSel::setDesc( const Attrib::Desc* ad )
{
    if ( !typfld || !ad )
    {
	typfld->setValue( 0 );
	return;
    }

    setDescSet( ad->descSet() );

    int type = 0;
    BufferString attribname = ad->attribName();
    if ( attribname == "ConstantSteering" )
    {
	type = 3;
	const Attrib::Desc& desc = *ad;
	mIfGetFloat( "dip", dip, dipfld->setValue( dip ) );
	mIfGetFloat( "azi", azi, dirfld->setValue( azi ) );
    }
    else if ( attribname == "CentralSteering" )
    {
	type = 1;
	inpfld->setDesc( ad->getInput(0) );
	inpfld->updateHistory( inpselhist );
    }
    else if ( attribname == "FullSteering" )
    {
	type = 2;
	inpfld->setDesc( ad->getInput(0) );
	inpfld->updateHistory( inpselhist );
    }

    if ( !notypechange_ )
	typfld->setValue( type );
    typeSel( 0 );
}


void uiSteeringSel::setDescSet( const DescSet* ads )
{
    descset_ = ads;
    inpfld->setDescSet( ads );
    if ( !notypechange_ )
    {
	typfld->setValue( 0 );
	typeSel( 0 );
    }
}


DescID uiSteeringSel::descID()
{
    if ( !typfld ) return DescID::undef();

    const int type = typfld->getIntValue();
    if ( type==0 ) return DescID::undef();

    if ( type==3 )
    {
	const char* attribnm = "ConstantSteering";
	for ( int idx=0; idx<descset_->nrDescs(); idx++ )
	{
	    const DescID descid = descset_->getID( idx );
	    const Desc& desc = *descset_->getDesc( descid );
	    if ( strcmp(attribnm,desc.attribName()) )
		continue;

	    float dip, azi;
	    mGetFloat( dip, "dip" );
	    mGetFloat( azi, "azi" );
	    if ( mIsEqual(dip,dipfld->getfValue(),mDefEps) &&
		 mIsEqual(azi,dirfld->getfValue(),mDefEps) )
		return descid;
	}

	Desc* desc = PF().createDescCopy( attribnm );
	if ( !desc ) return DescID::undef();
	desc->getValParam("dip")->setValue( dipfld->getfValue() );
	desc->getValParam("azi")->setValue( dirfld->getfValue() );

	DescSet* ads = const_cast<DescSet*>(descset_);
	BufferString userref = attribnm;
	desc->setUserRef( userref );
	desc->setHidden(true);
	return ads->addDesc( desc );
    }

    inpfld->processInput();
    const DescID inldipid = inpfld->inlDipID();
    const DescID crldipid = inpfld->crlDipID();
    if ( inldipid < 0 && crldipid < 0 )
    {
	uiMSG().error( "Selected Steering input is not valid" );
	return DescID::undef();
    }

    BufferString attribnm( type==1 ? "CentralSteering" : "FullSteering" );
    for ( int idx=0; idx<descset_->nrDescs(); idx++ )
    {
	const DescID descid = descset_->getID( idx );
	const Desc* desc = descset_->getDesc( descid );
	if ( attribnm != desc->attribName() ) continue;

	if ( desc->getInput(0) && desc->getInput(0)->id() == inldipid &&
	     desc->getInput(1) && desc->getInput(1)->id() == crldipid )
	    return descid;
    }

    Desc* desc = PF().createDescCopy( attribnm );
    if ( !desc ) return DescID::undef();

    DescSet* ads = const_cast<DescSet*>(descset_);
    desc->setInput( 0, ads->getDesc(inldipid) );
    desc->setInput( 1, ads->getDesc(crldipid) );
    desc->setHidden( true );
    desc->setSteering( true );

    const ValParam* param = 
	ads->getDesc(inldipid)->getValParam( StorageProvider::keyStr() );
    BufferString userref = attribnm; userref += " ";
    userref += param->getStringValue();
    desc->setUserRef( userref );

    return ads->addDesc( desc );
}


void uiSteeringSel::setType( int nr, bool nochg )
{
    if ( !typfld ) return;
    typfld->setValue( nr );
    typfld->setSensitive( !nochg );
    notypechange_ = nochg;
    typeSel(0);
}


void uiSteeringSel::set2D( bool yn )
{
    if ( !inpfld ) return;

    is2d_ = yn;
    inpfld->set2DPol( is2d_ ? Only2D : No2D );
    typeSel(0);
}




static const char* steer_seltxts[] = { "Steering Data", 0 };


uiSteerCubeSel::uiSteerCubeSel( uiParent* p, CtxtIOObj& c, 
				const DescSet* ads, const char* txt ) 
	: uiSeisSel(p,getCtio(c),SeisSelSetup().selattr(false),false,
			steer_seltxts)
	, attrdata( ads )
{
}


CtxtIOObj& uiSteerCubeSel::getCtio( CtxtIOObj& c )
{
    c.ctxt.parconstraints.set( sKey::Type, sKey::Steering );
    c.ctxt.includeconstraints = true;
    c.ctxt.allowcnstrsabsent = false;
    c.ctxt.forread = true;
    return c;
}


void uiSteerCubeSel::updateAttrSet2D()
{
    set2DPol( !attrdata.attrset || !attrdata.attrset->nrDescs() ? Both2DAnd3D
	    : (attrdata.attrset->is2D() ? Only2D : No2D) );
}


DescID uiSteerCubeSel::getDipID( int dipnr ) const
{
    const DescSet* ads = attrdata.attrset;
    if ( !ads )
    {
	pErrMsg( "No attribdescset set");
	return DescID::undef();
    }

    if ( !ctio.ioobj ) 
	return DescID::undef();

    LineKey linekey( ctio.ioobj->key() );
    if ( is2D() ) linekey.setAttrName( sKey::Steering );

    for ( int idx=0; idx<ads->nrDescs(); idx++ )
    {
	const DescID descid = ads->getID( idx );
	const Desc* desc = ads->getDesc( descid );
	if ( !desc->isStored() || desc->selectedOutput()!=dipnr )
	    continue;

	const Param* keypar = desc->getParam( StorageProvider::keyStr() );
	BufferString res; keypar->getCompositeValue( res );
	if ( res == linekey )
	    return descid;
    }

    Desc* desc = PF().createDescCopy( StorageProvider::attribName() );
    desc->setHidden( true );
    desc->selectOutput( dipnr );
    ValParam* keypar = desc->getValParam( StorageProvider::keyStr() );
    keypar->setValue( linekey );

    BufferString userref = ctio.ioobj->name();
    userref += dipnr==0 ? "_inline_dip" : "_crline_dip";
    desc->setUserRef( userref );
    desc->updateParams();

    return const_cast<DescSet*>(ads)->addDesc( desc );
}


void uiSteerCubeSel::setDescSet( const DescSet* ads )
{
    attrdata.attrset = ads;
    updateAttrSet2D();
}


void uiSteerCubeSel::setDesc( const Desc* desc )
{
    if ( !desc || desc->selectedOutput() ) return;

    if ( !desc->isStored() || desc->dataType() != Seis::Dip ) return;

    const ValParam* keypar = desc->getValParam( StorageProvider::keyStr() );
    const MultiID mid( keypar->getStringValue() );
    PtrMan<IOObj> ioobj = IOM().get( mid );
    ctio.setObj( ioobj ? ioobj->clone() : 0 );
    updateInput();
}


void uiSteerCubeSel::fillSelSpec( SelSpec& as, bool inldip )
{
    as.set( 0, inldip ? inlDipID() : crlDipID(), false, "" );
    as.setRefFromID( *attrdata.attrset );
}
