/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          May 2002
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";


#include "uiseis2dfileman.h"
#include "uiseispsman.h"

#include "file.h"
#include "filepath.h"
#include "iopar.h"
#include "keystrs.h"
#include "seis2ddata.h"
#include "seiscbvs.h"
#include "seiscbvs2d.h"
#include "seis2dlinemerge.h"
#include "seiscube2linedata.h"
#include "survinfo.h"
#include "survgeom2d.h"
#include "posinfo2dsurv.h"
#include "zdomain.h"
#include "linesetposinfo.h"

#include "uitoolbutton.h"
#include "uigeninputdlg.h"
#include "uiioobjmanip.h"
#include "uiioobjsel.h"
#include "uilistbox.h"
#include "uicombobox.h"
#include "uigeninput.h"
#include "uimsg.h"
#include "uiseis2dfrom3d.h"
#include "uiseisioobjinfo.h"
#include "uiseisbrowser.h"
#include "uiseissel.h"
#include "uisplitter.h"
#include "uitextedit.h"
#include "uitaskrunner.h"
#include "od_helpids.h"

mDefineInstanceCreatedNotifierAccess(uiSeis2DFileMan)


uiSeis2DFileMan::uiSeis2DFileMan( uiParent* p, const IOObj& ioobj )
    : uiDialog(p,uiDialog::Setup(tr("Manage 2D Seismic Lines"),mNoDlgTitle,
				 mODHelpKey(mSeis2DManHelpID) ))
    , issidomain(ZDomain::isSI( ioobj.pars() ))
    , zistm((SI().zIsTime() && issidomain) || (!SI().zIsTime() && !issidomain))
{
    setCtrlStyle( CloseOnly );

    objinfo_ = new uiSeisIOObjInfo( ioobj );
    dataset_ = new Seis2DDataSet( ioobj );

    uiGroup* topgrp = new uiGroup( this, "Top" );
    uiLabeledListBox* lllb = new uiLabeledListBox( topgrp, tr("2D lines"),
			    OD::ChooseAtLeastOne, uiLabeledListBox::AboveMid );
    linefld_ = lllb->box();
    linefld_->selectionChanged.notify( mCB(this,uiSeis2DFileMan,lineSel) );

    linegrp_ = new uiManipButGrp( lllb );
    linegrp_->addButton( uiManipButGrp::Rename, "Rename line",
			mCB(this,uiSeis2DFileMan,renameLine) );
    linegrp_->addButton( uiManipButGrp::Remove, "Remove line",
			mCB(this,uiSeis2DFileMan,removeLine) );
    linegrp_->addButton( "mergelines", "Merge lines",
			mCB(this,uiSeis2DFileMan,mergeLines) );
    linegrp_->addButton( "browseseis", "Browse/edit this line",
	    	        mCB(this,uiSeis2DFileMan,browsePush) );
    if ( SI().has3D() )
	linegrp_->addButton( "extr3dinto2d", "Extract from 3D cube",
			mCB(this,uiSeis2DFileMan,extrFrom3D) );
    linegrp_->attach( rightOf, linefld_ );

    uiGroup* botgrp = new uiGroup( this, "Bottom" );
    infofld_ = new uiTextEdit( botgrp, "File Info", true );
    infofld_->setPrefHeightInChar( 8 );
    infofld_->setPrefWidthInChar( 50 );

    uiSplitter* splitter = new uiSplitter( this, "Splitter", false );
    splitter->addGroup( topgrp );
    splitter->addGroup( botgrp );

    fillLineBox();

    mTriggerInstanceCreatedNotifier();
    lineSel(0);
}


uiSeis2DFileMan::~uiSeis2DFileMan()
{
    delete objinfo_;
    delete dataset_;
}


void uiSeis2DFileMan::fillLineBox()
{
    uiListBox* lb = linefld_;
    const int curitm = lb->size() ? lb->currentItem() : 0;
    BufferStringSet linenames;
    objinfo_->ioObjInfo().getLineNames( linenames );
    lb->setEmpty();
    lb->addItems( linenames );
    lb->setCurrentItem( curitm );
}


