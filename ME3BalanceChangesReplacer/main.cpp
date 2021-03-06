#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include "zlib.h"
#include <stdio.h>

BYTE pattern[] = { 0x8B, 0x07, 0x8B, 0x4F, 0x04, 0x83, 0xC7, 0x04 };
BYTE *compressedCoalesced;
DWORD locReturn;

__declspec(naked) void UseNewCoalesced()
{
	__asm
	{
		mov edi,[compressedCoalesced]
		add edi, 4
		mov eax,[edi]
		mov ecx,[edi+4]
		jmp locReturn
	}
}

bool DataCompare(const BYTE* OpCodes, const BYTE* Mask, const char* StrMask)  
{  
	while (*StrMask)  
	{  
		if(*StrMask == 'x' && *OpCodes != *Mask )  
			return false;  
		++StrMask;  
		++OpCodes;  
		++Mask;  
	}  
	return true;
} 

DWORD FindPattern(DWORD StartAddress, DWORD CodeLen, BYTE* Mask, char* StrMask, unsigned short ignore)  
{  
	unsigned short Ign = 0;  
	DWORD i = 0;  
	while (Ign <= ignore)  
	{  
		if(DataCompare((BYTE*)(StartAddress + i++), Mask, StrMask)) 
			++Ign;  
		else if (i>=CodeLen)  
			return 0;  
	} 
	return StartAddress + i - 1;  
}

DWORD WINAPI Start(LPVOID lpParam)
{
	DWORD codeloc, dwProtect;
	codeloc = FindPattern(0x401000, 0x1500000, pattern, "xxxxxxxx", 0);
	// if byte pattern is not found, exit
	if(!codeloc)
	{
		return 0;
	}
	FILE * coalescedFile = fopen("ServerCoalesced.bin", "rb");
	// if the file is not in the current folder, look for it in ASI subfolder
	if(!coalescedFile)
	{
		coalescedFile = fopen("ASI\\ServerCoalesced.bin", "rb");
		//  if the file is not in the ASI subfolder, exit
		if(!coalescedFile)
		{
			return 0;
		}
	}
	fseek(coalescedFile, 0, SEEK_END );
	int coalescedLen = ftell(coalescedFile);
	rewind(coalescedFile);
	BYTE *coalescedData = (BYTE*)malloc(coalescedLen);
	fread(coalescedData, coalescedLen, 1, coalescedFile);
	fclose(coalescedFile);
	// "NIBC" + (int)1 + (int)compressedSize + (int)coalescedLen = 16 bytes
	compressedCoalesced = (BYTE*)malloc(16 + coalescedLen); // we assume the compressed coalesced will be smaller than it's uncompressed size
	*(int*)&compressedCoalesced[0] = 0x4342494E; // "CBIN" as little-endian 32-bit number
	*(int*)&compressedCoalesced[4] = 1;
	*(int*)&compressedCoalesced[12] = coalescedLen;
	uLong compressedSize = compressBound(coalescedLen);
	int compressionResult = compress2(&compressedCoalesced[16], &compressedSize, coalescedData, coalescedLen, 6);
	// if compression failed, exit
	if(compressionResult != Z_OK)
	{
		free (coalescedData);
		return 0;
	}
	*(int*)&compressedCoalesced[8] = (int)compressedSize;
	// now that everything is ready, we can patch the game code to jump to our custom function :)
	locReturn = codeloc + 5;
	VirtualProtect((void*)codeloc, 0x5, PAGE_READWRITE, &dwProtect);
	BYTE* p = (BYTE*)codeloc;
	*p++ = 0xE9;
	*(DWORD*)p = (unsigned long)&UseNewCoalesced - (locReturn);
	VirtualProtect((void*)codeloc, 0x5, dwProtect, &dwProtect);
	free (coalescedData);
	return 0;
}

BOOL WINAPI DllMain(HINSTANCE hInst,DWORD reason,LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{			
		DWORD dwThreadId, dwThrdParam = 1;
		HANDLE hThread;
		hThread = CreateThread(NULL,0, Start, &dwThrdParam, 0, &dwThreadId);
	}
	return 1;
}