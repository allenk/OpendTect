#ifndef uibatchprestackproc_h
#define uibatchprestackproc_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	K. Tingdahl
 Date:		March 2009
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uiprestackprocessingmod.h"
#include "uidialog.h"
class CtxtIOObj;
class uiSeisSel;
class uiPosSubSel;
class uiBatchJobDispatcherSel;


namespace PreStack
{
class uiProcSel;

/*!Dialog to setup a time->depth conversion for volumes on disk. */

mExpClass(uiPreStackProcessing) uiBatchProcSetup : public uiDialog
{ mODTextTranslationClass(uiBatchProcSetup);
public:

			uiBatchProcSetup(uiParent*,bool is2d);
			~uiBatchProcSetup();

protected:

    bool		fillPar();
    bool		prepareProcessing();
    void		outputNameChangeCB(CallBacker*);

    uiProcSel*		chainsel_;

    CtxtIOObj&		inputctxt_;
    uiSeisSel*		inputsel_;

    uiPosSubSel*	possubsel_;

    CtxtIOObj&		outputctxt_;
    uiSeisSel*		outputsel_;
    bool		is2d_;

    uiBatchJobDispatcherSel* batchfld_;

    bool		acceptOK(CallBacker*);

};

}; //namespace


#endif
