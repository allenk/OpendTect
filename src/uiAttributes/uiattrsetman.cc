/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          September 2003
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiattrsetman.cc,v 1.8 2009-09-01 08:50:45 cvshelene Exp $";

#include "uiattrsetman.h"

#include "uiioobjsel.h"
#include "uitextedit.h"

#include "attribdesc.h"
#include "attribdescset.h"
#include "attribdescsettr.h"
#include "ctxtioobj.h"
#include "survinfo.h"

static const int cPrefWidth = 75;

uiAttrSetMan::uiAttrSetMan( uiParent* p )
    : uiObjFileMan(p,uiDialog::Setup("Attribute Set file management",
				     "Manage attribute sets",
				     "101.3.3").nrstatusflds(1),
	           AttribDescSetTranslatorGroup::ioContext())
{
    createDefaultUI();
    selgrp->setPrefWidthInChar( cPrefWidth );
    infofld->setPrefWidthInChar( cPrefWidth );

    selChg( this );
}


uiAttrSetMan::~uiAttrSetMan()
{
}


static void addAttrNms( const Attrib::DescSet& attrset, BufferString& txt,
			bool stor )
{
    const int totnrdescs = attrset.nrDescs( true, true );
    int nrdisp = 0;
    for ( int idx=0; idx<totnrdescs; idx++ )
    {
	const Attrib::Desc* desc = attrset.desc( idx );
	if ( desc->isHidden()
	  || (stor && !desc->isStored()) || (!stor && desc->isStored()) )
	    continue;

	if ( nrdisp > 0 )
	    txt += ", ";
	txt += desc->userRef();

	nrdisp++;
	if ( nrdisp > 2 )
	    { txt += ", ..."; break; }
    }
}


void uiAttrSetMan::mkFileInfo()
{
    if ( !curioobj_ ) { infofld->setText( "" ); return; }

    BufferString txt;
    Attrib::DescSet attrset( !SI().has3D() );
    if ( !AttribDescSetTranslator::retrieve(attrset,curioobj_,txt) )
    {
	BufferString msg( "Read error: '" ); msg += txt; msg += "'";
	txt = msg;
    }
    else
    {
	if ( !txt.isEmpty() )
	    ErrMsg( txt );

	const int nrattrs = attrset.nrDescs( false, false );
	const int nrwithstor = attrset.nrDescs( true, false );
	const int nrstor = nrwithstor - nrattrs;
	txt = "Type: "; txt += attrset.is2D() ? "2D" : "3D";
	if ( nrstor > 0 )
	{
	    txt += "\nInput"; txt += nrstor == 1 ? ": " : "s: "; 
	    addAttrNms( attrset, txt, true );
	}
	if ( nrattrs < 1 )
	    txt += "\nNo attributes defined";
	else
	{
	    txt += "\nAttribute"; txt += nrattrs == 1 ? ": " : "s: "; 
	    addAttrNms( attrset, txt, false );
	}
    }

    txt += "\n";
    txt += getFileInfo();

    infofld->setText( txt );
}
