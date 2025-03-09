#include "Decorator.h"

#include <algorithm>

#include "CKAll.h"

#undef min
#undef max

Decorator::Decorator(InterfaceData &target_data, CKContext *context) : m_Data(target_data), m_Context(context) {}

BehaviorBlock &Decorator::GetBehaviorBlock(CK_ID id) {
    int index = m_BehaviorMap[id];
    return (index >= 0) ? m_Data.behaviorBlocks[index] : m_Data.scriptRoot;
}

Operation &Decorator::GetOperation(CK_ID id) {
    auto &opInfo = m_OperationMap[id];
    int bbIndex = opInfo.first;
    int opIndex = opInfo.second;

    if (bbIndex >= 0) {
        return m_Data.behaviorBlocks[bbIndex].operations[opIndex];
    } else {
        return m_Data.scriptRoot.operations[opIndex];
    }
}

void Decorator::DecorateStart(BehaviorBlock &script, float verticalStartPos, float verticalSize) {
    m_Data.start.id = script.id;
    m_Data.start.vSize = verticalSize;
    m_Data.start.vStartPos = verticalStartPos;
    m_Data.start.vStart = 0;
}

void Decorator::Decorate(CKBehavior *script) {
    // Clear existing data
    m_Data.Clear();

    // Populate the interface data
    DecorateBehaviorTree(script);

    // Calculate the height of the behavior block
    float blockHeight = std::max(m_RequiredSize[script->GetID()].vSize + 4 * 20.0f, 200.0f);
    float startVertical = blockHeight / 2.0f;

    // Set start information and recalculate positions
    DecorateStart(m_Data.scriptRoot, startVertical, blockHeight);
    RecalculateAbsolutePositions(m_Data.scriptRoot, script, 0.0f, 0.0f);
}

void Decorator::DecorateBehaviorTree(CKBehavior *rootBehavior) {
    // Initialize data structures
    m_Data.behaviorBlockCount = 0;
    m_Data.behaviorBlocks.clear();
    m_BehaviorMap.clear();
    m_OperationMap.clear();
    m_InputParams.clear();
    m_OutputParams.clear();
    m_MovedOperations.clear();

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

    // Calculate layout for each behavior block
    for (auto &pair : m_BehaviorMap) {
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(pair.first);
        if (behaviorBlock.isBehaviorGraph) {
            CalculateBehaviorPositions(
                GetBehaviorBlock(pair.first),
                (CKBehavior *) m_Context->GetObjectA(behaviorBlock.id),
                behaviorBlock.depth == 0
            );
        }
    }

    // Calculate visual properties
    for (auto &pair : m_BehaviorMap) {
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(pair.first);
        if (behaviorBlock.isBehaviorGraph) {
            // Apply multiple passes of operation positioning
            for (int i = 0; i < MAX_FIX_STACK_OPS; ++i) {
                CalculateOperationPositions(
                    GetBehaviorBlock(pair.first),
                    (CKBehavior *) m_Context->GetObjectA(behaviorBlock.id)
                );
            }

            // Calculate parameter positions
            CalculateLocalParameterPositions(
                GetBehaviorBlock(pair.first),
                (CKBehavior *) m_Context->GetObjectA(behaviorBlock.id),
                false
            );

            CalculateLocalParameterPositions(
                GetBehaviorBlock(pair.first),
                (CKBehavior *) m_Context->GetObjectA(behaviorBlock.id),
                true
            );
        }
    }
}

