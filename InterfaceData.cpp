#include "InterfaceData.h"

#include "CKContext.h"
#include "CKBehavior.h"

#include "Decorator.h"

CKERROR InterfaceData::GenerateInterfaceChunk(SchematicNode &node) {
    if (!node.behavior)
        return CKERR_INVALIDPARAMETER;

    CKContext *context = node.behavior->GetCKContext();

    // Create state chunk if not provided
    if (!node.chunk) {
        node.chunk = CreateCKStateChunk(-1);
        if (!node.chunk)
            return CKERR_OUTOFMEMORY;
    }

    // Start writing to the chunk
    node.chunk->StartWrite();

    // Write header information
    node.chunk->WriteIdentifier(1);
    node.chunk->WriteDword(node.version);
    const int count = behaviorBlockCount + 1;
    node.chunk->WriteInt(count);

    // Process each behavior block
    for (int i = 0; i < count; ++i) {
        if (i != 0) {
            const BehaviorBlock &bb = behaviorBlocks[node.buildingBlockIndex];
            node.behavior = (CKBehavior *) context->GetObject(bb.id);
            if (!node.behavior) {
                context->OutputToConsoleEx("Error: Behavior <%s> not found", bb.id);
                return CKERR_NOTFOUND;
            }
        }

        if (SaveScriptHeader(node)) {
            if (!(node.flag & 0x8000)) {
                SaveScriptLinks(node);
                SaveScriptOps(node);
                SaveScriptComments(node);

                if (!node.isBuildingBlock)
                    SaveScriptParameters(node);

                if (node.isNotScript && !node.isBuildingBlock)
                    SaveScriptGraph(node);
            }
        }

        if (i != 0) {
            ++node.buildingBlockIndex;
        }
    }

    SaveScriptExtra(node);

    // Finish writing
    node.chunk->CloseChunk();

    return CK_OK;
}

CKBOOL InterfaceData::SaveScriptHeader(SchematicNode &node) {
    CKBehavior *beh = node.behavior;
    if (!beh)
        return FALSE;

    node.isNotScript = (beh->GetType() & CKBEHAVIORTYPE_SCRIPT) == 0;
    node.isBuildingBlock = (beh->GetFlags() & CKBEHAVIOR_BUILDINGBLOCK) != 0;

    CKStateChunk *chunk = node.chunk;

    // Write behavior object reference
    chunk->WriteObject(beh);

    if (!node.isNotScript) {
        // Save script-specific data
        chunk->WriteDword(0); // flag
        chunk->WriteDword(node.scriptIndex++); // index
        chunk->WriteFloat(0.0f); // h_start
        chunk->WriteFloat(start.vStart);
        chunk->WriteFloat(start.hStartPos);
        chunk->WriteFloat(start.vStartPos);
        chunk->WriteFloat(start.vSize);
        chunk->WriteBitmap(nullptr); // header snapshot
        chunk->WriteDword(0xC8C8C8); // header color
    } else {
        // Save behavior-specific data
        const BehaviorBlock &bb = behaviorBlocks[node.buildingBlockIndex];
        chunk->WriteDword(bb.folded ? 0x200 : 0);
        chunk->WriteDword(bb.depth);
        chunk->WriteFloat(bb.size.hPos);
        chunk->WriteFloat(bb.size.vPos);
        chunk->WriteFloat(bb.size.hSize);
        chunk->WriteFloat(bb.size.vSize);
        chunk->WriteFloat(bb.hExpandSize);
        chunk->WriteFloat(bb.vExpandSize);
    }

    return TRUE;
}

void InterfaceData::SaveScriptLinks(SchematicNode &node) {
    CKStateChunk *chunk = node.chunk;
    const BehaviorBlock &bb = GetBehaviorBlockForNode(node);

    // Write link count and data
    chunk->WriteInt(bb.links.size());
    for (const auto &link : bb.links) {
        chunk->WriteInt(link.type);
        chunk->WriteObjectID(link.id);
        chunk->WriteObjectID(link.start.id);
        chunk->WriteInt(link.start.index);
        chunk->WriteInt(link.start.type);
        chunk->WriteInt(link.points.size());
        for (const auto &point : link.points) {
            chunk->WriteFloat(point.h);
            chunk->WriteFloat(point.v);
        }
        chunk->WriteObjectID(link.end.id);
        chunk->WriteInt(link.end.index);
        chunk->WriteInt(link.end.type);
    }
}

