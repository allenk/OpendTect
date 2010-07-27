/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		September 2008
________________________________________________________________________

-*/
static const char* rcsID = "$Id: visfaultsticksetdisplay.cc,v 1.28 2010-07-27 09:01:24 cvsjaap Exp $";

#include "visfaultsticksetdisplay.h"

#include "emfaultstickset.h"
#include "emmanager.h"
#include "executor.h"
#include "faultstickseteditor.h"
#include "iopar.h"
#include "mpeengine.h"
#include "survinfo.h"
#include "undo.h"
#include "viscoord.h"
#include "visevent.h"
#include "vismarker.h"
#include "vismaterial.h"
#include "vismpeeditor.h"
#include "vispickstyle.h"
#include "visplanedatadisplay.h"
#include "vispolygonselection.h"
#include "vispolyline.h"
#include "visseis2ddisplay.h"
#include "vistransform.h"

mCreateFactoryEntry( visSurvey::FaultStickSetDisplay );

namespace visSurvey
{

FaultStickSetDisplay::FaultStickSetDisplay()
    : VisualObjectImpl(true)
    , emfss_(0)
    , eventcatcher_(0)
    , displaytransform_(0)
    , colorchange(this)
    , displaymodechange(this)
    , viseditor_(0)
    , fsseditor_(0)
    , activesticknr_( mUdf(int) )
    , sticks_( visBase::IndexedPolyLine3D::create() )
    , activestick_( visBase::IndexedPolyLine3D::create() )
    , showmanipulator_( false )
    , activestickpickstyle_( visBase::PickStyle::create() )
    , stickspickstyle_( visBase::PickStyle::create() )
    , displayonlyatsections_( false )
    , stickselectmode_( false )
{
    activestickpickstyle_->ref();
    activestickpickstyle_->setStyle( visBase::PickStyle::Unpickable );
    stickspickstyle_->ref();
    stickspickstyle_->setStyle( visBase::PickStyle::Unpickable );
    
    sticks_->ref();
    sticks_->setMaterial( 0 );
    sticks_->setRadius( 1, true );
    addChild( sticks_->getInventorNode() );
    activestick_->ref();
    activestick_->setMaterial( 0 );
    activestick_->setRadius( 1.5, true );
    addChild( activestick_->getInventorNode() );
    getMaterial()->setAmbience( 0.2 );

    sticks_->insertNode( stickspickstyle_->getInventorNode() );
    activestick_->insertNode( activestickpickstyle_->getInventorNode() );

    for ( int idx=0; idx<2; idx++ )
    {
	visBase::DataObjectGroup* group=visBase::DataObjectGroup::create();
	group->ref();
	addChild( group->getInventorNode() );
	knotmarkers_ += group;
	visBase::Material* knotmat = visBase::Material::create();
	group->addObject( knotmat );
	if ( idx )
	    knotmat->setColor( Color(0,255,0) );
	else
	    knotmat->setColor( Color(255,255,255) );
    }
}


FaultStickSetDisplay::~FaultStickSetDisplay()
{
    setSceneEventCatcher( 0 );

    if ( viseditor_ )
	viseditor_->unRef();
    if ( emfss_ )
	MPE::engine().removeEditor( emfss_->id() );
    if ( fsseditor_ )
	fsseditor_->unRef();
    fsseditor_ = 0;

    if ( emfss_ )
    {
	emfss_->change.remove( mCB(this,FaultStickSetDisplay,emChangeCB) );
       	emfss_->unRef();
	emfss_ = 0;
    }

    if ( displaytransform_ ) displaytransform_->unRef();

    sticks_->unRef();
    activestick_->unRef();
    stickspickstyle_->unRef();
    activestickpickstyle_->unRef();

    for ( int idx=knotmarkers_.size()-1; idx>=0; idx-- )
    {
	removeChild( knotmarkers_[idx]->getInventorNode() );
	knotmarkers_[idx]->unRef();
	knotmarkers_.remove( idx );
    }
}


void FaultStickSetDisplay::setSceneEventCatcher( visBase::EventCatcher* vec )
{
    if ( eventcatcher_ )
    {
	eventcatcher_->eventhappened.remove(
				    mCB(this,FaultStickSetDisplay,mouseCB) );
	eventcatcher_->unRef();
    }

    eventcatcher_ = vec;
    
    if ( eventcatcher_ )
    {
	eventcatcher_->ref();
	eventcatcher_->eventhappened.notify(
				    mCB(this,FaultStickSetDisplay,mouseCB) );
    }

    if ( viseditor_ )
	viseditor_->setSceneEventCatcher( eventcatcher_ );
}


EM::ObjectID FaultStickSetDisplay::getEMID() const
{ return emfss_ ? emfss_->id() : -1; }


#define mErrRet(s) { errmsg_ = s; return false; }

bool FaultStickSetDisplay::setEMID( const EM::ObjectID& emid )
{
    if ( emfss_ )
    {
	emfss_->change.remove( mCB(this,FaultStickSetDisplay,emChangeCB) );
	emfss_->unRef();
    }

    emfss_ = 0;
    if ( fsseditor_ )
    {
	fsseditor_->setEditIDs( 0 );
	fsseditor_->unRef();
    }
    fsseditor_ = 0;
    if ( viseditor_ )
	viseditor_->setEditor( (MPE::ObjectEditor*) 0 );

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::FaultStickSet*,emfss,emobject.ptr());
    if ( !emfss )
	return false;

