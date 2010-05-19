#ifndef uiwelltiecontrolview_h
#define uiwelltiecontrolview_h

/*+
  ________________________________________________________________________

(C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
Author:        Bruno
Date:          Feb 2009
________________________________________________________________________

-*/

#include "uiflatviewstdcontrol.h"
#include "welltiepickset.h"


class uiFlatViewer;
class uiButton;
class uiToolBar;

namespace WellTie
{

mClass uiControlView : public uiFlatViewStdControl
{
public:
			uiControlView(uiParent*,uiToolBar*,uiFlatViewer*);
			~uiControlView(){};
   
    const bool 		isZoomAtStart() const;
    void 		setEditOn(bool);
    void		setPickSetMGR(WellTie::PickSetMGR* pmgr)
    			{ picksetmgr_ = pmgr; }
    void		setSelView(bool isnewsel = true, bool viewall=false );
    
protected:
    
    bool                manip_;
    
    uiToolBar*		toolbar_;
    uiToolButton*	manipdrawbut_;
    
    WellTie::PickSetMGR*  picksetmgr_;
    
    bool 		checkIfInside(double,double);
    void 		finalPrepare();
    bool 		handleUserClick();
   
    void 		altZoomCB(CallBacker*);
    void 		keyPressCB(CallBacker*);
    void		rubBandCB(CallBacker*);
    void 		wheelMoveCB(CallBacker*);

};

/*
mClass uiTieClippingDlg : public uiDialog
{
public:
				uiTieClippingDlg(uiParent*);
				~uiTieClippingDlg();

protected :

    mStruct ClipData
    {
	float			cliprate_;			
	bool			issynthetic_;			
	Interval<float> 	timerange_;
    };

    uiGenInput*                 tracetypechoicefld_;
    uiSliderExtra*              sliderfld_;
    uiGenInput*                 timerangefld_;

    ClipData 			cd_;

    void 			getFromScreen(Callbacker*);
    void 			putToScreen(Callbacker*);
    void 			applyClipping(CallBacker*);
};
*/

}; //namespace WellTie

#endif
