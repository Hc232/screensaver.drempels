/* 
*  This source file is part of Drempels, a program that falls
*  under the Gnu Public License (GPL) open-source license.  
*  Use and distribution of this code is regulated by law under 
*  this license.  For license details, visit:
*    http://www.gnu.org/copyleft/gpl.html
* 
*  The Drempels open-source project is accessible at 
*  sourceforge.net; the direct URL is:
*    http://sourceforge.net/projects/drempels/
*  
*  Drempels was originally created by Ryan M. Geiss in 2001
*  and was open-sourced in February 2005.  The original
*  Drempels homepage is available here:
*    http://www.geisswerks.com/drempels/
*
*/

/*
DREMPELS, IN BRIEF
Drempels is an application that replaces your desktop background 
(or "wallpaper") with smoothly-animated psychedelic graphics.  
It renders the graphics on the CPU at a low resolution, and then 
uses a hardware overlay to stretch the image to fit on the entire screen.  
At video scan time, any color that is the same as the "overlay key color" 
(15,0,15 by default) will be replaced by the Drempels image, so we paint 
the wallpaper with the overlay key color to ensure that the Drempels image 
shows up where your wallpaper would normally be.  

Drempels can also be run as a screensaver.  The separate project, 
drempels_scr.dsw, will build a tiny screensaver that simply calls the 
main drempels.exe when it's time to run the screensaver.

Run drempels.exe with no arguments to start it in desktop mode.  Run 
"drempels /s" to run it fullscreen (screensaver mode), and run 
"drempels /c" to configure it.

Drempels is currently ported only to the Windows platform.  
It requires an MMX processor, DirectX 5.0 or later, and hardware 
overlay support (which is almost universal, since many video players 
use it).

SOURCE FILE OVERVIEW
main.cpp     - the bulk of the code
gpoly.cpp/h  - assembly code for the rasterization of the main image.
video.h      - assembly code for blitting image to the screen.  
includes stretch factors of 1X, 2X, and 4X.
yuv.cpp/h    - assembly code for conversion from RGB to YUV colorspace
(for blitting to overlay surfaces).
sysstuff.h   - code for reading/writing the registry, checking for MMX, etc.
texmgr.cpp/h - class that loads and manages the source bitmaps


RELEASING A NEW VERSION: checklist:
-update version # identifier and on the 2 dialogs (config + about)
-update version # in the DREMPELS.NSI (installer) file (2 occurences)
-put release date in DREMPELS.TXT (x2)
-compile in RELEASE mode
-post 2 zips and 1 txt on web site
-backup frozen source

NOTES ON INVOKING DREMPELS:
-startup folder:  'drempels.exe'    ( -> runs in app mode, where g_bRunningAsSaver is true)
-drempels folder: 'drempels.exe'    ( -> runs in app mode, where g_bRunningAsSaver is true)
-screensaver:     'drempels.scr [/s|/c]'   ( -> runs drempels.exe [/s|/c])

-------------------------------------------------------------------------------

to do:
-figure out how to get notification when monitor turns back on 
after powering off (and call Update_Overlay() then?) 
-screensaver seems to not always kick in on XP systems 
(and I've heard that maybe it just doesn't work on some XP systems). 
-a feature to automatically turn off 'active desktop' would be cool. 
-port to non-Windows platforms 
-make it multiple-monitors friendly (...for a good reference on how 
to make it multiple-monitor friendly, check out the VMS sample 
Winamp plug-in, which is 100% multimon-happy: http://www.nullsoft.com/free/vms/ ).
-
-
-
*/

// VS6 project settings:
// ---------------------
//  libs:  comctl32.lib ddraw.lib dsound.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib dxguid.lib winmm.lib LIBCMT.LIB 
//  settings:  C/C++ -> code generation -> calling convention is __stdcall.




#define CURRENT_VERSION 150                 // ex: "140" means v1.40

// size of the main grid for computation of motion vectors:
// (motion vectors are then bicubically interpolated, per-pixel, within grid cells)
#define UVCELLSX 36 //18
#define UVCELLSY 28 //14

#define WM_DREMPELS_SYSTRAY_MSG			WM_USER + 407
#define IDC_DREMPELS_SYSTRAY_ICON			555
#define ID_DREMPELS_SYSTRAY_CLOSE			556
#define ID_DREMPELS_SYSTRAY_HIDE_CTRL_WND	557
#define ID_DREMPELS_SYSTRAY_SHOW_CTRL_WND	558
#define ID_DREMPELS_SYSTRAY_RESUME			559
#define ID_DREMPELS_SYSTRAY_SUSPEND			560
#define ID_DREMPELS_SYSTRAY_HOTKEYS			561

#include "xbsBase.h"
#include <stdio.h>
#include <string>

extern LPDIRECT3DDEVICE9       g_pd3dDevice;

// PARAMETERS---------------

// hidden:
float warp_factor = 0.22f;			// 0.05 = no warping, 0.25 = good warping, 0.75 = chaos
float rotational_speed = 0.05f;
float mode_focus_exponent = 2.9f;	// 1 = simple linear combo (very blendy) / 4 = fairly focused / 16 = 99% singular
int limit_fps = 30;
int tex_scale = 10;			// goes from 0 to 20; 10 is normal; 0 is down 4X; 20 is up 4X
int speed_scale = 10;		// goes from 0 to 20; 10 is normal; 0 is down 8X; 20 is up 8X

