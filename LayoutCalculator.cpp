#include "LayoutCalculator.h"

#include <map>
#include <algorithm>
#include <stdexcept>

#include "CKAll.h"

#undef min
#undef max

LayoutCalculator::LayoutCalculator(InterfaceData &targetData, CKContext *context)
    : m_Data(targetData), m_Context(context) {}

void LayoutCalculator::CalculateLayout(CKBehavior *script) {
    // Get ordered behavior IDs for consistent processing
    const auto behaviorIds = std::move(GetBehaviorIds());

    // Clear graph state
    InitializeGraphState();

    // Phase 1: Calculate behavior positions
    CalculateBehaviorLayouts(behaviorIds);

    // Phase 2: Calculate operation and parameter positions
    CalculateElementPositions(behaviorIds);

    // Phase 3: Finalize script layout and recalculate absolute positions
    FinalizeScriptLayout(script);

    // Phase 4: Calculate link routes
    CalculateAllLinkRoutes();

    // Notify observers of changes
    m_Data.NotifyObservers(nullptr, InterfaceData::ElementAction::Modified);
}

void LayoutCalculator::InitializeGraphState() {
    m_Vertices.clear();
    m_DistanceFromRoot.clear();
    m_RequiredSize.clear();
    m_PredecessorEdge.clear();
    m_Edges.clear();
    m_MovedOperations.clear();
}

void LayoutCalculator::CalculateBehaviorLayouts(const std::vector<CK_ID> &behaviorIds) {
    // Create a map to store behavior depth information for sorting
    std::map<int, std::vector<std::pair<BehaviorData *, CKBehavior *>>> behaviorsByDepth;

    // Collect behaviors and store them by depth
    for (auto &behaviorId : behaviorIds) {
        BehaviorData *behaviorData = GetBehaviorData(behaviorId);
        auto *behavior = (CKBehavior *) m_Context->GetObject(behaviorId);
        if (!behaviorData || !behavior) {
            m_Context->OutputToConsoleEx((CKSTRING) "Behavior not found: %d", behaviorId);
            continue;
        }

        // Group behaviors by depth to ensure parent behaviors are processed before children
        behaviorsByDepth[behaviorData->depth].emplace_back(behaviorData, behavior);
    }

    // First pass: Calculate sizes for all behaviors, processing by depth (root first)
    for (const auto &depthGroup : behaviorsByDepth) {
        for (const auto &pair : depthGroup.second) {
            CalculateBehaviorSize(*pair.first, pair.second);
        }
    }

    // Second pass: Calculate positions, ensuring parents are positioned before children
    for (const auto &depthGroup : behaviorsByDepth) {
        for (const auto &pair : depthGroup.second) {
            CalculateBehaviorPositions(*pair.first, pair.second, pair.first->depth == 0);
        }
    }
}

void LayoutCalculator::CalculateBehaviorSize(BehaviorData &behaviorData, CKBehavior *behavior) {
    if (behaviorData.depth <= 0) return;

    // Calculate height with better scaling for behaviors with many inputs/outputs
    int inputCount = behavior->GetInputCount();
    int outputCount = behavior->GetOutputCount();

    // More sophisticated height calculation
    int height = std::max(inputCount, outputCount);
    if (height <= 1) {
        height = 1;
    }

    // Calculate width considering parameter counts and name length
    int paramInputCount = behavior->GetInputParameterCount();
    int paramOutputCount = behavior->GetOutputParameterCount();

    // Base width on parameter counts
    int width = std::max(paramInputCount, paramOutputCount);

    // Consider name length in width calculation
    const char *name = behavior->GetName();
    const size_t nameLength = name ? strlen(name) : 0;
    const int nameWidth = static_cast<int>(std::floor(nameLength * 0.4 + 1));
    width = std::max(width, nameWidth);
    width = std::max(width, 2);

    // Set size
    behaviorData.rect.hSize = static_cast<float>(width) * HORIZONTAL_SPACING;
    behaviorData.rect.vSize = static_cast<float>(height) * VERTICAL_SPACING;

    if (behaviorData.isUsingTarget) {
        behaviorData.rect.hSize += HORIZONTAL_SPACING;
    }

    // Set expanded size for behavior graphs
    if (behaviorData.isBehaviorGraph) {
        float expansionFactor = BEHAVIOR_EXPANSION_FACTOR;
        if (behavior->GetSubBehaviorCount() > 10) {
            expansionFactor *= 1.5f; // More space for very complex behavior graphs
        }

        behaviorData.hExpandSize = behaviorData.rect.hSize * expansionFactor;
        behaviorData.vExpandSize = behaviorData.rect.vSize * expansionFactor;
    }
}

void LayoutCalculator::CalculateElementPositions(const std::vector<CK_ID> &behaviorIds) {
    for (auto &behaviorId : behaviorIds) {
        BehaviorData *behaviorData = GetBehaviorData(behaviorId);
        if (behaviorData && behaviorData->isBehaviorGraph) {
            // Apply multiple passes of operation positioning
            for (int i = 0; i < MAX_FIX_STACK_OPS; ++i) {
                CalculateOperationPositions(*behaviorData);
            }

            // Calculate parameter positions
            CalculateLocalParameterPositions(*behaviorData, false); // Outputs
            CalculateLocalParameterPositions(*behaviorData, true);  // Inputs
        }
    }
}

