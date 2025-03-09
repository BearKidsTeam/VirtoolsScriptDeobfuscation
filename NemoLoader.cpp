#include "NemoLoader.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

#include "Decorator.h"

#define READER_COUNT 4

/*************************************************************************
 PLUGIN DECLARATION
**************************************************************************/
CKPluginInfo g_PluginInfo[READER_COUNT] = {
    CKPluginInfo(VIRTOOLS_COMPOSITION_READER_GUID,           // GUID
                 "Cmo",                                      // Extension supported
                 "Virtools Composition",                     // Reader Name
                 "Virtools",                                 // Company
                 "Virtools Plugin (Compositions)",           // Summary String
                 VIRTOOLS_COMPOSITION_READER_VERSION,        // Reader Version
                 nullptr,                                    // No Init Instance function needed
                 nullptr,                                    // No Exit Instance function needed
                 CKPLUGIN_MODEL_READER),                     // Plugin Type : Model Reader
    CKPluginInfo(VIRTOOLS_OBJECT_READER_GUID,                // GUID
                 "Nmo",                                      // Extension supported
                 "Virtools Object",                          // Reader Name
                 "Virtools",                                 // Company
                 "Virtools Plugin (Object)",                 // Summary String
                 VIRTOOLS_OBJECT_READER_VERSION,             // Reader Version
                 nullptr,                                    // No Init Instance function needed
                 nullptr,                                    // No Exit Instance function needed
                 CKPLUGIN_MODEL_READER),                     // Plugin Type : Model Reader
    CKPluginInfo(VIRTOOLS_BEHAVIORS_READER_GUID,             // GUID
                 "Nms",                                      // Extension supported
                 "Virtools Behaviors Graph/Script",          // Reader Name
                 "Virtools",                                 // Company
                 "Virtools Plugin (Behaviors Graph/Script)", // Summary String
                 VIRTOOLS_BEHAVIORS_READER_VERSION,          // Reader Version
                 nullptr,                                    // No Init Instance function needed
                 nullptr,                                    // No Exit Instance function needed
                 CKPLUGIN_MODEL_READER),                     // Plugin Type : Model Reader
    CKPluginInfo(VIRTOOLS_PLAYER_READER_GUID,                // GUID
                 "Vmo",                                      // Extension supported
                 "Virtools Player",                          // Reader Name
                 "Virtools",                                 // Company
                 "Virtools Plugin (Player)",                 // Summary String
                 VIRTOOLS_PLAYER_READER_VERSION,             // Reader Version
                 nullptr,                                    // No Init Instance function needed
                 nullptr,                                    // No Exit Instance function needed
                 CKPLUGIN_MODEL_READER),                     // Plugin Type : Model Reader
};

/**********************************************
 Called by the engine when a file with the CMO,
 NMO, NMS, and VMO extension is being loaded,
 a reader has to be created.
***********************************************/
PLUGIN_EXPORT CKDataReader *CKGetReader(int pos) {
    return new NemoLoader;
}

/******************************************
 Returns number of the reader which is the
 same as given to the engine at
 initialisation.
*******************************************/
PLUGIN_EXPORT int CKGetPluginInfoCount() {
    return READER_COUNT;
}

/******************************************
 Returns information about the reader which
 is the same as given to the engine at
 initialisation.
*******************************************/
PLUGIN_EXPORT CKPluginInfo *CKGetPluginInfo(int index) {
    return &g_PluginInfo[index];
}

class CKContextHook : public CKContext {
public:
    typedef CKERROR (CKContext::*GetFileInfoFunc)(int, void *, CKFileInfo *);

    CKERROR GetFileInfoHook(int BufferSize, void *MemoryBuffer, CKFileInfo *FileInfo) {
        CKERROR err = (this->*s_GetFileInfoFuncOrig)(BufferSize, MemoryBuffer, FileInfo);
        FileInfo->FileWriteMode &= ~CKFILE_FORVIEWER;
        return err;
    }

    static bool Hook() {
        HMODULE handle = ::GetModuleHandleA("CK2.dll");
#if CKVERSION == 0x13022002
        FARPROC lpGetFileInfoProc = ::GetProcAddress(handle, "?GetFileInfo@CKContext@@QAEJHPAXPAUCKFileInfo@@@Z");
#else
        FARPROC lpGetFileInfoProc = ::GetProcAddress(handle, "?GetFileInfo@CKContext@@QAEHHPAXPAUCKFileInfo@@@Z");
#endif
        if (!lpGetFileInfoProc)
            return false;

        s_GetFileInfoFuncTarget = *(GetFileInfoFunc *) &lpGetFileInfoProc;

        if (MH_CreateHook(*(LPVOID *) &s_GetFileInfoFuncTarget, *(LPVOID *) &s_GetFileInfoFunc,
                          (LPVOID *) &s_GetFileInfoFuncOrig) != MH_OK ||
            MH_EnableHook(*(LPVOID *) &s_GetFileInfoFuncTarget) != MH_OK)
            return false;

        s_Hooked = true;
        return true;
    }

