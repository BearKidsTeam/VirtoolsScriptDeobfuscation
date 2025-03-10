#include "InterfaceData.h"

#include <map>
#include <set>
#include <queue>
#include <algorithm>

// InterfaceElement implementations
template <typename T>
void InterfaceElement::SetMetadata(const std::string &key, const T &value) {
    metadata[key] = MetadataValue(value);
}

template <>
void InterfaceElement::SetMetadata<int>(const std::string &key, const int &value) {
    metadata[key] = MetadataValue(value);
}

template <>
void InterfaceElement::SetMetadata<float>(const std::string &key, const float &value) {
    metadata[key] = MetadataValue(value);
}

template <>
void InterfaceElement::SetMetadata<std::string>(const std::string &key, const std::string &value) {
    metadata[key] = MetadataValue(value);
}

template <>
void InterfaceElement::SetMetadata<bool>(const std::string &key, const bool &value) {
    metadata[key] = MetadataValue(value);
}

template <>
void InterfaceElement::SetMetadata<void *>(const std::string &key, void *const &value) {
    metadata[key] = MetadataValue(value);
}

template <typename T>
bool InterfaceElement::GetMetadata(const std::string &key, T &value) const {
    auto it = metadata.find(key);
    if (it != metadata.end()) {
        const T *pVal = it->second.get<T>();
        if (pVal) {
            value = *pVal;
            return true;
        }
    }
    return false;
}

bool InterfaceElement::HasMetadata(const std::string &key) const {
    return metadata.find(key) != metadata.end();
}

bool InterfaceElement::RemoveMetadata(const std::string &key) {
    return metadata.erase(key) > 0;
}

void InterfaceElement::ClearMetadata() {
    metadata.clear();
}

// Point implementations
float Point::DistanceTo(const Point &other) const {
    float dx = h - other.h;
    float dy = v - other.v;
    return std::sqrt(dx * dx + dy * dy);
}

float Point::DistanceSquaredTo(const Point &other) const {
    float dx = h - other.h;
    float dy = v - other.v;
    return dx * dx + dy * dy;
}

Point Point::operator+(const Point &other) const {
    return {h + other.h, v + other.v};
}

Point Point::operator-(const Point &other) const {
    return {h - other.h, v - other.v};
}

Point Point::operator*(float factor) const {
    return {h * factor, v * factor};
}

Point Point::operator/(float factor) const {
    return {h / factor, v / factor};
}

Point &Point::operator+=(const Point &other) {
    h += other.h;
    v += other.v;
    return *this;
}

Point &Point::operator-=(const Point &other) {
    h -= other.h;
    v -= other.v;
    return *this;
}

Point &Point::operator*=(float factor) {
    h *= factor;
    v *= factor;
    return *this;
}

Point &Point::operator/=(float factor) {
    h /= factor;
    v /= factor;
    return *this;
}

bool Point::operator==(const Point &other) const {
    return h == other.h && v == other.v;
}

bool Point::operator!=(const Point &other) const {
    return !(*this == other);
}

float Point::Dot(const Point &other) const {
    return h * other.h + v * other.v;
}

float Point::Cross(const Point &other) const {
    return h * other.v - v * other.h;
}

float Point::Length() const {
    return std::sqrt(h * h + v * v);
}

float Point::LengthSquared() const {
    return h * h + v * v;
}

Point Point::Normalized() const {
    float len = Length();
    if (len > 0) {
        return {h / len, v / len};
    }
    return *this;
}

Point Point::Rotated(float angle) const {
    float s = std::sin(angle);
    float c = std::cos(angle);
    return {h * c - v * s, h * s + v * c};
}

float Point::AngleTo(const Point &other) const {
    return std::atan2(Cross(other), Dot(other));
}

Point Point::Reflected(const Point &normal) const {
    Point n = normal.Normalized();
    return *this - n * (2.0f * Dot(n));
}

Point Point::Lerp(const Point &other, float t) const {
    return {h + (other.h - h) * t, v + (other.v - v) * t};
}

bool Point::ApproximatelyEqual(const Point &other, float epsilon) const {
    return std::abs(h - other.h) < epsilon && std::abs(v - other.v) < epsilon;
}

// Rect implementations
void Rect::SetPosition(float h, float v) {
    hPos = h;
    vPos = v;
}

void Rect::SetSize(float width, float height) {
    hSize = width;
    vSize = height;
}

float Rect::Right() const {
    return hPos + hSize;
}

float Rect::Bottom() const {
    return vPos + vSize;
}

float Rect::CenterX() const {
    return hPos + hSize / 2.0f;
}

float Rect::CenterY() const {
    return vPos + vSize / 2.0f;
}

bool Rect::Contains(float x, float y) const {
    return x >= hPos && x <= Right() && y >= vPos && y <= Bottom();
}

bool Rect::Contains(const Point &point) const {
    return Contains(point.h, point.v);
}

bool Rect::Intersects(const Rect &other) const {
    return !(other.hPos > Right() || other.Right() < hPos ||
        other.vPos > Bottom() || other.Bottom() < vPos);
}

float Rect::IntersectionArea(const Rect &other) const {
    if (!Intersects(other)) return 0.0f;

    float xOverlap = std::min(Right(), other.Right()) - std::max(hPos, other.hPos);
    float yOverlap = std::min(Bottom(), other.Bottom()) - std::max(vPos, other.vPos);

    return xOverlap * yOverlap;
}

void Rect::ExpandToInclude(float x, float y) {
    float newRight = std::max(Right(), x);
    float newBottom = std::max(Bottom(), y);

    hPos = std::min(hPos, x);
    vPos = std::min(vPos, y);

    hSize = newRight - hPos;
    vSize = newBottom - vPos;
}

void Rect::ExpandToInclude(const Rect &other) {
    float newRight = std::max(Right(), other.Right());
    float newBottom = std::max(Bottom(), other.Bottom());

    hPos = std::min(hPos, other.hPos);
    vPos = std::min(vPos, other.vPos);

    hSize = newRight - hPos;
    vSize = newBottom - vPos;
}

Rect Rect::Union(const Rect &other) const {
    Rect result = *this;
    result.ExpandToInclude(other);
    return result;
}

Rect Rect::Intersection(const Rect &other) const {
    if (!Intersects(other)) return {0, 0, 0, 0};

    float left = std::max(hPos, other.hPos);
    float top = std::max(vPos, other.vPos);
    float right = std::min(Right(), other.Right());
    float bottom = std::min(Bottom(), other.Bottom());

    return {left, top, right - left, bottom - top};
}

void Rect::Inset(float horizontal, float vertical) {
    hPos += horizontal;
    vPos += vertical;
    hSize -= horizontal * 2;
    vSize -= vertical * 2;

    // Ensure dimensions don't go negative
    if (hSize < 0) hSize = 0;
    if (vSize < 0) vSize = 0;
}

void Rect::Offset(float horizontal, float vertical) {
    hPos += horizontal;
    vPos += vertical;
}

// StartPoint implementations
Point StartPoint::GetPosition() const {
    return {hStartPos, vStartPos};
}

