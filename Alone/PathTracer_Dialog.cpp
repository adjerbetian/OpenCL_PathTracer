
#include "PathTracer_Dialog.h"

#include "../Controleur/PathTracer.h"
#include "PathTracer_bitmap.h"
#include <sstream>
#include <maya/MRenderView.h>

namespace PathTracerNS
{

	const char* PathTracerDialog::exportFolderPath = PATHTRACER_SCENE_FOLDER"ExportedImages\\";


	PathTracerDialog::PathTracerDialog()
		:pathTracerWidth(0),
		pathTracerHeight(0)
	{}

	void PathTracerDialog::PaintWindow(RGBAColor const * imageColor, uint const * imageRay)
	{
		if(saveRenderedImages)
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

		/////////////////////////////////////////////////////////////////////////////
		/// Print in Maya:
		/////////////////////////////////////////////////////////////////////////////

		MStatus stat = MS::kSuccess;

		// Check if the render view exists. It should always exist, unless
		// Maya is running in batch mode.
		//
		if (!MRenderView::doesRenderEditorExist())
		{
			CONSOLE << "Cannot renderViewInteractiveRender in batch render mode." << ENDL;
			CONSOLE << "Run in interactive mode, so that the render editor exists." << ENDL;
			return;
		};

		bool doNotClearBackground = true;

		if (MRenderView::startRender( pathTracerWidth, pathTracerHeight, doNotClearBackground, true) != MS::kSuccess)
		{
			CONSOLE << "renderViewInteractiveRender: error occurred in startRender." << ENDL;
			return;
		}

		RV_PIXEL* pixels = new RV_PIXEL[pathTracerWidth * pathTracerHeight];
		// Fill buffer with uniform color
		for (unsigned int index = 0; index < pathTracerWidth * pathTracerHeight; ++index )
		{
			pixels[index].r = imageColor[index].x * 255.0f / ( imageRay == NULL ? 1.f : imageRay[index] );
			pixels[index].g = imageColor[index].y * 255.0f / ( imageRay == NULL ? 1.f : imageRay[index] );
			pixels[index].b = imageColor[index].z * 255.0f / ( imageRay == NULL ? 1.f : imageRay[index] );
			pixels[index].a = imageColor[index].w * 255.0f / ( imageRay == NULL ? 1.f : imageRay[index] );
		}

		// Pushing buffer to Render View
		if (MRenderView::updatePixels(0, pathTracerWidth-1, 0, pathTracerHeight-1, pixels) 
			!= MS::kSuccess)
		{
			CONSOLE << "renderViewInteractiveRender: error occurred in updatePixels." << ENDL;
			delete[] pixels;
			return;
		}
		delete[] pixels;

		// Inform the Render View that we have completed rendering the entire image.
		//
		if (MRenderView::endRender() != MS::kSuccess)
		{
			CONSOLE << "renderViewInteractiveRender: error occurred in endRender.";
			return;
		}

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
			floatImage[i] = global__texturesData[texture.offset+i].toFloat4()/255.0;
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
