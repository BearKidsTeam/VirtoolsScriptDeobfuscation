#include "NemoLoader.h"

#include <cstdio>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <MinHook.h>

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

CKERROR NemoLoader::Load(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CKDWORD LoadFlags, CKCharacter *carac) {
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

    GenerateInterfaceChunks(context, liste);

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

CKERROR NemoLoader::GenerateInterfaceChunks(CKContext *context, CKObjectArray *list) {
    CKERROR err = CK_OK;
    SchematicNode node;

    list->Reset();
    while (!list->EndOfList()) {
        CKObject *obj = list->GetData(context);
        if (CKIsChildClassOf(obj, CKCID_BEHAVIOR)) {
            CKBehavior *beh = (CKBehavior *) list->GetData(context);
            if (beh->GetType() & CKBEHAVIORTYPE_SCRIPT) {
                node.behavior = beh;
                if (!beh->GetInterfaceChunk()) {
                    err = GenerateInterfaceChunk(node);
                    if (err == CK_OK) {
                        beh->SetInterfaceChunk(node.chunk);
                        context->OutputToConsoleEx((CKSTRING) "Generated interface chunk for <%s>", beh->GetName());
                    }
                }
                node.behavior = nullptr;
                node.chunk = nullptr;
            }
        }
        list->Next();
    }
    list->Reset();

    return CK_OK;
}

extern void decorate(interface_t &data, CKBehavior *bb);

CKERROR NemoLoader::GenerateInterfaceChunk(SchematicNode &node) {
    if (!node.behavior)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = node.behavior->GetCKContext();

    interface_t data = {};
    decorate(data, node.behavior);

    CKStateChunk *chunk = CreateCKStateChunk(-1);
    node.chunk = chunk;
    node.version = 0x16;
    node.scriptIndex = 0;
    node.buildingBlockIndex = 0;

    chunk->StartWrite();

    chunk->WriteIdentifier(1);
    chunk->WriteInt(node.version);
    const int count = data.n_bb + 1;
    chunk->WriteInt(count);

    for (int i = 0; i < count; ++i) {
        if (i != 0) {
            const bb_t &bb = data.bbs[node.buildingBlockIndex];
            node.behavior = (CKBehavior *) context->GetObject(bb.id);
            if (!node.behavior) {
                context->OutputToConsoleEx("Error: Behavior <%s> not found", bb.id);
                return CKERR_NOTFOUND;
            }
        }

        if (SaveScriptHeader(node, data)) {
            if (!(node.flag & 0x8000)) {
                SaveScriptLinks(node, data);
                SaveScriptOps(node, data);
                SaveScriptComments(node, data);

                if (!node.isBuildingBlock)
                    SaveScriptParameters(node, data);

                if (node.isNotScript && !node.isBuildingBlock)
                    SaveScriptGraph(node, data);
            }
        }

        if (i != 0) {
            ++node.buildingBlockIndex;
        }
    }

    SaveScriptExtra(node, data);

    chunk->CloseChunk();

    return CK_OK;
}

CKBOOL NemoLoader::SaveScriptHeader(SchematicNode &node, interface_t &data) {
    CKBehavior *beh = node.behavior;
    if (!beh)
        return FALSE;

    node.isNotScript = (beh->GetType() & CKBEHAVIORTYPE_SCRIPT) == 0;
    node.isBuildingBlock = (beh->GetFlags() & CKBEHAVIOR_BUILDINGBLOCK) != 0;

    CKStateChunk *chunk = node.chunk;

    chunk->WriteObject(beh);

    if (!node.isNotScript) {
        chunk->WriteDword(0); // flag
        chunk->WriteDword(node.scriptIndex++); // index
        chunk->WriteFloat(0.0f); // h_start
        chunk->WriteFloat(data.start.v_start);
        chunk->WriteFloat(data.start.h_start_pos);
        chunk->WriteFloat(data.start.v_start_pos);
        chunk->WriteFloat(data.start.v_size);
        chunk->WriteBitmap(nullptr); // header snapshot
        chunk->WriteDword(0xC8C8C8); // header color
    } else {
        const bb_t &bb = data.bbs[node.buildingBlockIndex];
        chunk->WriteDword(bb.folded ? 0x200 : 0);
        chunk->WriteDword(bb.depth);
        chunk->WriteFloat(bb.size.h_pos);
        chunk->WriteFloat(bb.size.v_pos);
        chunk->WriteFloat(bb.size.h_size);
        chunk->WriteFloat(bb.size.v_size);
        chunk->WriteFloat(bb.h_expand_size);
        chunk->WriteFloat(bb.v_expand_size);
    }

    return TRUE;
}

void NemoLoader::SaveScriptLinks(SchematicNode &node, interface_t &data) {
    CKStateChunk *chunk = node.chunk;

    const bb_t &bb = !node.isNotScript ? data.script_root : data.bbs[node.buildingBlockIndex];

    chunk->WriteInt(bb.links.size());
    for (const auto &link: bb.links) {
        chunk->WriteInt(link.type);
        chunk->WriteObjectID(link.id);
        chunk->WriteObjectID(link.start.id);
        chunk->WriteInt(link.start.index);
        chunk->WriteInt(link.start.type);
        chunk->WriteInt(link.points.size());
        for (const auto &point: link.points) {
            chunk->WriteFloat(point.h);
            chunk->WriteFloat(point.v);
        }
        chunk->WriteObjectID(link.end.id);
        chunk->WriteInt(link.end.index);
        chunk->WriteInt(link.end.type);
    }
}

void NemoLoader::SaveScriptOps(SchematicNode &node, interface_t &data) {
    CKStateChunk *chunk = node.chunk;

    const bb_t &bb = !node.isNotScript ? data.script_root : data.bbs[node.buildingBlockIndex];

    chunk->WriteInt(bb.ops.size());
    for (const auto &op: bb.ops) {
        chunk->WriteObjectID(op.id);
        chunk->WriteFloat(op.h_pos);
        chunk->WriteFloat(op.v_pos);
    }
}

void NemoLoader::SaveScriptComments(SchematicNode &node, interface_t &data) {
    CKStateChunk *chunk = node.chunk;

    // No comments will be generated
    chunk->WriteInt(0);
}

void NemoLoader::SaveScriptParameters(SchematicNode &node, interface_t &data) {
    CKStateChunk *chunk = node.chunk;

    const bb_t &bb = !node.isNotScript ? data.script_root : data.bbs[node.buildingBlockIndex];

    chunk->WriteInt(bb.local_params.size());
    for (const auto &param: bb.local_params) {
        chunk->WriteInt(param.h_pos);
        chunk->WriteInt(param.v_pos);
    }
    for (const auto &param: bb.local_params) {
        chunk->WriteInt(param.style);
    }

    chunk->WriteInt(bb.shared_params.size());
    for (const auto &param: bb.shared_params) {
        chunk->WriteInt(param.h_pos);
        chunk->WriteInt(param.v_pos);
    }
    for (const auto &param: bb.shared_params) {
        chunk->WriteInt(param.style);
    }
    for (const auto &param: bb.shared_params) {
        chunk->WriteObjectID(param.source_id);
    }
}

void NemoLoader::SaveScriptGraph(SchematicNode &node, interface_t &data) {
    CKStateChunk *chunk = node.chunk;

    const bb_t &bb = !node.isNotScript ? data.script_root : data.bbs[node.buildingBlockIndex];

    chunk->WriteInt(bb.inward_inputs.size());
    for (const auto &input: bb.inward_inputs) {
        chunk->WriteInt(input);
        chunk->WriteInt(-1);
    }

    chunk->WriteInt(bb.outward_inputs.size());
    for (const auto &input: bb.outward_inputs) {
        chunk->WriteInt(input);
        chunk->WriteInt(-1);
    }

    chunk->WriteInt(bb.inward_outputs.size());
    for (const auto &output: bb.inward_outputs) {
        chunk->WriteInt(output);
        chunk->WriteInt(1);
    }

    chunk->WriteInt(bb.outward_outputs.size());
    for (const auto &output: bb.outward_outputs) {
        chunk->WriteInt(output);
        chunk->WriteInt(1);
    }
}

void NemoLoader::SaveScriptExtra(SchematicNode &node, interface_t &data) {}
