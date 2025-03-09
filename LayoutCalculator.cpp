#include "LayoutCalculator.h"

#include <stdexcept>

#include "CKAll.h"

#undef min
#undef max

LayoutCalculator::LayoutCalculator(InterfaceData &target_data, CKContext *context)
    : m_Data(target_data), m_Context(context) {}

void LayoutCalculator::CalculateLayout(CKBehavior *script) {
    // Get ordered behavior IDs
    const auto behaviorIds = GetBehaviorIds();

    // Clear data structures to ensure consistent ordering
    m_VertexIds.clear();
    m_DistanceIds.clear();
    m_SizeIds.clear();
    m_PredecessorIds.clear();
    m_Vertices.clear();
    m_DistanceFromRoot.clear();
    m_RequiredSize.clear();
    m_PredecessorEdge.clear();
    m_Edges.clear();
    m_MovedOperations.clear();

    // Calculate layout for each behavior block in the order they were inserted
    for (auto &behaviorId : behaviorIds) {
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(behaviorId);
        if (behaviorBlock.isBehaviorGraph) {
            CalculateBehaviorPositions(
                behaviorBlock,
                (CKBehavior *) m_Context->GetObject(behaviorBlock.id),
                behaviorBlock.depth == 0
            );
        }
    }

    // Calculate visual properties in the same order
    for (auto &behaviorId : behaviorIds) {
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(behaviorId);
        if (behaviorBlock.isBehaviorGraph) {
            // Apply multiple passes of operation positioning
            for (int i = 0; i < MAX_FIX_STACK_OPS; ++i) {
                CalculateOperationPositions(
                    behaviorBlock,
                    (CKBehavior *) m_Context->GetObject(behaviorBlock.id)
                );
            }

            // Calculate parameter positions
            CalculateLocalParameterPositions(
                behaviorBlock,
                (CKBehavior *) m_Context->GetObject(behaviorBlock.id),
                false
            );

            CalculateLocalParameterPositions(
                behaviorBlock,
                (CKBehavior *) m_Context->GetObject(behaviorBlock.id),
                true
            );
        }
    }

    // Calculate the height of the behavior block
    float blockHeight = std::max(m_RequiredSize[script->GetID()].vSize + 4 * 20.0f, 200.0f);
    float startVertical = blockHeight / 2.0f;

    // Set start information and recalculate positions
    DecorateStart(m_Data.scriptRoot, startVertical, blockHeight);
    RecalculateAbsolutePositions(m_Data.scriptRoot, script, 0.0f, 0.0f);
}

BehaviorBlock &LayoutCalculator::GetBehaviorBlock(CK_ID id) {
    // Check if the ID is the script root
    if (m_Data.scriptRoot.id == id) {
        return m_Data.scriptRoot;
    }

    // Search for the block in the behavior blocks
    for (auto &block : m_Data.behaviorBlocks) {
        if (block.id == id) {
            return block;
        }
    }

    // If not found, throw an exception
    throw std::runtime_error("Behavior block not found with ID: " + std::to_string(id));
}

Operation &LayoutCalculator::GetOperation(CK_ID id) {
    // First check script root operations
    for (auto &op : m_Data.scriptRoot.operations) {
        if (op.id == id) {
            return op;
        }
    }

    // Check all behavior blocks
    for (auto &block : m_Data.behaviorBlocks) {
        for (auto &op : block.operations) {
            if (op.id == id) {
                return op;
            }
        }
    }

    // If not found, throw an exception
    throw std::runtime_error("Operation not found with ID: " + std::to_string(id));
}

bool LayoutCalculator::IsOperation(CK_ID id) const {
    CKObject *obj = m_Context->GetObject(id);
    return obj && obj->GetClassID() == CKCID_PARAMETEROPERATION;
}

std::vector<CK_ID> LayoutCalculator::GetBehaviorIds() const {
    std::vector<CK_ID> behaviorIds;

    // Add script root
    behaviorIds.push_back(m_Data.scriptRoot.id);

    // Add all behavior blocks
    for (const auto &block : m_Data.behaviorBlocks) {
        behaviorIds.push_back(block.id);
    }

    return behaviorIds;
}

