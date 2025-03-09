#pragma once

#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>
#include <string>

#include "CKDefines.h"
#include "CKStateChunk.h"

// Forward declarations
class CKContext;
class CKBehavior;
class CKFile;

/**
 * @struct SchematicNode
 * @brief Structure to hold information for serializing a behavior node
 */
struct SchematicNode {
    CKBehavior *behavior = nullptr; ///< Pointer to the behavior
    CKStateChunk *chunk = nullptr;  ///< State chunk for serialization
    CKBOOL isNotScript = FALSE;     ///< Whether this is not a script
    CKBOOL isBuildingBlock = FALSE; ///< Whether this is a building block
    CKDWORD flag = 0;               ///< Flags
    CKDWORD version = 0x16;         ///< Version
    CKDWORD scriptIndex = 0;        ///< Script index
    CKDWORD buildingBlockIndex = 0; ///< Building block index
};

/**
 * @file InterfaceData.h
 * @brief Data structures for visual representation of behavior trees
 */

//------------------------------------------------------
// Constants and types
//------------------------------------------------------

/**
 * @enum LinkType
 * @brief Defines the types of links in behavior networks
 */
enum LinkType {
    LINK_TYPE_BEHAVIOR     = 1,      ///< Behavior link (control flow)
    LINK_TYPE_PARAMETER    = 2,      ///< Parameter link (data flow)
    LINK_TYPE_PARAMETER_OP = 0x10002 ///< Parameter operation link
};

/**
 * @enum EndpointType
 * @brief Defines the types of endpoints for links
 */
enum EndpointType {
    ENDPOINT_POUT_SHORTCUT = 5,  ///< Parameter output link shortcut
    ENDPOINT_PIN           = 7,  ///< Parameter input
    ENDPOINT_POUT          = 8,  ///< Parameter output
    ENDPOINT_PLOCAL        = 9,  ///< Local parameter
    ENDPOINT_TARGET_PIN    = 10, ///< Target parameter input
    ENDPOINT_BIN           = 12, ///< Behavior input
    ENDPOINT_BOUT          = 13, ///< Behavior output
    ENDPOINT_START_BIN     = 26  ///< "Start" behavior input
};

/**
 * @enum ParameterStyle
 * @brief Defines the display style for parameters
 */
enum ParameterStyle {
    PARAM_STYLE_NAME      = 0x200,  ///< Display parameter name only
    PARAM_STYLE_CLOSED    = 0x400,  ///< Parameter is closed/collapsed
    PARAM_STYLE_NAMEVALUE = 0x1000, ///< Display parameter name and value
    PARAM_STYLE_VALUE     = 0x2000  ///< Display parameter value only
};

//------------------------------------------------------
// Core data structures
//------------------------------------------------------

/**
 * @struct Rect
 * @brief Represents a rectangular area in the interface
 */
struct Rect {
    float hPos = 300.0f;  ///< Horizontal position (left)
    float vPos = 100.0f;  ///< Vertical position (top)
    float hSize = 100.0f; ///< Width
    float vSize = 40.0f;  ///< Height

    Rect() = default;

    /**
     * @brief Constructor with all parameters
     */
    Rect(float hPosition, float vPosition, float width, float height)
        : hPos(hPosition), vPos(vPosition), hSize(width), vSize(height) {
    }

    /**
     * @brief Sets the position of the Rect
     */
    void SetPosition(float h, float v) {
        hPos = h;
        vPos = v;
    }

    /**
     * @brief Sets the size of the Rect
     */
    void SetSize(float width, float height) {
        hSize = width;
        vSize = height;
    }

    /**
     * @brief Returns the right edge position
     */
    float Right() const { return hPos + hSize; }

    /**
     * @brief Returns the bottom edge position
     */
    float Bottom() const { return vPos + vSize; }

    /**
     * @brief Returns the center x-coordinate
     */
    float CenterX() const { return hPos + hSize / 2.0f; }

    /**
     * @brief Returns the center y-coordinate
     */
    float CenterY() const { return vPos + vSize / 2.0f; }
};

