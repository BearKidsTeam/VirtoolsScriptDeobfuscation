#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>

#include "InterfaceData.h"

class CKContext;
class CKBehavior;
class CKParameterIn;
class CKParameterOut;
class CKParameterLocal;
class CKParameter;
class CKParameterOperation;
class CKBehaviorLink;

/**
 * GraphBuilder constructs the graph representation of a behavior tree.
 * It handles element relationships and data mapping.
 */
class GraphBuilder {
public:
    /**
     * Constructor
     * @param targetData Reference to the interface data to populate
     * @param context Pointer to the CK context
     */
    GraphBuilder(InterfaceData &targetData, CKContext *context);

    /**
     * Builds the graph representation of a behavior tree
     * @param rootBehavior The root behavior to process
     */
    void BuildGraph(CKBehavior *rootBehavior);

private:
    /**
     * Structure to represent a parameter IO position
     */
    struct ParameterPosition {
        CK_ID id = 0;         // Parameter ID
        int index = 0;        // Parameter index
        CK_ID behaviorId = 0; // Parent behavior ID
    };

    using ParameterChain = std::unordered_map<CK_ID, std::vector<ParameterPosition>>;

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
    std::vector<CK_ID> m_InputParams;
    std::vector<CK_ID> m_OutputParams;

    /**
     * Initializes the state of the graph builder
     */
    void InitializeState();

    /**
     * Process the behavior tree using BFS traversal
     * @param rootBehavior The root behavior
     */
    void ProcessBehaviorTree(CKBehavior *rootBehavior);

    /**
     * Map operations from a behavior
     * @param behavior The behavior containing operations
     * @param depth Depth in the tree
     */
    void MapOperations(CKBehavior *behavior, int depth);

    /**
     * Enqueue sub-behaviors for processing
     * @param behavior Current behavior
     * @param depth Current depth
     * @param queue Queue for BFS traversal
     */
    void EnqueueSubBehaviors(CKBehavior *behavior, int depth,
                             std::queue<std::pair<CKBehavior *, int>> &queue);

    /**
     * Gets a behavior block by ID
     * @param id Behavior ID
     * @return Reference to the behavior block
     */
    BehaviorData &GetBehaviorData(CK_ID id);

    /**
     * Checks if an ID is an operation
     * @param id ID to check
     * @return True if ID is an operation
     */
    bool IsOperation(CK_ID id) const;

    /**
     * Set up a single behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     * @param depth Depth in the tree
     */
    void SetupBehavior(BehaviorData &behaviorData, CKBehavior *behavior, int depth);

    /**
     * Processes parameters in a behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     */
    void ProcessParameters(BehaviorData &behaviorData, CKBehavior *behavior);

    /**
     * Process parameters of a parameter operation
     * @param operation Parameter operation to process
     */
    void ProcessOperationParameters(CKParameterOperation *operation);

    /**
     * Add an input parameter to tracking
     * @param paramId Parameter ID
     */
    void AddInputParameter(CK_ID paramId);

    /**
     * Add an output parameter to tracking
     * @param paramId Parameter ID
     */
    void AddOutputParameter(CK_ID paramId);

    /**
     * Add behavior links to a behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     */
    void AddBehaviorLinks(BehaviorData &behaviorData, CKBehavior *behavior);

    /**
     * Create a behavior link from a CK behavior link
     * @param behaviorLink CK behavior link
     * @return Link representation
     */
    Link CreateBehaviorLink(CKBehaviorLink *behaviorLink);

    /**
     * Add operations to a behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     */
    void AddOperations(BehaviorData &behaviorData, CKBehavior *behavior);

    /**
     * Add local parameters to a behavior
     * @param behaviorData Behavior data
     * @param behavior CK behavior
     */
    void AddLocalParameters(BehaviorData &behaviorData, CKBehavior *behavior);

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
    CKBehavior *GetParameterOwner(CKParameter *parameter);

    /**
     * Gets a shortcut parameter position
     * @param behaviorId Parent behavior ID
     * @param sourceId Source parameter ID
     * @return Parameter position
     */
    ParameterPosition GetShortcutParameterPosition(CK_ID behaviorId, CK_ID sourceId);

    /**
     * Configures parameter links for a behavior tree
     */
    void ConfigureParameterLinks();

    /**
     * Builds chains of input parameters upwards through the behavior tree.
     * @param inputChain The map of input parameter IDs to their position chains
     */
    void BuildInputParameterChains(ParameterChain &inputChain);

    /**
     * Builds a chain for a single input parameter through the behavior tree.
     * @param inputParam The input parameter
     * @param inputChain The map to store the chain in
     */
    void BuildInputParameterChain(CKParameterIn *inputParam, ParameterChain &inputChain);

    /**
     * Builds chains of output parameters upwards through the behavior tree.
     * @param outputChain The map of output parameter IDs to their position chains
     */
    void BuildOutputParameterChains(ParameterChain &outputChain);

    /**
     * Builds a chain for a single output parameter through the behavior tree.
     * @param outputParam The output parameter
     * @param outputChain The map to store the chain in
     */
    void BuildOutputParameterChain(CKParameterOut *outputParam, ParameterChain &outputChain);

    /**
     * Connects parameter chains to form complete parameter links.
     * @param inputChain The map of input parameter IDs to their position chains
     * @param outputChain The map of output parameter IDs to their position chains
     */
    void ConnectParameterChains(const ParameterChain &inputChain, const ParameterChain &outputChain);

    /**
     * Connects an input parameter to its direct source parameter.
     * @param inputParam The input parameter to connect
     * @param sourceParam The source (output) parameter
     * @param inputPositions The positions of the input parameter in the chain
     * @param position The initial position of the input parameter
     * @param outputChain The map of output parameter chains
     */
    void ConnectToDirectSource(CKParameterIn *inputParam, CKParameter *sourceParam,
                             const std::vector<ParameterPosition> &inputPositions,
                             const ParameterPosition &position,
                             const ParameterChain &outputChain);

    /**
     * Connects an input parameter to another input parameter it shares with.
     * @param inputParam The input parameter to connect
     * @param sharedInput The shared input parameter (another input parameter with the same source)
     * @param inputPositions The positions of the input parameter in the chain
     * @param inputChain The map of input parameter chains
     */
    void ConnectToSharedSource(CKParameterIn *inputParam, CKParameterIn *sharedInput,
                             const std::vector<ParameterPosition> &inputPositions,
                             const ParameterChain &inputChain);

    /**
     * Tries to connect parameters within the same behavior.
     * @param inputPositions The positions of the input parameter
     * @param sourcePositions The positions of the source parameter
     * @param sourceParam The source parameter
     * @return True if a connection was made, false otherwise
     */
    bool ConnectParametersInSameBehavior(
        const std::vector<ParameterPosition> &inputPositions,
        const std::vector<ParameterPosition> &sourcePositions,
        CKParameter *sourceParam);

    /**
     * Creates a parameter shortcut for a connection that can't be made directly.
     * @param position The position of the input parameter
     * @param sourceParam The source parameter
     */
    void CreateParameterShortcut(const ParameterPosition &position, CKParameter *sourceParam);
};
