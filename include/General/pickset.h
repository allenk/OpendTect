#ifndef pickset_h
#define pickset_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		May 2001 / Mar 2016
 Contents:	PickSet base classes
________________________________________________________________________

-*/

#include "picklocation.h"
#include "enums.h"
#include "namedobj.h"
#include "trckey.h"
#include "sets.h"
#include "draw.h"
#include "tableascio.h"
template <class T> class ODPolygon;


namespace Pick
{

class SetMgr;


/*!\brief Set of picks with something in common.

  The Set can be managed by a SetMgr. You will have to ask the SetMgr to do
  this, but you can designate the set to be part of a certain set manager.
  By default, this designation is the Pick::Mgr().

*/

mExpClass(General) Set : public NamedMonitorable, public TypeSet<Location>
{
public:

			Set(const char* nm=0,SetMgr* mgr=0);
			Set(const Set&);
			~Set();
    Set&		operator =(const Set&); //!< will not change SetMgr

    void		setSetMgr(SetMgr* mgr=0);
			//!< null for Pick::Mgr(). Will only add to new mgr
			//!< if set was already owned by its old mgr.
    SetMgr&		getSetMgr() const;
			//!< the SetMgr may not actually own this set (yet)

    bool		isMultiGeom() const;
    Pos::GeomID		firstgeomID() const;
    bool		has2D() const;
    bool		has3D() const;
    bool		hasOnly2D() const;
    bool		hasOnly3D() const;

    bool		isPolygon() const;
    void		getPolygon(ODPolygon<double>&) const;
    void		getLocations(ObjectSet<Location>&);
    void		getLocations(ObjectSet<const Location>&) const;
    float		getXYArea() const;
			//!<Only for closed polygons. Returns in m^2.
    size_type		find(const TrcKey&) const;
    size_type		nearestLocation(const Coord&) const;
    size_type		nearestLocation(const Coord3&,bool ignorez=false) const;

    static const char*	sKeyMarkerType()       { return "Marker Type"; }
    void		fillPar(IOPar&) const;
    bool		usePar(const IOPar&);

    void		removeSingleWithUndo(size_type idx);
    void		insertWithUndo(size_type,const Pick::Location&);
    void		appendWithUndo(const Pick::Location&);
    void		moveWithUndo(size_type,const Pick::Location&,
					const Pick::Location&);

    inline Location&	get( size_type idx )		{ return (*this)[idx]; }
    inline const Location& get( size_type idx ) const	{ return (*this)[idx]; }

    mExpClass(General) Disp
    {
    public:
	enum Connection { None, Open, Close };
			mDeclareEnumUtils(Connection);
			Disp() : connect_(None) {}

	Connection		connect_;	//!< connect picks in set order
	OD::MarkerStyle3D	mkstyle_;
    };
    Disp		disp_;
    IOPar&		pars_;

private:

    void		addUndoEvent(int,size_type,const Pick::Location&);
    mutable SetMgr*	mgr_;
    friend class	SetMgr;

};

/*!\brief ObjectSet of Pick::Location's. Does not manage. */

mExpClass(General) List : public ObjectSet<Location>
{
public:

			List()				{}
			List( const Pick::Set& ps )	{ addAll( ps ); }

    List&		add(const Pick::Location&,bool mkcopy=false);
    inline void		addAll( const Pick::Set& ps )
			{ const_cast<Pick::Set&>(ps).getLocations(*this); }

    bool		isMultiGeom() const;
    Pos::GeomID		firstgeomID() const;
    bool		has2D() const;
    bool		has3D() const;
    bool		hasOnly2D() const;
    bool		hasOnly3D() const;
    size_type		find(const TrcKey&) const;
    size_type		nearestLocation(const Coord&) const;
    size_type		nearestLocation(const Coord3&,bool ignorez=false) const;

    inline Location&	get( size_type idx )		{ return *(*this)[idx];}
    inline const Location& get( size_type idx ) const	{ return *(*this)[idx];}

};

} // namespace Pick


mExpClass(General) PickSetAscIO : public Table::AscIO
{
public:
				PickSetAscIO( const Table::FormatDesc& fd )
				    : Table::AscIO(fd)          {}

    static Table::FormatDesc*   getDesc(bool iszreq);
    static void			updateDesc(Table::FormatDesc&,bool iszreq);
    static void                 createDescBody(Table::FormatDesc*,bool iszreq);

    bool			isXY() const;
    bool			get(od_istream&,Pick::Set&,bool iszreq,
				    float zval) const;
};


#endif