    emfss_ = emfss;
    emfss_->change.notify( mCB(this,FaultStickSetDisplay,emChangeCB) );
    emfss_->ref();

    if ( !emfss_->name().isEmpty() )
	setName( emfss_->name() );

    if ( !viseditor_ )
    {
	viseditor_ = MPEEditor::create();
	viseditor_->ref();
	viseditor_->setSceneEventCatcher( eventcatcher_ );
	viseditor_->setDisplayTransformation( displaytransform_ );
	viseditor_->sower().alternateSowingOrder();
	insertChild( childIndex(sticks_->getInventorNode()),
		     viseditor_->getInventorNode() );
    }
    RefMan<MPE::ObjectEditor> editor = MPE::engine().getEditor( emid, true );
    mDynamicCastGet( MPE::FaultStickSetEditor*, fsseditor, editor.ptr() );
    fsseditor_ =  fsseditor;
    if ( fsseditor_ )
    {
	fsseditor_->ref();
	fsseditor_->setEditIDs( &editpids_ );
    }


    viseditor_->setEditor( fsseditor_ );

    getMaterial()->setColor( emfss_->preferredColor() );
    viseditor_->setMarkerSize(3);

    updateSticks();
    updateKnotMarkers();
    return true;
}


MultiID FaultStickSetDisplay::getMultiID() const
{
    return emfss_ ? emfss_->multiID() : MultiID();
}


void FaultStickSetDisplay::setColor( Color nc )
{
    if ( emfss_ )
	emfss_->setPreferredColor( nc );
    else
	getMaterial()->setColor( nc );

    colorchange.trigger();
}


NotifierAccess* FaultStickSetDisplay::materialChange()
{ return &getMaterial()->change; }


Color FaultStickSetDisplay::getColor() const
{ return getMaterial()->getColor(); }


void FaultStickSetDisplay::setDisplayTransformation(
						visBase::Transformation* nt )
{
    if ( viseditor_ ) viseditor_->setDisplayTransformation( nt );

    sticks_->setDisplayTransformation( nt );
    activestick_->setDisplayTransformation( nt );

    for ( int idx=0; idx<knotmarkers_.size(); idx++ )
	knotmarkers_[idx]->setDisplayTransformation( nt );

    if ( displaytransform_ ) displaytransform_->unRef();
    displaytransform_ = nt;
    if ( displaytransform_ ) displaytransform_->ref();
}


visBase::Transformation* FaultStickSetDisplay::getDisplayTransformation()
{ return displaytransform_; }


void FaultStickSetDisplay::updateEditPids()
{
    if ( !emfss_ || (viseditor_ && viseditor_->sower().moreToSow()) )
	return;

    editpids_.erase();

    for ( int sidx=0; !stickselectmode_ && sidx<emfss_->nrSections(); sidx++ )
    {
	int sid = emfss_->sectionID( sidx );
	mDynamicCastGet( const Geometry::FaultStickSet*, fss,
			 emfss_->sectionGeometry( sid ) );
	if ( fss->isEmpty() )
	    continue;

	RowCol rc;
	const StepInterval<int> rowrg = fss->rowRange();
	for ( rc.row=rowrg.start; rc.row<=rowrg.stop; rc.row+=rowrg.step )
	{
	    if ( fss->isStickHidden(rc.row) )
		continue;

	    const StepInterval<int> colrg = fss->colRange( rc.row );
	    for ( rc.col=colrg.start; rc.col<=colrg.stop; rc.col+=colrg.step )
	    {
		editpids_ += EM::PosID( emfss_->id(), sid, rc.toInt64() );
	    }
	}
    }
    if ( fsseditor_ )
	fsseditor_->editpositionchange.trigger();
}


