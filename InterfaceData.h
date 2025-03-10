#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <string>

#include "CKDefines.h"
#include "CKBehavior.h"
#include "CKStateChunk.h"

#undef min
#undef max

// Forward declarations
class InterfaceData;

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

/**
 * @enum CommentStyle
 * @brief Defines the display style for comments
 */
enum CommentStyle {
    COMMENT_STYLE_NORMAL      = 0x0, ///< Normal comment display
    COMMENT_STYLE_COLLAPSED   = 0x1, ///< Comment is collapsed to icon
    COMMENT_STYLE_LOCKED      = 0x2, ///< Comment is locked
    COMMENT_STYLE_TRANSPARENT = 0x4  ///< Comment is transparent
};

/**
 * @enum ExtraDataType
 * @brief Types of extra data that can be stored in interface chunks
 */
enum ExtraDataType {
    EXTRA_DATA_BEHAVIOR   = 1, ///< Behavior reference
    EXTRA_DATA_PARAMETER  = 2, ///< Parameter reference
    EXTRA_DATA_CONNECTION = 3, ///< Connection between objects
    EXTRA_DATA_VALUE      = 4  ///< Numeric value
};

// Forward declarations for core classes
struct Rect;
struct Point;
struct Header;
struct LinkEndpoint;
struct Link;
struct Operation;
struct Comment;
struct Parameter;
struct ExtraSubData;
struct ExtraData;
struct BehaviorData;

/**
 * @struct InterfaceElement
 * @brief Base class for all interface elements with common functionality
 */
struct InterfaceElement {
    CK_ID id = 0; ///< ID of the element

    InterfaceElement() = default;

    explicit InterfaceElement(CK_ID elementId) : id(elementId) {}

    virtual ~InterfaceElement() = default;
};

/**
 * @struct Point
 * @brief Represents a 2D point in the interface
 */
struct Point : InterfaceElement {
    float h = 0.0f; ///< Horizontal coordinate
    float v = 0.0f; ///< Vertical coordinate

    Point() = default;

    Point(float horizontal, float vertical) : h(horizontal), v(vertical) {}

    /**
     * @brief Calculates distance to another point
     */
    float DistanceTo(const Point &other) const;

    /**
     * @brief Calculates squared distance to another point (more efficient)
     */
    float DistanceSquaredTo(const Point &other) const;

    /**
     * @brief Adds another point to this one
     */
    Point operator+(const Point &other) const;

    /**
     * @brief Subtracts another point from this one
     */
    Point operator-(const Point &other) const;

    /**
     * @brief Scales the point by a factor
     */
    Point operator*(float factor) const;

    /**
     * @brief Divides the point by a factor
     */
    Point operator/(float factor) const;

    /**
     * @brief Adds another point to this one
     */
    Point &operator+=(const Point &other);

    /**
     * @brief Subtracts another point from this one
     */
    Point &operator-=(const Point &other);

    /**
     * @brief Scales the point by a factor
     */
    Point &operator*=(float factor);

    /**
     * @brief Divides the point by a factor
     */
    Point &operator/=(float factor);

    /**
     * @brief Checks if two points are equal
     */
    bool operator==(const Point &other) const;

    /**
     * @brief Checks if two points are not equal
     */
    bool operator!=(const Point &other) const;

    /**
     * @brief Calculates the dot product with another point
     */
    float Dot(const Point &other) const;

    /**
     * @brief Calculates the cross product with another point
     */
    float Cross(const Point &other) const;

    /**
     * @brief Returns the length of the vector
     */
    float Length() const;

    /**
     * @brief Returns the squared length of the vector (more efficient)
     */
    float LengthSquared() const;

    /**
     * @brief Normalizes the vector to unit length
     */
    Point Normalized() const;

    /**
     * @brief Rotates the point around the origin by the given angle (in radians)
     */
    Point Rotated(float angle) const;

