#include "Interface.h"
#include <direct.h> //_getcwd()
#include <io.h> // Check for recovery
#include <thread> //Check for updates
#include <filesystem>

#include "GLApp/GLFileBox.h"
#include "GLApp/GLMessageBox.h"
#include "GLApp/GLInputBox.h"
#include "GLApp/GLSaveDialog.h"
#include "GLApp\GLToolkit.h"
#include "GLApp/MathTools.h" //IDX
#include "RecoveryDialog.h"
#include "Facet.h"

//Updater
#include <PugiXML\pugixml.hpp>
#include "Web.h"
#include "ZipUtils/zip.h"
#include "ZipUtils/unzip.h"
#include "File.h" //File utils (Get extension, etc)

extern Worker worker;
extern std::vector<string> formulaPrefixes;
//extern const char* appName;

extern const char *fileLFilters;
extern const char *fileInsFilters;
extern const char *fileSFilters;
extern const char *fileDesFilters;

extern int   cSize;
extern int   cWidth[];
extern char *cName[];
extern std::string appName;
extern std::string appId;
extern int appVersion;

Interface::Interface() {
	//Get number of cores
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	numCPU = sysinfo.dwNumberOfProcessors;

	antiAliasing = TRUE;
	whiteBg = FALSE;
	autoUpdateFormulas = FALSE;
	compressSavedFiles = TRUE;
	/*double gasMass=28;
	double totalOutgassing=0.0; //total outgassing in Pa*m3/sec (internally everything is in SI units)
	double totalInFlux = 0.0; //total incoming molecules per second. For anisothermal system, it is (totalOutgassing / Kb / T)*/
	autoSaveFrequency = 10.0; //in minutes
	autoSaveSimuOnly = FALSE;
	autosaveFilename = "";
	compressProcessHandle = NULL;
	autoFrameMove = TRUE;

	installId = "default";
	checkForUpdates = FALSE;
	appLaunchesWithoutAsking = 0; //Default value

	lastSaveTime = 0.0f;
	lastSaveTimeSimu = 0.0f;
	changedSinceSave = FALSE;
	//lastHeartBeat=0.0f;
	nbDesStart = 0;
	nbHitStart = 0;

	lastUpdate = 0.0;
	nbFormula = 0;
	nbRecent = 0;

	nbView = 0;
	nbSelection = 0;
	idView = 0;
	idSelection = 0;

#ifdef _DEBUG
	nbProc = 1;
#else
	SATURATE(numCPU, 1, MIN(MAX_PROCESS, 16)); //limit the auto-detected processes to the maximum available, at least one, and max 16 (above it speed improvement not obvious)
	nbProc = numCPU;
#endif

	curViewer = 0;
	strcpy(currentDir, ".");
	strcpy(currentSelDir, ".");
	memset(formulas, 0, sizeof formulas);

	formulaSettings = NULL;
	collapseSettings = NULL;
	moveVertex = NULL;
	scaleVertex = NULL;
	scaleFacet = NULL;
	selectDialog = NULL;
	selectTextureType = NULL;
	moveFacet = NULL;
	extrudeFacet = NULL;
	mirrorFacet = NULL;
	mirrorVertex = NULL;
	splitFacet = NULL;
	buildIntersection = NULL;
	rotateFacet = NULL;
	rotateVertex = NULL;
	alignFacet = NULL;
	addVertex = NULL;
	loadStatus = NULL;
	facetCoordinates = NULL;
	vertexCoordinates = NULL;
	smartSelection = NULL;

	m_strWindowTitle = appName;
	latestVersionId = appVersion;
	wnd->SetBackgroundColor(212, 208, 200);
	m_bResizable = TRUE;
	m_minScreenWidth = 800;
	m_minScreenHeight = 600;
	tolerance = 1e-8;
	largeArea = 1.0;
	planarityThreshold = 1e-5;
}

void Interface::UpdateViewerFlags() {
	viewer[curViewer]->showNormal = showNormal->GetState();
	viewer[curViewer]->showRule = showRule->GetState();
	viewer[curViewer]->showUV = showUV->GetState();
	viewer[curViewer]->showLeak = showLeak->GetState();
	viewer[curViewer]->showHit = showHit->GetState();
	viewer[curViewer]->showLine = showLine->GetState();
	viewer[curViewer]->showVolume = showVolume->GetState();
	viewer[curViewer]->showTexture = showTexture->GetState();
	BOOL neededTexture = needsTexture;
	CheckNeedsTexture();

	if (!needsTexture && neededTexture) { //We just disabled mesh
		worker.GetGeometry()->ClearFacetTextures();
	}
	else if (needsTexture && !neededTexture) { //We just enabled mesh
		worker.RebuildTextures();
	}
	viewer[curViewer]->showFilter = showFilter->GetState();
	viewer[curViewer]->showVertex = showVertex->GetState();
	viewer[curViewer]->showIndex = showIndex->GetState();
	//worker.Update(0.0);
}

void Interface::ResetSimulation(BOOL askConfirm) {

	BOOL ok = TRUE;
	if (askConfirm)
		ok = GLMessageBox::Display("Reset simulation ?", "Question", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO) == GLDLG_OK;

	if (ok) {
		worker.ResetStatsAndHits(m_fTime);
		nbDesStart = 0;
		nbHitStart = 0;
	}
	UpdatePlotters();
}

void Interface::UpdateStructMenu() {

	char tmp[128];
	Geometry *geom = worker.GetGeometry();

	structMenu->Clear();
	structMenu->Add("New structure...", MENU_VIEW_NEWSTRUCT);
	structMenu->Add("Delete structure...", MENU_VIEW_DELSTRUCT);
	structMenu->Add(NULL); //Separator
	structMenu->Add("Show all", MENU_VIEW_STRUCTURE, SDLK_F1, CTRL_MODIFIER);
	structMenu->Add("Show previous", MENU_VIEW_PREVSTRUCT, SDLK_F11, CTRL_MODIFIER);
	structMenu->Add("Show next", MENU_VIEW_NEXTSTRUCT, SDLK_F12, CTRL_MODIFIER);
	structMenu->Add(NULL); //Separator

	for (int i = 0; i < geom->GetNbStructure(); i++) {
		sprintf(tmp, "Show #%d (%s)", i + 1, geom->GetStructureName(i));
		if (i < 10)
			structMenu->Add(tmp, MENU_VIEW_STRUCTURE + (i + 1), SDLK_F1 + i + 1, CTRL_MODIFIER);
		else
			structMenu->Add(tmp, MENU_VIEW_STRUCTURE + (i + 1));
	}

	structMenu->SetCheck(MENU_VIEW_STRUCTURE + geom->viewStruct + 1, TRUE);

	UpdateTitle();
}

void Interface::UpdateCurrentDir(char *fileName) {

	strncpy(currentDir, fileName, 1024);
	char *dp = strrchr(currentDir, '\\');
	if (!dp) dp = strrchr(currentDir, '/');
	if (dp) *dp = 0;

}

void Interface::UpdateCurrentSelDir(char *fileName) {

	strncpy(currentDir, fileName, 1024);
	char *dp = strrchr(currentDir, '\\');
	if (!dp) dp = strrchr(currentDir, '/');
	if (dp) *dp = 0;

}

void Interface::UpdateTitle() {

	std::string title;

	Geometry *geom = worker.GetGeometry();

	if (!geom->IsLoaded()) {
		title = appName;
	}
	else {
		if (geom->viewStruct < 0) {
			title = appName + " [" + worker.GetShortFileName() + "]";
		}
		else {
			title = appName + " [" + worker.GetShortFileName() + ": Struct #" + std::to_string(geom->viewStruct + 1) +" " + geom->GetStructureName(geom->viewStruct) +"]";
		}
	}

	SetTitle(title);

}

//-----------------------------------------------------------------------------
// Name: FormatInt()
// Desc: Format an integer in K,M,G,..
//-----------------------------------------------------------------------------
char* Interface::FormatInt(llong v, char *unit)
{

	double x = (double)v;

	static char ret[64];
	if (x < 1E3) {
		sprintf(ret, "%g %s", (double)x, unit);
	}
	else if (x < 1E6) {
		sprintf(ret, "%.1f K%s", x / 1E3, unit);
	}
	else if (x < 1E9) {
		sprintf(ret, "%.2f M%s", x / 1E6, unit);
	}
	else if (x < 1E12) {
		sprintf(ret, "%.2f G%s", x / 1E9, unit);
	}
	else {
		sprintf(ret, "%.2f T%s", x / 1E12, unit);
	}

	return ret;

}

//-----------------------------------------------------------------------------
// Name: FormatPS()
// Desc: Format a double in K,M,G,.. per sec
//-----------------------------------------------------------------------------
char *Interface::FormatPS(double v, char *unit)
{

	static char ret[64];
	if (v < 1000.0) {
		sprintf(ret, "%.1f %s/s", v, unit);
	}
	else if (v < 1000000.0) {
		sprintf(ret, "%.1f K%s/s", v / 1000.0, unit);
	}
	else if (v < 1000000000.0) {
		sprintf(ret, "%.1f M%s/s", v / 1000000.0, unit);
	}
	else {
		sprintf(ret, "%.1f G%s/s", v / 1000000000.0, unit);
	}

	return ret;

}

//-----------------------------------------------------------------------------
// Name: FormatSize()
// Desc: Format a double in K,M,G,.. per sec
//-----------------------------------------------------------------------------
char *Interface::FormatSize(DWORD size)
{

	static char ret[64];
	if (size < 1024UL) {
		sprintf(ret, "%d Bytes", size);
	}
	else if (size < 1048576UL) {
		sprintf(ret, "%.1f KB", (double)size / 1024.0);
	}
	else if (size < 1073741824UL) {
		sprintf(ret, "%.1f MB", (double)size / 1048576.0);
	}
	else {
		sprintf(ret, "%.1f GB", (double)size / 1073741824.0);
	}

	return ret;

}

//-----------------------------------------------------------------------------
// Name: FormatTime()
// Desc: Format time in HH:MM:SS
//-----------------------------------------------------------------------------
char* Interface::FormatTime(float t) {
	static char ret[64];
	int nbSec = (int)(t + 0.5f);
	sprintf(ret, "%02d:%02d:%02d", nbSec / 3600, (nbSec % 3600) / 60, nbSec % 60);
	return ret;
}

void Interface::LoadSelection(char *fName) {

	char fullName[1024];
	strcpy(fullName, "");
	FileReader *f = NULL;

	if (fName == NULL) {
		FILENAME *fn = GLFileBox::OpenFile(currentSelDir, NULL, "Load Selection", fileSelFilters, 0);
		if (fn)
			strcpy(fullName, fn->fullName);
	}
	else {
		strcpy(fullName, fName);
	}

	if (strlen(fullName) == 0) return;

	try {

		Geometry *geom = worker.GetGeometry();
		geom->UnselectAll();
		int nbFacet = geom->GetNbFacet();

		f = new FileReader(fullName);
		while (!f->IsEof()) {
			int s = f->ReadInt();
			if (s >= 0 && s < nbFacet) geom->Select(s);
		}
		geom->UpdateSelection();

		UpdateFacetParams(TRUE);
		UpdateCurrentSelDir(fullName);

	}
	catch (Error &e) {

		char errMsg[512];
		sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fullName);
		GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);

	}

	SAFE_DELETE(f);
	changedSinceSave = FALSE;

}

void Interface::SaveSelection() {

	FileWriter *f = NULL;
	Geometry *geom = worker.GetGeometry();
	if (geom->GetNbSelected() == 0) return;
	GLProgress *progressDlg2 = new GLProgress("Saving file", "Please wait");
	progressDlg2->SetProgress(0.5);
	progressDlg2->SetVisible(TRUE);
	//GLWindowManager::Repaint();

	FILENAME *fn = GLFileBox::SaveFile(currentSelDir, worker.GetShortFileName(), "Save selection", fileSelFilters, 0);

	if (fn) {

		try {


			char *ext = fn->fullName + strlen(fn->fullName) - 4;

			if (!(*ext == '.')) {
				sprintf(fn->fullName, "%s.sel", fn->fullName); //set to default SEL format
				ext = strrchr(fn->fullName, '.');
			}
			ext++;

			f = new FileWriter(fn->fullName);
			int nbSelected = geom->GetNbSelected();
			int nbFacet = geom->GetNbFacet();
			for (int i = 0; i < nbFacet; i++) {
				if (geom->GetFacet(i)->selected) f->WriteInt(i, "\n");
			}

		}
		catch (Error &e) {
			char errMsg[512];
			sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fn->fullName);
			GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);
		}

		SAFE_DELETE(f);

	}
	progressDlg2->SetVisible(FALSE);
	SAFE_DELETE(progressDlg2);
	changedSinceSave = FALSE;
}

void Interface::ExportSelection() {

	Geometry *geom = worker.GetGeometry();
	if (geom->GetNbSelected() == 0) {
		GLMessageBox::Display("Empty selection", "Error", GLDLG_OK, GLDLG_ICONERROR);
		return;
	}

	FILENAME *fn = GLFileBox::SaveFile(currentDir, worker.GetShortFileName(), "Export selection", fileSFilters, 0);
	GLProgress *progressDlg2 = new GLProgress("Saving file...", "Please wait");
	progressDlg2->SetProgress(0.0);
	progressDlg2->SetVisible(TRUE);
	//GLWindowManager::Repaint();
	if (fn) {

		try {
			worker.SaveGeometry(fn->fullName, progressDlg2, TRUE, TRUE);
			AddRecent(fn->fullName);
			//UpdateCurrentDir(fn->fullName);
			//UpdateTitle();
		}
		catch (Error &e) {
			char errMsg[512];
			sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fn->fullName);
			GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);
		}

	}

	progressDlg2->SetVisible(FALSE);
	SAFE_DELETE(progressDlg2);
}

