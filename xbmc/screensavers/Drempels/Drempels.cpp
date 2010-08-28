#include "../../addons/include/xbmc_scr_dll.h"
#include "xbsBase.h"
#include <string>


extern long FXW;
extern long FXH;
int g_width;
int g_height;
std::string m_scrName;
std::string g_strPresetsDir;

LPDIRECT3DDEVICE9 g_pd3dDevice;

BOOL DrempelsInit();
void DrempelsRender();
void DrempelsExit();

extern "C" 
{

//-- Create -------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
// XBMC has loaded us into memory,
// we should set our core values
// here and load any settings we
// may have from our config file
ADDON_STATUS Create(void* hdl, void* props)
{
  if (!props)
    return STATUS_UNKNOWN;
 
  SCR_PROPS* scrprops = (SCR_PROPS*) props;

  m_scrName.assign(scrprops->name);
  g_strPresetsDir.assign(scrprops->presets);

  FXW = 512;
  FXH = 512;
  g_width = scrprops->width;
  g_height = scrprops->height;
  g_pd3dDevice = (LPDIRECT3DDEVICE9) scrprops->device;

  return STATUS_OK;
} // Create

//-- Start --------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void Start()
{
	srand(::GetTickCount());
  DrempelsInit();

} // Start

//-- Render -------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void Render()
{
  g_pd3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 0), 1.0f, 0);
  g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
  g_pd3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_RGBA(255, 255, 255, 255));
  g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
  g_pd3dDevice->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
  g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
  g_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
  g_pd3dDevice->SetPixelShader(NULL);
  g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
  g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
  g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
  g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
  g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

  DrempelsRender();

} // Render

//-- Stop ---------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void Stop()
{
  DrempelsExit();

} // Stop

//-- GetInfo ------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
void GetInfo(SCR_INFO* pInfo)
{
	// not used, but can be used to pass info
	// back to XBMC if required in the future
	return;
}

void Destroy()
{
}

ADDON_STATUS GetStatus()
{
  return STATUS_OK;
}

bool HasSettings()
{
  return false;
}

unsigned int GetSettings(StructSetting ***sSet)
{
  return 0;
}

ADDON_STATUS SetSetting(const char *settingName, const void *settingValue)
{
  return STATUS_OK;
}

void FreeSettings()
{
}

void Remove()
{
}

} // extern "C"
