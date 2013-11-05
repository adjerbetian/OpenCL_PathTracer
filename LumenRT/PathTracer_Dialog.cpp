
#include "Viewer/Headers_Core.h"
#include "Viewer/Headers_UI.h"

#include "eongui/eongui.h"
#include "PathTracer_Dialog.h"
#include "Dialogs/dialogs.rh"
#include "LabelCtrl.h"
#include "UserDisplayCtrl.h"

#include "../Controleur/PathTracer.h"

//Utliser comme exemple : ConstantSliderSetup.cpp

namespace PathTracerNS
{

	PathTracerDialog::PathTracerDialog()
		: EONDialog(NULL, IDD_Empty),
		pathTracerWidth(0),
		pathTracerHeight(0),
		displayControl(NULL)
	{
		//SetParent((EONWnd *) &Viewer::globalWindow.GetInternalWindow());
	}

	void PathTracerDialog::SetupWindow()
	{
		startTime = clock();

		EONDialog::SetupWindow();
		SetWindowText(_L("PathTracer render view"));

		int window_left,
			window_top,
			window_right,
			window_bottom;
		int display_left,
			display_top,
			display_right,
			display_bottom;
		int text_left,
			text_top,
			text_right,
			text_bottom;

		display_left	= 10;
		display_top		= 30;
		display_right	= display_left + pathTracerWidth + 3;
		display_bottom	= display_top + pathTracerHeight + 3;

		text_left 		= display_right + 10;
		text_top		= 30;
		text_right		= text_left + 300;
		text_bottom		= display_bottom;

		window_left		= 0;
		window_top		= 0;
		window_right	= text_right + 10;
		window_bottom	= text_bottom + 10;
		
		displayControl = new EONUserDisplayCtrl(this, GetNextFreeControlID(), EONRect(display_left, display_top, display_right, display_bottom), "PathTracer_DisplayCtrl");
		AddControl(displayControl);

		EONRect rect;
		EONString text;
		int leftForLabel = text_left + 20;
		int leftForValue = text_left + 200;
		int topMargin = 28;

		int left, top;

		left = text_left + 100; top = text_top;
		CreateEONLabelCtrl("INFORMATION", left, top);

		left = text_left; top += 48;
		CreateEONLabelCtrl("GENERAL", left, top);

		top += topMargin;
		generalElapsedTime = CreateEONLabelCtrl("Elapsed time : ", leftForLabel, top, leftForValue);

		left = text_left; top += 48;
		CreateEONLabelCtrl("SCENE", left, top);

		top += topMargin;
		sceneNumberOfTriangles = CreateEONLabelCtrl("Number of triangles : ", leftForLabel, top, leftForValue);

		top += topMargin;
		sceneSizeOfBVH = CreateEONLabelCtrl("Size of the BVH : ", leftForLabel, top, leftForValue);

		top += topMargin;
		sceneDepthOfBVH = CreateEONLabelCtrl("Maximum depth of the BVH : ", leftForLabel, top, leftForValue);

		left = text_left; top += 48;
		CreateEONLabelCtrl("PATHTRACER", left, top);

		top += topMargin;
		pathRaysPerPixel = CreateEONLabelCtrl("Number of rays per pixel : ", leftForLabel, top, leftForValue);

		top += topMargin;
		pathPixelsPerSecond = CreateEONLabelCtrl("Number of pixels per seconds : ", leftForLabel, top, leftForValue);

		top += topMargin;
		pathRaysPerSecond = CreateEONLabelCtrl("Number of rays per seconds : ", leftForLabel, top, leftForValue);

		SetWindowPos(0, window_left, window_top, window_right, window_bottom, SWP_NOMOVE | SWP_NOZORDER);
	}