    static void Unhook() {
        if (s_Hooked) {
            MH_DisableHook(*(LPVOID *) &s_GetFileInfoFuncTarget);
            MH_RemoveHook(*(LPVOID *) &s_GetFileInfoFuncTarget);
            s_Hooked = false;
        }
    }

    static bool s_Hooked;
    static GetFileInfoFunc s_GetFileInfoFunc;
    static GetFileInfoFunc s_GetFileInfoFuncOrig;
    static GetFileInfoFunc s_GetFileInfoFuncTarget;
};

bool CKContextHook::s_Hooked = false;
CKContextHook::GetFileInfoFunc CKContextHook::s_GetFileInfoFunc = (GetFileInfoFunc) &CKContextHook::GetFileInfoHook;
CKContextHook::GetFileInfoFunc CKContextHook::s_GetFileInfoFuncOrig = nullptr;
CKContextHook::GetFileInfoFunc CKContextHook::s_GetFileInfoFuncTarget = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD fdwReason, LPVOID lpReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        if (MH_Initialize() == MH_OK)
            CKContextHook::Hook();
    } else if (fdwReason == DLL_PROCESS_DETACH) {
        CKContextHook::Unhook();
        MH_Uninitialize();
    }
    return TRUE;
}

/*************************************************************************
 PLUGIN IMPLEMENTATION
**************************************************************************/

CKPluginInfo *NemoLoader::GetReaderInfo() {
    return g_PluginInfo;
}

CKERROR NemoLoader::Load(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CKDWORD LoadFlags,
                         CKCharacter *carac) {
    if (!liste)
        return CKERR_INVALIDPARAMETER;

    CKFile *file = context->CreateCKFile();
    if (!file)
        return CKERR_OUTOFMEMORY;

    CKERROR err = file->Load(FileName, liste, (CK_LOAD_FLAGS) LoadFlags);
    if (err != CK_OK) {
        context->DeleteCKFile(file);
        return err;
    }

    CreateInterfaceChunks(context, liste);

    file->UpdateAndApplyAnimationsTo(carac);
    context->DeleteCKFile(file);
    return CK_OK;
}

CKERROR NemoLoader::Save(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CKDWORD SaveFlags) {
    if (!liste)
        return CKERR_INVALIDPARAMETER;

    CKFile *file = context->CreateCKFile();
    if (!file)
        return CKERR_OUTOFMEMORY;

    file->StartSave(FileName);
    file->SaveObjects(liste);
    CKERROR err = file->EndSave();

    context->DeleteCKFile(file);
    return err;
}

CKERROR NemoLoader::CreateInterfaceChunks(CKContext *context, CKObjectArray *list) {
    if (!list)
        return CKERR_INVALIDPARAMETER;

    CKERROR err = CK_OK;

    list->Reset();
    while (!list->EndOfList()) {
        CKObject *obj = list->GetData(context);
        if (CKIsChildClassOf(obj, CKCID_BEHAVIOR)) {
            CKBehavior *beh = (CKBehavior *) list->GetData(context);
            if (beh->GetType() & CKBEHAVIORTYPE_SCRIPT) {
                if (!beh->GetInterfaceChunk()) {
                    err = CreateInterfaceChunk(beh);
                    if (err == CK_OK) {
                        context->OutputToConsoleEx("Generated interface chunk for <%s>", beh->GetName());
                    } else {
                        context->OutputToConsoleEx("Error generating interface chunk for <%s>", beh->GetName());
                    }
                }
            }
        }
        list->Next();
    }
    list->Reset();

    return CK_OK;
}

CKERROR NemoLoader::CreateInterfaceChunk(CKBehavior *beh) {
    if (!beh)
        return CKERR_INVALIDPARAMETER;

    // Create interface data
    InterfaceData interfaceData;

    // Decorate the behavior
    Decorate(interfaceData, beh);

    CKStateChunk *chunk = GenerateInterfaceChunk(interfaceData, beh);
    if (!chunk) {
        return CKERR_INVALIDPARAMETER;
    }

    beh->SetInterfaceChunk(chunk);
    return CK_OK;
}
