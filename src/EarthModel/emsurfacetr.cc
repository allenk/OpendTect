/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        K. Tingdahl
 Date:          May 2002
 RCS:           $Id: emsurfacetr.cc,v 1.11 2006-05-24 07:46:56 cvskris Exp $
________________________________________________________________________

-*/

#include "emsurfacetr.h"
#include "emsurfaceio.h"

#include "ascstream.h"
#include "datachar.h"
#include "datainterp.h"
#include "emsurfauxdataio.h"
#include "executor.h"
#include "filegen.h"
#include "ioobj.h"
#include "iopar.h"
#include "iostrm.h"
#include "strmprov.h"
#include "ptrman.h"
#include "survinfo.h"
#include "cubesampling.h"
#include "settings.h"


const char* EMHorizonTranslatorGroup::keyword = "Horizon";

const IOObjContext& EMHorizonTranslatorGroup::ioContext()
{
    static IOObjContext* ctxt = 0;

    if ( !ctxt )
    {
	ctxt = new IOObjContext( 0 );
	ctxt->crlink = false;
	ctxt->newonlevel = 1;
	ctxt->needparent = false;
	ctxt->maychdir = false;
	ctxt->stdseltype = IOObjContext::Surf;
    }

    // Due to static initialisation order ...
    ctxt->trgroup = &theInst();
    return *ctxt;
}


int EMHorizonTranslatorGroup::selector( const char* key )
{
    return defaultSelector( keyword, key );
}


const char* EMHorizon2DTranslatorGroup::keyword = "Horizon 2D";

const IOObjContext& EMHorizon2DTranslatorGroup::ioContext()
{
    static IOObjContext* ctxt = 0;

    if ( !ctxt )
    {
	ctxt = new IOObjContext( 0 );
	ctxt->crlink = false;
	ctxt->newonlevel = 1;
	ctxt->needparent = false;
	ctxt->maychdir = false;
	ctxt->stdseltype = IOObjContext::Surf;
    }

    // Due to static initialisation order ...
    ctxt->trgroup = &theInst();
    return *ctxt;
}


int EMHorizon2DTranslatorGroup::selector( const char* key )
{
    return defaultSelector( keyword, key );
}


const char* EMFaultTranslatorGroup::keyword = "Fault";

const IOObjContext& EMFaultTranslatorGroup::ioContext()
{
    static IOObjContext* ctxt = 0;

    if ( !ctxt )
    {
	ctxt = new IOObjContext( &theInst() );
	ctxt->crlink = false;
	ctxt->newonlevel = 1;
	ctxt->needparent = false;
	ctxt->maychdir = false;
	ctxt->stdseltype = IOObjContext::Surf;
    }

    return *ctxt;
}


int EMFaultTranslatorGroup::selector( const char* key )
{
    return defaultSelector( keyword, key );
}

// *************************************************************************

EMSurfaceTranslator::~EMSurfaceTranslator()
{
    delete ioobj_;
}


void EMSurfaceTranslator::init( const EM::Surface* surf, const IOObj* ioobj )
{
    surface = const_cast<EM::Surface*>(surf);
    setIOObj( ioobj );
}


void EMSurfaceTranslator::setIOObj( const IOObj* ioobj )
{
    delete ioobj_;
    ioobj_ = ioobj ? ioobj->clone() : 0;
}


bool EMSurfaceTranslator::startRead( const IOObj& ioobj )
{
    init( 0, &ioobj );
    if ( !prepRead() )
	return false;
    sels_.setDefault();
    return true;
}


bool EMSurfaceTranslator::startWrite( const EM::Surface& surf )
{
    init( &surf, 0 );
    sd_.use( surf );
    if ( !prepWrite() )
	return false;
    sels_.setDefault();
    return true;
}


Executor* EMSurfaceTranslator::writer( const IOObj& ioobj )
{
    setIOObj( &ioobj );
    Executor* ret = getWriter();
    if ( ret )
	fullImplRemove( ioobj );
    return ret;
}


#define mImplStart(fn) \
    if ( !ioobj || strcmp(ioobj->translator(),"dGB") ) return false; \
    mDynamicCastGet(const IOStream*,iostrm,ioobj) \
    if ( !iostrm ) return false; \
 \
    BufferString pathnm = iostrm->dirName(); \
    BufferString basenm = iostrm->fileName(); \
 \
    StreamProvider sp( basenm ); \
    sp.addPathIfNecessary( pathnm ); \
    if ( !sp.fn ) return false;

#define mImplLoopStart \
    if ( gap > 100 ) return true; \
    StreamProvider loopsp( EM::dgbSurfDataWriter::createHovName(basenm,nr) ); \
    loopsp.addPathIfNecessary( pathnm );


bool EMSurfaceTranslator::implRemove( const IOObj* ioobj ) const
{
    mImplStart(remove(false));

    int gap = 0;
    for ( int nr=0; ; nr++ )
    {
	mImplLoopStart;
	if ( loopsp.exists(true) )
	    loopsp.remove(false);
	else
	    gap++;
    }

    return true;
}