//-----------------------------------------------------------------------------
// Name: UpdateModelParams()
// Desc: Update displayed model parameter on geometry ghange
//-----------------------------------------------------------------------------
void Interface::UpdateModelParams() {

	Geometry *geom = worker.GetGeometry();
	char tmp[256];
	double sumArea = 0;
	facetList->SetSize(cSize, geom->GetNbFacet(), FALSE, TRUE);
	facetList->SetColumnWidths((int*)cWidth);
	facetList->SetColumnLabels((char **)cName);

	UpdateFacetHits(TRUE);
	AABB bb = geom->GetBB();

	for (int i = 0; i < geom->GetNbFacet(); i++) {
		Facet *f = geom->GetFacet(i);
		if (f->sh.area>0) sumArea += f->sh.area*(f->sh.is2sided ? 2.0 : 1.0);
	}

	sprintf(tmp, "V:%d F:%d Dim:(%g,%g,%g) Area:%g", geom->GetNbVertex(), geom->GetNbFacet(),
		(bb.max.x - bb.min.x), (bb.max.y - bb.min.y), (bb.max.z - bb.min.z), sumArea);
	geomNumber->SetText(tmp);

}





void Interface::AnimateViewerChange(int next) {

	double xs1, ys1, xs2, ys2;
	double xe1, ye1, xe2, ye2;
	int sx = m_screenWidth - 205;
	int fWidth = m_screenWidth - 215;
	int fHeight = m_screenHeight - 27;
	int Width2 = fWidth / 2 - 1;
	int Height2 = fHeight / 2 - 1;

	// Reset to layout and make all visible

	for (int i = 0; i < MAX_VIEWER; i++)  viewer[i]->SetVisible(TRUE);
	viewer[0]->SetBounds(3, 3, Width2, Height2);
	viewer[1]->SetBounds(6 + Width2, 3, Width2, Height2);
	viewer[2]->SetBounds(3, 6 + Height2, Width2, Height2);
	viewer[3]->SetBounds(6 + Width2, 6 + Height2, Width2, Height2);


	if (modeSolo) {

		// Go from single to layout
		xs1 = (double)3;
		ys1 = (double)3;
		xs2 = (double)fWidth + xs1;
		ys2 = (double)fHeight + ys1;

		switch (next) {
		case 0:
			xe1 = (double)(3);
			ye1 = (double)(3);
			break;
		case 1:
			xe1 = (double)(5 + Width2);
			ye1 = (double)(3);
			break;
		case 2:
			xe1 = (double)(3);
			ye1 = (double)(5 + Height2);
			break;
		case 3:
			xe1 = (double)(5 + Width2);
			ye1 = (double)(5 + Height2);
			break;
		}

		xe2 = (double)(Width2)+xe1;
		ye2 = (double)(Height2)+ye1;

	}
	else {

		// Go from layout to single
		xe1 = (double)3;
		ye1 = (double)3;
		xe2 = (double)fWidth + xe1;
		ye2 = (double)fHeight + ye1;

		switch (next) {
		case 0:
			xs1 = (double)(3);
			ys1 = (double)(3);
			break;
		case 1:
			xs1 = (double)(5 + Width2);
			ys1 = (double)(3);
			break;
		case 2:
			xs1 = (double)(3);
			ys1 = (double)(5 + Height2);
			break;
		case 3:
			xs1 = (double)(5 + Width2);
			ys1 = (double)(5 + Height2);
			break;
		}

		xs2 = (double)(Width2)+xs1;
		ys2 = (double)(Height2)+ys1;

	}

	double t0 = (double)SDL_GetTicks() / 1000.0;
	double t1 = t0;
	double T = 0.15;

	while ((t1 - t0) < T) {
		double t = (t1 - t0) / T;
		int x1 = (int)(xs1 + t*(xe1 - xs1) + 0.5);
		int y1 = (int)(ys1 + t*(ye1 - ys1) + 0.5);
		int x2 = (int)(xs2 + t*(xe2 - xs2) + 0.5);
		int y2 = (int)(ys2 + t*(ye2 - ys2) + 0.5);
		viewer[next]->SetBounds(x1, y1, x2 - x1, y2 - y1);
		wnd->Paint();
		// Overides moving component
		viewer[next]->Paint();
		// Paint modeless
		int n;
		n = GLWindowManager::GetNbWindow();
		GLWindowManager::RepaintRange(1, n);
		t1 = (double)SDL_GetTicks() / 1000.0;
	}

	modeSolo = !modeSolo;
	SelectViewer(next);

}

void Interface::UpdateViewerPanel() {

	showNormal->SetState(viewer[curViewer]->showNormal);
	showRule->SetState(viewer[curViewer]->showRule);
	showUV->SetState(viewer[curViewer]->showUV);
	showLeak->SetState(viewer[curViewer]->showLeak);
	showHit->SetState(viewer[curViewer]->showHit);
	showVolume->SetState(viewer[curViewer]->showVolume);
	showLine->SetState(viewer[curViewer]->showLine);
	showTexture->SetState(viewer[curViewer]->showTexture);
	showFilter->SetState(viewer[curViewer]->showFilter);
	showVertex->SetState(viewer[curViewer]->showVertex);
	showIndex->SetState(viewer[curViewer]->showIndex);

	// Force all views to have the same showColormap
	viewer[1]->showColormap = viewer[0]->showColormap;
	viewer[2]->showColormap = viewer[0]->showColormap;
	viewer[3]->showColormap = viewer[0]->showColormap;
	worker.GetGeometry()->texColormap = viewer[0]->showColormap;

}

void Interface::SelectViewer(int s) {

	curViewer = s;
	for (int i = 0; i < MAX_VIEWER; i++) viewer[i]->SetSelected(i == curViewer);
	UpdateViewerPanel();

}

void Interface::Place3DViewer() {

	int sx = m_screenWidth - 205;

	// 3D Viewer ----------------------------------------------
	int fWidth = m_screenWidth - 215;
	int fHeight = m_screenHeight - 27;
	int Width2 = fWidth / 2 - 1;
	int Height2 = fHeight / 2 - 1;

	if (modeSolo) {
		for (int i = 0; i < MAX_VIEWER; i++)
			viewer[i]->SetVisible(FALSE);
		viewer[curViewer]->SetBounds(3, 3, fWidth, fHeight);
		viewer[curViewer]->SetVisible(TRUE);
	}
	else {
		for (int i = 0; i < MAX_VIEWER; i++)
			viewer[i]->SetVisible(TRUE);
		viewer[0]->SetBounds(3, 3, Width2, Height2);
		viewer[1]->SetBounds(6 + Width2, 3, Width2, Height2);
		viewer[2]->SetBounds(3, 6 + Height2, Width2, Height2);
		viewer[3]->SetBounds(6 + Width2, 6 + Height2, Width2, Height2);
	}
}

void Interface::UpdateViewers() {
	for (int i = 0; i < MAX_VIEWER; i++)
		viewer[i]->UpdateMatrix();
}

void Interface::SetFacetSearchPrg(BOOL visible, char *text) {
	static Uint32 lastUpd = 0;
	Uint32 now = SDL_GetTicks();
	if (!visible || (now - lastUpd > 500)) {
		for (int i = 0; i < MAX_VIEWER; i++) {
			viewer[i]->facetSearchState->SetVisible(visible);
			viewer[i]->facetSearchState->SetText(text);
		}
		GLWindowManager::Repaint();
		lastUpd = now;
	}
}

int Interface::OnExit() {
	SaveConfig(TRUE);
	remove(autosaveFilename.c_str());
	//empty TMP directory
	char tmp[1024];
	char CWD[MAX_PATH];
	_getcwd(CWD, MAX_PATH);
	sprintf(tmp, "del /Q \"%s\\tmp\\*.*\"", CWD);
	system(tmp);
	return GL_OK;
}



