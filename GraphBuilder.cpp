#include "GraphBuilder.h"
#include "CKAll.h"

#include <queue>
#include <algorithm>
#include <stdexcept>

#undef min
#undef max

GraphBuilder::GraphBuilder(InterfaceData &targetData, CKContext *context)
    : m_Data(targetData), m_Context(context) {}

void GraphBuilder::BuildGraph(CKBehavior *rootBehavior) {
    // Initialize data structures
    m_Data.Clear();
    m_BehaviorMap.clear();
    m_OperationMap.clear();
    m_InputParamSet.clear();
    m_OutputParamSet.clear();
    m_BehaviorIds.clear();
    m_OperationIds.clear();
    m_InputParamIds.clear();
    m_OutputParamIds.clear();

    // Create a queue for BFS traversal of the behavior tree
    std::queue<std::pair<CKBehavior *, int>> behaviorQueue;
    behaviorQueue.emplace(rootBehavior, 0);

    // Process behaviors in breadth-first order
    while (!behaviorQueue.empty()) {
        auto currentPair = behaviorQueue.front();
        CKBehavior *beh = currentPair.first;
        const int depth = currentPair.second;
        behaviorQueue.pop();

        // Create a new behavior data if not the root
        BehaviorData *currentBehaviorData = depth > 0 ? &m_Data.NewBehavior() : &m_Data.scriptRoot;

        // Store behavior ID and mapping
        CK_ID behaviorId = beh->GetID();
        m_BehaviorIds.push_back(behaviorId);
        m_BehaviorMap[behaviorId] = depth > 0 ? static_cast<int>(m_Data.behaviors.size()) - 1 : -1;

        // Map operation IDs to indices
        const int operationCount = beh->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            CKParameterOperation *operation = beh->GetParameterOperation(i);
            CK_ID operationId = operation->GetID();
            m_OperationIds.push_back(operationId);
            m_OperationMap[operationId] = std::make_pair(depth > 0 ? m_Data.behaviors.size() - 1 : -1, i);
        }

        // Set up the behavior
        SetupBehavior(*currentBehaviorData, beh, depth);

        // Enqueue sub-behaviors for processing
        const int subBehaviorCount = beh->GetSubBehaviorCount();
        for (int i = 0; i < subBehaviorCount; ++i) {
            CKBehavior *subBehavior = beh->GetSubBehavior(i);
            behaviorQueue.emplace(subBehavior, depth + 1);
        }
    }

    // Configure parameter links
    ConfigureParameterLinks();

    m_Data.NotifyObservers(nullptr, InterfaceData::ElementAction::Modified);
}

BehaviorData &GraphBuilder::GetBehavior(CK_ID id) {
    int index = m_BehaviorMap[id];
    return index >= 0 ? m_Data.behaviors[index] : m_Data.scriptRoot;
}

bool GraphBuilder::IsOperation(CK_ID id) const {
    CKObject *obj = m_Context->GetObject(id);
    return obj && obj->GetClassID() == CKCID_PARAMETEROPERATION;
}