void uiSeis2DFileMan::lineSel( CallBacker* )
{
    infofld_->setText( "" );
    BufferStringSet linenms;
    linefld_->getChosen( linenms );
    BufferString txt;
    for ( int idx=0; idx<linenms.size(); idx++ )
    {
	const Pos::GeomID geomid = Survey::GM().getGeomID( linenms.get(idx) );
	const int lineidx = dataset_->indexOf( geomid );
	if ( lineidx < 0 ) { pErrMsg("Huh"); continue; }

	StepInterval<int> trcrg;
	StepInterval<float> zrg;
	const bool hasrg = dataset_->getRanges( lineidx, trcrg, zrg );

	PosInfo::Line2DData l2dd;
	const Survey::Geometry* geom = Survey::GM().getGeometry( geomid );
	mDynamicCastGet( const Survey::Geometry2D*, geom2d, geom )
	if ( geom2d )
	    l2dd = geom2d->data();
	if ( !geom2d || l2dd.isEmpty() )
	{
	    txt += ( "\nCannot find geometry for line: " );
	    txt += linenms.get(idx);
	    continue;
	}

	const int sz = trcrg.nrSteps() + 1;
	PosInfo::Line2DPos firstpos, lastpos;
	l2dd.getPos( trcrg.start, firstpos );
	l2dd.getPos( trcrg.stop, lastpos );

	if ( hasrg )
	{
	    if ( idx > 0 )
		txt += "\n\n";
	    txt += "Details for line: "; txt += linenms.get(idx);
	    txt += "\nNumber of traces: "; txt += sz;
	    txt += "\nFirst trace: ";
	    if ( l2dd.getPos(trcrg.start,firstpos) )
		txt.add( firstpos.nr_ )
		   .add( " " ).add( firstpos.coord_.toString() );
	    txt += "\nLast trace: ";
	    if ( l2dd.getPos(trcrg.stop,lastpos) )
		txt.add( lastpos.nr_ )
		   .add( " " ).add( lastpos.coord_.toString() );

#define mAddZRangeTxt(memb) txt += zistm ? mNINT32(1000*memb) : memb
	    txt += "\nZ-range: "; mAddZRangeTxt(zrg.start); txt += " - ";
	    mAddZRangeTxt(zrg.stop);
	    txt += " ["; mAddZRangeTxt(zrg.step); txt += "]";
	}
	else
	{
	    txt += "\nCannot read ranges for line: "; txt += linenms.get(idx);
	    txt += "\nCBVS file might be corrupt or missing.\n";
	}

	SeisIOObjInfo sobinf( objinfo_->ioObj() );
	const int nrcomp = sobinf.nrComponents( geomid );
	if ( nrcomp > 1 )
	    { txt += "\nNumber of components: "; txt += nrcomp; }

	const IOPar& iopar = dataset_->getInfo( lineidx );
	BufferString fname( SeisCBVS2DLineIOProvider::getFileName(iopar) );
	FilePath fp( fname );

	txt += "\nLocation: "; txt += fp.pathOnly();
	txt += "\nFile name: "; txt += fp.fileName();
	txt += "\nFile size: ";
	txt += uiObjFileMan::getFileSizeString( File::getKbSize(fname) );
	const char* timestr = File::timeLastModified( fname );
	if ( timestr ) { txt += "\nLast modified: "; txt += timestr; }
    }

    infofld_->setText( txt );
}


void uiSeis2DFileMan::removeLine( CallBacker* )
{
    BufferStringSet sellines;
    linefld_->getChosen( sellines );
    if ( sellines.isEmpty() ||
	!uiMSG().askRemove(tr("All selected lines "
	    "will be removed. Do you want to continue?")) )
	return;

    for ( int idx=0; idx<sellines.size(); idx++ )
    {
	const char* linenm = sellines.get(idx);
        dataset_->remove( Survey::GM().getGeomID(linenm) );
    }
}