/**
 * @struct Point
 * @brief Represents a 2D point in the interface
 */
struct Point {
    float h = 0.0f; ///< Horizontal coordinate
    float v = 0.0f; ///< Vertical coordinate

    Point() = default;

    Point(float horizontal, float vertical) : h(horizontal), v(vertical) {
    }

    /**
     * @brief Calculates distance to another point
     */
    float DistanceTo(const Point &other) const {
        float dx = h - other.h;
        float dy = v - other.v;
        return std::sqrt(dx * dx + dy * dy);
    }
};

/**
 * @struct StartPoint
 * @brief Represents the starting point of a behavior script
 */
struct StartPoint {
    CK_ID id = 0;             ///< ID of the start point
    float vStart = 0.0f;      ///< Vertical offset where execution begins
    float hStartPos = 140.0f; ///< Horizontal position of start
    float vStartPos = 0.0f;   ///< Vertical position of start
    float vSize = 0.0f;       ///< Vertical size of the start region
};

/**
 * @struct LinkEndpoint
 * @brief Represents an endpoint of a link in the behavior tree
 */
struct LinkEndpoint {
    CK_ID id = 0;                                     ///< ID of the endpoint object
    int index = 0;                                    ///< Index of the endpoint within its owner
    EndpointType type = static_cast<EndpointType>(0); ///< Type of the endpoint

    LinkEndpoint() = default;

    LinkEndpoint(CK_ID endpointId, int endpointIndex, EndpointType endpointType)
        : id(endpointId), index(endpointIndex), type(endpointType) {
    }

    /**
     * @brief Convenience constructor from raw type value
     */
    LinkEndpoint(CK_ID endpointId, int endpointIndex, int endpointType)
        : id(endpointId), index(endpointIndex), type(static_cast<EndpointType>(endpointType)) {
    }

    /**
     * @brief Checks if this endpoint is a parameter input
     */
    bool IsParameterInput() const {
        return type == ENDPOINT_PIN || type == ENDPOINT_TARGET_PIN;
    }

    /**
     * @brief Checks if this endpoint is a parameter output
     */
    bool IsParameterOutput() const {
        return type == ENDPOINT_POUT || type == ENDPOINT_POUT_SHORTCUT;
    }

    /**
     * @brief Checks if this endpoint is a behavior input
     */
    bool IsBehaviorInput() const {
        return type == ENDPOINT_BIN || type == ENDPOINT_START_BIN;
    }

    /**
     * @brief Checks if this endpoint is a behavior output
     */
    bool IsBehaviorOutput() const {
        return type == ENDPOINT_BOUT;
    }
};

/**
 * @struct Link
 * @brief Represents a connection between two points in the behavior tree
 */
struct Link {
    CK_ID id = 0;                             ///< ID of the link
    LinkType type = static_cast<LinkType>(0); ///< Type of the link
    LinkEndpoint start;                       ///< Starting endpoint
    int pointCount = 0;                       ///< Number of control points
    std::vector<Point> points;                ///< Control points for link routing
    LinkEndpoint end;                         ///< Ending endpoint

    /**
     * @brief Default constructor
     */
    Link() = default;

    /**
     * @brief Constructor with all fields
     */
    Link(CK_ID linkId, LinkType linkType,
         const LinkEndpoint &startPoint, const LinkEndpoint &endPoint)
        : id(linkId), type(linkType), start(startPoint), end(endPoint), pointCount(0) {
    }

    /**
     * @brief Create a parameter link
     */
    static Link CreateParameter(CK_ID linkId, const LinkEndpoint &startPoint, const LinkEndpoint &endPoint) {
        return {linkId, LINK_TYPE_PARAMETER, startPoint, endPoint};
    }

    /**
     * @brief Create a behavior link
     */
    static Link CreateBehavior(CK_ID linkId, const LinkEndpoint &startPoint, const LinkEndpoint &endPoint) {
        return {linkId, LINK_TYPE_BEHAVIOR, startPoint, endPoint};
    }