void GraphBuilder::SetupBehavior(BehaviorData &behaviorData, CKBehavior *behavior, int depth) {
    behaviorData.id = behavior->GetID();
    behaviorData.folded = depth > 0;
    behaviorData.depth = depth;
    behaviorData.isBehaviorGraph = behavior->GetType() != CKBEHAVIORTYPE_BASE;

    CalculateBehaviorSize(behaviorData, behavior);

    // Track input parameters
    for (int i = 0, count = behavior->GetInputParameterCount(); i < count; ++i) {
        CK_ID paramId = behavior->GetInputParameter(i)->GetID();
        if (m_InputParamSet.find(paramId) == m_InputParamSet.end()) {
            m_InputParamIds.push_back(paramId);
            m_InputParamSet.insert(paramId);
        }
    }

    // Track output parameters
    for (int i = 0, count = behavior->GetOutputParameterCount(); i < count; ++i) {
        CK_ID paramId = behavior->GetOutputParameter(i)->GetID();
        if (m_OutputParamSet.find(paramId) == m_OutputParamSet.end()) {
            m_OutputParamIds.push_back(paramId);
            m_OutputParamSet.insert(paramId);
        }
    }

    // Track target parameter if used
    if (behavior->IsUsingTarget()) {
        CK_ID paramId = behavior->GetTargetParameter()->GetID();
        if (m_InputParamSet.find(paramId) == m_InputParamSet.end()) {
            m_InputParamIds.push_back(paramId);
            m_InputParamSet.insert(paramId);
        }
    }

    // Track operation parameters
    for (int i = 0, count = behavior->GetParameterOperationCount(); i < count; ++i) {
        CKParameterOperation *operation = behavior->GetParameterOperation(i);

        CK_ID inParam1Id = operation->GetInParameter1()->GetID();
        if (m_InputParamSet.find(inParam1Id) == m_InputParamSet.end()) {
            m_InputParamIds.push_back(inParam1Id);
            m_InputParamSet.insert(inParam1Id);
        }

        CK_ID inParam2Id = operation->GetInParameter2()->GetID();
        if (m_InputParamSet.find(inParam2Id) == m_InputParamSet.end()) {
            m_InputParamIds.push_back(inParam2Id);
            m_InputParamSet.insert(inParam2Id);
        }

        CK_ID outParamId = operation->GetOutParameter()->GetID();
        if (m_OutputParamSet.find(outParamId) == m_OutputParamSet.end()) {
            m_OutputParamIds.push_back(outParamId);
            m_OutputParamSet.insert(outParamId);
        }
    }

    // Process behavior links if this is a behavior graph
    if (behaviorData.isBehaviorGraph) {
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

            behaviorData.AddLink(link);
        }

        // Add operations
        for (int i = 0, count = behavior->GetParameterOperationCount(); i < count; ++i) {
            CKParameterOperation *operation = behavior->GetParameterOperation(i);
            Operation operationData(operation->GetID());
            behaviorData.AddOperation(operationData);
        }

        // Add local parameters
        for (int i = 0, count = behavior->GetLocalParameterCount(); i < count; ++i) {
            CKParameterLocal *localParam = behavior->GetLocalParameter(i);
            Parameter paramData(localParam->GetID(), PARAM_STYLE_CLOSED);
            behaviorData.AddLocalParameter(paramData);
        }
    }
}

