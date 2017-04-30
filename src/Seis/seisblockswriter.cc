/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Apr 2017
________________________________________________________________________

-*/

#include "seisblockswriter.h"
#include "seistrc.h"
#include "seisbuf.h"
#include "posidxpairdataset.h"
#include "paralleltask.h"
#include "executor.h"
#include "uistrings.h"
#include "scaler.h"
#include "datachar.h"
#include "file.h"
#include "keystrs.h"
#include "posinfo.h"
#include "survgeom3d.h"
#include "survinfo.h" // for survey name and zInFeet
#include "ascstream.h"
#include "separstr.h"


namespace Seis
{
namespace Blocks
{

/*!\brief Block with data buffer collecting data to be written. */

class MemBlock : public Block
{
public:

			MemBlock(GlobIdx,const Dimensions&,const DataInterp&);

    void		zero()			{ dbuf_.zero(); }
    void		retire()		{ dbuf_.reSize( 0, false ); }
    bool		isRetired() const	{ return dbuf_.isEmpty(); }

    float		value(const LocIdx&) const;
    void		setValue(const LocIdx&,float);

    DataBuffer		dbuf_;
    const DataInterp&	interp_;

protected:

    int			getBufIdx(const LocIdx&) const;

};


class MemBlockColumn : public Column
{
public:

    typedef ManagedObjectSet<MemBlock>	BlockSet;

			MemBlockColumn(const HGlobIdx&,const Dimensions&,
					int ncomps);
			~MemBlockColumn();

    void		retire();
    inline bool		isRetired() const
			{ return blocksets_.first()->first()->isRetired(); }
    void		getDefArea(HLocIdx&,HDimensions&) const;

    int			nruniquevisits_;
    od_stream_Pos	fileoffset_;
    bool**		visited_;
    ObjectSet<BlockSet> blocksets_; // one set per component

};

class StepFinder
{
public:

		StepFinder(Writer&);

    bool	isFinished() const	{ return tbuf_.isEmpty(); }
    void	finish(uiRetVal&);
    void	addTrace(const SeisTrc&,uiRetVal&);

    Writer&		wrr_;
    Pos::IdxPairDataSet positions_;
    SeisTrcBuf		tbuf_;

};

} // namespace Blocks

} // namespace Seis


Seis::Blocks::MemBlock::MemBlock( GlobIdx gidx, const Dimensions& dms,
				  const DataInterp& interp )
    : Block(gidx,HLocIdx(),dms)
    , dbuf_(0)
    , interp_(interp)
{
    const int bytesperval = interp_.nrBytes();
    dbuf_.reByte( bytesperval, false );
    const int totsz = (((int)dims_.inl())*dims_.crl()) * dims_.z();
    dbuf_.reSize( totsz, false );
}


int Seis::Blocks::MemBlock::getBufIdx( const LocIdx& sidx ) const
{
    return ((int)dims_.z()) * (sidx.inl()*dims_.crl() + sidx.crl()) + sidx.z();
}


float Seis::Blocks::MemBlock::value( const LocIdx& sidx ) const
{
    return interp_.get( dbuf_.data(), getBufIdx(sidx) );
}


void Seis::Blocks::MemBlock::setValue( const LocIdx& sidx, float val )
{
    interp_.put( dbuf_.data(), getBufIdx(sidx), val );
}


Seis::Blocks::MemBlockColumn::MemBlockColumn( const HGlobIdx& gidx,
					      const Dimensions& bldims,
					      int nrcomps )
    : Column(gidx,bldims,nrcomps)
    , nruniquevisits_(0)
    , fileoffset_(0)
{
    for ( int icomp=0; icomp<nrcomps_; icomp++ )
	blocksets_ += new BlockSet;

    visited_ = new bool* [dims_.inl()];
    for ( IdxType iinl=0; iinl<dims_.inl(); iinl++ )
    {
	visited_[iinl] = new bool [dims_.crl()];
	for ( IdxType icrl=0; icrl<dims_.crl(); icrl++ )
	    visited_[iinl][icrl] = false;
    }
}


