/*+
 * COPYRIGHT: (C) de Groot-Bril Earth Sciences B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : May 2002
-*/

static const char* rcsID = "$Id: viswelldisplay.cc,v 1.20 2003-10-23 15:09:34 nanne Exp $";

#include "vissurvwell.h"
#include "viswell.h"
#include "wellman.h"
#include "welllog.h"
#include "welllogset.h"
#include "welldata.h"
#include "welltransl.h"
#include "welltrack.h"
#include "wellmarker.h"
#include "welld2tmodel.h"
#include "iopar.h"
#include "ioman.h"
#include "ioobj.h"
#include "executor.h"
#include "ptrman.h"
#include "survinfo.h"
#include "draw.h"


mCreateFactoryEntry( visSurvey::WellDisplay );

namespace visSurvey
{

const char* WellDisplay::earthmodelidstr 	= "EarthModel ID";
const char* WellDisplay::ioobjidstr		= "IOObj ID";


WellDisplay::WellDisplay()
    : well( visBase::Well::create() )
    , wellid( -1 )
    , zistime(SI().zIsTime())
{
    well->ref();
    addChild( well->getData() );
}


WellDisplay::~WellDisplay()
{
    removeChild( well->getData() );
    well->unRef();
}


#define mErrRet(s) { errmsg = s; return false; }

bool WellDisplay::setWellId( const MultiID& multiid )
{
    Well::Data* wd = Well::MGR().get( multiid );
    if ( !wd ) return false;
    
    const Well::D2TModel* d2t = wd->d2TModel();
    if ( zistime && !d2t ) mErrRet( "No depth to time model defined" );

    wellid = multiid;
    setName( wd->name() );

    if ( wd->track().size() < 1 ) return true;
    TypeSet<Coord3> track;
    Coord3 pt;
    for ( int idx=0; idx<wd->track().size(); idx++ )
    {
	pt = wd->track().pos( idx );
	if ( zistime )
	    pt.z = d2t->getTime( wd->track().dah(idx) );
	if ( !mIsUndefined(pt.z) )
	    track += pt;
    }

    well->setTrack( track );
    well->setWellName( wd->name(), track.size() ? track[0] : Coord3(0,0,0) );
    addMarkers();

    return true;
}


const LineStyle& WellDisplay::lineStyle() const
{
    return well->lineStyle();
}


void WellDisplay::setLineStyle( const LineStyle& lst )
{
    well->setLineStyle( lst );
}


void WellDisplay::addMarkers()
{
    Well::Data* wd = Well::MGR().get( wellid );
    if ( !wd ) return;

    well->removeAllMarkers();
    for ( int idx=0; idx<wd->markers().size(); idx++ )
    {
	Well::Marker* wellmarker = wd->markers()[idx];
	Coord3 pos = wd->track().getPos( wellmarker->dah );
	if ( !pos.x && !pos.y && !pos.z ) continue;

	if ( zistime )
	    pos.z = wd->d2TModel()->getTime( pos.z );

	well->addMarker( pos, wellmarker->color, wellmarker->name() );
    }

    well->setMarkerScale( Coord3(1,1,1/(4*SPM().getZScale())) );
}


void WellDisplay::setMarkerSize( int sz )
{ well->setMarkerSize( sz ); }

int WellDisplay::markerSize() const
{ return well->markerSize(); }


#define mShowFunction( showObj, objShown ) \
void WellDisplay::showObj( bool yn ) \
{ \
    well->showObj( yn ); \
} \
\
bool WellDisplay::objShown() const \
{ \
    return well->objShown(); \
}

mShowFunction( showWellName, wellNameShown )
mShowFunction( showMarkers, markersShown )
mShowFunction( showMarkerName, markerNameShown )
mShowFunction( showLogs, logsShown )
mShowFunction( showLogName, logNameShown )


void WellDisplay::displayLog( int logidx, int lognr, 
			      const Interval<float>& range )
{
    Well::Data* wd = Well::MGR().get( wellid );
    if ( !wd || !wd->logs().size() ) return;

    Well::Log& log = wd->logs().getLog(logidx);
    const int logsz = log.size();
    if ( !logsz ) return;

    Well::Track& track = wd->track();
    const float enddepth = track.pos( track.size()-1 ).z;
    TypeSet<Coord3> coords;
    TypeSet<float> values;
    for ( int idx=0; idx<logsz; idx++ )
    {
	float dah = log.dah(idx);
	Coord3 pos = track.getPos( dah );
	if ( !pos.x && !pos.y && !pos.z ) break;

	pos.z = wd->d2TModel()->getTime( pos.z );
	coords += pos;
	values += log.value(idx);
    }

    well->setLog( coords, values, lognr );
}


const Color& WellDisplay::logColor( int logidx ) const
{
    return well->logColor( logidx );
}


void WellDisplay::setLogColor( const Color& col, int logidx )
{
    well->setLogColor( col, logidx );
}


void WellDisplay::fillPar( IOPar& par, TypeSet<int>& saveids ) const
{
    visBase::VisualObjectImpl::fillPar( par, saveids );

    par.set( earthmodelidstr, wellid );

    BufferString linestyle;
    lineStyle().toString( linestyle );
    par.set( visBase::Well::linestylestr, linestyle );
}


int WellDisplay::usePar( const IOPar& par )
{
    int res = visBase::VisualObjectImpl::usePar( par );
    if ( res!=1 ) return res;

    MultiID newwellid;
    if ( !par.get( earthmodelidstr, newwellid ))
	return -1;

    if ( !setWellId( newwellid ) )
	return -1;

    BufferString linestyle;
    if ( par.get(visBase::Well::linestylestr,linestyle) )
    {
	LineStyle lst;
	lst.fromString( linestyle );
	setLineStyle( lst );
    }

    bool wellnmshown = true;
    par.getYN( visBase::Well::showwellnmstr, wellnmshown );
    showWellName( wellnmshown );

    return 1;
}


void WellDisplay::setTransformation( visBase::Transformation* nt )
{
    well->setTransformation( nt );
}


visBase::Transformation* WellDisplay::getTransformation()
{ return well->getTransformation(); }

}; // namespace visSurvey
