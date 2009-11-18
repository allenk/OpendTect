#ifndef odusgclient_h
#define odusgclient_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Mar 2009
 RCS:           $Id: odusgclient.h,v 1.3 2009-11-18 15:00:02 cvsbert Exp $
________________________________________________________________________

-*/

#include "odusginfo.h"


namespace Usage
{
class Info;

mClass Client
{
public:

    			Client( const char* grp )
			    : usginfo_(grp)	{}
    virtual		~Client()		{}

    virtual void	prepUsgStart( const char* act=0 )
			{ usginfo_.prepStart(act); }
    virtual void	prepUsgEnd( const char* act=0 )
			{ usginfo_.prepEnd(act); }

protected:

    Info		usginfo_;

    virtual bool	sendUsgInfo();

};


} // namespace


#endif