void LayoutCalculator::FinalizeScriptLayout(CKBehavior *script) {
    // Calculate the total height of the behavior graph
    Rect &requiredSize = m_RequiredSize[script->GetID()];

    // Ensure we have a valid size
    if (requiredSize.vSize <= 0) {
        // Calculate fallback size based on behavior count
        float totalHeight = 0;
        for (const auto &behavior : m_Data.behaviors) {
            totalHeight += behavior.rect.vSize + VERTICAL_SPACING;
        }
        requiredSize.vSize = std::max(totalHeight, VERTICAL_SPACING * 5.0f);
    }

    float behaviorHeight = requiredSize.vSize + VERTICAL_SPACING * EXPANSION_PADDING;

    // Calculate the vertical center position for the start point
    float startVertical = (behaviorHeight - VERTICAL_SPACING) / 2.0f;

    // Set start information and recalculate positions
    SetStart(m_Data.rootBehavior, startVertical, behaviorHeight);
    RecalculateAbsolutePositions(m_Data.rootBehavior, script, 0.0f, 0.0f);
}

void LayoutCalculator::CalculateAllLinkRoutes() {
    for (auto &behavior : m_Data.behaviors) {
        CalculateLinkRoutes(behavior);
    }
    CalculateLinkRoutes(m_Data.rootBehavior);
}

//------------------------------------------------------
// Utility Methods
//------------------------------------------------------

BehaviorData *LayoutCalculator::GetBehaviorData(CK_ID id) const {
    return m_Data.FindBehavior(id);
}

Operation *LayoutCalculator::GetOperation(CK_ID id) const {
    return m_Data.FindOperation(id);
}

bool LayoutCalculator::IsOperation(CK_ID id) const {
    CKObject *obj = m_Context->GetObject(id);
    return obj && obj->GetClassID() == CKCID_PARAMETEROPERATION;
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

//------------------------------------------------------
// Graph Construction and Analysis
//------------------------------------------------------

void LayoutCalculator::ConstructGraph(BehaviorData &behaviorGraph, CKBehavior *behavior) {
    // Initialize graph structures
    m_Vertices.clear();
    m_Edges.clear();

    // Create vertices for root and all sub-behaviors
    CreateGraphVertices(behavior);

    // Get sub-behavior count
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    if (subBehaviorCount == 0) {
        return; // No sub-behaviors to connect
    }

    // Get valid behavior links
    std::vector<Link *> validBehaviorLinks = GetValidBehaviorLinks(behaviorGraph);

    // Handle the case where no valid links exist
    if (validBehaviorLinks.empty()) {
        ConnectDisconnectedBehaviorsToRoot(behavior);
        return;
    }

    // Sort and add links to the graph
    SortBehaviorLinks(validBehaviorLinks);
    AddBehaviorLinksToGraph(validBehaviorLinks);

    // Connect any orphaned behaviors
    ConnectOrphanedBehaviors(behaviorGraph, behavior->GetID());
}

void LayoutCalculator::CreateGraphVertices(CKBehavior *behavior) {
    // Create root vertex
    const CK_ID rootId = behavior->GetID();
    m_Vertices[rootId] = Vertex();

    // Create vertices for all sub-behaviors
    const int subBehaviorCount = behavior->GetSubBehaviorCount();
    for (int i = 0; i < subBehaviorCount; ++i) {
        CKBehavior *subBehavior = behavior->GetSubBehavior(i);
        if (subBehavior) {
            m_Vertices[subBehavior->GetID()] = Vertex();
        }
    }
}

std::vector<Link *> LayoutCalculator::GetValidBehaviorLinks(BehaviorData &behaviorGraph) {
    std::vector<Link *> validLinks;

    for (auto &link : behaviorGraph.links) {
        if (link.IsBehaviorLink()) {
            // Only include links between known behaviors
            if (m_Vertices.find(link.start.id) != m_Vertices.end() &&
                m_Vertices.find(link.end.id) != m_Vertices.end()) {
                validLinks.push_back(&link);
            }
        }
    }

    return validLinks;
}

void LayoutCalculator::AddBehaviorLinksToGraph(const std::vector<Link *> &behaviorLinks) {
    for (Link *link : behaviorLinks) {
        AddGraphEdge(link->start.id, link->end.id);
    }
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
    std::vector<OrphanedBehavior> orphanedBehaviors = FindOrphanedBehaviors(behaviorGraph, rootId);

    // Sort orphaned behaviors by vertical position (descending)
    SortOrphanedBehaviors(orphanedBehaviors);

    // Connect orphaned behaviors to form a chain
    ConnectOrphanedBehaviorsChain(orphanedBehaviors, rootId);
}

std::vector<LayoutCalculator::OrphanedBehavior> LayoutCalculator::FindOrphanedBehaviors(
    BehaviorData &behaviorGraph, CK_ID rootId) {
    std::vector<OrphanedBehavior> orphanedBehaviors;

    for (const auto &vertexPair : m_Vertices) {
        CK_ID behaviorId = vertexPair.first;
        const Vertex &vertex = vertexPair.second;

        // Skip root and behaviors with incoming edges
        if (behaviorId != rootId && vertex.incomingEdgeCount == 0) {
            float vPos = GetBehaviorVerticalPosition(behaviorGraph, behaviorId);
            orphanedBehaviors.push_back({behaviorId, vPos});
        }
    }

    return orphanedBehaviors;
}

float LayoutCalculator::GetBehaviorVerticalPosition(BehaviorData &behaviorGraph, CK_ID behaviorId) {
    // Find the behavior data to get vertical position
    BehaviorData *behaviorData = nullptr;
    if (behaviorId == behaviorGraph.id) {
        behaviorData = &behaviorGraph;
    } else {
        behaviorData = GetBehaviorData(behaviorId);
    }

    // Return vertical position if behavior data was found
    return behaviorData ? behaviorData->rect.vPos : 0.0f;
}

void LayoutCalculator::SortOrphanedBehaviors(std::vector<OrphanedBehavior> &orphanedBehaviors) {
    std::sort(orphanedBehaviors.begin(), orphanedBehaviors.end(),
              [](const OrphanedBehavior &a, const OrphanedBehavior &b) {
                  return a.verticalPosition > b.verticalPosition;
              });
}

void LayoutCalculator::ConnectOrphanedBehaviorsChain(
    const std::vector<OrphanedBehavior> &orphanedBehaviors, CK_ID rootId) {
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
    // for (const auto &vertex : m_Vertices) {
    //     CK_ID vertexId = vertex.first;
    //     if (m_DistanceFromRoot.find(vertexId) == m_DistanceFromRoot.end()) {
    //         // Node not reachable - ignored as it should be handled by virtual edges
    //     }
    // }
}

//------------------------------------------------------
// Size and Position Calculation
//------------------------------------------------------

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
        if (m_PredecessorEdge.find(targetId) != m_PredecessorEdge.end() &&
            m_PredecessorEdge[targetId] == edgeIndex) {
            Rect childSize = CalculateSubgraphSize(*targetData, false);
            totalVerticalSize += childSize.vSize + VERTICAL_SPACING * 2;
            maxHorizontalSize = std::max(maxHorizontalSize, childSize.hSize);
            childCount++;
        }
    }

    // Adjust vertical size (remove extra padding if multiple children)
    if (childCount > 0) {
        totalVerticalSize -= VERTICAL_SPACING * 2;
    }

    // Calculate final size
    size.hSize = size.hSize + (maxHorizontalSize > 0.0f ? maxHorizontalSize + HORIZONTAL_SPACING * 2 : 0.0f);
    size.vSize = std::max(size.vSize, totalVerticalSize);

    // Store required size and return
    m_RequiredSize[behaviorData.id] = size;
    return size;
}