    /**
     * @brief Calculates the angle between this point and another (in radians)
     */
    float AngleTo(const Point &other) const;

    /**
     * @brief Reflects the point across a normal vector
     */
    Point Reflected(const Point &normal) const;

    /**
     * @brief Linearly interpolates between this point and another
     * @param other The target point
     * @param t The interpolation factor (0-1)
     */
    Point Lerp(const Point &other, float t) const;

    /**
     * @brief Checks if the point is approximately equal to another
     * @param other The other point
     * @param epsilon The tolerance
     */
    bool ApproximatelyEqual(const Point &other, float epsilon = 0.001f) const;
};

/**
 * @struct Rect
 * @brief Represents a rectangular area in the interface
 */
struct Rect : InterfaceElement {
    float hPos = 300.0f;  ///< Horizontal position (left)
    float vPos = 100.0f;  ///< Vertical position (top)
    float hSize = 100.0f; ///< Width
    float vSize = 40.0f;  ///< Height

    Rect() = default;

    /**
     * @brief Constructor with all parameters
     */
    Rect(float hPosition, float vPosition, float width, float height)
        : hPos(hPosition), vPos(vPosition), hSize(width), vSize(height) {}

    /**
     * @brief Sets the position of the Rect
     */
    void SetPosition(float h, float v);

    /**
     * @brief Sets the size of the Rect
     */
    void SetSize(float width, float height);

    /**
     * @brief Returns the right edge position
     */
    float Right() const;

    /**
     * @brief Returns the bottom edge position
     */
    float Bottom() const;

    /**
     * @brief Returns the center x-coordinate
     */
    float CenterX() const;

    /**
     * @brief Returns the center y-coordinate
     */
    float CenterY() const;

    /**
     * @brief Checks if a point is inside the rectangle
     */
    bool Contains(float x, float y) const;

    /**
     * @brief Checks if a point is inside the rectangle
     */
    bool Contains(const Point &point) const;

    /**
     * @brief Checks if this rectangle intersects with another
     */
    bool Intersects(const Rect &other) const;

    /**
     * @brief Calculates the area of intersection with another rectangle
     * @return Area of intersection, 0 if no intersection
     */
    float IntersectionArea(const Rect &other) const;

    /**
     * @brief Expands the rectangle to include the given point
     */
    void ExpandToInclude(float x, float y);

    /**
     * @brief Expands the rectangle to include another rectangle
     */
    void ExpandToInclude(const Rect &other);

    /**
     * @brief Creates a rectangle that is the union of this and another
     */
    Rect Union(const Rect &other) const;

    /**
     * @brief Creates a rectangle that is the intersection of this and another
     * @return Intersection rectangle, or empty rectangle if no intersection
     */
    Rect Intersection(const Rect &other) const;

    /**
     * @brief Insets the rectangle by the specified amounts
     */
    void Inset(float horizontal, float vertical);

    /**
     * @brief Offsets the rectangle by the specified amounts
     */
    void Offset(float horizontal, float vertical);
};

/**
 * @struct Header
 * @brief Represents the header of a behavior script
 */
struct Header : InterfaceElement {
    float vStart = 0.0f;            ///< Vertical offset where execution begins
    float hStartPos = 140.0f;       ///< Horizontal position of start
    float vStartPos = 0.0f;         ///< Vertical position of start
    float vSize = 0.0f;             ///< Vertical size of the start region
    CKDWORD color = 0xC8C8C8;       ///< Color of the header bar
    void *snapshot = nullptr;       ///< Optional bitmap snapshot

    Point GetPosition() const;
    void SetPosition(float h, float v);
};

/**
 * @struct LinkEndpoint
 * @brief Represents an endpoint of a link in the behavior tree
 */
struct LinkEndpoint : InterfaceElement {
    int index = 0;                                    ///< Index of the endpoint within its owner
    EndpointType type = static_cast<EndpointType>(0); ///< Type of the endpoint

    LinkEndpoint() = default;