bool uiSeis2DFileMan::rename( const char* oldnm, BufferString& newnm )
{
    BufferString titl( "Rename '" );
    titl += oldnm; titl += "'";
    uiGenInputDlg dlg( this, titl, "New name", new StringInpSpec(oldnm) );
    if ( !dlg.go() ) return false;
    newnm = dlg.text();
    return newnm != oldnm;
}


void uiSeis2DFileMan::renameLine( CallBacker* )
{
    BufferStringSet linenms;
    linefld_->getChosen( linenms );
    if ( linenms.isEmpty() ) return;

    const char* linenm = linenms.get(0);
    BufferString newnm;
    if ( !rename(linenm,newnm) ) return;

    if ( linefld_->isPresent(newnm) )
    {
	uiMSG().error( tr("Linename already in use") );
	return;
    }

    /*if ( !dataset_->renameLine( linenm, newnm ) )
    {
	uiMSG().error( "Could not rename line" );
	return;
    }*/

    fillLineBox();
}


void uiSeis2DFileMan::browsePush( CallBacker* )
{
    if ( !objinfo_ || !objinfo_->ioObj() ) return;

    const LineKey lk( linefld_->getText() );
    uiSeisBrowser::doBrowse( this, *objinfo_->ioObj(), true, &lk );
}


class uiSeis2DFileManMergeDlg : public uiDialog
{
public:

uiSeis2DFileManMergeDlg( uiParent* p, const uiSeisIOObjInfo& objinf,
			 const BufferStringSet& sellns )
    : uiDialog(p,Setup("Merge lines","Merge two lines into a new one",
		       mODHelpKey(mSeis2DFileManMergeDlgHelpID) ) )
    , objinf_(objinf)
{
    BufferStringSet lnms; objinf_.ioObjInfo().getLineNames( lnms );
    uiLabeledComboBox* lcb1 = new uiLabeledComboBox( this, lnms, "First line" );
    uiLabeledComboBox* lcb2 = new uiLabeledComboBox( this, lnms, 
                                                     uiStrings::sAdd(true) );
    lcb2->attach( alignedBelow, lcb1 );
    ln1fld_ = lcb1->box(); ln2fld_ = lcb2->box();
    ln1fld_->setCurrentItem( sellns.get(0) );
    ln2fld_->setCurrentItem( sellns.get(1) );

    const char* mrgopts[]
	= { "Match trace numbers", "Match coordinates", "Bluntly append", 0 };
    mrgoptfld_ = new uiGenInput( this, "Merge method",
				 StringListInpSpec(mrgopts) );
    mrgoptfld_->attach( alignedBelow, lcb2 );
    mrgoptfld_->valuechanged.notify( mCB(this,uiSeis2DFileManMergeDlg,optSel) );
    stckfld_ = new uiGenInput( this, "Duplicate positions",
				BoolInpSpec(true,"Stack","Use first") );
    stckfld_->attach( alignedBelow, mrgoptfld_ );
    renumbfld_ = new uiGenInput( this, "Renumber; Start/step numbers",
				 IntInpSpec(1), IntInpSpec(1) );
    renumbfld_->setWithCheck( true );
    renumbfld_->setChecked( true );
    renumbfld_->attach( alignedBelow, stckfld_ );
    double defsd = SI().crlDistance() / 2;
    if ( SI().xyInFeet() ) defsd *= mToFeetFactorD;
    snapdistfld_ = new uiGenInput( this, "Snap distance", DoubleInpSpec(defsd));
    snapdistfld_->attach( alignedBelow, renumbfld_ );

    outfld_ = new uiGenInput( this, "New line name", StringInpSpec() );
    outfld_->attach( alignedBelow, snapdistfld_ );

    postFinalise().notify( mCB(this,uiSeis2DFileManMergeDlg,initWin) );
}

void initWin( CallBacker* )
{
    optSel(0);
    renumbfld_->valuechanged.notify( mCB(this,uiSeis2DFileManMergeDlg,optSel) );
    renumbfld_->checked.notify( mCB(this,uiSeis2DFileManMergeDlg,optSel) );
}

void optSel( CallBacker* )
{
    const int opt = mrgoptfld_->getIntValue();
    stckfld_->display( opt < 2 );
    renumbfld_->display( opt > 0 );
    snapdistfld_->display( opt == 1 );
}

#define mErrRet(s) { uiMSG().error(s); return false; }
bool acceptOK( CallBacker* )
{
    const char* outnm = outfld_->text();
    if ( !outnm || !*outnm )
	mErrRet( "Please enter a name for the merged line" );

    BufferStringSet lnms; objinf_.ioObjInfo().getLineNames( lnms );
    if ( lnms.isPresent( outnm ) )
	mErrRet( "Output line name already in Line Set" );

    Seis2DLineMerger lmrgr( objinf_.ioObj()->key() );
    lmrgr.lnm1_ = ln1fld_->text();
    lmrgr.lnm2_ = ln2fld_->text();
    if ( lmrgr.lnm1_ == lmrgr.lnm2_ )
	mErrRet( "Respectfully refusing to merge a line with itself" );

    lmrgr.outlnm_ = outnm;
    lmrgr.opt_ = (Seis2DLineMerger::Opt)mrgoptfld_->getIntValue();
    lmrgr.stckdupl_ = stckfld_->getBoolValue();
    lmrgr.renumber_ = lmrgr.opt_ != Seis2DLineMerger::MatchTrcNr
		   && renumbfld_->isChecked();

    if ( lmrgr.renumber_ )
    {
	lmrgr.numbering_.start = renumbfld_->getIntValue(0);
	lmrgr.numbering_.step = renumbfld_->getIntValue(1);
    }

    if ( lmrgr.opt_ == Seis2DLineMerger::MatchCoords )
    {
	lmrgr.snapdist_ = snapdistfld_->getdValue();
	if ( mIsUdf(lmrgr.snapdist_) || lmrgr.snapdist_ < 0 )
	    mErrRet( "Please specify a valid snap distance" );
    }

    uiTaskRunner taskrunner( this );
    TaskRunner::execute( &taskrunner, lmrgr );
    // return taskrunner.execute( lmrgr );
    return false;
}

