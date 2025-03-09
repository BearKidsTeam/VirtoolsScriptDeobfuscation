#ifndef PLUGINS_NEMOLOADER_H
#define PLUGINS_NEMOLOADER_H

#include "CKAll.h"

#define VIRTOOLS_COMPOSITION_READER_VERSION 0x0000001
#define VIRTOOLS_COMPOSITION_READER_GUID CKGUID(0x6B013E56, 0x64BA597E)

#define VIRTOOLS_OBJECT_READER_VERSION 0x0000001
#define VIRTOOLS_OBJECT_READER_GUID CKGUID(0x6117625E, 0x247C3CF2)

#define VIRTOOLS_BEHAVIORS_READER_VERSION 0x0000001
#define VIRTOOLS_BEHAVIORS_READER_GUID CKGUID(0x54CB32FD, 0x17E8715D)

#define VIRTOOLS_PLAYER_READER_VERSION 0x0000001
#define VIRTOOLS_PLAYER_READER_GUID CKGUID(0x28371AAB, 0x6F1A4498)

class NemoLoader : public CKModelReader {
public:
    NemoLoader() = default;
    ~NemoLoader() override = default;

    void Release() override { delete this; };

    CKPluginInfo *GetReaderInfo() override;

    int GetOptionsCount() override { return 0; }
    CKSTRING GetOptionDescription(int i) override { return nullptr; }

    CK_DATAREADER_FLAGS GetFlags() override {
        return (CK_DATAREADER_FLAGS) (CK_DATAREADER_FILELOAD | CK_DATAREADER_FILESAVE);
    }

    CKERROR Load(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CKDWORD LoadFlags, CKCharacter *carac) override;
    CKERROR Save(CKContext *context, CKSTRING FileName, CKObjectArray *liste, CKDWORD SaveFlags) override;

    CKERROR CreateInterfaceChunks(CKContext *context, CKObjectArray *list);
    CKERROR CreateInterfaceChunk(CKBehavior* behavior);
};

#endif // PLUGINS_NEMOLOADER_H