    LinkEndpoint(CK_ID endpointId, int endpointIndex, EndpointType endpointType)
        : InterfaceElement(endpointId), index(endpointIndex), type(endpointType) {}

    /**
     * @brief Convenience constructor from raw type value
     */
    LinkEndpoint(CK_ID endpointId, int endpointIndex, int endpointType)
        : InterfaceElement(endpointId), index(endpointIndex), type(static_cast<EndpointType>(endpointType)) {}

    /**
     * @brief Checks if this endpoint is a parameter input
     */
    bool IsParameterInput() const;

    /**
     * @brief Checks if this endpoint is a parameter output
     */
    bool IsParameterOutput() const;

    /**
     * @brief Checks if this endpoint is a behavior input
     */
    bool IsBehaviorInput() const;

    /**
     * @brief Checks if this endpoint is a "start" behavior input
     */
    bool IsStartBehaviorInput() const;

    /**
     * @brief Checks if this endpoint is a behavior output
     */
    bool IsBehaviorOutput() const;

    /**
     * @brief Checks if this endpoint is related to parameters
     */
    bool IsParameterRelated() const;

    /**
     * @brief Checks if this endpoint is related to behaviors
     */
    bool IsBehaviorRelated() const;

    /**
     * @brief Checks if this endpoint is compatible with another endpoint for linking
     * @param other The other endpoint to check against
     * @return true if the endpoints can be connected, false otherwise
     */
    bool IsCompatibleWith(const LinkEndpoint &other) const;
};

/**
 * @struct Link
 * @brief Represents a connection between two points in the behavior tree
 */
struct Link : InterfaceElement {
    LinkType type = static_cast<LinkType>(0); ///< Type of the link
    LinkEndpoint start;                       ///< Starting endpoint
    std::vector<Point> points;                ///< Control points for link routing
    LinkEndpoint end;                         ///< Ending endpoint

    /**
     * @brief Default constructor
     */
    Link() = default;

    /**
     * @brief Constructor with all fields
     */
    Link(CK_ID linkId, LinkType linkType, LinkEndpoint startPoint, LinkEndpoint endPoint)
        : InterfaceElement(linkId), type(linkType), start(std::move(startPoint)), end(std::move(endPoint)) {}

    /**
     * @brief Adds a control point to the link
     */
    void AddControlPoint(const Point &point);

    /**
     * @brief Inserts a control point at the specified index
     */
    void InsertControlPoint(int index, const Point &point);

    /**
     * @brief Removes a control point at the specified index
     */
    void RemoveControlPoint(int index);

    /**
     * @brief Updates a control point at the specified index
     */
    void UpdateControlPoint(int index, const Point &point);

    /**
     * @brief Checks if this is a behavior link
     */
    bool IsBehaviorLink() const;

    /**
     * @brief Checks if this is a parameter link
     */
    bool IsParameterLink() const;

    /**
     * @brief Checks if this is a parameter operation link
     */
    bool IsParameterOpLink() const;

    /**
     * @brief Offsets all control points by the given amount
     */
    void Offset(float h, float v);

    /**
     * @brief Scales all control points relative to a center point
     */
    void Scale(float factor, const Point &center);

    /**
     * @brief Calculates the total length of the link path
     */
    float GetPathLength() const;

    /**
     * @brief Gets a point along the path at the specified normalized distance (0-1)
     */
    Point GetPointAlong(float t) const;

    /**
     * @brief Checks if the link passes near a point within the specified distance
     */
    bool PassesNear(const Point &point, float maxDistance) const;
};

/**
 * @struct Operation
 * @brief Represents a parameter operation in the behavior tree
 */
struct Operation : InterfaceElement {
    float hPos = 0.0f; ///< Horizontal position
    float vPos = 0.0f; ///< Vertical position

    Operation() = default;

    explicit Operation(CK_ID opId) : InterfaceElement(opId) {}