Seis::Blocks::MemBlockColumn::~MemBlockColumn()
{
    deepErase(blocksets_);
    for ( IdxType idx=0; idx<dims_.inl(); idx++ )
	delete [] visited_[idx];
    delete [] visited_;
}


void Seis::Blocks::MemBlockColumn::retire()
{
    for ( int iset=0; iset<blocksets_.size(); iset++ )
    {
	BlockSet& bset = *blocksets_[iset];
	for ( int iblock=0; iblock<bset.size(); iblock++ )
	    bset[iblock]->retire();
    }
}


void Seis::Blocks::MemBlockColumn::getDefArea( HLocIdx& defstart,
					       HDimensions& defdims ) const
{
    IdxType mininl = dims_.inl()-1, mincrl = dims_.crl()-1;
    IdxType maxinl = 0, maxcrl = 0;

    for ( IdxType iinl=0; iinl<dims_.inl(); iinl++ )
    {
	for ( IdxType icrl=0; icrl<dims_.crl(); icrl++ )
	{
	    if ( visited_[iinl][icrl] )
	    {
		if ( mininl > iinl )
		    mininl = iinl;
		if ( mincrl > icrl )
		    mincrl = icrl;
		if ( maxinl < iinl )
		    maxinl = iinl;
		if ( maxcrl < icrl )
		    maxcrl = icrl;
	    }
	}
    }

    defstart.inl() = mininl;
    defstart.crl() = mincrl;
    defdims.inl() = maxinl - mininl + 1;
    defdims.crl() = maxcrl - mincrl + 1;
}


Seis::Blocks::StepFinder::StepFinder( Writer& wrr )
    : wrr_(wrr)
    , positions_(0,false)
    , tbuf_(false)
{
}


// Algo: first collect at least 2000 traces. If 3 inls and 3 crls found,
// finish the procedure.

void Seis::Blocks::StepFinder::addTrace( const SeisTrc& trc, uiRetVal& uirv )
{
    const BinID bid( trc.info().binID() );
    if ( positions_.includes(bid) )
	return;
    tbuf_.add( new SeisTrc(trc) );
    positions_.add( bid );
    if ( positions_.totalSize() % 2000 )
	return;
    const int nrinls = positions_.nrFirst();
    if ( nrinls < 3 )
	return;

    bool found3crls = false;
    for ( int iinl=0; iinl<nrinls; iinl++ )
    {
	if ( positions_.nrSecondAtIdx(iinl) > 2 )
	    { found3crls = true; break; }
    }
    if ( !found3crls )
	return;

    finish( uirv );
}


// Algo: if enough inls/crls found - or at the end, finish() will be called.
// If at least one step was seen, the default (SI or provided) is replaced.
// Now add the collected traces to the writer so it can start off making
// the appropriate blocks.
// The writer will know the detection is finished because the tbuf is empty.

