// stdafx.cpp : source file that includes just the standard includes
//	VirtoolsScriptDeobfuscation.pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "precomp.h"
#include <cstring>
#include <filesystem>
#include <Windows.h>

char* VSDTempFolderGenerator = nullptr;
char* VSDTempFolderParser = nullptr;
void InitVSDTempFolder(CKContext* ctx) {
	FreeVSDTempFolder();	// free first

	auto pathmgr = ctx->GetPathManager();
	assert(pathmgr != nullptr);

	// WARNING: it seems that Virtools will not cleanup temp folder
	// when crash or terminated by Visual Studio.
	// Programmer may need manually clean it.
	std::filesystem::path temp(pathmgr->GetVirtoolsTemporaryFolder().CStr());
	std::string cache;
	temp /= "VSD";

	auto generator = temp / "generator";
	cache = generator.string();
	VSDTempFolderGenerator = new char[cache.size() + 1];
	std::memcpy(VSDTempFolderGenerator, cache.c_str(), cache.size() + 1);
	std::filesystem::create_directories(generator);

	auto parser = temp / "parser";
	cache = parser.string();
	VSDTempFolderParser = new char[cache.size() + 1];
	std::memcpy(VSDTempFolderParser, cache.c_str(), cache.size() + 1);
	std::filesystem::create_directories(parser);

	ctx->OutputToConsoleEx("VirtoolsScriptDeobfuscation: Log path is \"%s\"", temp.string().c_str());
	OutputDebugStringA("Log path: ");
	OutputDebugStringA(temp.string().c_str());
	OutputDebugStringA("\n");
}
void FreeVSDTempFolder(void) {
	if (VSDTempFolderGenerator != nullptr) delete[] VSDTempFolderGenerator;
	if (VSDTempFolderParser != nullptr) delete[] VSDTempFolderParser;

	VSDTempFolderGenerator = VSDTempFolderParser = nullptr;
}
