/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          February 2004
 RCS:           $Id: uiwellattribsel.cc,v 1.1 2004-03-01 14:29:43 nanne Exp $
________________________________________________________________________

-*/

#include "uiwellattribsel.h"
#include "nlamodel.h"
#include "ioman.h"
#include "ioobj.h"
#include "iopar.h"
#include "ptrman.h"
#include "position.h"
#include "executor.h"
#include "attribengman.h"
#include "attribdescset.h"
#include "attribsel.h"
#include "survinfo.h"

#include "welllog.h"
#include "welllogset.h"
#include "welltrack.h"
#include "welldata.h"
#include "welld2tmodel.h"

#include "uiattrsel.h"
#include "uiexecutor.h"
#include "uimsg.h"
#include "uigeninput.h"
#include "uiwellpartserv.h"


uiWellAttribSel::uiWellAttribSel( uiParent* p, Well::Data& wd_,
       				  const AttribDescSet& as,
       				  const NLAModel* mdl )
    : uiDialog(p,uiDialog::Setup("Attributes",
				 "Attribute selection",
				 "107.3.0"))
    , attrset(as)
    , nlamodel(mdl)
    , wd(wd_)
{
    attribfld = new uiAttrSel( this, &attrset );
    attribfld->setNLAModel( nlamodel );
    attribfld->selectiondone.notify( mCB(this,uiWellAttribSel,selDone) );

    bool zinft = SI().zInFeet();
    SI().pars().getYN( uiWellPartServer::unitstr, zinft );
    BufferString lbl = "Depth range"; lbl += zinft ? "(ft)" : "(m)";
    rangefld = new uiGenInput( this, lbl, FloatInpIntervalSpec(true) );
    rangefld->attach( alignedBelow, attribfld );
    setDefaultRange( zinft );

    lognmfld = new uiGenInput( this, "Log name" );
    lognmfld->attach( alignedBelow, rangefld );
}


uiWellAttribSel::~uiWellAttribSel()
{
}


void uiWellAttribSel::selDone( CallBacker* )
{
    const char* inputstr = attribfld->getInput();
    lognmfld->setText( inputstr );
}


void uiWellAttribSel::setDefaultRange( bool zinft )
{
    StepInterval<float> dahintv;
    for ( int idx=0; idx<wd.logs().size(); idx++ )
    {
	const Well::Log& log = wd.logs().getLog(idx);
	const int logsz = log.size();
	if ( !logsz ) continue;

	assign( dahintv, wd.logs().dahInterval() );
	const float width = log.dah(logsz-1) - log.dah(0);
	dahintv.step = width / (logsz-1);
	break;
    }

    if ( !dahintv.width() )
    {
	const Well::Track& track = wd.track();
	const int sz = track.size();
	dahintv.start = track.dah(0); dahintv.stop = track.dah(sz-1);
	dahintv.step = dahintv.width() / (sz-1);
    }

    if ( zinft )
    {
	dahintv.start /= 0.3048;
	dahintv.stop /= 0.3048;
	dahintv.step /= 0.3048;
    }
    rangefld->setValue( dahintv );
}


#define mErrRet(msg) { uiMSG().error(msg); return false; }

bool uiWellAttribSel::acceptOK( CallBacker* )
{
    bool zistime = SI().zIsTime();
    const Well::D2TModel* d2t = wd.d2TModel();
    if ( zistime && !d2t ) mErrRet( "No depth to time model defined" );
    
    TypeSet<BinIDZValues> positions;
    TypeSet<float> mdset;
    StepInterval<float> intv = rangefld->getFStepInterval();
    const int nrsteps = intv.nrSteps();
    for ( int idx=0; idx<nrsteps; idx++ )
    {
	float md = intv.atIndex( idx );
	if ( SI().zInFeet() ) md *= 0.3048;
	Coord3 pos = wd.track().getPos( md );
	BinID bid = SI().transform( pos );
	if ( !bid.inl && !bid.crl ) continue;

	if ( zistime )
	    pos.z = d2t->getTime( md );
	mdset += md;
	positions += BinIDZValues( bid, pos.z );
    }

    AttribSelSpec selspec;
    attribfld->fillSelSpec( selspec );
    AttribEngMan aem;
    aem.setAttribSet( &attrset );
    aem.setNLAModel( nlamodel );
    aem.setAttribSpec( selspec );

    BufferString errmsg;
    ObjectSet< TypeSet<BinIDZValues> > posset;
    posset += &positions;
    PtrMan<Executor> exec = aem.tableOutputCreator( errmsg, posset );
    uiExecutor uiexec( this, *exec );
    bool ret = uiexec.go();
    if ( !ret ) return false;

    const char* lognm = lognmfld->text();
    Well::Log* newlog = new Well::Log( lognm );
    for ( int idx=0; idx<mdset.size(); idx++ )
    {
	TypeSet<float>& vals = positions[idx].values;
	newlog->addValue( mdset[idx], vals.size() ? vals[0] : mUndefValue );
    }

    wd.logs().add( newlog );
    return ret;
}