void Seis::Blocks::StepFinder::finish( uiRetVal& uirv )
{
    Pos::IdxPairDataSet::SPos spos;
    int inlstate = -1, crlstate = -1;
	// initialising these four because of alarmist code analysers
    int previnl = -1, prevcrl = -1;
    int inlstep = 1, crlstep = 1;
    while ( positions_.next(spos) )
    {
	const BinID bid( positions_.getIdxPair(spos) );
	if ( inlstate < 0 )
	{
	    previnl = bid.inl();
	    prevcrl = bid.crl();
	    inlstate = crlstate = 0;
	}
	const int inldiff = bid.inl() - previnl;
	if ( inldiff )
	{
	    previnl = bid.inl();
	    if ( inlstate < 1 )
	    {
		inlstep = inldiff;
		inlstate = 1;
	    }
	    else if ( inldiff < inlstep )
		inlstep = inldiff;
	    continue;
	}
	const int crldiff = bid.crl() - prevcrl;
	if ( crldiff < 1 )
	    continue;
	prevcrl = bid.crl();
	if ( crlstate < 1 )
	{
	    crlstep = crldiff;
	    crlstate = 1;
	}
	else if ( crldiff < crlstep )
	    crlstep = crldiff;
    }

    if ( inlstate > 0 )
	wrr_.hgeom_.sampling().hsamp_.step_.inl() = inlstep;
    if ( crlstate > 0 )
	wrr_.hgeom_.sampling().hsamp_.step_.crl() = crlstep;

    for ( int idx=0; idx<tbuf_.size(); idx++ )
    {
	SeisTrc* trc = tbuf_.get( idx );
	wrr_.doAdd( *trc, uirv );
	if ( uirv.isError() )
	    break;
    }

    tbuf_.deepErase(); // finished!
}



Seis::Blocks::Writer::Writer( const HGeom* geom )
    : specifiedfprep_(OD::AutoFPRep)
    , nrcomps_(1)
    , isfinished_(false)
    , stepfinder_(0)
    , interp_(new DataInterp(DataCharacteristics()))
    , strm_(0)
{
    if ( geom )
	hgeom_ = *geom;
    else
	hgeom_ = static_cast<const HGeom&>( HGeom::default3D() );
    zgeom_ = hgeom_.sampling().zsamp_;
}


Seis::Blocks::Writer::~Writer()
{
    if ( !isfinished_ )
    {
	Task* task = finisher();
	if ( task )
	{
	    task->execute();
	    delete task;
	}
    }

    delete strm_;
    deepErase( zevalpositions_ );
    delete interp_;
    delete stepfinder_;
}


void Seis::Blocks::Writer::setEmpty()
{
    clearColumns();
    deepErase( zevalpositions_ );
}


void Seis::Blocks::Writer::setBasePath( const File::Path& fp )
{
    if ( fp.fullPath() != basepath_.fullPath() )
    {
	basepath_ = fp;
	needreset_ = true;
    }
}


void Seis::Blocks::Writer::setFullPath( const char* nm )
{
    File::Path fp( nm );
    fp.setExtension( 0 );
    setBasePath( fp );
}


void Seis::Blocks::Writer::setFileNameBase( const char* nm )
{
    File::Path fp( basepath_ );
    fp.setFileName( nm );
    setBasePath( fp );
}


void Seis::Blocks::Writer::setFPRep( OD::FPDataRepType rep )
{
    if ( specifiedfprep_ != rep )
    {
	specifiedfprep_ = rep;
	needreset_ = true;
    }
}


void Seis::Blocks::Writer::setCubeName( const char* nm )
{
    cubename_ = nm;
}


void Seis::Blocks::Writer::setZDomain( const ZDomain::Def& def )
{
    hgeom_.setZDomain( def );
}


void Seis::Blocks::Writer::addComponentName( const char* nm )
{
    compnms_.add( nm );
}


void Seis::Blocks::Writer::addAuxInfo( const char* key, const IOPar& iop )
{
    BufferString auxkey( key );
    if ( auxkey.isEmpty() )
	{ pErrMsg( "Refusing to add section without key" ); return; }

    IOPar* toadd = new IOPar( iop );
    toadd->setName( BufferString(sKeySectionPre(),auxkey) );
    auxiops_ += toadd;
}


void Seis::Blocks::Writer::setScaler( const LinScaler* newscaler )
{
    if ( (!scaler_ && !newscaler) )
	return;

    delete scaler_;
    scaler_ = newscaler ? newscaler->clone() : 0;
    needreset_ = true;
}


