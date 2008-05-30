/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          Feb 2005
 RCS:           $Id: horizon2dscanner.cc,v 1.1 2008-05-30 07:08:52 cvsraman Exp $
________________________________________________________________________

-*/

#include "horizon2dscanner.h"
#include "binidvalset.h"
#include "emhorizon2d.h"
#include "ioman.h"
#include "iopar.h"
#include "seis2dline.h"
#include "strmprov.h"
#include "survinfo.h"
#include "oddirs.h"
#include "cubesampling.h"
#include "keystrs.h"
#include "tabledef.h"


Horizon2DScanner::Horizon2DScanner( const BufferStringSet& fnms,
				    const MultiID& setid,
				    Table::FormatDesc& fd )
    : Executor("Scan horizon file(s)")
    , fd_(fd)
    , ascio_(0)
    , lineset_(0)
    , isxy_(false)
    , bvalset_(0)
    , fileidx_(0)
{
    filenames_ = fnms;
    PtrMan<IOObj> ioobj = IOM().get( setid );
    if ( ioobj )
	lineset_ = new Seis2DLineSet( *ioobj );

    init();
}


void Horizon2DScanner::init()
{
    totalnr_ = -1;
    valranges_.erase();
}


const char* Horizon2DScanner::message() const
{
    return "Scanning";
}


const char* Horizon2DScanner::nrDoneText() const
{
    return "Positions handled";
}


int Horizon2DScanner::nrDone() const
{
    return bvalset_ ? bvalset_->totalSize() : 0;
}


int Horizon2DScanner::totalNr() const
{
    if ( totalnr_ > 0 ) return totalnr_;

    totalnr_ = 0;
    for ( int idx=0; idx<filenames_.size(); idx++ )
    {
	StreamProvider sp( filenames_.get(0).buf() );
	StreamData sd = sp.makeIStream();
	if ( !sd.usable() ) continue;

	char buf[80];
	while ( *sd.istrm )
	{
	    sd.istrm->getline( buf, 80 );
	    totalnr_++;
	}

	sd.close();
	totalnr_ -= fd_.nrhdrlines_;
    }

    return totalnr_;
}


void Horizon2DScanner::report( IOPar& iopar ) const
{
    iopar.clear();

    BufferString str = "Report for horizon file(s):\n";
    for ( int idx=0; idx<filenames_.size(); idx++ )
    {
	str += filenames_.get(idx).buf(); str += "\n";
    }
    str += "\n\n";
    iopar.setName( str.buf() );

    iopar.add( "->", "Geometry" );
    iopar.set( "Valid lines found", validnms_.size() );
    BufferString msg;
    for ( int idx=0; idx<validnms_.size(); idx++ )
    {
	msg += validnms_.get( idx );
	msg += ", ";
    }

    iopar.add( "Line names", msg );
    msg = "";
    for ( int idx=0; idx<invalidnms_.size(); idx++ )
    {
	msg += invalidnms_.get( idx );
	msg += ", ";
    }

    iopar.add( "Rejected Line names", msg );

    iopar.set( "Total postions", bvalset_ ? bvalset_->totalSize() : 0 );
    iopar.add( "Value ranges", "" );
    for ( int idx=0; idx<valranges_.size(); idx++ )
	iopar.set( fd_.bodyinfos_[idx+2]->name(), valranges_[idx] );
}



const char* Horizon2DScanner::defaultUserInfoFile()
{
    static BufferString ret;
    ret = GetProcFileName( "scan_horizon" );
    if ( GetSoftwareUser() )
	{ ret += "_"; ret += GetSoftwareUser(); }
    ret += ".txt";
    return ret.buf();
}


void Horizon2DScanner::launchBrowser( const char* fnm ) const
{
    if ( !fnm || !*fnm )
	fnm = defaultUserInfoFile();
    IOPar iopar; report( iopar );
    iopar.write( fnm, "_pretty" );

    ExecuteScriptCommand( "FileBrowser", fnm );
}


bool Horizon2DScanner::reInitAscIO( const char* fnm )
{
    StreamProvider sp( fnm );
    StreamData sd = sp.makeIStream();
    if ( !sd.usable() )
	return false;

    ascio_ = new EM::Horizon2DAscIO( fd_, *sd.istrm );
    if ( !ascio_ ) return false;

    isxy_ = ascio_->isXY();
    udfval_ = ascio_->getUdfVal();
    return true;
}


int Horizon2DScanner::nextStep()
{
    if ( fileidx_ >= filenames_.size() )
	return Executor::Finished;

    if ( !ascio_ && !reInitAscIO( filenames_.get(fileidx_).buf() ) )
	return Executor::ErrorOccurred;

    BufferString linenm;
    TypeSet<float> data;
    const int ret = ascio_->getNextLine( linenm, data );
    if ( ret < 0 ) return Executor::ErrorOccurred;
    if ( ret == 0 ) 
    {
	fileidx_++;
	delete ascio_;
	ascio_ = 0;
	return Executor::MoreToDo;
    }

    if ( curline_.isEmpty() || curline_ != linenm )
    {
	linegeom_.posns.erase();
	LineKey lk( linenm, LineKey::sKeyDefAttrib, true );
	const int lidx = lineset_->indexOf( lk );
	if ( lidx < 0 )
	{
	    invalidnms_.addIfNew( linenm );
	    return Executor::MoreToDo;
	}

	if ( !lineset_->getGeometry(lidx,linegeom_) )
	    return Executor::ErrorOccurred;

	validnms_.addIfNew( linenm );
	curline_ = linenm;
    }

    if ( !linegeom_.posns.size() )
	return Executor::ErrorOccurred;

    const int firstvalidx = isxy_ ? 2 : 1;
    if ( !bvalset_ )
	bvalset_ = new BinIDValueSet( data.size() - firstvalidx, false );

    PosInfo::Line2DPos pos;
    if ( !isxy_ )
    {
	const int trcnr = mNINT( data[0] );
	if ( !linegeom_.getPos(trcnr,pos) )
	    return Executor::MoreToDo;

	data[0] = pos.coord_.x;
	data.insert( 1, pos.coord_.y );
    }
    else
    {
	const Coord coord( data[0], data[1] );
	if ( !linegeom_.getPos(coord,pos) )
	    return Executor::MoreToDo;
    }

    int validx = 0;
    const int nrvals = data.size()-firstvalidx;
    while ( validx < nrvals )
    {
	while ( valranges_.size() < nrvals )
	    valranges_ += Interval<float>(mUdf(float),-mUdf(float));

	const float val = data[validx+firstvalidx];

	if ( !mIsUdf(val) )
	    valranges_[validx].include( val, false );
	validx++;
    }
    
    const BinID bid( 0, pos.nr_ );
    bvalset_->add( bid, data.arr() );

    return Executor::MoreToDo;
}


