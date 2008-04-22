/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Bert
 Date:          Apr 2008
 RCS:           $Id: uiwellattribxplot.cc,v 1.7 2008-04-22 16:20:39 cvsbert Exp $
________________________________________________________________________

-*/

#include "uiwellattribxplot.h"

#include "attribdesc.h"
#include "attribdescset.h"
#include "attribengman.h"
#include "attribsel.h"
#include "attribstorprovider.h"
#include "datacoldef.h"
#include "datapointset.h"
#include "executor.h"
#include "ioman.h"
#include "ioobj.h"
#include "iopar.h"
#include "survinfo.h"
#include "keystrs.h"
#include "posinfo.h"
#include "posvecdataset.h"
#include "posfilterset.h"
#include "seisioobjinfo.h"
#include "wellextractdata.h"
#include "wellmarker.h"

#include "mousecursor.h"
#include "uidatapointset.h"
#include "uiposfilterset.h"
#include "uigeninput.h"
#include "uilistbox.h"
#include "uicombobox.h"
#include "uimsg.h"
#include "uitaskrunner.h"

using namespace Attrib;

uiWellAttribCrossPlot::uiWellAttribCrossPlot( uiParent* p,
					      const Attrib::DescSet& d )
	: uiDialog(p,uiDialog::Setup("Attribute/Well cross-plotting",
		     "Select attributes and logs for cross-plot"
		     ,"107.3.1").modal(false))
	, ads_(*new Attrib::DescSet(d.is2D()))
    	, posfiltfld_(0)
{
    uiLabeledListBox* llba = new uiLabeledListBox( this, "Attributes", true );
    attrsfld_ = llba->box();
    uiLabeledListBox* llbw = new uiLabeledListBox( this, "Wells", true );
    wellsfld_ = llbw->box();
    llbw->attach( alignedBelow, llba );
    uiLabeledListBox* llbl = new uiLabeledListBox( this, "Logs", true,
						   uiLabeledListBox::RightTop );
    logsfld_ = llbl->box();
    llbl->attach( rightTo, llbw );

    const float inldist = SI().inlDistance();
    radiusfld_ = new uiGenInput( this, "Radius around wells",
	    			 FloatInpSpec((float)((int)(inldist+.5))) );
    radiusfld_->attach( alignedBelow, llbw );
    radiusfld_->attach( ensureBelow, llbl );

    uiGroup* attgrp = radiusfld_;
    if ( !ads_.is2D() )
    {
	uiPosFilterSet::Setup fsu( false );
	fsu.seltxt( "Filter positions" ).incprovs( true );
	posfiltfld_ = new uiPosFilterSetSel( this, fsu );
	posfiltfld_->attach( alignedBelow, radiusfld_ );
	attgrp = posfiltfld_;
    }

    uiLabeledComboBox* llc0 = new uiLabeledComboBox( this, "Extract between" );
    topmarkfld_ = llc0->box();
    llc0->attach( alignedBelow, attgrp );
    botmarkfld_ = new uiComboBox( this, "Bottom marker" );
    botmarkfld_->attach( rightOf, llc0 );
    BufferString txt = "Distance above/below ";
    txt += SI().depthsInFeetByDefault() ? "(ft)" : "(m)";
    abovefld_ = new uiGenInput( this, txt, FloatInpSpec(0) );
    abovefld_->attach( alignedBelow, llc0 );
    belowfld_ = new uiGenInput( this, "", FloatInpSpec(0) );
    belowfld_->attach( rightOf, abovefld_ );

    logresamplfld_ = new uiGenInput( this, "Log resampling method",
		  StringListInpSpec(Well::LogDataExtracter::SamplePolNames) );
    logresamplfld_->attach( alignedBelow, abovefld_ );

    setDescSet( d );
    finaliseDone.notify( mCB(this,uiWellAttribCrossPlot,initWin) );
}


uiWellAttribCrossPlot::~uiWellAttribCrossPlot()
{
    delete const_cast<Attrib::DescSet*>(&ads_);
    deepErase( wellobjs_ );
}


