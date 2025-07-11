#ifndef INI_H_GUARD
#define INI_H_GUARD
#include "framedata.h"
#include "cg.h"
#include "parts.h"

extern struct Settings
{
	//default
	float color[3]{0.202f, 0.243f, 0.293f};
	int zoomLevel = 1;
	bool bilinear = 0;
	int theme = 0;
	float fontSize = 18.f;
	short posX = 100;
	short posY = 100;
	short winSizeX = 1280;
	short winSizeY = 800;
	bool maximized = true;
	bool idleUpdate = true;
} gSettings;

bool LoadFromIni(FrameData *framedata, CG *cg, Parts *parts, const std::string& iniPath);
void InitIni();

#endif /* INI_H_GUARD */