	EONLabelCtrl* PathTracerDialog::CreateEONLabelCtrl(const EONString& labelText, int left, int top, int leftForValue)
	{
		int right = left + 175;
		int bottom = top + 18;
		EONString baseString = "PathTracer_";
		EONLabelCtrl* textControl;
		EONRect rect;

		rect = EONRect(left, top, right, bottom);
		textControl = new EONLabelCtrl(this, GetNextFreeControlID(), rect, baseString + "Label_" + labelText);
		textControl->SetText(labelText);
		AddControl(textControl);

		if(leftForValue != -1)
		{
			right = leftForValue + 100;
			rect = EONRect(leftForValue, top, right, bottom);
			textControl = new EONLabelCtrl(this, GetNextFreeControlID(), rect, baseString + "Value_" + labelText);
			textControl->SetText(EONString("UNKNOWN"));
			AddControl(textControl);
		}

		return textControl;
	}

	void PathTracerDialog::TestPaintWindow()
	{
		EONTrueColorDC *userDC = displayControl->GetUserDisplayDC();

		int w = userDC->GetWidth();
		int h = userDC->GetHeight();

		BYTE* p_point = userDC->GetBits();

		for(int y=0; y<h; y++)
		{
			for(int x=0; x<w; x++)
			{
				int r = (int) ((((float) x) / w) * 255);
				int g = (int) ((((float) y) / h) * 255);
				int b = 255;

				p_point[0] = b;
				p_point[1] = g;
				p_point[2] = r;

				p_point += BYTES_PER_PIXEL;
			}
		}
		displayControl->UserDisplayDCModified();
		Viewer::globalWindow.GetInternalApplication().getApplication()->PumpWaitingMessages();
	}


	void PathTracerDialog::PaintWindow(RGBAColor const * const * const imageColor, uint const * const * const imageRay)
	{
		EONTrueColorDC *userDC = displayControl->GetUserDisplayDC();

		int w = userDC->GetWidth();
		int h = userDC->GetHeight();

		BYTE* p_point = userDC->GetBits();

		for(int y=0; y<h; y++)
		{
			for(int x=0; x<w; x++)
			{
				uint nRays = max(imageRay[x][y], (uint) 1);
				RGBAColor rgb = min(imageColor[x][y] * 255.f / nRays, 255.f);

				RTASSERT(
					rgb.x >= 0 && rgb.x < 256 &&
					rgb.y >= 0 && rgb.y < 256 &&
					rgb.z >= 0 && rgb.z < 256);

				p_point[0] = rgb.z;
				p_point[1] = rgb.y;
				p_point[2] = rgb.x;

				p_point += BYTES_PER_PIXEL;
			}
		}

		displayControl->UserDisplayDCModified();
		Viewer::globalWindow.GetInternalApplication().getApplication()->PumpWaitingMessages();
	}

	void PathTracerDialog::PaintLine(int x, RGBAColor const * lineColor, uint const * lineRay)
	{
		tools::ConsoleOut console;

		EONTrueColorDC *userDC = displayControl->GetUserDisplayDC();

		int w = userDC->GetWidth();
		int h = userDC->GetHeight();

		BYTE* p_point = userDC->GetBits() + x * BYTES_PER_PIXEL;
		int bytesPerLine = BYTES_PER_PIXEL*w;

		for(int y=0; y<h; y++)
		{
			int nRays = lineRay[y];
			RGBAColor rgb = min(lineColor[y] * 255.f / nRays, 255.f);

			RTASSERT(
				rgb.x >= 0 && rgb.x < 256 &&
				rgb.y >= 0 && rgb.y < 256 &&
				rgb.z >= 0 && rgb.z < 256);

			p_point[0] = rgb.z;
			p_point[1] = rgb.y;
			p_point[2] = rgb.x;

			p_point += bytesPerLine;
		}

		displayControl->UserDisplayDCModified();
		Viewer::globalWindow.GetInternalApplication().getApplication()->PumpWaitingMessages();
	}

	void PathTracerDialog::OnOK()
	{
		EONDialog::OnOK();
	}

	void PathTracerDialog::OnClose()
	{
		EONDialog::OnClose();
	}


}
