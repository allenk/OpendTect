#ifndef propertyimpl_h
#define propertyimpl_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Bert Bril
 Date:		Jan 2004
 RCS:		$Id: mathproperty.h,v 1.2 2005-01-25 16:29:18 bert Exp $
________________________________________________________________________


-*/

#include "property.h"
class MathExpression;


/*!\brief Simple property */

class ValueProperty : public Property
{
public:

    			ValueProperty( const PropertyRef* pr,
				       float v=mUndefValue )
			: Property(pr), val_(v)		{}

    virtual float	value() const		{ return val_; }

    float		val_;

};


/*!\brief Calculated property */

class MathProperty : public Property
{
public:
    			MathProperty( const PropertyRef* pr, const char* ds=0 )
			: Property(pr), expr_(0)
			{ inps_.allowNull(true); setDef(ds); }
			~MathProperty();

    const char*		def() const			{ return def_.buf(); }
    void		setDef(const char*);

    int			nrInputs() const;
    const char*		inputName(int) const;
    void		setInput(int,const Property*);
    			//!< Must be done for all inputs after each setDef()

    virtual float	value() const;
    virtual bool	dependsOn(const Property*) const;

protected:

    BufferString		def_;
    MathExpression*		expr_;
    ObjectSet<const Property>	inps_;

};


/*!\brief Property defined by other property */

class IndirectProperty : public Property
{
public:
    			IndirectProperty( const Property* pr )
			: Property(pr->ref()), pr_(pr)	{}

    virtual float	value() const
    			{ return pr_->value(); }
    virtual bool        dependsOn( const Property* pr ) const
    			{ return pr_->dependsOn(pr); }

    Property*		pr_;
};


#endif