void LayoutCalculator::PlaceBehaviorInParent(BehaviorData &behaviorData, float hPos, float vPos, bool isRoot) {
    // Position the behavior (unless it's the root)
    if (!isRoot) {
        behaviorData.rect.hPos = hPos;
        // Center the behavior vertically within its allocated space
        float verticalCenter = (m_RequiredSize[behaviorData.id].vSize - behaviorData.rect.vSize) / 2;
        behaviorData.rect.vPos = vPos + verticalCenter;
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

            // Calculate horizontal position based on parent type (BEHAVIOR_PADDING is already pixels)
            float childHorizontalPos;
            if (isRoot) {
                childHorizontalPos = hPos + HORIZONTAL_SPACING;
            } else {
                childHorizontalPos = hPos + behaviorData.rect.hSize + BEHAVIOR_PADDING;
            }

            // Place the child behavior
            PlaceBehaviorInParent(
                *targetData,
                childHorizontalPos,
                vPos + currentVerticalOffset,
                false
            );

            // Update vertical offset for next child (BEHAVIOR_PADDING is already pixels)
            currentVerticalOffset += childSize.vSize + BEHAVIOR_PADDING;
            childCount++;
        }
    }
}

float LayoutCalculator::CalculateBehaviorPositions(BehaviorData &behaviorGraph, CKBehavior *behavior, bool isScript) {
    if (!behaviorGraph.isBehaviorGraph)
        return 0.0f;

    // Build the graph representation
    ConstructGraph(behaviorGraph, behavior);

    // Calculate minimum distances from root
    CalculateGraphDistances(behaviorGraph);

    // Calculate required sizes for all nodes
    const Rect size = CalculateSubgraphSize(behaviorGraph, true);

    // Calculate expanded sizes
    behaviorGraph.hExpandSize = size.hSize + HORIZONTAL_SPACING * EXPANSION_PADDING;
    behaviorGraph.vExpandSize = size.vSize + VERTICAL_SPACING * EXPANSION_PADDING;

    // Place behaviors within the graph (constants are already in pixels)
    float horizontalOffset = isScript ? BEHAVIOR_H_START_OFFSET : BEHAVIOR_PADDING;

    PlaceBehaviorInParent(
        behaviorGraph,
        horizontalOffset,
        BEHAVIOR_V_START_OFFSET,
        true
    );

    // Return vertical center position
    return size.vSize / 2;
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
        for (auto &op : behaviorData.operations) {
            op.hPos += behaviorData.rect.hPos;
            op.vPos += behaviorData.rect.vPos;
        }

        // Process parameters (stored as int grid indices)
        // Parameters are positioned in the local coordinate space of the behavior graph,
        // but serialized as integer (col,row) indices in the 20px grid. When converting
        // to absolute space, translate them by the parent's pixel offset expressed in grid units.
        const int dx = static_cast<int>(std::lround(behaviorData.rect.hPos / HORIZONTAL_SPACING));
        const int dy = static_cast<int>(std::lround(behaviorData.rect.vPos / VERTICAL_SPACING));

        for (auto &param : behaviorData.localParams) {
            param.hPos += dx;
            param.vPos += dy;
        }

        for (auto &param : behaviorData.sharedParams) {
            param.hPos += dx;
            param.vPos += dy;
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

//------------------------------------------------------
// Operation and Parameter Layout
//------------------------------------------------------
// UNIFIED PIXEL COORDINATE SYSTEM:
// - All functions use PIXEL coordinates
// - Parameter: serialized as int grid indices (col/row) in 20px grid
// - Operation: stored as float pixels, snapped to 20.0 grid
// - Binary-verified snap formula: (int)((rel - grid*0.5) / grid + 1) * grid
//------------------------------------------------------

void LayoutCalculator::MoveParameterToPosition(Parameter &parameter, const Point &pixelPos) {
    // Input: pixel coordinates
    // Output: integer (col,row) grid indices in 20px grid (binary-accurate serialization)
    if (std::isfinite(pixelPos.h) && std::isfinite(pixelPos.v)) {
        parameter.hPos = static_cast<int>(std::lround(pixelPos.h / HORIZONTAL_SPACING));
        parameter.vPos = static_cast<int>(std::lround(pixelPos.v / VERTICAL_SPACING));
    } else {
        m_Context->OutputToConsoleEx((CKSTRING) "Warning: Invalid parameter position (%f, %f)", pixelPos.h, pixelPos.v);
    }
}

void LayoutCalculator::MoveOperationToPosition(Operation &operation, const Point &pixelPos) {
    // Input: pixel coordinates (target position for operation's input)
    // Output: float pixels snapped to 20.0 grid
    operation.hPos = SnapToGrid(pixelPos.h, HORIZONTAL_SPACING);
    operation.vPos = SnapToGrid(pixelPos.v, VERTICAL_SPACING);
}

Point LayoutCalculator::GetInputParamPosition(CK_ID targetId, int inputIndex) {
    // Returns PIXEL coordinates for input parameter position
    Point position;

    if (IsOperation(targetId)) {
        Operation *operation = GetOperation(targetId);
        if (operation) {
            // Operation input parameters: horizontal offset by index * 2 * spacing
            position.h = operation->hPos + static_cast<float>(inputIndex) * HORIZONTAL_SPACING * 2.0f;
            position.v = operation->vPos;
        }
    } else {
        BehaviorData *behaviorData = GetBehaviorData(targetId);
        if (behaviorData) {
            // Behavior input parameters: above the behavior block
            float offset = behaviorData->isUsingTarget ? HORIZONTAL_SPACING : GRID_HALF_CELL;
            position.h = behaviorData->rect.hPos + offset + HORIZONTAL_SPACING * static_cast<float>(inputIndex);
            position.v = behaviorData->rect.vPos - HORIZONTAL_SPACING;  // One grid cell above
        }
    }

    return position;
}

Point LayoutCalculator::GetOutputParamPosition(CK_ID targetId, int outputIndex) {
    // Returns PIXEL coordinates for output parameter position
    Point position;

    if (IsOperation(targetId)) {
        Operation *operation = GetOperation(targetId);
        if (operation) {
            // Operation output: below the operation
            position.h = operation->hPos + HORIZONTAL_SPACING;
            position.v = operation->vPos + HORIZONTAL_SPACING * 2.0f;
        }
    } else {
        BehaviorData *behaviorData = GetBehaviorData(targetId);
        if (behaviorData) {
            // Behavior output parameters: below the behavior block
            float offset = behaviorData->isUsingTarget ? HORIZONTAL_SPACING : GRID_HALF_CELL;
            position.h = behaviorData->rect.hPos + offset + HORIZONTAL_SPACING * static_cast<float>(outputIndex);
            position.v = behaviorData->rect.vPos + behaviorData->rect.vSize + HORIZONTAL_SPACING;
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
                // Ensure destination exists before getting position
                BehaviorData *behaviorData = GetBehaviorData(paramLink.end.id);
                if (behaviorData || IsOperation(paramLink.end.id)) {
                    // Normal input
                    MoveOperationToPosition(*startOperation,
                                            GetInputParamPosition(paramLink.end.id, paramLink.end.index));
                    m_MovedOperations.insert(paramLink.start.id);
                }
            } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                // Ensure destination exists before getting position
                BehaviorData *behaviorData = GetBehaviorData(paramLink.end.id);
                if (behaviorData) {
                    // Target input
                    MoveOperationToPosition(*startOperation,
                                            GetInputParamPosition(paramLink.end.id, -1));
                    m_MovedOperations.insert(paramLink.start.id);
                }
            }
        }
    }
}