void FaultStickSetDisplay::updateSticks( bool activeonly )
{
    if ( !emfss_ || (viseditor_ && viseditor_->sower().moreToSow()) )
	return;

    visBase::IndexedPolyLine3D* poly = activeonly ? activestick_ : sticks_;

    poly->removeCoordIndexAfter(-1);
    poly->getCoordinates()->removeAfter(-1);
    int cii = 0;

    for ( int sidx=0; sidx<emfss_->nrSections(); sidx++ )
    {
	int sid = emfss_->sectionID( sidx );
	mDynamicCastGet( const Geometry::FaultStickSet*, fss,
			 emfss_->sectionGeometry( sid ) );
	if ( fss->isEmpty() )
	    continue;

	RowCol rc;
	const StepInterval<int> rowrg = fss->rowRange();
	for ( rc.row=rowrg.start; rc.row<=rowrg.stop; rc.row+=rowrg.step )
	{
	    if ( activeonly && rc.row!=activesticknr_ )
		continue;

	    if ( fss->isStickHidden(rc.row) )
		continue;

	    Seis2DDisplay* s2dd = 0;
	    if ( emfss_->geometry().pickedOn2DLine(sid,rc.row) )
	    { 
		const MultiID* lset = emfss_->geometry().lineSet( sid, rc.row );
		const char* lnm = emfss_->geometry().lineName( sid, rc.row );
		s2dd = Seis2DDisplay::getSeis2DDisplay( *lset, lnm );
	    }

	    const StepInterval<int> colrg = fss->colRange( rc.row );
	    if ( !colrg.width() )
	    {
		rc.col = colrg.start;
		for ( int dir=-1; dir<=1; dir+=2 )
		{
		    Coord3 pos = fss->getKnot( rc );
		    pos.x += SI().inlDistance() * 0.5 * dir;
		    const int ci = poly->getCoordinates()->addPos( pos );
		    poly->setCoordIndex( cii++, ci );
		}
		poly->setCoordIndex( cii++, -1 );

		for ( int dir=-1; dir<=1; dir+=2 )
		{
		    Coord3 pos = fss->getKnot( rc );
		    pos.y += SI().inlDistance() * 0.5 * dir;
		    const int ci = poly->getCoordinates()->addPos( pos );
		    poly->setCoordIndex( cii++, ci );
		}
		poly->setCoordIndex( cii++, -1 );

		for ( int dir=-1; dir<=1; dir+=2 )
		{
		    Coord3 pos = fss->getKnot( rc );
		    pos.z += SI().zStep() * 0.5 * dir;
		    const int ci = poly->getCoordinates()->addPos( pos );
		    poly->setCoordIndex( cii++, ci );
		}
		poly->setCoordIndex( cii++, -1 );
		continue;
	    }

	    for ( rc.col=colrg.start; rc.col<=colrg.stop; rc.col+=colrg.step )
	    {
		const Coord3 pos1 = fss->getKnot( rc );
		int ci = poly->getCoordinates()->addPos( pos1 );
		poly->setCoordIndex( cii++, ci );
		if ( !s2dd || rc.col==colrg.stop )
		    continue;

		RowCol nextrc = rc;
		nextrc.col += colrg.step;
		const Coord3 pos2 = fss->getKnot( nextrc );
		int trc11, trc12, trc21, trc22;
		float dummy;
		if ( s2dd->getNearestSegment(pos1,true,trc11,trc12,dummy) < 0 )
		    continue;
		if ( s2dd->getNearestSegment(pos2,true,trc21,trc22,dummy) < 0 )
		    continue;

		const int dir = trc11<=trc21 ? 1 : -1;
		const int trcnr1 = dir>0 ? trc12 : trc11;
		const int trcnr2 = dir>0 ? trc22 : trc21;

		double totarclen = 0.0;
		Coord prevpos = pos1;
		for ( int trcnr=trcnr1; dir*trcnr<=dir*trcnr2; trcnr+=dir )
		{
		    const Coord curpos = trcnr==trcnr2 ? (Coord) pos2
						       : s2dd->getCoord(trcnr);
		    if ( !curpos.isDefined() )
			continue;
		    totarclen += prevpos.distTo( curpos );
		    prevpos = curpos;
		}

		prevpos = pos1;
		int prevnr = trcnr1;
		Coord curpos = prevpos;
		int curnr = prevnr;
		double arclen = 0.0;
		double partarclen = 0.0;

		for ( int trcnr=trcnr1; dir*trcnr<=dir*trcnr2; trcnr+=dir )
		{
		    const Coord nextpos = trcnr==trcnr2 ? (Coord) pos2
							: s2dd->getCoord(trcnr);
		    if ( !nextpos.isDefined() )
			continue;

		    const double dist = curpos.distTo( nextpos );
		    partarclen += dist;
		    arclen += dist;
		    const double basedist = prevpos.distTo(nextpos);
		    if ( partarclen-basedist>0.001*partarclen ) 
		    {
			double maxperpdev = 0.0;
			partarclen = dist;
			for ( int backtrcnr=trcnr-dir;
			      dir*backtrcnr>dir*prevnr; backtrcnr-=dir )
			{
			    const Coord backpos = s2dd->getCoord( backtrcnr );
			    if ( !backpos.isDefined() )
				continue;

			    const double prevdist = backpos.distTo( prevpos );
			    const double nextdist = backpos.distTo( nextpos );
			    const double sp = (prevdist+nextdist+basedist) / 2;
			    const double dev = (sp-prevdist)*(sp-nextdist)*
					       (sp-basedist)*sp;

			    if ( dev < maxperpdev )
				break;

			    maxperpdev = dev;
			    partarclen += backpos.distTo( curpos ); 
			    curpos = backpos;
			    curnr = backtrcnr;
			}

			const double frac = (arclen-partarclen) / totarclen;
			if ( frac>mDefEps && 1.0-frac>mDefEps )
			{
			    const Coord3 pos( curpos,
					      (1-frac)*pos1.z+frac*pos2.z );
			    ci = poly->getCoordinates()->addPos( pos );
			    poly->setCoordIndex( cii++, ci );
			}

			prevpos = curpos;
			prevnr = curnr;
		    }
		    curpos = nextpos;
		    curnr = trcnr;
		}
	    }
	    poly->setCoordIndex( cii++, -1 );
	}
    }
    if ( !activeonly )
	updateSticks( true );
}