void Seis::Blocks::Writer::resetZ()
{
    const float eps = Seis::cDefSampleSnapDist();
    deepErase( zevalpositions_ );
    const int nrglobzidxs = Block::globIdx4Z(zgeom_,zgeom_.stop,dims_.z()) + 1;

    for ( IdxType globzidx=0; globzidx<nrglobzidxs; globzidx++ )
    {
	ZEvalPosSet* posset = new ZEvalPosSet;
	for ( IdxType loczidx=0; loczidx<dims_.z(); loczidx++ )
	{
	    const float z = Block::z4Idxs( zgeom_, dims_.z(),
					   globzidx, loczidx );
	    if ( z > zgeom_.start-eps && z < zgeom_.stop+eps )
		*posset += ZEvalPos( loczidx, z );
	}
	if ( posset->isEmpty() )
	    delete posset;
	else
	    zevalpositions_ += posset;
    }

    if ( zevalpositions_.isEmpty() )
    {
	pErrMsg("Huh");
	ZEvalPosSet* posset = new ZEvalPosSet;
	*posset += ZEvalPos( 0, zgeom_.start );
	 zevalpositions_ += posset;
    }
}


uiRetVal Seis::Blocks::Writer::add( const SeisTrc& trc )
{
    uiRetVal uirv;
    Threads::Locker locker( accesslock_ );

    if ( needreset_ )
    {
	needreset_ = false;
	isfinished_ = false;
	setEmpty();
	if ( trc.isEmpty() )
	{
	    uirv.add( tr("No data in input trace") );
	    return uirv;
	}

	fprep_ = specifiedfprep_;
	if ( fprep_ == OD::AutoFPRep )
	    fprep_ = trc.data().getInterpreter()->dataChar().userType();
	delete interp_;
	interp_ = DataInterp::create( DataCharacteristics(fprep_), true );

	zgeom_.start = trc.startPos();
	zgeom_.stop = trc.endPos();
	zgeom_.step = trc.stepPos();
	nrcomps_ = trc.nrComponents();
	resetZ();

	delete stepfinder_;
	stepfinder_ = new StepFinder( *const_cast<Writer*>(this) );
    }

    if ( stepfinder_ )
    {
	stepfinder_->addTrace( trc, uirv );
	if ( stepfinder_->isFinished() )
	    { delete stepfinder_; stepfinder_ = 0; }
	return uirv;
    }

    doAdd( trc, uirv );
    return uirv;
}


void Seis::Blocks::Writer::doAdd( const SeisTrc& trc, uiRetVal& uirv )
{
    const BinID bid = trc.info().binID();
    const HGlobIdx globidx( Block::globIdx4Inl(hgeom_,bid.inl(),dims_.inl()),
			    Block::globIdx4Crl(hgeom_,bid.crl(),dims_.crl()) );

    MemBlockColumn* column = getColumn( globidx );
    if ( !column )
    {
	uirv.set( tr("Memory needed for writing process unavailable.") );
	setEmpty();
	return;
    }
    else if ( isCompleted(*column) )
	return; // this check is absolutely necessary to for MT writing

    const HLocIdx locidx( Block::locIdx4Inl(hgeom_,bid.inl(),dims_.inl()),
			  Block::locIdx4Crl(hgeom_,bid.crl(),dims_.crl()) );

    for ( int icomp=0; icomp<nrcomps_; icomp++ )
    {
	MemBlockColumn::BlockSet& blockset = *column->blocksets_[icomp];
	for ( int iblock=0; iblock<blockset.size(); iblock++ )
	{
	    const ZEvalPosSet& posset = *zevalpositions_[iblock];
	    MemBlock& block = *blockset[iblock];
	    add2Block( block, posset, locidx, trc, icomp );
	}
    }

    bool& visited = column->visited_[locidx.inl()][locidx.crl()];
    if ( !visited )
    {
	column->nruniquevisits_++;
	visited = true;
    }

    if ( isCompleted(*column) )
	writeColumn( *column, uirv );
}