void LayoutCalculator::CalculateLocalParameterPositions(BehaviorData &behaviorGraph, bool isInputDirection) {
    // Maintain a set of parameters we've already positioned to avoid overwrites
    std::unordered_set<Parameter *> processedParams;

    for (auto &paramLink : behaviorGraph.links) {
        if (paramLink.type != LINK_TYPE_PARAMETER)
            continue;

        // Parameter link
        if (isInputDirection) {
            // Position source parameters (inputs)
            Parameter *startParam = nullptr;

            if (paramLink.start.type == ENDPOINT_PLOCAL) {
                // Local parameter
                if (paramLink.start.index >= 0 &&
                    paramLink.start.index < static_cast<int>(behaviorGraph.localParams.size())) {
                    startParam = &behaviorGraph.localParams[paramLink.start.index];
                }
            } else if (paramLink.start.type == ENDPOINT_POUT_SHORTCUT) {
                // Shared parameter
                if (paramLink.start.index >= 0 &&
                    paramLink.start.index < static_cast<int>(behaviorGraph.sharedParams.size())) {
                    startParam = &behaviorGraph.sharedParams[paramLink.start.index];
                }
            }

            if (startParam && processedParams.find(startParam) == processedParams.end()) {
                if (paramLink.end.type == ENDPOINT_PIN) {
                    // Normal input
                    MoveParameterToPosition(*startParam, GetInputParamPosition(paramLink.end.id, paramLink.end.index));
                    processedParams.insert(startParam);
                } else if (paramLink.end.type == ENDPOINT_TARGET_PIN) {
                    // Target input
                    MoveParameterToPosition(*startParam, GetInputParamPosition(paramLink.end.id, -1));
                    processedParams.insert(startParam);
                }
            }
        } else {
            // Position destination parameters (outputs)
            Parameter *endParam = nullptr;

            if (paramLink.end.type == ENDPOINT_PLOCAL) {
                // Local parameter
                if (paramLink.end.index >= 0 &&
                    paramLink.end.index < static_cast<int>(behaviorGraph.localParams.size())) {
                    endParam = &behaviorGraph.localParams[paramLink.end.index];
                }
            }

            if (endParam && processedParams.find(endParam) == processedParams.end()) {
                if (paramLink.start.type == ENDPOINT_POUT) {
                    // From output
                    MoveParameterToPosition(
                        *endParam, GetOutputParamPosition(paramLink.start.id, paramLink.start.index));
                    processedParams.insert(endParam);
                }
            }
        }
    }
}