void uiWellAttribCrossPlot::initWin( CallBacker* )
{
    wellsfld_->empty(); logsfld_->empty();
    topmarkfld_->empty(); botmarkfld_->empty();

    Well::InfoCollector wic;
    uiTaskRunner tr( this );
    if ( !tr.execute(wic) ) return;

    BufferStringSet markernms, lognms;
    for ( int iid=0; iid<wic.ids().size(); iid++ )
    {
	IOObj* ioobj = IOM().get( *wic.ids()[iid] );
	if ( !ioobj ) continue;
	wellobjs_ += ioobj;
	wellsfld_->addItem( ioobj->name() );

	const BufferStringSet& logs = *wic.logs()[iid];
	for ( int ilog=0; ilog<logs.size(); ilog++ )
	    lognms.addIfNew( logs.get(ilog) );
	const ObjectSet<Well::Marker>& mrkrs = *wic.markers()[iid];
	for ( int imrk=0; imrk<mrkrs.size(); imrk++ )
	    markernms.addIfNew( mrkrs[imrk]->name() );
    }
    markernms.sort();
    markernms.insertAt( new BufferString(Well::TrackSampler::sKeyDataStart), 0);
    markernms.add( Well::TrackSampler::sKeyDataEnd );

    for ( int idx=0; idx<lognms.size(); idx++ )
	logsfld_->addItem( lognms.get(idx) );
    topmarkfld_->addItems( markernms );
    botmarkfld_->addItems( markernms );
    topmarkfld_->setCurrentItem( 0 );
    botmarkfld_->setCurrentItem( markernms.size() - 1 );
}


void uiWellAttribCrossPlot::setDescSet( const Attrib::DescSet& newads )
{
    IOPar iop; newads.fillPar( iop );
    Attrib::DescSet& ads = const_cast<Attrib::DescSet&>( ads_ );
    ads.removeAll(); ads.usePar( iop );
    adsChg();
}


void uiWellAttribCrossPlot::adsChg()
{
    attrsfld_->empty();

    Attrib::SelInfo attrinf( &ads_, 0, ads_.is2D() );
    for ( int idx=0; idx<attrinf.attrnms.size(); idx++ )
	attrsfld_->addItem( attrinf.attrnms.get(idx), false );
    
    for ( int idx=0; idx<attrinf.ioobjids.size(); idx++ )
    {
	BufferStringSet bss;
	SeisIOObjInfo sii( MultiID( attrinf.ioobjids.get(idx) ) );
	sii.getDefKeys( bss, true );
	for ( int inm=0; inm<bss.size(); inm++ )
	{
	    const char* defkey = bss.get(inm).buf();
	    const char* ioobjnm = attrinf.ioobjnms.get(idx).buf();
	    attrsfld_->addItem(
		    SeisIOObjInfo::defKey2DispName(defkey,ioobjnm) );
	}
    }
}


static void addDCDs( uiListBox* lb, ObjectSet<DataColDef>& dcds,
		     BufferStringSet& nms )
{
    for ( int idx=0; idx<lb->size(); idx++ )
    {
	if ( !lb->isSelected(idx) ) continue;
	const char* nm = lb->textOfItem(idx);
	nms.add( nm );
	dcds += new DataColDef( nm );
    }
}


#define mErrRet(s) { if ( s ) uiMSG().error(s); return false; }

bool uiWellAttribCrossPlot::extractWellData( const BufferStringSet& ioobjids,
					     const BufferStringSet& lognms,
					     ObjectSet<DataPointSet>& dpss )
{
    Well::TrackSampler wts( ioobjids, dpss );
    wts.for2d = ads_.is2D(); wts.lognms = lognms;
    wts.locradius = radiusfld_->getfValue();
    wts.topmrkr = topmarkfld_->text(); wts.botmrkr = botmarkfld_->text();
    wts.above = abovefld_->getfValue(0,0);
    wts.below = belowfld_->getfValue(0,0);
    uiTaskRunner tr( this );
    if ( !tr.execute(wts) )
	return false;
    if ( dpss.isEmpty() )
	mErrRet("No wells found")

    for ( int idx=0; idx<lognms.size(); idx++ )
    {
	Well::LogDataExtracter wlde( ioobjids, dpss );
	wlde.lognm = lognms.get(idx);
	wlde.samppol = (Well::LogDataExtracter::SamplePol)
	    				logresamplfld_->getIntValue();
	if ( !tr.execute(wlde) )
	    return false;
    }

    return true;
}