std::vector<CK_ID> LayoutCalculator::GetOperationIds() const {
    std::vector<CK_ID> operationIds;

    // Add script root operations
    for (const auto &op : m_Data.scriptRoot.operations) {
        operationIds.push_back(op.id);
    }

    // Add all behavior block operations
    for (const auto &block : m_Data.behaviorBlocks) {
        for (const auto &op : block.operations) {
            operationIds.push_back(op.id);
        }
    }

    return operationIds;
}

void LayoutCalculator::AddGraphEdge(CK_ID sourceId, CK_ID targetId) {
    Edge edge = {};
    edge.sourceId = sourceId;
    edge.targetId = targetId;
    edge.nextEdgeIndex = m_Vertices[sourceId].firstEdgeIndex;
    m_Vertices[sourceId].firstEdgeIndex = m_Edges.size();
    m_Vertices[targetId].incomingEdgeCount++;
    m_Edges.push_back(edge);
}

void LayoutCalculator::ConstructGraph(BehaviorBlock &behaviorGraph, CKBehavior *behavior) {
    // Clear existing graph data
    m_Vertices.clear();
    m_VertexIds.clear();
    m_Edges.clear();

    // Initialize vertex for root behavior
    m_Vertices[behavior->GetID()] = Vertex();
    m_VertexIds.push_back(behavior->GetID());

    // Initialize vertices for sub-behaviors
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    for (int i = 0; i < subBehaviorCount; ++i) {
        CKBehavior *subBehavior = behavior->GetSubBehavior(i);
        m_Vertices[subBehavior->GetID()] = Vertex();
        m_VertexIds.push_back(subBehavior->GetID());
    }

    // Add edges from behavior links (in reverse order)
    for (auto it = behaviorGraph.links.rbegin(); it != behaviorGraph.links.rend(); ++it) {
        Link &link = *it;
        if (link.type == LINK_TYPE_BEHAVIOR) {
            // Behavior link
            AddGraphEdge(link.start.id, link.end.id);
        }
    }

    // Connect unconnected nodes to ensure a connected graph
    CK_ID currentSourceId = behavior->GetID();
    // Iterate through vertices in insertion order
    for (const auto &vertexId : m_VertexIds) {
        // If node has no incoming edges and isn't the root, create a virtual edge
        if (vertexId != behavior->GetID() && m_Vertices[vertexId].incomingEdgeCount == 0) {
            // Add a virtual edge and make them a chain
            AddGraphEdge(currentSourceId, vertexId);
            currentSourceId = vertexId;
        }
    }
}

void LayoutCalculator::CalculateDistancesFromQueue(std::queue<CK_ID> &nodeQueue) {
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
                m_DistanceIds.push_back(targetId);
                m_PredecessorEdge[targetId] = edgeIndex;
                m_PredecessorIds.push_back(targetId);
                nodeQueue.push(targetId);
            }
        }
    }
}

void LayoutCalculator::CalculateGraphDistances(BehaviorBlock &behaviorGraph) {
    m_DistanceFromRoot.clear();
    m_DistanceIds.clear();
    m_PredecessorEdge.clear();
    m_PredecessorIds.clear();

    // Start with root node at distance 0
    m_DistanceFromRoot[behaviorGraph.id] = 0;
    m_DistanceIds.push_back(behaviorGraph.id);

    std::queue<CK_ID> nodeQueue;
    nodeQueue.push(behaviorGraph.id);
    CalculateDistancesFromQueue(nodeQueue);

    // Check for disconnected components (rare case)
    for (const auto &vertexId : m_VertexIds) {
        if (m_DistanceFromRoot.find(vertexId) == m_DistanceFromRoot.end()) {
            // Node not reachable - ignored as it should be handled by virtual edges
        }
    }
}

