#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>

#include "InterfaceData.h"

class CKContext;
class CKBehavior;

/**
 * LayoutCalculator calculates the visual layout of a behavior tree.
 * It handles positioning and sizing of all elements based on the constructed InterfaceData.
 */
class LayoutCalculator {
public:
    /**
     * Constructor
     * @param targetData Reference to the interface data to populate
     * @param context Pointer to the CK context
     */
    LayoutCalculator(InterfaceData &targetData, CKContext *context);

    /**
     * Calculates the layout for the entire behavior tree
     * @param script The root behavior to calculate layout for
     */
    void CalculateLayout(CKBehavior *script);

private:
    // Reference to the interface data
    InterfaceData &m_Data;

    // Pointer to the CK context
    CKContext *m_Context;

    // Maximum fix stack operations
    static constexpr int MAX_FIX_STACK_OPS = 3;

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
        CK_ID sourceId = 0;       // Source node ID
        CK_ID targetId = 0;       // Target node ID
        int nextEdgeIndex = -1;   // Next edge index from the same source
    };

    // Graph state for layout calculation
    std::unordered_map<CK_ID, Vertex> m_Vertices;
    std::unordered_map<CK_ID, int> m_DistanceFromRoot;
    std::unordered_map<CK_ID, Rect> m_RequiredSize;
    std::unordered_map<CK_ID, int> m_PredecessorEdge;
    std::vector<Edge> m_Edges;
    std::unordered_set<CK_ID> m_MovedOperations;

    /**
     * Gets a behavior data by ID
     * @param id Behavior ID to find
     * @return Pointer to behavior or nullptr if not found
     */
    BehaviorData *GetBehaviorData(CK_ID id) const;

    /**
     * Gets an operation by ID from interface data
     * @param id Operation ID
     * @return Reference to the operation
     */
    Operation *GetOperation(CK_ID id) const;

    /**
     * Checks if an ID is an operation
     * @param id ID to check
     * @return True if ID is an operation
     */
    bool IsOperation(CK_ID id) const;

    /**
     * Gets a parameter in a behavior
     * @param behaviorId ID of behavior containing parameter
     * @param index Parameter index
     * @param isLocal Whether this is a local parameter
     * @return Pointer to parameter or nullptr if not found
     */
    Parameter* GetParameter(CK_ID behaviorId, int index, bool isLocal) const;

    /**
     * Gets a shared parameter in a behavior
     * @param behaviorId ID of behavior containing parameter
     * @param index Parameter index
     * @return Pointer to parameter or nullptr if not found
     */
    Parameter* GetSharedParameter(CK_ID behaviorId, int index) const;

    /**
     * Gets a vector of all behavior IDs
     * @return Vector of behavior IDs
     */
    std::vector<CK_ID> GetBehaviorIds() const;

    /**
     * Gets a vector of all operation IDs
     * @return Vector of operation IDs
     */
    std::vector<CK_ID> GetOperationIds() const;

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
    void ConstructGraph(BehaviorData &behaviorGraph, CKBehavior *behavior);

    /**
     * Sorts behavior links in appropriate order for left-to-right layout
     * @param behaviorLinks Vector of behavior links to sort
     */
    void SortBehaviorLinks(std::vector<Link*>& behaviorLinks);

    /**
     * Connects all behaviors directly to root when no valid links exist
     * @param behavior Root behavior
     */
    void ConnectDisconnectedBehaviorsToRoot(CKBehavior* behavior);

    /**
     * Connects behaviors that have no incoming edges
     * @param behaviorGraph Behavior graph containing all behaviors
     * @param rootId ID of the root behavior
     */
    void ConnectOrphanedBehaviors(BehaviorData &behaviorGraph, CK_ID rootId);

    /**
     * Helper for calculating minimum distances in the graph
     * @param nodeQueue Queue of nodes to process
     */
    void CalculateDistancesFromQueue(std::queue<CK_ID> &nodeQueue);

    /**
     * Calculates minimum distances from the root
     * @param behaviorGraph Behavior graph
     */
    void CalculateGraphDistances(BehaviorData &behaviorGraph);

    /**
     * Calculates the size of a subgraph
     * @param behaviorBlock Behavior building block
     * @param isRoot Whether this is the root node
     * @return Size Rect
     */
    Rect CalculateSubgraphSize(BehaviorData &behaviorBlock, bool isRoot);

    /**
     * Places a behavior within its parent
     * @param behaviorBlock Behavior building block
     * @param horizontalPos Horizontal position
     * @param verticalPos Vertical position
     * @param isRoot Whether this is the root node
     */
    void PlaceBehaviorInParent(BehaviorData &behaviorBlock, float horizontalPos, float verticalPos, bool isRoot);

    /**
     * Calculates positions for behaviors in the graph
     * @param behaviorGraph Behavior graph
     * @param behavior Behavior
     * @param isScript Whether the behavior is a script
     * @return Vertical center position
     */
    float CalculateBehaviorPositions(BehaviorData &behaviorGraph, CKBehavior *behavior, bool isScript);

    /**
     * Moves a parameter to a position
     * @param parameter Parameter
     * @param position Position
     */
    void MoveParameterToPosition(Parameter &parameter, const Point &position);

    /**
     * Moves an operation to a position
     * @param operation Operation
     * @param position Position
     */
    void MoveOperationToPosition(Operation &operation, const Point &position);

    /**
     * Gets the input position for an interface
     * @param targetId Target ID
     * @param inputIndex Input position
     * @return Point
     */
    Point GetInputParamPosition(CK_ID targetId, int inputIndex);

    /**
     * Gets the output position for an interface
     * @param targetId Target ID
     * @param outputIndex Output position
     * @return Point
     */
    Point GetOutputParamPosition(CK_ID targetId, int outputIndex);

    /**
     * Calculates positions for operations
     * @param behaviorGraph Behavior graph
     */
    void CalculateOperationPositions(BehaviorData &behaviorGraph);

    /**
     * Calculates positions for local parameters
     * @param behaviorGraph Behavior graph
     * @param isInputDirection Direction (true for inputs, false for outputs)
     */
    void CalculateLocalParameterPositions(BehaviorData &behaviorGraph, bool isInputDirection);

    /**
     * Sets start information for the behavior script
     * @param script The script behavior
     * @param verticalStartPos Vertical start position
     * @param verticalSize Vertical size
     */
    void SetStart(BehaviorData &script, float verticalStartPos, float verticalSize);

    /**
     * Recalculates absolute positions for behaviors
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     * @param startHorizontal Starting horizontal position
     * @param startVertical Starting vertical position
     */
    void RecalculateAbsolutePositions(BehaviorData &behaviorData, CKBehavior *behavior, float startHorizontal, float startVertical);
};