void GraphBuilder::CalculateBehaviorSize(BehaviorData &behaviorData, CKBehavior *behavior) {
    if (behaviorData.depth > 0) {
        // Calculate height based on max of inputs and outputs
        int height = std::max(behavior->GetOutputCount(), behavior->GetInputCount());
        if (height < 1) {
            height = 1;
        }

        // Calculate width based on parameters and name length
        int width = std::max(behavior->GetOutputParameterCount(), behavior->GetInputParameterCount());
        const char *name = behavior->GetName();
        const size_t nameLength = name ? strlen(name) : 0;
        const int nameWidth = static_cast<int>(std::floor(nameLength * 0.4 + 1));
        if (nameWidth > width) {
            width = nameWidth;
        }
        if (width < 2) {
            width = 2;
        }

        // Set size
        behaviorData.size.hSize = static_cast<float>(width) * 20.0f;
        behaviorData.size.vSize = static_cast<float>(height) * 20.0f;

        // Set expanded size for behavior graphs
        if (behaviorData.isBehaviorGraph) {
            behaviorData.hExpandSize = behaviorData.size.hSize * 10.0f;
            behaviorData.vExpandSize = behaviorData.size.vSize * 10.0f;
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

    m_Context->OutputToConsoleEx((CKSTRING) "Error: Unknown owner type for input parameter");
    throw std::runtime_error("Unknown owner type for input parameter");
}

GraphBuilder::ParameterPosition GraphBuilder::GetOutputParameterPosition(
    CKParameterOut *outputParam, CKBehavior **ownerBehavior) {
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

    m_Context->OutputToConsoleEx((CKSTRING) "Error: Unknown owner type for output parameter");
    throw std::runtime_error("Unknown owner type for output parameter");
}

GraphBuilder::ParameterPosition GraphBuilder::GetLocalParameterPosition(CKParameterLocal *localParam) {
    ParameterPosition position = {};
    CKObject *ownerObject = localParam->GetOwner();

    // Owner must be a behavior
    if (ownerObject->GetClassID() != CKCID_BEHAVIOR) {
        m_Context->OutputToConsoleEx((CKSTRING) "Error: Local parameter owner is not a behavior");
        throw std::runtime_error("Local parameter owner is not a behavior");
    }

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

        m_Context->OutputToConsoleEx((CKSTRING) "Error: Unknown owner type for output parameter");
    }

    m_Context->OutputToConsoleEx((CKSTRING) "Error: Unknown parameter type");
    return nullptr;
}

GraphBuilder::ParameterPosition GraphBuilder::GetShortcutParameterPosition(CK_ID behaviorId, CK_ID sourceId) {
    // Check if shortcut already exists
    BehaviorData &behavior = GetBehavior(behaviorId);

    // Check if shortcut already exists
    for (int i = 0; i < behavior.sharedParamCount; ++i) {
        if (behavior.sharedParams[i].sourceId == sourceId) {
            return {behaviorId, i, behaviorId};
        }
    }

    // Create a new shortcut parameter
    Parameter paramData(sourceId, PARAM_STYLE_CLOSED);
    paramData.sourceId = sourceId;
    behavior.AddSharedParameter(paramData);

    return {behaviorId, behavior.sharedParamCount - 1, behaviorId};
}

void GraphBuilder::ConfigureParameterLinks() {
    // Maps to track parameter chains
    std::unordered_map<CK_ID, std::vector<ParameterPosition>> inputChain;
    std::unordered_map<CK_ID, std::vector<ParameterPosition>> outputChain;

    // Process input parameters in insertion order
    for (const auto &id : m_InputParamIds) {
        auto *inputParam = (CKParameterIn *) m_Context->GetObject(id);
        if (!inputParam) continue;

        auto &positionChain = inputChain[id];
        CKBehavior *beh = nullptr;

        try {
            positionChain.push_back(GetInputParameterPosition(inputParam, &beh));

            LinkEndpoint lastEndpoint = {
                positionChain.back().id,
                positionChain.back().index,
                positionChain.back().index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
            };

            // Create chain of links for input parameters
            while (beh && beh->GetInputParameterPosition(inputParam) != -1) {
                positionChain.push_back({
                    beh->GetID(),
                    beh->GetInputParameterPosition(inputParam),
                    beh->GetParent()->GetID()
                });

                Link link;
                link.id = 0;
                link.type = LINK_TYPE_PARAMETER_OP;
                link.start = {positionChain.back().id, positionChain.back().index, ENDPOINT_PIN};
                link.end = lastEndpoint;
                lastEndpoint = link.start;

                GetBehavior(beh->GetID()).AddLink(link);
                beh = beh->GetParent();
            }
        } catch (const std::exception &e) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error processing input parameter %d: %s", id, e.what());
        }
    }

    // Process output parameters in insertion order
    for (const auto &id : m_OutputParamIds) {
        auto *outputParam = (CKParameterOut *) m_Context->GetObject(id);
        if (!outputParam) continue;

        auto &positionChain = outputChain[id];
        CKBehavior *beh = nullptr;

        try {
            positionChain.push_back(GetOutputParameterPosition(outputParam, &beh));

            LinkEndpoint lastEndpoint = {positionChain.back().id, positionChain.back().index, ENDPOINT_POUT};

            // Create chain of links for output parameters
            while (beh && beh->GetOutputParameterPosition(outputParam) != -1) {
                positionChain.push_back({
                    beh->GetID(),
                    beh->GetOutputParameterPosition(outputParam),
                    beh->GetParent()->GetID()
                });

                Link link;
                link.id = 0;
                link.type = LINK_TYPE_PARAMETER_OP;
                link.end = {positionChain.back().id, positionChain.back().index, ENDPOINT_POUT};
                link.start = lastEndpoint;
                lastEndpoint = link.end;

                GetBehavior(beh->GetID()).AddLink(link);
                beh = beh->GetParent();
            }
        } catch (const std::exception &e) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error processing output parameter %d: %s", id, e.what());
        }
    }

    // Connect parameter chains
    ConfigureDirectParameterConnections(inputChain, outputChain);
}

