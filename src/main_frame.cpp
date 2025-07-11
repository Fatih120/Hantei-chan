#include "main_frame.h"

#include "main.h"
#include "filedialog.h"
#include "ini.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <sstream>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include <windows.h>

#include <glad/glad.h>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "stb_image_write.h"

#include "misc.h"

#include "winbase.h"

MainFrame::MainFrame(ContextGl *context_):
context(context_),
currState{},
mainPane(&render, &framedata, currState),
rightPane(&render, &framedata, currState),
boxPane(&render, &framedata, currState),
curPalette(0),
x(0),y(150),
parts(&cg),
render(&cg, &parts)
{
	currState.layers = nullptr;
	LoadSettings();
	stbi_flip_vertically_on_write(true);

	
}

MainFrame::~MainFrame()
{
	ImGui::SaveIniSettingsToDisk(ImGui::GetCurrentContext()->IO.IniFilename);
}

void MainFrame::LoadSettings()
{
	LoadTheme(gSettings.theme);
	SetZoom(gSettings.zoomLevel);
	smoothRender = gSettings.bilinear;
	memcpy(clearColor, gSettings.color, sizeof(float)*3);
}

void MainFrame::Draw()
{
	if(drawImgui)
	{
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		DrawUi();
		DrawBack();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	}
	else
	{
		RenderUpdate();
		DrawBack();
	}

	SwapBuffers(context->dc);

	gSettings.theme = style_idx;
	gSettings.zoomLevel = zoom_idx;
	gSettings.bilinear = smoothRender;
	memcpy(gSettings.color, clearColor, sizeof(float)*3);
}

void MainFrame::DrawBack()
{
	render.filter = smoothRender;
	if(screenShot || outputAnimElement)
		glClearColor(0, 0, 0, 0.f);
	else
		glClearColor(clearColor[0], clearColor[1], clearColor[2], 1.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	render.x = (x+clientRect.x/2)/render.scale;
	render.y = (y+clientRect.y/2)/render.scale;
	render.Draw();

	if(parts.loaded)
		render.UpdateProj(clientRect.x, clientRect.y);

	if(screenShot || outputAnimElement)
	{
		size_t size = clientRect.x*clientRect.y*4;
		unsigned char* imageData = new unsigned char[size];
		glReadPixels(0, 0, clientRect.x, clientRect.y, GL_RGBA, GL_UNSIGNED_BYTE, imageData);
		for(int i = 0; i<clientRect.x*clientRect.y*4; i+=4) //Fix bad transparency issue.
		{
			auto& alpha = imageData[i+3];
			auto fixAlpha = std::clamp(sqrtf((float)alpha / 256.0) * 256.f, 0.f, 256.f);
			auto alphaFactor = (double)fixAlpha / 256.0;
			imageData[i + 3] = (int)fixAlpha;
			if(alphaFactor > 0)
			{
				float rgb[3];
				for(int ci = 0; ci < 3; ++ci)
				{
					rgb[ci] = (imageData[i+ci]/255.f) / alphaFactor;
					if(rgb[ci] > 1.)
						imageData[i+ci] = 255;
					else
						imageData[i+ci] = rgb[ci]*255;
				}
			}
		}
		
		int len;
		unsigned char* outData = stbi_write_png_to_mem(imageData, 0, clientRect.x, clientRect.y, 4, &len);
		delete[] imageData;

		std::stringstream ss;
		ss.flags(std::ios_base::right);
		if (screenShot) {
			ss << "pat-" << std::setfill('0') << std::setw(3) << currState.pattern << "_fr-" << currState.frame << ".png";
		}
		else {
			ss << "pat-" << std::setfill('0') << std::setw(3) << currState.pattern << "_fr-" << currState.frame << "_" << duration << ".png";
		}

		std::filesystem::path outName(dirLocation);
		outName /= ss.str();
		std::ofstream pngOut(outName, std::ios_base::binary);
		if(pngOut.is_open())
		{
			pngOut.write((char*)outData, len);
		}
		free(outData);
		screenShot = false;
	}
}

void MainFrame::DrawUi()
{
	ImGuiID errorPopupId = ImGui::GetID("Loading Error");
	

	//Fullscreen docker to provide the layout for the panes
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::Begin("Dock Window", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoNavFocus |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoBackground 
	);
		ImGui::PopStyleVar(3);
		Menu(errorPopupId);
		
		ImGuiID dockspaceID = ImGui::GetID("Dock Space");
		if (!ImGui::DockBuilderGetNode(dockspaceID)) {
			ImGui::DockBuilderRemoveNode(dockspaceID);
			ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockspaceID, clientRect); 

			ImGuiID toSplit = dockspaceID;
			ImGuiID dock_left_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Left, 0.30f, nullptr, &toSplit);
			ImGuiID dock_right_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Right, 0.45f, nullptr, &toSplit);
			ImGuiID dock_down_id = ImGui::DockBuilderSplitNode(toSplit, ImGuiDir_Down, 0.20f, nullptr, &toSplit);

			ImGui::DockBuilderDockWindow("Left Pane", dock_left_id);
			ImGui::DockBuilderDockWindow("Right Pane", dock_right_id);
 			ImGui::DockBuilderDockWindow("Box Pane", dock_down_id);

			ImGui::DockBuilderFinish(dockspaceID);
		}
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f),
			ImGuiDockNodeFlags_PassthruCentralNode |
			ImGuiDockNodeFlags_NoDockingInCentralNode |
			ImGuiDockNodeFlags_AutoHideTabBar 
			//ImGuiDockNodeFlags_NoSplit
		); 
	ImGui::End();

	ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	if (ImGui::BeginPopupModal("Loading Error", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("There was a problem loading the file.\n"
			"The file couldn't be accessed or it's not a valid file.\n\n");
		ImGui::Separator();
		if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
		ImGui::EndPopup();
	}

	mainPane.Draw(); 
	rightPane.Draw();
	boxPane.Draw();
	aboutWindow.Draw();
	helpWindow.Draw();

	RenderUpdate();
}