// exposed:
bool  g_bResize = true;
bool  g_bAutoBlend = false;
int   g_iBlendPercent = 7;     

float time_between_textures = 20.0f;
float texture_fade_time = 5.0f;
extern std::string g_strPresetsDir;
char  szTexPath[MAX_PATH];

int   g_nTimeBetweenSubdirs = 120;
int   g_subdirIndices[9] = { 0, 1, 2, 3, 4, 5, 6, 7, 8 };
int   g_nSubdirs = 9;
bool  g_bAutoSwitchSubdirs = false;
bool  g_bAutoSwitchRandom = false;
float g_fTimeTilSubdirSwitch = 120;
int   g_nSubdirIndex = -1;
float anim_speed = 1.0f;
float master_zoom = 1.0f;	// 2 = zoomed in 2x, 0.5 = zoomed OUT 2x.
float mode_switch_speed_multiplier = 1.0f;		// 1 = normal speed, 2 = double speed, 0.5 = half speed
int   motion_blur = 7;		// goes from 0 to 10
int   g_bExitOnMouseMove = true;
int   g_iMouseMoves = 0;
int   g_iMouseMovesThisFrame = 0;
int   g_bAnimFrozen = 0;
int   g_bPausedViaMinimize = 0;
int   g_bForceRepaintOnce = 0;
//int   g_bGeissMode  = 0;
int   g_bSuspended = 0;		// only available in Desktop mode; means old wallpaper is back & drempels is suspended.
int   g_bTrackingPopupMenu = false;

DWORD mainproc_start_time;


bool g_bIsRunningAsSaver = false;	// set to TRUE if /s flag is present
bool g_bStartupMode = false;			// set to TRUE if /y flag is present
bool g_bRunDrempelsAtStartup = false;

// procsize: 0=full, 1=half, 2=quarter
int  procsize_as_app   = 2;
int  procsize_as_saver = 0;
int  procsize = 1;			    

// rendermode:
// 0 fullscreen, 
// 1 windowed, 
// 2 background mode
// 3 desktop mode
int   rendermode_as_app   = 3;
int   rendermode_as_saver = 0;
int   rendermode = 0;		

bool  high_quality = true;

DWORD dwOldBackgroundColor = 0;
bool bOldWallpaperKnown = false;
bool bWallpaperSet = false;
unsigned char key_R = 15;	// christophe uses 0,5,8
unsigned char key_G = 0;	// 15,0,15 works on voodoo3 in 16-bit color!  
unsigned char key_B = 15;	//   (but 15,15,15 doesn't - dither problems)

extern int g_width;
extern int g_height;


bool  g_bCoeffsFrozen = false;
bool  g_bToggleCoeffFreeze = false;
float g_fCoeffFreezeTime;
float g_fCoeffFreezeDebt = 0;

// coords of the window that accomodates the FXWxFXH drawing area,
// if the drawing area upper-left corner is located at 0x0.
RECT g_clientRect;

//COLORREF g_FgTextColor = RGB(255,255,255);
//COLORREF g_BkgTextColor = RGB(0,0,0); 

float g_fBlurAmount = 0.88f;	// goes from 0 to 0.97

bool bWarpmodePaused = false;


BOOL MungeFPCW( WORD *pwOldCW )
{
	BOOL ret = FALSE;
	WORD wTemp, wSave;

	__asm fstcw wSave
		if (wSave & 0x300 ||            // Not single mode
			0x3f != (wSave & 0x3f) ||   // Exceptions enabled
			wSave & 0xC00)              // Not round to nearest mode
		{
			__asm
			{
				mov ax, wSave
					and ax, not 300h    ;; single mode
					or  ax, 3fh         ;; disable all exceptions
					and ax, not 0xC00   ;; round to nearest mode
					mov wTemp, ax
					fldcw   wTemp
			}
			ret = TRUE;
		}
		*pwOldCW = wSave;
		return ret;
}


void RestoreFPCW(WORD wSave)
{
	__asm fldcw wSave
}



#define WIN32_LEAN_AND_MEAN

#pragma hdrstop

#define NAME "Drempels"
#define TITLE "Drempels"

#include <time.h>
#include <math.h>

void FX_Init();

void DrempelsExit();

BOOL doInit();

//----------------------------------------------------

HWND hSaverMainWindow;
HWND g_hWndHotkeys = NULL;
bool g_bSystrayReady = false;
LPGUID g_lpGuid;
float debug_param = 1.0;

//----------------------------------------------------

int  VidMode = 0;
BOOL VidModeAutoPicked = false;
unsigned int  iNumVidModes = 0;  // set by DDraw's enumeration
unsigned int  iDispBits=24;
long FXW = 640;         // to set default pick, go to EnumModesCallback()
long FXH = 480;         // to set default pick, go to EnumModesCallback()
long FXW2 = FXW;
long FXH2 = FXH;



typedef struct
{
	int  iDispBits;
	int  FXW;
	int  FXH;
	BOOL VIDEO_CARD_555;
	char name[64];
} VidModeInfo;
#define MAX_VID_MODES 512
VidModeInfo VidList[MAX_VID_MODES];