    Operation(CK_ID opId, float h, float v) : InterfaceElement(opId), hPos(h), vPos(v) {}

    /**
     * @brief Sets the position of the operation
     */
    void SetPosition(float h, float v);

    /**
     * @brief Gets the position of the operation
     */
    Point GetPosition() const;
};

/**
 * @struct Comment
 * @brief Represents a comment in the behavior tree
 */
struct Comment : InterfaceElement {
    float hPos = 0.0f;      ///< Horizontal position
    float vPos = 0.0f;      ///< Vertical position
    float width = 100.0f;   ///< Width of the comment
    float height = 50.0f;   ///< Height of the comment
    std::string text;       ///< Comment text content
    CKDWORD styleFlags = 0; ///< Style flags

    Comment() = default;

    Comment(CK_ID commentId, float h, float v, float w, float h2, const std::string &commentText)
        : InterfaceElement(commentId), hPos(h), vPos(v), width(w), height(h2), text(commentText) {}

    /**
     * @brief Gets the rectangle that encloses the comment
     */
    Rect GetRect() const;

    /**
     * @brief Sets the rectangle that encloses the comment
     */
    void SetRect(const Rect &rect);

    /**
     * @brief Checks if the comment is collapsed
     */
    bool IsCollapsed() const;

    /**
     * @brief Checks if the comment is locked
     */
    bool IsLocked() const;

    /**
     * @brief Checks if the comment is transparent
     */
    bool IsTransparent() const;

    /**
     * @brief Sets the collapsed state
     */
    void SetCollapsed(bool collapsed);

    /**
     * @brief Sets the locked state
     */
    void SetLocked(bool locked);

    /**
     * @brief Sets the transparent state
     */
    void SetTransparent(bool transparent);

    /**
     * @brief Resizes the comment to fit the text plus padding
     * @param charWidth Average width of a character
     * @param lineHeight Height of a line of text
     * @param hPadding Horizontal padding
     * @param vPadding Vertical padding
     */
    void ResizeToFitText(float charWidth, float lineHeight, float hPadding = 10.0f, float vPadding = 10.0f);
};

/**
 * @struct Parameter
 * @brief Represents a parameter in the behavior tree
 */
struct Parameter : InterfaceElement {
    int hPos = 0;                            ///< Horizontal position
    int vPos = 0;                            ///< Vertical position
    ParameterStyle style = PARAM_STYLE_NAME; ///< Display style
    CK_ID sourceId = 0;                      ///< Source parameter ID for shortcuts

    Parameter() = default;

    Parameter(CK_ID paramId, ParameterStyle paramStyle) : InterfaceElement(paramId), style(paramStyle) {}

    Parameter(CK_ID paramId, int h, int v, ParameterStyle paramStyle = PARAM_STYLE_NAME)
        : InterfaceElement(paramId), hPos(h), vPos(v), style(paramStyle) {}

    /**
     * @brief Checks if this parameter is a shortcut
     */
    bool IsShortcut() const;

    /**
     * @brief Sets the position of the parameter
     */
    void SetPosition(int h, int v);

    /**
     * @brief Gets the position of the parameter
     */
    Point GetPosition() const;

    /**
     * @brief Checks if the style includes a specific flag
     */
    bool HasStyleFlag(ParameterStyle flag) const;

    /**
     * @brief Sets a specific style flag
     */
    void SetStyleFlag(ParameterStyle flag, bool value);

    /**
     * @brief Checks if the parameter shows its name
     */
    bool ShowsName() const;

    /**
     * @brief Checks if the parameter shows its value
     */
    bool ShowsValue() const;

    /**
     * @brief Checks if the parameter is closed/collapsed
     */
    bool IsClosed() const;

    /**
     * @brief Sets whether the parameter is closed/collapsed
     */
    void SetClosed(bool closed);
};

/**
 * @struct ExtraSubData
 * @brief Represents a sub-element in the extra data section
 */