void StartPoint::SetPosition(float h, float v) {
    hStartPos = h;
    vStartPos = v;
}

// LinkEndpoint implementations
bool LinkEndpoint::IsParameterInput() const {
    return type == ENDPOINT_PIN || type == ENDPOINT_TARGET_PIN;
}

bool LinkEndpoint::IsParameterOutput() const {
    return type == ENDPOINT_POUT || type == ENDPOINT_POUT_SHORTCUT;
}

bool LinkEndpoint::IsBehaviorInput() const {
    return type == ENDPOINT_BIN || type == ENDPOINT_START_BIN;
}

bool LinkEndpoint::IsBehaviorOutput() const {
    return type == ENDPOINT_BOUT;
}

bool LinkEndpoint::IsParameterRelated() const {
    return IsParameterInput() || IsParameterOutput() || type == ENDPOINT_PLOCAL;
}

bool LinkEndpoint::IsBehaviorRelated() const {
    return IsBehaviorInput() || IsBehaviorOutput();
}

bool LinkEndpoint::IsCompatibleWith(const LinkEndpoint &other) const {
    // Parameter outputs connect to parameter inputs
    if (IsParameterOutput() && other.IsParameterInput()) return true;

    // Behavior outputs connect to behavior inputs
    if (IsBehaviorOutput() && other.IsBehaviorInput()) return true;

    return false;
}

// Link implementations
void Link::AddControlPoint(const Point &point) {
    points.push_back(point);
    pointCount = static_cast<int>(points.size());
}

void Link::InsertControlPoint(int index, const Point &point) {
    if (index >= 0 && index <= pointCount) {
        points.insert(points.begin() + index, point);
        pointCount = static_cast<int>(points.size());
    }
}

void Link::RemoveControlPoint(int index) {
    if (index >= 0 && index < pointCount) {
        points.erase(points.begin() + index);
        pointCount = static_cast<int>(points.size());
    }
}

void Link::UpdateControlPoint(int index, const Point &point) {
    if (index >= 0 && index < pointCount) {
        points[index] = point;
    }
}

bool Link::IsBehaviorLink() const {
    return type == LINK_TYPE_BEHAVIOR;
}

bool Link::IsParameterLink() const {
    return type == LINK_TYPE_PARAMETER;
}

bool Link::IsParameterOpLink() const {
    return type == LINK_TYPE_PARAMETER_OP;
}

void Link::Offset(float h, float v) {
    for (auto &point : points) {
        point.h += h;
        point.v += v;
    }
}

void Link::Scale(float factor, const Point &center) {
    for (auto &point : points) {
        // Translate to origin, scale, and translate back
        point.h = center.h + (point.h - center.h) * factor;
        point.v = center.v + (point.v - center.v) * factor;
    }
}

float Link::GetPathLength() const {
    if (points.size() < 2) return 0.0f;

    float length = 0.0f;
    for (size_t i = 1; i < points.size(); ++i) {
        length += points[i].DistanceTo(points[i - 1]);
    }

    return length;
}

Point Link::GetPointAlong(float t) const {
    if (points.empty()) return {};
    if (points.size() == 1 || t <= 0.0f) return points.front();
    if (t >= 1.0f) return points.back();

    float totalLength = GetPathLength();
    float targetDistance = totalLength * t;
    float currentDistance = 0.0f;

    for (size_t i = 1; i < points.size(); ++i) {
        float segmentLength = points[i].DistanceTo(points[i - 1]);

        if (currentDistance + segmentLength >= targetDistance) {
            float segmentT = (targetDistance - currentDistance) / segmentLength;
            return points[i - 1].Lerp(points[i], segmentT);
        }

        currentDistance += segmentLength;
    }

    // Fallback
    return points.back();
}

bool Link::PassesNear(const Point &point, float maxDistance) const {
    if (points.size() < 2) return false;

    for (size_t i = 1; i < points.size(); ++i) {
        const Point &p1 = points[i - 1];
        const Point &p2 = points[i];

        // Line segment-point distance calculation
        float lineLength = p1.DistanceTo(p2);
        if (lineLength < 0.0001f) {
            // Points are practically the same, check distance to either
            if (point.DistanceTo(p1) <= maxDistance)
                return true;
            continue;
        }

        // Calculate projection
        float t = ((point.h - p1.h) * (p2.h - p1.h) + (point.v - p1.v) * (p2.v - p1.v)) / (lineLength * lineLength);

        if (t < 0.0f) {
            // Closest point is before the segment start
            if (point.DistanceTo(p1) <= maxDistance)
                return true;
        } else if (t > 1.0f) {
            // Closest point is after the segment end
            if (point.DistanceTo(p2) <= maxDistance)
                return true;
        } else {
            // Closest point is on the segment
            Point projection(p1.h + t * (p2.h - p1.h), p1.v + t * (p2.v - p1.v));
            if (point.DistanceTo(projection) <= maxDistance)
                return true;
        }
    }

    return false;
}

// Operation implementations
void Operation::SetPosition(float h, float v) {
    hPos = h;
    vPos = v;
}

Point Operation::GetPosition() const {
    return {hPos, vPos};
}

// Comment implementations
Rect Comment::GetRect() const {
    return {hPos, vPos, width, height};
}

void Comment::SetRect(const Rect &rect) {
    hPos = rect.hPos;
    vPos = rect.vPos;
    width = rect.hSize;
    height = rect.vSize;
}

bool Comment::IsCollapsed() const {
    return (styleFlags & COMMENT_STYLE_COLLAPSED) != 0;
}

bool Comment::IsLocked() const {
    return (styleFlags & COMMENT_STYLE_LOCKED) != 0;
}

bool Comment::IsTransparent() const {
    return (styleFlags & COMMENT_STYLE_TRANSPARENT) != 0;
}

void Comment::SetCollapsed(bool collapsed) {
    if (collapsed)
        styleFlags |= COMMENT_STYLE_COLLAPSED;
    else
        styleFlags &= ~COMMENT_STYLE_COLLAPSED;
}

void Comment::SetLocked(bool locked) {
    if (locked)
        styleFlags |= COMMENT_STYLE_LOCKED;
    else
        styleFlags &= ~COMMENT_STYLE_LOCKED;
}

void Comment::SetTransparent(bool transparent) {
    if (transparent)
        styleFlags |= COMMENT_STYLE_TRANSPARENT;
    else
        styleFlags &= ~COMMENT_STYLE_TRANSPARENT;
}

void Comment::ResizeToFitText(float charWidth, float lineHeight, float hPadding, float vPadding) {
    // Split text into lines
    std::vector<std::string> lines;
    std::string currentLine;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(currentLine);
            currentLine.clear();
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.empty()) {
        lines.push_back(currentLine);
    }

    // Calculate required width and height
    float maxLineWidth = 0.0f;
    for (const auto &line : lines) {
        maxLineWidth = std::max(maxLineWidth, line.length() * charWidth);
    }

    width = maxLineWidth + hPadding * 2;
    height = lines.size() * lineHeight + vPadding * 2;
}

