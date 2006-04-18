/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Helene Payraudeau
 Date:          February 2006
 RCS:           $Id: fingerprintattrib.cc,v 1.1 2006-04-18 11:09:05 cvshelene Exp $
________________________________________________________________________

-*/

#include "fingerprintattrib.h"

#include "attribdataholder.h"
#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "attribparamgroup.h"

#include <math.h>


namespace Attrib
{

void FingerPrint::initClass()
{
    Desc* desc = new Desc( attribName(), updateDesc );
    desc->ref();

    BinIDParam* refpos = new BinIDParam( refposStr() );
    refpos->setRequired( false );
    desc->addParam( refpos );
    
    FloatParam* refposz = new FloatParam( refposzStr() );
    refposz->setRequired( false );
    desc->addParam( refposz );

    FloatParam value( valStr() );
    ParamGroup<FloatParam>* valueset = 
				new ParamGroup<FloatParam>(0,valStr(),value);
    desc->addParam( valueset );

    desc->addInput( InputSpec("Input data",true) );
    desc->addOutputDataType( Seis::UnknowData );

    PF().addDesc( desc, createInstance );
    desc->unRef();
}


Provider* FingerPrint::createInstance( Desc& dsc )
{
    FingerPrint* res = new FingerPrint( dsc );
    res->ref();

    if ( !res->isOK() )
    {
	res->unRef();
	return 0;
    }

    res->unRefNoDelete();
    return res;
}


void FingerPrint::updateDesc( Desc& desc )
{
    mDescGetParamGroup(FloatParam,valueset,desc,valStr())

    while ( desc.nrInputs() )
	desc.removeInput(0);

    for ( int idx=0; idx<valueset->size(); idx++ )
    {
	BufferString bfs = "Input Data"; bfs += idx+1;
	desc.addInput( InputSpec(bfs, true) );
    }
}


FingerPrint::FingerPrint( Desc& dsc )
    : Provider( dsc )
{
    if ( !isOK() ) return;

    inputdata_.allowNull(true);

    mGetBinID( refpos_, refposStr() )
    mGetFloat( refposz_, refposzStr() )

    mDescGetParamGroup(FloatParam,valueset,desc,valStr())
    for ( int idx=0; idx<valueset->size(); idx++ )
    {
	const ValParam& param = (ValParam&)(*valueset)[idx];
	vector_ += param.getfValue(0);
    }
}


bool FingerPrint::getInputData( const BinID& relpos, int zintv )
{
    while ( inputdata_.size() < inputs.size() )
	inputdata_ += 0;

    for ( int idx=0; idx<inputs.size(); idx++ )
    {
	const DataHolder* data = inputs[idx]->getData( relpos, zintv );
	if ( !data ) return false;
	inputdata_.replace( idx, data );
	dataidx_ += getDataIndex( idx );
    }
    
    return true;
}


bool FingerPrint::computeData( const DataHolder& output, const BinID& relpos, 
			      int z0, int nrsamples ) const
{
    if ( !inputdata_.size() || !outputinterest[0] ) return false;

    for ( int idx=0; idx<nrsamples; idx++ )
    {
	const int cursample = idx + z0;
	float val = 0;
	for ( int inpidx=0; inpidx<inputdata_.size(); inpidx++ )
	{
	    const float localval = inputdata_[inpidx]->series(dataidx_[inpidx])
				->value( cursample-inputdata_[inpidx]->z0_ );
	    const float denom = mIsZero( vector_[inpidx], 0.001 ) ?
				vector_[inpidx] * vector_[inpidx] : 0.00001;
	    val += (vector_[inpidx]-localval)*(vector_[inpidx]-localval)/denom;
	}

    
	val = sqrt( val )/inputdata_.size();
	output.series(0)->setValue( cursample-output.z0_, val );
    }

    return true;
}

}; //namespace
