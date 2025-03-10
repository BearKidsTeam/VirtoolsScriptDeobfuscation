#include "LayoutCalculator.h"

#include <algorithm>
#include <stdexcept>

#include "CKAll.h"

#undef min
#undef max

LayoutCalculator::LayoutCalculator(InterfaceData &targetData, CKContext *context)
    : m_Data(targetData), m_Context(context) {}

void LayoutCalculator::CalculateLayout(CKBehavior *script) {
    // Get ordered behavior IDs
    const auto behaviorIds = GetBehaviorIds();

    // Clear data structures to ensure consistent ordering
    m_Vertices.clear();
    m_DistanceFromRoot.clear();
    m_RequiredSize.clear();
    m_PredecessorEdge.clear();
    m_Edges.clear();
    m_MovedOperations.clear();

    // Calculate layout for each behavior in the order they were inserted
    for (auto &behaviorId : behaviorIds) {
        BehaviorData *behaviorData = GetBehaviorData(behaviorId);
        if (behaviorData && behaviorData->isBehaviorGraph) {
            CalculateBehaviorPositions(
                *behaviorData,
                (CKBehavior *) m_Context->GetObject(behaviorData->id),
                behaviorData->depth == 0
            );
        }
    }

    // Calculate visual properties in the same order
    for (auto &behaviorId : behaviorIds) {
        BehaviorData *behaviorData = GetBehaviorData(behaviorId);
        if (behaviorData && behaviorData->isBehaviorGraph) {
            // Apply multiple passes of operation positioning
            for (int i = 0; i < MAX_FIX_STACK_OPS; ++i) {
                CalculateOperationPositions(*behaviorData);
            }

            // Calculate parameter positions
            CalculateLocalParameterPositions(*behaviorData, false);
            CalculateLocalParameterPositions(*behaviorData, true);
        }
    }

    // Calculate the height of the behavior
    float behaviorHeight = std::max(m_RequiredSize[script->GetID()].vSize + 4 * 20.0f, 200.0f);
    float startVertical = behaviorHeight / 2.0f;

    // Set start information and recalculate positions
    SetStart(m_Data.rootBehavior, startVertical, behaviorHeight);
    RecalculateAbsolutePositions(m_Data.rootBehavior, script, 0.0f, 0.0f);

    m_Data.NotifyObservers(nullptr, InterfaceData::ElementAction::Modified);
}

BehaviorData *LayoutCalculator::GetBehaviorData(CK_ID id) const {
    if (m_Data.rootBehavior.id == id) {
        return &m_Data.rootBehavior;
    }

    for (auto &behavior : m_Data.behaviors) {
        if (behavior.id == id) {
            return &behavior;
        }
    }

    return nullptr;
}

Operation *LayoutCalculator::GetOperation(CK_ID id) const {
    // Check root behavior operations
    for (auto &op : m_Data.rootBehavior.operations) {
        if (op.id == id) {
            return &op;
        }
    }

    // Check all other behaviors
    for (auto &behavior : m_Data.behaviors) {
        for (auto &op : behavior.operations) {
            if (op.id == id) {
                return &op;
            }
        }
    }

    return nullptr;
}

bool LayoutCalculator::IsOperation(CK_ID id) const {
    CKObject *obj = m_Context->GetObject(id);
    return obj && obj->GetClassID() == CKCID_PARAMETEROPERATION;
}


Parameter *LayoutCalculator::GetParameter(CK_ID behaviorId, int index, bool isLocal) const {
    BehaviorData *behaviorData = GetBehaviorData(behaviorId);
    if (!behaviorData) return nullptr;

    if (isLocal) {
        if (index >= 0 && index < behaviorData->localParams.size()) {
            return &behaviorData->localParams[index];
        }
    } else {
        if (index >= 0 && index < behaviorData->sharedParams.size()) {
            return &behaviorData->sharedParams[index];
        }
    }

    return nullptr;
}

Parameter *LayoutCalculator::GetSharedParameter(CK_ID behaviorId, int index) const {
    return GetParameter(behaviorId, index, false);
}

std::vector<CK_ID> LayoutCalculator::GetBehaviorIds() const {
    std::vector<CK_ID> behaviorIds;

    // Add script root
    behaviorIds.push_back(m_Data.rootBehavior.id);

    // Add all behaviors
    for (const auto &behavior : m_Data.behaviors) {
        behaviorIds.push_back(behavior.id);
    }

    return behaviorIds;
}