void Seis::Blocks::Writer::add2Block( MemBlock& block,
			const ZEvalPosSet& zevals, const HLocIdx& hlocidx,
			const SeisTrc& trc, int icomp )
{
    if ( block.isRetired() )
	return; // new visit of already written. Won't do, but no error.

    LocIdx locidx( hlocidx.inl(), hlocidx.crl(), 0 );
    for ( int idx=0; idx<zevals.size(); idx++ )
    {
	const ZEvalPos& evalpos = zevals[idx];
	locidx.z() = evalpos.first;
	float val2set = trc.getValue( evalpos.second, icomp );
	if ( scaler_ )
	    val2set = (float)scaler_->scale( val2set );
	block.setValue( locidx, val2set );
    }
}


Seis::Blocks::MemBlockColumn*
Seis::Blocks::Writer::mkNewColumn( const HGlobIdx& hglobidx )
{
    MemBlockColumn* column = new MemBlockColumn( hglobidx, dims_, nrcomps_ );

    const int nrglobzidxs = zevalpositions_.size();
    for ( IdxType globzidx=0; globzidx<nrglobzidxs; globzidx++ )
    {
	const ZEvalPosSet& evalposs = *zevalpositions_[globzidx];
	GlobIdx globidx( hglobidx.inl(), hglobidx.crl(), globzidx );
	Dimensions dims( dims_ );
	if ( globzidx == nrglobzidxs-1 )
	    dims.z() = (SzType)evalposs.size();

	for ( int icomp=0; icomp<nrcomps_; icomp++ )
	{
	    MemBlock* block = new MemBlock( globidx, dims, *interp_ );
	    if ( block->isRetired() ) // ouch
		{ delete column; return 0; }
	    block->zero();
	    *column->blocksets_[icomp] += block;
	}
    }

    return column;
}


Seis::Blocks::MemBlockColumn*
Seis::Blocks::Writer::getColumn( const HGlobIdx& globidx )
{
    Column* column = findColumn( globidx );
    if ( !column )
    {
	column = mkNewColumn( globidx );
	addColumn( column );
    }
    return (MemBlockColumn*)column;
}


bool Seis::Blocks::Writer::isCompleted( const MemBlockColumn& column ) const
{
    return column.nruniquevisits_ == ((int)dims_.inl()) * dims_.crl();
}


namespace Seis
{
namespace Blocks
{

class ColumnWriter : public Executor
{ mODTextTranslationClass(Seis::Blocks::ColumnWriter)
public:

ColumnWriter( Writer& wrr, MemBlockColumn& colmn )
    : Executor("Column Writer")
    , wrr_(wrr)
    , column_(colmn)
    , strm_(*wrr.strm_)
    , nrblocks_(wrr_.nrColumnBlocks())
    , iblock_(0)
{
    if ( strm_.isBad() )
	setErr( true );
    else
    {
	column_.fileoffset_ = strm_.position();
	column_.getDefArea( start_, dims_ );
	if ( !wrr_.writeColumnHeader(column_,start_,dims_) )
	    setErr();
    }
}

virtual od_int64 nrDone() const { return iblock_; }
virtual od_int64 totalNr() const { return nrblocks_; }
virtual uiString nrDoneText() const { return tr("Blocks written"); }
virtual uiString message() const
{
    if ( uirv_.isError() )
	return uirv_;
    return tr("Writing Traces");
}

virtual int nextStep()
{
    if ( uirv_.isError() )
	return ErrorOccurred();
    else if ( iblock_ >= nrblocks_ )
	return Finished();

    for ( int icomp=0; icomp<wrr_.nrcomps_; icomp++ )
    {
	MemBlockColumn::BlockSet& blockset = *column_.blocksets_[icomp];
	MemBlock& block = *blockset[iblock_];
	if ( !wrr_.writeBlock( block, start_, dims_ ) )
	    { setErr(); return ErrorOccurred(); }
    }

    iblock_++;
    return MoreToDo();
}

void setErr( bool initial=false )
{
    uirv_.set( initial ? uiStrings::phrCannotOpen(toUiString(strm_.fileName()))
	    : uiStrings::phrCannotWrite( toUiString(strm_.fileName()) ) );
    strm_.addErrMsgTo( uirv_ );
}