void MainFrame::RenderUpdate()
{
	Sequence *seq;
	if((seq = framedata.get_sequence(currState.pattern)) &&
		seq->frames.size() > 0)
	{
		if(currState.animeSeq != currState.pattern)
		{
			duration = 0;
			loopCounter = 0;
		}
		if(currState.animating || outputAnimElement)
		{
			currState.animeSeq = currState.pattern;
			auto GetNextFrame = [&,this](bool decreaseLoopCounter) {
				auto seq = framedata.get_sequence(currState.pattern);
				if (seq && !seq->frames.empty())
				{
					auto& af = seq->frames[currState.frame].AF;
					if (af.aniType == 1) {
						if (currState.frame + 1 >= seq->frames.size()) {
							return 0;
						}
						else {
							return currState.frame + 1;
						}
					}
					else if (af.aniType == 2)
					{
						if ((af.aniFlag & 0x2) && loopCounter < 0)
						{
							if (af.aniFlag & 0x8) {
								return currState.frame + af.loopEnd;
							}
							else {
								return af.loopEnd;
							}
						}
						else
						{
							if (af.aniFlag & 0x2 && decreaseLoopCounter) {
								--loopCounter;
							}
							if (af.aniFlag & 0x4) {
								return currState.frame + af.jump;
							}
							else {
								return af.jump;
							}
						}
					}
					else {
						return 0;
					}
				}
				return currState.frame;
			};
			if(duration >= seq->frames[currState.frame].AF.duration)
			{
				auto seq = framedata.get_sequence(currState.pattern);
				if(seq && !seq->frames.empty())
				{
					/*
					auto &af = seq->frames[currState.frame].AF;
					if(af.aniType == 1)
						currState.frame += 1;
					else if(af.aniType == 2)
					{
						if((af.aniFlag & 0x2) && loopCounter < 0)
						{
							if(af.aniFlag & 0x8)
								currState.frame += af.loopEnd;
							else
								currState.frame = af.loopEnd;
						}
						else
						{
							if(af.aniFlag & 0x4)
								currState.frame += af.jump;
							else
								currState.frame = af.jump;
							if(af.aniFlag & 0x2)
								--loopCounter;
						}
					}
					else
						currState.frame = 0;
					if(currState.frame >= seq->frames.size())
						currState.frame = 0;
					*/
					currState.frame = GetNextFrame(true);
					duration = 0;
					currState.nextFrame = GetNextFrame(false);
				}
				outputAnimElement = false;
			}
			if (seq->frames[currState.frame].AF.interpolationType == 1) {
				interpolationFactor = 1 - float(duration) / seq->frames[currState.frame].AF.duration;
			}
			else {
				interpolationFactor = 1;
			}
			
			duration++;
		}
		else {
			interpolationFactor = 1;
		}

		auto &frame =  seq->frames[currState.frame];
		currState.layers = &frame.AF.layers;
		if (interpolationFactor < 1 && seq->frames[currState.frame].AF.aniType != 0 && seq->frames[currState.frame].AF.aniType != 3) {
			currState.nextLayers = &seq->frames[currState.nextFrame].AF.layers;
		}
		else {
			currState.nextLayers = &frame.AF.layers;
		}
		
		render.GenerateHitboxVertices(frame.hitboxes);


		if(frame.AF.loopCount>0)
			loopCounter = frame.AF.loopCount;
		

	}
	else
	{
		currState.layers = nullptr;
		
		render.DontDraw();
	}
	render.SwitchImage(currState.layers, currState.nextLayers,interpolationFactor);
}

