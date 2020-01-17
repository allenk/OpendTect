#ifndef uicreateattriblogdlg_h
#define uicreateattriblogdlg_h
/*+
 ________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Satyaki Maitra
 Date:          March 2008
 RCS:           $Id$
 _______________________________________________________________________

-*/


#include "uiwellattribmod.h"
#include "uidialog.h"
#include "bufstringset.h"
#include "createattriblog.h"

namespace Attrib { class DescSet; }
class NLAModel;
class uiAttrSel;
class uiListBox;
class uiGenInput;
class uiWellExtractParams;

mExpClass(uiWellAttrib) uiCreateAttribLogDlg : public uiDialog
{ mODTextTranslationClass(uiCreateAttribLogDlg);
public:
				uiCreateAttribLogDlg(uiParent*,
						     const BufferStringSet&,
					             const Attrib::DescSet*,
						     const NLAModel*,bool);
				~uiCreateAttribLogDlg();

protected:

    uiAttrSel*			attribfld_;
    uiListBox*			welllistfld_;
    uiGenInput*			lognmfld_;
    uiWellExtractParams*	zrangeselfld_;
    const BufferStringSet&	wellnames_;
    int			sellogidx_;
    bool			singlewell_;
    AttribLogCreator::Setup	datasetup_;

    bool                        inputsOK(int);
    bool			acceptOK(CallBacker*);
    void			init(CallBacker*);
    void			selDone(CallBacker*);
};

#endif