// Parameter implementations
bool Parameter::IsShortcut() const {
    return sourceId != static_cast<CK_ID>(-1);
}

void Parameter::SetPosition(int h, int v) {
    hPos = h;
    vPos = v;
}

Point Parameter::GetPosition() const {
    return {static_cast<float>(hPos), static_cast<float>(vPos)};
}

bool Parameter::HasStyleFlag(ParameterStyle flag) const {
    return (style & flag) != 0;
}

void Parameter::SetStyleFlag(ParameterStyle flag, bool value) {
    if (value)
        style = static_cast<ParameterStyle>(style | flag);
    else
        style = static_cast<ParameterStyle>(style & ~flag);
}

bool Parameter::ShowsName() const {
    return HasStyleFlag(PARAM_STYLE_NAME) || HasStyleFlag(PARAM_STYLE_NAMEVALUE);
}

bool Parameter::ShowsValue() const {
    return HasStyleFlag(PARAM_STYLE_VALUE) || HasStyleFlag(PARAM_STYLE_NAMEVALUE);
}

bool Parameter::IsClosed() const {
    return HasStyleFlag(PARAM_STYLE_CLOSED);
}

void Parameter::SetClosed(bool closed) {
    SetStyleFlag(PARAM_STYLE_CLOSED, closed);
}

// ExtraData implementations
void ExtraData::AddSubData(const ExtraSubData &data) {
    subData.push_back(data);
}

// Behavior implementations
void BehaviorData::AddLink(const Link &link) {
    links.push_back(link);
    linkCount = static_cast<int>(links.size());
}

void BehaviorData::AddOperation(const Operation &op) {
    operations.push_back(op);
    operationCount = static_cast<int>(operations.size());
}

void BehaviorData::AddLocalParameter(const Parameter &param) {
    localParams.push_back(param);
    localParamCount = static_cast<int>(localParams.size());
}

void BehaviorData::AddSharedParameter(const Parameter &param) {
    sharedParams.push_back(param);
    sharedParamCount = static_cast<int>(sharedParams.size());
}

void BehaviorData::AddComment(const Comment &comment) {
    comments.push_back(comment);
    commentCount = static_cast<int>(comments.size());
}

Link *BehaviorData::FindLink(CK_ID linkId) {
    const auto it = std::find_if(links.begin(), links.end(),
                           [linkId](const Link &link) { return link.id == linkId; });
    return it != links.end() ? &(*it) : nullptr;
}

std::vector<Link *> BehaviorData::FindLinksConnectedTo(CK_ID objId) {
    std::vector<Link *> connectedLinks;

    for (auto &link : links) {
        if (link.start.id == objId || link.end.id == objId) {
            connectedLinks.push_back(&link);
        }
    }

    return connectedLinks;
}

Operation *BehaviorData::FindOperation(CK_ID opId) {
    const auto it = std::find_if(operations.begin(), operations.end(),
                           [opId](const Operation &op) { return op.id == opId; });
    return it != operations.end() ? &(*it) : nullptr;
}

Parameter *BehaviorData::FindLocalParameter(CK_ID paramId) {
    const auto it = std::find_if(localParams.begin(), localParams.end(),
                           [paramId](const Parameter &param) { return param.id == paramId; });
    return it != localParams.end() ? &(*it) : nullptr;
}

Parameter *BehaviorData::FindSharedParameter(CK_ID paramId) {
    const auto it = std::find_if(sharedParams.begin(), sharedParams.end(),
                           [paramId](const Parameter &param) { return param.id == paramId; });
    return it != sharedParams.end() ? &(*it) : nullptr;
}

Parameter *BehaviorData::FindParameter(CK_ID paramId) {
    Parameter *param = FindLocalParameter(paramId);
    if (param) return param;

    return FindSharedParameter(paramId);
}

Comment *BehaviorData::FindComment(CK_ID commentId) {
    const auto it = std::find_if(comments.begin(), comments.end(),
                           [commentId](const Comment &comment) { return comment.id == commentId; });
    return it != comments.end() ? &(*it) : nullptr;
}

std::vector<InterfaceElement *> BehaviorData::FindElementsAt(const Point &position, float tolerance) {
    std::vector<InterfaceElement *> elements;

    // Check if position is within the behavior itself
    if (size.Contains(position)) {
        elements.push_back(this);
    }

    // Check operations
    for (auto &op : operations) {
        if (Point(op.hPos, op.vPos).DistanceTo(position) <= tolerance) {
            elements.push_back(&op);
        }
    }

    // Check parameters
    for (auto &param : localParams) {
        if (Point(static_cast<float>(param.hPos), static_cast<float>(param.vPos)).DistanceTo(position) <= tolerance) {
            elements.push_back(&param);
        }
    }

    for (auto &param : sharedParams) {
        if (Point(static_cast<float>(param.hPos), static_cast<float>(param.vPos)).DistanceTo(position) <= tolerance) {
            elements.push_back(&param);
        }
    }

    // Check comments
    for (auto &comment : comments) {
        Rect commentRect(comment.hPos, comment.vPos, comment.width, comment.height);
        if (commentRect.Contains(position)) {
            elements.push_back(&comment);
        }
    }

    // Check links
    for (auto &link : links) {
        if (link.PassesNear(position, tolerance)) {
            elements.push_back(&link);
        }
    }

    return elements;
}

void BehaviorData::Reset() {
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

    ClearMetadata();
}

std::vector<InterfaceElement *> BehaviorData::GetAllElements() {
    std::vector<InterfaceElement *> elements;

    elements.push_back(this);

    for (auto &link : links) {
        elements.push_back(&link);
    }

    for (auto &op : operations) {
        elements.push_back(&op);
    }

    for (auto &comment : comments) {
        elements.push_back(&comment);
    }

    for (auto &param : localParams) {
        elements.push_back(&param);
    }

    for (auto &param : sharedParams) {
        elements.push_back(&param);
    }

    return elements;
}

bool BehaviorData::RemoveElement(InterfaceElement *element) {
    if (!element) return false;

    // Check element type and remove from appropriate container
    if (Link *link = dynamic_cast<Link *>(element)) {
        auto it = std::find_if(links.begin(), links.end(),
                               [link](const Link &l) { return l.id == link->id; });
        if (it != links.end()) {
            links.erase(it);
            linkCount = static_cast<int>(links.size());
            return true;
        }
    } else if (Operation *op = dynamic_cast<Operation *>(element)) {
        auto it = std::find_if(operations.begin(), operations.end(),
                               [op](const Operation &o) { return o.id == op->id; });
        if (it != operations.end()) {
            operations.erase(it);
            operationCount = static_cast<int>(operations.size());
            return true;
        }
    } else if (Comment *comment = dynamic_cast<Comment *>(element)) {
        auto it = std::find_if(comments.begin(), comments.end(),
                               [comment](const Comment &c) { return c.id == comment->id; });
        if (it != comments.end()) {
            comments.erase(it);
            commentCount = static_cast<int>(comments.size());
            return true;
        }
    } else if (Parameter *param = dynamic_cast<Parameter *>(element)) {
        // Check local parameters
        auto it = std::find_if(localParams.begin(), localParams.end(),
                               [param](const Parameter &p) { return p.id == param->id; });
        if (it != localParams.end()) {
            localParams.erase(it);
            localParamCount = static_cast<int>(localParams.size());
            return true;
        }

        // Check shared parameters
        it = std::find_if(sharedParams.begin(), sharedParams.end(),
                          [param](const Parameter &p) { return p.id == param->id; });
        if (it != sharedParams.end()) {
            sharedParams.erase(it);
            sharedParamCount = static_cast<int>(sharedParams.size());
            return true;
        }
    }

    return false;
}