void MainFrame::AdvancePattern(int dir)
{
	currState.pattern+= dir;
	if(currState.pattern < 0)
		currState.pattern = 0;
	else if(currState.pattern >= framedata.get_sequence_count())
		currState.pattern = framedata.get_sequence_count()-1;
	currState.frame = 0;
	currState.nextFrame = 0;
}

void MainFrame::AdvanceFrame(int dir)
{
	auto seq = framedata.get_sequence(currState.pattern);
	currState.frame += dir;
	if(currState.frame < 0)
		currState.frame = 0;
	else if(seq && currState.frame >= seq->frames.size())
		currState.frame = seq->frames.size()-1;
}

void MainFrame::ChangeOffset(int x, int y)
{
	auto seq = framedata.get_sequence(currState.pattern);
	if(seq && currState.frame < seq->frames.size())
	{
		auto &l = seq->frames[currState.frame].AF.layers[currState.selectedLayer];
		l.offset_x += x;
		l.offset_y += y;
	}
}

void MainFrame::UpdateBackProj(float x, float y)
{
	render.UpdateProj(x, y);
	glViewport(0, 0, x, y);
}

void MainFrame::HandleMouseDrag(int x_, int y_, bool dragRight, bool dragLeft)
{
	if(dragRight)
	{
		boxPane.BoxDrag(x_, y_);
	}
	else if(dragLeft)
	{
		x += x_;
		y += y_;
	}
}

void MainFrame::RightClick(int x_, int y_)
{
	boxPane.BoxStart((x_-x-clientRect.x/2)/render.scale, (y_-y-clientRect.y/2)/render.scale);
}

void MainFrame::AdjustBoxes(int dx, int dy, int mode)
{
    auto seq = framedata.get_sequence(currState.pattern);
    if(!seq || seq->frames.size() == 0) return;

    auto& frames = seq->frames;
    BoxList& boxes = frames[currState.frame].hitboxes;
	
    if (mode == 3) { //mv all
        for (auto& [id, box] : boxes) {
            box.xy[0] += dx;
            box.xy[1] += dy;
            box.xy[2] += dx;
            box.xy[3] += dy;
        }
        return;
    }
	
    int currentBox = boxPane.GetCurrentBox();
    auto it = boxes.find(currentBox);
    if(it == boxes.end()) return;

    Hitbox& box = it->second;
    switch (mode)
	{
        case 0: //mv
            box.xy[0] += dx;
            box.xy[1] += dy;
            box.xy[2] += dx;
            box.xy[3] += dy;
            break;
        case 1: //exp
            if (dx < 0) box.xy[0] += dx;
            if (dx > 0) box.xy[2] += dx;
            if (dy < 0) box.xy[1] += dy;
            if (dy > 0) box.xy[3] += dy;
            break;
        case 2: //shrk
            if (dx < 0) box.xy[2] += dx;
            if (dx > 0) box.xy[0] += dx;
            if (dy < 0) box.xy[3] += dy;
            if (dy > 0) box.xy[1] += dy;
            break;
    }
}