    /**
     * @brief Create a parameter operation link
     */
    static Link CreateParameterOp(CK_ID linkId, const LinkEndpoint &startPoint, const LinkEndpoint &endPoint) {
        return {linkId, LINK_TYPE_PARAMETER_OP, startPoint, endPoint};
    }

    /**
     * @brief Adds a control point to the link
     */
    void AddControlPoint(const Point &point) {
        points.push_back(point);
        pointCount = static_cast<int>(points.size());
    }

    /**
     * @brief Checks if this is a behavior link
     */
    bool IsBehaviorLink() const {
        return type == LINK_TYPE_BEHAVIOR;
    }

    /**
     * @brief Checks if this is a parameter link
     */
    bool IsParameterLink() const {
        return type == LINK_TYPE_PARAMETER;
    }

    /**
     * @brief Checks if this is a parameter operation link
     */
    bool IsParameterOpLink() const {
        return type == LINK_TYPE_PARAMETER_OP;
    }

    /**
     * @brief Optimizes control points by removing redundant ones
     */
    void OptimizePath() {
        if (points.size() <= 2) return;

        // Simple implementation - remove points that are in a straight line
        std::vector<Point> optimized;
        optimized.push_back(points.front());

        for (size_t i = 1; i < points.size() - 1; ++i) {
            const Point &prev = optimized.back();
            const Point &curr = points[i];
            const Point &next = points[i + 1];

            // If points are not collinear, keep the current point
            if (std::abs((curr.v - prev.v) * (next.h - curr.h) -
                (curr.h - prev.h) * (next.v - curr.v)) > 0.001f) {
                optimized.push_back(curr);
            }
        }

        optimized.push_back(points.back());
        points = std::move(optimized);
        pointCount = static_cast<int>(points.size());
    }
};

/**
 * @struct Operation
 * @brief Represents a parameter operation in the behavior tree
 */
struct Operation {
    CK_ID id = 0;      ///< ID of the operation
    float hPos = 0.0f; ///< Horizontal position
    float vPos = 0.0f; ///< Vertical position

    /**
     * @brief Sets the position of the operation
     */
    void SetPosition(float h, float v) {
        hPos = h;
        vPos = v;
    }
};

/**
 * @struct Comment
 * @brief Represents a comment in the behavior tree
 */
struct Comment {
    CK_ID id = 0;           ///< ID of the comment
    float hPos = 0.0f;      ///< Horizontal position
    float vPos = 0.0f;      ///< Vertical position
    float width = 100.0f;   ///< Width of the comment
    float height = 50.0f;   ///< Height of the comment
    std::string text;       ///< Comment text content
    bool collapsed = false; ///< Whether the comment is collapsed to an icon
};

/**
 * @struct Parameter
 * @brief Represents a parameter in the behavior tree
 */
struct Parameter {
    CK_ID id = 0;                            ///< ID of the parameter
    int hPos = 0;                            ///< Horizontal position
    int vPos = 0;                            ///< Vertical position
    ParameterStyle style = PARAM_STYLE_NAME; ///< Display style
    CK_ID sourceId = static_cast<CK_ID>(-1); ///< Source parameter ID for shortcuts

    /**
     * @brief Checks if this parameter is a shortcut
     */
    bool IsShortcut() const {
        return sourceId != static_cast<CK_ID>(-1);
    }

    /**
     * @brief Sets the position of the parameter
     */
    void SetPosition(int h, int v) {
        hPos = h;
        vPos = v;
    }
};

/**
 * @struct BehaviorBlock
 * @brief Represents a behavior building block in the tree
 */
struct BehaviorBlock {
    CK_ID id = 0;                 ///< ID of the behavior block
    bool folded = false;          ///< Whether the block is collapsed
    int depth = 0;                ///< Depth in the behavior hierarchy
    Rect size;                    ///< Size and position of the block
    float hExpandSize = 0.0f;     ///< Expanded horizontal size
    float vExpandSize = 0.0f;     ///< Expanded vertical size
    bool isBehaviorGraph = false; ///< Whether this is a behavior graph