    Writer&		wrr_;
    od_ostream&		strm_;
    MemBlockColumn&	column_;
    HDimensions		dims_;
    HLocIdx		start_;
    const int		nrblocks_;
    int			iblock_;
    uiRetVal		uirv_;

};

} // namespace Blocks
} // namespace Seis


void Seis::Blocks::Writer::writeColumn( MemBlockColumn& column, uiRetVal& uirv )
{
    if ( !strm_ )
	strm_ = new od_ostream( dataFileName() );
    ColumnWriter wrr( *this, column );
    if ( !wrr.execute() )
	uirv = wrr.uirv_;
}


bool Seis::Blocks::Writer::writeColumnHeader( const MemBlockColumn& column,
		    const HLocIdx& start, const HDimensions& dims ) const
{

    const SzType hdrsz = columnHeaderSize( version_ );
    const od_stream_Pos orgstrmpos = strm_->position();

    strm_->addBin( hdrsz );
    strm_->addBin( dims.first ).addBin( dims.second ).addBin( dims_.third );
    strm_->addBin( start.first ).addBin( start.second );
    strm_->addBin( column.globIdx().first ).addBin( column.globIdx().second );

    const int bytes_left_in_hdr = hdrsz - (int)(strm_->position()-orgstrmpos);
    char* buf = new char [bytes_left_in_hdr];
    OD::memZero( buf, bytes_left_in_hdr );
    strm_->addBin( buf, bytes_left_in_hdr );
    delete [] buf;

    return strm_->isOK();
}


bool Seis::Blocks::Writer::writeBlock( MemBlock& block, HLocIdx wrstart,
				       HDimensions wrhdims )
{
    const DataBuffer& dbuf = block.dbuf_;
    const Dimensions& blockdims = block.dims();
    const Dimensions wrdims( wrhdims.inl(), wrhdims.crl(), blockdims.z() );

    if ( wrdims == blockdims )
	strm_->addBin( dbuf.data(), dbuf.totalBytes() );
    else
    {
	const DataBuffer::buf_type* bufdata = dbuf.data();
	const int bytespersample = dbuf.bytesPerElement();
	const int bytesperentirecrl = bytespersample * blockdims.z();
	const int bytesperentireinl = bytesperentirecrl * blockdims.crl();

	const int bytes2write = wrdims.z() * bytespersample;
	const IdxType wrstopinl = wrstart.inl() + wrdims.inl();
	const IdxType wrstopcrl = wrstart.crl() + wrdims.crl();

	const DataBuffer::buf_type* dataptr;
	for ( IdxType iinl=wrstart.inl(); iinl<wrstopinl; iinl++ )
	{
	    dataptr = bufdata + iinl * bytesperentireinl
			      + wrstart.crl() * bytesperentirecrl;
	    for ( IdxType icrl=wrstart.crl(); icrl<wrstopcrl; icrl++ )
	    {
		strm_->addBin( dataptr, bytes2write );
		dataptr += bytesperentirecrl;
	    }
	}
    }

    block.retire();
    return strm_->isOK();
}


void Seis::Blocks::Writer::writeInfoFile( uiRetVal& uirv )
{
    od_ostream strm( infoFileName() );
    if ( strm.isBad() )
    {
	uirv.add( uiStrings::phrCannotOpen( toUiString(strm.fileName()) ) );
	return;
    }

    if ( !writeInfoFileData(strm) )
    {
	uirv.add( uiStrings::phrCannotWrite( toUiString(strm.fileName()) ) );
	return;
    }
}