bool MainFrame::HandleKeys(uint64_t vkey)
{
	bool ctrlDown  = GetAsyncKeyState(VK_CONTROL) & 0x8000;
	bool shiftDown = GetAsyncKeyState(VK_SHIFT)   & 0x8000;
	bool altDown   = GetAsyncKeyState(VK_MENU)    & 0x8000;
	bool freed = !(ctrlDown || shiftDown || altDown);
	
	int dx = 0, dy = 0;
	if (GetAsyncKeyState(VK_LEFT)  & 0x8000) dx -= 2;
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000) dx += 2;
	if (GetAsyncKeyState(VK_UP)    & 0x8000) dy -= 2;
	if (GetAsyncKeyState(VK_DOWN)  & 0x8000) dy += 2;
	
	if (dx != 0 || dy != 0) { //yanderedev i dont know if i should be remotely doing it like this
		if (freed) {
			if (dx != 0) AdvanceFrame(dx / 2); // i probably really shouldnt it also makes the horizontal scroll ugly
			if (dy != 0) AdvancePattern(dy / 2);
			return true;
		}
		if(shiftDown && !ctrlDown && !altDown && GetAsyncKeyState('A')) {
			AdjustBoxes(dx, dy, 3);
			return true;
		}
		if(shiftDown && ctrlDown && altDown) {
			AdjustBoxes(dx, dy, 2);
			return true;
		}
		if(shiftDown && ctrlDown && !altDown) {
			AdjustBoxes(dx, dy, 1);
			return true;
		}
		if(shiftDown && !ctrlDown && !altDown) {
			AdjustBoxes(dx, dy, 0);
			return true;
		}
		if(ctrlDown && !shiftDown && !altDown) {
			ChangeOffset(dx / 2, dy / 2);
			return true;
		}
	}
	
	switch (vkey)
	{
		case 'S':
			if (ctrlDown) {
				if (currentFilePath.empty())
					currentFilePath = FileDialog(fileType::HA6, true);
				if (!currentFilePath.empty())
					framedata.save(currentFilePath.c_str());
				return true;
			}
			break;
		case 'P':
			if (ctrlDown) {
				outputAnimElement = true;
				duration = 0;
			} else if (!outputAnimElement) {
				screenShot = true;
			}
			return true;
		case 'Z': boxPane.AdvanceBox(-1); return true;
		case 'X': boxPane.AdvanceBox(+1); return true;
		case 'F': drawImgui = !drawImgui; return true;
		case 'L': render.drawLines = !render.drawLines; return true;
		case 'H': render.drawBoxes = !render.drawBoxes; return true;
	}
	
	return false;

}


void MainFrame::ChangeClearColor(float r, float g, float b)
{
	clearColor[0] = r;
	clearColor[1] = g;
	clearColor[2] = b;
}

void MainFrame::SetZoom(int level)
{
	zoom_idx = level;
	switch (level)
	{
	case 0: render.SetScale(0.5f); break;
	case 1: render.SetScale(1.f); break;
	case 2: render.SetScale(1.5f); break;
	case 3: render.SetScale(2.f); break;
	case 4: render.SetScale(3.f); break;
	case 5: render.SetScale(4.f); break;
	}
}

void MainFrame::LoadTheme(int i )
{
	style_idx = i;
	switch (i)
	{
		case 0: ImGui::StyleColorsDark(); ImGui::GetStyle().Colors[ImGuiCol_TextDisabled] = ImVec4(0.20f, 0.80f, 1.00f, 0.80f); ChangeClearColor(0.202f, 0.243f, 0.293f); break;
		case 1: WarmStyle(); break;
		case 2: ImGui::StyleColorsLight(); ChangeClearColor(0.534f, 0.568f, 0.587f); break;
		case 3: ImGui::StyleColorsClassic(); ChangeClearColor(0.142f, 0.075f, 0.147f); break;
		case 4: LeafStyle(); break;
	}
}

