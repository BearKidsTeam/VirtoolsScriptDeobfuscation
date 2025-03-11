#include "GraphBuilder.h"
#include "CKAll.h"

#include <queue>
#include <stdexcept>

#undef min
#undef max

GraphBuilder::GraphBuilder(InterfaceData &targetData, CKContext *context)
    : m_Data(targetData), m_Context(context) {}

void GraphBuilder::BuildGraph(CKBehavior *rootBehavior) {
    if (!rootBehavior)
        return;

    // Initialize data structures
    InitializeState();

    // Process behaviors using BFS traversal
    ProcessBehaviorTree(rootBehavior);

    // Configure parameter links
    ConfigureParameterLinks();

    // Notify observers of changes
    m_Data.NotifyObservers(nullptr, InterfaceData::ElementAction::Modified);
}

void GraphBuilder::InitializeState() {
    m_Data.Clear();
    m_BehaviorMap.clear();
    m_OperationMap.clear();
    m_InputParamSet.clear();
    m_OutputParamSet.clear();
    m_InputParams.clear();
    m_OutputParams.clear();
}

void GraphBuilder::ProcessBehaviorTree(CKBehavior *rootBehavior) {
    std::queue<std::pair<CKBehavior *, int>> behaviorQueue;
    behaviorQueue.emplace(rootBehavior, 0);

    while (!behaviorQueue.empty()) {
        auto pair = behaviorQueue.front();
        CKBehavior *behavior = pair.first;
        int depth = pair.second;
        behaviorQueue.pop();

        // Create a new behavior data if not the root
        BehaviorData *behaviorData = depth > 0 ? &m_Data.NewBehavior() : &m_Data.rootBehavior;

        // Store behavior ID and mapping
        CK_ID behaviorId = behavior->GetID();
        m_BehaviorMap[behaviorId] = depth > 0 ? static_cast<int>(m_Data.behaviors.size()) - 1 : -1;

        // Map operation IDs to indices
        MapOperations(behavior, depth);

        // Set up the behavior
        SetupBehavior(*behaviorData, behavior, depth);

        // Enqueue sub-behaviors for processing
        EnqueueSubBehaviors(behavior, depth, behaviorQueue);
    }
}

void GraphBuilder::MapOperations(CKBehavior *behavior, int depth) {
    const int operationCount = behavior->GetParameterOperationCount();
    for (int i = 0; i < operationCount; ++i) {
        if (CKParameterOperation *operation = behavior->GetParameterOperation(i)) {
            m_OperationMap[operation->GetID()] = std::make_pair(
                depth > 0 ? m_Data.behaviors.size() - 1 : -1, i);
        }
    }
}

void GraphBuilder::EnqueueSubBehaviors(CKBehavior *behavior, int depth,
                                       std::queue<std::pair<CKBehavior *, int>> &queue) {
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    for (int i = 0; i < subBehaviorCount; ++i) {
        if (CKBehavior *subBehavior = behavior->GetSubBehavior(i)) {
            queue.emplace(subBehavior, depth + 1);
        }
    }
}

void GraphBuilder::SetupBehavior(BehaviorData &behaviorData, CKBehavior *behavior, int depth) {
    behaviorData.id = behavior->GetID();
    behaviorData.folded = depth > 0;
    behaviorData.depth = depth;
    behaviorData.isBehaviorGraph = behavior->GetType() != CKBEHAVIORTYPE_BASE;

    // Process parameters
    ProcessParameters(behaviorData, behavior);

    // Process behavior graph elements if this is a behavior graph
    if (behaviorData.isBehaviorGraph) {
        AddBehaviorLinks(behaviorData, behavior);
        AddOperations(behaviorData, behavior);
        AddLocalParameters(behaviorData, behavior);
    }
}

void GraphBuilder::ProcessParameters(BehaviorData &behaviorData, CKBehavior *behavior) {
    // Process input parameters
    const int inputParamCount = behavior->GetInputParameterCount();
    for (int i = 0; i < inputParamCount; ++i) {
        if (CKParameterIn *param = behavior->GetInputParameter(i)) {
            AddInputParameter(param->GetID());
        }
    }

    // Process output parameters
    const int outputParamCount = behavior->GetOutputParameterCount();
    for (int i = 0; i < outputParamCount; ++i) {
        if (CKParameterOut *param = behavior->GetOutputParameter(i)) {
            AddOutputParameter(param->GetID());
        }
    }

    // Process target parameter if used
    if (behavior->IsUsingTarget()) {
        behaviorData.isUsingTarget = true;
        if (CKParameterIn *targetParam = behavior->GetTargetParameter()) {
            AddInputParameter(targetParam->GetID());
        }
    }

    // Process operation parameters
    const int operationCount = behavior->GetParameterOperationCount();
    for (int i = 0; i < operationCount; ++i) {
        ProcessOperationParameters(behavior->GetParameterOperation(i));
    }
}