struct ExtraSubData : InterfaceElement {
    int value1 = 0;             ///< First value
    int value2 = 0;             ///< Second value
    CK_ID id1 = 0;              ///< First object ID
    CK_ID id2 = 0;              ///< Second object ID (optional)
    std::vector<CKBYTE> buffer; ///< Buffer data (optional)

    ExtraSubData() = default;

    /**
     * @brief Constructor for ID-based sub-data
     */
    ExtraSubData(int val1, int val2, CK_ID objId1, CK_ID objId2 = 0)
        : value1(val1), value2(val2), id1(objId1), id2(objId2) {}

    /**
     * @brief Constructor for buffer-based sub-data
     */
    ExtraSubData(int val1, int val2, CK_ID objId1, const std::vector<CKBYTE> &data)
        : value1(val1), value2(val2), id1(objId1), buffer(data) {}
};

/**
 * @struct ExtraData
 * @brief Represents extra data for the interface
 */
struct ExtraData : InterfaceElement {
    ExtraDataType type = EXTRA_DATA_VALUE; ///< Type of extra data
    CK_ID id1 = 0;                         ///< First object ID
    CK_ID id2 = 0;                         ///< Second object ID (for connections)
    int value = 0;                         ///< Value (for value type)
    std::vector<ExtraSubData> subData;     ///< Sub-data elements

    ExtraData() = default;

    /**
     * @brief Constructor for behavior reference extra data
     */
    explicit ExtraData(CK_ID behaviorId) : type(EXTRA_DATA_BEHAVIOR), id1(behaviorId) {}

    /**
     * @brief Constructor for parameter reference extra data
     */
    explicit ExtraData(CK_ID parameterId, ExtraDataType extraType = EXTRA_DATA_PARAMETER)
        : type(extraType), id1(parameterId) {}

    /**
     * @brief Constructor for connection extra data
     */
    ExtraData(CK_ID sourceId, CK_ID targetId)
        : type(EXTRA_DATA_CONNECTION), id1(sourceId), id2(targetId) {}

    /**
     * @brief Constructor for value extra data
     */
    explicit ExtraData(int val) : type(EXTRA_DATA_VALUE), value(val) {}

    /**
     * @brief Adds a sub-data element
     */
    void AddSubData(const ExtraSubData &data);
};

/**
 * @class ElementObserver
 * @brief Interface for observers of interface element changes
 */
class ElementObserver {
public:
    virtual ~ElementObserver() = default;

    /**
     * @brief Called when an element is added
     * @param element Pointer to the added element
     */
    virtual void OnElementAdded(InterfaceElement *element) = 0;

    /**
     * @brief Called when an element is removed
     * @param element Pointer to the removed element
     */
    virtual void OnElementRemoved(InterfaceElement *element) = 0;

    /**
     * @brief Called when an element is modified
     * @param element Pointer to the modified element
     */
    virtual void OnElementModified(InterfaceElement *element) = 0;
};

/**
 * @struct BehaviorData
 * @brief Represents a behavior in the tree
 */
struct BehaviorData : InterfaceElement {
    bool folded = false;          ///< Whether the behavior is collapsed
    CKDWORD depth = 0;            ///< Depth in the behavior hierarchy
    Rect rect;                    ///< Size and position of the behavior
    float hExpandSize = 0.0f;     ///< Expanded horizontal size
    float vExpandSize = 0.0f;     ///< Expanded vertical size
    bool isBehaviorGraph = false; ///< Whether this is a behavior graph

    // Links
    std::vector<Link> links; ///< Links within this behavior

    // Operations
    std::vector<Operation> operations; ///< Operations within this behavior

    // Comments
    std::vector<Comment> comments; ///< Comments within this behavior

    // Parameters
    std::vector<Parameter> localParams; ///< Local parameters

    std::vector<Parameter> sharedParams; ///< Shared parameters

    // Input/Output indices for graph behaviors
    std::vector<int> inwardInputs;  ///< Inward-facing inputs
    std::vector<int> outwardInputs; ///< Outward-facing inputs