// InterfaceData implementations
InterfaceData::InterfaceData() {
    // Initialize any required data
    Clear();
}

InterfaceData::~InterfaceData() {
    // Clean up resources
    Clear();
}

void InterfaceData::AddBehavior(BehaviorData &behavior) {
    behaviors.push_back(behavior);
    behaviorCount = static_cast<int>(behaviors.size());

    NotifyObservers(&behavior, ElementAction::Added);
}

bool InterfaceData::RemoveBehavior(CK_ID behaviorId) {
    auto it = std::find_if(behaviors.begin(), behaviors.end(),
                           [behaviorId](const BehaviorData &behavior) { return behavior.id == behaviorId; });

    if (it != behaviors.end()) {
        NotifyObservers(&(*it), ElementAction::Removed);
        behaviors.erase(it);
        behaviorCount = static_cast<int>(behaviors.size());
        return true;
    }

    return false;
}

BehaviorData *InterfaceData::FindBehavior(CK_ID id) {
    if (scriptRoot.id == id) {
        return &scriptRoot;
    }

    const auto it = std::find_if(behaviors.begin(), behaviors.end(),
                           [id](const BehaviorData &behavior) { return behavior.id == id; });
    return it != behaviors.end() ? &(*it) : nullptr;
}

void InterfaceData::AddExtraData(const ExtraData &data) {
    extraData.push_back(data);
}

void InterfaceData::Clear() {
    version = 0x16;
    start = StartPoint();
    scriptRoot.Reset();
    behaviors.clear();
    behaviorCount = 0;
    extraDataVersion = 0;
    extraData.clear();
    userData.clear();
}

void InterfaceData::AddObserver(ElementObserver *observer) {
    if (observer && std::find(observers.begin(), observers.end(), observer) == observers.end()) {
        observers.push_back(observer);
    }
}

void InterfaceData::RemoveObserver(ElementObserver *observer) {
    const auto it = std::find(observers.begin(), observers.end(), observer);
    if (it != observers.end()) {
        observers.erase(it);
    }
}

void InterfaceData::NotifyObservers(InterfaceElement *element, ElementAction action) {
    for (auto *observer : observers) {
        switch (action) {
        case ElementAction::Added:
            observer->OnElementAdded(element);
            break;
        case ElementAction::Removed:
            observer->OnElementRemoved(element);
            break;
        case ElementAction::Modified:
            observer->OnElementModified(element);
            break;
        }
    }
}

std::vector<InterfaceElement *> InterfaceData::FindElementsById(CK_ID id) {
    std::vector<InterfaceElement *> result;

    // Check script root
    if (scriptRoot.id == id) {
        result.push_back(&scriptRoot);
    }

    // Check start point
    if (start.id == id) {
        result.push_back(&start);
    }

    // Check all behaviors
    for (auto &behavior : behaviors) {
        if (behavior.id == id) {
            result.push_back(&behavior);
        }

        // Check elements within behavior
        for (auto &link : behavior.links) {
            if (link.id == id) {
                result.push_back(&link);
            }
        }

        for (auto &op : behavior.operations) {
            if (op.id == id) {
                result.push_back(&op);
            }
        }

        for (auto &comment : behavior.comments) {
            if (comment.id == id) {
                result.push_back(&comment);
            }
        }

        for (auto &param : behavior.localParams) {
            if (param.id == id) {
                result.push_back(&param);
            }
        }

        for (auto &param : behavior.sharedParams) {
            if (param.id == id) {
                result.push_back(&param);
            }
        }
    }

    return result;
}

std::vector<InterfaceElement *> InterfaceData::FindElementsAt(const Point &position, float tolerance) {
    std::vector<InterfaceElement *> elements;

    // Check all behaviors
    for (auto &behavior : behaviors) {
        auto behaviorElements = behavior.FindElementsAt(position, tolerance);
        elements.insert(elements.end(), behaviorElements.begin(), behaviorElements.end());
    }

    // Check script root
    auto rootElements = scriptRoot.FindElementsAt(position, tolerance);
    elements.insert(elements.end(), rootElements.begin(), rootElements.end());

    return elements;
}

std::vector<Link *> InterfaceData::FindLinksConnectedTo(CK_ID objId) {
    std::vector<Link *> connectedLinks;

    // Check script root links
    for (auto &link : scriptRoot.links) {
        if (link.start.id == objId || link.end.id == objId) {
            connectedLinks.push_back(&link);
        }
    }

    // Check all behavior links
    for (auto &behavior : behaviors) {
        auto behaviorLinks = behavior.FindLinksConnectedTo(objId);
        connectedLinks.insert(connectedLinks.end(), behaviorLinks.begin(), behaviorLinks.end());
    }

    return connectedLinks;
}

std::vector<BehaviorData *> InterfaceData::GetSubTree(CK_ID rootId) {
    std::vector<BehaviorData *> subTree;

    // Find the root behavior
    BehaviorData *root = FindBehavior(rootId);
    if (!root) return subTree;

    // Add the root
    subTree.push_back(root);

    // Simple breadth-first search to find connected behaviors
    std::set<CK_ID> visited;
    std::queue<CK_ID> queue;

    visited.insert(rootId);
    queue.push(rootId);

    while (!queue.empty()) {
        CK_ID currentId = queue.front();
        queue.pop();

        // Find all links from this behavior
        auto links = FindLinksConnectedTo(currentId);

        for (auto *link : links) {
            // If the link ends in a behavior we haven't visited
            if (link->end.IsBehaviorRelated() && visited.find(link->end.id) == visited.end()) {
                BehaviorData *connectedBehavior = FindBehavior(link->end.id);
                if (connectedBehavior) {
                    subTree.push_back(connectedBehavior);
                    visited.insert(link->end.id);
                    queue.push(link->end.id);
                }
            }
        }
    }

    return subTree;
}

BehaviorData *InterfaceData::FindParentBehavior(CK_ID behaviorId) {
    // A behavior's parent is connected to it via a behavior link
    auto links = FindLinksConnectedTo(behaviorId);

    for (auto *link : links) {
        if (link->IsBehaviorLink() && link->end.id == behaviorId) {
            return FindBehavior(link->start.id);
        }
    }

    return nullptr;
}

std::vector<BehaviorData *> InterfaceData::FindChildBehaviors(CK_ID behaviorId) {
    std::vector<BehaviorData *> children;

    // Children are connected via behavior links from this behavior
    auto links = FindLinksConnectedTo(behaviorId);

    for (auto *link : links) {
        if (link->IsBehaviorLink() && link->start.id == behaviorId) {
            BehaviorData *child = FindBehavior(link->end.id);
            if (child) {
                children.push_back(child);
            }
        }
    }

    return children;
}