void LayoutCalculator::CalculateLinkRoutes(BehaviorData &behaviorData) {
    // Process each link in the behavior
    for (auto &link : behaviorData.links) {
        RouteLink(link);
    }
}

void LayoutCalculator::RouteLink(Link &link) {
    // Get endpoint positions
    const Point startPos = GetEndpointPosition(link.start);
    const Point endPos = GetEndpointPosition(link.end);

    if (startPos.Zero() || endPos.Zero()) {
        // Invalid start or end position
        m_Context->OutputToConsoleEx((CKSTRING) "Warning: Invalid start or end position for link %d", link.id);
        return;
    }

    // Create a path connecting the points - exclude start and end positions
    link.points = CreatePath(startPos, endPos, link);
}

LayoutCalculator::LinkCharacteristics LayoutCalculator::DetermineRoutingCharacteristics(const Link &link) {
    LinkCharacteristics result;

    // Determine link type
    if (link.IsBehaviorLink()) {
        result.linkType = LinkType::BehaviorFlow;
    } else if (link.IsParameterOpLink()) {
        result.linkType = LinkType::ParameterOperation;
    } else {
        result.linkType = LinkType::ParameterData;
    }

    // Check if this is a self-connection
    result.isSelfConnection = (link.start.id == link.end.id);

    // Check if this is a start link
    result.isStartLink = link.start.IsStartBehaviorInput();

    // Determine endpoint types
    result.isSourceBehaviorOutput = link.start.IsBehaviorOutput();
    result.isSourceParameterOutput = link.start.IsParameterOutput();
    result.isTargetBehaviorInput = link.end.IsBehaviorInput();
    result.isTargetParameterInput = link.end.IsParameterInput();

    // Check for operation involvement
    result.isSourceOperation = IsOperation(link.start.id);
    result.isTargetOperation = IsOperation(link.end.id);

    // Check for parameter shortcuts
    result.isParameterShortcut = link.start.IsParameterShortCut();

    return result;
}

Point LayoutCalculator::GetEndpointPosition(const LinkEndpoint &endpoint) {
    // Handle different endpoint types based on their category
    if (endpoint.IsParameterInput()) {
        return GetParameterInputPosition(endpoint);
    } else if (endpoint.IsParameterOutput()) {
        return GetParameterOutputPosition(endpoint);
    } else if (endpoint.IsParameterLocal()) {
        return GetLocalParameterPosition(endpoint);
    } else if (endpoint.IsBehaviorInput()) {
        return GetBehaviorInputPosition(endpoint);
    } else if (endpoint.IsBehaviorOutput()) {
        return GetBehaviorOutputPosition(endpoint);
    }

    // Unknown endpoint type - use default position
    m_Context->OutputToConsoleEx((CKSTRING) "Warning: Unknown endpoint type %d", endpoint.type);
    return {};
}

Point LayoutCalculator::GetParameterInputPosition(const LinkEndpoint &endpoint) {
    Point position;

    if (endpoint.type == ENDPOINT_TARGET_PIN) {
        // Target parameter is special
        BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
        if (!behaviorData) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
            return position;
        }

        // Left side, middle of the block
        position.h = behaviorData->rect.hPos;
        position.v = behaviorData->rect.vPos;
    } else if (IsOperation(endpoint.id)) {
        // Parameter input on operation
        Operation *operation = GetOperation(endpoint.id);
        if (!operation) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Operation with ID %d not found", endpoint.id);
            return position;
        }

        // Position based on input index
        position.h = operation->hPos;
        position.v = operation->vPos;

        if (endpoint.index == 0) {
            // First input is on left side
            position.h -= GRID_HALF_CELL;
        } else if (endpoint.index == 1) {
            // Second input is on right side
            position.h += GRID_HALF_CELL;
        }
    } else {
        // Parameter input on behavior
        BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
        if (!behaviorData) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
            return position;
        }

        float offset = (behaviorData->isUsingTarget) ? HORIZONTAL_SPACING : GRID_HALF_CELL;
        position.h = behaviorData->rect.hPos + offset + HORIZONTAL_SPACING * endpoint.index;
        position.v = behaviorData->rect.vPos;
    }

    return position;
}

