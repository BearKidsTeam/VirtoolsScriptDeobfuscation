#include "GraphBuilder.h"
#include "CKAll.h"

#include <queue>

#undef min
#undef max

GraphBuilder::GraphBuilder(InterfaceData &target_data, CKContext *context)
    : m_Data(target_data), m_Context(context) {}

BehaviorBlock &GraphBuilder::GetBehaviorBlock(CK_ID id) {
    int index = m_BehaviorMap[id];
    return (index >= 0) ? m_Data.behaviorBlocks[index] : m_Data.scriptRoot;
}

Operation &GraphBuilder::GetOperation(CK_ID id) {
    auto &opInfo = m_OperationMap[id];
    int bbIndex = opInfo.first;
    int opIndex = opInfo.second;

    if (bbIndex >= 0) {
        return m_Data.behaviorBlocks[bbIndex].operations[opIndex];
    } else {
        return m_Data.scriptRoot.operations[opIndex];
    }
}

bool GraphBuilder::IsOperation(CK_ID id) {
    CKObject *obj = m_Context->GetObject(id);
    return obj && obj->GetClassID() == CKCID_PARAMETEROPERATION;
}

void GraphBuilder::BuildGraph(CKBehavior *rootBehavior) {
    // Initialize data structures
    m_Data.Clear();
    m_Data.behaviorBlockCount = 0;
    m_Data.behaviorBlocks.clear();
    m_BehaviorMap.clear();
    m_OperationMap.clear();
    m_InputParams.clear();
    m_OutputParams.clear();

    // Create a queue for BFS traversal of the behavior tree
    std::queue<std::pair<CKBehavior *, int>> behaviorQueue;
    behaviorQueue.emplace(rootBehavior, 0);

    // Process behaviors in breadth-first order
    while (!behaviorQueue.empty()) {
        CKBehavior *currentBehavior = behaviorQueue.front().first;
        const int depth = behaviorQueue.front().second;
        behaviorQueue.pop();

        // Create a new behavior block if not the root
        if (depth > 0) {
            m_Data.behaviorBlocks.emplace_back();
            ++m_Data.behaviorBlockCount;
        }

        // Map behavior ID to index
        m_BehaviorMap[currentBehavior->GetID()] = depth > 0 ? m_Data.behaviorBlocks.size() - 1 : -1;

        // Map operation IDs to indices
        const int operationCount = currentBehavior->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            CKParameterOperation *operation = currentBehavior->GetParameterOperation(i);
            m_OperationMap[operation->GetID()] = std::make_pair(depth > 0 ? m_Data.behaviorBlocks.size() - 1 : -1, i);
        }

        // Get the behavior block and decorate it
        BehaviorBlock &behaviorBlock = depth > 0 ? m_Data.behaviorBlocks.back() : m_Data.scriptRoot;
        DecorateBehavior(behaviorBlock, currentBehavior, depth);

        // Enqueue sub-behaviors for processing
        const int subBehaviorCount = currentBehavior->GetSubBehaviorCount();
        for (int i = 0; i < subBehaviorCount; ++i) {
            CKBehavior *subBehavior = currentBehavior->GetSubBehavior(i);
            behaviorQueue.emplace(subBehavior, depth + 1);
        }
    }

    // Configure parameter links
    ConfigureParameterLinks(rootBehavior);
}

