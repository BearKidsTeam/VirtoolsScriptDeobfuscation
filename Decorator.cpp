#include "Decorator.h"

#include <algorithm>

#include "CKAll.h"

#undef min
#undef max

Decorator::Decorator(interface_t &target_data, CKContext *context) : m_data(target_data), m_context(context) {}

bb_t &Decorator::GetBehaviorBlock(CK_ID id) {
    int index = m_behaviorMap[id];
    return (index >= 0) ? m_data.bbs[index] : m_data.script_root;
}

op_t &Decorator::GetOperation(CK_ID id) {
    auto &opInfo = m_operationMap[id];
    int bbIndex = opInfo.first;
    int opIndex = opInfo.second;

    if (bbIndex >= 0) {
        return m_data.bbs[bbIndex].ops[opIndex];
    } else {
        return m_data.script_root.ops[opIndex];
    }
}

void Decorator::DecorateStart(bb_t &script, float verticalStartPos, float verticalSize) {
    m_data.start.id = script.id;
    m_data.start.v_size = verticalSize;
    m_data.start.v_start_pos = verticalStartPos;
    m_data.start.v_start = 0;
}

void Decorator::Decorate(CKBehavior *script) {
    // Populate the interface data
    DecorateBehaviorTree(script);

    // Calculate the height of the behavior block
    float blockHeight = std::max(m_requiredSize[script->GetID()].v_size + 4 * 20.0f, 200.0f);
    float startVertical = blockHeight / 2.0f;

    // Set start information and recalculate positions
    DecorateStart(m_data.script_root, startVertical, blockHeight);
    RecalculateAbsolutePositions(m_data.script_root, script, 0.0f, 0.0f);
}

void Decorator::DecorateBehaviorTree(CKBehavior *rootBehavior) {
    // Initialize data structures
    m_data.n_bb = 0;
    m_data.bbs.clear();
    m_behaviorMap.clear();
    m_operationMap.clear();
    m_inputParams.clear();
    m_outputParams.clear();
    m_movedOperations.clear();

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
            m_data.bbs.emplace_back();
            ++m_data.n_bb;
        }

        // Map behavior ID to index
        m_behaviorMap[currentBehavior->GetID()] = depth > 0 ? m_data.bbs.size() - 1 : -1;

        // Map operation IDs to indices
        const int operationCount = currentBehavior->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            CKParameterOperation *operation = currentBehavior->GetParameterOperation(i);
            m_operationMap[operation->GetID()] = std::make_pair(depth > 0 ? m_data.bbs.size() - 1 : -1, i);
        }

        // Get the behavior block and decorate it
        bb_t &behaviorBlock = depth > 0 ? m_data.bbs.back() : m_data.script_root;
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
    for (auto &pair : m_behaviorMap) {
        bb_t &behaviorBlock = GetBehaviorBlock(pair.first);
        if (behaviorBlock.is_bg) {
            CalculateBehaviorPositions(
                GetBehaviorBlock(pair.first),
                (CKBehavior *) m_context->GetObjectA(behaviorBlock.id),
                behaviorBlock.depth == 0
            );
        }
    }

    // Calculate visual properties
    for (auto &pair : m_behaviorMap) {
        bb_t &behaviorBlock = GetBehaviorBlock(pair.first);
        if (behaviorBlock.is_bg) {
            // Apply multiple passes of operation positioning
            for (int i = 0; i < MAX_FIX_STACK_OPS; ++i) {
                CalculateOperationPositions(
                    GetBehaviorBlock(pair.first),
                    (CKBehavior *) m_context->GetObjectA(behaviorBlock.id)
                );
            }

            // Calculate parameter positions
            CalculateLocalParameterPositions(
                GetBehaviorBlock(pair.first),
                (CKBehavior *) m_context->GetObjectA(behaviorBlock.id),
                false
            );

            CalculateLocalParameterPositions(
                GetBehaviorBlock(pair.first),
                (CKBehavior *) m_context->GetObjectA(behaviorBlock.id),
                true
            );
        }
    }
}

