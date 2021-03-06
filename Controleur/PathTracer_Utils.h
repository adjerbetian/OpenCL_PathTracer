﻿#ifndef PATHTRACER_UTILS
#define PATHTRACER_UTILS

#include "PathTracer_PreProc.h"
#include <algorithm>
#include <cmath>
#include <sstream>



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


	class Float4
	{
	public:

		inline Float4 ()										{ x = 0; y = 0; z = 0; w = 0;};
		inline Float4 (const Float4& v)							{ x = v.x; y = v.y; z = v.z; w = v.w;};
		inline Float4 (float _x, float _y, float _z, float _w)	{ x = _x; y = _y; z = _z; w = _w;};

		inline Float4  operator + (const Float4& v) const		{ return Float4(x + v.x, y + v.y, z + v.z, w + v.w);};
		inline Float4& operator += (const Float4& v)			{ x += v.x; y += v.y; z += v.z; w += v.w; return *this;};

		inline Float4  operator - (const Float4& v) const		{ return Float4(x - v.x, y - v.y, z - v.z, w - v.w);};
		inline Float4& operator -= (const Float4& v)				{ x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this;};
		inline Float4  operator - () const						{ return Float4(-x, -y, -z, -w);};

		inline Float4  operator * (float a) const				{ return Float4(x * a, y * a, z * a, w * a);};
		inline Float4& operator *= (float a)					{ x *= a; y *= a; z *= a; w *= a; return *this;};

		inline Float4  operator * (const Float4& v) const		{ return Float4(x * v.x, y * v.y, z * v.z, w * v.w );};
		inline Float4& operator *= (const Float4& v)			{ x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this;};

		inline Float4  operator / (float a) const				{ return Float4(x / a, y / a, z / a, w / a);};
		inline Float4& operator /= (float a)					{ x /= a; y /= a; z /= a; w /= a; return *this;};

		inline Float4  operator / (const Float4& v) const		{ return Float4(x / v.x, y / v.y, z / v.z, w / v.w );};
		inline Float4& operator /= (const Float4& v)			{ x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this;};

		inline void operator = (const Float4& v)				{ x = v.x; y = v.y; z = v.z; w = v.w;};
		inline bool operator == (const Float4& v) const			{ return ( x == v.x && y == v.y && z == v.z && w == v.w);};
		inline bool operator != (const Float4& v) const			{ return ( x != v.x || y != v.y || z != v.z || w != v.w);};

		inline std::string toString() const						{ std::stringstream oss; oss << "[ " << x << " , " << y << " , " << z << " , " << w << " ]"; return oss.str();};

		float x, y, z, w;
	};

	typedef Float4 RGBAColor;

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
		inline Float4 toFloat4() const							{ return Float4(x,y,z,w); };
		uchar x,y,z,w;
	};



	inline float	dot				(const Float2& v1, const Float2& v2)		{return (v1.x * v2.x) + (v1.y * v2.y);};
	inline float	dot				(const Float4& v1, const Float4& v2)		{return (v1.x * v2.x) + (v1.y * v2.y) + (v1.z * v2.z);};
	inline Float4	cross			(const Float4& v1, const Float4& v2)		{return Float4( (v1.y * v2.z) - (v1.z * v2.y), (v1.z * v2.x) - (v1.x * v2.z), (v1.x * v2.y) - (v1.y * v2.x), 1 );};
	inline Float4	ppmin			(const Float4& v1, const Float4& v2)		{return Float4( std::min<float>(v1.x, v2.x), std::min<float>(v1.y, v2.y), std::min<float>(v1.z, v2.z), std::min<float>(v1.w, v2.w));};
	inline Float4	ppmax			(const Float4& v1, const Float4& v2)		{return Float4( std::max<float>(v1.x, v2.x), std::max<float>(v1.y, v2.y), std::max<float>(v1.z, v2.z), std::max<float>(v1.w, v2.w));};
	inline Float4	pmin			(const Float4& v1, float a)					{return ppmin(v1, Float4(a,a,a,a));};
	inline Float4	pmax			(const Float4& v1, float a)					{return ppmax(v1, Float4(a,a,a,a));};
	inline Float4	exp				(const Float4& v)							{return Float4( std::exp(v.x), std::exp(v.y), std::exp(v.z), std::exp(v.w));};

	//inline float	exp				(float a)									{return std::exp(a);};
	//inline float	log				(float a)									{return std::log(a);};
	//inline float	sqrt			(float a)									{return std::sqrt(a);};

	inline float	length			(const Float4& v)							{ return std::sqrt(dot(v,v));};
	inline float	distance		(const Float4& v1, const Float4& v2)		{ return length(v2 - v1);};
	inline Float4	normalize		(const Float4& v)							{ Float4 vn = v / length(v); vn.w = v.w; return vn;};


	//	Vector

	inline float Vector_SquaredNorm			(Float4 const &This)					{ return dot(This, This);};
	inline float Vector_SquaredDistanceTo	(Float4 const &This, Float4 const& v)	{ return Vector_SquaredNorm(v - (This));};
	inline bool	 Vector_LexLessThan			(Float4 const &This, Float4 const& v)	{ return ( This.x < v.x ) || ( (This.x == v.x) && (This.y < v.y) ) || ( (This.x == v.x) && (This.y == v.y) && (This.z < v.z) );};
	inline float Vector_Max					(Float4 const &This)					{ return std::max<float>(This.x, std::max<float>(This.y, This.z));};
	inline float Vector_Mean				(Float4 const &This)					{ return ( This.x + This.y + This.z ) / 3;};

	inline void Vector_PutInSameHemisphereAs( Float4 *This, Float4 const *N)
	{
		float dotProd = dot(*This, *N);
		if( dotProd < 0.001f )
			(*This) += (*N) * (0.01f - dotProd);

		ASSERT( dot(*This, *N) > 0.0f);
	};



	// RGBAColor

	inline void RGBAColor_SetColor	(RGBAColor *This, int r, int g, int b, int a)	{ *This = RGBAColor((float)r,(float)g,(float)b,(float)a) / 255.f;};
	inline void RGBAColor_SetToWhite(RGBAColor *This)								{ *This = RGBAColor(1.f,1.f,1.f,1.f);};
	inline void RGBAColor_SetToBlack(RGBAColor *This)								{ *This = RGBAColor(0.f,0.f,0.f,0.f);};
	inline bool RGBAColor_IsTransparent(RGBAColor *This)							{ return (*This).w > 0.5f;};

	//n = coefficient de normalisation pour le cas ou plusieurs rayons ont contribuי
	inline int RGBAColor_GetR(RGBAColor const *This, int n)							{ if(n==0) return 0; double c = (*This).x/n; if(c>1) return 255; return (int) (255*c); };
	inline int RGBAColor_GetG(RGBAColor const *This, int n)							{ if(n==0) return 0; double c = (*This).y/n; if(c>1) return 255; return (int) (255*c); };
	inline int RGBAColor_GetB(RGBAColor const *This, int n)							{ if(n==0) return 0; double c = (*This).z/n; if(c>1) return 255; return (int) (255*c); };

}

#endif