#ifndef visscenecoltab_h
#define visscenecoltab_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Nanne Hemstra
 Date:		August 2008
 RCS:		$Id: visscenecoltab.h,v 1.3 2008-08-29 10:48:53 cvsnanne Exp $
________________________________________________________________________

-*/

#include "visobject.h"

class LegendKit;
namespace ColTab { class Sequence; }

template<class T> class Interval;

namespace visBase
{

class SceneColTab : public VisualObjectImpl
{
public:
    static SceneColTab*		create()
				mCreateDataObj(SceneColTab);

    void			setColTabID(int);
    void			setColTabSequence(const ColTab::Sequence&);
    void			setRange(const Interval<float>&);

    void			setDisplayTransformation(Transformation*) {}

protected:
				~SceneColTab();

    LegendKit*			legendkit_;
};

} // class visBase


#endif