void GraphBuilder::ProcessOperationParameters(CKParameterOperation *operation) {
    if (!operation) return;

    // Process input parameter 1
    if (CKParameterIn *inParam1 = operation->GetInParameter1()) {
        AddInputParameter(inParam1->GetID());
    }

    // Process input parameter 2
    if (CKParameterIn *inParam2 = operation->GetInParameter2()) {
        AddInputParameter(inParam2->GetID());
    }

    // Process output parameter
    if (CKParameterOut *outParam = operation->GetOutParameter()) {
        AddOutputParameter(outParam->GetID());
    }
}

void GraphBuilder::AddInputParameter(CK_ID paramId) {
    if (m_InputParamSet.insert(paramId).second) {
        m_InputParams.push_back(paramId);
    }
}

void GraphBuilder::AddOutputParameter(CK_ID paramId) {
    if (m_OutputParamSet.insert(paramId).second) {
        m_OutputParams.push_back(paramId);
    }
}

void GraphBuilder::AddBehaviorLinks(BehaviorData &behaviorData, CKBehavior *behavior) {
    // Add behavior links
    const int linkCount = behavior->GetSubBehaviorLinkCount();
    for (int i = 0; i < linkCount; ++i) {
        if (CKBehaviorLink *behaviorLink = behavior->GetSubBehaviorLink(i)) {
            Link link = CreateBehaviorLink(behaviorLink);
            behaviorData.AddLink(link);
        }
    }
}

Link GraphBuilder::CreateBehaviorLink(CKBehaviorLink *behaviorLink) {
    // Set start endpoint
    CKBehaviorIO *inputIO = behaviorLink->GetInBehaviorIO();
    CKBehavior *inputBehavior = inputIO->GetOwner();
    LinkEndpoint start = {inputBehavior->GetID(), inputBehavior->GetOutputPosition(inputIO), ENDPOINT_BOUT};

    if (start.index == -1) {
        start.index = inputBehavior->GetInputPosition(inputIO);
        start.type = inputBehavior->GetType() == CKBEHAVIORTYPE_SCRIPT ? ENDPOINT_START_BIN : ENDPOINT_BIN;
    }

    // Set end endpoint
    CKBehaviorIO *outputIO = behaviorLink->GetOutBehaviorIO();
    CKBehavior *outputBehavior = outputIO->GetOwner();
    LinkEndpoint end = {outputBehavior->GetID(), outputBehavior->GetInputPosition(outputIO), ENDPOINT_BIN};

    if (end.index == -1) {
        end.index = outputBehavior->GetOutputPosition(outputIO);
        end.type = ENDPOINT_BOUT;
    }

    return {behaviorLink->GetID(), LINK_TYPE_BEHAVIOR, start, end};
}

void GraphBuilder::AddOperations(BehaviorData &behaviorData, CKBehavior *behavior) {
    const int operationCount = behavior->GetParameterOperationCount();
    for (int i = 0; i < operationCount; ++i) {
        if (CKParameterOperation *operation = behavior->GetParameterOperation(i)) {
            Operation operationData(operation->GetID());
            behaviorData.AddOperation(operationData);
        }
    }
}

void GraphBuilder::AddLocalParameters(BehaviorData &behaviorData, CKBehavior *behavior) {
    const int localParamCount = behavior->GetLocalParameterCount();
    for (int i = 0; i < localParamCount; ++i) {
        if (CKParameterLocal *localParam = behavior->GetLocalParameter(i)) {
            Parameter paramData(localParam->GetID(), PARAM_STYLE_CLOSED);
            behaviorData.AddLocalParameter(paramData);
        }
    }
}

BehaviorData &GraphBuilder::GetBehaviorData(CK_ID id) {
    const int index = m_BehaviorMap[id];
    return index >= 0 ? m_Data.behaviors[index] : m_Data.rootBehavior;
}

bool GraphBuilder::IsOperation(CK_ID id) const {
    CKObject *obj = m_Context->GetObject(id);
    return obj && obj->GetClassID() == CKCID_PARAMETEROPERATION;
}

