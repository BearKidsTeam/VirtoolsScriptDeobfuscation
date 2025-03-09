#include "Decorator.h"
#include "GraphBuilder.h"
#include "FlowLayout.h"
#include "CKAll.h"

class DecoratorImpl {
public:
    DecoratorImpl(InterfaceData &target_data, CKContext *context)
        : m_GraphBuilder(target_data, context),
          m_FlowLayout(target_data, context, m_GraphBuilder) {}

    void Decorate(CKBehavior *script) {
        // First, build the graph representation of the behavior tree
        m_GraphBuilder.BuildGraph(script);

        // Then, calculate the layout for the tree
        m_FlowLayout.CalculateLayout(script);
    }

private:
    GraphBuilder m_GraphBuilder;
    FlowLayout m_FlowLayout;
};

void Decorate(InterfaceData &data, CKBehavior *behavior) {
    // Clear any existing data
    data.Clear();

    // Create the decorator implementation and run the decoration process
    DecoratorImpl decoratorImpl(data, behavior->GetCKContext());
    decoratorImpl.Decorate(behavior);
}