int Interface::OneTimeSceneInit_shared() {
	GLToolkit::SetIcon32x32("images/app_icon.png");

	for (int i = 0; i < MAX_VIEWER; i++) {
		viewer[i] = new GeometryViewer(i);
		Add(viewer[i]);
	}
	modeSolo = TRUE;
	//nbSt = 0;

	menu = new GLMenuBar(0);
	wnd->SetMenuBar(menu);
	menu->Add("File");
	menu->GetSubMenu("File")->Add("&Load", MENU_FILE_LOAD, SDLK_o, CTRL_MODIFIER);
	menu->GetSubMenu("File")->Add("Load recent");
	menu->GetSubMenu("File")->Add(NULL); //separator
	menu->GetSubMenu("File")->Add("&Insert geometry");
	menu->GetSubMenu("File")->GetSubMenu("Insert geometry")->Add("&To current structure", MENU_FILE_INSERTGEO);
	menu->GetSubMenu("File")->GetSubMenu("Insert geometry")->Add("&To new structure", MENU_FILE_INSERTGEO_NEWSTR);
	menu->GetSubMenu("File")->Add(NULL); //separator
	menu->GetSubMenu("File")->Add("&Save", MENU_FILE_SAVE, SDLK_s, CTRL_MODIFIER);
	menu->GetSubMenu("File")->Add("&Save as", MENU_FILE_SAVEAS);
	menu->GetSubMenu("File")->Add(NULL); //separator
	
	menu->GetSubMenu("File")->Add("Export selected facets", MENU_FILE_EXPORT_SELECTION);
	
	menu->GetSubMenu("File")->Add("Export selected profiles", MENU_FILE_EXPORTPROFILES);	

	menu->GetSubMenu("File")->SetIcon(MENU_FILE_SAVE, 83, 24);
	menu->GetSubMenu("File")->SetIcon(MENU_FILE_SAVEAS, 101, 24);
	menu->GetSubMenu("File")->SetIcon(MENU_FILE_LOAD, 65, 24);//65,24
	//menu->GetSubMenu("File")->SetIcon(MENU_FILE_LOADRECENT,83,24);//83,24

	menu->Add("Selection");
	menu->GetSubMenu("Selection")->Add("Smart Select facets...", MENU_SELECTION_SMARTSELECTION, SDLK_s, ALT_MODIFIER);
	menu->GetSubMenu("Selection")->Add(NULL); // Separator
	menu->GetSubMenu("Selection")->Add("Select All Facets", MENU_FACET_SELECTALL, SDLK_a, CTRL_MODIFIER);
	menu->GetSubMenu("Selection")->Add("Select by Facet Number...", MENU_SELECTION_SELECTFACETNUMBER, SDLK_n, ALT_MODIFIER);
	menu->GetSubMenu("Selection")->Add("Select Sticking", MENU_FACET_SELECTSTICK);
	menu->GetSubMenu("Selection")->Add("Select Transparent", MENU_FACET_SELECTTRANS);
	menu->GetSubMenu("Selection")->Add("Select 2 sided", MENU_FACET_SELECT2SIDE);
	menu->GetSubMenu("Selection")->Add("Select Texture", MENU_FACET_SELECTTEXT);
	menu->GetSubMenu("Selection")->Add("Select by Texture type...", MENU_SELECTION_TEXTURETYPE);
	menu->GetSubMenu("Selection")->Add("Select Profile", MENU_FACET_SELECTPROF);

	menu->GetSubMenu("Selection")->Add(NULL); // Separator
	menu->GetSubMenu("Selection")->Add("Select Abs > 0", MENU_FACET_SELECTABS);
	menu->GetSubMenu("Selection")->Add("Select Hit > 0", MENU_FACET_SELECTHITS);
	menu->GetSubMenu("Selection")->Add("Select large with no hits", MENU_FACET_SELECTNOHITS_AREA);
	menu->GetSubMenu("Selection")->Add(NULL); // Separator

	menu->GetSubMenu("Selection")->Add("Select link facets", MENU_FACET_SELECTDEST);
	menu->GetSubMenu("Selection")->Add("Select teleport facets", MENU_FACET_SELECTTELEPORT);
	menu->GetSubMenu("Selection")->Add("Select non planar facets", MENU_FACET_SELECTNONPLANAR);
	menu->GetSubMenu("Selection")->Add("Select non simple facets", MENU_FACET_SELECTERR);
	//menu->GetSubMenu("Selection")->Add(NULL); // Separator
	//menu->GetSubMenu("Selection")->Add("Load selection",MENU_FACET_LOADSEL);
	//menu->GetSubMenu("Selection")->Add("Save selection",MENU_FACET_SAVESEL);
	menu->GetSubMenu("Selection")->Add("Invert selection", MENU_FACET_INVERTSEL, SDLK_i, CTRL_MODIFIER);
	menu->GetSubMenu("Selection")->Add(NULL); // Separator 

	menu->GetSubMenu("Selection")->Add("Memorize selection to");
	memorizeSelectionsMenu = menu->GetSubMenu("Selection")->GetSubMenu("Memorize selection to");
	memorizeSelectionsMenu->Add("Add new...", MENU_SELECTION_ADDNEW, SDLK_w, CTRL_MODIFIER);
	memorizeSelectionsMenu->Add(NULL); // Separator

	menu->GetSubMenu("Selection")->Add("Select memorized");
	selectionsMenu = menu->GetSubMenu("Selection")->GetSubMenu("Select memorized");

	menu->GetSubMenu("Selection")->Add("Clear memorized", MENU_SELECTION_CLEARSELECTIONS);
	clearSelectionsMenu = menu->GetSubMenu("Selection")->GetSubMenu("Clear memorized");
	clearSelectionsMenu->Add("Clear All", MENU_SELECTION_CLEARALL);
	clearSelectionsMenu->Add(NULL); // Separator


	menu->Add("Tools");

	menu->GetSubMenu("Tools")->Add("Add formula ...", MENU_EDIT_ADDFORMULA);
	menu->GetSubMenu("Tools")->Add("Update formulas now!", MENU_EDIT_UPDATEFORMULAS, SDLK_f, ALT_MODIFIER);
	menu->GetSubMenu("Tools")->Add(NULL); // Separator
	menu->GetSubMenu("Tools")->Add("Texture Plotter ...", MENU_TOOLS_TEXPLOTTER, SDLK_t, ALT_MODIFIER);
	menu->GetSubMenu("Tools")->Add("Profile Plotter ...", MENU_TOOLS_PROFPLOTTER, SDLK_p, ALT_MODIFIER);
	menu->GetSubMenu("Tools")->Add(NULL); // Separator
	menu->GetSubMenu("Tools")->Add("Texture scaling...", MENU_EDIT_TSCALING, SDLK_d, CTRL_MODIFIER);
	menu->GetSubMenu("Tools")->Add("Global Settings ...", MENU_EDIT_GLOBALSETTINGS);

	menu->GetSubMenu("Tools")->SetIcon(MENU_EDIT_TSCALING, 137, 24);
	menu->GetSubMenu("Tools")->SetIcon(MENU_EDIT_ADDFORMULA, 155, 24);
	menu->GetSubMenu("Tools")->SetIcon(MENU_EDIT_GLOBALSETTINGS, 0, 77);

	menu->Add("Facet");
	menu->GetSubMenu("Facet")->Add("Collapse ...", MENU_FACET_COLLAPSE);
	menu->GetSubMenu("Facet")->Add("Swap normal", MENU_FACET_SWAPNORMAL, SDLK_n, CTRL_MODIFIER);
	menu->GetSubMenu("Facet")->Add("Shift vertex", MENU_FACET_SHIFTVERTEX, SDLK_h, CTRL_MODIFIER);
	menu->GetSubMenu("Facet")->Add("Edit coordinates ...", MENU_FACET_COORDINATES);
	menu->GetSubMenu("Facet")->Add("Move ...", MENU_FACET_MOVE);
	menu->GetSubMenu("Facet")->Add("Scale ...", MENU_FACET_SCALE);
	menu->GetSubMenu("Facet")->Add("Mirror / Project ...", MENU_FACET_MIRROR);
	menu->GetSubMenu("Facet")->Add("Rotate ...", MENU_FACET_ROTATE);
	menu->GetSubMenu("Facet")->Add("Align ...", MENU_FACET_ALIGN);
	menu->GetSubMenu("Facet")->Add("Extrude ...", MENU_FACET_EXTRUDE);
	menu->GetSubMenu("Facet")->Add("Split by plane ...", MENU_FACET_SPLIT);
	menu->GetSubMenu("Facet")->Add("Remove selected", MENU_FACET_REMOVESEL, SDLK_DELETE, CTRL_MODIFIER);
	menu->GetSubMenu("Facet")->Add("Explode selected", MENU_FACET_EXPLODE);
	menu->GetSubMenu("Facet")->Add("Create two facets' ...");
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->Add("Difference");
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->GetSubMenu("Difference")->Add("Auto (non-zero)", MENU_FACET_CREATE_DIFFERENCE_AUTO);
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->GetSubMenu("Difference")->Add("First - Second", MENU_FACET_CREATE_DIFFERENCE);
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->GetSubMenu("Difference")->Add("Second - First", MENU_FACET_CREATE_DIFFERENCE2);
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->Add("Union", MENU_FACET_CREATE_UNION);
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->Add("Intersection", MENU_FACET_CREATE_INTERSECTION);
	menu->GetSubMenu("Facet")->GetSubMenu("Create two facets' ...")->Add("XOR", MENU_FACET_CREATE_XOR);
	menu->GetSubMenu("Facet")->Add("Transition between 2", MENU_FACET_LOFT);
	menu->GetSubMenu("Facet")->Add("Build intersection...", MENU_FACET_INTERSECT);

	//menu->GetSubMenu("Facet")->Add("Facet Details ...", MENU_FACET_DETAILS);
	//menu->GetSubMenu("Facet")->Add("Facet Mesh ...",MENU_FACET_MESH);

	//facetMenu = menu->GetSubMenu("Facet");
	//facetMenu->SetEnabled(MENU_FACET_MESH,FALSE);

	menu->GetSubMenu("Facet")->SetIcon(MENU_FACET_COLLAPSE, 173, 24);
	menu->GetSubMenu("Facet")->SetIcon(MENU_FACET_SWAPNORMAL, 191, 24);
	menu->GetSubMenu("Facet")->SetIcon(MENU_FACET_SHIFTVERTEX, 90, 77);
	menu->GetSubMenu("Facet")->SetIcon(MENU_FACET_COORDINATES, 209, 24);
	menu->GetSubMenu("Facet")->SetIcon(MENU_TOOLS_PROFPLOTTER, 227, 24);
	menu->GetSubMenu("Facet")->SetIcon(MENU_FACET_DETAILS, 54, 77);
	//menu->GetSubMenu("Facet")->SetIcon(MENU_FACET_MESH,72,77);
	menu->GetSubMenu("Facet")->SetIcon(MENU_TOOLS_TEXPLOTTER, 108, 77);

	menu->Add("Vertex");
	menu->GetSubMenu("Vertex")->Add("Create Facet from Selected");
	menu->GetSubMenu("Vertex")->GetSubMenu("Create Facet from Selected")->Add("Convex Hull", MENU_VERTEX_CREATE_POLY_CONVEX, SDLK_v, ALT_MODIFIER);
	menu->GetSubMenu("Vertex")->GetSubMenu("Create Facet from Selected")->Add("Keep selection order", MENU_VERTEX_CREATE_POLY_ORDER);
	menu->GetSubMenu("Vertex")->Add("Clear isolated", MENU_VERTEX_CLEAR_ISOLATED);
	menu->GetSubMenu("Vertex")->Add("Remove selected", MENU_VERTEX_REMOVE);
	menu->GetSubMenu("Vertex")->Add("Vertex coordinates...", MENU_VERTEX_COORDINATES);
	menu->GetSubMenu("Vertex")->Add("Move...", MENU_VERTEX_MOVE);
	menu->GetSubMenu("Vertex")->Add("Scale...", MENU_VERTEX_SCALE);
	menu->GetSubMenu("Vertex")->Add("Mirror / Project ...", MENU_VERTEX_MIRROR);
	menu->GetSubMenu("Vertex")->Add("Rotate...", MENU_VERTEX_ROTATE);
	menu->GetSubMenu("Vertex")->Add("Add new...", MENU_VERTEX_ADD);
	menu->GetSubMenu("Vertex")->Add(NULL); // Separator
	menu->GetSubMenu("Vertex")->Add("Select all vertex", MENU_VERTEX_SELECTALL);
	menu->GetSubMenu("Vertex")->Add("Unselect all vertex", MENU_VERTEX_UNSELECTALL);
	menu->GetSubMenu("Vertex")->Add("Select coplanar vertex (visible on screen)", MENU_VERTEX_SELECT_COPLANAR);
	menu->GetSubMenu("Vertex")->Add("Select isolated vertex", MENU_VERTEX_SELECT_ISOLATED);

	menu->Add("View");

	menu->GetSubMenu("View")->Add("Structure", MENU_VIEW_STRUCTURE_P);
	structMenu = menu->GetSubMenu("View")->GetSubMenu("Structure");
	UpdateStructMenu();

	menu->GetSubMenu("View")->Add("Full Screen", MENU_VIEW_FULLSCREEN);

	menu->GetSubMenu("View")->Add(NULL); // Separator 

	menu->GetSubMenu("View")->Add("Memorize view to");
	memorizeViewsMenu = menu->GetSubMenu("View")->GetSubMenu("Memorize view to");
	memorizeViewsMenu->Add("Add new...", MENU_VIEW_ADDNEW, SDLK_q, CTRL_MODIFIER);
	memorizeViewsMenu->Add(NULL); // Separator

	menu->GetSubMenu("View")->Add("Select memorized");
	viewsMenu = menu->GetSubMenu("View")->GetSubMenu("Select memorized");

	menu->GetSubMenu("View")->Add("Clear memorized", MENU_VIEW_CLEARVIEWS);
	clearViewsMenu = menu->GetSubMenu("View")->GetSubMenu("Clear memorized");
	clearViewsMenu->Add("Clear All", MENU_VIEW_CLEARALL);

	//menu->GetSubMenu("View")->SetIcon(MENU_VIEW_STRUCTURE_P,0,77);
	menu->GetSubMenu("View")->SetIcon(MENU_VIEW_FULLSCREEN, 18, 77);
	//menu->GetSubMenu("View")->SetIcon(MENU_VIEW_ADD,36,77);

	menu->Add("Test");
	menu->GetSubMenu("Test")->Add("Pipe (L/R=0.0001)", MENU_TEST_PIPE0001);
	menu->GetSubMenu("Test")->Add("Pipe (L/R=1)", MENU_TEST_PIPE1);
	menu->GetSubMenu("Test")->Add("Pipe (L/R=10)", MENU_TEST_PIPE10);
	menu->GetSubMenu("Test")->Add("Pipe (L/R=100)", MENU_TEST_PIPE100);
	menu->GetSubMenu("Test")->Add("Pipe (L/R=1000)", MENU_TEST_PIPE1000);
	menu->GetSubMenu("Test")->Add("Pipe (L/R=10000)", MENU_TEST_PIPE10000);
	//Quick test pipe
	menu->GetSubMenu("Test")->Add(NULL);
	menu->GetSubMenu("Test")->Add("Quick Pipe", MENU_QUICKPIPE, SDLK_q, ALT_MODIFIER);

	geomNumber = new GLTextField(0, NULL);
	geomNumber->SetEditable(FALSE);
	Add(geomNumber);

	togglePanel = new GLTitledPanel("3D Viewer settings");
	togglePanel->SetClosable(TRUE);
	Add(togglePanel);

	showNormal = new GLToggle(0, "Normals");
	togglePanel->Add(showNormal);

	showRule = new GLToggle(0, "Rules");
	togglePanel->Add(showRule);

	showUV = new GLToggle(0, "\201,\202");
	togglePanel->Add(showUV);

	showLeak = new GLToggle(0, "Leaks");
	togglePanel->Add(showLeak);

	showHit = new GLToggle(0, "Hits");
	togglePanel->Add(showHit);

	showLine = new GLToggle(0, "Lines");
	togglePanel->Add(showLine);

	showVolume = new GLToggle(0, "Volume");
	togglePanel->Add(showVolume);

	showTexture = new GLToggle(0, "Texture");
	togglePanel->Add(showTexture);

	showIndex = new GLToggle(0, "Indices");
	togglePanel->Add(showIndex);

	showVertex = new GLToggle(0, "Vertices");
	togglePanel->Add(showVertex);

	simuPanel = new GLTitledPanel("Simulation");
	simuPanel->SetClosable(TRUE);
	Add(simuPanel);

	startSimu = new GLButton(0, "Start/Stop");
	simuPanel->Add(startSimu);

	resetSimu = new GLButton(0, "Reset");
	simuPanel->Add(resetSimu);

	autoFrameMoveToggle = new GLToggle(0, "Auto update scene");
	autoFrameMoveToggle->SetState(autoFrameMove);
	simuPanel->Add(autoFrameMoveToggle);

	forceFrameMoveButton = new GLButton(0, "Update");
	forceFrameMoveButton->SetEnabled(!autoFrameMove);
	simuPanel->Add(forceFrameMoveButton);

	hitLabel = new GLLabel("Hits");
	simuPanel->Add(hitLabel);

	hitNumber = new GLTextField(0, NULL);
	hitNumber->SetEditable(FALSE);
	simuPanel->Add(hitNumber);

	desLabel = new GLLabel("Des.");
	simuPanel->Add(desLabel);

	desNumber = new GLTextField(0, NULL);
	desNumber->SetEditable(FALSE);
	simuPanel->Add(desNumber);

	leakLabel = new GLLabel("Leaks");
	simuPanel->Add(leakLabel);

	leakNumber = new GLTextField(0, NULL);
	leakNumber->SetEditable(FALSE);
	simuPanel->Add(leakNumber);

	sTimeLabel = new GLLabel("Time");
	simuPanel->Add(sTimeLabel);

	sTime = new GLTextField(0, NULL);
	sTime->SetEditable(FALSE);
	simuPanel->Add(sTime);

	facetPanel = new GLTitledPanel("Selected Facet");
	facetPanel->SetClosable(TRUE);
	Add(facetPanel);

	facetSideLabel = new GLLabel("Sides:");
	facetPanel->Add(facetSideLabel);

	facetSideType = new GLCombo(0);
	facetSideType->SetSize(2);
	facetSideType->SetValueAt(0, "1 Sided");
	facetSideType->SetValueAt(1, "2 Sided");
	facetPanel->Add(facetSideType);

	facetTLabel = new GLLabel("Opacity:");
	facetPanel->Add(facetTLabel);
	facetOpacity = new GLTextField(0, NULL);
	facetPanel->Add(facetOpacity);

	facetAreaLabel = new GLLabel("Area (cm\262):");
	facetPanel->Add(facetAreaLabel);
	facetArea = new GLTextField(0, NULL);
	facetPanel->Add(facetArea);

	facetDetailsBtn = new GLButton(0, "Details...");
	facetPanel->Add(facetDetailsBtn);

	facetCoordBtn = new GLButton(0, "Coord...");
	facetPanel->Add(facetCoordBtn);

	facetApplyBtn = new GLButton(0, "Apply");
	facetApplyBtn->SetEnabled(FALSE);
	facetPanel->Add(facetApplyBtn);

	return GL_OK;
}