    const uiSeisIOObjInfo&	objinf_;

    uiComboBox*			ln1fld_;
    uiComboBox*			ln2fld_;
    uiGenInput*			mrgoptfld_;
    uiGenInput*			stckfld_;
    uiGenInput*			renumbfld_;
    uiGenInput*			snapdistfld_;
    uiGenInput*			outfld_;

};


void uiSeis2DFileMan::redoAllLists()
{
    const MultiID lsid( objinfo_->ioObj()->key() );
    delete objinfo_;
    objinfo_ = new uiSeisIOObjInfo( lsid );
    if ( objinfo_->isOK() )
    {
	delete dataset_;
	dataset_ = new Seis2DDataSet( *(objinfo_->ioObj()) );
    }
    fillLineBox();
}


void uiSeis2DFileMan::mergeLines( CallBacker* )
{
    if ( linefld_->size() < 2 ) return;

    BufferStringSet sellnms; int firstsel = -1;
    for ( int idx=0; idx<linefld_->size(); idx++ )
    {
	if ( linefld_->isChosen(idx) )
	{
	    sellnms.add( linefld_->textOfItem(idx) );
	    if ( firstsel < 0 ) firstsel = idx;
	    if ( sellnms.size() > 1 ) break;
	}
    }
    if ( firstsel < 0 )
	{ firstsel = 0; sellnms.add( linefld_->textOfItem(0) ); }
    if ( sellnms.size() == 1 )
    {
	if ( firstsel >= linefld_->size() )
	    firstsel = -1;
	sellnms.add( linefld_->textOfItem(firstsel+1) );
    }

    uiSeis2DFileManMergeDlg dlg( this, *objinfo_, sellnms );
    if ( dlg.go() )
	redoAllLists();
}


void uiSeis2DFileMan::extrFrom3D( CallBacker* )
{
    uiSeis2DFrom3D dlg( this );
    if ( dlg.go() )
	redoAllLists();
}
