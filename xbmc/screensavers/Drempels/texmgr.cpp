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

#include "xbsBase.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <io.h>
#include <string.h>
#include "texmgr.h"

extern LPDIRECT3DDEVICE9 g_pd3dDevice;

texmgr::texmgr()
{
	for (int i=0; i<NUM_TEX; i++)
	{
		tex[i] = NULL;
		orig_tex[i] = NULL;
	}

	iNumFiles = 0;
	files = NULL;
}

texmgr::~texmgr()
{
	for (int i=0; i<NUM_TEX; i++)
	{
		if (orig_tex[i])
		{
			delete orig_tex[i];
			tex[i] = NULL;
			orig_tex[i] = NULL;
		}
	}

	while (files)
	{
		td_filenode *pNode = files->next;
		if (files->szFilename) delete files->szFilename;
		delete files;
		files = pNode;
	}
}


char* texmgr::GetRandomFilename()
{
	if (!files) return NULL;
	if (iNumFiles == 0) return NULL;

	int iFileNum = rand() % iNumFiles;
	int i;
	td_filenode *pNode = files;

	for (i = 0; i < iFileNum; i++)
	{
		if (!pNode) return NULL;
		pNode = pNode->next;
	}

	if (!pNode) return NULL;

	return pNode->szFilename;
}


bool texmgr::EnumTgaAndBmpFiles(char *szFileDir)
{
	struct _finddata_t c_file;
	long hFile;

	char *szMask = new char[strlen(szFileDir) + 32];
	char *szPath = new char[strlen(szFileDir) + 32];

	strcpy(szMask, szFileDir);
	if (szMask[strlen(szMask)-1] != '\\')	
		strcat(szMask, "\\*.tga");
	else
		strcat(szMask, "*.tga");

	strcpy(szPath, szFileDir);
	if (szPath[strlen(szPath)-1] != '\\') 
	{
		strcat(szPath, "\\");
	}

	// clean up old list
	while (files)
	{
		td_filenode *pNode = files->next;
		if (files->szFilename) delete files->szFilename;
		delete files;
		files = pNode;
	}
	iNumFiles = 0;

	/* Find first .TGA file */
	if( (hFile = _findfirst(szMask, &c_file )) != -1L )
	{
		// start new list
		if (files == NULL)
		{
			files = new td_filenode;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			files->next = NULL;
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}
		else
		{
			td_filenode *temp = new td_filenode;
			temp->next = files;
			files = temp;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}

		/* Find the rest of the .BMP files */
		while( _findnext( hFile, &c_file ) == 0 )
		{
			td_filenode *temp = new td_filenode;
			temp->next = files;
			files = temp;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}

		_findclose( hFile );
	}

	strcpy(szMask, szFileDir);
	if (szMask[strlen(szMask)-1] != '\\')	
		strcat(szMask, "\\*.bmp");
	else
		strcat(szMask, "*.bmp");

	/* Find first .BMP file */
	if( (hFile = _findfirst(szMask, &c_file )) != -1L )
	{
		td_filenode *pNode = files;

		// start new list
		if (files == NULL)
		{
			files = new td_filenode;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			files->next = NULL;
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}
		else
		{
			td_filenode *temp = new td_filenode;
			temp->next = files;
			files = temp;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}

		/* Find the rest of the .BMP files */
		while( _findnext( hFile, &c_file ) == 0 )
		{
			td_filenode *temp = new td_filenode;
			temp->next = files;
			files = temp;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}

		_findclose( hFile );
	}

	strcpy(szMask, szFileDir);
	if (szMask[strlen(szMask)-1] != '\\')	
		strcat(szMask, "\\*.jpg");
	else
		strcat(szMask, "*.jpg");

	/* Find first .JPG file */
	if( (hFile = _findfirst(szMask, &c_file )) != -1L )
	{
		td_filenode *pNode = files;

		// start new list
		if (files == NULL)
		{
			files = new td_filenode;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			files->next = NULL;
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}
		else
		{
			td_filenode *temp = new td_filenode;
			temp->next = files;
			files = temp;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}

		/* Find the rest of the .JPG files */
		while( _findnext( hFile, &c_file ) == 0 )
		{
			td_filenode *temp = new td_filenode;
			temp->next = files;
			files = temp;
			files->szFilename = new char[strlen(c_file.name) + strlen(szPath) + 1];
			strcpy(files->szFilename, szPath);
			strcat(files->szFilename, c_file.name);
			iNumFiles++;
		}

		_findclose( hFile );
	}

	// no files found case:
	delete szMask;
	delete szPath;
	return (iNumFiles != 0);    // false if no files found
}