    // Links
    int linkCount = 0;       ///< Number of links
    std::vector<Link> links; ///< Links within this block

    // Operations
    int operationCount = 0;            ///< Number of operations
    std::vector<Operation> operations; ///< Operations within this block

    // Comments
    int commentCount = 0;          ///< Number of comments
    std::vector<Comment> comments; ///< Comments within this block

    // Parameters
    int localParamCount = 0;            ///< Number of local parameters
    std::vector<Parameter> localParams; ///< Local parameters

    int sharedParamCount = 0;            ///< Number of shared parameters
    std::vector<Parameter> sharedParams; ///< Shared parameters

    // Input/Output indices for graph behaviors
    int inputCount = 0;             ///< Number of inputs
    std::vector<int> inwardInputs;  ///< Inward-facing inputs
    std::vector<int> outwardInputs; ///< Outward-facing inputs

    int outputCount = 0;             ///< Number of outputs
    std::vector<int> inwardOutputs;  ///< Inward-facing outputs
    std::vector<int> outwardOutputs; ///< Outward-facing outputs

    /**
     * @brief Adds a link to the behavior block
     */
    void AddLink(const Link &link) {
        links.push_back(link);
        linkCount = static_cast<int>(links.size());
    }

    /**
     * @brief Adds an operation to the behavior block
     */
    void AddOperation(const Operation &op) {
        operations.push_back(op);
        operationCount = static_cast<int>(operations.size());
    }

    /**
     * @brief Adds a local parameter to the behavior block
     */
    void AddLocalParameter(const Parameter &param) {
        localParams.push_back(param);
        localParamCount = static_cast<int>(localParams.size());
    }

    /**
     * @brief Adds a shared parameter to the behavior block
     */
    void AddSharedParameter(const Parameter &param) {
        sharedParams.push_back(param);
        sharedParamCount = static_cast<int>(sharedParams.size());
    }

    /**
     * @brief Adds a comment to the behavior block
     */
    void AddComment(const Comment &comment) {
        comments.push_back(comment);
        commentCount = static_cast<int>(comments.size());
    }