void Decorator::DecorateBehavior(BehaviorBlock &behaviorBlock, CKBehavior *behavior, int depth) {
    behaviorBlock.id = behavior->GetID();
    behaviorBlock.folded = true;
    behaviorBlock.depth = depth;
    behaviorBlock.isBehaviorGraph = behavior->GetType() != CKBEHAVIORTYPE_BASE;

    // Calculate size based on behavior properties
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

void Decorator::RecalculateAbsolutePositions(BehaviorBlock &behaviorBlock, CKBehavior *behavior, float startHorizontal,
                                             float startVertical) {
    // Reset position for root behavior
    if (behaviorBlock.depth == 0) {
        behaviorBlock.size.hPos = 0;
        behaviorBlock.size.vPos = 0;
    }

    // Apply offset
    behaviorBlock.size.hPos += startHorizontal;
    behaviorBlock.size.vPos += startVertical;

    // Process sub-behaviors and operations if this is a behavior graph
    if (behaviorBlock.isBehaviorGraph) {
        // Process sub-behaviors
        const int subBehaviorCount = behavior->GetSubBehaviorCount();
        for (int i = 0; i < subBehaviorCount; ++i) {
            CKBehavior *subBehavior = behavior->GetSubBehavior(i);
            RecalculateAbsolutePositions(
                GetBehaviorBlock(subBehavior->GetID()),
                subBehavior,
                behaviorBlock.size.hPos,
                behaviorBlock.size.vPos
            );
        }

        // Process operations
        const int operationCount = behavior->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            Operation &operation = GetOperation(behavior->GetParameterOperation(i)->GetID());
            operation.hPos += behaviorBlock.size.hPos;
            operation.vPos += behaviorBlock.size.vPos;
        }
    }
}