    std::vector<int> inwardOutputs;  ///< Inward-facing outputs
    std::vector<int> outwardOutputs; ///< Outward-facing outputs

    BehaviorData() = default;

    explicit BehaviorData(CK_ID behaviorId) : InterfaceElement(behaviorId) {}

    /**
     * @brief Adds a link to the behavior
     */
    void AddLink(const Link &link);

    /**
     * @brief Adds an operation to the behavior
     */
    void AddOperation(const Operation &op);

    /**
     * @brief Adds a local parameter to the behavior
     */
    void AddLocalParameter(const Parameter &param);

    /**
     * @brief Adds a shared parameter to the behavior
     */
    void AddSharedParameter(const Parameter &param);

    /**
     * @brief Adds a comment to the behavior
     */
    void AddComment(const Comment &comment);

    /**
     * @brief Finds a link by ID
     * @return Pointer to the link or nullptr if not found
     */
    Link *FindLink(CK_ID linkId);

    /**
     * @brief Finds links connected to a specific object
     * @param objId ID of the object to find connections for
     * @return Vector of pointers to connected links
     */
    std::vector<Link *> FindLinksConnectedTo(CK_ID objId);

    /**
     * @brief Finds an operation by ID
     * @return Pointer to the operation or nullptr if not found
     */
    Operation *FindOperation(CK_ID opId);

    /**
     * @brief Finds a local parameter by ID
     * @return Pointer to the parameter or nullptr if not found
     */
    Parameter *FindLocalParameter(CK_ID paramId);

    /**
     * @brief Finds a shared parameter by ID
     * @return Pointer to the parameter or nullptr if not found
     */
    Parameter *FindSharedParameter(CK_ID paramId);

    /**
     * @brief Finds a parameter (local or shared) by ID
     * @return Pointer to the parameter or nullptr if not found
     */
    Parameter *FindParameter(CK_ID paramId);

    /**
     * @brief Finds a comment by ID
     * @return Pointer to the comment or nullptr if not found
     */
    Comment *FindComment(CK_ID commentId);

    /**
     * @brief Finds elements at the given position
     * @param position The position to check
     * @param tolerance Distance tolerance for considering an element hit
     * @return Vector of pointers to elements at the position
     */
    std::vector<InterfaceElement *> FindElementsAt(const Point &position, float tolerance = 5.0f);

    /**
     * @brief Resets all data in the behavior
     */
    void Reset();

    /**
     * @brief Gets all elements in the block
     * @return Vector of pointers to all elements
     */
    std::vector<InterfaceElement *> GetAllElements();

    /**
     * @brief Removes an element from the block
     * @param element Pointer to the element to remove
     * @return true if element was removed, false otherwise
     */
    bool RemoveElement(InterfaceElement *element);
};

/**
 * @class InterfaceData
 * @brief Main container for behavior tree visual representation
 */
class InterfaceData {
public:
    InterfaceData();
    ~InterfaceData();

    //------------------------------------------------------
    // Core data
    //------------------------------------------------------
    CKDWORD version = 0x16;              ///< Interface chunk version
    Header header;                       ///< Header of the script
    BehaviorData rootBehavior;           ///< Root behavior
    std::vector<BehaviorData> behaviors; ///< Behavior in the tree
    int behaviorCount = 0;               ///< Number of behaviors

    // Extra data section
    int extraDataVersion = 0;            ///< Version of extra data
    std::vector<ExtraData> extraData;    ///< Extra data entries

    // Extension data
    std::vector<ElementObserver *> observers;  ///< Observers for change tracking

    //------------------------------------------------------
    // Basic Operations
    //------------------------------------------------------

    /**
     * @brief Creates a new behavior in the interface
     * @return Reference to the new behavior
     */
    BehaviorData &NewBehavior();