Coord3 FaultStickSetDisplay::disp2world( const Coord3& displaypos ) const
{
    Coord3 pos = displaypos;  
    if ( pos.isDefined() )
    {
	if ( scene_ )
	    pos = scene_->getZScaleTransform()->transformBack( pos ); 
	if ( displaytransform_ )
	    pos = displaytransform_->transformBack( pos ); 
    }
    return pos;
}

#define mZScale() \
    ( scene_ ? scene_->getZScale()*scene_->getZStretch() : SI().zScale() )

#define mSetUserInteractionEnd() \
    if ( !viseditor_->sower().moreToSow() ) \
	EM::EMM().undo().setUserInteractionEnd( \
					EM::EMM().undo().currentEventID() );

void FaultStickSetDisplay::mouseCB( CallBacker* cb )
{
    if ( stickselectmode_ )
	return stickSelectCB( cb );

    if ( !emfss_ || !fsseditor_ || !viseditor_ || !isOn() ||
	 eventcatcher_->isHandled() || !isSelected() )
	return;

    mCBCapsuleUnpack(const visBase::EventInfo&,eventinfo,cb);

    fsseditor_->setSowingPivot( disp2world(viseditor_->sower().pivotPos()) );
    if ( viseditor_->sower().accept(eventinfo) )
	return;

    const EM::PosID mousepid =
		    viseditor_->mouseClickDragger( eventinfo.pickedobjids );

    EM::FaultStickSetGeometry& fssg = emfss_->geometry();

    PlaneDataDisplay* plane = 0;
    Seis2DDisplay* s2dd = 0;
    const MultiID* lineset = 0;
    const char* linenm = 0;
    PtrMan<Coord3> normal = 0;
    Coord3 pos;

    if ( !mousepid.isUdf() )
    {
	const int sticknr = RowCol( mousepid.subID() ).row;
	lineset = fssg.lineSet( mousepid.sectionID(), sticknr );
	linenm = fssg.lineName( mousepid.sectionID(), sticknr );
	pos = emfss_->getPos( mousepid );
    }
    else
    {
	for ( int idx=0; idx<eventinfo.pickedobjids.size(); idx++ )
	{
	    const int visid = eventinfo.pickedobjids[idx];
	    visBase::DataObject* dataobj = visBase::DM().getObject( visid );

	    mDynamicCastGet(Seis2DDisplay*,disp2d,dataobj);
	    if ( disp2d )
	    {
		s2dd = disp2d;
		lineset = &s2dd->lineSetID();
		linenm = s2dd->name();
		break;
	    }
	    mDynamicCastGet(PlaneDataDisplay*,pdd,dataobj);
	    if ( pdd )
	    {
		plane = pdd;
		normal = new Coord3( plane->getNormal(Coord3::udf()) );
		break;	
	    }
	    mDynamicCastGet(FaultStickSetDisplay*,fssd,dataobj);
	    if ( fssd )
		return;
	}

	pos = disp2world( eventinfo.displaypickedpos );
    }

    EM::PosID insertpid;
    fsseditor_->getInteractionInfo( insertpid, lineset, linenm,
				    pos, mZScale(), normal );

    if ( mousepid.isUdf() && !viseditor_->isDragging() )
	setActiveStick( insertpid );

    if ( locked_ || !pos.isDefined() || 
	 eventinfo.type!=visBase::MouseClick || viseditor_->isDragging() ||
	 OD::altKeyboardButton(eventinfo.buttonstate_) ||
	 !OD::leftMouseButton(eventinfo.buttonstate_) )
	return;

    if ( !mousepid.isUdf() )
    {
	fsseditor_->setLastClicked( mousepid );
	setActiveStick( mousepid );
    }

    if ( !mousepid.isUdf() && OD::ctrlKeyboardButton(eventinfo.buttonstate_) &&
	 !OD::shiftKeyboardButton(eventinfo.buttonstate_) )
    {
	// Remove knot/stick
	eventcatcher_->setHandled();
	if ( eventinfo.pressed )
	    return;

	editpids_.erase();
	const int rmnr = RowCol(mousepid.subID()).row;
	if ( fssg.nrKnots(mousepid.sectionID(), rmnr) == 1 )
	{
	    fssg.removeStick( mousepid.sectionID(), rmnr, true );
	    fsseditor_->setLastClicked( EM::PosID::udf() );
	}
	else
	    fssg.removeKnot( mousepid.sectionID(), mousepid.subID(), true );

	mSetUserInteractionEnd();
	updateEditPids();
	return;
    }

    if ( !mousepid.isUdf() || OD::ctrlKeyboardButton(eventinfo.buttonstate_) )
	return;
    
    if ( viseditor_->sower().activate(emfss_->preferredColor(), eventinfo) )
	return;

    if ( eventinfo.pressed )
	return;

    if ( OD::shiftKeyboardButton(eventinfo.buttonstate_) || insertpid.isUdf() )
    {
	// Add stick
	if ( plane || s2dd )
	{
	    const Coord3 editnormal( plane ? plane->getNormal(Coord3()) :
		    Coord3(s2dd->getNormal(s2dd->getNearestTraceNr(pos)),0) );

	    const int sid = emfss_->sectionID(0);
	    Geometry::FaultStickSet* fss = fssg.sectionGeometry( sid );

	    const int insertsticknr = 
			!fss || fss->isEmpty() ? 0 : fss->rowRange().stop+1;

	    editpids_.erase();
	    fssg.insertStick( sid, insertsticknr, 0, pos, editnormal,
			      lineset, linenm, true );
	    const EM::SubID subid = RowCol(insertsticknr,0).toInt64();
	    fsseditor_->setLastClicked( EM::PosID(emfss_->id(),sid,subid) );
	    setActiveStick( EM::PosID(emfss_->id(),sid,subid) );
	    mSetUserInteractionEnd();
	    updateEditPids();
	}
    }
    else
    {
	// Add knot
	editpids_.erase();
	fssg.insertKnot( insertpid.sectionID(), insertpid.subID(), pos, true );
	fsseditor_->setLastClicked( insertpid );
	mSetUserInteractionEnd();
	updateEditPids();
    }

    eventcatcher_->setHandled();
}