int Interface::RestoreDeviceObjects_shared() {
	Geometry *geom = worker.GetGeometry();
	geom->RestoreDeviceObjects();
	//worker.Update(0.0f);

	// Restore dialog which are not displayed
	// Those which are displayed are invalidated by the window manager
	RVALIDATE_DLG(formulaSettings);
	RVALIDATE_DLG(collapseSettings);
	RVALIDATE_DLG(moveVertex);
	RVALIDATE_DLG(scaleVertex);
	RVALIDATE_DLG(scaleFacet);
	RVALIDATE_DLG(selectDialog);
	RVALIDATE_DLG(selectTextureType);
	RVALIDATE_DLG(moveFacet);
	RVALIDATE_DLG(extrudeFacet);
	RVALIDATE_DLG(mirrorFacet);
	RVALIDATE_DLG(mirrorVertex);
	RVALIDATE_DLG(splitFacet);
	RVALIDATE_DLG(buildIntersection);
	RVALIDATE_DLG(rotateFacet);
	RVALIDATE_DLG(rotateVertex);
	RVALIDATE_DLG(alignFacet);
	RVALIDATE_DLG(addVertex);
	RVALIDATE_DLG(loadStatus);
	RVALIDATE_DLG(facetCoordinates);
	RVALIDATE_DLG(vertexCoordinates);

	UpdateTitle();

	return GL_OK;
}

int Interface::InvalidateDeviceObjects_shared() {
	Geometry *geom = worker.GetGeometry();
	geom->InvalidateDeviceObjects();
	//worker.Update(0.0f);

	// Restore dialog which are not displayed
	// Those which are displayed are invalidated by the window manager
	IVALIDATE_DLG(formulaSettings);
	IVALIDATE_DLG(collapseSettings);
	IVALIDATE_DLG(moveVertex);
	IVALIDATE_DLG(scaleVertex);
	IVALIDATE_DLG(scaleFacet);
	IVALIDATE_DLG(selectDialog);
	IVALIDATE_DLG(selectTextureType);
	IVALIDATE_DLG(moveFacet);
	IVALIDATE_DLG(extrudeFacet);
	IVALIDATE_DLG(mirrorFacet);
	IVALIDATE_DLG(mirrorVertex);
	IVALIDATE_DLG(splitFacet);
	IVALIDATE_DLG(buildIntersection);
	IVALIDATE_DLG(rotateFacet);
	IVALIDATE_DLG(rotateFacet);
	IVALIDATE_DLG(alignFacet);
	IVALIDATE_DLG(addVertex);
	IVALIDATE_DLG(loadStatus);
	IVALIDATE_DLG(facetCoordinates);
	IVALIDATE_DLG(vertexCoordinates);

	UpdateTitle();

	return GL_OK;
}

BOOL Interface::ProcessMessage_shared(GLComponent *src, int message) {
	Geometry *geom = worker.GetGeometry();
	char *input;
	char tmp[128];

	switch (message) {

	//MENU --------------------------------------------------------------------
	case MSG_MENU:
		switch (src->GetId()) {
		case MENU_FILE_LOAD:
			if (AskToSave()) {
				if (worker.running) worker.Stop_Public();
				LoadFile();
			}
			return TRUE;
		case MENU_FILE_INSERTGEO:
			if (geom->IsLoaded()) {
				if (worker.running) worker.Stop_Public();
				InsertGeometry(FALSE);
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_FILE_INSERTGEO_NEWSTR:
			if (geom->IsLoaded()) {
				if (worker.running) worker.Stop_Public();
				InsertGeometry(TRUE);
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_FILE_SAVEAS:
			if (geom->IsLoaded()) {
				SaveFileAs();
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_FILE_EXPORT_SELECTION:
			ExportSelection();
			return TRUE;
		case MENU_FILE_SAVE:
			if (geom->IsLoaded()) SaveFile();
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_FILE_EXIT:
			if (AskToSave()) Exit();
			return TRUE;

		case MENU_EDIT_ADDFORMULA:
			if (!formulaSettings) formulaSettings = new FormulaSettings();
			formulaSettings->Update(NULL, -1);
			formulaSettings->SetVisible(TRUE);
			return TRUE;
		case MENU_EDIT_UPDATEFORMULAS:
			UpdateFormula();
			return TRUE;

		case MENU_FACET_COLLAPSE:
			if (geom->IsLoaded()) {
				DisplayCollapseDialog();
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_FACET_SWAPNORMAL:
			if (AskToReset()) {
				geom->SwapNormal();
				// Send to sub process
				try { worker.Reload(); }
				catch (Error &e) {
					GLMessageBox::Display((char *)e.GetMsg(), "Error", GLDLG_OK, GLDLG_ICONERROR);
				}
			}
			return TRUE;
		case MENU_FACET_EXTRUDE:
			if (!extrudeFacet || !extrudeFacet->IsVisible()) {
				SAFE_DELETE(extrudeFacet);
				extrudeFacet = new ExtrudeFacet(geom, &worker);
			}
			extrudeFacet->SetVisible(TRUE);
			return TRUE;

		case MENU_FACET_SHIFTVERTEX:
			if (AskToReset()) {
				geom->ShiftVertex();
				// Send to sub process
				worker.Reload();
			}
			return TRUE;
		case MENU_FACET_COORDINATES:

			if (!facetCoordinates) facetCoordinates = new FacetCoordinates();
			facetCoordinates->Display(&worker);
			return TRUE;
		case MENU_FACET_MOVE:
			if (!moveFacet || !moveFacet->IsVisible()) {
				SAFE_DELETE(moveFacet);
				moveFacet = new MoveFacet(geom, &worker);
			}
			moveFacet->SetVisible(TRUE);
			return TRUE;
		case MENU_FACET_SCALE:
			if (geom->IsLoaded()) {
				if (!scaleFacet) scaleFacet = new ScaleFacet(geom, &worker);

				scaleFacet->SetVisible(TRUE);

			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_FACET_MIRROR:
			if (!mirrorFacet) mirrorFacet = new MirrorFacet(geom, &worker);
			mirrorFacet->SetVisible(TRUE);
			return TRUE;
		case MENU_FACET_SPLIT:
			if (!splitFacet || !splitFacet->IsVisible()) {
				SAFE_DELETE(splitFacet);
				splitFacet = new SplitFacet(geom, &worker);
				splitFacet->SetVisible(TRUE);
			}
			return TRUE;
		case MENU_FACET_ROTATE:
			if (!rotateFacet) rotateFacet = new RotateFacet(geom, &worker);
			rotateFacet->SetVisible(TRUE);
			return TRUE;
		case MENU_FACET_ALIGN:
			if (!alignFacet) alignFacet = new AlignFacet(geom, &worker);
			alignFacet->MemorizeSelection();
			alignFacet->SetVisible(TRUE);
			return TRUE;

		case MENU_FACET_EXPLODE:
			if (GLMessageBox::Display("Explode selected facet?", "Question", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO) == GLDLG_OK) {
				if (AskToReset()) {
					int err;
					try {
						err = geom->ExplodeSelected();
					}
					catch (Error &e) {
						GLMessageBox::Display((char *)e.GetMsg(), "Error exploding", GLDLG_OK, GLDLG_ICONERROR);
					}
					if (err == -1) {
						GLMessageBox::Display("Empty selection", "Error", GLDLG_OK, GLDLG_ICONERROR);
					}
					else if (err == -2) {
						GLMessageBox::Display("All selected facets must have a mesh with boudary correction enabled", "Error", GLDLG_OK, GLDLG_ICONERROR);
					}
					else if (err == 0) {

						UpdateModelParams();
						UpdateFacetParams(TRUE);
						// Send to sub process
						try { worker.Reload(); }
						catch (Error &e) {
							GLMessageBox::Display((char *)e.GetMsg(), "Error reloading worker", GLDLG_OK, GLDLG_ICONERROR);
						}
					}
				}
			}
			return TRUE;

		case MENU_SELECTION_SMARTSELECTION:
			if (!smartSelection) smartSelection = new SmartSelection(worker.GetGeometry(), &worker);
			smartSelection->SetVisible(TRUE);
			return TRUE;
		case MENU_FACET_SELECTALL:
			geom->SelectAll();
			UpdateFacetParams(TRUE);
			return TRUE;
		case MENU_FACET_SELECTTRANS:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)
				if (geom->GetFacet(i)->sh.opacity != 1.0 && geom->GetFacet(i)->sh.opacity != 2.0)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;
		case MENU_FACET_SELECT2SIDE:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)
				if (geom->GetFacet(i)->sh.is2sided)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;
		case MENU_FACET_SELECTTEXT:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)
				if (geom->GetFacet(i)->sh.isTextured != NULL)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;
		case MENU_FACET_SELECTPROF:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)
				if (geom->GetFacet(i)->sh.isProfile != NULL)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;

		case MENU_FACET_SELECTNONPLANAR:

			sprintf(tmp, "%g", planarityThreshold);
			//sprintf(title,"Pipe L/R = %g",L/R);
			input = GLInputBox::GetInput(tmp, "Planarity larger than:", "Select non planar facets");
			if (!input) return TRUE;
			if (!sscanf(input, "%lf", &planarityThreshold)) {
				GLMessageBox::Display("Invalid number", "Error", GLDLG_OK, GLDLG_ICONERROR);
				return TRUE;
			}
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)
				if (geom->GetFacet(i)->err >= planarityThreshold)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;


		case MENU_FACET_SELECTERR:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)

				if (geom->GetFacet(i)->sh.sign == 0.0)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;

		case MENU_FACET_SELECTDEST:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)

				if (geom->GetFacet(i)->sh.superDest != 0)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;

		case MENU_FACET_SELECTTELEPORT:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)

				if (geom->GetFacet(i)->sh.teleportDest != 0)
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;

		case MENU_FACET_SELECTABS:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++) {
#ifdef MOLFLOW
				if (geom->GetFacet(i)->counterCache.hit.nbAbsorbed > 0)
#endif
#ifdef SYNRAD
					if (geom->GetFacet(i)->counterCache.nbAbsorbed > 0)
#endif
					geom->Select(i);
			}
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;

		case MENU_FACET_SELECTHITS:
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)

#ifdef MOLFLOW
				if (geom->GetFacet(i)->counterCache.hit.nbHit > 0)
#endif
#ifdef SYNRAD
					if (geom->GetFacet(i)->counterCache.nbHit > 0)
#endif
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;

		case MENU_FACET_SELECTNOHITS_AREA:

			sprintf(tmp, "%g", largeArea);
			//sprintf(title,"Pipe L/R = %g",L/R);
			input = GLInputBox::GetInput(tmp, "Min.area (cm\262)", "Select large facets without hits");
			if (!input) return TRUE;
			if ((sscanf(input, "%lf", &largeArea) <= 0) || (largeArea <= 0.0)) {
				GLMessageBox::Display("Invalid number", "Error", GLDLG_OK, GLDLG_ICONERROR);
				return TRUE;
			}
			geom->UnselectAll();
			for (int i = 0; i < geom->GetNbFacet(); i++)
#ifdef MOLFLOW
				if (geom->GetFacet(i)->counterCache.hit.nbHit == 0 && geom->GetFacet(i)->sh.area >= largeArea)
#endif
#ifdef SYNRAD
				if (geom->GetFacet(i)->counterCache.nbHit == 0 && geom->GetFacet(i)->sh.area >= largeArea)