void texmgr::SwapTex(int s1, int s2)
{
	unsigned char *pOrig = orig_tex[s1];
	unsigned char *pTex  = tex[s1];
	unsigned int iW      = texW[s1];
	unsigned int iH      = texH[s1];

	orig_tex[s1] 	= orig_tex[s2];
	tex[s1] 		= tex[s2];
	texW[s1] 		= texW[s2];
	texH[s1] 		= texH[s2];

	orig_tex[s2]	= pOrig;
	tex[s2]			= pTex;
	texW[s2]		= iW;
	texH[s2]		= iH;
}

void texmgr::BlendTex(int src1, int src2, int dest, float t)
{
	if (tex[src1] && 
		tex[src2] && 
		(texW[src1] == texW[src2]) &&
		(texH[src1] == texH[src2]))
	{
		int end = texW[src1]*texH[src1];
		int m1 = (int)((1.0f-t)*255);
		int m2 = (int)(t*255);

		if (tex[dest] && 
			(texW[src1] != texW[dest] || texH[src1] != texH[dest]))
		{
			delete orig_tex[dest];
			orig_tex[dest] = NULL;
			tex[dest] = NULL;
		}

		if (!tex[dest])
		{
			orig_tex[dest] = new unsigned char[ texW[src1]*texH[src1]*4 + 16 ];
			tex[dest] = orig_tex[dest];
			if (((unsigned long)(orig_tex[dest])) % 8 != 0)
			{
				//align tex buffer to 8-byte boundary
				tex[dest] = (unsigned char *)((((unsigned long)(orig_tex[dest]))/8 + 1) * 8);
			}
			texW[dest] = texW[src1];
			texH[dest] = texH[src1];
		}

		unsigned char *s1 = (unsigned char *)tex[src1];
		unsigned char *s2 = (unsigned char *)tex[src2];
		unsigned char *d  = (unsigned char *)tex[dest];

		for (int i=0; i<end; i++)
		{
			*(d++) = (((*s1++) * m1) + ((*s2++) * m2)) >> 8;
			*(d++) = (((*s1++) * m1) + ((*s2++) * m2)) >> 8;
			*(d++) = (((*s1++) * m1) + ((*s2++) * m2)) >> 8;
			*(d++) = (((*s1++) * m1) + ((*s2++) * m2)) >> 8;
		}

	}
}

bool texmgr::LoadTex256(char *szFilename, int iSlot, bool bResize, bool bAutoBlend, int iBlendPercent)
{
	if (iSlot < 0) return false;
	if (iSlot >= NUM_TEX) return false;

	// Allocate memory for image if not already allocated
	if (orig_tex[iSlot] == NULL)
	{
		orig_tex[iSlot] = new unsigned char[ 256*256*4 + 16 ];
		tex[iSlot]      = orig_tex[iSlot];
		if (((unsigned long)(orig_tex[iSlot])) % 8 != 0)
		{
			//align tex buffer to 8-byte boundary
			tex[iSlot] = (unsigned char *)((((unsigned long)(orig_tex[iSlot]))/8 + 1) * 8);
		}
	}

	// done loading new
	texW[iSlot]     = 256;
	texH[iSlot]     = 256;

	LPDIRECT3DTEXTURE9 texture = NULL;
  D3DXCreateTextureFromFileEx(g_pd3dDevice, szFilename, 256, 256, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, D3DX_FILTER_LINEAR, D3DX_FILTER_LINEAR, 0x00000000, NULL, NULL, &texture);
	if (texture)
	{
		IDirect3DSurface9* surface;
		D3DLOCKED_RECT lock;

		texture->GetSurfaceLevel(0, &surface);
		surface->LockRect(&lock, NULL, 0);
		memcpy(tex[iSlot], lock.pBits, 256*256*4);
		surface->UnlockRect();
		surface->Release();
		texture->Release();
	}
	else
	{
		memset(tex[iSlot], 0, 256*256*4);
	}

	return true;
}