void GraphBuilder::DecorateBehavior(BehaviorBlock &behaviorBlock, CKBehavior *behavior, int depth) {
    behaviorBlock.id = behavior->GetID();
    behaviorBlock.folded = depth > 0;
    behaviorBlock.depth = depth;
    behaviorBlock.isBehaviorGraph = behavior->GetType() != CKBEHAVIORTYPE_BASE;

    CalculateBehaviorSize(behaviorBlock, behavior);

    // Track input parameters
    for (int i = 0, count = behavior->GetInputParameterCount(); i < count; ++i) {
        m_InputParams.insert(behavior->GetInputParameter(i)->GetID());
    }

    // Track output parameters
    for (int i = 0, count = behavior->GetOutputParameterCount(); i < count; ++i) {
        m_OutputParams.insert(behavior->GetOutputParameter(i)->GetID());
    }

    // Track target parameter if used
    if (behavior->IsUsingTarget()) {
        m_InputParams.insert(behavior->GetTargetParameter()->GetID());
    }

    // Track operation parameters
    for (int i = 0, count = behavior->GetParameterOperationCount(); i < count; ++i) {
        CKParameterOperation *operation = behavior->GetParameterOperation(i);
        m_InputParams.insert(operation->GetInParameter1()->GetID());
        m_InputParams.insert(operation->GetInParameter2()->GetID());
        m_OutputParams.insert(operation->GetOutParameter()->GetID());
    }

    // Process behavior links if this is a behavior graph
    if (behaviorBlock.isBehaviorGraph) {
        // Add behavior links
        for (int i = 0, count = behavior->GetSubBehaviorLinkCount(); i < count; ++i) {
            CKBehaviorLink *behaviorLink = behavior->GetSubBehaviorLink(i);

            Link link;
            link.id = behaviorLink->GetID();
            link.type = LINK_TYPE_BEHAVIOR; // Behavior link
            link.pointCount = 0;
            link.start = link.end = LinkEndpoint();

            // Set start endpoint
            CKBehaviorIO *inputIO = behaviorLink->GetInBehaviorIO();
            CKBehavior *inputBehavior = inputIO->GetOwner();
            link.start.id = inputBehavior->GetID();
            link.start.type = ENDPOINT_BOUT; // Output
            link.start.index = inputBehavior->GetOutputPosition(inputIO);

            if (link.start.index == -1) {
                link.start.index = inputBehavior->GetInputPosition(inputIO);
                link.start.type = ENDPOINT_BIN; // Input
                if (inputBehavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                    link.start.type = ENDPOINT_START_BIN; // Start input
                }
            }

            // Set end endpoint
            CKBehaviorIO *outputIO = behaviorLink->GetOutBehaviorIO();
            CKBehavior *outputBehavior = outputIO->GetOwner();
            link.end.id = outputBehavior->GetID();
            link.end.type = ENDPOINT_BIN; // Input
            link.end.index = outputBehavior->GetInputPosition(outputIO);

            if (link.end.index == -1) {
                link.end.index = outputBehavior->GetOutputPosition(outputIO);
                link.end.type = ENDPOINT_BOUT; // Output
            }

            behaviorBlock.AddLink(link);
        }

        // Add operations
        for (int i = 0, count = behavior->GetParameterOperationCount(); i < count; ++i) {
            CKParameterOperation *operation = behavior->GetParameterOperation(i);

            Operation operationData;
            operationData.id = operation->GetID();
            behaviorBlock.AddOperation(operationData);
        }

        // Add local parameters
        for (int i = 0, count = behavior->GetLocalParameterCount(); i < count; ++i) {
            CKParameterLocal *localParam = behavior->GetLocalParameter(i);

            Parameter paramData;
            paramData.id = localParam->GetID();
            paramData.style = PARAM_STYLE_CLOSED;
            behaviorBlock.AddLocalParameter(paramData);
        }
    }
}

void GraphBuilder::CalculateBehaviorSize(BehaviorBlock &behaviorBlock, CKBehavior *behavior) {
    if (behaviorBlock.depth > 0) {
        int height = std::max(behavior->GetOutputCount(), behavior->GetInputCount());
        height = std::max(height, 1);

        int width = std::max(behavior->GetOutputParameterCount(), behavior->GetInputParameterCount());
        width = std::max(width, int((strlen(behavior->GetName()) - 1) / 2.5) + 1);
        width = std::max(width, 2);

        behaviorBlock.size.hSize = (float) width * 20.0f;
        behaviorBlock.size.vSize = (float) height * 20.0f;
        if (behaviorBlock.isBehaviorGraph) {
            behaviorBlock.hExpandSize = behaviorBlock.size.hSize * 10;
            behaviorBlock.vExpandSize = behaviorBlock.size.vSize * 10;
        }
    }
}

GraphBuilder::ParameterPosition GraphBuilder::GetInputParameterPosition(CKParameterIn *inputParam, CKBehavior **owner) {
    ParameterPosition position = {};
    CKObject *ownerObject = inputParam->GetOwner();
    position.id = ownerObject->GetID();

    // Check if owner is a behavior
    if (ownerObject->GetClassID() == CKCID_BEHAVIOR) {
        CKBehavior *ownerBehavior = (CKBehavior *) ownerObject;
        position.index = ownerBehavior->GetInputParameterPosition(inputParam);

        // Handle target parameter
        if (ownerBehavior->IsUsingTarget() && ownerBehavior->GetTargetParameter()->GetID() == inputParam->GetID()) {
            position.index = -2;
        }

        *owner = ownerBehavior->GetParent();
        position.behaviorId = (*owner)->GetID();
        return position;
    }

    // Check if owner is a parameter operation
    if (ownerObject->GetClassID() == CKCID_PARAMETEROPERATION) {
        CKParameterOperation *operation = (CKParameterOperation *) ownerObject;
        position.index = operation->GetInParameter1()->GetID() == inputParam->GetID() ? 0 : 1;
        *owner = operation->GetOwner();
        position.behaviorId = (*owner)->GetID();
        return position;
    }

    throw; // Unexpected owner type
}