void FaultStickSetDisplay::stickSelectCB( CallBacker* cb )
{
    if ( !emfss_ || !isOn() || eventcatcher_->isHandled() || !isSelected() )
	return;

    mCBCapsuleUnpack(const visBase::EventInfo&,eventinfo,cb);

    ctrldown_ = OD::ctrlKeyboardButton(eventinfo.buttonstate_);

    if ( eventinfo.type!=visBase::MouseClick ||
	 !OD::leftMouseButton(eventinfo.buttonstate_) )
	return;

    EM::PosID pid = EM::PosID::udf();

    for ( int idx=0; idx<eventinfo.pickedobjids.size(); idx++ )
    {
	const int visid = eventinfo.pickedobjids[idx];
	visBase::DataObject* dataobj = visBase::DM().getObject( visid );
	mDynamicCastGet( visBase::Marker*, marker, dataobj );
	if ( marker )
	{
	    PtrMan<EM::EMObjectIterator> iter =
					 emfss_->geometry().createIterator(-1);
	    while ( true )
	    {
		pid = iter->next();
		if ( pid.objectID() == -1 )
		    return;

		const Coord3 diff = emfss_->getPos(pid) - marker->centerPos();
		if ( diff.abs() < 1e-4 )
		    break;
	    }
	}
    }

    if ( pid.isUdf() )
    	return;

    const int sticknr = RowCol( pid.subID() ).row;
    const bool isselected = !OD::ctrlKeyboardButton( eventinfo.buttonstate_ );

    const int sid = emfss_->sectionID(0);
    Geometry::FaultStickSet* fss = emfss_->geometry().sectionGeometry( sid );

    fss->selectStick( sticknr, isselected ); 
    updateKnotMarkers();
    eventcatcher_->setHandled();
}