void GraphBuilder::ConfigureDirectParameterConnections(const ParameterChain &inputChain,
                                                       const ParameterChain &outputChain) {
    // Connect input parameters to their sources
    for (const auto &id : m_InputParamIds) {
        auto *inputParam = (CKParameterIn *) m_Context->GetObject(id);
        if (!inputParam) continue;

        try {
            CKBehavior *beh = nullptr;
            auto position = GetInputParameterPosition(inputParam, &beh);
            const auto &inputPositions = inputChain.at(inputParam->GetID());

            // Direct source connection
            if (inputParam->GetDirectSource()) {
                CKParameter *sourceParam = inputParam->GetDirectSource();
                auto sourceIt = outputChain.find(sourceParam->GetID());
                std::vector<ParameterPosition> sourcePositions;

                if (sourceIt != outputChain.end()) {
                    sourcePositions = sourceIt->second;
                } else if (sourceParam->GetClassID() == CKCID_PARAMETERLOCAL) {
                    // Handle local parameters not in output chain
                    sourcePositions.push_back(GetLocalParameterPosition((CKParameterLocal *) sourceParam));
                }

                bool connected = false;

                // Try direct connection within same behavior
                for (const auto &inputPos : inputPositions) {
                    for (const auto &sourcePos : sourcePositions) {
                        if (inputPos.behaviorId == sourcePos.behaviorId) {
                            Link link;
                            link.id = 0;
                            link.type = LINK_TYPE_PARAMETER;
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

                            GetBehavior(inputPos.behaviorId).AddLink(link);
                            connected = true;
                            break;
                        }
                    }
                    if (connected) break;
                }

                // Use shortcut if no direct connection
                if (!connected) {
                    Link link;
                    link.id = 0;
                    link.type = LINK_TYPE_PARAMETER;
                    auto shortcutPos = GetShortcutParameterPosition(position.behaviorId, sourceParam->GetID());
                    link.start = {position.behaviorId, shortcutPos.index, ENDPOINT_POUT_SHORTCUT};
                    link.end = {
                        position.id, position.index,
                        position.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
                    };

                    GetBehavior(position.behaviorId).AddLink(link);
                }
            } else if (inputParam->GetSharedSource()) {
                // Shared source connection
                CKParameterIn *sharedInput = inputParam->GetSharedSource();
                if (sharedInput->GetOwner()->GetClassID() != CKCID_BEHAVIOR) {
                    throw std::runtime_error("Shared input owner is not a behavior");
                }

                auto sharedIt = inputChain.find(sharedInput->GetID());
                if (sharedIt == inputChain.end()) continue;

                const auto &sharedInputPositions = sharedIt->second;
                bool connected = false;

                // Find connection between input and shared source
                for (const auto &inputPos : inputPositions) {
                    for (const auto &sharedPos : sharedInputPositions) {
                        if (inputPos.behaviorId == sharedPos.id) {
                            Link link;
                            link.id = 0;
                            link.type = LINK_TYPE_PARAMETER;
                            link.start = {sharedPos.id, sharedPos.index, ENDPOINT_PIN};
                            link.end = {
                                inputPos.id,
                                inputPos.index,
                                inputPos.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
                            };

                            GetBehavior(inputPos.behaviorId).AddLink(link);
                            connected = true;
                            break;
                        }
                    }
                    if (connected) break;
                }

                if (!connected) {
                    m_Context->OutputToConsoleEx((CKSTRING) "pin: can't connect %d <-> %d, source type is %d",
                                                 inputParam->GetID(), sharedInput->GetID(), sharedInput->GetClassID());
                }
            }
        } catch (const std::exception &e) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error connecting input parameter %d: %s", id, e.what());
        }
    }
}