Decorator::ParameterPosition Decorator::GetInputParameterPosition(CKParameterIn *inputParam, CKBehavior **owner) {
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

Decorator::ParameterPosition Decorator::GetOutputParameterPosition(CKParameterOut *outputParam,
                                                                   CKBehavior **ownerBehavior) {
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

Decorator::ParameterPosition Decorator::GetLocalParameterPosition(CKParameterLocal *localParam) {
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

LinkEndpoint Decorator::GetParameterEndpoint(CKParameter *parameter) {
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

CKBehavior *Decorator::GetParameterOwnerBehavior(CKParameter *parameter) {
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

Decorator::ParameterPosition Decorator::GetShortcutParameterPosition(CK_ID behaviorId, CK_ID sourceId) {
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

void Decorator::ConfigureParameterLinks(CKBehavior *root) {
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
        }
        // Shared source connection
        else if (inputParam->GetSharedSource()) {
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
                m_Context->OutputToConsoleEx("pin: can't connect %d <-> %d, source type is %d",
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
                    m_Context->OutputToConsoleEx("pout: can't connect %d <-> %d, dest type is %d",
                                                 outputParam->GetID(), destParam->GetID(), destParam->GetClassID());
                }
            }
        }
    }
}

void Decorator::AddGraphEdge(CK_ID sourceId, CK_ID targetId) {
    Edge edge = {};
    edge.sourceId = sourceId;
    edge.targetId = targetId;
    edge.nextEdgeIndex = m_Vertices[edge.sourceId].firstEdgeIndex;
    m_Vertices[edge.sourceId].firstEdgeIndex = m_Edges.size();
    m_Vertices[edge.targetId].incomingEdgeCount++;
    m_Edges.push_back(edge);
}

void Decorator::ConstructGraph(BehaviorBlock &behaviorGraph, CKBehavior *behavior) {
    // Clear existing graph data
    m_Vertices.clear();
    m_Edges.clear();

    // Initialize vertex for root behavior
    m_Vertices[behavior->GetID()] = Vertex();

    // Initialize vertices for sub-behaviors
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    for (int i = 0; i < subBehaviorCount; ++i) {
        CKBehavior *subBehavior = behavior->GetSubBehavior(i);
        m_Vertices[subBehavior->GetID()] = Vertex();
    }

    // Add edges from behavior links (in reverse order)
    for (int i = behaviorGraph.links.size() - 1; i >= 0; --i) {
        Link &link = behaviorGraph.links[i];
        if (link.type == LINK_TYPE_BEHAVIOR) {
            // Behavior link
            AddGraphEdge(link.start.id, link.end.id);
        }
    }

    // Connect unconnected nodes to ensure a connected graph
    CK_ID currentSourceId = behavior->GetID();
    for (auto &vertexPair : m_Vertices) {
        // If node has no incoming edges and isn't the root, create a virtual edge
        if (vertexPair.first != behavior->GetID() && vertexPair.second.incomingEdgeCount == 0) {
            // Add a virtual edge and make them a chain
            AddGraphEdge(currentSourceId, vertexPair.first);
            currentSourceId = vertexPair.first;
        }
    }
}

void Decorator::CalculateDistancesFromQueue(std::queue<CK_ID> &nodeQueue) {
    while (!nodeQueue.empty()) {
        CK_ID currentId = nodeQueue.front();
        nodeQueue.pop();

        // Process all outgoing edges from this node
        for (int edgeIndex = m_Vertices[currentId].firstEdgeIndex;
             edgeIndex != -1;
             edgeIndex = m_Edges[edgeIndex].nextEdgeIndex) {
            CK_ID targetId = m_Edges[edgeIndex].targetId;

            // If destination not yet visited, compute distance and enqueue
            if (m_DistanceFromRoot.find(targetId) == m_DistanceFromRoot.end()) {
                m_DistanceFromRoot[targetId] = m_DistanceFromRoot[currentId] + 1;
                m_PredecessorEdge[targetId] = edgeIndex;
                nodeQueue.push(targetId);
            }
        }
    }
}

void Decorator::CalculateGraphDistances(BehaviorBlock &behaviorGraph) {
    m_DistanceFromRoot.clear();
    m_PredecessorEdge.clear();

    // Start with root node at distance 0
    m_DistanceFromRoot[behaviorGraph.id] = 0;

    std::queue<CK_ID> nodeQueue;
    nodeQueue.push(behaviorGraph.id);
    CalculateDistancesFromQueue(nodeQueue);

    // Check for disconnected components (rare case)
    for (auto &vertexPair : m_Vertices) {
        if (m_DistanceFromRoot.find(vertexPair.first) == m_DistanceFromRoot.end()) {
            // Node not reachable - ignored as mentioned in original code
        }
    }
}

Rect Decorator::CalculateSubgraphSize(BehaviorBlock &behaviorBlock, bool isRoot) {
    CK_ID currentId = behaviorBlock.id;
    Rect size = behaviorBlock.size;

    // Reset size for root node
    if (isRoot) {
        size.hSize = 0.0f;
        size.vSize = 0.0f;
    }

    // Calculate size contribution from child nodes
    int childCount = 0;
    float totalVerticalSize = 0;
    float maxHorizontalSize = 0;

    for (int edgeIndex = m_Vertices[currentId].firstEdgeIndex;
         edgeIndex != -1;
         edgeIndex = m_Edges[edgeIndex].nextEdgeIndex) {
        CK_ID targetId = m_Edges[edgeIndex].targetId;

        // Only consider nodes that are direct children in the shortest path tree
        if (m_PredecessorEdge.find(targetId) != m_PredecessorEdge.end() &&
            m_PredecessorEdge[targetId] == edgeIndex) {
            Rect childSize = CalculateSubgraphSize(GetBehaviorBlock(targetId), false);
            totalVerticalSize += childSize.vSize + 20.0f * 2;
            maxHorizontalSize = std::max(maxHorizontalSize, childSize.hSize);
            childCount++;
        }
    }

    // Adjust vertical size (remove extra padding if multiple children)
    if (childCount > 0) {
        totalVerticalSize -= 20.0f * 2;
    }

    // Calculate final size
    size.vSize = std::max(size.vSize, totalVerticalSize);
    size.hSize = size.hSize + (maxHorizontalSize > 0.0f ? maxHorizontalSize + 20.0f * 2 : 0.0f);

    // Store required size and return
    return m_RequiredSize[behaviorBlock.id] = size;
}

void Decorator::PlaceBehaviorInParent(BehaviorBlock &behaviorBlock, float horizontalPos, float verticalPos,
                                      bool isRoot) {
    // Position the behavior (unless it's the root)
    if (!isRoot) {
        behaviorBlock.size.hPos = horizontalPos;
        behaviorBlock.size.vPos = verticalPos +
            (m_RequiredSize[behaviorBlock.id].vSize - behaviorBlock.size.vSize) / 2;
    }

    // Position all children
    int childCount = 0;
    float currentVerticalOffset = 0;
    CK_ID currentId = behaviorBlock.id;

    for (int edgeIndex = m_Vertices[currentId].firstEdgeIndex;
         edgeIndex != -1;
         edgeIndex = m_Edges[edgeIndex].nextEdgeIndex) {
        CK_ID targetId = m_Edges[edgeIndex].targetId;

        // Only consider nodes that are direct children in the shortest path tree
        if (m_PredecessorEdge.find(targetId) != m_PredecessorEdge.end() &&
            m_PredecessorEdge[targetId] == edgeIndex) {
            Rect childSize = m_RequiredSize[targetId];
            PlaceBehaviorInParent(
                GetBehaviorBlock(targetId),
                horizontalPos + (isRoot ? 20.0f : behaviorBlock.size.hSize + 20.0f * 2),
                verticalPos + currentVerticalOffset,
                false
            );
            currentVerticalOffset += childSize.vSize + 20.0f * 2;
            childCount++;
        }
    }

    // Adjust final vertical size
    if (childCount > 0) {
        currentVerticalOffset -= 20.0f * 2;
    }
}

float Decorator::CalculateBehaviorPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior, bool isScript) {
    // Build the graph representation
    ConstructGraph(behaviorGraph, behavior);

    // Calculate minimum distances from root
    CalculateGraphDistances(behaviorGraph);

    // Calculate required sizes for all nodes
    Rect size = CalculateSubgraphSize(behaviorGraph, true);

    // Calculate expanded sizes
    behaviorGraph.hExpandSize = size.hSize + 20.0f * 4;
    behaviorGraph.vExpandSize = size.vSize + 20.0f * 4;

    // Place behaviors within the graph
    PlaceBehaviorInParent(
        behaviorGraph,
        (isScript ? 140.0f : 0.0f) + 20.0f * 2,
        20.0f * 2,
        true
    );

    // Return vertical center position
    return size.vSize / 2;
}

bool Decorator::IsOperation(CK_ID id) {
    return m_Context->GetObjectA(id)->GetClassID() == CKCID_PARAMETEROPERATION;
}

void Decorator::MoveParameterToPosition(Parameter &parameter, Point position) {
    parameter.hPos = (int) roundf(position.h);
    parameter.vPos = (int) roundf(position.v);
}

void Decorator::MoveOperationToPosition(Operation &operation, Point position) {
    operation.hPos = (position.h - 1) * 20;
    operation.vPos = (position.v - 2) * 20;
}

Point Decorator::GetInterfaceInputPosition(CK_ID targetId, int inputIndex) {
    Point position = {};

    // Handle operation
    if (IsOperation(targetId)) {
        Operation &operation = GetOperation(targetId);
        position.h = roundf(operation.hPos / 20.0f) + inputIndex * 2;
        position.v = roundf(operation.vPos / 20.0f);
    }
    // Handle behavior
    else {
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.hPos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.vPos / 20.0f);
        position.h = horizontalPos + (float) inputIndex;
        position.v = verticalPos - 1.0f;
    }

    return position;
}

Point Decorator::GetInterfaceOutputPosition(CK_ID targetId, int outputIndex) {
    Point position = {};

    // Handle operation
    if (IsOperation(targetId)) {
        Operation &operation = GetOperation(targetId);
        position.h = roundf(operation.hPos / 20.0f) + 1;
        position.v = roundf(operation.vPos / 20.0f) + 2;
    }
    // Handle behavior
    else {
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.hPos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.vPos / 20.0f);
        position.h = horizontalPos + (float) outputIndex;
        position.v = verticalPos + roundf(behaviorBlock.size.vSize / 20.0f) + 1;
    }

    return position;
}