GraphBuilder::ParameterPosition GraphBuilder::GetOutputParameterPosition(CKParameterOut *outputParam, CKBehavior **ownerBehavior) {
    ParameterPosition position = {};
    CKObject *ownerObject = outputParam->GetOwner();
    position.id = ownerObject->GetID();

    // Check if owner is a behavior
    if (ownerObject->GetClassID() == CKCID_BEHAVIOR) {
        CKBehavior *ownerBeh = (CKBehavior *) ownerObject;
        position.index = ownerBeh->GetOutputParameterPosition(outputParam);
        *ownerBehavior = ownerBeh->GetParent();
        position.behaviorId = (*ownerBehavior)->GetID();
        return position;
    }

    // Check if owner is a parameter operation
    if (ownerObject->GetClassID() == CKCID_PARAMETEROPERATION) {
        CKParameterOperation *operation = (CKParameterOperation *) ownerObject;
        position.index = 0;
        *ownerBehavior = operation->GetOwner();
        position.behaviorId = (*ownerBehavior)->GetID();
        return position;
    }

    throw; // Unexpected owner type
}

GraphBuilder::ParameterPosition GraphBuilder::GetLocalParameterPosition(CKParameterLocal *localParam) {
    ParameterPosition position = {};
    CKObject *ownerObject = localParam->GetOwner();

    // Owner must be a behavior
    assert(ownerObject->GetClassID() == CKCID_BEHAVIOR);

    CKBehavior *ownerBehavior = (CKBehavior *) ownerObject;
    position.id = ownerBehavior->GetID();
    position.index = ownerBehavior->GetLocalParameterPosition(localParam);
    position.behaviorId = position.id;
    return position;
}

LinkEndpoint GraphBuilder::GetParameterEndpoint(CKParameter *parameter) {
    // Check if parameter is local
    if (parameter->GetClassID() == CKCID_PARAMETERLOCAL) {
        ParameterPosition position = GetLocalParameterPosition((CKParameterLocal *) parameter);
        return {position.id, position.index, ENDPOINT_PLOCAL}; // Local parameter endpoint
    }

    // Get output parameter endpoint
    CKBehavior *dummy;
    ParameterPosition position = GetOutputParameterPosition((CKParameterOut *) parameter, &dummy);
    return {position.id, position.index, ENDPOINT_POUT}; // Output parameter endpoint
}

CKBehavior *GraphBuilder::GetParameterOwnerBehavior(CKParameter *parameter) {
    // Local parameter case
    if (parameter->GetClassID() == CKCID_PARAMETERLOCAL) {
        return (CKBehavior *) parameter->GetOwner();
    }

    // Output parameter case
    if (parameter->GetClassID() == CKCID_PARAMETEROUT) {
        CKObject *owner = parameter->GetOwner();

        // Owner is a behavior
        if (owner->GetClassID() == CKCID_BEHAVIOR) {
            return ((CKBehavior *) owner)->GetParent();
        }

        // Owner is a parameter operation
        if (owner->GetClassID() == CKCID_PARAMETEROPERATION) {
            return ((CKParameterOperation *) owner)->GetOwner();
        }

        throw; // Unexpected owner type
    }

    throw; // Unexpected parameter type
}

GraphBuilder::ParameterPosition GraphBuilder::GetShortcutParameterPosition(CK_ID behaviorId, CK_ID sourceId) {
    // Check if shortcut already exists
    BehaviorBlock &behaviorBlock = GetBehaviorBlock(behaviorId);
    for (int i = 0, count = behaviorBlock.sharedParamCount; i < count; ++i) {
        if (behaviorBlock.sharedParams[i].sourceId == sourceId) {
            return {behaviorId, i, behaviorId};
        }
    }

    // Create a new shortcut parameter
    Parameter paramData;
    paramData.sourceId = sourceId;
    paramData.hPos = paramData.vPos = 0;
    paramData.id = sourceId;
    paramData.style = PARAM_STYLE_CLOSED;
    behaviorBlock.AddSharedParameter(paramData);

    return {behaviorId, behaviorBlock.sharedParamCount - 1, behaviorId};
}