bool InterfaceData::HasPath(CK_ID startId, CK_ID endId) {
    // Breadth-first search to find a path
    std::set<CK_ID> visited;
    std::queue<CK_ID> queue;

    visited.insert(startId);
    queue.push(startId);

    while (!queue.empty()) {
        CK_ID currentId = queue.front();
        queue.pop();

        if (currentId == endId) {
            return true;
        }

        // Find all links from this behavior
        auto links = FindLinksConnectedTo(currentId);

        for (auto *link : links) {
            // Only follow behavior links
            if (link->IsBehaviorLink() && link->start.id == currentId) {
                CK_ID nextId = link->end.id;

                if (visited.find(nextId) == visited.end()) {
                    visited.insert(nextId);
                    queue.push(nextId);
                }
            }
        }
    }

    return false;
}

std::vector<CK_ID> InterfaceData::FindPath(CK_ID startId, CK_ID endId) {
    // Use breadth-first search to find the shortest path
    std::map<CK_ID, CK_ID> cameFrom;
    std::set<CK_ID> visited;
    std::queue<CK_ID> queue;

    visited.insert(startId);
    queue.push(startId);

    bool found = false;

    while (!queue.empty() && !found) {
        CK_ID currentId = queue.front();
        queue.pop();

        if (currentId == endId) {
            found = true;
            break;
        }

        // Find all links from this behavior
        auto links = FindLinksConnectedTo(currentId);

        for (auto *link : links) {
            // Only follow behavior links
            if (link->IsBehaviorLink() && link->start.id == currentId) {
                CK_ID nextId = link->end.id;

                if (visited.find(nextId) == visited.end()) {
                    visited.insert(nextId);
                    cameFrom[nextId] = currentId;
                    queue.push(nextId);
                }
            }
        }
    }

    // If no path was found, return empty vector
    if (!found) {
        return {};
    }

    // Reconstruct the path
    std::vector<CK_ID> path;
    CK_ID current = endId;

    while (current != startId) {
        path.push_back(current);
        current = cameFrom[current];
    }

    path.push_back(startId);
    std::reverse(path.begin(), path.end());

    return path;
}

const BehaviorData &InterfaceData::GetBehaviorForContext(const SerializationContext &context) const {
    return !context.isNotScript ? scriptRoot : behaviors[context.behaviorIndex];
}

BehaviorData &InterfaceData::GetBehaviorForContext(SerializationContext &context) {
    return !context.isNotScript ? scriptRoot : behaviors[context.behaviorIndex];
}

CKBOOL InterfaceData::LoadBehaviorHeader(SerializationContext &context, BehaviorData &behavior) {
    CKContext *ckContext = context.behavior->GetCKContext();
    CKStateChunk *chunk = context.chunk;

    // Read behavior ID and flags
    CK_ID behID = chunk->ReadObjectID();
    CKDWORD flag = chunk->ReadDword();
    CKDWORD index = chunk->ReadDword();

    // Get the behavior object
    CKBehavior *beh = (CKBehavior *) ckContext->GetObject(behID);
    if (!beh || !CKIsChildClassOf(beh, CKCID_BEHAVIOR)) {
        ckContext->OutputToConsoleEx((CKSTRING) "Error: Behavior <%s> not found", behID);
        return FALSE;
    }

    // Store data in the context for future use
    context.flags = flag;
    context.behavior = beh;
    context.isBuildingBlock = (beh->GetFlags() & CKBEHAVIOR_BUILDINGBLOCK) != 0;

    // Populate the behavior
    behavior.id = behID;
    behavior.folded = (flag & 0x200) != 0;

    // Read position
    const float x = chunk->ReadFloat();
    const float y = chunk->ReadFloat();
    behavior.size.hPos = x;
    behavior.size.vPos = y;

    if (!context.isNotScript) {
        // Script-specific data
        context.scriptIndex = index;

        // Read start position
        const float startX = chunk->ReadFloat();
        const float startY = chunk->ReadFloat();
        start.hStartPos = startX;
        start.vStartPos = startY;

        // Read height
        const float height = chunk->ReadFloat();
        start.vSize = height;

        // Skip bitmap but store if needed
        BITMAP_HANDLE snapshot = chunk->ReadBitmap();
        if (snapshot) {
            start.snapshot = snapshot; // TODO: Would need proper handling
        }

        // Read header color if version >= 0x14
        if (context.version >= 0x14) {
            start.headerColor = chunk->ReadDword();
        }
    } else {
        // behavior specific data
        behavior.depth = index;

        // Read size
        const float width = chunk->ReadFloat();
        const float height = chunk->ReadFloat();
        behavior.size.hSize = width;
        behavior.size.vSize = height;

        // Read expanded size
        const float expandWidth = chunk->ReadFloat();
        const float expandHeight = chunk->ReadFloat();
        behavior.hExpandSize = expandWidth;
        behavior.vExpandSize = expandHeight;
    }

    return TRUE;
}

void InterfaceData::LoadBehaviorLinks(SerializationContext &context, BehaviorData &behavior) {
    CKStateChunk *chunk = context.chunk;

    // Read link count
    const int linkCount = chunk->ReadInt();
    behavior.links.clear();
    behavior.links.reserve(linkCount);
    behavior.linkCount = linkCount;

    // Read each link
    for (int i = 0; i < linkCount; ++i) {
        Link link;

        // Read link type and ID
        link.type = static_cast<LinkType>(chunk->ReadDword());
        link.id = chunk->ReadObjectID();

        // Read start endpoint
        link.start.id = chunk->ReadObjectID();
        link.start.index = chunk->ReadInt();
        link.start.type = static_cast<EndpointType>(chunk->ReadDword());

        // Read control points
        const int pointCount = chunk->ReadInt();
        link.pointCount = pointCount;
        link.points.resize(pointCount);

        for (int j = 0; j < pointCount; ++j) {
            link.points[j].h = chunk->ReadFloat();
            link.points[j].v = chunk->ReadFloat();
        }

        // Read end endpoint
        link.end.id = chunk->ReadObjectID();
        link.end.index = chunk->ReadInt();
        link.end.type = static_cast<EndpointType>(chunk->ReadDword());

        // Add the link to the behavior
        behavior.links.push_back(link);
    }
}

void InterfaceData::LoadBehaviorOperations(SerializationContext &context, BehaviorData &behavior) {
    CKStateChunk *chunk = context.chunk;

    // Read operation count
    const int opCount = chunk->ReadInt();
    behavior.operations.clear();
    behavior.operations.reserve(opCount);
    behavior.operationCount = opCount;

    // Read each operation
    for (int i = 0; i < opCount; ++i) {
        Operation op;

        // Read operation ID and position
        op.id = chunk->ReadObjectID();
        op.hPos = chunk->ReadFloat();
        op.vPos = chunk->ReadFloat();

        // Add the operation to the behavior
        behavior.operations.push_back(op);
    }
}