void FaultStickSetDisplay::setActiveStick( const EM::PosID& pid )
{
    const int sticknr = pid.isUdf() ? mUdf(int) : RowCol(pid.subID()).row;
    if ( activesticknr_ != sticknr )
    {
	activesticknr_ = sticknr;
	updateSticks( true );
    }
}


void FaultStickSetDisplay::emChangeCB( CallBacker* cber )
{
    mCBCapsuleUnpack(const EM::EMObjectCallbackData&,cbdata,cber);
    if ( cbdata.event==EM::EMObjectCallbackData::PrefColorChange )
	getMaterial()->setColor( emfss_->preferredColor() );

    if ( cbdata.event==EM::EMObjectCallbackData::PositionChange && emfss_ )
    {
	const int sid = cbdata.pid0.sectionID();
	RowCol rc( cbdata.pid0.subID() );

	if ( emfss_->geometry().pickedOn2DLine(sid, rc.row) )
	{
	    const MultiID* lset = emfss_->geometry().lineSet( sid, rc.row );
	    const char* lnm = emfss_->geometry().lineName( sid, rc.row );

	    Seis2DDisplay* s2dd = Seis2DDisplay::getSeis2DDisplay( *lset, lnm );
	    if ( s2dd ) 
	    {
		const Coord3 curpos = emfss_->getPos( cbdata.pid0 ); 
		const Coord3 newpos = s2dd->getNearestSubPos( curpos, true );

		CallBack cb = mCB(this,FaultStickSetDisplay,emChangeCB);
		emfss_->change.remove( cb );
		emfss_->setPos( cbdata.pid0, newpos, false );
		emfss_->change.notify( cb );
	    }
	}
    }
    updateAll();
}


void FaultStickSetDisplay::showManipulator( bool yn )
{
    showmanipulator_ = yn;
    if ( viseditor_ )
	viseditor_->turnOn( yn );

    updateKnotMarkers();

    if ( scene_ )
	scene_->blockMouseSelection( yn );
}


bool  FaultStickSetDisplay::isManipulatorShown() const
{ return showmanipulator_; }


void FaultStickSetDisplay::removeSelection( const Selector<Coord3>& sel,
	TaskRunner* tr )
{
    if ( fsseditor_ )
	fsseditor_->removeSelection( sel );
}


void FaultStickSetDisplay::otherObjectsMoved(
				const ObjectSet<const SurveyObject>& objs,
				int whichobj )
{
    if ( !displayonlyatsections_ )
	return;

    updateAll();
}