Rect LayoutCalculator::CalculateSubgraphSize(BehaviorBlock &behaviorBlock, bool isRoot) {
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
    m_RequiredSize[behaviorBlock.id] = size;
    m_SizeIds.push_back(behaviorBlock.id);
    return size;
}

void LayoutCalculator::PlaceBehaviorInParent(BehaviorBlock &behaviorBlock, float horizontalPos, float verticalPos,
                                            bool isRoot) {
    // Position the behavior (unless it's the root)
    if (!isRoot) {
        behaviorBlock.size.hPos = horizontalPos;
        behaviorBlock.size.vPos = verticalPos + (m_RequiredSize[behaviorBlock.id].vSize - behaviorBlock.size.vSize) / 2;
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

float LayoutCalculator::CalculateBehaviorPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior, bool isScript) {
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

void LayoutCalculator::MoveParameterToPosition(Parameter &parameter, Point position) {
    parameter.hPos = static_cast<int>(roundf(position.h));
    parameter.vPos = static_cast<int>(roundf(position.v));
}

void LayoutCalculator::MoveOperationToPosition(Operation &operation, Point position) {
    operation.hPos = (position.h - 1) * 20;
    operation.vPos = (position.v - 2) * 20;
}

Point LayoutCalculator::GetInterfaceInputPosition(CK_ID targetId, int inputIndex) {
    Point position = {};

    // Handle operation
    if (IsOperation(targetId)) {
        Operation &operation = GetOperation(targetId);
        position.h = roundf(operation.hPos / 20.0f) + inputIndex * 2;
        position.v = roundf(operation.vPos / 20.0f);
    } else {
        // Handle behavior
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.hPos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.vPos / 20.0f);
        position.h = horizontalPos + static_cast<float>(inputIndex);
        position.v = verticalPos - 1.0f;
    }

    return position;
}

Point LayoutCalculator::GetInterfaceOutputPosition(CK_ID targetId, int outputIndex) {
    Point position = {};

    // Handle operation
    if (IsOperation(targetId)) {
        Operation &operation = GetOperation(targetId);
        position.h = roundf(operation.hPos / 20.0f) + 1;
        position.v = roundf(operation.vPos / 20.0f) + 2;
    } else {
        // Handle behavior
        BehaviorBlock &behaviorBlock = GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.hPos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.vPos / 20.0f);
        position.h = horizontalPos + static_cast<float>(outputIndex);
        position.v = verticalPos + roundf(behaviorBlock.size.vSize / 20.0f) + 1;
    }

    return position;
}

void LayoutCalculator::CalculateOperationPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior) {
    // Position operations based on their parameter links
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type == LINK_TYPE_PARAMETER) {
            // Parameter link
            if (paramLink.start.type == ENDPOINT_POUT && IsOperation(paramLink.start.id)) {
                // Skip if already moved
                if (m_MovedOperations.find(paramLink.start.id) != m_MovedOperations.end()) {
                    continue;
                }

                Operation *startOperation = &GetOperation(paramLink.start.id);

                // Position based on destination
                if (paramLink.end.type == ENDPOINT_PIN) {
                    // Normal input
                    MoveOperationToPosition(*startOperation,
                                           GetInterfaceInputPosition(paramLink.end.id, paramLink.end.index));
                    m_MovedOperations.insert(paramLink.start.id);
                } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                    // Target input
                    MoveOperationToPosition(*startOperation,
                                           GetInterfaceInputPosition(paramLink.end.id, -1));
                    m_MovedOperations.insert(paramLink.start.id);
                }
            }
        }
    }
}

void LayoutCalculator::CalculateLocalParameterPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior,
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

void LayoutCalculator::DecorateStart(BehaviorBlock &script, float verticalStartPos, float verticalSize) {
    m_Data.start.id = script.id;
    m_Data.start.vSize = verticalSize;
    m_Data.start.vStartPos = verticalStartPos;
    m_Data.start.vStart = 0;
}

void LayoutCalculator::RecalculateAbsolutePositions(BehaviorBlock &behaviorBlock, CKBehavior *behavior,
                                                   float startHorizontal, float startVertical) {
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