void Decorator::DecorateBehavior(bb_t &behaviorBlock, CKBehavior *behavior, int depth) {
    behaviorBlock.id = behavior->GetID();
    behaviorBlock.folded = true;
    behaviorBlock.depth = depth;
    behaviorBlock.is_bg = behavior->GetType() != CKBEHAVIORTYPE_BASE;

    // Calculate size based on behavior properties
    CalculateBehaviorSize(behaviorBlock, behavior);

    // Track input parameters
    for (int i = 0, count = behavior->GetInputParameterCount(); i < count; ++i) {
        m_inputParams.insert(behavior->GetInputParameter(i)->GetID());
    }

    // Track output parameters
    for (int i = 0, count = behavior->GetOutputParameterCount(); i < count; ++i) {
        m_outputParams.insert(behavior->GetOutputParameter(i)->GetID());
    }

    // Track target parameter if used
    if (behavior->IsUsingTarget()) {
        m_inputParams.insert(behavior->GetTargetParameter()->GetID());
    }

    // Track operation parameters
    for (int i = 0, count = behavior->GetParameterOperationCount(); i < count; ++i) {
        CKParameterOperation *operation = behavior->GetParameterOperation(i);
        m_inputParams.insert(operation->GetInParameter1()->GetID());
        m_inputParams.insert(operation->GetInParameter2()->GetID());
        m_outputParams.insert(operation->GetOutParameter()->GetID());
    }

    // Process behavior links if this is a behavior graph
    if (behaviorBlock.is_bg) {
        // Add behavior links
        for (int i = 0, count = behavior->GetSubBehaviorLinkCount(); i < count; ++i) {
            CKBehaviorLink *behaviorLink = behavior->GetSubBehaviorLink(i);

            link_t link;
            link.type = 1; // Behavior link
            link.id = behaviorLink->GetID();
            link.point_count = 0;
            link.start = link.end = link_endpoint_t();

            // Set start endpoint
            CKBehaviorIO *inputIO = behaviorLink->GetInBehaviorIO();
            CKBehavior *inputBehavior = inputIO->GetOwner();
            link.start.id = inputBehavior->GetID();
            link.start.type = 13; // Output
            link.start.index = inputBehavior->GetOutputPosition(inputIO);

            if (link.start.index == -1) {
                link.start.index = inputBehavior->GetInputPosition(inputIO);
                link.start.type = 12; // Input
                if (inputBehavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                    link.start.type = 26; // Start input
                }
            }

            // Set end endpoint
            CKBehaviorIO *outputIO = behaviorLink->GetOutBehaviorIO();
            CKBehavior *outputBehavior = outputIO->GetOwner();
            link.end.id = outputBehavior->GetID();
            link.end.type = 12; // Input
            link.end.index = outputBehavior->GetInputPosition(outputIO);

            if (link.end.index == -1) {
                link.end.index = outputBehavior->GetOutputPosition(outputIO);
                link.end.type = 13; // Output
            }

            behaviorBlock.links.push_back(link);
        }

        // Add operations
        for (int i = 0, count = behavior->GetParameterOperationCount(); i < count; ++i) {
            CKParameterOperation *operation = behavior->GetParameterOperation(i);

            op_t operationData;
            operationData.id = operation->GetID();
            behaviorBlock.ops.push_back(operationData);
        }
        behaviorBlock.n_ops = behaviorBlock.ops.size();

        // Add local parameters
        for (int i = 0, count = behavior->GetLocalParameterCount(); i < count; ++i) {
            CKParameterLocal *localParam = behavior->GetLocalParameter(i);

            param_t paramData;
            paramData.id = localParam->GetID();
            paramData.style = param_style_closed;
            behaviorBlock.local_params.push_back(paramData);
        }
        behaviorBlock.n_local_param = behaviorBlock.local_params.size();
    }
}

