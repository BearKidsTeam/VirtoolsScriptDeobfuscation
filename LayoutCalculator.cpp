#include "LayoutCalculator.h"
#include "CKAll.h"

#undef min
#undef max

LayoutCalculator::LayoutCalculator(InterfaceData &target_data, CKContext *context, GraphBuilder &graph_builder)
    : m_Data(target_data), m_Context(context), m_GraphBuilder(graph_builder) {}

void LayoutCalculator::DecorateStart(BehaviorBlock &script, float verticalStartPos, float verticalSize) {
    m_Data.start.id = script.id;
    m_Data.start.vSize = verticalSize;
    m_Data.start.vStartPos = verticalStartPos;
    m_Data.start.vStart = 0;
}

void LayoutCalculator::CalculateLayout(CKBehavior *script) {
    // Get behavior map from the graph builder
    const auto& behaviorMap = m_GraphBuilder.GetBehaviorMap();

    // Calculate layout for each behavior block
    for (auto &pair : behaviorMap) {
        BehaviorBlock &behaviorBlock = m_GraphBuilder.GetBehaviorBlock(pair.first);
        if (behaviorBlock.isBehaviorGraph) {
            CalculateBehaviorPositions(
                m_GraphBuilder.GetBehaviorBlock(pair.first),
                (CKBehavior *) m_Context->GetObject(behaviorBlock.id),
                behaviorBlock.depth == 0
            );
        }
    }

    // Calculate visual properties
    for (auto &pair : behaviorMap) {
        BehaviorBlock &behaviorBlock = m_GraphBuilder.GetBehaviorBlock(pair.first);
        if (behaviorBlock.isBehaviorGraph) {
            // Apply multiple passes of operation positioning
            for (int i = 0; i < MAX_FIX_STACK_OPS; ++i) {
                CalculateOperationPositions(
                    m_GraphBuilder.GetBehaviorBlock(pair.first),
                    (CKBehavior *) m_Context->GetObject(behaviorBlock.id)
                );
            }

            // Calculate parameter positions
            CalculateLocalParameterPositions(
                m_GraphBuilder.GetBehaviorBlock(pair.first),
                (CKBehavior *) m_Context->GetObject(behaviorBlock.id),
                false
            );

            CalculateLocalParameterPositions(
                m_GraphBuilder.GetBehaviorBlock(pair.first),
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
                m_GraphBuilder.GetBehaviorBlock(subBehavior->GetID()),
                subBehavior,
                behaviorBlock.size.hPos,
                behaviorBlock.size.vPos
            );
        }

        // Process operations
        const int operationCount = behavior->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            Operation &operation = m_GraphBuilder.GetOperation(behavior->GetParameterOperation(i)->GetID());
            operation.hPos += behaviorBlock.size.hPos;
            operation.vPos += behaviorBlock.size.vPos;
        }
    }
}

void LayoutCalculator::AddGraphEdge(CK_ID sourceId, CK_ID targetId) {
    Edge edge = {};
    edge.sourceId = sourceId;
    edge.targetId = targetId;
    edge.nextEdgeIndex = m_Vertices[edge.sourceId].firstEdgeIndex;
    m_Vertices[edge.sourceId].firstEdgeIndex = m_Edges.size();
    m_Vertices[edge.targetId].incomingEdgeCount++;
    m_Edges.push_back(edge);
}

void LayoutCalculator::ConstructGraph(BehaviorBlock &behaviorGraph, CKBehavior *behavior) {
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
                m_PredecessorEdge[targetId] = edgeIndex;
                nodeQueue.push(targetId);
            }
        }
    }
}

void LayoutCalculator::CalculateGraphDistances(BehaviorBlock &behaviorGraph) {
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
            Rect childSize = CalculateSubgraphSize(m_GraphBuilder.GetBehaviorBlock(targetId), false);
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

void LayoutCalculator::PlaceBehaviorInParent(BehaviorBlock &behaviorBlock, float horizontalPos, float verticalPos, bool isRoot) {
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
                m_GraphBuilder.GetBehaviorBlock(targetId),
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
    parameter.hPos = (int) roundf(position.h);
    parameter.vPos = (int) roundf(position.v);
}

void LayoutCalculator::MoveOperationToPosition(Operation &operation, Point position) {
    operation.hPos = (position.h - 1) * 20;
    operation.vPos = (position.v - 2) * 20;
}

Point LayoutCalculator::GetInterfaceInputPosition(CK_ID targetId, int inputIndex) {
    Point position = {};

    // Handle operation
    if (m_GraphBuilder.IsOperation(targetId)) {
        Operation &operation = m_GraphBuilder.GetOperation(targetId);
        position.h = roundf(operation.hPos / 20.0f) + inputIndex * 2;
        position.v = roundf(operation.vPos / 20.0f);
    } else {
        // Handle behavior
        BehaviorBlock &behaviorBlock = m_GraphBuilder.GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.hPos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.vPos / 20.0f);
        position.h = horizontalPos + (float) inputIndex;
        position.v = verticalPos - 1.0f;
    }

    return position;
}

Point LayoutCalculator::GetInterfaceOutputPosition(CK_ID targetId, int outputIndex) {
    Point position = {};

    // Handle operation
    if (m_GraphBuilder.IsOperation(targetId)) {
        Operation &operation = m_GraphBuilder.GetOperation(targetId);
        position.h = roundf(operation.hPos / 20.0f) + 1;
        position.v = roundf(operation.vPos / 20.0f) + 2;
    } else {
        // Handle behavior
        BehaviorBlock &behaviorBlock = m_GraphBuilder.GetBehaviorBlock(targetId);
        float horizontalPos = roundf(behaviorBlock.size.hPos / 20.0f);
        float verticalPos = roundf(behaviorBlock.size.vPos / 20.0f);
        position.h = horizontalPos + (float) outputIndex;
        position.v = verticalPos + roundf(behaviorBlock.size.vSize / 20.0f) + 1;
    }

    return position;
}

void LayoutCalculator::CalculateOperationPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior) {
    // Position operations based on their parameter links
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type == LINK_TYPE_PARAMETER) {
            // Parameter link
            if (paramLink.start.type == ENDPOINT_POUT && m_GraphBuilder.IsOperation(paramLink.start.id)) {
                Operation *startOperation = &m_GraphBuilder.GetOperation(paramLink.start.id);

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

void LayoutCalculator::CalculateBehaviorSize(BehaviorBlock &behaviorBlock, CKBehavior *behavior) {
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