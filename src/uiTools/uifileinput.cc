/*+
________________________________________________________________________

 CopyRight:     (C) de Groot-Bril Earth Sciences B.V.
 Author:        A.H. Lammertink
 Date:          08/08/2000
 RCS:           $Id: uifileinput.cc,v 1.15 2003-04-01 12:16:39 bert Exp $
________________________________________________________________________

-*/

#include "uifileinput.h"
#include "uifiledlg.h"
#include "uilabel.h"
#include "uibutton.h"
#include "uigeninput.h"
#include "filegen.h"


uiFileInput::uiFileInput( uiParent* p, const char* txt, const char* fnm,
			  bool fr, const char* filt )
    : uiGenInput( p, txt, FileNameInpSpec(fnm) )
    , forread(fr)
    , fname( fnm )
    , filter(filt)
    , newfltr(false)
    , selmodset(false)
    , selmode(uiFileDialog::AnyFile)
{
    setWithSelect( true );
}


void uiFileInput::setFileName( const char* s )
{
    setText( s );
}


void uiFileInput::doSelect( CallBacker* )
{
    fname = text();
    if ( fname == "" )	fname = defseldir;
    if ( newfltr )	filter = selfltr;

    uiFileDialog dlg( this, forread, fname, filter );

    if ( selmodset )
	dlg.setMode( selmode );

    if ( dlg.go() )
    {
	BufferString oldfname( fname );
	BufferString newfname( dlg.fileName() );
	setFileName( newfname );
	if ( newfname != oldfname )
	    valuechanged.trigger( *this );
    }
}


const char* uiFileInput::fileName()
{
    fname = text();
    if ( !File_isAbsPath(fname) && fname != "" && defseldir != "" )
	fname = File_getFullPath( defseldir, fname );
    return fname;
}