Point LayoutCalculator::GetParameterOutputPosition(const LinkEndpoint &endpoint) {
    Point position;

    if (endpoint.type == ENDPOINT_POUT_SHORTCUT) {
        // Parameter output shortcut
        BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
        if (!behaviorData) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
            return position;
        }

        // Validate index is within bounds
        if (endpoint.index < 0 || endpoint.index >= static_cast<int>(behaviorData->sharedParams.size())) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Parameter shortcut index %d out of bounds (size %d)",
                                         endpoint.index, behaviorData->sharedParams.size());
            return position;
        }

        // Get the parameter position
        Parameter &param = behaviorData->sharedParams[endpoint.index];
        position.h = static_cast<float>(param.hPos) * HORIZONTAL_SPACING;
        position.v = static_cast<float>(param.vPos) * VERTICAL_SPACING;
    } else if (IsOperation(endpoint.id)) {
        // Parameter output on operation
        Operation *operation = GetOperation(endpoint.id);
        if (!operation) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Operation with ID %d not found", endpoint.id);
            return position;
        }

        // Output of operation - center with slight vertical offset
        position.h = operation->hPos;
        position.v = operation->vPos + PARAMETER_LINK_VERTICAL_OFFSET;
    } else {
        // Parameter output on behavior
        BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
        if (!behaviorData) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
            return position;
        }

        float offset = (behaviorData->isUsingTarget) ? HORIZONTAL_SPACING : GRID_HALF_CELL;
        position.h = behaviorData->rect.hPos + offset + HORIZONTAL_SPACING * endpoint.index;
        position.v = behaviorData->rect.vPos + behaviorData->rect.vSize;
    }

    return position;
}

Point LayoutCalculator::GetLocalParameterPosition(const LinkEndpoint &endpoint) {
    Point position;

    BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
    if (!behaviorData) {
        m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
        return position;
    }

    // Validate parameter index
    if (endpoint.index < 0 || endpoint.index >= static_cast<int>(behaviorData->localParams.size())) {
        m_Context->OutputToConsoleEx((CKSTRING) "Error: Local parameter index %d out of bounds (size %d)",
                                     endpoint.index, behaviorData->localParams.size());
        return position;
    }

    // Get exact parameter position
    const Parameter &param = behaviorData->localParams[endpoint.index];
    position.h = static_cast<float>(param.hPos) * HORIZONTAL_SPACING;
    position.v = static_cast<float>(param.vPos) * VERTICAL_SPACING;

    return position;
}

Point LayoutCalculator::GetBehaviorInputPosition(const LinkEndpoint &endpoint) {
    Point position;

    if (endpoint.IsStartBehaviorInput()) {
        // Start behavior input is special - comes from script header
        // Binary-accurate: start point comes from header, shifted left by one grid cell
        position.h = m_Data.header.hStartPos - HORIZONTAL_SPACING;
        position.v = m_Data.header.vStartPos;
    } else {
        BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
        if (!behaviorData) {
            m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
            return position;
        }

        // Left side of behavior block
        position.h = behaviorData->rect.hPos - BEHAVIOR_IO_OFFSET;

        // Binary-accurate Y: vPos + spacing * index + IO_Y_OFFSET (-5px)
        int clampedIndex = std::max(0, endpoint.index);
        position.v = behaviorData->rect.vPos + VERTICAL_SPACING * clampedIndex + BEHAVIOR_IO_Y_OFFSET;
    }

    return position;
}

Point LayoutCalculator::GetBehaviorOutputPosition(const LinkEndpoint &endpoint) {
    Point position;

    BehaviorData *behaviorData = GetBehaviorData(endpoint.id);
    if (!behaviorData) {
        m_Context->OutputToConsoleEx((CKSTRING) "Error: Behavior data not found for ID %d", endpoint.id);
        return position;
    }

    // Right side of behavior block
    position.h = behaviorData->rect.hPos + behaviorData->rect.hSize + BEHAVIOR_IO_OFFSET;

    // Binary-accurate Y: vPos + spacing * index + IO_Y_OFFSET (-5px)
    int clampedIndex = std::max(0, endpoint.index);
    position.v = behaviorData->rect.vPos + VERTICAL_SPACING * clampedIndex + BEHAVIOR_IO_Y_OFFSET;

    return position;
}

