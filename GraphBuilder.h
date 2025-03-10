#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>

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

private:
    // Reference to the interface data
    InterfaceData &m_Data;

    // Pointer to the CK context
    CKContext *m_Context;

    // Maps to track object relationships (for fast lookup)
    std::unordered_map<CK_ID, int> m_BehaviorMap;                  // Maps behavior ID to index in behaviors array
    std::unordered_map<CK_ID, std::pair<int, int>> m_OperationMap; // Maps operation ID to <block index, op index>

    // Sets to track parameters (for fast lookup)
    std::unordered_set<CK_ID> m_InputParamSet;  // Input parameter IDs
    std::unordered_set<CK_ID> m_OutputParamSet; // Output parameter IDs

    // Vectors to maintain insertion order
    std::vector<CK_ID> m_BehaviorIds;
    std::vector<CK_ID> m_OperationIds;
    std::vector<CK_ID> m_InputParamIds;
    std::vector<CK_ID> m_OutputParamIds;

    /**
     * Gets a behavior block by ID
     * @param id Behavior ID
     * @return Reference to the behavior block
     */
    BehaviorData &GetBehavior(CK_ID id);

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
    bool IsOperation(CK_ID id) const;

    /**
     * Structure to represent a parameter IO position
     */
    struct ParameterPosition {
        CK_ID id = 0;         // Parameter ID
        int index = 0;        // Parameter index
        CK_ID behaviorId = 0; // Parent behavior ID
    };

    /**
     * Decorates a single behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     * @param depth Depth in the tree
     */
    void DecorateBehavior(BehaviorData &behaviorData, CKBehavior *behavior, int depth);

    /**
     * Calculates the size of a behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     */
    void CalculateBehaviorSize(BehaviorData &behaviorData, CKBehavior *behavior);

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

    /**
     * Connects input parameters to their sources
     * @param inputChain Chain of input parameters
     * @param outputChain Chain of output parameters
     */
    void ConfigureDirectParameterConnections(
        const std::unordered_map<CK_ID, std::vector<ParameterPosition>> &inputChain,
        const std::unordered_map<CK_ID, std::vector<ParameterPosition>> &outputChain);
};