#endif
					geom->Select(i);
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;
		case MENU_FACET_INVERTSEL:
			for (int i = 0; i < geom->GetNbFacet(); i++)
				geom->GetFacet(i)->selected = !geom->GetFacet(i)->selected;
			geom->UpdateSelection();
			UpdateFacetParams(TRUE);
			return TRUE;
		case MENU_SELECTION_SELECTFACETNUMBER:
			if (!selectDialog) selectDialog = new SelectDialog(&worker);
			selectDialog->SetVisible(TRUE);
			return TRUE;
		case MENU_SELECTION_TEXTURETYPE:
			if (!selectTextureType) selectTextureType = new SelectTextureType(&worker);
			selectTextureType->SetVisible(TRUE);
			return TRUE;
		case MENU_FACET_SAVESEL:
			SaveSelection();
			return TRUE;
		case MENU_FACET_LOADSEL:
			LoadSelection();
			return TRUE;
		case MENU_SELECTION_ADDNEW:
			AddSelection();
			return TRUE;
		case  MENU_SELECTION_CLEARALL:
			if (GLMessageBox::Display("Clear all selections ?", "Question", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO) == GLDLG_OK) {
				ClearAllSelections();
			}
			return TRUE;
		case MENU_VERTEX_UNSELECTALL:
			geom->UnselectAllVertex();
			return TRUE;
		case MENU_VERTEX_SELECTALL:
			geom->SelectAllVertex();
			return TRUE;
		case MENU_VERTEX_SELECT_ISOLATED:
			geom->SelectIsolatedVertices();
			return TRUE;
		case MENU_VERTEX_CLEAR_ISOLATED:
			geom->DeleteIsolatedVertices(FALSE);
			UpdateModelParams();
			if (facetCoordinates) facetCoordinates->UpdateFromSelection();
			if (vertexCoordinates) vertexCoordinates->Update();
			return TRUE;
		case MENU_VERTEX_CREATE_POLY_CONVEX:
			if (AskToReset()) {
				try {
					geom->CreatePolyFromVertices_Convex();
				}
				catch (Error &e) {
					GLMessageBox::Display((char *)e.GetMsg(), "Error creating polygon", GLDLG_OK, GLDLG_ICONERROR);
				}
				//UpdateModelParams();
				try {
					worker.Reload();
				}
				catch (Error &e) {
					GLMessageBox::Display((char *)e.GetMsg(), "Error reloading worker", GLDLG_OK, GLDLG_ICONERROR);
				}
			}
			return TRUE;
		case MENU_VERTEX_CREATE_POLY_ORDER:
			if (AskToReset()) {
				try {
					geom->CreatePolyFromVertices_Order();
				}
				catch (Error &e) {
					GLMessageBox::Display((char *)e.GetMsg(), "Error creating polygon", GLDLG_OK, GLDLG_ICONERROR);
				}
				//UpdateModelParams();
				try {
					worker.Reload();
				}
				catch (Error &e) {

					GLMessageBox::Display((char *)e.GetMsg(), "Error reloading worker", GLDLG_OK, GLDLG_ICONERROR);
				}
			}
			return TRUE;
		case MENU_FACET_CREATE_DIFFERENCE:
			CreateOfTwoFacets(ClipperLib::ctDifference, 0);
			return TRUE;
		case MENU_FACET_CREATE_DIFFERENCE2:
			CreateOfTwoFacets(ClipperLib::ctDifference, 1);
			return TRUE;
		case MENU_FACET_CREATE_DIFFERENCE_AUTO:
			CreateOfTwoFacets(ClipperLib::ctDifference, 2);
			return TRUE;
		case MENU_FACET_CREATE_UNION:
			CreateOfTwoFacets(ClipperLib::ctUnion);
			return TRUE;
		case MENU_FACET_CREATE_INTERSECTION:
			CreateOfTwoFacets(ClipperLib::ctIntersection);
			return TRUE;
		case MENU_FACET_CREATE_XOR:
			CreateOfTwoFacets(ClipperLib::ctXor);
			return TRUE;
		case MENU_FACET_LOFT:
			if (geom->GetNbSelected() != 2) {
				GLMessageBox::Display("Select exactly 2 facets", "Can't create loft", GLDLG_OK, GLDLG_ICONERROR);
				return TRUE;
			}
			if (AskToReset()) {
				geom->CreateLoft();
			}
			worker.Reload();
			UpdateModelParams();
			UpdateFacetlistSelected();
			UpdateViewers();
			return TRUE;
		case MENU_FACET_INTERSECT:
			if (!buildIntersection || !buildIntersection->IsVisible()) {
				SAFE_DELETE(buildIntersection);
				buildIntersection = new BuildIntersection(geom, &worker);
				buildIntersection->SetVisible(TRUE);
			}
			return TRUE;

		case MENU_VERTEX_SELECT_COPLANAR:
			char *input;
			if (geom->IsLoaded()) {
				if (geom->GetNbSelectedVertex() != 3) {
					GLMessageBox::Display("Select exactly 3 vertices", "Can't define plane", GLDLG_OK, GLDLG_ICONERROR);
					return TRUE;
				}
				sprintf(tmp, "%g", tolerance);
				//sprintf(title,"Pipe L/R = %g",L/R);
				input = GLInputBox::GetInput(tmp, "Tolerance (cm)", "Select coplanar vertices");
				if (!input) return TRUE;
				if ((sscanf(input, "%lf", &tolerance) <= 0) || (tolerance <= 0.0)) {
					GLMessageBox::Display("Invalid number", "Error", GLDLG_OK, GLDLG_ICONERROR);
					return TRUE;
				}
				try { viewer[curViewer]->SelectCoplanar(tolerance); }
				catch (Error &e) {
					GLMessageBox::Display((char *)e.GetMsg(), "Error selecting coplanar vertices", GLDLG_OK, GLDLG_ICONERROR);
				}
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_VERTEX_MOVE:
			if (geom->IsLoaded()) {
				if (!moveVertex) moveVertex = new MoveVertex(geom, &worker);

				//moveVertex->DoModal();
				moveVertex->SetVisible(TRUE);

				/*
				UpdateModelParams();
				try { worker.Reload(); } catch(Error &e) {
				GLMessageBox::Display((char *)e.GetMsg(),"Error reloading worker",GLDLG_OK,GLDLG_ICONERROR);
				*/

			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_VERTEX_SCALE:
			if (geom->IsLoaded()) {
				if (!scaleVertex) scaleVertex = new ScaleVertex(geom, &worker);
				scaleVertex->SetVisible(TRUE);
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;
		case MENU_VERTEX_MIRROR:
			if (!mirrorVertex) mirrorVertex = new MirrorVertex(geom, &worker);
			mirrorVertex->SetVisible(TRUE);
			return TRUE;
		case MENU_VERTEX_ROTATE:
			if (!rotateVertex) rotateVertex = new RotateVertex(geom, &worker);
			rotateVertex->SetVisible(TRUE);
			return TRUE;
		
		case MENU_VERTEX_COORDINATES:

			if (!vertexCoordinates) vertexCoordinates = new VertexCoordinates();
			vertexCoordinates->Display(&worker);
			return TRUE;

		case MENU_VERTEX_ADD:
			if (geom->IsLoaded()) {
				if (!addVertex) addVertex = new AddVertex(geom, &worker);
				addVertex->SetVisible(TRUE);
			}
			else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
			return TRUE;

		case MENU_VIEW_FULLSCREEN:
			if (m_bWindowed) {
				ToggleFullscreen();
				PlaceComponents();
			}
			else {
				Resize(1024, 800, TRUE);
			}
			menu->GetSubMenu("View")->SetCheck(MENU_VIEW_FULLSCREEN, !m_bWindowed);
			return TRUE;

		case MENU_VIEW_ADDNEW:
			AddView();
			return TRUE;
			return TRUE;
		case  MENU_VIEW_CLEARALL:
			if (GLMessageBox::Display("Clear all views ?", "Question", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO) == GLDLG_OK) {
				ClearAllViews();
			}
			return TRUE;
		case MENU_TEST_PIPE0001:
			if (AskToSave()) BuildPipe(0.0001);
			return TRUE;
		case MENU_TEST_PIPE1:
			if (AskToSave()) BuildPipe(1.0);
			return TRUE;
		case MENU_TEST_PIPE10:
			if (AskToSave()) BuildPipe(10.0);
			return TRUE;
		case MENU_TEST_PIPE100:
			if (AskToSave()) BuildPipe(100.0);
			return TRUE;
		case MENU_TEST_PIPE1000:
			if (AskToSave()) BuildPipe(1000.0);
			return TRUE;
		case MENU_TEST_PIPE10000:
			if (AskToSave()) BuildPipe(10000.0);
			return TRUE;
		case MENU_QUICKPIPE:
			if (AskToSave()) BuildPipe(5.0,5);
			return TRUE;
		}
		// Load recent menu
		if (src->GetId() >= MENU_FILE_LOADRECENT && src->GetId() < MENU_FILE_LOADRECENT + nbRecent) {
			if (AskToSave()) {
				if (worker.running) worker.Stop_Public();
				LoadFile(recents[src->GetId() - MENU_FILE_LOADRECENT]);
			}
			return TRUE;
		}

		// Show structure menu
		else if (src->GetId() >= MENU_VIEW_STRUCTURE && src->GetId() <= MENU_VIEW_STRUCTURE + geom->GetNbStructure()) {
			geom->viewStruct = src->GetId() - MENU_VIEW_STRUCTURE - 1;
			if (src->GetId() > MENU_VIEW_STRUCTURE) geom->UnselectAll();
			UpdateStructMenu();
			return TRUE;
		}
		else if (src->GetId() == MENU_VIEW_NEWSTRUCT) {
			AddStruct();
			UpdateStructMenu();
			return TRUE;
		}
		else if (src->GetId() == MENU_VIEW_DELSTRUCT) {
			DeleteStruct();
			UpdateStructMenu();

			#ifdef MOLFLOW
				worker.CalcTotalOutgassing();
			#endif

			return TRUE;
		}
		else if (src->GetId() == MENU_VIEW_PREVSTRUCT) {
			geom->viewStruct = IDX(geom->viewStruct - 1, geom->GetNbStructure());
			geom->UnselectAll();
			UpdateStructMenu();
			return TRUE;
		}
		else if (src->GetId() == MENU_VIEW_NEXTSTRUCT) {
			geom->viewStruct = IDX(geom->viewStruct + 1, geom->GetNbStructure());
			geom->UnselectAll();
			UpdateStructMenu();
			return TRUE;
		}

		// Select selection
		else if (MENU_SELECTION_SELECTIONS + nbSelection > src->GetId() && src->GetId() >= MENU_SELECTION_SELECTIONS) { //Choose selection by number
			SelectSelection(src->GetId() - MENU_SELECTION_SELECTIONS);
			return TRUE;
		}
		else if (src->GetId() == (MENU_SELECTION_SELECTIONS + nbSelection)) { //Previous selection
			SelectSelection(IDX(idSelection - 1, nbSelection));
			return TRUE;
		}
		else if (src->GetId() == (MENU_SELECTION_SELECTIONS + nbSelection + 1)) { //Next selection
			SelectSelection(IDX(idSelection + 1, nbSelection));
			return TRUE;
		}

		// Clear selection
		else if (src->GetId() >= MENU_SELECTION_CLEARSELECTIONS && src->GetId() < MENU_SELECTION_CLEARSELECTIONS + nbSelection) {
			char tmpname[256];
			sprintf(tmpname, "Clear %s?", selections[src->GetId() - MENU_SELECTION_CLEARSELECTIONS].name);
			if (GLMessageBox::Display(tmpname, "Confirmation", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO) == GLDLG_OK) {
				ClearSelection(src->GetId() - MENU_SELECTION_CLEARSELECTIONS);
			}
			return TRUE;
		}

		// Memorize selection
		else if (src->GetId() >= MENU_SELECTION_MEMORIZESELECTIONS && src->GetId() < MENU_SELECTION_MEMORIZESELECTIONS + nbSelection) {
			OverWriteSelection(src->GetId() - MENU_SELECTION_MEMORIZESELECTIONS);
			return TRUE;
		}

		// Select view
		else if (src->GetId() >= MENU_VIEW_VIEWS && src->GetId() < MENU_VIEW_VIEWS + nbView) {
			SelectView(src->GetId() - MENU_VIEW_VIEWS);
			return TRUE;
		}
		// Clear view
		else if (src->GetId() >= MENU_VIEW_CLEARVIEWS && src->GetId() < MENU_VIEW_CLEARVIEWS + nbView) {
			char tmpname[256];
			sprintf(tmpname, "Clear %s?", views[src->GetId() - MENU_VIEW_CLEARVIEWS].name);
			if (GLMessageBox::Display(tmpname, "Confirmation", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO) == GLDLG_OK) {
				ClearView(src->GetId() - MENU_VIEW_CLEARVIEWS);
			}
			return TRUE;
		}
		// Memorize view
		else if (src->GetId() >= MENU_VIEW_MEMORIZEVIEWS && src->GetId() < MENU_VIEW_MEMORIZEVIEWS + nbView) {
			OverWriteView(src->GetId() - MENU_VIEW_MEMORIZEVIEWS);
			return TRUE;
		}
		break;

		//LIST --------------------------------------------------------------------
	case MSG_LIST:
		if (src == facetList && geom->IsLoaded()) {
			int *sels = (int *)malloc((geom->GetNbFacet()) * sizeof(int));
			int nbSel;
			facetList->GetSelectedRows(&sels, &nbSel, TRUE);
			geom->UnselectAll();
			for (int i = 0; i < nbSel; i++)
				geom->Select(sels[i]);
			geom->UpdateSelection();
			UpdateFacetParams();
			SAFE_FREE(sels);
			return TRUE;
		}
		break;

		//GEOMVIEWER ------------------------------------------------------------------
	case MSG_GEOMVIEWER_MAXIMISE:
	{
		if (src == viewer[0]) {
			AnimateViewerChange(0);
		}
		else if (src == viewer[1]) {
			AnimateViewerChange(1);
		}
		else if (src == viewer[2]) {
			AnimateViewerChange(2);
		}
		else if (src == viewer[3]) {
			AnimateViewerChange(3);
		}
		Place3DViewer();

		BOOL neededTexture = needsTexture;

		BOOL neededMesh = needsMesh;
		CheckNeedsTexture();

		if (!needsTexture && neededTexture) { //We just disabled textures
			worker.GetGeometry()->ClearFacetTextures();
		}
		else if (needsTexture && !neededTexture) { //We just enabled textures
			worker.RebuildTextures();
		}

		if (!needsMesh && neededMesh) { //We just disabled mesh
			geom->ClearFacetMeshLists();
		}
		else if (needsMesh && !neededMesh) { //We just enabled mesh
			geom->BuildFacetMeshLists();
		}

		return TRUE;
	}
	case MSG_GEOMVIEWER_SELECT: {
		SelectViewer(src->GetId());
		return TRUE;
	}

		//BUTTON ------------------------------------------------------------------
	case MSG_BUTTON:
		if (src == resetSimu) {
			changedSinceSave = TRUE;
			ResetSimulation();
			return TRUE;
		}
		else if (src == forceFrameMoveButton) {
			updateRequested = TRUE;
			FrameMove();
			return TRUE;
		}
		break;

		//Panel open/close ---------------------------------------------------------
	case MSG_PANELR:
		PlaceComponents();
		return TRUE;
	}
	return FALSE;
}

void Interface::CheckNeedsTexture()
{
	needsMesh = needsTexture = needsDirection = FALSE;
	for (int i = 0;i < MAX_VIEWER;i++) {
		needsMesh = needsMesh || (viewer[i]->IsVisible() && viewer[i]->showMesh);
		needsTexture = needsTexture || (viewer[i]->IsVisible() && viewer[i]->showTexture);
		needsDirection = needsDirection || (viewer[i]->IsVisible() && viewer[i]->showDir);
	}
}

//SELECTIONS
//-----------------------------------------------------------------------------

void Interface::SelectView(int v) {
	viewer[curViewer]->SetCurrentView(views[v]);
}

