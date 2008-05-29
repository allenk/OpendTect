/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : K. Tingdahl
 * DATE     : Oct 1999
-*/

static const char* rcsID = "$Id: trigonometry.cc,v 1.35 2008-05-29 17:53:38 cvsyuancheng Exp $";

#include "trigonometry.h"

#include "errh.h"
#include "math2.h"
#include "pca.h"
#include "position.h"
#include "sets.h"

#include <math.h>


TypeSet<Vector3>* makeSphereVectorSet( double dradius )
{
    TypeSet<Vector3>& vectors(*new TypeSet<Vector3>);


    const int nrdips = mNINT(M_PI_2/dradius)+1;
    const double ddip = M_PI_2/(nrdips-1);

    for ( int dipidx=0; dipidx<nrdips; dipidx++ )
    {
	const double dip = ddip*dipidx;
	static const double twopi = M_PI*2;
	const double radius = cos(dip);
	const double perimeter = twopi*radius;

	int nrazi = mNINT((dipidx ? perimeter : M_PI)/dradius);
	//!< No +1 since it's comes back to 0 when angle is 2*PI
	//!< The dipidx ? stuff is to avoid multiples 
	if ( !nrazi ) nrazi = 1;
	const double dazi = (dipidx ? twopi : M_PI)/nrazi;

	for ( int aziidx=0; aziidx<nrazi; aziidx++ )
	{
	    double azi = aziidx*dazi;
	    vectors += Vector3( cos(azi)*radius, sin(azi)*radius, sin(dip));
	}
    }

    return &vectors;
}


Coord3 estimateAverageVector( const TypeSet<Coord3>& vectors, bool normalize,
       		 	      bool checkforundefs )
{
    const int nrvectors = vectors.size();
    TypeSet<Coord3> ownvectors;
    if ( normalize || checkforundefs )
    {
	for ( int idx=0; idx<nrvectors; idx++ )
	{
	    const Coord3& vector = vectors[idx];
	    if ( checkforundefs && !vector.isDefined() )
		continue;

	    const double len = vector.abs();
	    if ( mIsZero(len,mDefEps) )
		continue;

	    ownvectors += normalize ? vector/len : vector;
	}
    }

    const TypeSet<Coord3>& usedvectors =  normalize || checkforundefs
					 ? ownvectors : vectors;

    static const Coord3 udfcrd3( mUdf(double), mUdf(double), mUdf(double) );
    const int nrusedvectors = usedvectors.size();
    if ( !nrusedvectors )
	return udfcrd3;

    if ( nrusedvectors==1 )
	return usedvectors[0];

    Coord3 average(0,0,0);
    for ( int idx=0; idx<nrusedvectors; idx++ )
	average += usedvectors[idx];

    const double avglen = average.abs();
    if ( !mIsZero(avglen,mDefEps) )
	return average/avglen;

    PCA pca(3);
    for ( int idx=0; idx<nrusedvectors; idx++ )
	pca.addSample( usedvectors[idx] );
    
    if ( !pca.calculate() )
	return udfcrd3;

    Coord3 res;
    pca.getEigenVector(0, res );

    int nrnegative = 0;
    for ( int idx=0; idx<nrusedvectors; idx++ )
    {
	if ( res.dot(usedvectors[idx])<0 )
	    nrnegative++;
    }

    if ( nrnegative*2> nrusedvectors )
	return -res;

    return res;
}


Quaternion::Quaternion( float s, float x, float y, float z )
    : vec_( x, y, z )
    , s_( s )
{ }


Quaternion::Quaternion( const Vector3& axis, float angle )
{
    setRotation( axis, angle );
}


void Quaternion::setRotation( const Vector3& axis, float angle )
{
    const float halfangle = angle/2;
    s_ = cos(halfangle);
    const float sineval = sin(halfangle);

    const Coord3 a = axis.normalize();

    vec_.x = a.x * sineval;
    vec_.y = a.y * sineval;
    vec_.z = a.z * sineval;
}