void InterfaceData::LoadBehaviorComments(SerializationContext &context, BehaviorData &behavior) {
    CKStateChunk *chunk = context.chunk;

    // Read comment count
    const int commentCount = chunk->ReadInt();
    behavior.comments.clear();
    behavior.comments.reserve(commentCount);
    behavior.commentCount = commentCount;

    // Read each comment
    for (int i = 0; i < commentCount; ++i) {
        Comment comment;

        // Read comment rectangle
        const float commentLeft = chunk->ReadFloat();
        const float commentTop = chunk->ReadFloat();
        const float commentRight = chunk->ReadFloat();
        const float commentBottom = chunk->ReadFloat();

        comment.hPos = commentLeft;
        comment.vPos = commentTop;
        comment.width = commentRight - commentLeft;
        comment.height = commentBottom - commentTop;

        // Read comment text
        CKSTRING commentText = nullptr;
        chunk->ReadString(&commentText);
        comment.text = commentText ? commentText : "";
        CKDeletePointer(commentText);

        // Read comment style if version >= 0x16
        if (context.version >= 0x16) {
            comment.styleFlags = chunk->ReadDword();
        }

        // Add the comment to the behavior
        behavior.comments.push_back(comment);
    }
}

void InterfaceData::LoadBehaviorParameters(SerializationContext &context, BehaviorData &behavior) {
    CKStateChunk *chunk = context.chunk;

    // Read local parameter count
    const int localParamCount = chunk->ReadInt();
    behavior.localParams.clear();
    behavior.localParams.reserve(localParamCount);
    behavior.localParamCount = localParamCount;

    // Read local parameter positions
    for (int i = 0; i < localParamCount; ++i) {
        Parameter param;
        param.hPos = chunk->ReadInt();
        param.vPos = chunk->ReadInt();
        behavior.localParams.push_back(param);
    }

    // Read local parameter styles
    for (int i = 0; i < localParamCount; ++i) {
        behavior.localParams[i].style = static_cast<ParameterStyle>(chunk->ReadInt());
    }

    // Read shared parameter count
    const int paramShortcutCount = chunk->ReadInt();
    behavior.sharedParams.clear();
    behavior.sharedParams.reserve(paramShortcutCount);
    behavior.sharedParamCount = paramShortcutCount;

    // Read shared parameter positions
    for (int i = 0; i < paramShortcutCount; ++i) {
        Parameter param;
        param.hPos = chunk->ReadInt();
        param.vPos = chunk->ReadInt();
        behavior.sharedParams.push_back(param);
    }

    // Read shared parameter styles
    for (int i = 0; i < paramShortcutCount; ++i) {
        behavior.sharedParams[i].style = static_cast<ParameterStyle>(chunk->ReadInt());
    }

    // Read shared parameter sources
    for (int i = 0; i < paramShortcutCount; ++i) {
        CK_ID paramShortcutSourceID;
        if (context.version >= 0x15) {
            paramShortcutSourceID = chunk->ReadObjectID();
        } else {
            chunk->ReadObjectID();
            paramShortcutSourceID = chunk->ReadObjectID();
            chunk->ReadInt();
        }
        behavior.sharedParams[i].sourceId = paramShortcutSourceID;
    }
}

void InterfaceData::LoadBehaviorGraph(SerializationContext &context, BehaviorData &behavior) {
    CKStateChunk *chunk = context.chunk;

    // Mark as a behavior graph
    behavior.isBehaviorGraph = true;

    // Read inward inputs
    const int inwardInputCount = chunk->ReadInt();
    behavior.inwardInputs.clear();
    behavior.inwardInputs.reserve(inwardInputCount);

    for (int i = 0; i < inwardInputCount; ++i) {
        int inputValue = chunk->ReadInt();
        behavior.inwardInputs.push_back(inputValue);
        chunk->ReadInt(); // Skip extra value
    }

    // Read outward inputs
    const int outwardInputCount = chunk->ReadInt();
    behavior.outwardInputs.clear();
    behavior.outwardInputs.reserve(outwardInputCount);

    for (int i = 0; i < outwardInputCount; ++i) {
        int inputValue = chunk->ReadInt();
        behavior.outwardInputs.push_back(inputValue);
        chunk->ReadInt(); // Skip extra value
    }

    // Read inward outputs
    const int inwardOutputCount = chunk->ReadInt();
    behavior.inwardOutputs.clear();
    behavior.inwardOutputs.reserve(inwardOutputCount);

    for (int i = 0; i < inwardOutputCount; ++i) {
        int outputValue = chunk->ReadInt();
        behavior.inwardOutputs.push_back(outputValue);
        chunk->ReadInt(); // Skip extra value
    }

    // Read outward outputs
    const int outwardOutputCount = chunk->ReadInt();
    behavior.outwardOutputs.clear();
    behavior.outwardOutputs.reserve(outwardOutputCount);

    for (int i = 0; i < outwardOutputCount; ++i) {
        int outputValue = chunk->ReadInt();
        behavior.outwardOutputs.push_back(outputValue);
        chunk->ReadInt(); // Skip extra value
    }

    // Update input and output counts
    behavior.inputCount = inwardInputCount + outwardInputCount;
    behavior.outputCount = inwardOutputCount + outwardOutputCount;
}

void InterfaceData::LoadExtraData(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;

    // Check for extra data identifiers
    int extraVersion = 0;
    if (chunk->SeekIdentifier(0xA12312F7)) {
        extraVersion = 3;
    } else if (chunk->SeekIdentifier(0xA12312F6)) {
        extraVersion = 2;
    } else if (chunk->SeekIdentifier(0xA12312F5)) {
        extraVersion = 1;
    } else {
        extraData.clear();
        return; // No extra data
    }

    // Store extra data version
    extraDataVersion = extraVersion;

    // Read extra data count
    const int count = chunk->ReadInt();
    extraData.clear();
    extraData.reserve(count);

    // Read each extra data entry
    for (int i = 0; i < count; ++i) {
        ExtraData data;

        // Read type
        data.type = static_cast<ExtraDataType>(chunk->ReadDword());

        // Read type-specific data
        switch (data.type) {
        case EXTRA_DATA_BEHAVIOR:
        case EXTRA_DATA_PARAMETER:
            data.id1 = chunk->ReadObjectID();
            break;
        case EXTRA_DATA_CONNECTION:
            data.id1 = chunk->ReadObjectID();
            data.id2 = chunk->ReadObjectID();
            break;
        case EXTRA_DATA_VALUE:
            data.value = chunk->ReadInt();
            break;
        default:
            break;
        }

        // Read sub-data if version >= 2
        if (extraVersion >= 2) {
            int size = chunk->ReadInt();
            data.subData.reserve(size);

            for (int j = 0; j < size; ++j) {
                ExtraSubData subData;

                // Read values
                subData.value1 = chunk->ReadInt();
                if (extraVersion == 2 && subData.value1 >= 4)
                    subData.value1 += 2;
                subData.value2 = chunk->ReadInt();
                subData.id1 = chunk->ReadObjectID();

                // Read additional data based on value1
                if (subData.value1 == 2 || subData.value1 == 3 ||
                    subData.value1 == 8 || subData.value1 == 9 ||
                    subData.value1 == 10 || subData.value1 == 11) {
                    subData.id2 = chunk->ReadObjectID();
                } else {
                    CKBYTE *buffer = nullptr;
                    int bufferLength = chunk->ReadBuffer((void **) &buffer);
                    if (buffer && bufferLength > 0) {
                        subData.buffer.resize(bufferLength);
                        memcpy(subData.buffer.data(), buffer, bufferLength);
                        delete[] buffer;
                    }
                }

                data.subData.push_back(subData);
            }
        }

        extraData.push_back(data);
    }
}