//-----------------------------------------------------------------------------

void Interface::SelectSelection(int v) {
	Geometry *geom = worker.GetGeometry();
	geom->SetSelection((&selections[v].selection), &(selections[v].nbSel), viewer[0]->GetWindow()->IsShiftDown(), viewer[0]->GetWindow()->IsCtrlDown());
	idSelection = v;
}

//-----------------------------------------------------------------------------
void Interface::ClearSelectionMenus() {
	memorizeSelectionsMenu->Clear();
	memorizeSelectionsMenu->Add("Add new...", MENU_SELECTION_ADDNEW, SDLK_w, CTRL_MODIFIER);
	memorizeSelectionsMenu->Add(NULL); // Separator
	clearSelectionsMenu->Clear();
	clearSelectionsMenu->Add("Clear All", MENU_SELECTION_CLEARALL);
	clearSelectionsMenu->Add(NULL); // Separator
	selectionsMenu->Clear();
}

void Interface::RebuildSelectionMenus() {
	ClearSelectionMenus();
	int i;
	for (i = 0; i < nbSelection; i++) {
		if (i <= 8) {
			selectionsMenu->Add(selections[i].name, MENU_SELECTION_SELECTIONS + i, SDLK_1 + i, ALT_MODIFIER);
		}
		else {
			selectionsMenu->Add(selections[i].name, MENU_SELECTION_SELECTIONS + i); //no place for ALT+shortcut
		}
		clearSelectionsMenu->Add(selections[i].name, MENU_SELECTION_CLEARSELECTIONS + i);
		memorizeSelectionsMenu->Add(selections[i].name, MENU_SELECTION_MEMORIZESELECTIONS + i);
	}
	selectionsMenu->Add(NULL); //Separator
	selectionsMenu->Add("Select previous", MENU_SELECTION_SELECTIONS + i, SDLK_F11, ALT_MODIFIER);
	selectionsMenu->Add("Select next", MENU_SELECTION_SELECTIONS + i + 1, SDLK_F12, ALT_MODIFIER);
}

void Interface::AddSelection(char *selectionName, ASELECTION s) {

	if (nbSelection < MAX_SELECTION) {
		selections[nbSelection] = s;
		selections[nbSelection].name = _strdup(selectionName);
		nbSelection++;
	}
	else {
		SAFE_FREE(selections[0].name);
		for (int i = 0; i < MAX_SELECTION - 1; i++) selections[i] = selections[i + 1];
		selections[MAX_SELECTION - 1] = s;
		selections[MAX_SELECTION - 1].name = _strdup(selectionName);
	}
	RebuildSelectionMenus();
}

void Interface::ClearSelection(int idClr) {
	SAFE_FREE(selections[idClr].name);
	for (int i = idClr; i < nbSelection - 1; i++) selections[i] = selections[i + 1];
	nbSelection--;
	RebuildSelectionMenus();
}

void Interface::ClearAllSelections() {
	for (int i = 0; i < nbSelection; i++) SAFE_FREE(selections[i].name);
	nbSelection = 0;
	ClearSelectionMenus();
}

void Interface::OverWriteSelection(int idOvr) {
	Geometry *geom = worker.GetGeometry();
	char *selectionName = GLInputBox::GetInput(selections[idOvr].name, "Selection name", "Enter selection name");
	if (!selectionName) return;

	geom->GetSelection(&(selections[idOvr].selection), &(selections[idOvr].nbSel));
	selections[idOvr].name = _strdup(selectionName);
	RebuildSelectionMenus();
}

void Interface::AddSelection() {
	Geometry *geom = worker.GetGeometry();
	char tmp[32];
	sprintf(tmp, "Selection #%d", nbSelection + 1);
	char *selectionName = GLInputBox::GetInput(tmp, "Selection name", "Enter selection name");
	if (!selectionName) return;

	if (nbSelection < MAX_SELECTION) {
		geom->GetSelection(&(selections[nbSelection].selection), &(selections[nbSelection].nbSel));
		selections[nbSelection].name = _strdup(selectionName);
		nbSelection++;
	}
	else {
		SAFE_FREE(selections[0].name);
		for (int i = 0; i < MAX_SELECTION - 1; i++) selections[i] = selections[i + 1];
		geom->GetSelection(&(selections[MAX_SELECTION - 1].selection), &(selections[MAX_SELECTION - 1].nbSel));
		selections[MAX_SELECTION - 1].name = _strdup(selectionName);
	}
	RebuildSelectionMenus();
}

//VIEWS
//-----------------------------------------------------------------------------
void Interface::ClearViewMenus() {
	memorizeViewsMenu->Clear();
	memorizeViewsMenu->Add("Add new...", MENU_VIEW_ADDNEW, SDLK_q, CTRL_MODIFIER);
	memorizeViewsMenu->Add(NULL); // Separator
	clearViewsMenu->Clear();
	clearViewsMenu->Add("Clear All", MENU_VIEW_CLEARALL);
	clearViewsMenu->Add(NULL); // Separator
	viewsMenu->Clear();
}

void Interface::RebuildViewMenus() {
	ClearViewMenus();
	for (int i = 0; i < nbView; i++) {
		int id = i;
		if (nbView >= 10) id = i - nbView + 8;
		if (id >= 0 && id <= 8) {
			viewsMenu->Add(views[i].name, MENU_VIEW_VIEWS + i, SDLK_F1 + id, ALT_MODIFIER);
		}
		else {
			viewsMenu->Add(views[i].name, MENU_VIEW_VIEWS + i);
		}
		clearViewsMenu->Add(views[i].name, MENU_VIEW_CLEARVIEWS + i);
		memorizeViewsMenu->Add(views[i].name, MENU_VIEW_MEMORIZEVIEWS + i);
	}
}


void Interface::AddView(char *viewName, AVIEW v) {

	if (nbView < MAX_VIEW) {
		views[nbView] = v;
		views[nbView].name = _strdup(viewName);
		nbView++;
	}
	else {
		SAFE_FREE(views[0].name);
		for (int i = 0; i < MAX_VIEW - 1; i++) views[i] = views[i + 1];
		views[MAX_VIEW - 1] = v;
		views[MAX_VIEW - 1].name = _strdup(viewName);
	}
	RebuildViewMenus();
}

void Interface::ClearView(int idClr) {
	SAFE_FREE(views[idClr].name);
	for (int i = idClr; i < nbView - 1; i++) views[i] = views[i + 1];
	nbView--;
	RebuildViewMenus();
}

void Interface::ClearAllViews() {
	for (int i = 0; i < nbView; i++) SAFE_FREE(views[i].name);
	nbView = 0;
	ClearViewMenus();
}

void Interface::OverWriteView(int idOvr) {
	Geometry *geom = worker.GetGeometry();
	char *viewName = GLInputBox::GetInput(views[idOvr].name, "View name", "Enter view name");
	if (!viewName) return;

	views[idOvr] = viewer[curViewer]->GetCurrentView();
	views[idOvr].name = _strdup(viewName);
	RebuildViewMenus();
}

void Interface::AddView() {
	Geometry *geom = worker.GetGeometry();
	char tmp[32];
	sprintf(tmp, "View #%d", nbView + 1);
	char *viewName = GLInputBox::GetInput(tmp, "View name", "Enter view name");
	if (!viewName) return;

	if (nbView < MAX_VIEW) {
		views[nbView] = viewer[curViewer]->GetCurrentView();
		views[nbView].name = _strdup(viewName);
		nbView++;
	}
	else {
		SAFE_FREE(views[0].name);
		for (int i = 0; i < MAX_VIEW - 1; i++) views[i] = views[i + 1];
		views[MAX_VIEW - 1] = viewer[curViewer]->GetCurrentView();
		views[MAX_VIEW - 1].name = _strdup(viewName);
	}
	RebuildViewMenus();
}

//-----------------------------------------------------------------------------

void Interface::RemoveRecent(char *fileName) {

	if (!fileName) return;

	BOOL found = FALSE;
	int i = 0;
	while (!found && i < nbRecent) {
		found = strcmp(fileName, recents[i]) == 0;
		if (!found) i++;
	}
	if (!found) return;

	SAFE_FREE(recents[i]);
	for (int j = i; j < nbRecent - 1; j++)
		recents[j] = recents[j + 1];
	nbRecent--;

	// Update menu
	GLMenu *m = menu->GetSubMenu("File")->GetSubMenu("Load recent");
	m->Clear();
	for (i = nbRecent - 1; i >= 0; i--)
		m->Add(recents[i], MENU_FILE_LOADRECENT + i);
	SaveConfig();
}

void Interface::AddRecent(char *fileName) {

	// Check if already exists
	BOOL found = FALSE;
	int i = 0;
	while (!found && i < nbRecent) {
		found = strcmp(fileName, recents[i]) == 0;
		if (!found) i++;
	}
	if (found) {
		for (int j = i; j < nbRecent - 1; j++) {
			recents[j] = recents[j + 1];
		}
		recents[nbRecent - 1] = _strdup(fileName);
		// Update menu
		GLMenu *m = menu->GetSubMenu("File")->GetSubMenu("Load recent");
		m->Clear();
		for (int i = nbRecent - 1; i >= 0; i--)
			m->Add(recents[i], MENU_FILE_LOADRECENT + i);
		SaveConfig();
		return;
	}

	// Add the new recent file
	if (nbRecent < MAX_RECENT) {
		recents[nbRecent] = _strdup(fileName);
		nbRecent++;
	}
	else {
		// Shift
		SAFE_FREE(recents[0]);
		for (int i = 0; i < MAX_RECENT - 1; i++)
			recents[i] = recents[i + 1];
		recents[MAX_RECENT - 1] = _strdup(fileName);
	}

	// Update menu
	GLMenu *m = menu->GetSubMenu("File")->GetSubMenu("Load recent");
	m->Clear();
	for (int i = nbRecent - 1; i >= 0; i--)
		m->Add(recents[i], MENU_FILE_LOADRECENT + i);
	SaveConfig();
}

void Interface::AddStruct() {
	Geometry *geom = worker.GetGeometry();
	char tmp[32];
	sprintf(tmp, "Structure #%d", geom->GetNbStructure() + 1);
	char *structName = GLInputBox::GetInput(tmp, "Structure name", "Enter name of new structure");
	if (!structName) return;
	geom->AddStruct(structName);
	// Send to sub process
	try { worker.Reload(); }
	catch (Error &e) {
		GLMessageBox::Display((char *)e.GetMsg(), "Error", GLDLG_OK, GLDLG_ICONERROR);
		return;
	}
}

void Interface::DeleteStruct() {
	Geometry *geom = worker.GetGeometry();
	char *structNum = GLInputBox::GetInput("", "Structure number", "Number of structure to delete:");
	if (!structNum) return;
	int structNumInt;
	if (!sscanf(structNum, "%d", &structNumInt)) {
		GLMessageBox::Display("Invalid structure number");
		return;
	}
	if (structNumInt<1 || structNumInt>geom->GetNbStructure()) {
		GLMessageBox::Display("Invalid structure number");
		return;
	}
	BOOL hasFacets = FALSE;
	for (int i = 0; i < geom->GetNbFacet() && !hasFacets; i++) {
		if (geom->GetFacet(i)->sh.superIdx == (structNumInt - 1)) hasFacets = TRUE;
	}
	if (hasFacets) {
		int rep = GLMessageBox::Display("This structure has facets. They will be deleted with the structure.", "Structure delete", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONWARNING);
		if (rep != GLDLG_OK) return;
	}
	if (!AskToReset()) return;
	geom->DelStruct(structNumInt - 1);
	// Send to sub process
	try { worker.Reload(); }
	catch (Error &e) {
		GLMessageBox::Display((char *)e.GetMsg(), "Error", GLDLG_OK, GLDLG_ICONERROR);
		return;
	}
}

void Interface::DisplayCollapseDialog() {
	Geometry *geom = worker.GetGeometry();
	if (!collapseSettings) collapseSettings = new CollapseSettings();
	collapseSettings->SetGeometry(geom, &worker);
	collapseSettings->SetVisible(TRUE);
}

void Interface::RenumberSelections(const std::vector<int> &newRefs) {
	for (int i = 0; i < nbSelection; i++) {
		BOOL found = FALSE;
		for (int j = 0; j < selections[i].nbSel; j++) {
			if (selections[i].selection[j] >= newRefs.size() || newRefs[selections[i].selection[j]] == -1) { //remove from selection
				for (int k = j; k < (selections[i].nbSel - 1); k++)
					selections[i].selection[k] = selections[i].selection[k + 1];
				selections[i].nbSel--;
				if (selections[i].nbSel == 0) {
					ClearSelection(i); //last facet removed from selection
				}
				j--; //Do again the element as now it's the next
			}
			else { //renumber
				selections[i].selection[j] = newRefs[selections[i].selection[j]];
			}
		}
	}
}

void Interface::RenumberFormulas(std::vector<int> *newRefs) {
	for (int i = 0; i < nbFormula; i++) {
		char expression[1024];
		strcpy(expression, this->formulas[i].parser->GetExpression());
		if (OffsetFormula(expression, NULL, NULL, newRefs)) {
			this->formulas[i].parser->SetExpression(expression);
			this->formulas[i].parser->Parse();
			std::string formulaName = formulas[i].parser->GetName();
			if (formulaName.empty()) formulaName = expression;
			formulas[i].name->SetText(formulaName.c_str());
		}
	}
}

//-----------------------------------------------------------------------------
// Name: ProcessFormulaButtons()
// Desc: Handle forumla button event
//-----------------------------------------------------------------------------
void Interface::ProcessFormulaButtons(GLComponent *src) {

	// Search formula buttons
	BOOL found = FALSE;
	int i = 0;
	while (!found && i < nbFormula) {
		found = (src == formulas[i].setBtn);
		if (!found) i++;
	}
	if (found) {
		if (!formulaSettings) formulaSettings = new FormulaSettings();
		formulaSettings->Update(formulas[i].parser, i);
		formulaSettings->SetVisible(TRUE);
	}
}

