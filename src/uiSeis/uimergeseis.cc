/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          January 2002
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uimergeseis.h"

#include "bufstringset.h"
#include "seiscbvs.h"
#include "seismerge.h"
#include "seistrctr.h"
#include "ctxtioobj.h"
#include "ioman.h"
#include "iodir.h"
#include "ioobj.h"
#include "iopar.h"
#include "keystrs.h"

#include "uilistbox.h"
#include "uitaskrunner.h"
#include "uigeninput.h"
#include "uimsg.h"
#include "uiseissel.h"
#include "uiseisfmtscale.h"
#include "od_helpids.h"

#include <math.h>


uiMergeSeis::uiMergeSeis( uiParent* p )
    : uiDialog(p,uiDialog::Setup(tr("Seismic file merging"),
				 tr("Specify input/output seismics"),
				 mODHelpKey(mMergeSeisHelpID) ))
    , ctio_(*mMkCtxtIOObj(SeisTrc))
{
    const IODir iodir( ctio_.ctxt.getSelKey() );
    const ObjectSet<IOObj>& ioobjs = iodir.getObjs();
    BufferStringSet ioobjnms;
    for ( int idx=0; idx<ioobjs.size(); idx++ )
    {
	if ( ioobjs[idx]->isUserSelectable(true) &&
	     ioobjs[idx]->group() == "Seismic Data" )
	    ioobjnms.add( ioobjs[idx]->name() );
    }

    ioobjnms.sort();
    for ( int idx=0; idx<ioobjnms.size(); idx++ )
    {
	const char* nm = ioobjnms.get(idx).buf();
	const IOObj* ioobj = IOM().getLocal( nm,
					ctio_.ctxt.trgroup->userName() );
        ioobjids_ += new MultiID( ioobj ? (const char*)ioobj->key() : "" );
    }
    uiLabeledListBox* llb = new uiLabeledListBox( this, tr("Input Cubes"),
						    OD::ChooseZeroOrMore );
    inpfld_ = llb->box();
    inpfld_->setCurrentItem( 0 );
    inpfld_->addItems( ioobjnms );
    inpfld_->selectionChanged.notify( mCB(this,uiMergeSeis,selChangeCB) );

    stackfld_ = new uiGenInput( this, tr("Duplicate traces"),
				BoolInpSpec(true,tr("Stack"),tr("Use first")) );
    stackfld_->attach( alignedBelow, llb );

    scfmtfld_ = new uiSeisFmtScale( this, Seis::Vol, false, false );
    scfmtfld_->attach( alignedBelow, stackfld_ );

    ctio_.ctxt.forread = false;
    outfld_ = new uiSeisSel( this, ctio_, uiSeisSel::Setup(Seis::Vol) );
    outfld_->attach( alignedBelow, scfmtfld_ );
}


uiMergeSeis::~uiMergeSeis()
{
    delete ctio_.ioobj; delete &ctio_;
    deepErase( ioobjids_ );
}


bool uiMergeSeis::acceptOK( CallBacker* )
{
    ObjectSet<IOPar> inpars; IOPar outpar;
    if ( !getInput(inpars,outpar) )
	return false;

    SeisMerger mrgr( inpars, outpar, false );
    mrgr.stacktrcs_ = stackfld_->getBoolValue();
    mrgr.setScaler( scfmtfld_->getScaler() );
    uiTaskRunner dlg( this );
    return TaskRunner::execute( &dlg, mrgr );
}


void uiMergeSeis::selChangeCB( CallBacker* cb )
{
    for ( int idx=0; idx<inpfld_->size(); idx++ )
    {
        if ( !inpfld_->isChosen(idx) )
	    continue;

	PtrMan<IOObj> firstselobj = IOM().get( *ioobjids_[idx] );
	if ( !firstselobj ) continue;

	scfmtfld_->updateFrom( *firstselobj );
	break;
    }
}


bool uiMergeSeis::getInput( ObjectSet<IOPar>& inpars, IOPar& outpar )
{
    if ( !outfld_->commitInput() )
    {
	if ( outfld_->isEmpty() )
	    uiMSG().error( tr("Please enter an output Seismic data set name") );
        return false;
    }

    scfmtfld_->updateIOObj( ctio_.ioobj );
    outpar.set( sKey::ID(), ctio_.ioobj->key() );

    ObjectSet<IOObj> selobjs;
    for ( int idx=0; idx<inpfld_->size(); idx++ )
    {
        if ( inpfld_->isChosen(idx) )
            selobjs += IOM().get( *ioobjids_[idx] );
    }

    const int inpsz = selobjs.size();
    if ( inpsz < 2 )
    {
	uiMSG().error( tr("Please select at least 2 inputs") );
	deepErase( selobjs );
	return false;
    }

    static const char* optdirkey = "Optimized direction";
    BufferString type = "";
    BufferString optdir = "";
    for ( int idx=0; idx<inpsz; idx++ )
    {
	const IOObj& ioobj = *selobjs[idx];
	if ( idx == 0 )
	{
	    if ( ioobj.pars().hasKey(sKey::Type()) )
		type = ioobj.pars().find(sKey::Type());
	    if ( ioobj.pars().hasKey(optdirkey) )
		optdir = ioobj.pars().find( optdirkey );
	}
	IOPar* iop = new IOPar;
	iop->set( sKey::ID(), ioobj.key() );
	inpars += iop;
    }

    if ( type.isEmpty() )
	ctio_.ioobj->pars().removeWithKey( sKey::Type() );
    else
	ctio_.ioobj->pars().set( sKey::Type(), type );
    if ( optdir.isEmpty() )
	ctio_.ioobj->pars().removeWithKey( optdirkey );
    else
	ctio_.ioobj->pars().set( optdirkey, optdir );

    IOM().commitChanges( *ctio_.ioobj );

    deepErase( selobjs );
    return true;
}