int  ThreadNr = 0;
BOOL g_QuitASAP = false;
BOOL g_MainProcFinished = false;
BOOL g_bFirstRun = false;
BOOL g_bDumpFileCleared = false;
BOOL g_bDebugMode = false;
BOOL g_bSuppressHelpMsg = false;
BOOL g_bSuppressAllMsg = false;
BOOL g_DisclaimerAgreed = true;
BOOL g_ConfigAccepted = false;
BOOL g_Capturing = false;
BOOL g_bTakeScreenshot = false;
BOOL g_bLostFocus = false;
BOOL g_bUserAltTabbedOut = false;

/*-------------------------------------------------*/

bool  bExitOnMouse = FALSE;
int   last_mouse_x = -1;
int   last_mouse_y = -1;

// fps info
#define FPS_FRAMES 32
float fps = 0;			// fps over the last FPS_FRAMES frames, **factoring out the time spent blending textures**
float honest_fps = 0;	// honest fps over the last 32 frames (for report to user)
float tex_fade_debt[FPS_FRAMES];
int   tex_fade_debt_pos	= 0;
float time_rec[FPS_FRAMES];
int   time_rec_pos = 0;
float sleep_amount = 0;		// adjusted to maintain limit_fps
float avg_texfade_time = 0;

void RandomizeStartValues();
float fRandStart1;
float fRandStart2;
float fRandStart3;
float fRandStart4;
float warp_w[4];
float warp_uscale[4];
float warp_vscale[4];
float warp_phase[4];

char szDEBUG[512];
char szMCM[] = " [click mouse button to exit - press h for help] ";
char szTEXNAME[512];
char szTEXLOADFAIL[512];
char szBehLocked[] = " - behavior is LOCKED - ";
char szBehUnlocked[] = " - behavior is unlocked - ";
char szLocked[] = " - texture is LOCKED - ";
char szUnlocked[] = " - texture is unlocked - ";
char szTrack[] = " - stopped at track xxx/yyy -              ";

char szH1[]  = " SPACE: load random texture ";
char szH2[]  = " R: randomize texture & behavior ";
char szH3[]  = " T/B: lock/unlock cur. texture/behavior "; //"z x c v b:  << play pause stop >> (for CD) ";
char szH4[]  = " Q: toggle texture quality (hi/lo) "; 
char szH5[]  = " +/-: adjust motion blur ";
char szH6[]  = " J/K: adjust zoom, U/I: adjust speed ";
char szH7[]  = " H/F: display help/fps ";
char szH8[]  = " P: [un]pause animation ";
char szH9[]  = " M: toggle messages on/off ";//" @: save screenshot to C:\\ ";
char szH10[] = " ESC: quit, N: minimize ";
char szH11[] = "";//" F5: refresh texture file listing ";
char szCurrentCD[128];
char szNewCD[128];

LPDIRECT3DTEXTURE9 lpD3DVS[2] = {NULL, NULL};

BOOL					bUserQuit = FALSE;

int gXC, gYC;

char winpath[512];

long  intframe=0;
unsigned char VIDEO_CARD_555=0;		// ~ VIDEO_CARD_IS_555
unsigned char SHOW_DEBUG          = 0;
unsigned char SHOW_MOUSECLICK_MSG = 30;
unsigned char SHOW_TRACK_MSG      = 0;
unsigned char SHOW_MODEPREFS_MSG  = 0;
unsigned char SHOW_LOCKED         = 0;
unsigned char SHOW_UNLOCKED       = 0;
unsigned char SHOW_BEH_LOCKED     = 0;
unsigned char SHOW_BEH_UNLOCKED   = 0;
unsigned char SHOW_TEXLOADFAIL    = 0;
unsigned char SHOW_TEXNAME        = 0;
bool          SHOW_FPS            = 0;
bool          SHOW_HELP_MSG       = 0;


extern HINSTANCE hMainInstance; /* screen saver instance handle  */ 
HINSTANCE g_hSaverMainInstance = NULL;

enum TScrMode {smNone,smConfig,smPassword,smPreview,smSaver};
TScrMode ScrMode=smNone;

#include "gpoly.h"
#include "texmgr.h"

texmgr TEX;
int    g_bRandomizeTextures = true;
float  g_fTexStartTime = 0.0f;
int    g_bTexFading = false;
int    g_bTexLocked = false;
td_cellcornerinfo cell[UVCELLSX][UVCELLSY];

HINSTANCE hInstance=NULL;
HWND hScrWindow=NULL;


struct TEXVERTEX
{
	FLOAT x, y, z, w;	// Position
	DWORD colour;		// Colour
	float u, v;			// Texture coords
};

#define	D3DFVF_TEXVERTEX (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)



