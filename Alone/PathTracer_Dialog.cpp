
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

	void PathTracerDialog::PaintWindow(RGBAColor const * const * const imageColor, uint const * const * imageRay)
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

		std::wstring wFilePath = oss.str();
		LPCWSTR filePath = wFilePath.c_str();

		long newBufferSize = 0;
		BYTE* buffer = ConvertRGBAToBMPBuffer(imageColor, imageRay, pathTracerWidth, pathTracerHeight, &newBufferSize);
		SaveBMP(buffer, pathTracerWidth, pathTracerHeight, newBufferSize, filePath);
		delete[] buffer;

		numImage++;

	}


}