void InterfaceData::SaveScriptOps(SchematicNode &node) {
    CKStateChunk *chunk = node.chunk;
    const BehaviorBlock &bb = GetBehaviorBlockForNode(node);

    // Write operation count and data
    chunk->WriteInt(bb.operations.size());
    for (const auto &op : bb.operations) {
        chunk->WriteObjectID(op.id);
        chunk->WriteFloat(op.hPos);
        chunk->WriteFloat(op.vPos);
    }
}

void InterfaceData::SaveScriptComments(SchematicNode &node) {
    CKStateChunk *chunk = node.chunk;
    const BehaviorBlock &bb = GetBehaviorBlockForNode(node);

    // Write comment count and data
    chunk->WriteInt(bb.comments.size());
    for (const auto &comment : bb.comments) {
        // Write comment data - implementation depends on comment structure
        // Currently empty since the original implementation had no comments
    }
}

void InterfaceData::SaveScriptParameters(SchematicNode &node) {
    CKStateChunk *chunk = node.chunk;
    const BehaviorBlock &bb = GetBehaviorBlockForNode(node);

    // Save local parameters
    chunk->WriteInt(bb.localParams.size());
    for (const auto &param : bb.localParams) {
        chunk->WriteInt(param.hPos);
        chunk->WriteInt(param.vPos);
    }
    for (const auto &param : bb.localParams) {
        chunk->WriteInt(param.style);
    }

    // Save shared parameters
    chunk->WriteInt(bb.sharedParams.size());
    for (const auto &param : bb.sharedParams) {
        chunk->WriteInt(param.hPos);
        chunk->WriteInt(param.vPos);
    }
    for (const auto &param : bb.sharedParams) {
        chunk->WriteInt(param.style);
    }
    for (const auto &param : bb.sharedParams) {
        chunk->WriteObjectID(param.sourceId);
    }
}

void InterfaceData::SaveScriptGraph(SchematicNode &node) {
    CKStateChunk *chunk = node.chunk;
    const BehaviorBlock &bb = GetBehaviorBlockForNode(node);

    // Save inputs and outputs for graph
    chunk->WriteInt(bb.inwardInputs.size());
    for (const auto &input : bb.inwardInputs) {
        chunk->WriteInt(input);
        chunk->WriteInt(-1);
    }

    chunk->WriteInt(bb.outwardInputs.size());
    for (const auto &input : bb.outwardInputs) {
        chunk->WriteInt(input);
        chunk->WriteInt(-1);
    }

    chunk->WriteInt(bb.inwardOutputs.size());
    for (const auto &output : bb.inwardOutputs) {
        chunk->WriteInt(output);
        chunk->WriteInt(1);
    }

    chunk->WriteInt(bb.outwardOutputs.size());
    for (const auto &output : bb.outwardOutputs) {
        chunk->WriteInt(output);
        chunk->WriteInt(1);
    }
}

void InterfaceData::SaveScriptExtra(SchematicNode &node) {
    // For future extensions, currently empty
}

CKStateChunk *GenerateInterfaceChunk(InterfaceData &data, CKBehavior *behavior) {
    if (!behavior)
        return nullptr;

    SchematicNode node;
    node.behavior = behavior;
    node.version = 0x16;

    // Create and start the chunk
    node.chunk = CreateCKStateChunk(-1);
    if (!node.chunk)
        return nullptr;

    // Generate the interface chunk
    if (data.GenerateInterfaceChunk(node) != CK_OK) {
        delete node.chunk;
        return nullptr;
    }

    return node.chunk;
}

CKStateChunk *DecorateAndGenerateChunk(CKBehavior *behavior) {
    if (!behavior)
        return nullptr;

    // Create interface data
    InterfaceData data;

    // Decorate the behavior into the interface data
    Decorate(data, behavior);

    // Generate and return the chunk
    return GenerateInterfaceChunk(data, behavior);
}
