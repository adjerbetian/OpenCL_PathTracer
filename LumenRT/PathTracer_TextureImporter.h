
#ifndef PATHTRACER_TEXTUREIMPORTER
#define PATHTRACER_TEXTUREIMPORTER

#include "dds/ATI_Compress.h"

#include "../Controleur/PathTracer_Structs.h"

namespace PathTracerNS
{

	bool Texture_HasAlpha	( Texture const *This, Uchar4 const *textureData );

	ATI_TC_FORMAT	Texture_GetFormat					(DWORD dwFourCC);
	ATI_TC_Texture	Texture_LoadDDSFromBuffer			(char *buffer);
	ATI_TC_Texture	Texture_LoadDDSFromCubeMapBuffer	(char *buffer, int faceId);
	void			Texture_Decompress					(ATI_TC_Texture& src, ATI_TC_Texture& dest);

}

#endif