bool FaultStickSetDisplay::coincidesWith2DLine(
			    const Geometry::FaultStickSet& fss, int sticknr,
			    const MultiID& lineset, const char* linenm ) const
{
    RowCol rc( sticknr, 0 );
    const StepInterval<int> rowrg = fss.rowRange();
    if ( !scene_ || !rowrg.includes(sticknr) || rowrg.snap(sticknr)!=sticknr )
	return false;

    for ( int idx=0; idx<scene_->size(); idx++ )
    {
	visBase::DataObject* dataobj = scene_->getObject( idx );
	mDynamicCastGet( Seis2DDisplay*, s2dd, dataobj );
	if ( !s2dd || !s2dd->isOn() || lineset!=s2dd->lineSetID() ||
	     strcmp(linenm,s2dd->getLineName())  )
	    continue;

	const float onestepdist = SI().oneStepDistance(Coord3(0,0,1),mZScale());

	const StepInterval<int> colrg = fss.colRange( rc.row );
	for ( rc.col=colrg.start; rc.col<=colrg.stop; rc.col+=colrg.step )
	{
	    Coord3 pos = fss.getKnot(rc);
	    if ( displaytransform_ )
		pos = displaytransform_->transform( pos ); 

	    if ( s2dd->calcDist( pos ) <= 0.5*onestepdist )
		return true;
	}
    }
    return false;
}


bool FaultStickSetDisplay::coincidesWithPlane(
			const Geometry::FaultStickSet& fss, int sticknr ) const
{
    RowCol rc( sticknr, 0 );
    const StepInterval<int> rowrg = fss.rowRange();
    if ( !scene_ || !rowrg.includes(sticknr) || rowrg.snap(sticknr)!=sticknr )
	return false;

    for ( int idx=0; idx<scene_->size(); idx++ )
    {
	visBase::DataObject* dataobj = scene_->getObject( idx );
	mDynamicCastGet( PlaneDataDisplay*, plane, dataobj );
	if ( !plane || !plane->isOn() )
	    continue;

	const Coord3 vec1 = fss.getEditPlaneNormal(sticknr).normalize();
	const Coord3 vec2 = plane->getNormal(Coord3()).normalize();
	if ( fabs(vec1.dot(vec2)) < 0.5 )
	    continue;

	const float onestepdist = SI().oneStepDistance(
				  plane->getNormal(Coord3::udf()), mZScale() );
	float prevdist;
	Coord3 prevpos;

	const StepInterval<int> colrg = fss.colRange( rc.row );
	for ( rc.col=colrg.start; rc.col<=colrg.stop; rc.col+=colrg.step )
	{
	    Coord3 curpos = fss.getKnot(rc);
	    if ( displaytransform_ )
		curpos = displaytransform_->transform( curpos ); 

	    const float curdist = plane->calcDist( curpos );
	    if ( curdist <= 0.5*onestepdist )
		return true;

	    if ( rc.col != colrg.start )
	    {
		const float frac = prevdist / (prevdist+curdist);
		Coord3 interpos = (1-frac)*prevpos + frac*curpos;

		if ( plane->calcDist(interpos) <= 0.5*onestepdist )
		    return true;
	    }

	    prevdist = curdist;
	    prevpos = curpos;
	}
    }
    return false;
}


void FaultStickSetDisplay::displayOnlyAtSectionsUpdate()
{
    if ( !emfss_ )
	return;

    for ( int sidx=0; sidx<emfss_->nrSections(); sidx++ )
    {
	int sid = emfss_->sectionID( sidx );
	mDynamicCastGet( Geometry::FaultStickSet*, fss,
			 emfss_->sectionGeometry( sid ) );
	if ( fss->isEmpty() )
	    continue;

	RowCol rc;
	const StepInterval<int> rowrg = fss->rowRange();
	for ( rc.row=rowrg.start; rc.row<=rowrg.stop; rc.row+=rowrg.step )
	{
	    fss->hideStick( rc.row, false );
	    if ( !displayonlyatsections_ )
		continue;

	    if ( emfss_->geometry().pickedOn2DLine(sid,rc.row) )
	    { 
		const MultiID* lset = emfss_->geometry().lineSet( sid, rc.row );
		const char* lnm = emfss_->geometry().lineName( sid, rc.row );
		if ( !coincidesWith2DLine(*fss, rc.row, *lset, lnm) )
		    fss->hideStick( rc.row, true );
	    }
	    else if ( emfss_->geometry().pickedOnPlane(sid,rc.row) )
	    {
		if ( !coincidesWithPlane(*fss, rc.row) )
		    fss->hideStick( rc.row, true );
	    }
	}
    }

    updateSticks();
    updateEditPids();
    updateKnotMarkers();
}