void Interface::AddFormula(GLParser *f, BOOL doUpdate) {

	if (f) {
		if (nbFormula < MAX_FORMULA) {
			formulas[nbFormula].parser = f;
			std::string formulaName = f->GetName();
			if (formulaName.empty()) formulaName = f->GetExpression();
			formulas[nbFormula].name = new GLLabel(formulaName.c_str());
			Add(formulas[nbFormula].name);
			formulas[nbFormula].value = new GLTextField(0, "");
			formulas[nbFormula].value->SetEditable(FALSE);
			Add(formulas[nbFormula].value);
			formulas[nbFormula].setBtn = new GLButton(0, "...");
			Add(formulas[nbFormula].setBtn);
			nbFormula++;
			PlaceComponents();
			if (doUpdate) UpdateFormula();
		}
		else {
			SAFE_DELETE(f);
		}
	}

}


void Interface::AddFormula(const char *fName, const char *formula) {

	GLParser *f = new GLParser();
	f->SetExpression(formula);
	f->SetName(fName);
	f->Parse();
	AddFormula(f, FALSE);

}


void Interface::ClearFormula() {

	for (int i = 0; i < nbFormula; i++) {
		wnd->PostDelete(formulas[i].name);
		wnd->PostDelete(formulas[i].value);
		wnd->PostDelete(formulas[i].setBtn);
		formulas[i].name = NULL;
		formulas[i].value = NULL;
		formulas[i].setBtn = NULL;
		SAFE_DELETE(formulas[i].parser);
	}
	nbFormula = 0;
	PlaceComponents();

}

void Interface::UpdateFormulaName(int i) {
		std::string formulaName = formulas[i].parser->GetName();
		if (formulaName.empty()) formulaName = formulas[i].parser->GetExpression();
		formulas[i].name->SetText(formulaName.c_str());
		UpdateFormula();
}

void Interface::DeleteFormula(int i) {
			// Delete
			wnd->PostDelete(formulas[i].name);
			wnd->PostDelete(formulas[i].value);
			wnd->PostDelete(formulas[i].setBtn);
			formulas[i].name = NULL;
			formulas[i].value = NULL;
			formulas[i].setBtn = NULL;
			SAFE_DELETE(formulas[i].parser);
			for (int j = i; j < nbFormula - 1; j++) {
				formulas[j] = formulas[j + 1];
			}
			nbFormula--;
			PlaceComponents();
			wnd->DoPostDelete(); //forces redraw
			//UpdateFormula(); //no new values needed
}

BOOL Interface::OffsetFormula(char *expression, int offset, int filter, std::vector<int> *newRefs) {
	//will increase or decrease facet numbers in a formula
	//only applies to facet numbers larger than "filter" parameter
	//If *newRefs is not NULL, a vector is passed containing the new references
	BOOL changed = FALSE;

	string expr = expression; //convert char array to string

	size_t pos = 0; //analyzed until this position
	while (pos < expr.size()) { //while not end of expression

		vector<size_t> location; //for each prefix, we store where it was found

		for (int j = 0; j < (int)formulaPrefixes.size(); j++) { //try all expressions
			location.push_back(expr.find(formulaPrefixes[j], pos));
		}
		size_t minPos = string::npos;
		size_t maxLength = 0;
		for (int j = 0; j < (int)formulaPrefixes.size(); j++)  //try all expressions, find first prefix location
			if (location[j] < minPos) minPos = location[j];
		for (int j = 0; j < (int)formulaPrefixes.size(); j++)  //try all expressions, find longest prefix at location
			if (location[j] == minPos && formulaPrefixes[j].size() > maxLength) maxLength = formulaPrefixes[j].size();
		int digitsLength = 0;
		if (minPos != string::npos) { //found expression, let's find tailing facet number digits
			while ((minPos + maxLength + digitsLength) < expr.length() && expr[minPos + maxLength + digitsLength] >= '0' && expr[minPos + maxLength + digitsLength] <= '9')
				digitsLength++;
			if (digitsLength > 0) { //there was a digit after the prefix
				int facetNumber;
				if (sscanf(expr.substr(minPos + maxLength, digitsLength).c_str(), "%d", &facetNumber)) {
					if (newRefs == NULL) { //Offset mode
						if ((facetNumber - 1) > filter) {
							char tmp[10];
							sprintf(tmp, "%d", facetNumber + offset);
							expr.replace(minPos + maxLength, digitsLength, tmp);
							changed = TRUE;
						}
						else if ((facetNumber - 1) == filter) {
							expr.replace(minPos + maxLength, digitsLength, "0");
							changed = TRUE;
						}
					}
					else { //newRefs mode
						if ((facetNumber - 1) >= (*newRefs).size() || (*newRefs)[facetNumber - 1] == -1) { //Facet doesn't exist anymore
							expr.replace(minPos + maxLength, digitsLength, "0");
							changed = TRUE;
						}
						else { //Update facet number
							char tmp[10];
							sprintf(tmp, "%d", (*newRefs)[facetNumber - 1]);
							expr.replace(minPos + maxLength, digitsLength, tmp);
							changed = TRUE;
						}
					}
				}
			}
		}
		if (minPos != string::npos) pos = minPos + maxLength + digitsLength;
		else pos = minPos;
	}
	strcpy(expression, expr.c_str());
	return changed;
}

int Interface::Resize(DWORD width, DWORD height, BOOL forceWindowed) {
	int r = GLApplication::Resize(width, height, forceWindowed);
	PlaceComponents();
	return r;
}

void Interface::UpdateFacetlistSelected() {
	int nbSelected = 0;
	Geometry *geom = worker.GetGeometry();
	int nbFacet = geom->GetNbFacet();
	int* selection = (int*)malloc(nbFacet * sizeof(int));
	for (int i = 0; i < nbFacet; i++) {
		if (geom->GetFacet(i)->selected) {
			selection[nbSelected] = i;
			nbSelected++;
		}
	}

	//facetList->SetSelectedRows(selection,nbSelected,TRUE);
	if (nbSelected > 1000) {
		facetList->ReOrder();
		facetList->SetSelectedRows(selection, nbSelected, FALSE);
	}
	else {
		facetList->SetSelectedRows(selection, nbSelected, TRUE);
	}
	SAFE_FREE(selection);
}

int Interface::GetVariable(char *name, char *prefix) {

	char tmp[256];
	int  idx;
	int lgthP = (int)strlen(prefix);
	int lgthN = (int)strlen(name);

	if (lgthP >= lgthN) {
		return -1;
	}
	else {
		strcpy(tmp, name);
		tmp[lgthP] = 0;
		if (_stricmp(tmp, prefix) == 0) {
			strcpy(tmp, name + lgthP);
			int conv = sscanf(tmp, "%d", &idx);
			if (conv) {
				return idx;
			}
			else {
				return -1;
			}
		}
	}
	return -1;

}

void Interface::UpdateFormula() {

	char tmp[256];

	int idx;
	Geometry *geom = worker.GetGeometry();
	int nbFacet = geom->GetNbFacet();

	for (int i = 0; i < nbFormula; i++) {

		GLParser *f = formulas[i].parser;
		f->Parse(); //If selection group changed

					// Evaluate variables
		int nbVar = f->GetNbVariable();
		BOOL ok = TRUE;
		for (int j = 0; j < nbVar && ok; j++) {
			VLIST *v = f->GetVariableAt(j);
			ok = EvaluateVariable(v, &worker, geom);
		}

		// Evaluation
		if (ok) { //Variables succesfully evaluated
			double r;
			if (f->Evaluate(&r)) {
				sprintf(tmp, "%g", r);
				formulas[i].value->SetText(tmp);
			}
			else { //Variables OK but the formula itself can't be evaluated
				formulas[i].value->SetText(f->GetErrorMsg());
			}
#ifdef MOLFLOW
			formulas[i].value->SetTextColor(0.0f, 0.0f, worker.displayedMoment == 0 ? 0.0f : 1.0f);
#endif
		}
		else { //Error while evaluating variables
			formulas[i].value->SetText("Invalid variable name");
		}
	}
}

BOOL Interface::AskToSave() {
	if (!changedSinceSave) return TRUE;
	int ret = GLSaveDialog::Display("Save current geometry first?", "File not saved", GLDLG_SAVE | GLDLG_DISCARD | GLDLG_CANCEL_S, GLDLG_ICONINFO);
	if (ret == GLDLG_SAVE) {
		FILENAME *fn = GLFileBox::SaveFile(currentDir, worker.GetShortFileName(), "Save File", fileSFilters, 0);
		if (fn) {
			GLProgress *progressDlg2 = new GLProgress("Saving file...", "Please wait");
			progressDlg2->SetVisible(TRUE);
			progressDlg2->SetProgress(0.0);
			//GLWindowManager::Repaint();
			try {
				worker.SaveGeometry(fn->fullName, progressDlg2);
				changedSinceSave = FALSE;
				UpdateCurrentDir(fn->fullName);
				UpdateTitle();
				AddRecent(fn->fullName);
			}
			catch (Error &e) {
				char errMsg[512];
				sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fn->fullName);
				GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);
				RemoveRecent(fn->fullName);
			}
			progressDlg2->SetVisible(FALSE);
			SAFE_DELETE(progressDlg2);
			return TRUE;
		}
		else return FALSE;
	}
	else if (ret == GLDLG_DISCARD) return TRUE;
	return FALSE;
}

void Interface::CreateOfTwoFacets(ClipperLib::ClipType type, int reverseOrder) {
	Geometry *geom = worker.GetGeometry();
	if (geom->IsLoaded()) {
		try {
			if (AskToReset()) {
				//geom->CreateDifference();
				geom->ClipSelectedPolygons(type,reverseOrder);
			}
		}
		catch (Error &e) {
			GLMessageBox::Display((char *)e.GetMsg(), "Error creating polygon", GLDLG_OK, GLDLG_ICONERROR);
		}
		//UpdateModelParams();
		try { worker.Reload(); }
		catch (Error &e) {
			GLMessageBox::Display((char *)e.GetMsg(), "Error reloading worker", GLDLG_OK, GLDLG_ICONERROR);
		}
	}
	else GLMessageBox::Display("No geometry loaded.", "No geometry", GLDLG_OK, GLDLG_ICONERROR);
}

void Interface::UpdateRecentMenu(){
	// Update menu
	GLMenu *m = menu->GetSubMenu("File")->GetSubMenu("Load recent");
	m->Clear();
	for (int i = nbRecent - 1; i >= 0; i--)
		m->Add(recents[i], MENU_FILE_LOADRECENT + i);
}

void Interface::SaveFileAs() {

	FILENAME *fn = GLFileBox::SaveFile(currentDir, worker.GetShortFileName(), "Save File", fileSFilters, 0);

	GLProgress *progressDlg2 = new GLProgress("Saving file...", "Please wait");
	progressDlg2->SetProgress(0.0);
	progressDlg2->SetVisible(TRUE);
	//GLWindowManager::Repaint();  
	if (fn) {

		try {

			worker.SaveGeometry(fn->fullName, progressDlg2);
			ResetAutoSaveTimer();
			changedSinceSave = FALSE;
			UpdateCurrentDir(worker.fullFileName);
			UpdateTitle();
			AddRecent(worker.fullFileName);
		}
		catch (Error &e) {
			char errMsg[512];
			sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fn->fullName);
			GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);
			RemoveRecent(fn->fullName);
		}

	}

	progressDlg2->SetVisible(FALSE);
	SAFE_DELETE(progressDlg2);
}

void Interface::ExportTextures(int grouping, int mode) {

	Geometry *geom = worker.GetGeometry();
	if (geom->GetNbSelected() == 0) {
		GLMessageBox::Display("Empty selection", "Error", GLDLG_OK, GLDLG_ICONERROR);
		return;
	}
	if (!worker.IsDpInitialized()) {
		GLMessageBox::Display("Worker Dataport not initialized yet", "Error", GLDLG_OK, GLDLG_ICONERROR);
		return;
	}

	FILENAME *fn = GLFileBox::SaveFile(currentDir, NULL, "Save File", fileTexFilters, 0);

	if (fn) {

		try {
			worker.ExportTextures(fn->fullName, grouping, mode, TRUE, TRUE);
			//UpdateCurrentDir(fn->fullName);
			//UpdateTitle();
		}
		catch (Error &e) {
			char errMsg[512];
			sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fn->fullName);
			GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);
		}

	}

}

void Interface::DoEvents(BOOL forced)
{
	static float lastChkEvent = 0;
	static float lastRepaint = 0;
	int time = SDL_GetTicks();
	if (forced || (time - lastChkEvent > 200)) { //Don't check for inputs more than 5 times a second
		SDL_Event sdlEvent;
		SDL_PollEvent(&sdlEvent);
		UpdateEventCount(&sdlEvent);
		/*if (GLWindowManager::ManageEvent(&sdlEvent)) {
		// Relay to GLApp EventProc
		mApp->EventProc(&sdlEvent);
		}*/
		GLWindowManager::ManageEvent(&sdlEvent);
		lastChkEvent = time;
	}
	if (forced || (time - lastRepaint > 500)) { //Don't redraw more than every 500 msec
		GLWindowManager::Repaint();
		GLToolkit::CheckGLErrors("GLApplication::Paint()");
		lastRepaint = time;
	}

}

