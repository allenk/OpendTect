/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        Nanne Hemstra
 Date:          January 2002
 RCS:           $Id: uibatchprogs.cc,v 1.12 2003-11-12 12:57:04 bert Exp $
________________________________________________________________________

-*/

#include "uibatchprogs.h"
#include "uifilebrowser.h"
#include "uifileinput.h"
#include "uicombobox.h"
#include "uitextedit.h"
#include "uibutton.h"
#include "uilabel.h"
#include "uimsg.h"
#include "ascstream.h"
#include "separstr.h"
#include "strmprov.h"
#include "filegen.h"
#include "iopar.h"
#include "errh.h"
#include <iostream>


class BatchProgPar
{
public:

    			BatchProgPar(const char*);

    enum Type		{ File, Words, QWord };

    Type		type;
    bool		mandatory;
    BufferString	desc;

    static Type		getType( const char* s )
			{
			    return *s == 'W' ? Words
				: (*s == 'Q' ? QWord
					     : File);
			}
};


BatchProgPar::BatchProgPar( const char* val )
{
    FileMultiString fms( val );
    mandatory = *fms[0] == 'M';
    type = getType( fms[1] );
    desc = fms[2];
}


class BatchProgInfo
{
public:

				BatchProgInfo( const char* nm )
				: name(nm), issys(false) {}
				~BatchProgInfo()	 { deepErase(args); }

    BufferString		name;
    ObjectSet<BatchProgPar>	args;
    BufferString		comments;
    BufferString		exampleinput;
    bool			issys;

};


class BatchProgInfoList : public ObjectSet<BatchProgInfo>
{
public:

			BatchProgInfoList(const char*);
			~BatchProgInfoList()	{ deepErase(*this); }
};


BatchProgInfoList::BatchProgInfoList( const char* appnm )
{
    BufferString fnm( "BatchPrograms" );
    if ( appnm && *appnm )
	{ fnm += "."; fnm += appnm; }
    fnm = GetDataFileName( fnm );
    if ( File_isEmpty(fnm.buf()) ) return;

    StreamData sd = StreamProvider( fnm ).makeIStream();
    if ( !sd.usable() ) return;

    ascistream astrm( *sd.istrm, true );

    while ( astrm.type() != ascistream::EndOfFile )
    {
	if ( atEndOfSection(astrm.next()) )
	    continue;

	const bool issys = astrm.hasValue("Sys");
	BatchProgInfo* bpi = new BatchProgInfo(astrm.keyWord());
	if ( issys ) bpi->issys = true;

	while ( !atEndOfSection(astrm.next()) )
	{
	    if ( astrm.hasKeyword("ExampleInput") )
		bpi->exampleinput = astrm.value();
	    else if ( astrm.hasKeyword("Arg") )
		bpi->args += new BatchProgPar( astrm.value() );
	    else if ( astrm.hasKeyword("Comment") )
	    {
		char* ptr = const_cast<char*>(astrm.value());
		while ( *ptr && *ptr == '>' ) *ptr++ = ' ';
		if ( bpi->comments != "" )
		    bpi->comments += "\n";
		bpi->comments += astrm.value();
	    }
	}

	if ( bpi ) *this += bpi;
    }
    sd.close();
}


uiBatchProgLaunch::uiBatchProgLaunch( uiParent* p, const char* appnm )
        : uiDialog(p,uiDialog::Setup("Run batch program",
		   "Specify batch program and parameters","0.1.5"))
	, pil(*new BatchProgInfoList(appnm))
	, progfld(0)
	, browser(0)
	, exbut(0)
{
    if ( pil.size() < 1 )
    {
	new uiLabel( this, "No 'BatchPrograms[.xxx]' file present" );
	return;
    }

    progfld = new uiLabeledComboBox( this, "Batch program" );
    for ( int idx=0; idx<pil.size(); idx++ )
	progfld->box()->addItem( pil[idx]->name );
    progfld->box()->setCurrentItem( 0 );
    progfld->box()->selectionChanged.notify(
	    		mCB(this,uiBatchProgLaunch,progSel) );

    commfld = new uiTextEdit( this, "Comments" );
    commfld->attach( centeredBelow, progfld );
    commfld->setPrefHeightInChar( 5 );
    commfld->setPrefWidth( 400 );

    BufferString startdir( GetDataDir() );
    for ( int ibpi=0; ibpi<pil.size(); ibpi++ )
    {
	const BatchProgInfo& bpi = *pil[ibpi];
	ObjectSet<uiGenInput>* inplst = new ObjectSet<uiGenInput>;
	inps += inplst;
	for ( int iarg=0; iarg<bpi.args.size(); iarg++ )
	{
	    BufferString txt;
	    const BatchProgPar& bpp = *bpi.args[iarg];
	    if ( !bpp.mandatory ) txt += "[";
	    txt += bpp.desc;
	    if ( !bpp.mandatory ) txt += "]";

	    uiGenInput* newinp;
	    if ( bpp.type != BatchProgPar::File )
		newinp = new uiGenInput( this, txt );
	    else
	    {
		uiFileInput* fi = new uiFileInput( this, txt );
		newinp = fi;
		fi->setDefaultSelectionDir( startdir );
		BufferString filt( "*" );
		if ( bpp.desc == "Parameter file" )
		    filt += ";;*.par";
		fi->setFilter( filt );
	    }

	    if ( iarg )
		newinp->attach( alignedBelow, (*inplst)[iarg-1] );
	    else
	    {
		newinp->attach( alignedBelow, progfld );
		newinp->attach( ensureBelow, commfld );
	    }
	    (*inplst) += newinp;
	}
	if ( bpi.exampleinput != "" )
	{
	    if ( !exbut )
	    {
		exbut = new uiPushButton( this, "Show example input ...",
				mCB(this,uiBatchProgLaunch,exButPush) );
		if ( inplst->size() )
		    exbut->attach( alignedBelow, (*inplst)[inplst->size()-1] );
	    }
	    else if ( inplst->size() )
		exbut->attach( ensureBelow, (*inplst)[inplst->size()-1] );
	}
    }


    finaliseDone.notify( mCB(this,uiBatchProgLaunch,progSel) );
}