void GraphBuilder::ConfigureParameterLinks() {
    // Maps to track parameter chains
    ParameterChain inputChain;
    ParameterChain outputChain;

    // Process input parameters
    for (const auto &id : m_InputParams) {
        auto *inputParam = (CKParameterIn *) m_Context->GetObject(id);
        if (inputParam) {
            ProcessInputParameter(inputParam, inputChain);
        }
    }

    // Process output parameters
    for (const auto &id : m_OutputParams) {
        auto *outputParam = (CKParameterOut *) m_Context->GetObject(id);
        if (outputParam) {
            ProcessOutputParameter(outputParam, outputChain);
        }
    }

    // Connect parameter chains
    ConfigureDirectParameterConnections(inputChain, outputChain);
}

void GraphBuilder::ProcessInputParameter(CKParameterIn *inputParam, ParameterChain &inputChain) {
    CKBehavior *beh = nullptr;
    try {
        auto &positionChain = inputChain[inputParam->GetID()];
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

            const LinkEndpoint start = {positionChain.back().id, positionChain.back().index, ENDPOINT_PIN};
            Link link(0, LINK_TYPE_PARAMETER_OP, start, lastEndpoint);
            lastEndpoint = start;

            GetBehaviorData(beh->GetID()).AddLink(link);
            beh = beh->GetParent();
        }
    } catch (const std::exception &e) {
        m_Context->OutputToConsoleEx((CKSTRING) "Error processing input parameter %d: %s", inputParam->GetID(), e.what());
    }
}

void GraphBuilder::ProcessOutputParameter(CKParameterOut *outputParam, ParameterChain &outputChain) {
    CKBehavior *beh = nullptr;
    try {
        auto &positionChain = outputChain[outputParam->GetID()];
        positionChain.push_back(GetOutputParameterPosition(outputParam, &beh));

        LinkEndpoint lastEndpoint = {positionChain.back().id, positionChain.back().index, ENDPOINT_POUT};

        // Create chain of links for output parameters
        while (beh && beh->GetOutputParameterPosition(outputParam) != -1) {
            positionChain.push_back({
                beh->GetID(),
                beh->GetOutputParameterPosition(outputParam),
                beh->GetParent()->GetID()
            });

            const LinkEndpoint end = {positionChain.back().id, positionChain.back().index, ENDPOINT_POUT};
            Link link(0, LINK_TYPE_PARAMETER_OP, lastEndpoint, end);
            lastEndpoint = end;

            GetBehaviorData(beh->GetID()).AddLink(link);
            beh = beh->GetParent();
        }
    } catch (const std::exception &e) {
        m_Context->OutputToConsoleEx((CKSTRING) "Error processing output parameter %d: %s",
                                     outputParam->GetID(), e.what());
    }
}

