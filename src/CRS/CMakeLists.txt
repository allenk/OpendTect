#_______________________Pmake___________________________________________________
#
#	Makefile :	CRS
#	May 2017	Raman
#	RCS :		$Id$
#_______________________________________________________________________________

set( OD_USEPROJ4 1 )
set( OD_MODULE_DEPS Basic )
set( OD_MODULE_SOURCES 
	    crsproj.cc
	    crssystem.cc
	    initcrs.cc )

set( OD_TEST_PROGS
	    crs.cc )

OD_INIT_MODULE()