uiBatchProgLaunch::~uiBatchProgLaunch()
{
    if ( browser ) browser->reject(0);
    delete &pil;
}


void uiBatchProgLaunch::progSel( CallBacker* )
{
    const int selidx = progfld->box()->currentItem();
    const BatchProgInfo& bpi = *pil[selidx];
    commfld->setText( bpi.comments );

    for ( int ilst=0; ilst<inps.size(); ilst++ )
    {
	ObjectSet<uiGenInput>& inplst = *inps[ilst];
	for ( int iinp=0; iinp<inplst.size(); iinp++ )
	    inplst[iinp]->display( ilst == selidx );
    }

    if ( exbut )
	exbut->display( bpi.exampleinput != "" );
}


void uiBatchProgLaunch::exButPush( CallBacker* )
{
    const int selidx = progfld->box()->currentItem();
    const BatchProgInfo& bpi = *pil[selidx];
    if ( bpi.exampleinput == "" )
	{ pErrMsg("In CB that shouldn't be called for entry"); return; }
    BufferString sourceex( GetDataFileName(bpi.exampleinput) );
    if ( File_isEmpty(sourceex) )
	{ pErrMsg("Installation problem"); return; }

    BufferString targetex( GetDataDir() );
    targetex = File_getFullPath( targetex, "Proc" );
    targetex = File_getFullPath( targetex, bpi.exampleinput );
    if ( !File_exists(targetex) )
    {
	File_copy( sourceex, targetex, NO );
	File_makeWritable( targetex, NO, YES );
    }

    if ( browser )
	browser->setFileName( targetex );
    else
    {
	browser = new uiFileBrowser( this, uiFileBrowser::Setup(targetex)
							.readonly(false) );
	browser->newfilenm.notify( mCB(this,uiBatchProgLaunch,filenmUpd) );
    }
    browser->show();
}


bool uiBatchProgLaunch::acceptOK( CallBacker* )
{
    if ( !progfld ) return true;

    const int selidx = progfld->box()->currentItem();
    const BatchProgInfo& bpi = *pil[selidx];

    BufferString comm( "@" );
    comm += mGetExecScript();

    comm += " --inxterm+askclose ";
    if ( bpi.issys ) comm += "--sys ";
    comm += progfld->box()->text();

    ObjectSet<uiGenInput>& inplst = *inps[selidx];
    for ( int iinp=0; iinp<inplst.size(); iinp++ )
    {
	uiGenInput* inp = inplst[iinp];
	mDynamicCastGet(uiFileInput*,finp,inp)
	BufferString val;
	if ( finp )
	    val = finp->fileName();
	else if ( bpi.args[iinp]->type != BatchProgPar::QWord )
	    val = inp->text();
	else
	    { val = "'"; val += inp->text(); val += "'"; }

	comm += " ";
	comm += val;
    }

    StreamProvider sp( comm );
    return sp.executeCommand( true );
}


void uiBatchProgLaunch::filenmUpd( CallBacker* cb )
{
    mDynamicCastGet(uiFileBrowser*,fb,cb)
    if ( !fb ) return;

    const int selidx = progfld->box()->currentItem();

    ObjectSet<uiGenInput>& inplst = *inps[selidx];
    for ( int iinp=0; iinp<inplst.size(); iinp++ )
    {
	uiGenInput* inp = inplst[iinp];
	mDynamicCastGet(uiFileInput*,finp,inp)
	BufferString val;
	if ( finp )
	    finp->setText( fb->name() );
    }
}