    /**
     * @brief Adds a behavior to the interface
     */
    void AddBehavior(BehaviorData &behavior);

    /**
     * @brief Removes a behavior from the interface
     * @param behaviorId ID of the behavior to remove
     * @return true if block was removed, false otherwise
     */
    bool RemoveBehavior(CK_ID behaviorId);

    /**
     * @brief Finds a behavior by ID
     * @return Pointer to the behavior if found, nullptr otherwise
     */
    BehaviorData *FindBehavior(CK_ID id);

    /**
     * @brief Adds extra data to the interface
     */
    void AddExtraData(const ExtraData &data);

    /**
     * @brief Clears all data in the interface
     */
    void Clear();

    //------------------------------------------------------
    // Observer Pattern
    //------------------------------------------------------

    enum class ElementAction {
        Added,
        Removed,
        Modified
    };

    /**
     * @brief Adds an observer for element changes
     */
    void AddObserver(ElementObserver *observer);

    /**
     * @brief Removes an observer
     */
    void RemoveObserver(ElementObserver *observer);

    /**
     * @brief Notifies all observers of element changes
     */
    void NotifyObservers(InterfaceElement *element, ElementAction action) const;

    //------------------------------------------------------
    // Advanced Querying
    //------------------------------------------------------

    /**
     * @brief Finds all elements with the given ID
     * @param id The ID to search for
     * @return Vector of pointers to found elements
     */
    std::vector<InterfaceElement *> FindElementsById(CK_ID id);

    /**
     * @brief Finds all elements at the given position
     * @param position The position to check
     * @param tolerance Distance tolerance for considering an element hit
     * @return Vector of pointers to elements at the position
     */
    std::vector<InterfaceElement *> FindElementsAt(const Point &position, float tolerance = 5.0f);

    /**
     * @brief Finds all links connected to the given object
     * @param objId ID of the object to find connections for
     * @return Vector of pointers to connected links
     */
    std::vector<Link *> FindLinksConnectedTo(CK_ID objId);

    /**
     * @brief Gets a sub-tree below a behavior
     * @param rootId ID of the root block for the sub-tree
     * @return Vector of pointers to blocks in the sub-tree
     */
    std::vector<BehaviorData *> GetSubTree(CK_ID rootId);

    /**
     * @brief Finds the parent block of a given block
     * @param behaviorId ID of the block to find the parent for
     * @return Pointer to the parent block or nullptr if not found
     */
    BehaviorData *FindParentBehavior(CK_ID behaviorId);

    /**
     * @brief Finds all child blocks of a given block
     * @param behaviorId ID of the block to find children for
     * @return Vector of pointers to child blocks
     */
    std::vector<BehaviorData *> FindChildBehaviors(CK_ID behaviorId);

    /**
     * @brief Checks if there's a path between two blocks
     * @param startId ID of the starting block
     * @param endId ID of the ending block
     * @return true if there's a path, false otherwise
     */
    bool HasPath(CK_ID startId, CK_ID endId);

    /**
     * @brief Finds a path between two blocks
     * @param startId ID of the starting block
     * @param endId ID of the ending block
     * @return Vector of block IDs in the path, empty if no path exists
     */
    std::vector<CK_ID> FindPath(CK_ID startId, CK_ID endId);

    //------------------------------------------------------
    // Serialization Methods
    //------------------------------------------------------

    /**
     * @brief Loads interface data from a state chunk
     * @param behavior The behavior containing the data
     * @param chunk The state chunk to load from
     * @return CK_OK if successful, error code otherwise
     */
    CKERROR LoadFromChunk(CKBehavior *behavior, CKStateChunk *chunk);

