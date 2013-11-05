#ifndef PATHTRACER_PATHTRACERDIALOG
#define PATHTRACER_PATHTRACERDIALOG


#include "toolsLog.h"

#include "eongui/eongui.h"

//#include "eongui/dialog.h"
//#include "UserDisplayCtrl.h"

#include "../Controleur/PathTracer_Structs.h"

#include <vector>
#include <string>

/*

Utliser comme exemple : ConstantSiderSetup.cpp

idée :
hériter de EON Dialog
Dans les ressources, utiliser le fichier Empty.erc qui est une manière générque de créer un dialogue
Redéfinir la fonction SetupWindow pour qu'elle crée la fenêtre comme il faut
A la fin de la fonction, utiliser SetWindowPos pour redéfinir la position et la taille de la fenêtre

utiliser les fonction getUserDisplayControl pour récupérer le buffer d'écriture
utiliser UserDisplayDCModified pour informer l'OS qu'on a modifié la fenêtre


*/

namespace PathTracerNS
{
	class PathTracer;

	class PathTracerDialog : public EONDialog
	{

	public:

		PathTracerDialog();

		virtual void	SetupWindow();
		virtual void	OnOK();
		virtual void	OnClose();
		inline void		SetWidth	(uint w)		{ pathTracerWidth = w; };
		inline void		SetHeight	(uint h)		{ pathTracerHeight = h; };

		void			TestPaintWindow();
		void			PaintWindow(RGBAColor const * const * imageColor, uint const * const * imageRay);
		void			PaintLine(int x, RGBAColor const * lineColor, uint const * lineRay);

		inline void		SetNumberOfTriangles	(uint n)		{ EONString text; text.Format(_L("%u"), n); sceneNumberOfTriangles->SetText(text); };
		inline void		SetSizeOfBVH			(uint n)		{ EONString text; text.Format(_L("%u"), n); sceneSizeOfBVH->SetText(text); };
		inline void		SetMaxDepthOfBVH		(uint n)		{ EONString text; text.Format(_L("%u"), n); sceneDepthOfBVH->SetText(text); };
		inline void		SetRaysPerPixel			(uint n)		{ EONString text; text.Format(_L("%u"), n); pathRaysPerPixel->SetText(text); };
		inline void		SetPixelPerSeconds		(uint n)		{ EONString text; text.Format(_L("~ %u 000"), n); pathPixelsPerSecond->SetText(text); };
		inline void		SetRaysPerSeconds		(uint n)		{ EONString text; text.Format(_L("~ %u 000"), n); pathRaysPerSecond->SetText(text); };

		inline void		UpdateElapsedTime		()			{ EONString text; int elapsedTime = (int) ((clock() - startTime)/1000.0f); int seconds = elapsedTime%60; elapsedTime = elapsedTime / 60; int minutes = elapsedTime % 60; elapsedTime = elapsedTime / 60; int hours = elapsedTime; text.Format(_L("%dh %dm %ds"), hours, minutes, seconds); generalElapsedTime->SetText(text); };

		EONLabelCtrl*	CreateEONLabelCtrl(const EONString& labelText, int left, int top, int leftForValue = -1);

		uint				pathTracerWidth;
		uint				pathTracerHeight;
		clock_t				startTime;				

		EONLabelCtrl		*generalElapsedTime;
		EONLabelCtrl		*sceneNumberOfTriangles;
		EONLabelCtrl		*sceneSizeOfBVH;
		EONLabelCtrl		*sceneDepthOfBVH;
		EONLabelCtrl		*sceneNumberOfLights;
		EONLabelCtrl		*pathRaysPerPixel;
		EONLabelCtrl		*pathPixelsPerSecond;
		EONLabelCtrl		*pathRaysPerSecond;
		EONUserDisplayCtrl	*displayControl;
	};

}


#endif