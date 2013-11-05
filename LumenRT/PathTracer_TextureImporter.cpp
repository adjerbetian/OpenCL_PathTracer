#include "Viewer/PCH.h"

#include "toolsLog.h"

#include "PathTracer_TextureImporter.h"

#include <stdio.h>
#include <tchar.h>
#include <assert.h>
#include "ddraw.h"
#include "d3d9types.h"
#include "dds/ATI_Compress.h"

#define FOURCC_ATI1N		        MAKEFOURCC('A', 'T', 'I', '1')
#define FOURCC_ATI2N		        MAKEFOURCC('A', 'T', 'I', '2')
#define FOURCC_ATI2N_XY		        MAKEFOURCC('A', '2', 'X', 'Y')
#define FOURCC_ATI2N_DXT5	        MAKEFOURCC('A', '2', 'D', '5')
#define FOURCC_DXT5_xGBR	        MAKEFOURCC('x', 'G', 'B', 'R')
#define FOURCC_DXT5_RxBG	        MAKEFOURCC('R', 'x', 'B', 'G')
#define FOURCC_DXT5_RBxG	        MAKEFOURCC('R', 'B', 'x', 'G')
#define FOURCC_DXT5_xRBG	        MAKEFOURCC('x', 'R', 'B', 'G')
#define FOURCC_DXT5_RGxB	        MAKEFOURCC('R', 'G', 'x', 'B')
#define FOURCC_DXT5_xGxR	        MAKEFOURCC('x', 'G', 'x', 'R')
#define FOURCC_APC1			        MAKEFOURCC('A', 'P', 'C', '1')
#define FOURCC_APC2			        MAKEFOURCC('A', 'P', 'C', '2')
#define FOURCC_APC3			        MAKEFOURCC('A', 'P', 'C', '3')
#define FOURCC_APC4			        MAKEFOURCC('A', 'P', 'C', '4')
#define FOURCC_APC5			        MAKEFOURCC('A', 'P', 'C', '5')
#define FOURCC_APC6			        MAKEFOURCC('A', 'P', 'C', '6')
#define FOURCC_ATC_RGB			    MAKEFOURCC('A', 'T', 'C', ' ')
#define FOURCC_ATC_RGBA_EXPLICIT    MAKEFOURCC('A', 'T', 'C', 'A')
#define FOURCC_ATC_RGBA_INTERP	    MAKEFOURCC('A', 'T', 'C', 'I')
#define FOURCC_ETC_RGB			    MAKEFOURCC('E', 'T', 'C', ' ')
#define FOURCC_BC1                  MAKEFOURCC('B', 'C', '1', ' ')
#define FOURCC_BC2                  MAKEFOURCC('B', 'C', '2', ' ')
#define FOURCC_BC3                  MAKEFOURCC('B', 'C', '3', ' ')
#define FOURCC_BC4                  MAKEFOURCC('B', 'C', '4', ' ')
#define FOURCC_BC4S                 MAKEFOURCC('B', 'C', '4', 'S')
#define FOURCC_BC4U                 MAKEFOURCC('B', 'C', '4', 'U')
#define FOURCC_BC5                  MAKEFOURCC('B', 'C', '5', ' ')
#define FOURCC_BC5S                 MAKEFOURCC('B', 'C', '5', 'S')
// Deprecated but still supported for decompression
#define FOURCC_DXT5_GXRB            MAKEFOURCC('G', 'X', 'R', 'B')
#define FOURCC_DXT5_GRXB            MAKEFOURCC('G', 'R', 'X', 'B')
#define FOURCC_DXT5_RXGB            MAKEFOURCC('R', 'X', 'G', 'B')
#define FOURCC_DXT5_BRGX            MAKEFOURCC('B', 'R', 'G', 'X')

namespace PathTracerNS
{
	typedef struct
	{
		DWORD dwFourCC;
		ATI_TC_FORMAT nFormat;
	} ATI_TC_FourCC;