void RandomizeStartValues()
{
	if (g_bCoeffsFrozen) return;

	fRandStart1 = 6.28f*(rand() % 4096)/4096.0f;
	fRandStart2 = 6.28f*(rand() % 4096)/4096.0f;
	fRandStart3 = 6.28f*(rand() % 4096)/4096.0f;
	fRandStart4 = 6.28f*(rand() % 4096)/4096.0f;

	// randomize rotational direction
	if (rand()%2) rotational_speed *= -1.0f;

	// randomize warping parameters
	for (int i=0; i<4; i++)
	{
		warp_w[i]      = 0.02f + 0.015f*(rand()%4096)/4096.0f; 
		warp_uscale[i] = 0.23f + 0.120f*(rand()%4096)/4096.0f; 
		warp_vscale[i] = 0.23f + 0.120f*(rand()%4096)/4096.0f; 
		warp_phase[i]  = 6.28f*(rand()%4096)/4096.0f;
		if (rand() % 2) warp_w[i] *= -1;
		if (rand() % 2) warp_uscale[i] *= -1;
		if (rand() % 2) warp_vscale[i] *= -1;
		if (rand() % 2) warp_phase[i] *= -1;
	}
}



void DrempelsRender()
{    
	int iStep = 1;

	// animate (image will move)

	// regular timekeeping stuff
	intframe++;

	static float fOldTime; 
	static float fTime = 0;
	if (fTime == 0)
		fTime = (GetTickCount() - mainproc_start_time) * 0.001f / 0.92f;
	else
		fTime = fTime*0.92f + 0.08f*(GetTickCount() - mainproc_start_time) * 0.001f;

	float fDeltaT = fTime - fOldTime;

	fOldTime = fTime;

	// target-fps stuff
	float prev_time_rec = time_rec[time_rec_pos];
	time_rec[time_rec_pos++] = fTime;
	time_rec_pos %= FPS_FRAMES;

	tex_fade_debt_pos = (tex_fade_debt_pos+1) % FPS_FRAMES;
	if (g_bTexFading && !g_bTexLocked)
		tex_fade_debt[tex_fade_debt_pos] = avg_texfade_time*0.001f;
	else
		tex_fade_debt[tex_fade_debt_pos] = 0;

	float tex_fade_debt_sum = 0;
	for (int i=0; i<FPS_FRAMES; i++) tex_fade_debt_sum += tex_fade_debt[i];

	if (intframe > FPS_FRAMES*3) 
	{
		// damp it just a little bit
		fps        = (float)FPS_FRAMES / (fTime - prev_time_rec - tex_fade_debt_sum);	
		honest_fps = (float)FPS_FRAMES / (fTime - prev_time_rec);	

		// adjust sleepy time
		float spf_now = 1.0f/fps;
		float spf_des = 1.0f/(float)limit_fps;
		sleep_amount += 0.05f/fps*(spf_des - spf_now)*1000.0f;	// it will adjust by up to 0.1x the difference, every *second* of animation.
	}
	else if (intframe == FPS_FRAMES*2)
	{
		fps        = (float)FPS_FRAMES / (fTime - prev_time_rec - tex_fade_debt_sum);
		honest_fps = (float)FPS_FRAMES / (fTime - prev_time_rec);	

		// initially set sleepy time.,,
		float spf_now = 1.0f/fps;
		float spf_des = 1.0f/(float)limit_fps;
		sleep_amount = (spf_des - spf_now)*1000.0f;

		fps        = 0;	// so it still reports 'calibrating' instead of the initial high-speed fps.
		honest_fps = 0;
	}
	else 
	{
		fps        = 0;
		honest_fps = 0;
	}

	if (g_bRandomizeTextures)
	{
		g_bRandomizeTextures = false;
		g_bTexLocked = false;
		g_bTexFading = false;
		g_fTexStartTime = fTime;

		char *s = TEX.GetRandomFilename();
		if (s==NULL)
		{
			// load up the built-in texture
			//TEX.LoadBuiltInTex256(0);
		}
		else if (!TEX.LoadTex256(s, 0, g_bResize, g_bAutoBlend, g_iBlendPercent))
		{
			char *s2 = strrchr(s, '\\');
			if (s2 == NULL) 
				s2 = s; 
			else
				s2++;
			sprintf(szTEXLOADFAIL, "BAD IMAGE: %s", s2);
			SHOW_TEXLOADFAIL = 30;
		}				
	}


	if (fTime - g_fTexStartTime > time_between_textures)
	{
		if (fTime - g_fTexStartTime < time_between_textures + texture_fade_time)
		{
			// fade
			if (!g_bTexFading)
			{
				// start fading
				g_bTexFading = true;
				TEX.SwapTex(0, 2);

				char *s = TEX.GetRandomFilename();
				if (!TEX.LoadTex256(s, 1, g_bResize, g_bAutoBlend, g_iBlendPercent))
				{
					char *s2 = strrchr(s, '\\');
					if (s2 == NULL) 
						s2 = s; 
					else
						s2++;

					if (g_nSubdirIndex != -1)
						sprintf(szTEXNAME, " %d\\%s ", g_subdirIndices[g_nSubdirIndex], s2);
					else 
						sprintf(szTEXNAME, " %s ", s2);               							
				}

			}

			// continue fading
			if (!g_bTexLocked)
			{
				static int sampleframes = 1;
				DWORD a = GetTickCount();
				TEX.BlendTex(2, 1, 0, (fTime - g_fTexStartTime - time_between_textures)/texture_fade_time);
				DWORD b = GetTickCount();

				if (sampleframes == 1)
				{
					avg_texfade_time = (float)(b - a + 1);
				}
				else if (sampleframes < 50)
				{
					avg_texfade_time *= (float)(sampleframes-1) / (float)sampleframes;
					avg_texfade_time += (float)(b - a + 1) / (float)sampleframes;
				}
				else
				{
					avg_texfade_time = avg_texfade_time*0.98f + 0.02f*(b - a + 1);
				}

				sampleframes++;
			}
		}
		else
		{
			// done fading
			g_bTexFading = false;
			g_fTexStartTime = fTime;
			TEX.SwapTex(0, 1);
		}
	}


	static float fAnimTime = 0;
	float fmult = anim_speed;
	fmult *= 0.75f;
	fmult *= powf(8.0f, 1.0f - speed_scale*0.1f);
	if (rendermode == 3)
		fmult *= 0.8f;
	fAnimTime += fDeltaT * fmult;

	if (TEX.tex[0] != NULL)//TEX.texW[0] == 256 && TEX.texH[0] == 256)
	{
		float intframe2 = fAnimTime*22.5f;
		float scale = 0.45f + 0.1f*sinf(intframe2*0.01f);

		float rot = fAnimTime*rotational_speed*6.28f;

		float eye_x = 0.5f + 0.4f*sinf(fAnimTime*0.054f) + 0.4f*sinf(fAnimTime*0.0054f);
		float eye_y = 0.5f + 0.4f*cosf(fAnimTime*0.047f) + 0.4f*cosf(fAnimTime*0.0084f);

		float ut, vt;
		int i, j;

		memset(cell, 0, sizeof(td_cellcornerinfo)*(UVCELLSX)*(UVCELLSY));



		WORD wOldCW;
		BOOL bChangedFPCW = MungeFPCW( &wOldCW );

#define NUM_MODES 7

		float fCoeffTime;
		if (g_bCoeffsFrozen)
			fCoeffTime = g_fCoeffFreezeTime;
		else
			fCoeffTime = fAnimTime - g_fCoeffFreezeDebt;

		float t[NUM_MODES];
		t[0] = powf(0.50f + 0.50f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.1216f + fRandStart1), 1.0f);
		t[1] = powf(0.48f + 0.48f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.0625f + fRandStart2), 2.0f);
		t[2] = powf(0.45f + 0.45f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.0253f + fRandStart3), 12.0f);
		t[3] = powf(0.50f + 0.50f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.0916f + fRandStart4), 2.0f);
		t[4] = powf(0.50f + 0.50f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.0625f + fRandStart1), 2.0f);
		t[5] = powf(0.70f + 0.50f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.0466f + fRandStart2), 1.0f);
		t[6] = powf(0.50f + 0.50f*sinf(fCoeffTime*mode_switch_speed_multiplier * 0.0587f + fRandStart3), 2.0f);
		//t[(intframe/120) % NUM_MODES] += 20.0f;

		// normalize
		float sum = 0.0f;
		for (i=0; i<NUM_MODES; i++) sum += t[i]*t[i];
		float mag = 1.0f/sqrtf(sum);
		for (i=0; i<NUM_MODES; i++) t[i] *= mag;

		// keep top dog at 1.0, and scale all others down by raising to some exponent
		for (i=0; i<NUM_MODES; i++) t[i] = powf(t[i], mode_focus_exponent);

		// bias t[1] by bass (stomach)
		//t[1] += max(0, (bass - 1.1f)*2.5f);

		// bias t[2] by treble (crazy)
		//t[2] += max(0, (treb - 1.1f)*1.5f);

		// give bias to original drempels effect
		t[0] += 0.2f;

		// re-normalize
		sum = 0.0f;
		for (i=0; i<NUM_MODES; i++) sum += t[i]*t[i];
		mag = 1.0f/sqrtf(sum);
		//        if (g_bGeissMode)
		//          mag *= 0.03f;
		for (i=0; i<NUM_MODES; i++) t[i] *= mag;


		// orig: 1.0-4.5... now: 1.0 + 1.15*[0.0...3.0]
		float fscale1 = 1.0f + 1.15f*(powf(2.0f, 1.0f + 0.5f*sinf(fAnimTime*0.892f) + 0.5f*sinf(fAnimTime*0.624f)) - 1.0f);
		float fscale2 = 4.0f + 1.0f*sinf(fRandStart3 + fAnimTime*0.517f) + 1.0f*sinf(fRandStart4 + fAnimTime*0.976f);
		float fscale3 = 4.0f + 1.0f*sinf(fRandStart1 + fAnimTime*0.654f) + 1.0f*sinf(fRandStart1 + fAnimTime*1.044f);
		float fscale4 = 4.0f + 1.0f*sinf(fRandStart2 + fAnimTime*0.517f) + 1.0f*sinf(fRandStart3 + fAnimTime*0.976f);
		float fscale5 = 4.0f + 1.0f*sinf(fRandStart4 + fAnimTime*0.654f) + 1.0f*sinf(fRandStart2 + fAnimTime*1.044f);

		float t3_uc = 0.3f*sinf(0.217f*(fAnimTime+fRandStart1)) + 0.2f*sinf(0.185f*(fAnimTime+fRandStart2));
		float t3_vc = 0.3f*cosf(0.249f*(fAnimTime+fRandStart3)) + 0.2f*cosf(0.153f*(fAnimTime+fRandStart4));
		float t3_rot = 3.3f*cosf(0.1290f*(fAnimTime+fRandStart2)) + 2.2f*cosf(0.1039f*(fAnimTime+fRandStart3));
		float cosf_t3_rot = cosf(t3_rot);
		float sinf_t3_rot = sinf(t3_rot);
		float t4_uc = 0.2f*sinf(0.207f*(fAnimTime+fRandStart2)) + 0.2f*sinf(0.145f*(fAnimTime+fRandStart4));
		float t4_vc = 0.2f*cosf(0.219f*(fAnimTime+fRandStart1)) + 0.2f*cosf(0.163f*(fAnimTime+fRandStart3));
		float t4_rot = 0.61f*cosf(0.1230f*(fAnimTime+fRandStart4)) + 0.43f*cosf(0.1009f*(fAnimTime+fRandStart1));
		float cosf_t4_rot = cosf(t4_rot);
		float sinf_t4_rot = sinf(t4_rot);

		float u_delta = 0.05f;//1.0f/((UVCELLSX-4)/2) * 0.05f; //0.01f;
		float v_delta = 0.05f;//1.0f/((UVCELLSY-4)/2) * 0.05f; //0.01f;

		float u_offset = 0.5f;// + gXC/(float)FXW*4.0f;
		float v_offset = 0.5f;// + gYC/(float)FXH*4.0f;

		for (i=0; i<UVCELLSX; i++)
			for (j=0; j<UVCELLSY; j++)
			{
				float base_u = (i/2*2)/(float)(UVCELLSX-2) - u_offset;
				float base_v = (j/2*2)/(float)(UVCELLSY-2) - v_offset;
				if (i & 1) base_u += u_delta;
				if (j & 1) base_v += v_delta;
				base_v *= -1.0f;

				cell[i][j].u = 0;//fTime*0.4f + base_u;//base_u; 
				cell[i][j].v = 0;//base_v; 


				// correct for aspect ratio:
				base_u *= 1.333f;


				//------------------------------ v1.0 code
				{
					float u = base_u; 
					float v = base_v; 
					u += warp_factor*0.65f*sinf(intframe2*warp_w[0] + (base_u*warp_uscale[0] + base_v*warp_vscale[0])*6.28f + warp_phase[0]); 
					v += warp_factor*0.65f*sinf(intframe2*warp_w[1] + (base_u*warp_uscale[1] - base_v*warp_vscale[1])*6.28f + warp_phase[1]); 
					u += warp_factor*0.35f*sinf(intframe2*warp_w[2] + (base_u*warp_uscale[2] - base_v*warp_vscale[2])*6.28f + warp_phase[2]); 
					v += warp_factor*0.35f*sinf(intframe2*warp_w[3] + (base_u*warp_uscale[3] + base_v*warp_vscale[3])*6.28f + warp_phase[3]); 
					u /= scale;
					v /= scale;

					ut = u;
					vt = v;
					u = ut*cosf(rot) - vt*sinf(rot);
					v = ut*sinf(rot) + vt*cosf(rot);

					// NOTE: THIS MULTIPLIER WAS --2.7-- IN THE ORIGINAL DREMPELS 1.0!!!
					u += 2.0f*sinf(intframe2*0.00613f);
					v += 2.0f*cosf(intframe2*0.0138f);

					cell[i][j].u += u * t[0]; 
					cell[i][j].v += v * t[0]; 
				}
				//------------------------------ v1.0 code

				{
					// stomach
					float u = base_u; 
					float v = base_v; 

					float rad = sqrtf(u*u + v*v);
					float ang = atan2f(u, v);

					rad *= 1.0f + 0.3f*sinf(fAnimTime * 0.53f + ang*1.0f + fRandStart2);
					ang += 0.9f*sinf(fAnimTime * 0.45f + rad*4.2f + fRandStart3);

					u = rad*cosf(ang)*1.7f;
					v = rad*sinf(ang)*1.7f;

					cell[i][j].u += u * t[1]; 
					cell[i][j].v += v * t[1]; 
				}						


				{
					// crazy
					float u = base_u; 
					float v = base_v; 

					float rad = sqrtf(u*u + v*v);
					float ang = atan2f(u, v);

					rad *= 1.0f + 0.3f*sinf(fAnimTime * 1.59f + ang*20.4f + fRandStart3);
					ang += 1.8f*sinf(fAnimTime * 1.35f + rad*22.1f + fRandStart4);

					u = rad*cosf(ang);
					v = rad*sinf(ang);

					cell[i][j].u += u * t[2]; 
					cell[i][j].v += v * t[2]; 
				}

				{
					// rotation
					//float u = (i/(float)UVCELLSX)*1.6f - 0.5f - t3_uc;  
					//float v = (j/(float)UVCELLSY)*1.6f - 0.5f - t3_vc; 
					float u = base_u*1.6f - t3_uc; 
					float v = base_v*1.6f - t3_vc; 
					float u2 = u*cosf_t3_rot - v*sinf_t3_rot + t3_uc;
					float v2 = u*sinf_t3_rot + v*cosf_t3_rot + t3_vc;

					cell[i][j].u += u2 * t[3]; 
					cell[i][j].v += v2 * t[3]; 
				}

				{
					// zoom out & minor rotate (to keep it interesting, if isolated)
					//float u = i/(float)UVCELLSX - 0.5f - t4_uc; 
					//float v = j/(float)UVCELLSY - 0.5f - t4_vc; 
					float u = base_u - t4_uc; 
					float v = base_v - t4_vc; 

					u = u*fscale1 + t4_uc - t3_uc;
					v = v*fscale1 + t4_vc - t3_uc;

					float u2 = u*cosf_t4_rot - v*sinf_t4_rot + t3_uc;
					float v2 = u*sinf_t4_rot + v*cosf_t4_rot + t3_vc;

					cell[i][j].u += u2 * t[4]; 
					cell[i][j].v += v2 * t[4]; 
				}

				{
					// SWIRLIES!
					float u = base_u*1.4f;
					float v = base_v*1.4f;
					float offset = 0;//((u+2.0f)*(v-2.0f) + u*u + v*v)*50.0f;

					float u2 = u + 0.03f*sinf(u*(fscale2 + 2.0f) + v*(fscale3 + 2.0f) + fRandStart4 + fAnimTime*1.13f + 3.0f + offset);
					float v2 = v + 0.03f*cosf(u*(fscale4 + 2.0f) - v*(fscale5 + 2.0f) + fRandStart2 + fAnimTime*1.03f - 7.0f + offset);
					u2 += 0.024f*sinf(u*(fscale3*-0.1f) + v*(fscale5*0.9f) + fRandStart3 + fAnimTime*0.53f - 3.0f);
					v2 += 0.024f*cosf(u*(fscale2*0.9f) + v*(fscale4*-0.1f) + fRandStart1 + fAnimTime*0.58f + 2.0f);

					cell[i][j].u += u2*1.25f * t[5]; 
					cell[i][j].v += v2*1.25f * t[5]; 
				}						


				{
					// tunnel
					float u = base_u*1.4f - t4_vc;
					float v = base_v*1.4f - t4_uc;

					float rad = sqrtf(u*u + v*v);
					float ang = atan2f(u, v);

					u = rad + 3.0f*sinf(fAnimTime*0.133f + fRandStart1) + t4_vc;
					v = rad*0.5f * 0.1f*cosf(ang + fAnimTime*0.079f + fRandStart4) + t4_uc;

					cell[i][j].u += u * t[6]; 
					cell[i][j].v += v * t[6]; 
				}

			}

			float inv_master_zoom = 1.0f / (master_zoom * 1.8f);
			inv_master_zoom *= powf(4.0f, 1.0f - tex_scale*0.1f);
			float int_scalar = 256.0f*(INTFACTOR);
			for (j=0; j<UVCELLSY; j++)
				for (i=0; i<UVCELLSX; i++)
				{
					cell[i][j].u *= inv_master_zoom;
					cell[i][j].v *= inv_master_zoom;
					cell[i][j].u += 0.5f; 
					cell[i][j].v += 0.5f; 
					cell[i][j].u *= int_scalar;
					cell[i][j].v *= int_scalar;
				}

				for (j=0; j<UVCELLSY; j++)
					for (i=0; i<UVCELLSX-1; i+=2)
					{
						cell[i][j].r = (cell[i+1][j].u - cell[i][j].u) / (u_delta*FXW2);
						cell[i][j].s = (cell[i+1][j].v - cell[i][j].v) / (v_delta*FXW2);
					}

					for (j=0; j<UVCELLSY-1; j+=2)
						for (i=0; i<UVCELLSX; i+=2)
						{
							cell[i][j].dudy = (cell[i][j+1].u - cell[i][j].u) / (u_delta*FXH2);
							cell[i][j].dvdy = (cell[i][j+1].v - cell[i][j].v) / (v_delta*FXH2);
							cell[i][j].drdy = (cell[i][j+1].r - cell[i][j].r) / (u_delta*FXH2);
							cell[i][j].dsdy = (cell[i][j+1].s - cell[i][j].s) / (v_delta*FXH2);
						}

						g_fBlurAmount = 0.97f*powf(motion_blur*0.1f, 0.27f);
						float src_scale = (1.0f-g_fBlurAmount)*255.0f;
						float dst_scale = g_fBlurAmount*255.0f;

						if ( bChangedFPCW )
							RestoreFPCW( wOldCW );

						IDirect3DSurface9* surface1, *surface2;
						D3DLOCKED_RECT lock1, lock2;

						lpD3DVS[intframe%2]->GetSurfaceLevel(0, &surface1);
						surface1->LockRect(&lock1, NULL, 0);
						lpD3DVS[(intframe+1)%2]->GetSurfaceLevel(0, &surface2);
						surface2->LockRect(&lock2, NULL, 0);

						if (TEX.texW[0]==256 && TEX.texH[0]==256)
						{
							for (j=0; j<UVCELLSY-2; j+=2)
								for (i=0; i<UVCELLSX-2; i+=2) 
									BlitWarp256AndMix(cell[i][j], cell[i+2][j], cell[i][j+2], cell[i+2][j+2],
									(i)*FXW2/(UVCELLSX-2), 
									(j)*FXH2/(UVCELLSY-2), 
									(i+2)*FXW2/(UVCELLSX-2), 
									(j+2)*FXH2/(UVCELLSY-2), 
									(unsigned char *)lock1.pBits,
									(int)dst_scale, 
									(int)src_scale,
									(unsigned char *)lock2.pBits, 
									FXW2, 
									FXH2,	
									TEX.tex[0], 
									high_quality);
						}




						surface2->UnlockRect();
						surface2->Release();
						surface1->UnlockRect();
						surface1->Release(); 


	}


	g_bForceRepaintOnce = false;



	g_pd3dDevice->SetTexture(0, lpD3DVS[(intframe+1)%2]);

	float x = 0;
	float y = 0;
	float sizeX = (float)g_width;
	float sizeY = (float)g_height;
	int col = 0xffffffff;

	TEXVERTEX	v[4];

	v[0].x = x;
	v[0].y = y;
	v[0].z = 0;
	v[0].w = 1.0f;
	v[0].colour = col;
	v[0].u = 0;
	v[0].v = 0;

	v[1].x = x + sizeX;
	v[1].y = y;
	v[1].z = 0;
	v[1].w = 1.0f;
	v[1].colour = col;
	v[1].u = 1.0f;//(float)FXW2;
	v[1].v = 0;

	v[2].x = x + sizeX;
	v[2].y = y + sizeY;
	v[2].z = 0;
	v[2].w = 1.0f;
	v[2].colour = col;
	v[2].u = 1.0f;//(float)FXW2;
	v[2].v = 1.0f;//(float)FXH2;

	v[3].x = x;
	v[3].y = y + sizeY;
	v[3].z = 0;
	v[3].w = 1.0f;
	v[3].colour = col;
	v[3].u = 0;
	v[3].v = 1.0f;//(float)FXH2;

	g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	g_pd3dDevice->SetFVF(D3DFVF_TEXVERTEX);
	g_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLEFAN, 2, v, sizeof(TEXVERTEX));	

}