void MainFrame::Menu(unsigned int errorPopupId)
{
	if (ImGui::BeginMenuBar())
	{
		//ImGui::Separator();
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New file"))
			{
				framedata.initEmpty();
				currentFilePath.clear();
				mainPane.RegenerateNames();
			}
			if (ImGui::MenuItem("Close file"))
			{
				framedata.Free();
				currentFilePath.clear();
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Load from .txt..."))
			{
				std::string &&file = FileDialog(fileType::TXT);
				if (!file.empty())
				{
					if (!LoadFromIni(&framedata, &cg, &parts, file))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					else
					{
						currentFilePath.clear();
						mainPane.RegenerateNames();
						render.SwitchImage(nullptr, nullptr, 1);
			
						std::string paletteFile = file;
						paletteFile.erase(paletteFile.find_last_of('.'));
						if (paletteFile.size() >= 2 && paletteFile.compare(paletteFile.size() - 2, 2, "_0") == 0)
							paletteFile.erase(paletteFile.size() - 2);
						paletteFile += ".pal";
						if (!cg.loadPalette(paletteFile.c_str()))
						{
							ImGui::OpenPopup(errorPopupId);
						}
						render.SwitchImage(nullptr, nullptr, 1);
					}
				}
			}

			if (ImGui::MenuItem("Load HA6..."))
			{
				std::string &&file = FileDialog(fileType::HA6);
				if(!file.empty())
				{
					if(!framedata.load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					else
					{
						currentFilePath = file;
						mainPane.RegenerateNames();
					}
				}
			}

			if (ImGui::MenuItem("Load HA6 and Patch..."))
			{
				std::string &&file = FileDialog(fileType::HA6);
				if(!file.empty())
				{
					if(!framedata.load(file.c_str(), true))
					{
						ImGui::OpenPopup(errorPopupId);
					}
					else
						mainPane.RegenerateNames();
					currentFilePath.clear();
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Save", "Ctrl+S")) 
			{
				if(currentFilePath.empty())
					currentFilePath = FileDialog(fileType::HA6, true);
				if(!currentFilePath.empty())
				{
					framedata.save(currentFilePath.c_str());
				}
			}

			if (ImGui::MenuItem("Save as...")) 
			{
				std::string &&file = FileDialog(fileType::HA6, true);
				if(!file.empty())
				{
					framedata.save(file.c_str());
					currentFilePath = file;
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Load CG...")) 
			{
				std::string &&file = FileDialog(fileType::CG);
				if(!file.empty())
				{
					if(!cg.load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);	
					}
					render.SwitchImage(nullptr, nullptr,1);
				}
			}

			if (ImGui::MenuItem("Load Parts...")) 
			{
				std::string &&file = FileDialog(fileType::PAT);
				if(!file.empty())
				{
					if(!parts.Load(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);	
					}
					render.SwitchImage(nullptr, nullptr, 1);
				}
			}

			if (ImGui::MenuItem("Load palette...")) 
			{
				std::string &&file = FileDialog(fileType::PAL);
				if(!file.empty())
				{
					if(!cg.loadPalette(file.c_str()))
					{
						ImGui::OpenPopup(errorPopupId);	
					}
					render.SwitchImage(nullptr, nullptr, 1);
				}
			}

			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Preferences"))
		{
			if (ImGui::BeginMenu("Switch preset style"))
			{		
				if (ImGui::Combo("Style", &style_idx, "Dark\0Warm\0Light\0ImGui\0Leaf\0"))
				{
					LoadTheme(style_idx);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Background color"))
			{
				ImGui::ColorEdit3("##clearColor", (float*)&clearColor, ImGuiColorEditFlags_NoInputs);
				ImGui::EndMenu();
			}
			if (cg.getPalNumber() > 0 && ImGui::BeginMenu("Palette number"))
			{
				ImGui::SetNextItemWidth(80);
				ImGui::InputInt("Palette", &curPalette);
				if(curPalette >= cg.getPalNumber())
					curPalette = cg.getPalNumber()-1;
				else if(curPalette < 0)
					curPalette = 0;
				if(cg.changePaletteNumber(curPalette))
					render.SwitchImage(nullptr, nullptr, 1);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Zoom level"))
			{
				ImGui::SetNextItemWidth(80);
				if (ImGui::Combo("Zoom", &zoom_idx, "x0.5\0x1\0x1.5\0x2\0x3\0x4\0"))
				{
					SetZoom(zoom_idx);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Filter"))
			{
				if (ImGui::Checkbox("Bilinear", &smoothRender))
				{
					render.filter = smoothRender;
					render.SwitchImage(nullptr, nullptr, 1);
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Tools"))
		{
			if (ImGui::MenuItem("Fill missing layers")){
				for(auto &seq : framedata.m_sequences)
				{
					for(auto &frame : seq.frames)
					{
						if(frame.AF.layers.size() < 3)
							frame.AF.layers.resize(3);
					}
				}
			}
			if (ImGui::BeginMenu("Scale all layers")){
				ImGui::InputFloat("Factor", &reScaleFactor);
				if (ImGui::Button("Apply", ImVec2(120, 0)))
				{
					for(auto &seq : framedata.m_sequences)
					{
						for(auto &frame : seq.frames)
						{
							for(auto &layer : frame.AF.layers)
							{
									layer.scale[0] *= reScaleFactor;
									layer.scale[1] *= reScaleFactor;
							}
							for (auto &[k,v] : frame.hitboxes)
							{
								for(int jii = 0; jii < 4; ++jii)
									v.xy[jii] *= reScaleFactor;
							}
						}
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem("About")) aboutWindow.isVisible = !aboutWindow.isVisible;
			if (ImGui::MenuItem("Shortcuts")) helpWindow.isVisible = !helpWindow.isVisible;
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
}

	void MainFrame::WarmStyle()
	{
		ImVec4* colors = ImGui::GetStyle().Colors;

		colors[ImGuiCol_Text]                   = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
		colors[ImGuiCol_TextDisabled]           = ImVec4(0.60f, 0.60f, 0.60f, 1.00f);
		colors[ImGuiCol_WindowBg]               = ImVec4(0.95f, 0.91f, 0.85f, 1.00f);
		colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_PopupBg]                = ImVec4(0.98f, 0.96f, 0.93f, 1.00f);
		colors[ImGuiCol_Border]                 = ImVec4(0.00f, 0.00f, 0.00f, 0.30f);
		colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg]                = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
		colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.95f, 1.00f, 0.62f, 1.00f);
		colors[ImGuiCol_FrameBgActive]          = ImVec4(0.98f, 1.00f, 0.81f, 1.00f);
		colors[ImGuiCol_TitleBg]                = ImVec4(0.82f, 0.73f, 0.64f, 0.81f);
		colors[ImGuiCol_TitleBgActive]          = ImVec4(0.97f, 0.65f, 0.00f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(1.00f, 1.00f, 1.00f, 0.51f);
		colors[ImGuiCol_MenuBarBg]              = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
		colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.89f, 0.83f, 0.76f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab]          = ImVec4(1.00f, 0.96f, 0.87f, 0.99f);
		colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(1.00f, 0.98f, 0.94f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.72f, 0.66f, 0.48f, 0.99f);
		colors[ImGuiCol_CheckMark]              = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
		colors[ImGuiCol_SliderGrab]             = ImVec4(1.00f, 0.52f, 0.00f, 1.00f);
		colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.55f, 0.53f, 0.32f, 1.00f);
		colors[ImGuiCol_Button]                 = ImVec4(0.74f, 1.00f, 0.53f, 0.25f);
		colors[ImGuiCol_ButtonHovered]          = ImVec4(1.00f, 0.77f, 0.41f, 0.96f);
		colors[ImGuiCol_ButtonActive]           = ImVec4(1.00f, 0.47f, 0.00f, 1.00f);
		colors[ImGuiCol_Header]                 = ImVec4(0.74f, 0.57f, 0.33f, 0.31f);
		colors[ImGuiCol_HeaderHovered]          = ImVec4(0.94f, 0.75f, 0.36f, 0.42f);
		colors[ImGuiCol_HeaderActive]           = ImVec4(1.00f, 0.75f, 0.01f, 0.61f);
		colors[ImGuiCol_Separator]              = ImVec4(0.38f, 0.34f, 0.25f, 0.66f);
		colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.76f, 0.70f, 0.59f, 0.98f);
		colors[ImGuiCol_SeparatorActive]        = ImVec4(0.32f, 0.32f, 0.32f, 0.45f);
		colors[ImGuiCol_ResizeGrip]             = ImVec4(0.35f, 0.35f, 0.35f, 0.17f);
		colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.41f, 0.80f, 1.00f, 0.84f);
		colors[ImGuiCol_ResizeGripActive]       = ImVec4(1.00f, 0.61f, 0.23f, 1.00f);
		colors[ImGuiCol_Tab]                    = ImVec4(0.79f, 0.74f, 0.64f, 0.00f);
		colors[ImGuiCol_TabHovered]             = ImVec4(1.00f, 0.64f, 0.06f, 0.85f);
		colors[ImGuiCol_TabActive]              = ImVec4(0.69f, 0.40f, 0.12f, 0.31f);
		colors[ImGuiCol_TabUnfocused]           = ImVec4(0.93f, 0.92f, 0.92f, 0.98f);
		colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.91f, 0.87f, 0.74f, 1.00f);
		colors[ImGuiCol_DockingPreview]         = ImVec4(0.26f, 0.98f, 0.35f, 0.22f);
		colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
		colors[ImGuiCol_PlotLines]              = ImVec4(0.39f, 0.39f, 0.39f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.58f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.40f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.45f, 0.00f, 1.00f);
		colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.69f, 0.53f, 0.32f, 0.30f);
		colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.69f, 0.58f, 0.44f, 1.00f);
		colors[ImGuiCol_TableBorderLight]       = ImVec4(0.70f, 0.62f, 0.42f, 0.40f);
		colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.30f, 0.30f, 0.30f, 0.09f);
		colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.98f, 0.89f, 0.35f);
		colors[ImGuiCol_DragDropTarget]         = ImVec4(0.26f, 0.98f, 0.94f, 0.95f);
		colors[ImGuiCol_NavHighlight]           = ImVec4(0.26f, 0.97f, 0.98f, 0.80f);
		colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.70f, 0.70f, 0.70f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.20f, 0.20f, 0.20f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.20f, 0.20f, 0.20f, 0.35f);
		ChangeClearColor(0.324f, 0.409f, 0.185f);
	}

	void MainFrame::LeafStyle()
	{
		ImVec4* colors = ImGui::GetStyle().Colors;
		
		colors[ImGuiCol_Text]                   = ImVec4(0.95f, 1.00f, 0.90f, 1.00f);
		colors[ImGuiCol_TextDisabled]           = ImVec4(0.40f, 0.80f, 0.40f, 1.00f);
		colors[ImGuiCol_WindowBg]               = ImVec4(0.00f, 0.08f, 0.03f, 1.00f);
		colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.08f, 0.00f, 0.90f);
		colors[ImGuiCol_PopupBg]                = ImVec4(0.13f, 0.20f, 0.13f, 0.98f);
		colors[ImGuiCol_Border]                 = ImVec4(0.25f, 0.18f, 0.12f, 0.60f);
		colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg]                = ImVec4(0.23f, 0.18f, 0.12f, 1.00f);
		colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.32f, 0.24f, 0.15f, 1.00f);
		colors[ImGuiCol_FrameBgActive]          = ImVec4(0.41f, 0.32f, 0.21f, 1.00f);
		colors[ImGuiCol_TitleBg]                = ImVec4(0.14f, 0.22f, 0.14f, 0.90f);
		colors[ImGuiCol_TitleBgActive]          = ImVec4(0.18f, 0.28f, 0.18f, 1.00f);
		colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.18f, 0.14f, 0.12f, 0.70f);
		colors[ImGuiCol_MenuBarBg]              = ImVec4(0.13f, 0.20f, 0.13f, 1.00f);
		colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.13f, 0.20f, 0.13f, 1.00f);
		colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.23f, 0.18f, 0.12f, 0.85f);
		colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.32f, 0.24f, 0.15f, 0.95f);
		colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.41f, 0.32f, 0.21f, 1.00f);
		colors[ImGuiCol_CheckMark]              = ImVec4(0.51f, 0.78f, 0.46f, 1.00f);
		colors[ImGuiCol_SliderGrab]             = ImVec4(0.51f, 0.78f, 0.46f, 1.00f);
		colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.36f, 0.55f, 0.32f, 1.00f);
		colors[ImGuiCol_Button]                 = ImVec4(0.23f, 0.18f, 0.12f, 0.65f);
		colors[ImGuiCol_ButtonHovered]          = ImVec4(0.32f, 0.24f, 0.15f, 0.85f);
		colors[ImGuiCol_ButtonActive]           = ImVec4(0.41f, 0.32f, 0.21f, 1.00f);
		colors[ImGuiCol_Header]                 = ImVec4(0.19f, 0.29f, 0.19f, 0.54f);
		colors[ImGuiCol_HeaderHovered]          = ImVec4(0.27f, 0.40f, 0.27f, 0.80f);
		colors[ImGuiCol_HeaderActive]           = ImVec4(0.36f, 0.55f, 0.32f, 1.00f);
		colors[ImGuiCol_Separator]              = ImVec4(0.25f, 0.18f, 0.12f, 0.60f);
		colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.32f, 0.24f, 0.15f, 0.85f);
		colors[ImGuiCol_SeparatorActive]        = ImVec4(0.41f, 0.32f, 0.21f, 1.00f);
		colors[ImGuiCol_ResizeGrip]             = ImVec4(0.19f, 0.29f, 0.19f, 0.25f);
		colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.27f, 0.40f, 0.27f, 0.67f);
		colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.36f, 0.55f, 0.32f, 0.95f);
		colors[ImGuiCol_Tab]                    = ImVec4(0.23f, 0.18f, 0.12f, 0.80f);
		colors[ImGuiCol_TabHovered]             = ImVec4(0.36f, 0.55f, 0.32f, 0.80f);
		colors[ImGuiCol_TabActive]              = ImVec4(0.27f, 0.40f, 0.27f, 0.90f);
		colors[ImGuiCol_TabUnfocused]           = ImVec4(0.13f, 0.20f, 0.13f, 0.80f);
		colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.19f, 0.29f, 0.19f, 0.85f);
		colors[ImGuiCol_DockingPreview]         = ImVec4(0.51f, 0.78f, 0.46f, 0.30f);
		colors[ImGuiCol_DockingEmptyBg]         = ImVec4(0.10f, 0.18f, 0.13f, 1.00f);
		colors[ImGuiCol_PlotLines]              = ImVec4(0.51f, 0.78f, 0.46f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered]       = ImVec4(0.41f, 0.32f, 0.21f, 1.00f);
		colors[ImGuiCol_PlotHistogram]          = ImVec4(0.51f, 0.78f, 0.46f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(0.41f, 0.32f, 0.21f, 1.00f);
		colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.23f, 0.18f, 0.12f, 0.80f);
		colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.25f, 0.18f, 0.12f, 1.00f);
		colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.18f, 0.12f, 0.40f);
		colors[ImGuiCol_TableRowBg]             = ImVec4(0.10f, 0.18f, 0.13f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt]          = ImVec4(0.13f, 0.20f, 0.13f, 0.09f);
		colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.36f, 0.55f, 0.32f, 0.35f);
		colors[ImGuiCol_DragDropTarget]         = ImVec4(0.51f, 0.78f, 0.46f, 0.95f);
		colors[ImGuiCol_NavHighlight]           = ImVec4(0.51f, 0.78f, 0.46f, 0.80f);
		colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(0.85f, 0.90f, 0.80f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.10f, 0.18f, 0.13f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.10f, 0.18f, 0.13f, 0.35f);
		ChangeClearColor(0.10f, 0.18f, 0.13f);
	}
	