void Quaternion::getRotation( Vector3& axis, float& angle ) const
{
    if ( s_>=1 || s_<=-1 ) angle = 0;
    else angle = Math::ACos( s_ ) * 2;

    //This should really be axis=vec_/sin(angle/2)
    //but can be simplified to this since length of axis is irrelevant
    axis = vec_;
}


Coord3 Quaternion::rotate( const Coord3& v ) const
{
    const Coord3 qvv = s_*v + vec_.cross(v);

    const Coord3 iqvec = -vec_;

    return (iqvec.dot(v))*iqvec+s_*qvv+qvv.cross(iqvec);
}
    

Quaternion Quaternion::operator+( const Quaternion& b ) const
{
    const Vector3 vec = vec_+b.vec_;
    return Quaternion( s_+b.s_, vec.x, vec.y, vec.z );
}


Quaternion& Quaternion::operator+=( const Quaternion& b )
{
    (*this) = (*this) + b;
    return *this;
}



Quaternion Quaternion::operator-( const Quaternion& b ) const
{
    const Vector3 vec =  vec_-b.vec_;
    return Quaternion( s_-b.s_, vec.x, vec.y, vec.z );
}


Quaternion& Quaternion::operator-=( const Quaternion& b )
{
    (*this) = (*this) - b;
    return *this;
}


Quaternion Quaternion::operator*( const Quaternion& b ) const
{
    const Vector3 vec = s_*b.vec_ + b.s_*vec_ + vec_.cross(b.vec_);
    return Quaternion( s_*b.s_-vec_.dot(b.vec_), vec.x, vec.y, vec.z );
}


Quaternion& Quaternion::operator*=( const Quaternion& b )
{
    (*this) = (*this) * b;
    return *this;
}


Quaternion Quaternion::inverse() const
{
    return Quaternion( s_, -vec_.x, -vec_.y, -vec_.z );
}

Line3::Line3() {}

Line3::Line3( double x0, double y0, double z0, double alpha, double beta,
	      double gamma )
    : x0_( x0 )
    , y0_( y0 )
    , z0_( z0 )
    , alpha_( alpha )
    , beta_( beta )
    , gamma_( gamma )
{}


Line3::Line3( const Coord3& point, const Vector3& vector )
    : x0_( point.x )
    , y0_( point.y )
    , z0_( point.z )
    , alpha_( vector.x )
    , beta_(vector.y )
    , gamma_( vector.z )
{}


double Line3::distanceToPoint( const Coord3& point ) const
{
   const Vector3 p0p1( point.x - x0_, point.y - y0_, point.z - z0_ );
   const Vector3 v( alpha_, beta_, gamma_ );

   return v.cross( p0p1 ).abs() / v.abs();
}


/*
   |
   B-------C
   |      /
   |     /
   |    /
   |   /
   |  /
   |a/
   |/
   A
   |
   |

Given: A, C and dir
Wanted: B

B = dir/|dir| * |AB|
|AB| = |AC| * cos(a)
dir.AC = |dir|*|AC|*cos(a)

dir.AC / |dir| = |AC|*cos(a)

B = dir/|dir| * dir.AC / |dir|

*/


bool Line3::closestPoint( const Line3& line, double& t ) const
{
    const Coord3 dir0 = direction().normalize();
    const Coord3 dir1 = line.direction().normalize();
    const double cosalpha =  dir0.dot( dir1 );
    if ( mIsEqual(cosalpha,1,mDefEps) )
	return false;

    Coord3 diff = line.getPoint(0)-Coord3(x0_,y0_,z0_);
    double deter = dir1.x*dir0.y-dir1.y*dir0.x;
    double u = (diff.x*dir0.y-diff.y*dir0.x)/deter;
    t = (dir1.x*diff.y-dir1.y*diff.x)/deter;
    
    if ( mIsEqual(t*dir0.z-u*dir1.z, diff.z, mDefEps) )
	return true;

    const Coord3 crs = dir0.cross( dir1 );
    Coord3 v2 = -diff.cross(crs);
    Coord3 v0 = dir0.cross(crs);
    Coord3 v1 = dir1.cross(crs);
    
    t = (v2.x*v1.y-v1.x*v2.y)/(v1.x*v0.y-v0.x*v1.y);
    return true;
}


