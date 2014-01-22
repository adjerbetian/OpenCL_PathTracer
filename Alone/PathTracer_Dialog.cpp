
#include "PathTracer_Dialog.h"

#ifdef MAYA
#include <maya/MRenderView.h>
#endif

#include "../Controleur/PathTracer.h"
#include "PathTracer_bitmap.h"
#include <sstream>

namespace PathTracerNS
{

	const char* PathTracerDialog::exportFolderPath = PATHTRACER_SCENE_FOLDER"ExportedImages\\";


	PathTracerDialog::PathTracerDialog()
		:pathTracerWidth(0),
		pathTracerHeight(0),
		imageIndex(1)
	{}

	bool PathTracerDialog::PaintWindow(RGBAColor const * imageColor, float const * imageRay)
	{

#ifdef MAYA

		if(true)
		{
			/////////////////////////////////////////////////////////////////////////////
			/// Print in Maya:
			/////////////////////////////////////////////////////////////////////////////

			MStatus stat = MS::kSuccess;

			// Check if the render view exists. It should always exist, unless
			// Maya is running in batch mode.
			//
			if (!MRenderView::doesRenderEditorExist())
			{
				CONSOLE_LOG << "Cannot renderViewInteractiveRender in batch render mode." << ENDL;
				CONSOLE_LOG << "Run in interactive mode, so that the render editor exists." << ENDL;
				return false;
			};

			bool doNotClearBackground = true;

			if (MRenderView::startRender( pathTracerWidth, pathTracerHeight, doNotClearBackground, true) != MS::kSuccess)
			{
				CONSOLE_LOG << "renderViewInteractiveRender: error occurred in startRender." << ENDL;
				return false;
			}

			RV_PIXEL* pixels = new RV_PIXEL[pathTracerWidth * pathTracerHeight];
			// Fill buffer with uniform color
			for (unsigned int index = 0; index < pathTracerWidth * pathTracerHeight; ++index )
			{
				float c = 255.0f / imageRay[index];
				pixels[index].r = imageColor[index].x * c;
				pixels[index].g = imageColor[index].y * c;
				pixels[index].b = imageColor[index].z * c;
				pixels[index].a = imageColor[index].w * c;
			}

			// Pushing buffer to Render View
			if (MRenderView::updatePixels(0, pathTracerWidth-1, 0, pathTracerHeight-1, pixels) 
				!= MS::kSuccess)
			{
				CONSOLE_LOG << "renderViewInteractiveRender: error occurred in updatePixels." << ENDL;
				delete[] pixels;
				return false;
			}
			delete[] pixels;

			// Inform the Render View that we have completed rendering the entire image.
			//
			if (MRenderView::endRender() != MS::kSuccess)
			{
				CONSOLE_LOG << "renderViewInteractiveRender: error occurred in endRender.";
				return false;
			}
		}
		else
		{
			static float* imageRay2 = NULL;
			if(imageRay2 ==NULL)
			{
				imageRay2 = new float[pathTracerWidth * pathTracerHeight];
				memset(imageRay2, 0, pathTracerWidth * pathTracerHeight * sizeof(float));
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
				CONSOLE_LOG << "Cannot renderViewInteractiveRender in batch render mode." << ENDL;
				CONSOLE_LOG << "Run in interactive mode, so that the render editor exists." << ENDL;
				return false;
			};

			bool doNotClearBackground = true;

			if (MRenderView::startRender( pathTracerWidth, pathTracerHeight, doNotClearBackground, true) != MS::kSuccess)
			{
				CONSOLE_LOG << "renderViewInteractiveRender: error occurred in startRender." << ENDL;
				return false;
			}
			
			float nbRayMin = imageRay[0];
			float nbRayMax = nbRayMin;
			for(int i=0; i<pathTracerWidth * pathTracerHeight; i++)
			{
				nbRayMax = max(nbRayMax, imageRay[i]);
				nbRayMin = min(nbRayMin, imageRay[i]);
			}

			RV_PIXEL* pixels = new RV_PIXEL[pathTracerWidth * pathTracerHeight];
			// Fill buffer with uniform color
			for (unsigned int index = 0; index < pathTracerWidth * pathTracerHeight; ++index )
			{
				//float c = 255.f*(imageRay[index]-nbRayMin) / (nbRayMax-nbRayMin);
				float c = 255.f*(imageRay[index] - imageRay2[index]);
				pixels[index].r = c;
				pixels[index].g = c;
				pixels[index].b = c;
				pixels[index].a = 1;
			}

			memcpy(imageRay2, imageRay, pathTracerWidth * pathTracerHeight * sizeof(float));

			// Pushing buffer to Render View
			if (MRenderView::updatePixels(0, pathTracerWidth-1, 0, pathTracerHeight-1, pixels) 
				!= MS::kSuccess)
			{
				CONSOLE_LOG << "renderViewInteractiveRender: error occurred in updatePixels." << ENDL;
				delete[] pixels;
				return false;
			}
			delete[] pixels;

			// Inform the Render View that we have completed rendering the entire image.
			//
			if (MRenderView::endRender() != MS::kSuccess)
			{
				CONSOLE_LOG << "renderViewInteractiveRender: error occurred in endRender.";
				return false;
			}
		}

#endif // END MAYA


		if(saveRenderedImages || imageIndex==500)
		{

			std::wostringstream oss;
			oss << exportFolderPath;
			if(imageIndex / 100 == 0)
			{
				oss << "0";
				if(imageIndex / 10 == 0)
					oss << "0";
			}
			oss << imageIndex << ".bmp";

			std::wstring sFilePath = oss.str();
			LPCWSTR filePath = sFilePath.c_str();

			long newBufferSize = 0;
			BYTE* buffer = ConvertRGBAToBMPBuffer(imageColor, imageRay, pathTracerWidth, pathTracerHeight, &newBufferSize);
			if(!SaveBMP(buffer, pathTracerWidth, pathTracerHeight, newBufferSize, filePath))
			{
				CONSOLE_LOG << "WARNING : unable to save rendered picture in the folder \"" << exportFolderPath << "\". Aborting saving." << ENDL;
				saveRenderedImages = false;
			}
			delete[] buffer;
		}



		imageIndex++;
		return true;
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