    /**
     * @brief Finds a link by ID
     * @return Pointer to the link or nullptr if not found
     */
    Link *FindLink(CK_ID linkId) {
        auto it = std::find_if(links.begin(), links.end(),
                               [linkId](const Link &link) { return link.id == linkId; });
        return it != links.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Finds an operation by ID
     * @return Pointer to the operation or nullptr if not found
     */
    Operation *FindOperation(CK_ID opId) {
        auto it = std::find_if(operations.begin(), operations.end(),
                               [opId](const Operation &op) { return op.id == opId; });
        return it != operations.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Finds a local parameter by ID
     * @return Pointer to the parameter or nullptr if not found
     */
    Parameter *FindLocalParameter(CK_ID paramId) {
        auto it = std::find_if(localParams.begin(), localParams.end(),
                               [paramId](const Parameter &param) { return param.id == paramId; });
        return it != localParams.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Finds a shared parameter by ID
     * @return Pointer to the parameter or nullptr if not found
     */
    Parameter *FindSharedParameter(CK_ID paramId) {
        auto it = std::find_if(sharedParams.begin(), sharedParams.end(),
                               [paramId](const Parameter &param) { return param.id == paramId; });
        return it != sharedParams.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Resets all data in the behavior block
     */
    void Reset() {
        id = 0;
        folded = false;
        depth = 0;
        size = Rect();
        hExpandSize = 0.0f;
        vExpandSize = 0.0f;
        isBehaviorGraph = false;
        links.clear();
        linkCount = 0;
        operations.clear();
        operationCount = 0;
        comments.clear();
        commentCount = 0;
        localParams.clear();
        localParamCount = 0;
        sharedParams.clear();
        sharedParamCount = 0;
        inputCount = 0;
        inwardInputs.clear();
        outwardInputs.clear();
        outputCount = 0;
        inwardOutputs.clear();
        outwardOutputs.clear();
    }
};

/**
 * @struct InterfaceData
 * @brief Main container for behavior tree visual representation
 */
struct InterfaceData {
    StartPoint start;                          ///< Start point of the script
    BehaviorBlock scriptRoot;                  ///< Root behavior block
    int behaviorBlockCount = 0;                ///< Number of behavior blocks
    std::vector<BehaviorBlock> behaviorBlocks; ///< Behavior blocks in the tree

    /**
     * @brief Adds a behavior block to the interface
     */
    void AddBehaviorBlock(const BehaviorBlock &block) {
        behaviorBlocks.push_back(block);
        behaviorBlockCount = static_cast<int>(behaviorBlocks.size());
    }

    /**
     * @brief Finds a behavior block by ID
     * @return Pointer to the behavior block if found, nullptr otherwise
     */
    BehaviorBlock *FindBehaviorBlock(CK_ID id) {
        if (scriptRoot.id == id) {
            return &scriptRoot;
        }

        auto it = std::find_if(behaviorBlocks.begin(), behaviorBlocks.end(),
                               [id](const BehaviorBlock &block) { return block.id == id; });
        return it != behaviorBlocks.end() ? &(*it) : nullptr;
    }

    /**
     * @brief Clears all data in the interface
     */
    void Clear() {
        start = StartPoint();
        scriptRoot.Reset();
        behaviorBlocks.clear();
        behaviorBlockCount = 0;
    }

    //------------------------------------------------------
    // Serialization Methods
    //------------------------------------------------------

    /**
     * @brief Generates an interface chunk for serialization
     * @param node SchematicNode containing behavior and chunk information
     * @return CK_OK if successful
     */
    CKERROR GenerateInterfaceChunk(SchematicNode &node);

    /**
     * @brief Saves the script header to the chunk
     * @param node SchematicNode containing behavior and chunk information
     * @return TRUE if successful
     */
    CKBOOL SaveScriptHeader(SchematicNode &node);

    /**
     * @brief Saves link information to the chunk
     * @param node SchematicNode containing behavior and chunk information
     */
    void SaveScriptLinks(SchematicNode &node);

    /**
     * @brief Saves operation information to the chunk
     * @param node SchematicNode containing behavior and chunk information
     */
    void SaveScriptOps(SchematicNode &node);

    /**
     * @brief Saves comment information to the chunk
     * @param node SchematicNode containing behavior and chunk information
     */
    void SaveScriptComments(SchematicNode &node);

    /**
     * @brief Saves parameter information to the chunk
     * @param node SchematicNode containing behavior and chunk information
     */
    void SaveScriptParameters(SchematicNode &node);

    /**
     * @brief Saves graph information to the chunk
     * @param node SchematicNode containing behavior and chunk information
     */
    void SaveScriptGraph(SchematicNode &node);

    /**
     * @brief Saves extra information to the chunk
     * @param node SchematicNode containing behavior and chunk information
     */
    void SaveScriptExtra(SchematicNode &node);

private:
    /**
     * @brief Gets the appropriate behavior block based on the node settings
     * @param node SchematicNode containing behavior and chunk information
     * @return Reference to the behavior block
     */
    BehaviorBlock &GetBehaviorBlockForNode(const SchematicNode &node) {
        return !node.isNotScript ? scriptRoot : behaviorBlocks[node.buildingBlockIndex];
    }
};

//------------------------------------------------------
// Implementation of serialization methods
//------------------------------------------------------

/**
 * @brief Generates an interface chunk from a behavior
 * @param data Interface data to serialize
 * @param behavior Behavior to generate chunk for
 * @return Generated state chunk
 */
CKStateChunk *GenerateInterfaceChunk(InterfaceData &data, CKBehavior *behavior);

/**
 * @brief Decorates a behavior into interface data and generates a state chunk
 * @param behavior Behavior to decorate
 * @return Generated state chunk, or nullptr on failure
 */
CKStateChunk *DecorateAndGenerateChunk(CKBehavior *behavior);