std::vector<Point> LayoutCalculator::CreatePath(const Point &startPos, const Point &endPos, const Link &link) {
    // Determine link characteristics for routing
    LinkCharacteristics characteristics = DetermineRoutingCharacteristics(link);

    // Add alignment checks
    characteristics.isVerticalAlignment = std::abs(startPos.h - endPos.h) < LINK_MARGIN;
    characteristics.isHorizontalAlignment = std::abs(startPos.v - endPos.v) < LINK_MARGIN;

    // Choose the appropriate routing strategy
    if (characteristics.isSelfConnection) {
        // Self-connection loop
        return CreateSelfConnectionPath(startPos, endPos, link);
    } else if (characteristics.isStartLink) {
        // Special routing for start point links
        return CreateStartLinkPath(startPos, endPos, characteristics.isHorizontalAlignment);
    } else if (characteristics.isVerticalAlignment || characteristics.isHorizontalAlignment) {
        // Direct connection for aligned points
        return {};
    } else if (characteristics.IsSharedParameterLink()) {
        // Special handling for shared parameter links
        return CreateParameterSharePath(startPos, endPos, link);
    } else if (characteristics.IsBehaviorFlowLink()) {
        // Standard behavior flow (bOut -> bIn) - right to left
        return CreateBehaviorFlowPath(startPos, endPos);
    } else if (characteristics.IsParameterDataLink()) {
        // Parameter data flow (pOut -> pIn) - typically top to bottom
        return CreateParameterDataPath(startPos, endPos);
    } else if (characteristics.IsOperationLink()) {
        // Operation link with special routing
        return CreateOperationLinkPath(startPos, endPos, characteristics);
    } else {
        // Default case - create appropriate path based on endpoint types
        if (characteristics.isSourceBehaviorOutput || characteristics.isTargetBehaviorInput) {
            // Contains behavior endpoints - use behavior flow path
            return CreateBehaviorFlowPath(startPos, endPos);
        } else {
            // Contains parameter endpoints - use parameter flow path
            return CreateParameterDataPath(startPos, endPos);
        }
    }
}

std::vector<Point> LayoutCalculator::CreateSelfConnectionPath(const Point &startPos, const Point &endPos,
                                                              const Link &link) {
    std::vector<Point> path;

    // Determine element size and type
    float loopWidth = LOOP_SIZE;
    float loopHeight = LOOP_SIZE;

    // Behavior endpoints - use behavior size to calculate loop size
    if (link.start.IsBehaviorRelated()) {
        BehaviorData *behaviorData = GetBehaviorData(link.start.id);
        if (behaviorData) {
            // Make loop proportional to behavior size, but with minimum/maximum limits
            loopWidth = std::max(behaviorData->rect.hSize * 0.5f, LOOP_SIZE);
            loopHeight = std::max(behaviorData->rect.vSize * 0.5f, LOOP_SIZE);
        }
    }
    // Parameter endpoints - adjust based on parameter spacing
    else if (link.start.IsParameterRelated()) {
        if (IsOperation(link.start.id)) {
            // For operations, use smaller loops
            loopWidth = LOOP_SIZE * 0.75f;
            loopHeight = LOOP_SIZE * 0.75f;
        } else {
            // For other parameters, check if we can get behavior data
            BehaviorData *behaviorData = GetBehaviorData(link.start.id);
            if (behaviorData) {
                // Use parameter spacing as a guide
                float paramSpacing = behaviorData->rect.hSize / (behaviorData->rect.hSize / HORIZONTAL_SPACING + 1);
                loopWidth = std::max(paramSpacing * 2.0f, LOOP_SIZE * 0.5f);
                loopHeight = VERTICAL_SPACING * 2.0f;
            }
        }
    }

    // Adjust loop direction based on endpoint types
    bool loopToRight = true;
    bool loopUp = true;

    // For behavior outputs, loop to right
    if (link.start.IsBehaviorOutput()) {
        loopToRight = true;
    }
    // For behavior inputs, loop to left
    else if (link.start.IsBehaviorInput()) {
        loopToRight = false;
        loopWidth = -loopWidth; // Negative to go left
    }
    // For parameter outputs (typically bottom), loop down
    else if (link.start.IsParameterOutput()) {
        loopUp = false;
        loopHeight = -loopHeight; // Negative to go down
    }

    // Create path with adaptive loop size and direction
    // First horizontal segment
    Point corner1 = startPos;
    corner1.h += loopWidth;
    path.push_back(corner1);

    // Vertical segment
    Point corner2 = corner1;
    corner2.v += loopUp ? -loopHeight : loopHeight;
    path.push_back(corner2);

    // Second horizontal segment
    Point corner3 = corner2;
    corner3.h -= loopWidth;
    path.push_back(corner3);

    // Final vertical adjustment to match endpoint
    Point corner4 = corner3;
    corner4.v = endPos.v;
    path.push_back(corner4);

    return path;
}

std::vector<Point> LayoutCalculator::CreateStartLinkPath(
    const Point &startPos, const Point &endPos, bool isHorizontalAligned) {
    std::vector<Point> path;

    if (!isHorizontalAligned) {
        // Calculate the right extension from start point (header size + spacing)
        float rightExtension = m_Data.header.hStartSize + HORIZONTAL_SPACING;

        // First go right from start point
        Point rightPoint(startPos.h + rightExtension, startPos.v);
        path.push_back(rightPoint);

        // Then go to end position's vertical level
        Point alignedPoint(rightPoint.h, endPos.v);
        path.push_back(alignedPoint);
    }

    return path;
}