    /**
     * @brief Saves interface data to a state chunk
     * @param behavior The behavior to save data for
     * @param chunk The state chunk to save to
     * @return CK_OK if successful, error code otherwise
     */
    CKERROR SaveToChunk(CKBehavior *behavior, CKStateChunk *chunk);

private:
    /**
     * @brief Holds context for serialization operations
     */
    struct SerializationContext {
        CKBehavior *behavior = nullptr;
        CKStateChunk *chunk = nullptr;
        bool isNotScript = false;
        bool isBuildingBlock = false;
        CKDWORD flags = 0;
        CKDWORD version = 0x16;
        CKDWORD scriptIndex = 0;
        CKDWORD behaviorIndex = 0;
    };

    /**
     * @brief Loads a behavior header from a chunk
     * @param context Serialization context
     * @param block The behavior to populate
     * @return TRUE if successful, FALSE otherwise
     */
    CKBOOL LoadBehaviorHeader(SerializationContext &context, BehaviorData &block);

    /**
     * @brief Loads links from a chunk
     * @param context Serialization context
     * @param block The behavior to populate
     */
    void LoadBehaviorLinks(SerializationContext &context, BehaviorData &block);

    /**
     * @brief Loads operations from a chunk
     * @param context Serialization context
     * @param block The behavior to populate
     */
    void LoadBehaviorOperations(SerializationContext &context, BehaviorData &block);

    /**
     * @brief Loads comments from a chunk
     * @param context Serialization context
     * @param block The behavior to populate
     */
    void LoadBehaviorComments(SerializationContext &context, BehaviorData &block);

    /**
     * @brief Loads parameters from a chunk
     * @param context Serialization context
     * @param block The behavior to populate
     */
    void LoadBehaviorParameters(SerializationContext &context, BehaviorData &block);

    /**
     * @brief Loads graph information from a chunk
     * @param context Serialization context
     * @param block The behavior to populate
     */
    void LoadBehaviorGraph(SerializationContext &context, BehaviorData &block);

    /**
     * @brief Loads extra information from a chunk
     * @param context Serialization context
     */
    void LoadExtraData(SerializationContext &context);

    /**
     * @brief Saves a behavior header to a chunk
     * @param context Serialization context
     * @return TRUE if successful, FALSE otherwise
     */
    CKBOOL SaveBehaviorHeader(SerializationContext &context);

    /**
     * @brief Saves links to a chunk
     * @param context Serialization context
     */
    void SaveBehaviorLinks(SerializationContext &context);

    /**
     * @brief Saves operations to a chunk
     * @param context Serialization context
     */
    void SaveBehaviorOperations(SerializationContext &context);

    /**
     * @brief Saves comments to a chunk
     * @param context Serialization context
     */
    void SaveBehaviorComments(SerializationContext &context);

    /**
     * @brief Saves parameters to a chunk
     * @param context Serialization context
     */
    void SaveBehaviorParameters(SerializationContext &context);

    /**
     * @brief Saves graph information to a chunk
     * @param context Serialization context
     */
    void SaveBehaviorGraph(SerializationContext &context);

    /**
     * @brief Saves extra information to a chunk
     * @param context Serialization context
     */
    void SaveExtraData(SerializationContext &context);

    /**
     * @brief Gets the appropriate behavior based on the context
     * @param context Serialization context
     * @return Reference to the behavior
     */
    const BehaviorData &GetBehaviorForContext(const SerializationContext &context) const;

    /**
     * @brief Gets a modifiable reference to the appropriate behavior based on the context
     * @param context Serialization context
     * @return Reference to the behavior
     */
    BehaviorData &GetBehaviorForContext(SerializationContext &context);
};

/**
 * @brief Creates interface data from a behavior's interface chunk
 * @param behavior Behavior with chunk to extract
 * @param data Interface data to populate
 * @return CK_OK if successful
 */
CKERROR ExtractInterfaceData(CKBehavior *behavior, InterfaceData &data);

/**
 * @brief Generates an interface chunk from a behavior
 * @param data Interface data to serialize
 * @param behavior Behavior to generate chunk for
 * @return Generated state chunk
 */
CKStateChunk *GenerateInterfaceChunk(InterfaceData &data, CKBehavior *behavior);