bool Seis::Blocks::Writer::writeInfoFileData( od_ostream& strm )
{
    ascostream ascostrm( strm );
    if ( !ascostrm.putHeader(sKeyFileType()) )
	return false;

    PosInfo::CubeData cubedata;
    Interval<IdxType> globinlidxrg, globcrlidxrg;
    Interval<int> inlrg, crlrg;
    Interval<double> xrg, yrg;
    scanPositions( cubedata, globinlidxrg, globcrlidxrg,
		    inlrg, crlrg, xrg, yrg );

    IOPar iop( sKeyGenSection() );
    iop.set( sKeyFmtVersion(), version_ );
    iop.set( sKeySurveyName(), SI().name() );
    iop.set( sKeyCubeName(), cubename_ );
    DataCharacteristics::putUserTypeToPar( iop, fprep_ );
    if ( scaler_ )
    {
	// write the scaler needed to reconstruct the org values
	LinScaler* invscaler = scaler_->inverse();
	invscaler->put( iop );
	delete invscaler;
    }
    iop.set( sKey::XRange(), xrg );
    iop.set( sKey::YRange(), yrg );
    iop.set( sKey::InlRange(), inlrg );
    iop.set( sKey::CrlRange(), crlrg );
    iop.set( sKey::ZRange(), zgeom_ );
    hgeom_.zDomain().set( iop );
    if ( hgeom_.zDomain().isDepth() && SI().zInFeet() )
	iop.setYN( sKeyDepthInFeet(), true );

    FileMultiString fms;
    for ( int icomp=0; icomp<nrcomps_; icomp++ )
    {
	BufferString nm;
	if ( icomp < compnms_.size() )
	    nm = compnms_.get( icomp );
	else
	    nm.set( "Component " ).add( icomp+1 );
	fms += nm;
    }
    iop.set( sKeyComponents(), fms );
    if ( datatype_ != UnknownData )
	iop.set( sKeyDataType(), nameOf(datatype_) );

    hgeom_.putMapInfo( iop );
    iop.set( sKeyDimensions(), dims_.inl(), dims_.crl(), dims_.z() );
    iop.set( sKeyGlobInlRg(), globinlidxrg );
    iop.set( sKeyGlobCrlRg(), globcrlidxrg );

    iop.putTo( ascostrm );
    if ( !strm.isOK() )
	return false;

    Pos::IdxPairDataSet::SPos spos;
    IOPar offsiop( sKeyOffSection() );
    while ( columns_.next(spos) )
    {
	const MemBlockColumn& column = *(MemBlockColumn*)columns_.getObj(spos);
	if ( column.nruniquevisits_ > 0 )
	{
	    const HGlobIdx& gidx = column.globIdx();
	    BufferString ky;
	    ky.add( gidx.inl() ).add( '.' ).add( gidx.crl() );
	    offsiop.add( ky, column.fileoffset_ );
	}
    }
    offsiop.putTo( ascostrm );
    if ( !strm.isOK() )
	return false;

    for ( int idx=0; idx<auxiops_.size(); idx++ )
    {
	auxiops_[idx]->putTo( ascostrm );
	if ( !strm.isOK() )
	    return false;
    }

    strm << sKeyPosSection() << od_endl;
    return cubedata.write( strm, true );
}