	ATI_TC_FourCC g_FourCCs[] =
	{
		{FOURCC_DXT1,               ATI_TC_FORMAT_DXT1},
		{FOURCC_DXT3,               ATI_TC_FORMAT_DXT3},
		{FOURCC_DXT5,               ATI_TC_FORMAT_DXT5},
		{FOURCC_DXT5_xGBR,          ATI_TC_FORMAT_DXT5_xGBR},
		{FOURCC_DXT5_RxBG,          ATI_TC_FORMAT_DXT5_RxBG},
		{FOURCC_DXT5_RBxG,          ATI_TC_FORMAT_DXT5_RBxG},
		{FOURCC_DXT5_xRBG,          ATI_TC_FORMAT_DXT5_xRBG},
		{FOURCC_DXT5_RGxB,          ATI_TC_FORMAT_DXT5_RGxB},
		{FOURCC_DXT5_xGxR,          ATI_TC_FORMAT_DXT5_xGxR},
		{FOURCC_DXT5_GXRB,          ATI_TC_FORMAT_DXT5_xRBG},
		{FOURCC_DXT5_GRXB,          ATI_TC_FORMAT_DXT5_RxBG},
		{FOURCC_DXT5_RXGB,          ATI_TC_FORMAT_DXT5_xGBR},
		{FOURCC_DXT5_BRGX,          ATI_TC_FORMAT_DXT5_RGxB},
		{FOURCC_ATI1N,              ATI_TC_FORMAT_ATI1N},
		{FOURCC_ATI2N,              ATI_TC_FORMAT_ATI2N},
		{FOURCC_ATI2N_XY,           ATI_TC_FORMAT_ATI2N_XY},
		{FOURCC_ATI2N_DXT5,         ATI_TC_FORMAT_ATI2N_DXT5},
		{FOURCC_BC1,                ATI_TC_FORMAT_BC1},
		{FOURCC_BC2,                ATI_TC_FORMAT_BC2},
		{FOURCC_BC3,                ATI_TC_FORMAT_BC3},
		{FOURCC_BC4,                ATI_TC_FORMAT_BC4},
		{FOURCC_BC4S,               ATI_TC_FORMAT_BC4},
		{FOURCC_BC4U,               ATI_TC_FORMAT_BC4},
		{FOURCC_BC5,                ATI_TC_FORMAT_BC5},
		{FOURCC_BC5S,               ATI_TC_FORMAT_BC5},
		{FOURCC_ATC_RGB,            ATI_TC_FORMAT_ATC_RGB},
		{FOURCC_ATC_RGBA_EXPLICIT,  ATI_TC_FORMAT_ATC_RGBA_Explicit},
		{FOURCC_ATC_RGBA_INTERP,    ATI_TC_FORMAT_ATC_RGBA_Interpolated},
		{FOURCC_ETC_RGB,            ATI_TC_FORMAT_ETC_RGB},
	};

	DWORD g_dwFourCCCount = sizeof(g_FourCCs) / sizeof(g_FourCCs[0]);

	ATI_TC_FORMAT Texture_GetFormat(DWORD dwFourCC)
	{
		for(DWORD i = 0; i < g_dwFourCCCount; i++)
			if(g_FourCCs[i].dwFourCC == dwFourCC)
				return g_FourCCs[i].nFormat;

		return ATI_TC_FORMAT_Unknown;
	}

#ifdef _WIN64
#	define POINTER_64 __ptr64

#pragma pack(4)

	typedef struct _DDSURFACEDESC2_64
	{
		DWORD               dwSize;                 // size of the DDSURFACEDESC structure
		DWORD               dwFlags;                // determines what fields are valid
		DWORD               dwHeight;               // height of surface to be created
		DWORD               dwWidth;                // width of input surface
		union
		{
			LONG            lPitch;                 // distance to start of next line (return value only)
			DWORD           dwLinearSize;           // Formless late-allocated optimized surface size
		} DUMMYUNIONNAMEN(1);
		union
		{
			DWORD           dwBackBufferCount;      // number of back buffers requested
			DWORD           dwDepth;                // the depth if this is a volume texture
		} DUMMYUNIONNAMEN(5);
		union
		{
			DWORD           dwMipMapCount;          // number of mip-map levels requestde
			// dwZBufferBitDepth removed, use ddpfPixelFormat one instead
			DWORD           dwRefreshRate;          // refresh rate (used when display mode is described)
			DWORD           dwSrcVBHandle;          // The source used in VB::Optimize
		} DUMMYUNIONNAMEN(2);
		DWORD               dwAlphaBitDepth;        // depth of a buffer requested
		DWORD               dwReserved;             // reserved
		void* __ptr32       lpSurface;              // pointer to the associated surface memory
		union
		{
			DDCOLORKEY      ddckCKDestOverlay;      // color key for destination overlay use
			DWORD           dwEmptyFaceColor;       // Physical color for empty cubemap faces
		} DUMMYUNIONNAMEN(3);
		DDCOLORKEY          ddckCKDestBlt;          // color key for destination blt use
		DDCOLORKEY          ddckCKSrcOverlay;       // color key for source overlay use
		DDCOLORKEY          ddckCKSrcBlt;           // color key for source blt use
		union
		{
			DDPIXELFORMAT   ddpfPixelFormat;        // pixel format description of the surface
			DWORD           dwFVF;                  // vertex format description of vertex buffers
		} DUMMYUNIONNAMEN(4);
		DDSCAPS2            ddsCaps;                // direct draw surface capabilities
		DWORD               dwTextureStage;         // stage in multitexture cascade
	} DDSURFACEDESC2_64;

#define DDSD2 DDSURFACEDESC2_64
#else
#define DDSD2 DDSURFACEDESC2
#endif

	static const DWORD DDS_HEADER = MAKEFOURCC('D', 'D', 'S', ' ');





