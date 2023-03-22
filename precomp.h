
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "VxDefines.h"
#include "CKAll.h"

extern char* VSDTempFolderGenerator;
extern char* VSDTempFolderParser;
extern void InitVSDTempFolder(CKContext* ctx);
extern void FreeVSDTempFolder(void);

extern void printfdbg(const char* fmt,...);