bool Line3::intersectWith( const Plane3& b, double& t ) const
{
    const double denominator = ( alpha_*b.A_ + beta_*b.B_ + gamma_*b.C_ );
    if ( mIsZero(denominator,mDefEps) )
	return false;

    t = -( b.A_*x0_ + b.B_*y0_ + b.C_*z0_ + b.D_ ) / denominator;

    return true;
}


Coord3 Line3::getPoint( double t ) const
{
    return Coord3( x0_+alpha_*t, y0_+beta_*t, z0_+gamma_*t ); 
}


Plane3::Plane3() {}

							     
Plane3::Plane3( double A, double B, double C, double D )
    : A_( A )
    , B_( B )
    , C_( C )
    , D_( D )
{}


Plane3::Plane3( const Coord3& vec, const Coord3& point, bool istwovec )
{
    set( vec, point, istwovec );
}


Plane3::Plane3( const Coord3& a, const Coord3& b, const Coord3& c )
{
    set( a, b, c );
}


Plane3::Plane3( const TypeSet<Coord3>& pts )
{
    set( pts );
}


void Plane3::set( const Vector3& norm, const Coord3& point, bool istwovec )
{
    if ( istwovec )
    {
	Vector3 cross = point.cross( norm );
	A_ = cross.x;
	B_ = cross.y;
	C_ = cross.z;
	D_ = 0;
    }
    else
    {
	A_ = norm.x;
	B_ = norm.y;
	C_ = norm.z;
	D_ =  -(norm.x*point.x) - (norm.y*point.y) - ( norm.z*point.z );
    }
}


void Plane3::set( const Coord3& a, const Coord3& b, const Coord3& c )
{
    Vector3 ab( b.x -a.x, b.y -a.y, b.z -a.z );
    Vector3 ac( c.x -a.x, c.y -a.y, c.z -a.z );

    Vector3 n = ab.cross( ac );

    A_ = n.x;
    B_ = n.y;
    C_ = n.z;
    D_ = A_*(-a.x)  - B_*a.y - C_*a.z;
}


float Plane3::set( const TypeSet<Coord3>& pts )
{
    const int nrpts = pts.size();
    if ( nrpts<3 )
    {
	A_ = 0; B_=0; C_=0; D_=0;
	return 0;
    }
    if ( nrpts==3 )
    {
	set( pts[0], pts[1], pts[2] );
	return 1;
    }

    PCA pca(3);
    Coord3 midpt( 0, 0, 0 );
    for ( int idx=0; idx<nrpts; idx++ )
    {
	pca.addSample( pts[idx] );
	midpt += pts[idx];
    }

    midpt.x /= nrpts;
    midpt.y /= nrpts;
    midpt.z /= nrpts;

    if ( !pca.calculate() )
	return -1;

    Vector3 norm;
    pca.getEigenVector(2,norm);
    set( norm, midpt, false );
    return 1-pca.getEigenValue(2)/
	   (pca.getEigenValue(0)+pca.getEigenValue(0)+pca.getEigenValue(0));
}


bool Plane3::operator==(const Plane3& b ) const
{
    Vector3 a_vec = normal();
    Vector3 b_vec = normal();

    const double a_len = a_vec.abs();
    const double b_len = b_vec.abs();

    const bool a_iszero = mIsZero(a_len,mDefEps);
    const bool b_iszero = mIsZero(b_len,mDefEps);

    if ( a_iszero||b_iszero) 
    {
	if ( a_iszero&&b_iszero ) return true;
	pErrMsg("Zero-length Vector");
	return false;
    }

    const double cross = 1-a_vec.dot(b_vec);

    if ( !mIsZero(cross,mDefEps) ) return false;

    const double ddiff = D_/a_len - b.D_/b_len;

    return mIsZero(ddiff,mDefEps);
}


bool Plane3::operator!=(const Plane3& b ) const
{
    return !((*this)==b);
}