bool uiWellAttribCrossPlot::extractAttribData( DataPointSet& dps )
{
    IOPar descsetpars;
    ads_.fillPar( descsetpars );
    const_cast<PosVecDataSet*>( &(dps.dataSet()) )->pars() = descsetpars;

    MouseCursorManager::setOverride( MouseCursor::Wait );
    Attrib::EngineMan aem; BufferString errmsg;
    PtrMan<Executor> tabextr = aem.getTableExtractor( dps, ads_, errmsg );
    MouseCursorManager::restoreOverride();
    if ( !errmsg.isEmpty() )
	mErrRet(errmsg)
    uiTaskRunner tr( this );
    return tr.execute( *tabextr );
}


#define mDPM DPM(DataPackMgr::PointID)
#undef mErrRet
#define mErrRet(s) { deepErase(dcds); if ( s ) uiMSG().error(s); return false; }

bool uiWellAttribCrossPlot::acceptOK( CallBacker* )
{
    ObjectSet<DataColDef> dcds;
    BufferStringSet attrnms; addDCDs( attrsfld_, dcds,  attrnms );
    BufferStringSet lognms; addDCDs( logsfld_, dcds, lognms );
    if ( lognms.isEmpty() )
	mErrRet("Please select at least one log")

    BufferStringSet ioobjids;
    for ( int idx=0; idx<wellsfld_->size(); idx++ )
    {
	if ( wellsfld_->isSelected(idx) )
	    ioobjids.add( wellobjs_[idx]->key() );
    }
    if ( ioobjids.isEmpty() )
	mErrRet("Please select at least one well")

    ObjectSet<DataPointSet> dpss;
    if ( !extractWellData(ioobjids,lognms,dpss) )
	mErrRet(0)

    PtrMan<Pos::Filter> filt = 0;
    if ( posfiltfld_ )
    {
	IOPar iop; posfiltfld_->fillPar( iop );
	filt = Pos::Filter::make( iop, false );
	if ( filt )
	{
	    uiTaskRunner tr( this );
	    if ( !filt->initialize(&tr) )
		return false;
	}
    }


    MouseCursorManager::setOverride( MouseCursor::Wait );
    DataPointSet* dps = new DataPointSet( TypeSet<DataPointSet::DataRow>(),
	    				  dcds, ads_.is2D(), false );
    deepErase( dcds );
    const int nrattribs = attrnms.size();
    const int nrlogs = attrnms.size();
    DataPointSet::DataRow dr;
    for ( int idps=0; idps<dpss.size(); idps++ )
    {
	DataPointSet& curdps = *dpss[idps];
	for ( int idr=0; idr<curdps.size(); idr++ )
	{
	    dr = curdps.dataRow( idr );
	    if ( filt && !filt->includes(dr.pos_.coord()) )
		continue;

	    dr.data_.setSize( nrattribs + nrlogs, mUdf(float) );
	    for ( int ilog=0; ilog<nrlogs; ilog++ )
	    {
		const float val = dr.data_[ilog];
		dr.data_[ilog] = mUdf(float);
		dr.data_[nrattribs+ilog] = val;
	    }
	    dr.setGroup( (unsigned short)(idps+1) );
	    dps->setRow( dr );
	}
    }
    deepErase( dpss );
    dps->dataChanged();
    MouseCursorManager::restoreOverride();
    if ( dps->isEmpty() )
	mErrRet("No positions found matching criteria")

    BufferString dpsnm( "Well data" );
    if ( !attrnms.isEmpty() )
    {
	dpsnm += " / Attributes";
	if ( !extractAttribData(*dps) )
	    return false;
    }

    dps->setName( dpsnm );
    uiDataPointSet* dlg = new uiDataPointSet( this, *dps,
			uiDataPointSet::Setup("Attribute data") );
    return dlg->go() ? true : false;
}