CKBOOL InterfaceData::SaveBehaviorHeader(SerializationContext &context) {
    CKBehavior *beh = context.behavior;
    if (!beh)
        return FALSE;

    context.isNotScript = (beh->GetType() & CKBEHAVIORTYPE_SCRIPT) == 0;
    context.isBuildingBlock = (beh->GetFlags() & CKBEHAVIOR_BUILDINGBLOCK) != 0;

    CKStateChunk *chunk = context.chunk;
    const BehaviorData &behavior = GetBehaviorForContext(context);

    // Write behavior object reference
    chunk->WriteObject(beh);

    // Write flags
    CKDWORD flag = behavior.folded ? 0x200 : 0;
    chunk->WriteDword(flag);

    if (!context.isNotScript) {
        // Save script-specific data
        chunk->WriteDword(context.scriptIndex++); // index
        chunk->WriteFloat(behavior.size.hPos);
        chunk->WriteFloat(behavior.size.vPos);
        chunk->WriteFloat(start.hStartPos);
        chunk->WriteFloat(start.vStartPos);
        chunk->WriteFloat(start.vSize);
        chunk->WriteBitmap(start.snapshot); // header snapshot
        chunk->WriteDword(start.headerColor);
    } else {
        // Save behavior-specific data
        chunk->WriteDword(behavior.depth);
        chunk->WriteFloat(behavior.size.hPos);
        chunk->WriteFloat(behavior.size.vPos);
        chunk->WriteFloat(behavior.size.hSize);
        chunk->WriteFloat(behavior.size.vSize);
        chunk->WriteFloat(behavior.hExpandSize);
        chunk->WriteFloat(behavior.vExpandSize);
    }

    return TRUE;
}

void InterfaceData::SaveBehaviorLinks(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;
    const BehaviorData &behavior = GetBehaviorForContext(context);

    // Write link count and data
    chunk->WriteInt(behavior.links.size());
    for (const auto &link : behavior.links) {
        chunk->WriteInt(static_cast<int>(link.type));
        chunk->WriteObjectID(link.id);
        chunk->WriteObjectID(link.start.id);
        chunk->WriteInt(link.start.index);
        chunk->WriteInt(link.start.type);
        chunk->WriteInt(link.points.size());
        for (const auto &point : link.points) {
            chunk->WriteFloat(point.h);
            chunk->WriteFloat(point.v);
        }
        chunk->WriteObjectID(link.end.id);
        chunk->WriteInt(link.end.index);
        chunk->WriteInt(link.end.type);
    }
}

void InterfaceData::SaveBehaviorOperations(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;
    const BehaviorData &behavior = GetBehaviorForContext(context);

    // Write operation count and data
    chunk->WriteInt(behavior.operations.size());
    for (const auto &op : behavior.operations) {
        chunk->WriteObjectID(op.id);
        chunk->WriteFloat(op.hPos);
        chunk->WriteFloat(op.vPos);
    }
}

void InterfaceData::SaveBehaviorComments(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;
    const BehaviorData &behavior = GetBehaviorForContext(context);

    // Write comment count and data
    chunk->WriteInt(behavior.comments.size());
    for (const auto &comment : behavior.comments) {
        // Write comment rectangle
        chunk->WriteFloat(comment.hPos);
        chunk->WriteFloat(comment.vPos);
        chunk->WriteFloat(comment.hPos + comment.width);
        chunk->WriteFloat(comment.vPos + comment.height);

        // Write comment text
        chunk->WriteString(comment.text.c_str());

        // Write style flags if version >= 0x16
        if (context.version >= 0x16) {
            chunk->WriteDword(comment.styleFlags);
        }
    }
}

void InterfaceData::SaveBehaviorParameters(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;
    const BehaviorData &behavior = GetBehaviorForContext(context);

    // Save local parameters
    chunk->WriteInt(behavior.localParams.size());
    for (const auto &param : behavior.localParams) {
        chunk->WriteInt(param.hPos);
        chunk->WriteInt(param.vPos);
    }
    for (const auto &param : behavior.localParams) {
        chunk->WriteInt(param.style);
    }

    // Save shared parameters
    chunk->WriteInt(behavior.sharedParams.size());
    for (const auto &param : behavior.sharedParams) {
        chunk->WriteInt(param.hPos);
        chunk->WriteInt(param.vPos);
    }
    for (const auto &param : behavior.sharedParams) {
        chunk->WriteInt(param.style);
    }
    for (const auto &param : behavior.sharedParams) {
        chunk->WriteObjectID(param.sourceId);
    }
}

void InterfaceData::SaveBehaviorGraph(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;
    const BehaviorData &behavior = GetBehaviorForContext(context);

    // Save inputs and outputs for graph
    chunk->WriteInt(behavior.inwardInputs.size());
    for (const auto &input : behavior.inwardInputs) {
        chunk->WriteInt(input);
        chunk->WriteInt(-1);
    }

    chunk->WriteInt(behavior.outwardInputs.size());
    for (const auto &input : behavior.outwardInputs) {
        chunk->WriteInt(input);
        chunk->WriteInt(-1);
    }

    chunk->WriteInt(behavior.inwardOutputs.size());
    for (const auto &output : behavior.inwardOutputs) {
        chunk->WriteInt(output);
        chunk->WriteInt(1);
    }

    chunk->WriteInt(behavior.outwardOutputs.size());
    for (const auto &output : behavior.outwardOutputs) {
        chunk->WriteInt(output);
        chunk->WriteInt(1);
    }
}