void Decorator::RecalculateAbsolutePositions(bb_t &behaviorBlock, CKBehavior *behavior, float startHorizontal,
                                             float startVertical) {
    // Reset position for root behavior
    if (behaviorBlock.depth == 0) {
        behaviorBlock.size.h_pos = 0;
        behaviorBlock.size.v_pos = 0;
    }

    // Apply offset
    behaviorBlock.size.h_pos += startHorizontal;
    behaviorBlock.size.v_pos += startVertical;

    // Process sub-behaviors and operations if this is a behavior graph
    if (behaviorBlock.is_bg) {
        // Process sub-behaviors
        const int subBehaviorCount = behavior->GetSubBehaviorCount();
        for (int i = 0; i < subBehaviorCount; ++i) {
            CKBehavior *subBehavior = behavior->GetSubBehavior(i);
            RecalculateAbsolutePositions(
                GetBehaviorBlock(subBehavior->GetID()),
                subBehavior,
                behaviorBlock.size.h_pos,
                behaviorBlock.size.v_pos
            );
        }

        // Process operations
        const int operationCount = behavior->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            op_t &operation = GetOperation(behavior->GetParameterOperation(i)->GetID());
            operation.h_pos += behaviorBlock.size.h_pos;
            operation.v_pos += behaviorBlock.size.v_pos;
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

Decorator::ParameterPosition Decorator::GetOutputParameterPosition(CKParameterOut *outputParam, CKBehavior **ownerBehavior) {
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

link_endpoint_t Decorator::GetParameterEndpoint(CKParameter *parameter) {
    // Check if parameter is local
    if (parameter->GetClassID() == CKCID_PARAMETERLOCAL) {
        ParameterPosition position = GetLocalParameterPosition((CKParameterLocal *) parameter);
        return {position.id, position.index, 9}; // Local parameter endpoint
    }

    // Get output parameter endpoint
    CKBehavior *dummy;
    ParameterPosition position = GetOutputParameterPosition((CKParameterOut *) parameter, &dummy);
    return {position.id, position.index, 8}; // Output parameter endpoint
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
    bb_t &behaviorBlock = GetBehaviorBlock(behaviorId);
    for (int i = 0, count = behaviorBlock.n_shared_param; i < count; ++i) {
        if (behaviorBlock.shared_params[i].source_id == sourceId) {
            return {behaviorId, i, behaviorId};
        }
    }

    // Create a new shortcut parameter
    param_t paramData;
    paramData.source_id = sourceId;
    paramData.h_pos = paramData.v_pos = 0;
    paramData.id = sourceId;
    paramData.style = param_style_closed;
    behaviorBlock.shared_params.push_back(paramData);
    ++behaviorBlock.n_shared_param;

    return {behaviorId, behaviorBlock.n_shared_param - 1, behaviorId};
}

void Decorator::ConfigureParameterLinks(CKBehavior *root) {
    // Maps to track parameter chains
    std::map<CK_ID, std::vector<ParameterPosition>> inputChain;
    std::map<CK_ID, std::vector<ParameterPosition>> outputChain;
    CKBehavior *currentBehavior;

    // Process input parameters
    for (auto &id : m_inputParams) {
        auto &positionChain = inputChain[id] = {};
        auto *inputParam = (CKParameterIn *) m_context->GetObject(id);

        positionChain.push_back(GetInputParameterPosition(inputParam, &currentBehavior));
        link_endpoint_t lastEndpoint = {
            positionChain.back().id,
            positionChain.back().index,
            positionChain.back().index == -2 ? 10 : 7
        };

        // Create chain of links for input parameters
        for (; currentBehavior && currentBehavior->GetInputParameterPosition(inputParam) != -1;
               currentBehavior = currentBehavior->GetParent()) {
            positionChain.push_back({
                currentBehavior->GetID(),
                currentBehavior->GetInputParameterPosition(inputParam),
                currentBehavior->GetParent()->GetID()
            });

            link_t link;
            link.id = 0;
            link.type = 0x10002;
            link.point_count = 0;
            link.start = {positionChain.back().id, positionChain.back().index, 7};
            link.end = lastEndpoint;
            lastEndpoint = link.start;

            GetBehaviorBlock(currentBehavior->GetID()).links.push_back(link);
            ++GetBehaviorBlock(currentBehavior->GetID()).n_links;
        }
    }

    // Process output parameters
    for (auto &id : m_outputParams) {
        std::vector<ParameterPosition> &positionChain = outputChain[id] = std::vector<ParameterPosition>();
        CKParameterOut *outputParam = (CKParameterOut *) m_context->GetObject(id);

        positionChain.push_back(GetOutputParameterPosition(outputParam, &currentBehavior));
        link_endpoint_t lastEndpoint = {positionChain.back().id, positionChain.back().index, 8};

        // Create chain of links for output parameters
        for (; currentBehavior && currentBehavior->GetOutputParameterPosition(outputParam) != -1;
               currentBehavior = currentBehavior->GetParent()) {
            positionChain.push_back({
                currentBehavior->GetID(),
                currentBehavior->GetOutputParameterPosition(outputParam),
                currentBehavior->GetParent()->GetID()
            });

            link_t link;
            link.id = 0;
            link.type = 0x10002;
            link.point_count = 0;
            link.end = {positionChain.back().id, positionChain.back().index, 8};
            link.start = lastEndpoint;
            lastEndpoint = link.end;

            GetBehaviorBlock(currentBehavior->GetID()).links.push_back(link);
            ++GetBehaviorBlock(currentBehavior->GetID()).n_links;
        }
    }

    // Connect input parameters to their sources
    for (auto &id : m_inputParams) {
        CKParameterIn *inputParam = (CKParameterIn *) m_context->GetObject(id);
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
                        link_t link;
                        link.id = 0;
                        link.type = 2;
                        link.point_count = 0;
                        link.start = {
                            sourcePos.id,
                            sourcePos.index,
                            sourceParam->GetClassID() == CKCID_PARAMETERLOCAL ? 9 : 8
                        };
                        link.end = {
                            inputPos.id,
                            inputPos.index,
                            inputPos.index == -2 ? 10 : 7
                        };

                        GetBehaviorBlock(inputPos.behaviorId).links.push_back(link);
                        ++GetBehaviorBlock(inputPos.behaviorId).n_links;
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
                link_t link;
                link.id = 0;
                link.type = 2;
                link.point_count = 0;
                ParameterPosition shortcutPos = GetShortcutParameterPosition(position.behaviorId, sourceParam->GetID());
                link.start = {position.behaviorId, shortcutPos.index, 5};
                link.end = {position.id, position.index, position.index == -2 ? 10 : 7};

                GetBehaviorBlock(position.behaviorId).links.push_back(link);
                ++GetBehaviorBlock(position.behaviorId).n_links;
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
                        link_t link;
                        link.id = 0;
                        link.type = 2;
                        link.point_count = 0;
                        link.start = {sharedPos.id, sharedPos.index, 7};
                        link.end = {inputPos.id, inputPos.index, inputPos.index == -2 ? 10 : 7};

                        GetBehaviorBlock(inputPos.behaviorId).links.push_back(link);
                        ++GetBehaviorBlock(inputPos.behaviorId).n_links;
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
                m_context->OutputToConsoleEx("pin: can't connect %d <-> %d, source type is %d",
                                             inputParam->GetID(), sharedInput->GetID(), sharedInput->GetClassID());
            }
        }
    }

    // Connect output parameters to their destinations
    for (auto &id : m_outputParams) {
        CKParameterOut *outputParam = (CKParameterOut *) m_context->GetObject(id);
        std::vector<ParameterPosition> &outputPositions = outputChain[outputParam->GetID()];

        for (int j = 0, destCount = outputParam->GetDestinationCount(); j < destCount; ++j) {
            CKParameter *destParam = outputParam->GetDestination(j);
            link_endpoint_t destEndpoint = GetParameterEndpoint(destParam);
            CK_ID destBehaviorId = destParam->GetOwner()->GetID();

            bool connected = false;

            // Try to find a direct connection within the same behavior
            for (auto &outputPos : outputPositions) {
                if (outputPos.behaviorId == destBehaviorId) {
                    link_t link;
                    link.id = 0;
                    link.type = 2;
                    link.point_count = 0;
                    link.start = {outputPos.id, outputPos.index, 8};
                    link.end = destEndpoint;

                    GetBehaviorBlock(outputPos.behaviorId).links.push_back(link);
                    ++GetBehaviorBlock(outputPos.behaviorId).n_links;
                    connected = true;
                    break;
                }
            }

            // If no direct connection found, handle special cases
            if (!connected) {
                // Handle connection to a local parameter shortcut
                if (destParam->GetClassID() == CKCID_PARAMETERLOCAL) {
                    link_t link;
                    link.id = 0;
                    link.type = 2;
                    link.point_count = 0;
                    ParameterPosition sourcePos = outputPositions.front();
                    ParameterPosition shortcutPos = GetShortcutParameterPosition(
                        sourcePos.behaviorId, destParam->GetID());
                    link.start = {sourcePos.id, sourcePos.index, 8};
                    link.end = {shortcutPos.id, shortcutPos.index, 5};

                    GetBehaviorBlock(sourcePos.behaviorId).links.push_back(link);
                    ++GetBehaviorBlock(sourcePos.behaviorId).n_links;
                } else {
                    // Log warning for other cases
                    m_context->OutputToConsoleEx("pout: can't connect %d <-> %d, dest type is %d",
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
    edge.nextEdgeIndex = m_vertices[edge.sourceId].firstEdgeIndex;
    m_vertices[edge.sourceId].firstEdgeIndex = m_edges.size();
    m_vertices[edge.targetId].incomingEdgeCount++;
    m_edges.push_back(edge);
}

void Decorator::ConstructGraph(bb_t &behaviorGraph, CKBehavior *behavior) {
    // Clear existing graph data
    m_vertices.clear();
    m_edges.clear();

    // Initialize vertex for root behavior
    m_vertices[behavior->GetID()] = Vertex();

    // Initialize vertices for sub-behaviors
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    for (int i = 0; i < subBehaviorCount; ++i) {
        CKBehavior *subBehavior = behavior->GetSubBehavior(i);
        m_vertices[subBehavior->GetID()] = Vertex();
    }

    // Add edges from behavior links (in reverse order)
    for (int i = behaviorGraph.links.size() - 1; i >= 0; --i) {
        link_t &link = behaviorGraph.links[i];
        if (link.type == 1) {
            // Behavior link
            AddGraphEdge(link.start.id, link.end.id);
        }
    }

    // Connect unconnected nodes to ensure a connected graph
    CK_ID currentSourceId = behavior->GetID();
    for (auto &vertexPair : m_vertices) {
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
        for (int edgeIndex = m_vertices[currentId].firstEdgeIndex;
             edgeIndex != -1;
             edgeIndex = m_edges[edgeIndex].nextEdgeIndex) {
            CK_ID targetId = m_edges[edgeIndex].targetId;

            // If destination not yet visited, compute distance and enqueue
            if (m_distanceFromRoot.find(targetId) == m_distanceFromRoot.end()) {
                m_distanceFromRoot[targetId] = m_distanceFromRoot[currentId] + 1;
                m_predecessorEdge[targetId] = edgeIndex;
                nodeQueue.push(targetId);
            }
        }
    }
}

void Decorator::CalculateGraphDistances(bb_t &behaviorGraph) {
    m_distanceFromRoot.clear();
    m_predecessorEdge.clear();

    // Start with root node at distance 0
    m_distanceFromRoot[behaviorGraph.id] = 0;

    std::queue<CK_ID> nodeQueue;
    nodeQueue.push(behaviorGraph.id);
    CalculateDistancesFromQueue(nodeQueue);

    // Check for disconnected components (rare case)
    for (auto &vertexPair : m_vertices) {
        if (m_distanceFromRoot.find(vertexPair.first) == m_distanceFromRoot.end()) {
            // Node not reachable - ignored as mentioned in original code
        }
    }
}

rect_t Decorator::CalculateSubgraphSize(bb_t &behaviorBlock, bool isRoot) {
    CK_ID currentId = behaviorBlock.id;
    rect_t size = behaviorBlock.size;

    // Reset size for root node
    if (isRoot) {
        size.h_size = 0.0f;
        size.v_size = 0.0f;
    }

    // Calculate size contribution from child nodes
    int childCount = 0;
    float totalVerticalSize = 0;
    float maxHorizontalSize = 0;

    for (int edgeIndex = m_vertices[currentId].firstEdgeIndex;
         edgeIndex != -1;
         edgeIndex = m_edges[edgeIndex].nextEdgeIndex) {
        CK_ID targetId = m_edges[edgeIndex].targetId;

        // Only consider nodes that are direct children in the shortest path tree
        if (m_predecessorEdge.find(targetId) != m_predecessorEdge.end() &&
            m_predecessorEdge[targetId] == edgeIndex) {
            rect_t childSize = CalculateSubgraphSize(GetBehaviorBlock(targetId), false);
            totalVerticalSize += childSize.v_size + 20.0f * 2;
            maxHorizontalSize = std::max(maxHorizontalSize, childSize.h_size);
            childCount++;
        }
    }

    // Adjust vertical size (remove extra padding if multiple children)
    if (childCount > 0) {
        totalVerticalSize -= 20.0f * 2;
    }

    // Calculate final size
    size.v_size = std::max(size.v_size, totalVerticalSize);
    size.h_size = size.h_size + (maxHorizontalSize > 0.0f ? maxHorizontalSize + 20.0f * 2 : 0.0f);

    // Store required size and return
    return m_requiredSize[behaviorBlock.id] = size;
}

void Decorator::PlaceBehaviorInParent(bb_t &behaviorBlock, float horizontalPos, float verticalPos, bool isRoot) {
    // Position the behavior (unless it's the root)
    if (!isRoot) {
        behaviorBlock.size.h_pos = horizontalPos;
        behaviorBlock.size.v_pos = verticalPos +
            (m_requiredSize[behaviorBlock.id].v_size - behaviorBlock.size.v_size) / 2;
    }

    // Position all children
    int childCount = 0;
    float currentVerticalOffset = 0;
    CK_ID currentId = behaviorBlock.id;

    for (int edgeIndex = m_vertices[currentId].firstEdgeIndex;
         edgeIndex != -1;
         edgeIndex = m_edges[edgeIndex].nextEdgeIndex) {
        CK_ID targetId = m_edges[edgeIndex].targetId;

        // Only consider nodes that are direct children in the shortest path tree
        if (m_predecessorEdge.find(targetId) != m_predecessorEdge.end() &&
            m_predecessorEdge[targetId] == edgeIndex) {
            rect_t childSize = m_requiredSize[targetId];
            PlaceBehaviorInParent(
                GetBehaviorBlock(targetId),
                horizontalPos + (isRoot ? 20.0f : behaviorBlock.size.h_size + 20.0f * 2),
                verticalPos + currentVerticalOffset,
                false
            );
            currentVerticalOffset += childSize.v_size + 20.0f * 2;
            childCount++;
        }
    }

    // Adjust final vertical size
    if (childCount > 0) {
        currentVerticalOffset -= 20.0f * 2;
    }
}

float Decorator::CalculateBehaviorPositions(bb_t &behaviorGraph, CKBehavior *behavior, bool isScript) {
    // Build the graph representation
    ConstructGraph(behaviorGraph, behavior);

    // Calculate minimum distances from root
    CalculateGraphDistances(behaviorGraph);

    // Calculate required sizes for all nodes
    rect_t size = CalculateSubgraphSize(behaviorGraph, true);

    // Calculate expanded sizes
    behaviorGraph.h_expand_size = size.h_size + 20.0f * 4;
    behaviorGraph.v_expand_size = size.v_size + 20.0f * 4;

    // Place behaviors within the graph
    PlaceBehaviorInParent(
        behaviorGraph,
        (isScript ? 140.0f : 0.0f) + 20.0f * 2,
        20.0f * 2,
        true
    );

    // Return vertical center position
    return size.v_size / 2;
}

bool Decorator::IsOperation(CK_ID id) {
    return m_context->GetObject(id)->GetClassID() == CKCID_PARAMETEROPERATION;
}

void Decorator::MoveParameterToPosition(param_t &parameter, point_t position) {
    parameter.h_pos = (int) roundf(position.h);
    parameter.v_pos = (int) roundf(position.v);
}

void Decorator::MoveOperationToPosition(op_t &operation, point_t position) {
    operation.h_pos = (position.h - 1) * 20;
    operation.v_pos = (position.v - 2) * 20;
}

point_t Decorator::GetInterfaceInputPosition(CK_ID targetId, int inputIndex) {
    point_t position = {};

    // Handle operation
    if (IsOperation(targetId)) {
        op_t &operation = GetOperation(targetId);
        position.h = roundf(operation.h_pos / 20.0f) + inputIndex * 2;
        position.v = roundf(operation.v_pos / 20.0f);
    }
    // Handle behavior
    else {
        bb_t &behaviorBlock = GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.h_pos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.v_pos / 20.0f);
        position.h = horizontalPos + (float) inputIndex;
        position.v = verticalPos - 1.0f;
    }

    return position;
}

point_t Decorator::GetInterfaceOutputPosition(CK_ID targetId, int outputIndex) {
    point_t position = {};

    // Handle operation
    if (IsOperation(targetId)) {
        op_t &operation = GetOperation(targetId);
        position.h = roundf(operation.h_pos / 20.0f) + 1;
        position.v = roundf(operation.v_pos / 20.0f) + 2;
    }
    // Handle behavior
    else {
        bb_t &behaviorBlock = GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.h_pos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.v_pos / 20.0f);
        position.h = horizontalPos + (float) outputIndex;
        position.v = verticalPos + roundf(behaviorBlock.size.v_size / 20.0f) + 1;
    }

    return position;
}

void Decorator::CalculateOperationPositions(bb_t &behaviorGraph, CKBehavior *behavior) {
    // Position operations based on their parameter links
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type == 2) {
            // Parameter link
            if (paramLink.start.type == 8 && IsOperation(paramLink.start.id)) {
                op_t *startOperation = &GetOperation(paramLink.start.id);

                // Position based on destination
                if (paramLink.end.type == 7) {
                    // Normal input
                    MoveOperationToPosition(*startOperation,
                                            GetInterfaceInputPosition(paramLink.end.id, paramLink.end.index));
                } else if (paramLink.end.type == 10) {
                    // Target input
                    MoveOperationToPosition(*startOperation,
                                            GetInterfaceInputPosition(paramLink.end.id, -1));
                }
            }
        }
    }
}

void Decorator::CalculateLocalParameterPositions(bb_t &behaviorGraph, CKBehavior *behavior, bool isInputDirection) {
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type == 2) {
            // Parameter link
            if (isInputDirection) {
                // Position source parameters (inputs)
                param_t *startParam = nullptr;

                if (paramLink.start.type == 9) {
                    // Local parameter
                    startParam = &behaviorGraph.local_params[paramLink.start.index];
                } else if (paramLink.start.type == 5) {
                    // Shared parameter
                    startParam = &behaviorGraph.shared_params[paramLink.start.index];
                }

                if (startParam) {
                    if (paramLink.end.type == 7) {
                        // Normal input
                        MoveParameterToPosition(*startParam,
                                                GetInterfaceInputPosition(paramLink.end.id, paramLink.end.index));
                    } else if (paramLink.end.type == 10) {
                        // Target input
                        MoveParameterToPosition(*startParam,
                                                GetInterfaceInputPosition(paramLink.end.id, -1));
                    }
                }
            } else {
                // Position destination parameters (outputs)
                param_t *endParam = nullptr;

                if (paramLink.end.type == 9) {
                    // Local parameter
                    endParam = &behaviorGraph.local_params[paramLink.end.index];
                }

                if (endParam) {
                    if (paramLink.start.type == 8) {
                        // From output
                        MoveParameterToPosition(*endParam,
                                                GetInterfaceOutputPosition(paramLink.start.id, paramLink.start.index));
                    }
                }
            }
        }
    }
}

void Decorator::CalculateBehaviorSize(bb_t &behaviorBlock, CKBehavior *behavior) {
    if (behaviorBlock.depth > 0) {
        // Calculate height based on max of inputs and outputs
        int height = std::max(behavior->GetOutputCount(), behavior->GetInputCount());
        height = std::max(height, 1);

        // Calculate width based on max of input and output parameters, or name length
        int width = std::max(behavior->GetOutputParameterCount(), behavior->GetInputParameterCount());
        width = std::max(width, int((strlen(behavior->GetName()) - 1) / 2.5) + 1);
        width = std::max(width, 2);

        // Set size
        behaviorBlock.size.h_size = (float) width * 20.0f;
        behaviorBlock.size.v_size = (float) height * 20.0f;

        // Set expanded size for behavior graphs
        if (behaviorBlock.is_bg) {
            behaviorBlock.h_expand_size = behaviorBlock.size.h_size * 10;
            behaviorBlock.v_expand_size = behaviorBlock.size.v_size * 10;
        }
    }
}

void Decorate(interface_t &data, CKBehavior *behavior) {
    Decorator decorator(data, behavior->GetCKContext());
    decorator.Decorate(behavior);
}