void GraphBuilder::ConfigureParameterLinks(CKBehavior *root) {
    // Maps to track parameter chains
    std::unordered_map<CK_ID, std::vector<ParameterPosition>> inputChain;
    std::unordered_map<CK_ID, std::vector<ParameterPosition>> outputChain;
    CKBehavior *currentBehavior;

    // Process input parameters
    for (auto &id : m_InputParams) {
        auto &positionChain = inputChain[id] = {};
        auto *inputParam = (CKParameterIn *) m_Context->GetObject(id);

        positionChain.push_back(GetInputParameterPosition(inputParam, &currentBehavior));
        LinkEndpoint lastEndpoint = {
            positionChain.back().id,
            positionChain.back().index,
            positionChain.back().index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
        };

        // Create chain of links for input parameters
        for (; currentBehavior && currentBehavior->GetInputParameterPosition(inputParam) != -1;
               currentBehavior = currentBehavior->GetParent()) {
            positionChain.push_back({
                currentBehavior->GetID(),
                currentBehavior->GetInputParameterPosition(inputParam),
                currentBehavior->GetParent()->GetID()
            });

            Link link;
            link.id = 0;
            link.type = LINK_TYPE_PARAMETER_OP;
            link.pointCount = 0;
            link.start = {positionChain.back().id, positionChain.back().index, ENDPOINT_PIN};
            link.end = lastEndpoint;
            lastEndpoint = link.start;

            GetBehaviorBlock(currentBehavior->GetID()).AddLink(link);
        }
    }

    // Process output parameters
    for (auto &id : m_OutputParams) {
        std::vector<ParameterPosition> &positionChain = outputChain[id] = std::vector<ParameterPosition>();
        CKParameterOut *outputParam = (CKParameterOut *) m_Context->GetObject(id);

        positionChain.push_back(GetOutputParameterPosition(outputParam, &currentBehavior));
        LinkEndpoint lastEndpoint = {positionChain.back().id, positionChain.back().index, ENDPOINT_POUT};

        // Create chain of links for output parameters
        for (; currentBehavior && currentBehavior->GetOutputParameterPosition(outputParam) != -1;
               currentBehavior = currentBehavior->GetParent()) {
            positionChain.push_back({
                currentBehavior->GetID(),
                currentBehavior->GetOutputParameterPosition(outputParam),
                currentBehavior->GetParent()->GetID()
            });

            Link link;
            link.id = 0;
            link.type = LINK_TYPE_PARAMETER_OP;
            link.pointCount = 0;
            link.end = {positionChain.back().id, positionChain.back().index, ENDPOINT_POUT};
            link.start = lastEndpoint;
            lastEndpoint = link.end;

            GetBehaviorBlock(currentBehavior->GetID()).AddLink(link);
        }
    }

    // Connect input parameters to their sources
    for (auto &id : m_InputParams) {
        CKParameterIn *inputParam = (CKParameterIn *) m_Context->GetObject(id);
        ParameterPosition position = GetInputParameterPosition(inputParam, &currentBehavior);
        std::vector<ParameterPosition> &inputPositions = inputChain[inputParam->GetID()];

        // Direct source connection
        if (inputParam->GetDirectSource()) {
            CKParameter *sourceParam = inputParam->GetDirectSource();
            std::vector<ParameterPosition> &sourcePositions = outputChain[sourceParam->GetID()];

            // Handle local parameters that aren't in the output chain
            if (sourceParam->GetClassID() == CKCID_PARAMETERLOCAL && sourcePositions.empty()) {
                sourcePositions.push_back(GetLocalParameterPosition((CKParameterLocal *) sourceParam));
            }

            bool connected = false;

            // Try to find a direct connection within the same behavior
            for (auto &inputPos : inputPositions) {
                for (auto &sourcePos : sourcePositions) {
                    if (inputPos.behaviorId == sourcePos.behaviorId) {
                        Link link;
                        link.id = 0;
                        link.type = LINK_TYPE_PARAMETER;
                        link.pointCount = 0;
                        link.start = {
                            sourcePos.id,
                            sourcePos.index,
                            sourceParam->GetClassID() == CKCID_PARAMETERLOCAL ? ENDPOINT_PLOCAL : ENDPOINT_POUT
                        };
                        link.end = {
                            inputPos.id,
                            inputPos.index,
                            inputPos.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
                        };

                        GetBehaviorBlock(inputPos.behaviorId).AddLink(link);
                        connected = true;
                        break;
                    }
                }
                if (connected) {
                    break;
                }
            }

            // If no direct connection found, use a shortcut
            if (!connected) {
                Link link;
                link.id = 0;
                link.type = LINK_TYPE_PARAMETER;
                link.pointCount = 0;
                ParameterPosition shortcutPos = GetShortcutParameterPosition(position.behaviorId, sourceParam->GetID());
                link.start = {position.behaviorId, shortcutPos.index, ENDPOINT_POUT_SHORTCUT};
                link.end = {position.id, position.index, position.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN};

                GetBehaviorBlock(position.behaviorId).AddLink(link);
            }
        } else if (inputParam->GetSharedSource()) {
            // Shared source connection
            CKParameterIn *sharedInput = inputParam->GetSharedSource();
            assert(sharedInput->GetOwner()->GetClassID() == CKCID_BEHAVIOR);
            std::vector<ParameterPosition> &sharedInputPositions = inputChain[sharedInput->GetID()];

            bool connected = false;

            // Try to find a connection between the input and its shared source
            for (auto &inputPos : inputPositions) {
                for (auto &sharedPos : sharedInputPositions) {
                    if (inputPos.behaviorId == sharedPos.id) {
                        Link link;
                        link.id = 0;
                        link.type = LINK_TYPE_PARAMETER;
                        link.pointCount = 0;
                        link.start = {sharedPos.id, sharedPos.index, ENDPOINT_PIN};
                        link.end = {
                            inputPos.id, inputPos.index, inputPos.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
                        };

                        GetBehaviorBlock(inputPos.behaviorId).AddLink(link);
                        connected = true;
                        break;
                    }
                }
                if (connected) {
                    break;
                }
            }

            // Log warning if no connection found
            if (!connected) {
                m_Context->OutputToConsoleEx((CKSTRING) "pin: can't connect %d <-> %d, source type is %d",
                                             inputParam->GetID(), sharedInput->GetID(), sharedInput->GetClassID());
            }
        }
    }

    // Connect output parameters to their destinations
    for (auto &id : m_OutputParams) {
        CKParameterOut *outputParam = (CKParameterOut *) m_Context->GetObject(id);
        std::vector<ParameterPosition> &outputPositions = outputChain[outputParam->GetID()];

        for (int j = 0, destCount = outputParam->GetDestinationCount(); j < destCount; ++j) {
            CKParameter *destParam = outputParam->GetDestination(j);
            LinkEndpoint destEndpoint = GetParameterEndpoint(destParam);
            CK_ID destBehaviorId = destParam->GetOwner()->GetID();

            bool connected = false;

            // Try to find a direct connection within the same behavior
            for (auto &outputPos : outputPositions) {
                if (outputPos.behaviorId == destBehaviorId) {
                    Link link;
                    link.id = 0;
                    link.type = LINK_TYPE_PARAMETER;
                    link.pointCount = 0;
                    link.start = {outputPos.id, outputPos.index, ENDPOINT_POUT};
                    link.end = destEndpoint;

                    GetBehaviorBlock(outputPos.behaviorId).AddLink(link);
                    connected = true;
                    break;
                }
            }

            // If no direct connection found, handle special cases
            if (!connected) {
                // Handle connection to a local parameter shortcut
                if (destParam->GetClassID() == CKCID_PARAMETERLOCAL) {
                    Link link;
                    link.id = 0;
                    link.type = LINK_TYPE_PARAMETER;
                    link.pointCount = 0;
                    ParameterPosition sourcePos = outputPositions.front();
                    ParameterPosition shortcutPos = GetShortcutParameterPosition(
                        sourcePos.behaviorId, destParam->GetID());
                    link.start = {sourcePos.id, sourcePos.index, ENDPOINT_POUT};
                    link.end = {shortcutPos.id, shortcutPos.index, ENDPOINT_POUT_SHORTCUT};

                    GetBehaviorBlock(sourcePos.behaviorId).AddLink(link);
                } else {
                    // Log warning for other cases
                    m_Context->OutputToConsoleEx((CKSTRING) "pout: can't connect %d <-> %d, dest type is %d",
                                                 outputParam->GetID(), destParam->GetID(), destParam->GetClassID());
                }
            }
        }
    }
}