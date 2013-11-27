
#ifndef PATHTRACER_BITMAP
#define PATHTRACER_BITMAP

#include <WTypes.h>
#include "../Controleur/PathTracer_Utils.h"


namespace PathTracerNS
{
	bool LoadBMPIntoDC ( HDC hDC, LPCTSTR bmpfile );
	BYTE* LoadBMP ( int* width, int* height, long* size, LPCTSTR bmpfile );
	BYTE* ConvertBMPToRGBBuffer ( BYTE* Buffer, int width, int height );
	bool SaveBMP ( BYTE* buffer, int width, int height, long paddedsize, LPCWSTR bmpfile );
	BYTE* ConvertRGBToBMPBuffer ( BYTE* Buffer, int width, int height, long* newsize );
	BYTE* ConvertRGBAToBMPBuffer ( RGBAColor const * imageColor, uint const * imageRay, int width, int height, long* newsize );
}

#endif