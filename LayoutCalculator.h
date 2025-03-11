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
    // Layout constants
    static constexpr int MAX_FIX_STACK_OPS = 3;
    static constexpr float HORIZONTAL_SPACING = 20.0f;
    static constexpr float VERTICAL_SPACING = 20.0f;
    static constexpr float LINK_MARGIN = 10.0f;

    // Grid constants
    static constexpr float GRID_HALF_CELL = HORIZONTAL_SPACING / 2.0f;
    static constexpr float GRID_QUARTER_CELL = HORIZONTAL_SPACING / 4.0f;

    // Position adjustment constants
    static constexpr float OPERATION_H_OFFSET = 1.0f;
    static constexpr float OPERATION_V_OFFSET = 2.0f;
    static constexpr float BEHAVIOR_H_START_OFFSET = 9.0f; // 7 + 2
    static constexpr float BEHAVIOR_V_START_OFFSET = 2.0f;
    static constexpr float BEHAVIOR_PADDING = 2.0f;

    // Link routing constants
    static constexpr float LOOP_SIZE = 30.0f;
    static constexpr float BEHAVIOR_IO_OFFSET = VERTICAL_SPACING / 2.0f;
    static constexpr float BEHAVIOR_TOP_OFFSET = VERTICAL_SPACING / 2.0f;
    static constexpr float BEHAVIOR_BOTTOM_OFFSET = VERTICAL_SPACING / 2.0f;
    static constexpr float PARAMETER_LINK_VERTICAL_OFFSET = VERTICAL_SPACING;
    static constexpr float PARAM_OP_OFFSET = HORIZONTAL_SPACING / 4.0f;

    // Expansion and margin constants
    static constexpr float EXPANSION_PADDING = 4.0f;
    static constexpr float BEHAVIOR_EXPANSION_FACTOR = 10.0f;

    // Data structures for graph representation
    struct Vertex {
        int incomingEdgeCount = 0;
        int firstEdgeIndex = -1;
    };

    struct Edge {
        CK_ID sourceId = 0;
        CK_ID targetId = 0;
        int nextEdgeIndex = -1;
    };

    struct OrphanedBehavior {
        CK_ID id;
        float verticalPosition;
    };

    // Link type enumeration for more accurate routing
    enum class LinkType {
        BehaviorFlow,      // Behavior link (bLink)
        ParameterData,     // Parameter link (pLink)
        ParameterOperation // Parameter operation link
    };

    // Structure to hold link characteristics for routing decisions
    struct LinkCharacteristics {
        LinkType linkType = LinkType::BehaviorFlow;
        bool isSelfConnection = false;
        bool isStartLink = false;
        bool isVerticalAlignment = false;
        bool isHorizontalAlignment = false;

        // Endpoint characteristics
        bool isSourceBehaviorOutput = false;
        bool isSourceParameterOutput = false;
        bool isTargetBehaviorInput = false;
        bool isTargetParameterInput = false;
        bool isSourceOperation = false;
        bool isTargetOperation = false;
        bool isParameterShortcut = false;

        // Get whether this is a typical behavior link (BBOut -> BBIn)
        bool IsBehaviorFlowLink() const {
            return linkType == LinkType::BehaviorFlow &&
                   isSourceBehaviorOutput &&
                   isTargetBehaviorInput;
        }

        // Get whether this is a parameter data link (paramOut -> paramIn)
        bool IsParameterDataLink() const {
            return linkType == LinkType::ParameterData &&
                   isSourceParameterOutput &&
                   isTargetParameterInput;
        }

        // Get whether this is an operation link
        bool IsOperationLink() const {
            return linkType == LinkType::ParameterOperation ||
                   isSourceOperation ||
                   isTargetOperation;
        }

        bool IsShortcutLink() const {
            return isParameterShortcut;
        }
    };

    // Member variables
    InterfaceData &m_Data;
    CKContext *m_Context;
    std::unordered_map<CK_ID, Vertex> m_Vertices;
    std::unordered_map<CK_ID, int> m_DistanceFromRoot;
    std::unordered_map<CK_ID, Rect> m_RequiredSize;
    std::unordered_map<CK_ID, int> m_PredecessorEdge;
    std::vector<Edge> m_Edges;
    std::unordered_set<CK_ID> m_MovedOperations;

    //------------------------------------------------------
    // Layout Process Coordination
    //------------------------------------------------------

    /**
     * Initializes graph state for a new layout calculation
     */
    void InitializeGraphState();

    /**
     * Calculates layouts for all behaviors
     * @param behaviorIds Ordered list of behavior IDs
     */
    void CalculateBehaviorLayouts(const std::vector<CK_ID> &behaviorIds);

    /**
     * Calculates the size of a behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     */
    void CalculateBehaviorSize(BehaviorData &behaviorData, CKBehavior *behavior);

    /**
     * Calculates positions for operations and parameters
     * @param behaviorIds Ordered list of behavior IDs
     */
    void CalculateElementPositions(const std::vector<CK_ID> &behaviorIds);

    /**
     * Finalizes script layout and sets script header
     * @param script The root behavior
     */
    void FinalizeScriptLayout(CKBehavior *script);

    /**
     * Calculates routes for all links in the behaviors
     */
    void CalculateAllLinkRoutes();

    //------------------------------------------------------
    // Utility Methods
    //------------------------------------------------------

    /**
     * Gets a behavior data by ID
     * @param id Behavior ID
     * @return Pointer to behavior data or nullptr
     */
    BehaviorData* GetBehaviorData(CK_ID id) const;

    /**
     * Gets an operation by ID
     * @param id Operation ID
     * @return Pointer to operation or nullptr
     */
    Operation* GetOperation(CK_ID id) const;

    /**
     * Checks if an ID is an operation
     * @param id ID to check
     * @return True if ID represents an operation
     */
    bool IsOperation(CK_ID id) const;

    /**
     * Gets all behavior IDs in consistent order
     * @return Vector of behavior IDs
     */
    std::vector<CK_ID> GetBehaviorIds() const;

    /**
     * Gets all operation IDs in consistent order
     * @return Vector of operation IDs
     */
    std::vector<CK_ID> GetOperationIds() const;

    //------------------------------------------------------
    // Graph Construction and Analysis
    //------------------------------------------------------

    /**
     * Constructs graph representation of a behavior
     * @param behaviorGraph Behavior graph data
     * @param behavior CK behavior
     */
    void ConstructGraph(BehaviorData &behaviorGraph, CKBehavior *behavior);

    /**
     * Creates vertices for all behaviors
     * @param behavior CK behavior
     */
    void CreateGraphVertices(CKBehavior *behavior);

    /**
     * Gets valid behavior links from the graph
     * @param behaviorGraph Behavior graph data
     * @return Vector of valid link pointers
     */
    std::vector<Link *> GetValidBehaviorLinks(BehaviorData &behaviorGraph);

    /**
     * Adds behavior links to the graph
     * @param behaviorLinks Vector of behavior links
     */
    void AddBehaviorLinksToGraph(const std::vector<Link *> &behaviorLinks);

    /**
     * Adds an edge to the graph
     * @param sourceId Source node ID
     * @param targetId Target node ID
     */
    void AddGraphEdge(CK_ID sourceId, CK_ID targetId);

    /**
     * Sorts behavior links for optimal layout
     * @param behaviorLinks Vector of behavior links
     */
    void SortBehaviorLinks(std::vector<Link*>& behaviorLinks);

    /**
     * Connects all behaviors to root when no valid links exist
     * @param behavior Root behavior
     */
    void ConnectDisconnectedBehaviorsToRoot(CKBehavior* behavior);

    /**
     * Connects behaviors without incoming edges
     * @param behaviorGraph Behavior graph data
     * @param rootId Root behavior ID
     */
    void ConnectOrphanedBehaviors(BehaviorData &behaviorGraph, CK_ID rootId);

    /**
     * Finds behaviors without incoming edges
     * @param behaviorGraph Behavior graph data
     * @param rootId Root behavior ID
     * @return Vector of orphaned behaviors
     */
    std::vector<OrphanedBehavior> FindOrphanedBehaviors(BehaviorData &behaviorGraph, CK_ID rootId);

    /**
     * Gets vertical position of a behavior
     * @param behaviorGraph Behavior graph data
     * @param behaviorId Behavior ID
     * @return Vertical position
     */
    float GetBehaviorVerticalPosition(BehaviorData &behaviorGraph, CK_ID behaviorId);

    /**
     * Sorts orphaned behaviors by position
     * @param orphanedBehaviors Vector of orphaned behaviors
     */
    void SortOrphanedBehaviors(std::vector<OrphanedBehavior> &orphanedBehaviors);

    /**
     * Connects orphaned behaviors in a chain
     * @param orphanedBehaviors Vector of orphaned behaviors
     * @param rootId Root behavior ID
     */
    void ConnectOrphanedBehaviorsChain(const std::vector<OrphanedBehavior> &orphanedBehaviors, CK_ID rootId);

    /**
     * Calculates distances from queue of nodes
     * @param nodeQueue Queue of nodes to process
     */
    void CalculateDistancesFromQueue(std::queue<CK_ID> &nodeQueue);

    /**
     * Calculates minimum distances from root
     * @param behaviorGraph Behavior graph data
     */
    void CalculateGraphDistances(BehaviorData &behaviorGraph);

    //------------------------------------------------------
    // Size and Position Calculation
    //------------------------------------------------------

    /**
     * Calculates required size for a subgraph
     * @param behaviorData Behavior data
     * @param isRoot Whether this is the root node
     * @return Required size
     */
    Rect CalculateSubgraphSize(BehaviorData &behaviorData, bool isRoot);

    /**
     * Places a behavior within its parent
     * @param behaviorData Behavior data
     * @param hPos Horizontal position
     * @param vPos Vertical position
     * @param isRoot Whether this is the root node
     */
    void PlaceBehaviorInParent(BehaviorData &behaviorData, float hPos, float vPos, bool isRoot);

    /**
     * Calculates positions for behaviors
     * @param behaviorGraph Behavior graph data
     * @param behavior CK behavior
     * @param isScript Whether this is a script (root behavior)
     * @return Vertical center position
     */
    float CalculateBehaviorPositions(BehaviorData &behaviorGraph, CKBehavior *behavior, bool isScript);

    /**
     * Recalculates absolute positions for all elements
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     * @param startHorizontal Horizontal offset
     * @param startVertical Vertical offset
     */
    void RecalculateAbsolutePositions(BehaviorData &behaviorData, CKBehavior *behavior, float startHorizontal, float startVertical);

    /**
     * Sets start information for script
     * @param script Script behavior data
     * @param verticalStartPos Vertical start position
     * @param verticalSize Vertical size
     */
    void SetStart(BehaviorData &script, float verticalStartPos, float verticalSize);

    //------------------------------------------------------
    // Operation and Parameter Layout
    //------------------------------------------------------

    /**
     * Calculates positions for operations
     * @param behaviorGraph Behavior graph data
     */
    void CalculateOperationPositions(BehaviorData &behaviorGraph);

    /**
     * Calculates positions for local parameters
     * @param behaviorGraph Behavior graph data
     * @param isInputDirection Whether to process input direction
     */
    void CalculateLocalParameterPositions(BehaviorData &behaviorGraph, bool isInputDirection);

    /**
     * Moves a parameter to a position
     * @param parameter Parameter to move
     * @param position Target position
     */
    void MoveParameterToPosition(Parameter &parameter, const Point &position);

    /**
     * Moves an operation to a position
     * @param operation Operation to move
     * @param position Target position
     */
    void MoveOperationToPosition(Operation &operation, const Point &position);

    /**
     * Gets input parameter position
     * @param targetId Target element ID
     * @param inputIndex Input index
     * @return Position
     */
    Point GetInputParamPosition(CK_ID targetId, int inputIndex);

    /**
     * Gets output parameter position
     * @param targetId Target element ID
     * @param outputIndex Output index
     * @return Position
     */
    Point GetOutputParamPosition(CK_ID targetId, int outputIndex);

    //------------------------------------------------------
    // Link Routing
    //------------------------------------------------------

    /**
     * Calculates routes for all links in a behavior
     * @param behaviorData Behavior data
     */
    void CalculateLinkRoutes(BehaviorData &behaviorData);

    /**
     * Routes a single link
     * @param link Link to route
     */
    void RouteLink(Link &link);

    /**
     * Gets the position of a link endpoint
     * @param endpoint Link endpoint
     * @return Position
     */
    Point GetEndpointPosition(const LinkEndpoint &endpoint);

    /**
     * Gets position for a parameter input endpoint
     * @param endpoint Link endpoint
     * @return Position
     */
    Point GetParameterInputPosition(const LinkEndpoint &endpoint);

    /**
     * Gets position for a parameter output endpoint
     * @param endpoint Link endpoint
     * @return Position
     */
    Point GetParameterOutputPosition(const LinkEndpoint &endpoint);

    /**
     * Gets position for a local parameter endpoint
     * @param endpoint Link endpoint
     * @return Position
     */
    Point GetLocalParameterPosition(const LinkEndpoint &endpoint);

    /**
     * Gets position for a behavior input endpoint
     * @param endpoint Link endpoint
     * @return Position
     */
    Point GetBehaviorInputPosition(const LinkEndpoint &endpoint);

    /**
     * Gets position for a behavior output endpoint
     * @param endpoint Link endpoint
     * @return Position
     */
    Point GetBehaviorOutputPosition(const LinkEndpoint &endpoint);

    /**
     * Creates a path between two points
     * @param startPos Start position
     * @param endPos End position
     * @param link Link being routed
     * @return Vector of control points
     */
    std::vector<Point> CreatePath(const Point &startPos, const Point &endPos, const Link &link);

    /**
     * Gets characteristics of a link for routing
     * @param link Link being routed
     * @return Link characteristics
     */
    LinkCharacteristics DetermineRoutingCharacteristics(const Link &link);

    /**
     * Creates a path for a self-connection
     * @param startPos Start position
     * @param endPos End position
     * @param link Link being routed
     * @return Vector of control points
     */
    std::vector<Point> CreateSelfConnectionPath(const Point &startPos, const Point &endPos, const Link &link);

    /**
     * Creates a path for a start link
     * @param startPos Start position
     * @param endPos End position
     * @param isHorizontalAligned Whether link is horizontally aligned
     * @return Vector of control points
     */
    std::vector<Point> CreateStartLinkPath(const Point &startPos, const Point &endPos, bool isHorizontalAligned);

    /**
     * Creates a behavior flow link path (from behavior output to behavior input)
     * @param startPos Start position
     * @param endPos End position
     * @return Vector of control points
     */
    std::vector<Point> CreateBehaviorFlowPath(const Point &startPos, const Point &endPos);

    /**
     * Creates a parameter data link path (from parameter output to parameter input)
     * @param startPos Start position
     * @param endPos End position
     * @return Vector of control points
     */
    std::vector<Point> CreateParameterDataPath(const Point &startPos, const Point &endPos);

    /**
     * Creates a shortcut path for parameter links
     * @param startPos Start position
     * @param endPos End position
     * @param link Link being routed
     * @return Vector of control points
     */
    std::vector<Point> CreateParameterShortcutPath(const Point &startPos, const Point &endPos, const Link &link);

    /**
     * Creates a parameter operation link path
     * @param startPos Start position
     * @param endPos End position
     * @param characteristics Link characteristics
     * @return Vector of control points
     */
    std::vector<Point> CreateOperationLinkPath(const Point &startPos, const Point &endPos,
                                               const LinkCharacteristics &characteristics);

    /**
     * Determines if points are in a suitable alignment
     * @param startPos Start position
     * @param endPos End position
     * @param margin Margin for alignment check
     * @return Whether the points are aligned
     */
    bool ArePointsAligned(const Point &startPos, const Point &endPos, float margin) const;
};