void RefreshFileListings()
{
	// also check on existence of the 9 subdirs
	g_nSubdirs = 0;
	g_nSubdirIndex = -1;
	for (int i=1; i<=9; i++) {
		char buf[MAX_PATH];
    sprintf(buf, "%s\\%s\\%d", g_strPresetsDir.c_str(), "resources/textures", i);
		if (GetFileAttributes(buf) != -1)
			g_subdirIndices[g_nSubdirs++] = i;
	}
}

BOOL DrempelsInit()
{ 
	sprintf((char*)&szTexPath, "%s\\%s", g_strPresetsDir.c_str(), "resources/textures");	

	ScrMode = smSaver;

	// first get TGA file listing!
	TEX.EnumTgaAndBmpFiles(szTexPath);

	RefreshFileListings();


	if (!doInit())
		return FALSE;

	mainproc_start_time = GetTickCount();

	RandomizeStartValues();

	return TRUE;
}

//--------------------------------------------------------------
//--------------------------------------------------------------

void FX_Init()
{
	memset(tex_fade_debt, 0, sizeof(float)*FPS_FRAMES);
	memset(time_rec, 0, sizeof(float)*FPS_FRAMES);

	srand((unsigned int)time(NULL) + (rand()%256));

	gXC = FXW/2;
	gYC = FXH/2;
}