void InterfaceData::SaveExtraData(SerializationContext &context) {
    CKStateChunk *chunk = context.chunk;

    if (extraData.empty())
        return;

    // Write extra data identifier based on version
    switch (extraDataVersion) {
    case 3:
        chunk->WriteIdentifier(0xA12312F7);
        break;
    case 2:
        chunk->WriteIdentifier(0xA12312F6);
        break;
    case 1:
    default:
        chunk->WriteIdentifier(0xA12312F5);
        break;
    }

    // Write extra data count
    chunk->WriteInt(extraData.size());

    // Write each extra data entry
    for (const auto &data : extraData) {
        // Write type
        chunk->WriteInt(data.type);

        // Write type-specific data
        switch (data.type) {
        case EXTRA_DATA_BEHAVIOR:
        case EXTRA_DATA_PARAMETER:
            chunk->WriteObjectID(data.id1);
            break;
        case EXTRA_DATA_CONNECTION:
            chunk->WriteObjectID(data.id1);
            chunk->WriteObjectID(data.id2);
            break;
        case EXTRA_DATA_VALUE:
            chunk->WriteInt(data.value);
            break;
        default:
            break;
        }

        // Write sub-data for version >= 2
        if (extraDataVersion >= 2) {
            chunk->WriteInt(data.subData.size());

            for (const auto &sub : data.subData) {
                int value1 = sub.value1;
                if (extraDataVersion == 2 && value1 >= 6)
                    value1 -= 2; // Adjust for version 2

                chunk->WriteInt(value1);
                chunk->WriteInt(sub.value2);
                chunk->WriteObjectID(sub.id1);

                if (value1 == 2 || value1 == 3 ||
                    value1 == 8 || value1 == 9 ||
                    value1 == 10 || value1 == 11) {
                    chunk->WriteObjectID(sub.id2);
                } else if (!sub.buffer.empty()) {
                    chunk->WriteBuffer(sub.buffer.size(), (void *) (sub.buffer.data()));
                } else {
                    // Write empty buffer
                    chunk->WriteBuffer(0, nullptr);
                }
            }
        }
    }
}

CKERROR InterfaceData::LoadFromChunk(CKBehavior *behavior, CKStateChunk *chunk) {
    if (!behavior || !chunk)
        return CKERR_INVALIDPARAMETER;

    // Initialize serialization context
    SerializationContext context;
    context.behavior = behavior;
    context.chunk = chunk;

    // Get behavior context
    CKContext *ckContext = behavior->GetCKContext();

    // Clear existing data
    Clear();

    // Determine if behavior is a script
    context.isNotScript = (behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) == 0;

    // Start reading the chunk
    chunk->StartRead();

    // Read version
    context.version = 0;
    if (chunk->SeekIdentifier(0xB0000001)) {
        context.version = chunk->ReadInt();
    }
    if (chunk->SeekIdentifier(1)) {
        context.version = chunk->ReadInt();
    }

    // Store version for future reference
    version = context.version;

    // Check version compatibility
    if (context.version < 0x12) {
        ckContext->OutputToConsole((CKSTRING) "Deprecated version of interface chunk");
        return CKERR_OBSOLETEVIRTOOLS;
    }

    if (context.version > 0x16) {
        ckContext->OutputToConsole((CKSTRING) "Unsupported version of interface chunk");
        return CKERR_NOTIMPLEMENTED;
    }

    // Read behavior count
    const int count = chunk->ReadInt();
    behaviorCount = count - 1; // Subtract 1 for the root
    behaviors.resize(behaviorCount);

    // Read each behavior
    for (int i = 0; i < count; ++i) {
        if (i != 0) {
            CK_ID behID = chunk->ReadObjectID();
            CKBehavior *beh = (CKBehavior *) ckContext->GetObject(behID);
            if (!beh) {
                ckContext->OutputToConsoleEx((CKSTRING) "Error: Behavior <%s> not found", behID);
                return CKERR_NOTFOUND;
            }
            context.behavior = beh;
        }

        context.behaviorIndex = i - 1;
        BehaviorData *targetBehavior = (i == 0) ? &scriptRoot : &behaviors[i - 1];

        if (LoadBehaviorHeader(context, *targetBehavior)) {
            if (!(context.flags & 0x8000)) {
                LoadBehaviorLinks(context, *targetBehavior);
                LoadBehaviorOperations(context, *targetBehavior);
                LoadBehaviorComments(context, *targetBehavior);

                if (!context.isBuildingBlock)
                    LoadBehaviorParameters(context, *targetBehavior);

                if (context.isNotScript && (context.version == 0x12 || !context.isBuildingBlock))
                    LoadBehaviorGraph(context, *targetBehavior);
            }
            context.isNotScript = TRUE;
        }
    }

    // Load extra data
    LoadExtraData(context);

    chunk->CloseChunk();
    return CK_OK;
}

// Utility functions for extracting and generating interface chunks
CKERROR ExtractInterfaceData(CKBehavior *behavior, InterfaceData &data) {
    if (!behavior)
        return CKERR_INVALIDPARAMETER;

    // Clear existing data
    data.Clear();

    // Get interface chunk
    CKStateChunk *chunk = behavior->GetInterfaceChunk();
    if (!chunk)
        return CKERR_NOTFOUND;

    // Load data from chunk
    return data.LoadFromChunk(behavior, chunk);
}

CKStateChunk *GenerateInterfaceChunk(InterfaceData &data, CKBehavior *behavior) {
    if (!behavior)
        return nullptr;

    // Create and start the chunk
    CKStateChunk *chunk = CreateCKStateChunk(-1);
    if (!chunk)
        return nullptr;

    // Generate the interface chunk
    if (data.SaveToChunk(behavior, chunk) != CK_OK) {
        delete chunk;
        return nullptr;
    }

    return chunk;
}

CKERROR InterfaceData::SaveToChunk(CKBehavior *behavior, CKStateChunk *chunk) {
    if (!behavior || !chunk)
        return CKERR_INVALIDPARAMETER;

    // Initialize serialization context
    SerializationContext context;
    context.behavior = behavior;
    context.chunk = chunk;
    context.version = version;

    // Start writing to the chunk
    chunk->StartWrite();

    // Write header information
    chunk->WriteIdentifier(1);
    chunk->WriteDword(context.version);
    const int count = behaviorCount + 1;
    chunk->WriteInt(count);

    // Process each behavior
    context.scriptIndex = 0;
    context.behaviorIndex = 0;

    // First, write the script root
    context.behavior = behavior;
    if (SaveBehaviorHeader(context)) {
        SaveBehaviorLinks(context);
        SaveBehaviorOperations(context);
        SaveBehaviorComments(context);
        SaveBehaviorParameters(context);

        if (context.isNotScript)
            SaveBehaviorGraph(context);
    }

    // Then, write each additional behavior
    for (int i = 0; i < behaviorCount; ++i) {
        const BehaviorData &bb = behaviors[i];
        context.behavior = (CKBehavior *) behavior->GetCKContext()->GetObject(bb.id);
        if (!context.behavior) {
            behavior->GetCKContext()->OutputToConsoleEx((CKSTRING) "Error: Behavior <%s> not found", bb.id);
            return CKERR_NOTFOUND;
        }

        context.behaviorIndex = i;
        context.isNotScript = (context.behavior->GetType() & CKBEHAVIORTYPE_SCRIPT) == 0;
        context.isBuildingBlock = (context.behavior->GetFlags() & CKBEHAVIOR_BUILDINGBLOCK) != 0;

        if (SaveBehaviorHeader(context)) {
            SaveBehaviorLinks(context);
            SaveBehaviorOperations(context);
            SaveBehaviorComments(context);

            if (!context.isBuildingBlock)
                SaveBehaviorParameters(context);

            if (context.isNotScript && !context.isBuildingBlock)
                SaveBehaviorGraph(context);
        }
    }

    // Write extra data if any
    SaveExtraData(context);

    // Finish writing
    chunk->CloseChunk();

    return CK_OK;
}