BOOL Interface::AskToReset(Worker *work) {
	if (work == NULL) work = &worker;
	if (work->nbHit > 0) {
		int rep = GLMessageBox::Display("This will reset simulation data.", "Geometry change", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONWARNING);
		if (rep == GLDLG_OK) {
			work->ResetStatsAndHits(m_fTime);
			nbDesStart = 0;
			nbHitStart = 0;

			//resetSimu->SetEnabled(FALSE);
			UpdatePlotters();
			return TRUE;
		}
		else return FALSE;
	}
	else return TRUE;
}

int Interface::FrameMove()
{
	char tmp[256];
	Geometry *geom = worker.GetGeometry();

	//Autosave routines
	BOOL timeForAutoSave = FALSE;
	if (geom->IsLoaded()) {
		if (autoSaveSimuOnly) {
			if (worker.running) {
				if (((worker.simuTime + (m_fTime - worker.startTime)) - lastSaveTimeSimu) >= (float)autoSaveFrequency*60.0f) {
					timeForAutoSave = TRUE;
				}
			}
		}
		else {
			if ((m_fTime - lastSaveTime) >= (float)autoSaveFrequency*60.0f) {
				timeForAutoSave = TRUE;
			}
		}
	}

	if (worker.running) {
		if (m_fTime - lastUpdate >= 1.0f) {

			sprintf(tmp, "Running: %s", FormatTime(worker.simuTime + (m_fTime - worker.startTime)));
			sTime->SetText(tmp);
			wereEvents = TRUE; //Will repaint

			UpdateStats(); //Update m_fTime
			lastUpdate = m_fTime;

			if (updateRequested || autoFrameMove) {

				forceFrameMoveButton->SetEnabled(FALSE);
				forceFrameMoveButton->SetText("Updating...");
				//forceFrameMoveButton->Paint();
				//GLWindowManager::Repaint();

				updateRequested = FALSE;

				// Update hits
				try {
					worker.Update(m_fTime);
				}
				catch (Error &e) {
					GLMessageBox::Display((char *)e.GetMsg(), "Error (Stop)", GLDLG_OK, GLDLG_ICONERROR);
				}
				// Simulation monitoring
				UpdatePlotters();

				// Formulas
				if (autoUpdateFormulas) UpdateFormula();

				//lastUpdate = GetTick(); //changed from m_fTime: include update duration


				// Update timing measurements
				if (worker.nbHit != lastNbHit || worker.nbDesorption != lastNbDes) {
					double dTime = (double)(m_fTime - lastMeasTime);
					hps = (double)(worker.nbHit - lastNbHit) / dTime;
					dps = (double)(worker.nbDesorption - lastNbDes) / dTime;
					if (lastHps != 0.0) {
						hps = 0.2*(hps)+0.8*lastHps;
						dps = 0.2*(dps)+0.8*lastDps;
					}
					lastHps = hps;
					lastDps = dps;
					lastNbHit = worker.nbHit;
					lastNbDes = worker.nbDesorption;
					lastMeasTime = m_fTime;
				}
			}
		}

#ifdef MOLFLOW
		if (worker.calcAC) {
			sprintf(tmp, "Calc AC: %s (%d %%)", FormatTime(worker.simuTime + (m_fTime - worker.startTime)),
				worker.calcACprg);
		}
		else {
#endif
			
#ifdef MOLFLOW
		}
#endif

		

		forceFrameMoveButton->SetEnabled(!autoFrameMove);
		forceFrameMoveButton->SetText("Update");
	}
	else {
		if (worker.simuTime > 0.0) {
			hps = (double)(worker.nbHit - nbHitStart) / worker.simuTime;
			dps = (double)(worker.nbDesorption - nbDesStart) / worker.simuTime;
		}
		else {
			hps = 0.0;
			dps = 0.0;
		}
		sprintf(tmp, "Stopped: %s", FormatTime(worker.simuTime));
		sTime->SetText(tmp);
	}

	// Facet parameters and hits
	if (viewer[0]->SelectionChanged() ||
		viewer[1]->SelectionChanged() ||
		viewer[2]->SelectionChanged() ||
		viewer[3]->SelectionChanged()) {
		UpdateFacetParams(TRUE);
	}
	UpdateFacetHits();

	//Autosave
	if (timeForAutoSave) AutoSave();

	//Updates
	if (latestVersionId > appVersion) {
		std::stringstream tmp;
		tmp << "New version: " << latestVersionId << "\nCurrent: " << appVersion;
		GLMessageBox::Display(tmp.str().c_str(), "Update available");
		latestVersionId = -1;
	}

	if (worker.nbLeakTotal) {
		sprintf(tmp, "%g (%.4f%%)", (double)worker.nbLeakTotal, (double)(worker.nbLeakTotal)*100.0 / (double)worker.nbDesorption);
		leakNumber->SetText(tmp);
	}
	else {
		leakNumber->SetText("None");
	}
	resetSimu->SetEnabled(!worker.running&&worker.nbDesorption > 0);

	if (worker.running) {
		startSimu->SetText("Pause");
		//startSimu->SetFontColor(255, 204, 0);
	}
	else if (worker.nbHit > 0) {
		startSimu->SetText("Resume");
		//startSimu->SetFontColor(0, 140, 0);
	}
	else {
		startSimu->SetText("Begin");
		//startSimu->SetFontColor(0, 140, 0);
	}

	/*
	// Sleep a bit to avoid unwanted CPU load
	if (viewer[0]->IsDragging() ||
		viewer[1]->IsDragging() ||
		viewer[2]->IsDragging() ||
		viewer[3]->IsDragging() || !worker.running)
	{
		SDL_Delay(22); //was 22
	}
	else
	{
		SDL_Delay(60); //was 60
	}
	*/
	double delayTime = 0.03 - (wereEvents?fPaintTime:0.0) - fMoveTime;
	if (delayTime > 0) SDL_Delay((int)(1000.0*delayTime)); //Limits framerate at about 60fps

	return GL_OK;
}

void Interface::ResetAutoSaveTimer() {
	UpdateStats(); //updates m_fTime
	if (autoSaveSimuOnly) lastSaveTimeSimu = worker.simuTime + (m_fTime - worker.startTime);
	else lastSaveTime = m_fTime;
}

BOOL Interface::AutoSave(BOOL crashSave) {
	if (!changedSinceSave) return TRUE;
	GLProgress *progressDlg2 = new GLProgress("Peforming autosave...", "Please wait");
	progressDlg2->SetProgress(0.0);
	progressDlg2->SetVisible(TRUE);
	//GLWindowManager::Repaint();
	char CWD[MAX_PATH];
	_getcwd(CWD, MAX_PATH);

	std::string shortFn(worker.GetShortFileName());
#ifdef MOLFLOW
	std::string newAutosaveFilename = "Molflow_Autosave";
	if (shortFn != "") newAutosaveFilename += "(" + shortFn + ")";
	newAutosaveFilename += ".zip";
#endif
#ifdef SYNRAD
	std::string newAutosaveFilename = "Synrad_Autosave";
	if (shortFn != "") newAutosaveFilename += "(" + shortFn + ")";
	newAutosaveFilename += ".syn7z";
#endif
	char fn[1024];
	strcpy(fn, newAutosaveFilename.c_str());
	try {
		worker.SaveGeometry(fn, progressDlg2, FALSE, FALSE, TRUE, crashSave);
		//Success:
		if (autosaveFilename != "" && autosaveFilename != newAutosaveFilename) remove(autosaveFilename.c_str());
		autosaveFilename = newAutosaveFilename;
		ResetAutoSaveTimer(); //deduct saving time from interval
	}
	catch (Error &e) {
		//delete fn;
		char errMsg[512];
		sprintf(errMsg, "%s\nFile:%s", e.GetMsg(), fn);
		GLMessageBox::Display(errMsg, "Error", GLDLG_OK, GLDLG_ICONERROR);
		progressDlg2->SetVisible(FALSE);
		SAFE_DELETE(progressDlg2);
		ResetAutoSaveTimer();
		return FALSE;
	}
	//lastSaveTime=(worker.simuTime+(m_fTime-worker.startTime));
	progressDlg2->SetVisible(FALSE);
	SAFE_DELETE(progressDlg2);
	return TRUE;
}

void Interface::CheckForRecovery() {
	// Check for autosave files in current dir.
	intptr_t file;
	_finddata_t filedata;
#ifdef MOLFLOW
	file = _findfirst("Molflow_Autosave*.zip", &filedata);
#endif

#ifdef SYNRAD
	file = _findfirst("Synrad_Autosave*.syn*", &filedata);
#endif

	if (file != -1)
	{
		do
		{
			std::ostringstream msg;
			msg << "Autosave file found:\n" << filedata.name << "\n";
			int rep = RecoveryDialog::Display(msg.str().c_str(), "Autosave recovery", GLDLG_LOAD | GLDLG_SKIP, GLDLG_DELETE);
			if (rep == GLDLG_LOAD) {
				LoadFile(filedata.name);
				RemoveRecent(filedata.name);
			}
			else if (rep == GLDLG_CANCEL) return;
			else if (rep == GLDLG_SKIP) continue;
			else if (rep == GLDLG_DELETE) remove(filedata.name);
		} while (_findnext(file, &filedata) == 0);
	}
	_findclose(file);
}

void Interface::AppUpdater() {
	if (!checkForUpdates) {
		if (appLaunchesWithoutAsking >= 2) { //Third time launching app, time to ask if we can check for updates
			std::stringstream msg;
			msg << "Can " << appId << " check online for updates?\nYou can change the setting later in Tools / Global Settings";
			int ret = GLMessageBox::Display(msg.str().c_str(), "Auto-updater", GLDLG_OK | GLDLG_CANCEL, GLDLG_ICONINFO);
			checkForUpdates = (ret == GLDLG_OK);
			appLaunchesWithoutAsking = -1;SaveConfig(); //Don't ask again
		}
	}
	else {
		appLaunchesWithoutAsking = -1; //Don't ask if already checking for updates
	}

	if (checkForUpdates) {
		latestVersionId = -1;
		updateThread = std::thread(&Interface::CheckUpdates, (Interface*)this); //Launch parallel update-checking thread
	}
}

void Interface::CheckUpdates() {
	//Update checker

	std::string url = "https://molflow.web.cern.ch/sites/molflow.web.cern.ch/files/autoupdate.xml"; //Update feed
	std::string body = DownloadString(url);

	pugi::xml_document updateDoc;
	pugi::xml_parse_result result = updateDoc.load_string(body.c_str());

	//DownloadInstallUpdate("https://molflow.web.cern.ch/sites/molflow.web.cern.ch/files/molflow_2.6.33.zip", "molflow_2.6.33.zip", "molflow_2.6.33", "molflow.cfg", TRUE);
	
	if (installId == "default") {
		//First update check: generate random install identifier, like a browser cookie (4 alphanumerical characters)
		//It is generated based on the computer's network name and the logged in user name
		//FOR USER PRIVACY, THESE ARE NOT SENT TO GOOGLE ANALYTICS, JUST AN ANONYMOUS HASH
		//Should get the same hash even in case of subsequent installs

		char computerName[1024];
		char userName[1024];
		DWORD size = 1024;
		GetComputerName(computerName, &size);
		GetUserName(userName, &size);
		std::string id = computerName;
		id += "/";
		id += userName;

		//Create hash code from computer name
		//Source: http://www.cse.yorku.ca/~oz/hash.html
		size_t hashCode = 5381;
		int c;
		size_t index = 0;
		while (c = id.c_str()[index++]) {
			hashCode = ((hashCode << 5) + hashCode) + c; /* hash * 33 + c */
		}		

		//Convert hash number to alphanumerical hash (base62)
		std::string alphaNum =
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

		installId = "";
		while (hashCode > 0) {
			size_t dividend = (size_t)(hashCode / alphaNum.length());
			size_t remainder = hashCode - dividend*alphaNum.length();
			hashCode = dividend;
			installId = alphaNum[remainder] + installId;
		}
	}

	std::string GoogleAnalyticsTrackingId = "UA-86802533-2";
	std::string eventCategory = "updateCheck";

	std::stringstream payload;
	payload << "v=1&t=event&tid=" << GoogleAnalyticsTrackingId << "&cid=" << installId << "&ec=" << eventCategory << "&ea=" << appId << "_" << appVersion;
	SendHTTPPostRequest("http://www.google-analytics.com/collect", payload.str()); //Sends random app and version id for analytics. Also sends install id to count number of users
}

void Interface::DownloadInstallUpdate(std::string zipUrl, std::string zipName, std::string folderName, std::string configName, BOOL copyCfg) {

	//Download the zipped new version to parent directory
	std::stringstream zipDest; zipDest << "..\\" << zipName;
	DownloadFile(zipUrl, zipDest.str());

	//Extract it to LOCAL directory (no way to directly extract to parent)
	HZIP hz = OpenZip(zipDest.str().c_str(), 0);
	ZIPENTRY ze; GetZipItem(hz, -1, &ze); int numitems = ze.index;
	// -1 gives overall information about the zipfile
	for (int zi = 0; zi<numitems; zi++)
	{
		ZIPENTRY ze; GetZipItem(hz, zi, &ze); // fetch individual details
		UnzipItem(hz, zi, ze.name);         // e.g. the item's name.
	}
	CloseZip(hz);

	//ZIP file not required anymore
	DeleteFile(zipDest.str().c_str());

	//Move extracted dir to parent dir
	std::stringstream folderDest; folderDest << "..\\" << folderName;
	MoveFileEx(folderName.c_str(), folderDest.str().c_str(), MOVEFILE_WRITE_THROUGH);

	//Copy current config file to new version's dir
	std::stringstream configDest; configDest << folderDest.str() << configName;
	if (copyCfg) CopyFile(configName.c_str(), folderDest.str().c_str(),FALSE);

	//Optionally run "..\app_name.exe" and exit

}