void FaultStickSetDisplay::setDisplayOnlyAtSections( bool yn )
{
    displayonlyatsections_ = yn;
    updateAll();
    displaymodechange.trigger();
}


bool FaultStickSetDisplay::displayedOnlyAtSections() const
{ return displayonlyatsections_; }


void FaultStickSetDisplay::setStickSelectMode( bool yn )
{
    stickselectmode_ = yn;
    ctrldown_ = false;

    updateEditPids();
    updateKnotMarkers();

    const CallBack cb = mCB( this, FaultStickSetDisplay, polygonFinishedCB );
    if ( yn )
	scene_->getPolySelection()->polygonFinished()->notify( cb );
    else
	scene_->getPolySelection()->polygonFinished()->remove( cb );
}


void FaultStickSetDisplay::polygonFinishedCB( CallBacker* cb )
{
    if ( !stickselectmode_ || !emfss_ || !scene_ )
	return;

    PtrMan<EM::EMObjectIterator> iter = emfss_->geometry().createIterator(-1);
    while ( true )
    {
	EM::PosID pid = iter->next();
	if ( pid.objectID() == -1 )
	    break;

	if ( !scene_->getPolySelection()->isInside(emfss_->getPos(pid) ) )
	    continue;

	const int sticknr = RowCol( pid.subID() ).row;
	const EM::SectionID sid = pid.sectionID();
	Geometry::FaultStickSet* fss = emfss_->geometry().sectionGeometry(sid);
	fss->selectStick( sticknr, !ctrldown_ ); 
    }

    updateKnotMarkers();
}


bool FaultStickSetDisplay::isInStickSelectMode() const
{ return stickselectmode_; }


void FaultStickSetDisplay::updateKnotMarkers()
{
    if ( !emfss_ || (viseditor_ && viseditor_->sower().moreToSow()) )
	return;
    
    for ( int idx=0; idx<knotmarkers_.size(); idx++ )
    {
	while ( knotmarkers_[idx]->size() > 1 )
	    knotmarkers_[idx]->removeObject( 1 );
    }

    if ( !showmanipulator_ || !stickselectmode_ )
	return;

    PtrMan<EM::EMObjectIterator> iter = emfss_->geometry().createIterator(-1);
    while ( true )
    {
	const EM::PosID pid = iter->next();
	if ( pid.objectID() == -1 )
	    break;

	const int sid = pid.sectionID();
	const int sticknr = RowCol( pid.subID() ).row;
	Geometry::FaultStickSet* fss = emfss_->geometry().sectionGeometry(sid);
	if ( !fss || fss->isStickHidden(sticknr) )
	    continue;

	const Coord3 pos = emfss_->getPos( pid );

	visBase::Marker* marker = visBase::Marker::create();
	marker->setMarkerStyle( emfss_->getPosAttrMarkerStyle(0) );
	marker->setMaterial(0);
	marker->setDisplayTransformation( displaytransform_ );
	marker->setCenterPos(pos);
	marker->setScreenSize(3);

	const int groupidx = fss->isStickSelected(sticknr) ? 1 : 0;
	knotmarkers_[groupidx]->addObject( marker );
    }
}


void FaultStickSetDisplay::updateAll()
{
    displayOnlyAtSectionsUpdate();
    updateSticks();
    updateEditPids();
    updateKnotMarkers();
}


void FaultStickSetDisplay::fillPar( IOPar& par, TypeSet<int>& saveids ) const
{
    visBase::VisualObjectImpl::fillPar( par, saveids );

    par.set( sKeyEarthModelID(), getMultiID() );
}


int FaultStickSetDisplay::usePar( const IOPar& par )
{
    int res = visBase::VisualObjectImpl::usePar( par );
    if ( res!=1 ) return res;

    MultiID newmid;
    if ( par.get(sKeyEarthModelID(),newmid) )
    {
	EM::ObjectID emid = EM::EMM().getObjectID( newmid );
	RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
	if ( !emobject )
	{
	    PtrMan<Executor> loader = EM::EMM().objectLoader( newmid );
	    if ( loader ) loader->execute();
	    emid = EM::EMM().getObjectID( newmid );
	    emobject = EM::EMM().getObject( emid );
	}

	if ( emobject ) setEMID( emobject->id() );
    }

    return 1;
}

} // namespace visSurvey
