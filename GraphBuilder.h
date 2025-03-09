#pragma once

#include <unordered_map>
#include <unordered_set>
#include "InterfaceData.h"

class CKContext;
class CKBehavior;
class CKParameterIn;
class CKParameterOut;
class CKParameterLocal;
class CKParameter;
class CKParameterOperation;

/**
 * GraphBuilder constructs the graph representation of a behavior tree.
 * It handles element relationships and data mapping.
 */
class GraphBuilder {
public:
    /**
     * Constructor
     * @param target_data Reference to the interface data to populate
     * @param context Pointer to the CK context
     */
    GraphBuilder(InterfaceData &target_data, CKContext *context);

    /**
     * Builds the graph representation of a behavior tree
     * @param rootBehavior The root behavior to process
     */
    void BuildGraph(CKBehavior *rootBehavior);

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

    /**
     * Checks if an ID is an operation
     * @param id ID to check
     * @return True if ID is an operation
     */
    bool IsOperation(CK_ID id);

    /**
     * Gets the behavior map for sharing with FlowLayout
     */
    const std::unordered_map<CK_ID, int>& GetBehaviorMap() const {
        return m_BehaviorMap;
    }

    /**
     * Gets the operation map for sharing with FlowLayout
     */
    const std::unordered_map<CK_ID, std::pair<int, int>>& GetOperationMap() const {
        return m_OperationMap;
    }

private:
    // Reference to the interface data
    InterfaceData &m_Data;

    // Pointer to the CK context
    CKContext *m_Context;

    // Maps to track object relationships
    std::unordered_map<CK_ID, int> m_BehaviorMap;                  // Maps behavior ID to index in behaviorBlocks array
    std::unordered_map<CK_ID, std::pair<int, int>> m_OperationMap; // Maps operation ID to <block index, op index>

    // Sets to track parameters
    std::unordered_set<CK_ID> m_InputParams;  // Input parameter IDs
    std::unordered_set<CK_ID> m_OutputParams; // Output parameter IDs

    /**
     * Structure to represent a parameter IO position
     */
    struct ParameterPosition {
        CK_ID id;         // Parameter ID
        int index;        // Parameter index
        CK_ID behaviorId; // Parent behavior ID
    };

    /**
     * Decorates a single behavior
     * @param behaviorBlock Behavior building block
     * @param behavior CK behavior
     * @param depth Depth in the tree
     */
    void DecorateBehavior(BehaviorBlock &behaviorBlock, CKBehavior *behavior, int depth);

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
};