BOOL doInit()
{

	// now determine FXW2, FXH2
	switch(procsize)
	{
	case 0:
		FXW2 = FXW;
		FXH2 = FXH;
		break;
	case 1:
		FXW2 = FXW/2;
		FXH2 = FXH/2;
		break;
	case 2:
		FXW2 = FXW/4;
		FXH2 = FXH/4;
		break;
	default:
		procsize = 0;
		FXW2 = FXW;
		FXH2 = FXH;
		break;
	}
	
	FX_Init();

	if(FAILED(g_pd3dDevice->CreateTexture(FXW2, FXH2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &lpD3DVS[0], NULL)))
		return FALSE;

	if(FAILED(g_pd3dDevice->CreateTexture(FXW2, FXH2, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &lpD3DVS[1], NULL)))
		return FALSE;

	IDirect3DSurface9* surface1, *surface2;
	D3DLOCKED_RECT lock1, lock2;

	if (FAILED(lpD3DVS[0]->GetSurfaceLevel(0, &surface1)))
		return FALSE;

	if (FAILED(surface1->LockRect(&lock1, NULL, 0)))
		return FALSE;

	if (FAILED(lpD3DVS[1]->GetSurfaceLevel(0, &surface2)))
		return FALSE;

	if (FAILED(surface2->LockRect(&lock2, NULL, 0)))
		return FALSE;

	memset(lock1.pBits, 0, FXW2 * FXH2 * 4);
	memset(lock2.pBits, 0, FXW2 * FXH2 * 4);
	surface2->UnlockRect();
	surface2->Release();
	surface1->UnlockRect();
	surface1->Release();

	return TRUE;  // all systems go.
}

void DrempelsExit()
{
	if (lpD3DVS[0] != NULL)
		lpD3DVS[0]->Release();

	if (lpD3DVS[1] != NULL)
		lpD3DVS[1]->Release();
}