std::vector<Point> LayoutCalculator::CreateBehaviorFlowPath(const Point &startPos, const Point &endPos) {
    std::vector<Point> path;

    // For behavior links, create a mainly horizontal path that moves from right to left
    // Since behavior outputs are on the right and behavior inputs are on the left

    // Calculate the midpoint to avoid other behaviors
    float midH = (startPos.h + endPos.h) / 2.0f;

    // Adjust depending on whether we're going left-to-right (forward) or right-to-left (backward)
    bool isForwardLink = startPos.h < endPos.h;
    bool isBackwardLink = startPos.h > endPos.h;

    if (isForwardLink) {
        // Start to mid horizontal
        Point mid1(midH, startPos.v);
        path.push_back(mid1);

        // Mid vertical to end vertical level
        Point mid2(midH, endPos.v);
        path.push_back(mid2);
    } else if (isBackwardLink) {
        // More direct path for backward links - they're more common in behavior links
        // Direct horizontal connection to target's column
        Point horizontalPoint(endPos.h + HORIZONTAL_SPACING, startPos.v);
        path.push_back(horizontalPoint);

        // Vertical connection to target's level
        Point verticalPoint(horizontalPoint.h, endPos.v);
        path.push_back(verticalPoint);
    } else {
        // Same horizontal position but different vertical positions
        // Create a single midpoint to avoid a direct vertical line
        Point midPoint(startPos.h + HORIZONTAL_SPACING, startPos.v);
        path.push_back(midPoint);

        // Now connect vertically to the endpoint
        Point verticalPoint(midPoint.h, endPos.v);
        path.push_back(verticalPoint);

        // Finally back to the endpoint's horizontal position
        Point finalPoint(endPos.h, endPos.v);
        path.push_back(finalPoint);
    }

    return path;
}

std::vector<Point> LayoutCalculator::CreateParameterDataPath(const Point &startPos, const Point &endPos) {
    std::vector<Point> path;

    // For parameter links, create a mainly vertical path that moves from top to bottom
    // Since parameter outputs are typically on the bottom and parameter inputs on the top

    // Calculate the vertical midpoint
    float midV = (startPos.v + endPos.v) / 2.0f;

    // Determine if this is an upward or downward link
    bool isDownwardLink = startPos.v < endPos.v;
    bool isUpwardLink = startPos.v > endPos.v;

    if (isDownwardLink) {
        // More direct path for downward links (more common in parameter flow)
        // Vertical first to midpoint
        Point verticalPoint(startPos.h, midV);
        path.push_back(verticalPoint);

        // Horizontal to target's column
        Point horizontalPoint(endPos.h, midV);
        path.push_back(horizontalPoint);
    } else if (isUpwardLink) {
        // For upward links, we need a more complex path to avoid crossing other elements
        // Go horizontal first
        Point horizontalPoint(startPos.h + GRID_HALF_CELL, startPos.v);
        path.push_back(horizontalPoint);

        // Then up to the level just above the end position
        Point upPoint(horizontalPoint.h, endPos.v - GRID_HALF_CELL);
        path.push_back(upPoint);

        // Then horizontally to the end position's column
        Point finalPoint(endPos.h, upPoint.v);
        path.push_back(finalPoint);
    } else {
        // Same vertical position but different horizontal positions
        // Create a single midpoint for a horizontal connection
        Point midPoint(startPos.h, startPos.v + VERTICAL_SPACING);
        path.push_back(midPoint);

        // Then connect horizontally to the endpoint's column
        Point horizontalPoint(endPos.h, midPoint.v);
        path.push_back(horizontalPoint);
    }

    return path;
}

std::vector<Point> LayoutCalculator::CreateParameterSharePath(const Point &startPos, const Point &endPos,
                                                              const Link &link) {
    // TODO: Implement a path for parameter shortcuts
    return {};
}

std::vector<Point> LayoutCalculator::CreateOperationLinkPath(
    const Point &startPos, const Point &endPos, const LinkCharacteristics &characteristics) {
    std::vector<Point> path;

    if (characteristics.isSourceOperation && characteristics.isTargetOperation) {
        // Operation to operation connection (usually more direct)
        // Go vertically down from start
        Point verticalPoint = startPos;
        verticalPoint.v += PARAMETER_LINK_VERTICAL_OFFSET;
        path.push_back(verticalPoint);

        // Go horizontally to end's column
        Point horizontalPoint = verticalPoint;
        horizontalPoint.h = endPos.h;
        path.push_back(horizontalPoint);

        // Go up to end position
        Point upPoint = horizontalPoint;
        upPoint.v = endPos.v;
        path.push_back(upPoint);
    } else if (characteristics.isSourceOperation) {
        // From operation to another element
        // Operation outputs go down first
        Point downPoint(startPos.h, startPos.v + PARAMETER_LINK_VERTICAL_OFFSET);
        path.push_back(downPoint);

        // Then horizontally toward target
        Point horizontalPoint(endPos.h, downPoint.v);
        path.push_back(horizontalPoint);

        // Then vertically to target
        if (std::abs(horizontalPoint.v - endPos.v) > LINK_MARGIN) {
            Point finalPoint(horizontalPoint.h, endPos.v);
            path.push_back(finalPoint);
        }
    } else if (characteristics.isTargetOperation) {
        // From element to operation
        // Go vertically first to align with operation's level
        Point verticalPoint(startPos.h, endPos.v);
        path.push_back(verticalPoint);

        // Then horizontally to operation
        if (std::abs(verticalPoint.h - endPos.h) > LINK_MARGIN) {
            Point finalPoint(endPos.h, verticalPoint.v);
            path.push_back(finalPoint);
        }
    } else {
        // Default for parameter operations that aren't directly on operations
        // Use a more direct path with fewer bends
        Point midPoint(startPos.h + (endPos.h - startPos.h) / 2.0f, startPos.v);
        path.push_back(midPoint);

        Point verticalPoint(midPoint.h, endPos.v);
        path.push_back(verticalPoint);
    }

    return path;
}

bool LayoutCalculator::ArePointsAligned(const Point &startPos, const Point &endPos, float margin) const {
    return std::abs(startPos.h - endPos.h) < margin || std::abs(startPos.v - endPos.v) < margin;
}
