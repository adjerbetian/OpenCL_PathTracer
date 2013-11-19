
#include "PathTracer_Dialog.h"

#include "../Controleur/PathTracer.h"
#include "PathTracer_bitmap.h"
#include <sstream>

namespace PathTracerNS
{

	const char* PathTracerDialog::exportFolderPath = PATHTRACER_SCENE_FOLDER"ExportedImages\\";


	PathTracerDialog::PathTracerDialog()
		:pathTracerWidth(0),
		pathTracerHeight(0)
	{}

	void PathTracerDialog::PaintWindow(RGBAColor const * imageColor, uint const * imageRay)
	{
		static int numImage = 1;
		
		std::wostringstream oss;
		oss << exportFolderPath;
		if(numImage / 100 == 0)
		{
			oss << "0";
			if(numImage / 10 == 0)
				oss << "0";
		}
		oss << numImage << ".bmp";

		std::wstring sFilePath = oss.str();
		LPCWSTR filePath = sFilePath.c_str();

		long newBufferSize = 0;
		BYTE* buffer = ConvertRGBAToBMPBuffer(imageColor, imageRay, pathTracerWidth, pathTracerHeight, &newBufferSize);
		SaveBMP(buffer, pathTracerWidth, pathTracerHeight, newBufferSize, filePath);
		delete[] buffer;

		numImage++;

	}

	void PathTracerDialog::PaintTexture(Uchar4 const * global__texturesData, Texture& texture)
	{
		static int numTexture = 1;
		
		std::wostringstream oss;
		oss << exportFolderPath;
		oss << "Texture ";
		if(numTexture / 100 == 0)
		{
			oss << "0";
			if(numTexture / 10 == 0)
				oss << "0";
		}
		oss << numTexture << ".bmp";

		std::wstring sFilePath = oss.str();
		LPCWSTR filePath = sFilePath.c_str();

		RGBAColor* floatImage = (RGBAColor*) malloc(sizeof(RGBAColor)*texture.width*texture.height);
		for(uint i=0; i<texture.width*texture.height; i++)
		{
			floatImage[i] = global__texturesData[texture.offset+i].toDouble4()/255.0;
			floatImage[i].w /= 255.0;
		}

		long newBufferSize = 0;
		BYTE* buffer = ConvertRGBAToBMPBuffer(floatImage, NULL, texture.width, texture.height, &newBufferSize);
		SaveBMP(buffer, texture.width, texture.height, newBufferSize, filePath);

		delete[] buffer;
		free(floatImage);
		numTexture++;

	}


}