GraphBuilder::ParameterPosition GraphBuilder::GetInputParameterPosition(CKParameterIn *inputParam, CKBehavior **owner) {
    ParameterPosition position = {};
    CKObject *ownerObject = inputParam->GetOwner();
    position.id = ownerObject->GetID();

    // Check if owner is a behavior
    if (ownerObject->GetClassID() == CKCID_BEHAVIOR) {
        CKBehavior *ownerBeh = (CKBehavior *) ownerObject;
        position.index = ownerBeh->GetInputParameterPosition(inputParam);

        // Handle target parameter
        if (ownerBeh->IsUsingTarget() && ownerBeh->GetTargetParameter()->GetID() == inputParam->GetID()) {
            position.index = -2;
        }

        *owner = ownerBeh->GetParent();
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

    CKBehavior *ownerBeh = (CKBehavior *) ownerObject;
    position.id = ownerBeh->GetID();
    position.index = ownerBeh->GetLocalParameterPosition(localParam);
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

CKBehavior *GraphBuilder::GetParameterOwner(CKParameter *parameter) {
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
    // Get behavior data
    BehaviorData &behaviorData = GetBehaviorData(behaviorId);

    // Check if shortcut already exists
    const int sharedParamCount = static_cast<int>(behaviorData.sharedParams.size());
    for (int i = 0; i < sharedParamCount; ++i) {
        if (behaviorData.sharedParams[i].sourceId == sourceId) {
            return {behaviorId, i, behaviorId};
        }
    }

    // Create a new shortcut parameter
    Parameter paramData(sourceId, PARAM_STYLE_CLOSED);
    paramData.sourceId = sourceId;
    behaviorData.AddSharedParameter(paramData);

    return {behaviorId, static_cast<int>(behaviorData.sharedParams.size()) - 1, behaviorId};
}

void GraphBuilder::ConfigureDirectParameterConnections(const ParameterChain &inputChain,
                                                       const ParameterChain &outputChain) {
    // Connect input parameters to their sources
    for (const auto &id : m_InputParams) {
        auto *inputParam = (CKParameterIn *) m_Context->GetObject(id);
        if (!inputParam) continue;

        try {
            CKBehavior *beh = nullptr;
            auto position = GetInputParameterPosition(inputParam, &beh);

            // Find the input positions in the chain
            auto inputIt = inputChain.find(inputParam->GetID());
            if (inputIt == inputChain.end()) continue;
            const auto &inputPositions = inputIt->second;

            // Direct source connection
            if (CKParameter *sourceParam = inputParam->GetDirectSource()) {
                ProcessDirectSourceConnection(inputParam, sourceParam, inputPositions, position, outputChain);
            }
            // Shared source connection
            else if (CKParameterIn *sharedInput = inputParam->GetSharedSource()) {
                ProcessSharedSourceConnection(inputParam, sharedInput, inputPositions, inputChain);
            }
        } catch (const std::exception &e) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error connecting input parameter %d: %s", id, e.what());
        }
    }
}

void GraphBuilder::ProcessDirectSourceConnection(CKParameterIn *inputParam, CKParameter *sourceParam,
                                                 const std::vector<ParameterPosition> &inputPositions,
                                                 const ParameterPosition &position,
                                                 const ParameterChain &outputChain) {
    // Find source positions
    auto sourceIt = outputChain.find(sourceParam->GetID());
    std::vector<ParameterPosition> sourcePositions;

    if (sourceIt != outputChain.end()) {
        sourcePositions = sourceIt->second;
    } else if (sourceParam->GetClassID() == CKCID_PARAMETERLOCAL) {
        // Handle local parameters not in output chain
        sourcePositions.push_back(GetLocalParameterPosition((CKParameterLocal *) sourceParam));
    }

    bool connected = TryConnectWithinSameBehavior(inputPositions, sourcePositions, sourceParam);

    // Use shortcut if no direct connection
    if (!connected) {
        CreateShortcutConnection(position, sourceParam);
    }
}

bool GraphBuilder::TryConnectWithinSameBehavior(const std::vector<ParameterPosition> &inputPositions,
                                                const std::vector<ParameterPosition> &sourcePositions,
                                                CKParameter *sourceParam) {
    for (const auto &inputPos : inputPositions) {
        for (const auto &sourcePos : sourcePositions) {
            if (inputPos.behaviorId == sourcePos.behaviorId) {
                LinkEndpoint start = {
                    sourcePos.id,
                    sourcePos.index,
                    sourceParam->GetClassID() == CKCID_PARAMETERLOCAL ? ENDPOINT_PLOCAL : ENDPOINT_POUT
                };
                LinkEndpoint end = {
                    inputPos.id,
                    inputPos.index,
                    inputPos.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
                };
                Link link(0, LINK_TYPE_PARAMETER, start, end);
                GetBehaviorData(inputPos.behaviorId).AddLink(link);
                return true;
            }
        }
    }
    return false;
}

void GraphBuilder::CreateShortcutConnection(const ParameterPosition &position, CKParameter *sourceParam) {
    auto shortcutPos = GetShortcutParameterPosition(position.behaviorId, sourceParam->GetID());
    LinkEndpoint start = {position.behaviorId, shortcutPos.index, ENDPOINT_POUT_SHORTCUT};
    LinkEndpoint end = {
        position.id, position.index,
        position.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
    };
    Link link = {0, LINK_TYPE_PARAMETER, start, end};
    GetBehaviorData(position.behaviorId).AddLink(link);
}

void GraphBuilder::ProcessSharedSourceConnection(CKParameterIn *inputParam, CKParameterIn *sharedInput,
                                                 const std::vector<ParameterPosition> &inputPositions,
                                                 const ParameterChain &inputChain) {
    if (sharedInput->GetOwner()->GetClassID() != CKCID_BEHAVIOR) {
        throw std::runtime_error("Shared input owner is not a behavior");
    }

    auto sharedIt = inputChain.find(sharedInput->GetID());
    if (sharedIt == inputChain.end()) return;

    const auto &sharedInputPositions = sharedIt->second;
    bool connected = false;

    // Find connection between input and shared source
    for (const auto &inputPos : inputPositions) {
        for (const auto &sharedPos : sharedInputPositions) {
            if (inputPos.behaviorId == sharedPos.id) {
                LinkEndpoint start = {sharedPos.id, sharedPos.index, ENDPOINT_PIN};
                LinkEndpoint end = {
                    inputPos.id,
                    inputPos.index,
                    inputPos.index == -2 ? ENDPOINT_TARGET_PIN : ENDPOINT_PIN
                };
                Link link = {0, LINK_TYPE_PARAMETER, start, end};
                GetBehaviorData(inputPos.behaviorId).AddLink(link);
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