void Seis::Blocks::Writer::scanPositions( PosInfo::CubeData& cubedata,
	Interval<IdxType>& globinlrg, Interval<IdxType>& globcrlrg,
	Interval<int>& inlrg, Interval<int>& crlrg,
	Interval<double>& xrg, Interval<double>& yrg )
{
    Pos::IdxPairDataSet sortedpositions( 0, false );
    Pos::IdxPairDataSet::SPos spos;
    bool first = true;
    while ( columns_.next(spos) )
    {
	const MemBlockColumn& column = *(MemBlockColumn*)columns_.getObj(spos);
	if ( column.nruniquevisits_ < 1 )
	    continue;

	const HGlobIdx& globidx = column.globIdx();
	if ( first )
	{
	    globinlrg.start = globinlrg.stop = globidx.inl();
	    globcrlrg.start = globcrlrg.stop = globidx.crl();
	}
	else
	{
	    globinlrg.include( globidx.inl(), false );
	    globcrlrg.include( globidx.crl(), false );
	}

	for ( IdxType iinl=0; iinl<dims_.inl(); iinl++ )
	{
	    for ( IdxType icrl=0; icrl<dims_.crl(); icrl++ )
	    {
		if ( !column.visited_[iinl][icrl] )
		    continue;
		const int inl = Block::inl4Idxs( hgeom_, dims_.inl(),
						 globidx.inl(), iinl );
		const int crl = Block::crl4Idxs( hgeom_, dims_.crl(),
						 globidx.crl(), icrl );
		const Coord coord = hgeom_.toCoord( inl, crl );
		if ( first )
		{
		    inlrg.start = inlrg.stop = inl;
		    crlrg.start = crlrg.stop = crl;
		    xrg.start = xrg.stop = coord.x_;
		    yrg.start = yrg.stop = coord.y_;
		}
		else
		{
		    inlrg.include( inl, false );
		    crlrg.include( crl, false );
		    xrg.include( coord.x_, false );
		    yrg.include( coord.y_, false );
		}

		sortedpositions.add( BinID(inl,crl) );
	    }
	}

	first = false;
    }

    PosInfo::CubeDataFiller cdf( cubedata );
    spos.reset();
    while ( sortedpositions.next(spos) )
    {
	const Pos::IdxPair ip( sortedpositions.getIdxPair( spos ) );
	cdf.add( BinID(ip.inl(),ip.crl()) );
    }
}


namespace Seis
{
namespace Blocks
{

class WriterFinisher : public Executor
{ mODTextTranslationClass(Seis::Blocks::WriterFinisher)
public:

WriterFinisher( Writer& wrr )
    : Executor("Seis Blocks Writer finisher")
    , wrr_(wrr)
    , colidx_(0)
{
    if ( wrr_.stepfinder_ )
    {
	wrr_.stepfinder_->finish( uirv_ );
	if ( uirv_.isError() )
	    return;
    }

    Pos::IdxPairDataSet::SPos spos;
    while ( wrr_.columns_.next(spos) )
    {
	MemBlockColumn* column = (MemBlockColumn*)wrr_.columns_.getObj( spos );
	if ( column->nruniquevisits_ < 1 )
	    column->retire();
	else if ( !column->isRetired() )
	    towrite_ += column;
    }
}

virtual od_int64 nrDone() const
{
    return colidx_;
}

virtual od_int64 totalNr() const
{
    return towrite_.size();
}

virtual uiString nrDoneText() const
{
   return tr("Column files written");
}

virtual uiString message() const
{
    if ( uirv_.isOK() )
       return tr("Writing edge blocks");
    return uirv_;
}

virtual int nextStep()
{
    if ( uirv_.isError() )
	return ErrorOccurred();

    if ( !towrite_.validIdx(colidx_) )
	return wrapUp();

    wrr_.writeColumn( *towrite_[colidx_], uirv_ );
    if ( uirv_.isError() )
	return ErrorOccurred();

    colidx_++;
    return MoreToDo();
}

int wrapUp()
{
    delete wrr_.strm_; wrr_.strm_ = 0;
    wrr_.writeInfoFile( uirv_ );
    wrr_.isfinished_ = true;
    return uirv_.isOK() ? Finished() : ErrorOccurred();
}

    int			colidx_;
    uiRetVal		uirv_;
    Writer&		wrr_;
    ObjectSet<MemBlockColumn> towrite_;

};

} // namespace Blocks
} // namespace Seis


Task* Seis::Blocks::Writer::finisher()
{
    WriterFinisher* wf = new WriterFinisher( *this );
    if ( wf->towrite_.isEmpty() )
	{ delete wf; wf = 0; }
    return wf;
}
