#pragma once

#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <utility>

#include "InterfaceData.h"

class CKContext;
class CKBehavior;
class CKParameterIn;
class CKParameterOut;
class CKParameterLocal;
class CKParameter;
class CKParameterOperation;

/**
 * Decorator transforms a behavior tree into a visual representation
 * that can be rendered in the UI.
 */
class Decorator {
public:
    /**
     * Constructor
     * @param target_data Reference to the interface data to populate
     * @param context Pointer to the CK context
     */
    Decorator(InterfaceData &target_data, CKContext *context);

    /**
     * Sets start information for the behavior script
     * @param script The script behavior
     * @param verticalStartPos Vertical start position
     * @param verticalSize Vertical size
     */
    void DecorateStart(BehaviorBlock &script, float verticalStartPos, float verticalSize);

    /**
     * Decorates a behavior tree, populating the interface data
     * @param script The root behavior to decorate
     */
    void Decorate(CKBehavior *script);

private:
    // Reference to the interface data
    InterfaceData &m_Data;

    // Pointer to the CK context
    CKContext *m_Context;

    // Maximum fix stack operations
    static const int MAX_FIX_STACK_OPS = 3;

    // Maps to track object relationships
    std::unordered_map<CK_ID, int> m_BehaviorMap;                  // Maps behavior ID to index in behaviorBlocks array
    std::unordered_map<CK_ID, std::pair<int, int>> m_OperationMap; // Maps operation ID to <block index, op index>

    // Sets to track parameters
    std::unordered_set<CK_ID> m_InputParams;  // Input parameter IDs
    std::unordered_set<CK_ID> m_OutputParams; // Output parameter IDs
    std::unordered_set<CK_ID> m_MovedOperations;

    /**
     * Gets a behavior block by ID
     * @param id Behavior ID
     * @return Reference to the behavior block
     */
    BehaviorBlock &GetBehaviorBlock(CK_ID id);

    /**
     * Gets an operation by ID
     * @param id Operation ID
     * @return Reference to the operation
     */
    Operation &GetOperation(CK_ID id);

    //------------------------------------------------------------------
    // Parameter-related types and methods
    //------------------------------------------------------------------

    /**
     * Structure to represent a parameter IO position
     */
    struct ParameterPosition {
        CK_ID id;         // Parameter ID
        int index;        // Parameter index
        CK_ID behaviorId; // Parent behavior ID
    };

    /**
     * Gets the position information for an input parameter
     * @param inputParam Input parameter
     * @param owner Output: Owner behavior
     * @return Position information
     */
    ParameterPosition GetInputParameterPosition(CKParameterIn *inputParam, CKBehavior **owner);

    /**
     * Gets the position information for an output parameter
     * @param outputParam Output parameter
     * @param ownerBehavior Output: Owner behavior
     * @return Position information
     */
    ParameterPosition GetOutputParameterPosition(CKParameterOut *outputParam, CKBehavior **ownerBehavior);

    /**
     * Gets the position information for a local parameter
     * @param localParam Local parameter
     * @return Position information
     */
    ParameterPosition GetLocalParameterPosition(CKParameterLocal *localParam);

    /**
     * Gets the link endpoint information for a parameter
     * @param parameter Parameter
     * @return Link endpoint
     */
    LinkEndpoint GetParameterEndpoint(CKParameter *parameter);

    /**
     * Gets the owner behavior of a parameter
     * @param parameter Parameter
     * @return Owner behavior
     */
    CKBehavior *GetParameterOwnerBehavior(CKParameter *parameter);

    /**
     * Gets a shortcut parameter position
     * @param behaviorId Parent behavior ID
     * @param sourceId Source parameter ID
     * @return Parameter position
     */
    ParameterPosition GetShortcutParameterPosition(CK_ID behaviorId, CK_ID sourceId);

    /**
     * Configures parameter links for a behavior tree
     * @param root Root behavior
     */
    void ConfigureParameterLinks(CKBehavior *root);

    //------------------------------------------------------------------
    // Graph structure and layout
    //------------------------------------------------------------------

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
    std::unordered_map<CK_ID, std::vector<int>> m_Bridges;
    std::vector<Edge> m_Edges;

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

    //------------------------------------------------------------------
    // Visual property calculation
    //------------------------------------------------------------------

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
     * Checks if an ID is an operation
     * @param id ID to check
     * @return True if ID is an operation
     */
    bool IsOperation(CK_ID id);

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

    /**
     * Recalculates absolute positions for behaviors
     * @param behaviorBlock Behavior
     * @param behavior CK behavior
     * @param startHorizontal Starting horizontal position
     * @param startVertical Starting vertical position
     */
    void RecalculateAbsolutePositions(BehaviorBlock &behaviorBlock, CKBehavior *behavior, float startHorizontal,
                                      float startVertical);

    //------------------------------------------------------------------
    // Behavior decoration
    //------------------------------------------------------------------

    /**
     * Decorates a single behavior
     * @param behaviorBlock Behavior building block
     * @param behavior CK behavior
     * @param depth Depth in the tree
     */
    void DecorateBehavior(BehaviorBlock &behaviorBlock, CKBehavior *behavior, int depth);

    /**
     * Decorates all behaviors in the tree
     * @param rootBehavior Root behavior
     */
    void DecorateBehaviorTree(CKBehavior *rootBehavior);
};

/**
 * Global function to decorate a behavior
 * @param data Interface data
 * @param behavior Behavior to decorate
 */
void Decorate(InterfaceData &data, CKBehavior *behavior);