double Plane3::distanceToPoint( const Coord3& point, bool whichside ) const
{
    Vector3 norm( normal().normalize() );
    const Line3 linetoplane( point, norm );

    Coord3 p0;
    if ( intersectWith( linetoplane, p0 ) ) 
    {
	const Coord3 diff = point-p0;
	return whichside ? diff.dot(norm) : diff.abs();
    }
    else 
        return 0;	
}


bool Plane3::intersectWith( const Line3& b, Coord3& res ) const
{
    double t;
    if ( !b.intersectWith( *this, t ) )
	return false;

    res = b.getPoint( t );

    return true;
}


bool Plane3::intersectWith( const Plane3& b, Line3& res ) const
{
    const double detbc = B_*b.C_-b.C_*B_;
    const double detca = C_*b.A_-b.C_*A_;
    const double detab = A_*b.B_-b.A_*B_;

    if ( mIsZero(detbc,mDefEps) && mIsZero(detca,mDefEps)
	    && mIsZero(detab,mDefEps) ) return false; 

    res.alpha_ = detbc; res.beta_=detca; res.gamma_=detab;

    const double denumerator = detbc*detbc+detca*detca+detab*detab;

    const double detdc = D_*b.C_-b.D_*C_;
    const double detdb = D_*b.B_-b.D_*B_;
    const double detda = D_*b.A_-b.D_*A_;

    res.x0_ = (res.beta_*detdc-res.gamma_*detdb)/denumerator;
    res.y0_ = (res.gamma_*detda-res.alpha_*detdc)/denumerator;
    res.z0_ = (res.alpha_*detdb-res.beta_*detda)/denumerator;

    return true;
}


Plane3CoordSystem::Plane3CoordSystem(const Coord3& normal, const Coord3& origin,
				     const Coord3& pt10)
    : origin_( origin )
    , plane_( normal, origin, false )
{
    vec10_ = pt10-origin;
    const double vec10len = vec10_.abs();
    isok_ = !mIsZero(vec10len, 1e-5 );
    if ( isok_ )
	vec10_ /= vec10len;

    vec01_ = normal.cross( vec10_ ).normalize();
}


bool Plane3CoordSystem::isOK() const { return isok_; }


Coord Plane3CoordSystem::transform( const Coord3& pt, bool project ) const
{
    if ( !isok_ )
	return Coord3::udf();

    Coord3 v0;
    if ( project )
    {
	const Line3 line( pt, plane_.normal() );
	plane_.intersectWith( line, v0 );
	v0 -= origin_;
    }
    else
	v0 = pt-origin_;
	
    const double len = v0.abs();
    if ( !len )
	return Coord( 0, 0 );
    const Coord3 dir = v0 / len;

    const double cosphi = vec10_.dot( dir );
    const double sinphi = vec01_.dot( dir );

    return Coord( cosphi * len, sinphi * len );
}


Coord3 Plane3CoordSystem::transform( const Coord& coord ) const
{
    const double len = coord.abs();
    return vec01_*coord.x + vec10_*coord.y;
}

	
Sphere cartesian2Spherical( const Coord3& crd, bool math )
{
    float theta, phi;
    float rad = crd.abs();
    if ( math )
    {
	theta = rad ? Math::ACos( crd.z / rad ) : 0;
	phi = atan2( crd.y, crd.x );
    }
    else
    {
	theta = rad ? Math::ASin( crd.z / rad ) : 0;
	phi = atan2( crd.x, crd.y );
    }

    return Sphere(rad,theta,phi);
}


Coord3 spherical2Cartesian( const Sphere& sph, bool math )
{
    float x, y, z;
    if ( math )
    {
	x = sph.radius * cos(sph.phi) * sin(sph.theta);
	y = sph.radius * sin(sph.phi) * sin(sph.theta);
	z = sph.radius * cos(sph.theta);
    }
    else
    {
	x = sph.radius * sin(sph.phi) * cos(sph.theta);
	y = sph.radius * cos(sph.phi) * cos(sph.theta);
	z = sph.radius * sin(sph.theta);
    }

    return Coord3(x,y,z);
}
