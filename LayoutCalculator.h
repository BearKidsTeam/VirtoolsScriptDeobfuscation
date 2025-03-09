#pragma once

#include <unordered_map>
#include <queue>
#include "InterfaceData.h"
#include "GraphBuilder.h"

class CKContext;
class CKBehavior;

/**
 * LayoutCalculator calculates the visual layout of a behavior tree.
 * It handles positioning and sizing of all elements.
 */
class LayoutCalculator {
public:
    /**
     * Constructor
     * @param target_data Reference to the interface data to populate
     * @param context Pointer to the CK context
     * @param graph_builder Reference to the graph builder
     */
    LayoutCalculator(InterfaceData &target_data, CKContext *context, GraphBuilder &graph_builder);

    /**
     * Sets start information for the behavior script
     * @param script The script behavior
     * @param verticalStartPos Vertical start position
     * @param verticalSize Vertical size
     */
    void DecorateStart(BehaviorBlock &script, float verticalStartPos, float verticalSize);

    /**
     * Calculates the layout for the entire behavior tree
     * @param script The root behavior to calculate layout for
     */
    void CalculateLayout(CKBehavior *script);

    /**
     * Recalculates absolute positions for behaviors
     * @param behaviorBlock Behavior
     * @param behavior CK behavior
     * @param startHorizontal Starting horizontal position
     * @param startVertical Starting vertical position
     */
    void RecalculateAbsolutePositions(BehaviorBlock &behaviorBlock, CKBehavior *behavior,
                                      float startHorizontal, float startVertical);

private:
    // Reference to the interface data
    InterfaceData &m_Data;

    // Pointer to the CK context
    CKContext *m_Context;

    // Reference to the graph builder
    GraphBuilder &m_GraphBuilder;

    // Maximum fix stack operations
    static const int MAX_FIX_STACK_OPS = 3;

    /**
     * Vertex structure for graph representation
     */
    struct Vertex {
        int incomingEdgeCount = 0; // Number of incoming edges
        int firstEdgeIndex = -1;   // First edge index
    };

    /**
     * Edge structure for graph representation
     */
    struct Edge {
        CK_ID sourceId = 0;         // Source node ID
        CK_ID targetId = 0;         // Target node ID
        int nextEdgeIndex = -1; // Next edge index from the same source
    };

    // Graph state
    std::unordered_map<CK_ID, Vertex> m_Vertices;
    std::unordered_map<CK_ID, int> m_DistanceFromRoot;
    std::unordered_map<CK_ID, Rect> m_RequiredSize;
    std::unordered_map<CK_ID, int> m_PredecessorEdge;
    std::vector<Edge> m_Edges;
    std::unordered_set<CK_ID> m_MovedOperations;

    /**
     * Adds an edge to the graph
     * @param sourceId Source node ID
     * @param targetId Target node ID
     */
    void AddGraphEdge(CK_ID sourceId, CK_ID targetId);

    /**
     * Constructs a graph representation of the behavior
     * @param behaviorGraph Behavior graph
     * @param behavior Behavior
     */
    void ConstructGraph(BehaviorBlock &behaviorGraph, CKBehavior *behavior);

    /**
     * Helper for calculating minimum distances in the graph
     * @param nodeQueue Queue of nodes to process
     */
    void CalculateDistancesFromQueue(std::queue<CK_ID> &nodeQueue);

    /**
     * Calculates minimum distances from the root
     * @param behaviorGraph Behavior graph
     */
    void CalculateGraphDistances(BehaviorBlock &behaviorGraph);

    /**
     * Calculates the size of a subgraph
     * @param behaviorBlock Behavior building block
     * @param isRoot Whether this is the root node
     * @return Size Rect
     */
    Rect CalculateSubgraphSize(BehaviorBlock &behaviorBlock, bool isRoot);

    /**
     * Places a behavior within its parent
     * @param behaviorBlock Behavior building block
     * @param horizontalPos Horizontal position
     * @param verticalPos Vertical position
     * @param isRoot Whether this is the root node
     */
    void PlaceBehaviorInParent(BehaviorBlock &behaviorBlock, float horizontalPos, float verticalPos, bool isRoot);

    /**
     * Calculates positions for behaviors in the graph
     * @param behaviorGraph Behavior graph
     * @param behavior Behavior
     * @param isScript Whether the behavior is a script
     * @return Vertical center position
     */
    float CalculateBehaviorPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior, bool isScript);

    /**
     * Moves a parameter to a position
     * @param parameter Parameter
     * @param position Position
     */
    void MoveParameterToPosition(Parameter &parameter, Point position);

    /**
     * Moves an operation to a position
     * @param operation Operation
     * @param position Position
     */
    void MoveOperationToPosition(Operation &operation, Point position);

    /**
     * Gets the input position for an interface
     * @param targetId Target ID
     * @param inputIndex Input position
     * @return Point
     */
    Point GetInterfaceInputPosition(CK_ID targetId, int inputIndex);

    /**
     * Gets the output position for an interface
     * @param targetId Target ID
     * @param outputIndex Output position
     * @return Point
     */
    Point GetInterfaceOutputPosition(CK_ID targetId, int outputIndex);

    /**
     * Calculates positions for operations
     * @param behaviorGraph Behavior graph
     * @param behavior Behavior
     */
    void CalculateOperationPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior);

    /**
     * Calculates positions for local parameters
     * @param behaviorGraph Behavior graph
     * @param behavior Behavior
     * @param isInputDirection Direction (true for inputs, false for outputs)
     */
    void CalculateLocalParameterPositions(BehaviorBlock &behaviorGraph, CKBehavior *behavior, bool isInputDirection);

    /**
     * Calculates the size of a behavior
     * @param behaviorBlock Behavior
     * @param behavior CK behavior
     */
    void CalculateBehaviorSize(BehaviorBlock &behaviorBlock, CKBehavior *behavior);
};