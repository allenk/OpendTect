/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Nanne Hemstra
 Date:          December 2003
 RCS:           $Id: visdragger.cc,v 1.5 2004-05-13 09:12:40 kristofer Exp $
________________________________________________________________________

-*/


#include "visdragger.h"
#include "vistransform.h"

#include <Inventor/draggers/SoDragPointDragger.h>
#include <Inventor/draggers/SoTranslate1Dragger.h>
#include <Inventor/nodes/SoSwitch.h>
#include <Inventor/nodes/SoSeparator.h>


mCreateFactoryEntry( visBase::Dragger );

namespace visBase
{

Dragger::Dragger()
    : dragger(0)
    , started(this)
    , motion(this)
    , finished(this)
    , onoff( new SoSwitch )
    , separator( new SoSeparator )
    , positiontransform( visBase::Transformation::create() )
    , displaytrans( 0 )
    , rightclicknotifier(this)
    , ownshape( 0 )
{
    onoff->ref();
    onoff->addChild( separator );

    positiontransform->ref();
    separator->addChild( positiontransform->getInventorNode() );
    setDraggerType( Translate );
    setRotation( Coord3(0,1,0), -M_PI/2 );
}


Dragger::~Dragger()
{
    if ( dragger )
    {
	dragger->removeStartCallback( visBase::Dragger::startCB, this );
	dragger->removeMotionCallback( visBase::Dragger::motionCB, this );
	dragger->removeFinishCallback( visBase::Dragger::finishCB, this );
	separator->removeChild( dragger );
    }

    separator->removeChild( positiontransform->getInventorNode() );
    positiontransform->unRef();
    onoff->unref();
    if ( displaytrans ) displaytrans->unRef();
    if ( ownshape ) ownshape->unRef();
}


bool Dragger::selectable() const { return true; }


void Dragger::setDraggerType( Type tp )
{
    if ( dragger )
    {
	dragger->removeStartCallback( visBase::Dragger::startCB, this );
	dragger->removeMotionCallback( visBase::Dragger::motionCB, this );
	dragger->removeFinishCallback( visBase::Dragger::finishCB, this );
	separator->removeChild( dragger );
    }
    
    if ( tp == DragPoint )
	dragger = new SoDragPointDragger;
    else
	dragger = new SoTranslate1Dragger;

    separator->addChild( dragger );
    dragger->addStartCallback( visBase::Dragger::startCB, this );
    dragger->addMotionCallback( visBase::Dragger::motionCB, this );
    dragger->addFinishCallback( visBase::Dragger::finishCB, this );
}


void Dragger::setTransformation( Transformation* nt )
{
    Coord3 pos = getPos();
    if ( displaytrans )
    {
	displaytrans->unRef();
	displaytrans = 0;
    }

    displaytrans = nt;
    if ( displaytrans )
    {
	displaytrans->ref();
    }

    setPos( pos );
}


Transformation* Dragger::getTransformation()
{
    return displaytrans;
}


void Dragger::setOwnShape(visBase::DataObject* newshape)
{
    if ( ownshape ) ownshape->unRef();
    ownshape = newshape;

    if ( ownshape && ownshape->getInventorNode() )
    {
	dragger->setPart("translator",ownshape->getInventorNode());
	dragger->setPart("translatorActive",ownshape->getInventorNode());
	ownshape->ref();
    }
    else
	ownshape=0;
}



void Dragger::startCB( void* obj, SoDragger* )
{
    ( (visBase::Dragger*)obj )->started.trigger();
}


void Dragger::motionCB( void* obj, SoDragger* )
{
    ( (visBase::Dragger*)obj )->motion.trigger();
}


void Dragger::finishCB( void* obj, SoDragger* )
{
    ( (visBase::Dragger*)obj )->finished.trigger();
}


void Dragger::turnOn( bool yn )
{
    onoff->whichChild = yn ? 0 : SO_SWITCH_NONE;
}


bool Dragger::isOn() const
{
    return !onoff->whichChild.getValue();
}


void Dragger::setSize( const Coord3& size )
{
    positiontransform->setScale( size );
}


Coord3 Dragger::getSize() const
{
    return positiontransform->getScale();
}


void Dragger::setRotation( const Coord3& vec, float angle )
{
    positiontransform->setRotation( vec, angle );
}


SoNode* Dragger::getInventorNode()
{
    return onoff;
}


void Dragger::setPos( const Coord3& pos )
{
    Coord3 newpos = displaytrans ? displaytrans->transform( pos ) : pos;
    positiontransform->setTranslation( newpos );

    mDynamicCastGet( SoTranslate1Dragger*, t1d, dragger )
    mDynamicCastGet( SoDragPointDragger*, dpd, dragger )
    if ( t1d ) t1d->translation.setValue( 0, 0, 0 );
    else if ( dpd ) dpd->translation.setValue( 0, 0, 0 );
}


Coord3 Dragger::getPos() const
{
    SbVec3f pos_;
    mDynamicCastGet( SoTranslate1Dragger*, t1d, dragger )
    mDynamicCastGet( SoDragPointDragger*, dpd, dragger )
    if ( t1d ) pos_ = t1d->translation.getValue();
    else pos_ = dpd->translation.getValue();

    Coord3 pos( pos_[1], pos_[2], pos_[0] );
    pos.x *= positiontransform->getScale()[0];
    pos.y *= positiontransform->getScale()[1];
    pos.z *= positiontransform->getScale()[2];

    Coord3 newpos = positiontransform->getTranslation();
    pos += newpos;
    return displaytrans ? displaytrans->transformBack( pos ) : pos;
}


void Dragger::setColor( const Color& col )
{
}


}; // namespace visBase