void Decorator::CalculateOperationPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior) {
    // Position operations based on their parameter links
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type == LINK_TYPE_PARAMETER) {
            // Parameter link
            if (paramLink.start.type == ENDPOINT_POUT && IsOperation(paramLink.start.id)) {
                Operation *startOperation = &GetOperation(paramLink.start.id);

                // Position based on destination
                if (paramLink.end.type == ENDPOINT_PIN) {
                    // Normal input
                    MoveOperationToPosition(*startOperation,
                                            GetInterfaceInputPosition(paramLink.end.id, paramLink.end.index));
                } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                    // Target input
                    MoveOperationToPosition(*startOperation,
                                            GetInterfaceInputPosition(paramLink.end.id, -1));
                }
            }
        }
    }
}

void Decorator::CalculateLocalParameterPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior,
                                                 bool isInputDirection) {
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type == LINK_TYPE_PARAMETER) {
            // Parameter link
            if (isInputDirection) {
                // Position source parameters (inputs)
                Parameter *startParam = nullptr;

                if (paramLink.start.type == ENDPOINT_PLOCAL) {
                    // Local parameter
                    startParam = &behaviorGraph.localParams[paramLink.start.index];
                } else if (paramLink.start.type == ENDPOINT_POUT_SHORTCUT) {
                    // Shared parameter
                    startParam = &behaviorGraph.sharedParams[paramLink.start.index];
                }

                if (startParam) {
                    if (paramLink.end.type == ENDPOINT_PIN) {
                        // Normal input
                        MoveParameterToPosition(*startParam,
                                                GetInterfaceInputPosition(paramLink.end.id, paramLink.end.index));
                    } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                        // Target input
                        MoveParameterToPosition(*startParam,
                                                GetInterfaceInputPosition(paramLink.end.id, -1));
                    }
                }
            } else {
                // Position destination parameters (outputs)
                Parameter *endParam = nullptr;

                if (paramLink.end.type == ENDPOINT_PLOCAL) {
                    // Local parameter
                    endParam = &behaviorGraph.localParams[paramLink.end.index];
                }

                if (endParam) {
                    if (paramLink.start.type == ENDPOINT_POUT) {
                        // From output
                        MoveParameterToPosition(*endParam,
                                                GetInterfaceOutputPosition(paramLink.start.id, paramLink.start.index));
                    }
                }
            }
        }
    }
}

