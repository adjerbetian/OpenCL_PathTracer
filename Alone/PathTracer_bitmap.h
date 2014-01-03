
#ifndef PATHTRACER_BITMAP
#define PATHTRACER_BITMAP

#include "../Controleur/PathTracer_Utils.h"
#include <WTypes.h>


namespace PathTracerNS
{
	bool LoadBMPIntoDC ( HDC hDC, LPCTSTR bmpfile );
	BYTE* LoadBMP ( int* width, int* height, long* size, char* filePath );
	BYTE* ConvertBMPToRGBBuffer ( BYTE* Buffer, int width, int height );
	bool SaveBMP ( BYTE* buffer, int width, int height, long paddedsize, LPCWSTR bmpfile );
	BYTE* ConvertRGBToBMPBuffer ( BYTE* Buffer, int width, int height, long* newsize );
	BYTE* ConvertRGBAToBMPBuffer ( RGBAColor const * imageColor, float const * imageRay, int width, int height, long* newsize );
}

#endif