std::vector<CK_ID> LayoutCalculator::GetOperationIds() const {
    std::vector<CK_ID> operationIds;

    // Add script root operations
    operationIds.reserve(m_Data.rootBehavior.operations.size());
    for (const auto &op : m_Data.rootBehavior.operations) {
        operationIds.push_back(op.id);
    }

    // Add all behavior operations
    for (const auto &behavior : m_Data.behaviors) {
        for (const auto &op : behavior.operations) {
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

void LayoutCalculator::ConstructGraph(BehaviorData &behaviorGraph, CKBehavior *behavior) {
    // Initialize graph structures
    m_Vertices.clear();
    m_Edges.clear();

    const CK_ID rootId = behavior->GetID();

    // Create root vertex
    m_Vertices[rootId] = Vertex();

    // Create vertices for all sub-behaviors
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    for (int i = 0; i < subBehaviorCount; ++i) {
        CKBehavior *subBehavior = behavior->GetSubBehavior(i);
        if (subBehavior) {
            m_Vertices[subBehavior->GetID()] = Vertex();
        }
    }

    // Skip further processing if no sub-behaviors exist
    if (subBehaviorCount == 0) {
        return;
    }

    // Collect and filter valid behavior links
    std::vector<Link *> behaviorLinks;
    for (auto &link : behaviorGraph.links) {
        if (link.IsBehaviorLink()) {
            // Only include links between known behaviors
            if (m_Vertices.find(link.start.id) != m_Vertices.end() &&
                m_Vertices.find(link.end.id) != m_Vertices.end()) {
                behaviorLinks.push_back(&link);
            }
        }
    }

    // Handle the case where no valid links exist
    if (behaviorLinks.empty()) {
        ConnectDisconnectedBehaviorsToRoot(behavior);
        return;
    }

    // Sort links in proper order for left-to-right layout
    SortBehaviorLinks(behaviorLinks);

    // Add edges to graph in sorted order
    for (Link *link : behaviorLinks) {
        AddGraphEdge(link->start.id, link->end.id);
    }

    // Connect any remaining unconnected behaviors
    ConnectOrphanedBehaviors(behaviorGraph, rootId);
}

void LayoutCalculator::SortBehaviorLinks(std::vector<Link *> &behaviorLinks) {
    // Sort links by:
    // 1. Source behavior ID (descending)
    // 2. Output index (descending)
    // 3. Target behavior ID (descending)
    // 4. Input index (descending)
    std::sort(behaviorLinks.begin(), behaviorLinks.end(),
              [](const Link *a, const Link *b) {
                  // Compare source behaviors
                  if (a->start.id != b->start.id) {
                      return a->start.id > b->start.id;
                  }

                  // Compare output indices
                  if (a->start.index != b->start.index) {
                      return a->start.index > b->start.index;
                  }

                  // Compare target behaviors
                  if (a->end.id != b->end.id) {
                      return a->end.id > b->end.id;
                  }

                  // Compare input indices
                  if (a->end.index != b->end.index) {
                      return a->end.index > b->end.index;
                  }

                  // Use link ID for stable sorting
                  return a->id > b->id;
              });
}

void LayoutCalculator::ConnectDisconnectedBehaviorsToRoot(CKBehavior *behavior) {
    // For an empty graph, connect all behaviors to root in sequential chain
    CK_ID prevId = behavior->GetID();
    const int subCount = behavior->GetSubBehaviorCount();

    for (int i = 0; i < subCount; ++i) {
        CKBehavior *subBehavior = behavior->GetSubBehavior(i);
        if (subBehavior) {
            CK_ID currentId = subBehavior->GetID();
            AddGraphEdge(prevId, currentId);
            prevId = currentId;
        }
    }
}

void LayoutCalculator::ConnectOrphanedBehaviors(BehaviorData &behaviorGraph, CK_ID rootId) {
    // Find behaviors without incoming edges and connect them

    // Create a list of orphaned behaviors with their display position
    struct OrphanedBehavior {
        CK_ID id;
        float verticalPosition;
    };

    std::vector<OrphanedBehavior> orphanedBehaviors;

    for (const auto &vertexPair : m_Vertices) {
        CK_ID behaviorId = vertexPair.first;
        const Vertex &vertex = vertexPair.second;

        // Skip root and behaviors with incoming edges
        if (behaviorId != rootId && vertex.incomingEdgeCount == 0) {
            float vPos = 0.0f;

            // Find the behavior data to get vertical position
            BehaviorData *behaviorData = nullptr;
            if (behaviorId == behaviorGraph.id) {
                behaviorData = &behaviorGraph;
            } else {
                for (auto &behavior : m_Data.behaviors) {
                    if (behavior.id == behaviorId) {
                        behaviorData = &behavior;
                        break;
                    }
                }
            }

            // Get vertical position if behavior data was found
            if (behaviorData) {
                vPos = behaviorData->rect.vPos;
            }

            orphanedBehaviors.push_back({behaviorId, vPos});
        }
    }

    // Sort orphaned behaviors by vertical position (descending)
    std::sort(orphanedBehaviors.begin(), orphanedBehaviors.end(),
              [](const OrphanedBehavior &a, const OrphanedBehavior &b) {
                  return a.verticalPosition > b.verticalPosition;
              });

    // Connect orphaned behaviors to form a chain
    CK_ID sourceId = rootId;
    for (const auto &orphan : orphanedBehaviors) {
        AddGraphEdge(sourceId, orphan.id);
        sourceId = orphan.id;
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

void LayoutCalculator::CalculateGraphDistances(BehaviorData &behaviorGraph) {
    m_DistanceFromRoot.clear();
    m_PredecessorEdge.clear();

    // Start with root node at distance 0
    m_DistanceFromRoot[behaviorGraph.id] = 0;

    std::queue<CK_ID> nodeQueue;
    nodeQueue.push(behaviorGraph.id);
    CalculateDistancesFromQueue(nodeQueue);

    // Check for disconnected components (rare case)
    for (const auto &vertex : m_Vertices) {
        CK_ID vertexId = vertex.first;
        if (m_DistanceFromRoot.find(vertexId) == m_DistanceFromRoot.end()) {
            // Node not reachable - ignored as it should be handled by virtual edges
        }
    }
}

Rect LayoutCalculator::CalculateSubgraphSize(BehaviorData &behaviorData, bool isRoot) {
    CK_ID currentId = behaviorData.id;
    Rect size = behaviorData.rect;

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
        BehaviorData *targetData = GetBehaviorData(targetId);
        if (!targetData) continue;

        // Only consider nodes that are direct children in the shortest path tree
        if (m_PredecessorEdge.find(targetId) != m_PredecessorEdge.end() && m_PredecessorEdge[targetId] == edgeIndex) {
            Rect childSize = CalculateSubgraphSize(*targetData, false);
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
    m_RequiredSize[behaviorData.id] = size;
    return size;
}

void LayoutCalculator::PlaceBehaviorInParent(BehaviorData &behaviorData, float horizontalPos, float verticalPos,
                                             bool isRoot) {
    // Position the behavior (unless it's the root)
    if (!isRoot) {
        behaviorData.rect.hPos = horizontalPos;
        behaviorData.rect.vPos = verticalPos + (m_RequiredSize[behaviorData.id].vSize - behaviorData.rect.vSize) / 2;
    }

    // Position all children
    int childCount = 0;
    float currentVerticalOffset = 0;
    CK_ID currentId = behaviorData.id;

    for (int edgeIndex = m_Vertices[currentId].firstEdgeIndex;
         edgeIndex != -1;
         edgeIndex = m_Edges[edgeIndex].nextEdgeIndex) {
        CK_ID targetId = m_Edges[edgeIndex].targetId;
        BehaviorData *targetData = GetBehaviorData(targetId);
        if (!targetData) continue;

        // Only consider nodes that are direct children in the shortest path tree
        if (m_PredecessorEdge.find(targetId) != m_PredecessorEdge.end() &&
            m_PredecessorEdge[targetId] == edgeIndex) {
            const Rect &childSize = m_RequiredSize[targetId];
            PlaceBehaviorInParent(
                *targetData,
                horizontalPos + (isRoot ? 20.0f : behaviorData.rect.hSize + 20.0f * 2),
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

float LayoutCalculator::CalculateBehaviorPositions(BehaviorData &behaviorGraph, CKBehavior *behavior, bool isScript) {
    // Build the graph representation
    ConstructGraph(behaviorGraph, behavior);

    // Calculate minimum distances from root
    CalculateGraphDistances(behaviorGraph);

    // Calculate required sizes for all nodes
    const Rect size = CalculateSubgraphSize(behaviorGraph, true);

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

void LayoutCalculator::MoveParameterToPosition(Parameter &parameter, const Point &position) {
    parameter.hPos = static_cast<int>(roundf(position.h));
    parameter.vPos = static_cast<int>(roundf(position.v));
}

void LayoutCalculator::MoveOperationToPosition(Operation &operation, const Point &position) {
    operation.hPos = (position.h - 1) * 20;
    operation.vPos = (position.v - 2) * 20;
}

Point LayoutCalculator::GetInputParamPosition(CK_ID targetId, int inputIndex) {
    Point position;

    // Handle operation
    if (IsOperation(targetId)) {
        Operation *operation = GetOperation(targetId);
        if (operation) {
            position.h = roundf(operation->hPos / 20.0f) + inputIndex * 2;
            position.v = roundf(operation->vPos / 20.0f);
        }
    } else {
        // Handle behavior
        BehaviorData *behaviorData = GetBehaviorData(targetId);
        if (behaviorData) {
            float horizontalPos = roundf(behaviorData->rect.hPos / 20.0f);
            float verticalPos = roundf(behaviorData->rect.vPos / 20.0f);
            position.h = horizontalPos + static_cast<float>(inputIndex);
            position.v = verticalPos - 1.0f;
        }
    }

    return position;
}

Point LayoutCalculator::GetOutputParamPosition(CK_ID targetId, int outputIndex) {
    Point position;

    // Handle operation
    if (IsOperation(targetId)) {
        Operation *operation = GetOperation(targetId);
        if (operation) {
            position.h = roundf(operation->hPos / 20.0f) + 1;
            position.v = roundf(operation->vPos / 20.0f) + 2;
        }
    } else {
        // Handle behavior
        BehaviorData *behaviorData = GetBehaviorData(targetId);
        if (behaviorData) {
            float horizontalPos = roundf(behaviorData->rect.hPos / 20.0f);
            float verticalPos = roundf(behaviorData->rect.vPos / 20.0f);
            position.h = horizontalPos + static_cast<float>(outputIndex);
            position.v = verticalPos + roundf(behaviorData->rect.vSize / 20.0f) + 1;
        }
    }

    return position;
}

void LayoutCalculator::CalculateOperationPositions(BehaviorData &behaviorGraph) {
    // Position operations based on their parameter links
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type != LINK_TYPE_PARAMETER)
            continue;

        // Parameter link
        if (paramLink.start.type == ENDPOINT_POUT && IsOperation(paramLink.start.id)) {
            // Skip if already moved
            if (m_MovedOperations.find(paramLink.start.id) != m_MovedOperations.end()) {
                continue;
            }

            Operation *startOperation = GetOperation(paramLink.start.id);
            if (!startOperation) {
                continue;
            }

            // Position based on destination
            if (paramLink.end.type == ENDPOINT_PIN) {
                // Normal input
                MoveOperationToPosition(*startOperation,
                                        GetInputParamPosition(paramLink.end.id, paramLink.end.index));
                m_MovedOperations.insert(paramLink.start.id);
            } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                // Target input
                MoveOperationToPosition(*startOperation,
                                        GetInputParamPosition(paramLink.end.id, -1));
                m_MovedOperations.insert(paramLink.start.id);
            }
        }
    }
}

void LayoutCalculator::CalculateLocalParameterPositions(BehaviorData &behaviorGraph, bool isInputDirection) {
    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type != LINK_TYPE_PARAMETER)
            continue;

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
                    MoveParameterToPosition(*startParam, GetInputParamPosition(paramLink.end.id, paramLink.end.index));
                } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                    // Target input
                    MoveParameterToPosition(*startParam, GetInputParamPosition(paramLink.end.id, -1));
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
                                            GetOutputParamPosition(paramLink.start.id, paramLink.start.index));
                }
            }
        }
    }
}

void LayoutCalculator::SetStart(BehaviorData &script, float verticalStartPos, float verticalSize) {
    auto &header = m_Data.header;
    header.id = script.id;
    header.vSize = verticalSize;
    header.vStartPos = verticalStartPos;
    header.vStart = 0;
}

void LayoutCalculator::RecalculateAbsolutePositions(BehaviorData &behaviorData, CKBehavior *behavior,
                                                    float startHorizontal, float startVertical) {
    // Reset position for root behavior
    if (behaviorData.depth == 0) {
        behaviorData.rect.hPos = 0;
        behaviorData.rect.vPos = 0;
    }

    // Apply offset
    behaviorData.rect.hPos += startHorizontal;
    behaviorData.rect.vPos += startVertical;

    // Process sub-behaviors and operations if this is a behavior graph
    if (behaviorData.isBehaviorGraph) {
        // Process sub-behaviors
        const int subBehaviorCount = behavior->GetSubBehaviorCount();
        for (int i = 0; i < subBehaviorCount; ++i) {
            CKBehavior *subBeh = behavior->GetSubBehavior(i);
            if (!subBeh) continue;
            BehaviorData *subBehData = GetBehaviorData(subBeh->GetID());
            if (!subBehData) continue;
            RecalculateAbsolutePositions(
                *subBehData, subBeh,
                behaviorData.rect.hPos, behaviorData.rect.vPos
            );
        }

        // Process operations
        const int operationCount = behavior->GetParameterOperationCount();
        for (int i = 0; i < operationCount; ++i) {
            CKParameterOperation *operation = behavior->GetParameterOperation(i);
            if (operation) {
                Operation *operationData = GetOperation(operation->GetID());
                if (operationData) {
                    operationData->hPos += behaviorData.rect.hPos;
                    operationData->vPos += behaviorData.rect.vPos;
                }
            }
        }
    }
}