void Decorator::CalculateBehaviorSize(BehaviorBlock &behaviorBlock, CKBehavior *behavior) {
    if (behaviorBlock.depth > 0) {
        // Calculate height based on max of inputs and outputs
        int height = std::max(behavior->GetOutputCount(), behavior->GetInputCount());
        height = std::max(height, 1);

        // Calculate width based on max of input and output parameters, or name length
        int width = std::max(behavior->GetOutputParameterCount(), behavior->GetInputParameterCount());
        width = std::max(width, int((strlen(behavior->GetName()) - 1) / 2.5) + 1);
        width = std::max(width, 2);

        // Set size
        behaviorBlock.size.hSize = (float) width * 20.0f;
        behaviorBlock.size.vSize = (float) height * 20.0f;

        // Set expanded size for behavior graphs
        if (behaviorBlock.isBehaviorGraph) {
            behaviorBlock.hExpandSize = behaviorBlock.size.hSize * 10;
            behaviorBlock.vExpandSize = behaviorBlock.size.vSize * 10;
        }
    }
}

void Decorate(InterfaceData &data, CKBehavior *behavior) {
    // Clear any existing data
    data.Clear();

    // Create a decorator and populate the interface data
    Decorator decorator(data, behavior->GetCKContext());
    decorator.Decorate(behavior);
}
