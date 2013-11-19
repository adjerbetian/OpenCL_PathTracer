#ifndef PATHTRACER_UTILS
#define PATHTRACER_UTILS

#include <maya/MPoint.h>

#include "PathTracer_PreProc.h"
#include <algorithm>
#include <cmath>

namespace PathTracerNS
{

	typedef unsigned int uint;
	typedef unsigned char uchar;
	typedef unsigned long long int ulong;


	class Int2
	{
	public:
		inline Int2 ()									{ x = 0; y = 0;};
		inline Int2 (int _x, int _y)					{ x = _x; y = _y;};

		int x, y;

	};

	class Int4
	{
	public:
		inline Int4 ()									{ x = 0; y = 0;};
		inline Int4 (int _x, int _y, int _z, int _w)	{ x = _x; y = _y; y = _y; w = _w;};

		int x, y, z, w;

	};


	class Char4
	{
	public:
		inline Char4 ()										{ x = 0; y = 0;};
		inline Char4 (char _x, char _y, char _z, char _w)	{ x = _x; y = _y; y = _y; w = _w;};

		char x, y, z, w;

	};

	class Uchar4
	{
	public:
		inline Uchar4 ()										{ x =  0; y =  0; z =  0; w =  0;};
		inline Uchar4 (uchar _x, uchar _y, uchar _z, uchar _w)	{ x = _x; y = _y; z = _y; w = _w;};
		inline MPoint toDouble4() const							{ return MPoint(x,y,z,w); };
		uchar x,y,z,w;
	};



	class Float2
	{
	public:

		inline Float2 ()									{ x = 0; y = 0;};
		inline Float2 (const Float2& v)						{ x = v.x; y = v.y;};
		inline Float2 (float _x, float _y)					{ x = _x; y = _y;};

		inline Float2 operator + (const Float2& v) const	{ return Float2(x + v.x, y + v.y);};
		inline Float2& operator += (const Float2& v)		{ x += v.x; y += v.y; return *this;};

		inline Float2 operator - (const Float2& v) const	{ return Float2(x - v.x, y - v.y);};
		inline Float2& operator -= (const Float2& v)		{ x -= v.x; y -= v.y; return *this;};

		inline Float2 operator * (float a) const			{ return Float2(x * a, y * a);};
		inline Float2& operator *= (float a)				{ x *= a; y *= a; return *this;};
		inline Float2 operator - () const					{ return Float2(-x, -y);};

		inline Float2 operator / (float a) const			{ return Float2(x / a, y / a);};
		inline Float2& operator /= (float a)				{ x /= a; y /= a; return *this;};

		inline void operator = (const Float2& v)			{ x = v.x; y = v.y;};
		inline bool operator == (const Float2& v) const		{ return ( x == v.x && y == v.y);};
		inline bool operator != (const Float2& v) const		{ return ( x != v.x || y != v.y);};

		float x, y;

	};

	typedef MPoint Double4;
	typedef MPoint RGBAColor;

	inline Double4	permute_xyz_to_zxy	(const MPoint &m)						{return Double4(m.z, m.x, m.y, m.w);};
	inline double	dot				(const Float2& v1, const Float2& v2)		{return (v1.x * v2.x) + (v1.y * v2.y);};
	inline double	dot				(const Double4& v1, const Double4& v2)		{return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z) + (v1.w * v2.w);};
	inline Double4	cross			(const Double4& v1, const Double4& v2)		{return Double4( (v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z), (v1.x * v2.y) - (v1.y * v2.x), 0 );};
	inline Double4	ppmin			(const Double4& v1, const Double4& v2)		{return Double4( min(v1.x, v2.x), min(v1.y, v2.y), min(v1.z, v2.z), min(v1.w, v2.w));};
	inline Double4	ppmax			(const Double4& v1, const Double4& v2)		{return Double4( max(v1.x, v2.x), max(v1.y, v2.y), max(v1.z, v2.z), max(v1.w, v2.w));};
	inline Double4	pmin			(const Double4& v1, float a)					{return ppmin(v1, Double4(a,a,a,a));};
	inline Double4	pmax			(const Double4& v1, float a)					{return ppmax(v1, Double4(a,a,a,a));};
	inline Double4	exp				(const Double4& v)							{return Double4( std::exp(v.x), std::exp(v.y), std::exp(v.z), std::exp(v.w));};

	inline float	exp				(float a)									{return std::exp(a);};
	inline float	log				(float a)									{return std::log(a);};
	inline float	sqrt			(float a)									{return std::sqrt(a);};

	inline double	length			(const Double4& v)							{ return std::sqrt(dot(v,v));};
	inline double	distance		(const Double4& v1, const Double4& v2)		{ return length(v2 - v1);};
	inline Double4	normalize		(const Double4& v)							{ return v / length(v);};


	//	Vector

	inline double Vector_SquaredNorm			(Double4 const	*This)					{ return dot(*This, *This);};
	inline double Vector_SquaredDistanceTo	(Double4 const	*This, Double4 const *v)	{ Double4 temp = (*v) - ((*This)); return Vector_SquaredNorm(&temp);};
	inline bool	 Vector_LexLessThan			(Double4 const	*This, Double4 const *v)	{ return ( (*This).x < (*v).x ) || ( ((*This).x == (*v).x) && ((*This).y < (*v).y) ) || ( ((*This).x == (*v).x) && ((*This).y == (*v).y) && ((*This).z < (*v).z) );};
	inline double Vector_Max					(Double4 const	*This)					{ return max((*This).x, max((*This).y, (*This).z));};
	inline double Vector_Mean				(Double4 const	*This)					{ return ( (*This).x + (*This).y + (*This).z ) / 3;};

	inline void Vector_PutInSameHemisphereAs( Double4 *This, Double4 const *N)
	{
		double dotProd = dot(*This, *N);
		if( dotProd < 0.001f )
			(*This) += (*N) * (0.01f - dotProd);

		RTASSERT( dot(*This, *N) > 0.0f);
	};



	// RGBAColor

	inline void RGBAColor_SetColor	(RGBAColor *This, int r, int g, int b, int a)	{ *This = RGBAColor(r,g,b,a) / 255.f;};
	inline void RGBAColor_SetToWhite(RGBAColor *This)								{ *This = RGBAColor(1.f,1.f,1.f,1.f);};
	inline void RGBAColor_SetToBlack(RGBAColor *This)								{ *This = RGBAColor(0.f,0.f,0.f,0.f);};
	inline bool RGBAColor_IsTransparent(RGBAColor *This)							{ return (*This).w > 0.5f;};

	//n = coefficient de normalisation pour le cas ou plusieurs rayons ont contribué
	inline int RGBAColor_GetR(RGBAColor const *This, int n)							{ if(n==0) return 0; double c = (*This).x/n; if(c>1) return 255; return (int) (255*c); };
	inline int RGBAColor_GetG(RGBAColor const *This, int n)							{ if(n==0) return 0; double c = (*This).y/n; if(c>1) return 255; return (int) (255*c); };
	inline int RGBAColor_GetB(RGBAColor const *This, int n)							{ if(n==0) return 0; double c = (*This).z/n; if(c>1) return 255; return (int) (255*c); };

}

#endif