bool EMSurfaceTranslator::implRename( const IOObj* ioobj, const char* newnm,
				      const CallBack* cb ) const
{
    mImplStart(rename(newnm,cb));

    int gap = 0;
    for ( int nr=0; ; nr++ )
    {
	mImplLoopStart;
	StreamProvider spnew( EM::dgbSurfDataWriter::createHovName(newnm,nr) );
	spnew.addPathIfNecessary( pathnm );
	if ( loopsp.exists(true) )
	    loopsp.rename( spnew.fileName(), cb );
	else
	    gap++;
    }

    return true;
}


bool EMSurfaceTranslator::implSetReadOnly( const IOObj* ioobj, bool ro ) const
{
    mImplStart(setReadOnly(ro));

    return true;
}


bool EMSurfaceTranslator::getBinarySetting()
{
    bool isbinary = true;
    mSettUse(getYN,"dTect.Surface","Binary format",isbinary);
    return isbinary;
}


dgbEMSurfaceTranslator::dgbEMSurfaceTranslator( const char* nm, const char* unm)
    : EMSurfaceTranslator(nm,unm)
    , reader_(0)
{
}


dgbEMSurfaceTranslator::~dgbEMSurfaceTranslator()
{
    delete reader_;
}


bool dgbEMSurfaceTranslator::prepRead()
{
    if ( reader_ ) delete reader_;
    BufferString unm = group() ? group()->userName() : 0;
    reader_ = new EM::dgbSurfaceReader( *ioobj_, unm );
    if ( !reader_->isOK() )
    {
	errmsg_ = reader_->message();
	return false;
    }

    for ( int idx=0; idx<reader_->nrSections(); idx++ )
	sd_.sections += new BufferString( reader_->sectionName(idx) );
    
    for ( int idx=0; idx<reader_->nrAuxVals(); idx++ )
	sd_.valnames += new BufferString( reader_->auxDataName(idx) );

    const int version = reader_->version();
    if ( version==1 )
    {
	sd_.rg.start = SI().sampling(false).hrg.start;
	sd_.rg.stop = SI().sampling(false).hrg.stop;
	sd_.rg.step = SI().sampling(false).hrg.step;
    }
    else
    {
	sd_.rg.start.inl = reader_->rowInterval().start;
	sd_.rg.stop.inl = reader_->rowInterval().stop;
	sd_.rg.step.inl = reader_->rowInterval().step;
	sd_.rg.start.crl = reader_->colInterval().start;
	sd_.rg.stop.crl = reader_->colInterval().stop;
	sd_.rg.step.crl = reader_->colInterval().step;

	sd_.dbinfo = reader_->dbInfo();
    }

    reader_->setReadOnlyZ(  readOnlyZ() );

    return true;
}


void dgbEMSurfaceTranslator::getSels( StepInterval<int>& rrg,
				      StepInterval<int>& crg )
{
    if ( sels_.rg.isEmpty() )
	sels_.rg = sd_.rg;

    rrg.start = sels_.rg.start.inl; rrg.stop = sels_.rg.stop.inl;
    rrg.step = sels_.rg.step.inl;
    crg.start = sels_.rg.start.crl; crg.stop = sels_.rg.stop.crl;
    crg.step = sels_.rg.step.crl;
}


Executor* dgbEMSurfaceTranslator::reader( EM::Surface& surf )
{
    surface = &surf;
    Executor* res = reader_;
    if ( reader_ )
    {
	reader_->setOutput( surf );
	if ( hasRangeSelection() )
	{
	    StepInterval<int> rrg, crg; getSels( rrg, crg );
	    reader_->setRowInterval( rrg ); reader_->setColInterval( crg );
	}
	TypeSet<EM::SectionID> sectionids;
	for ( int idx=0; idx<sels_.selsections.size(); idx++ )
	    sectionids += reader_->sectionID( sels_.selsections[idx] );
	
	reader_->selSections( sectionids );
	reader_->selAuxData( sels_.selvalues );
    }

    reader_ = 0;
    return res;
}

Executor* dgbEMSurfaceTranslator::getWriter()
{
    BufferString unm = group() ? group()->userName() : 0;
    EM::dgbSurfaceWriter* res =
	new EM::dgbSurfaceWriter(ioobj_,unm, *surface,getBinarySetting());
    res->setWriteOnlyZ( writeOnlyZ() );

    if ( hasRangeSelection() && !sels_.rg.isEmpty() )
    {
	StepInterval<int> rrg, crg; getSels( rrg, crg );
	res->setRowInterval( rrg ); res->setColInterval( crg );
    }
    TypeSet<EM::SectionID> sectionids;
    for ( int idx=0; idx<sels_.selsections.size(); idx++ )
	sectionids += res->sectionID( sels_.selsections[idx] );
    
    res->selSections( sectionids );
    res->selAuxData( sels_.selvalues );

    return res;
}