	ATI_TC_Texture Texture_LoadDDSFromBuffer(char *buffer)
	{

		ATI_TC_Texture texture;
		tools::ConsoleOut console;

		DWORD dwBufferHeader;
		memcpy(&dwBufferHeader, buffer, sizeof(DWORD));
		buffer+=sizeof(DWORD);
		if(dwBufferHeader != DDS_HEADER)
		{
			console << "Source file is not a valid DDS." << tools::endl;
		}

		DDSD2 ddsd;
		memcpy(&ddsd, buffer, sizeof(DDSD2));
		buffer+=sizeof(DDSD2);

		memset(&texture, 0, sizeof(texture));
		texture.dwSize = sizeof(texture);
		texture.dwWidth = ddsd.dwWidth;
		texture.dwHeight = ddsd.dwHeight;
		texture.dwPitch = ddsd.lPitch;

		if(ddsd.ddpfPixelFormat.dwRGBBitCount==32)
			texture.format = ATI_TC_FORMAT_ARGB_8888;
		else if(ddsd.ddpfPixelFormat.dwRGBBitCount==24)
			texture.format = ATI_TC_FORMAT_RGB_888;
		else if(Texture_GetFormat(ddsd.ddpfPixelFormat.dwPrivateFormatBitCount) != ATI_TC_FORMAT_Unknown)
			texture.format = Texture_GetFormat(ddsd.ddpfPixelFormat.dwPrivateFormatBitCount);
		else if(Texture_GetFormat(ddsd.ddpfPixelFormat.dwFourCC) != ATI_TC_FORMAT_Unknown)
			texture.format = Texture_GetFormat(ddsd.ddpfPixelFormat.dwFourCC);
		else
		{
			console << "Unsupported source format." << tools::endl;
		}

		// Init source texture
		texture.dwDataSize = ATI_TC_CalculateBufferSize(&texture);
		texture.pData = (ATI_TC_BYTE*) malloc(texture.dwDataSize);

		memcpy(texture.pData, buffer, texture.dwDataSize);
		
		buffer+=texture.dwDataSize;

		return texture;
	}

	ATI_TC_Texture Texture_LoadDDSFromCubeMapBuffer(char *buffer, int faceId)
	{
		ATI_TC_Texture texture;
		tools::ConsoleOut console;

		DWORD dwBufferHeader;
		memcpy(&dwBufferHeader, buffer, sizeof(DWORD));
		buffer+=sizeof(DWORD);
		if(dwBufferHeader != DDS_HEADER)
		{
			console << "Source file is not a valid DDS." << tools::endl;
		}

		DDSD2 ddsd;
		memcpy(&ddsd, buffer, sizeof(DDSD2));
		buffer+=sizeof(DDSD2);

		memset(&texture, 0, sizeof(texture));
		texture.dwSize = sizeof(texture);
		texture.dwWidth = ddsd.dwWidth;
		texture.dwHeight = ddsd.dwHeight;
		texture.dwPitch = ddsd.lPitch;

		if(ddsd.ddpfPixelFormat.dwRGBBitCount==32)
			texture.format = ATI_TC_FORMAT_ARGB_8888;
		else if(ddsd.ddpfPixelFormat.dwRGBBitCount==24)
			texture.format = ATI_TC_FORMAT_RGB_888;
		else if(Texture_GetFormat(ddsd.ddpfPixelFormat.dwPrivateFormatBitCount) != ATI_TC_FORMAT_Unknown)
			texture.format = Texture_GetFormat(ddsd.ddpfPixelFormat.dwPrivateFormatBitCount);
		else if(Texture_GetFormat(ddsd.ddpfPixelFormat.dwFourCC) != ATI_TC_FORMAT_Unknown)
			texture.format = Texture_GetFormat(ddsd.ddpfPixelFormat.dwFourCC);
		else
		{
			console << "Unsupported source format." << tools::endl;
		}

		// Init source texture
		texture.dwDataSize = ATI_TC_CalculateBufferSize(&texture);
		texture.pData = (ATI_TC_BYTE*) malloc(texture.dwDataSize);

		buffer += (faceId-1) * texture.dwDataSize;

		memcpy(texture.pData, buffer, texture.dwDataSize);

		return texture;
	}


	void Texture_Decompress(ATI_TC_Texture& src, ATI_TC_Texture& dest)
	{
		dest.dwSize = sizeof(dest);
		dest.dwWidth = src.dwWidth;
		dest.dwHeight = src.dwHeight;
		dest.dwPitch = 0;
		dest.format = ATI_TC_FORMAT_ARGB_8888;

		dest.dwDataSize = ATI_TC_CalculateBufferSize(&dest);
		dest.pData = (ATI_TC_BYTE*) malloc(dest.dwDataSize);

		ATI_TC_CompressOptions options;
		memset(&options, 0, sizeof(options));
		options.dwSize = sizeof(options);
		options.nCompressionSpeed = ATI_TC_Speed_Normal;

		ATI_TC_ConvertTexture(&src, &dest, &options, NULL, NULL, NULL);
	}

	bool Texture_HasAlpha( Texture const *This, Uchar4 const *textureData )
	{
		int offset = This->offset;
		for(uint y = 0; y < This->height; y++)
		{
			for(uint x = 0; x < This->width; x++)
			{
				if(textureData[offset].w < 128)
					return true;
				offset++;
			}
		}
		return false;
	}

}