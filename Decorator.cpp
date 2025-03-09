#include "Decorator.h"

#include "CKAll.h"

#include "GraphBuilder.h"
#include "LayoutCalculator.h"

void Decorate(InterfaceData &data, CKBehavior *behavior) {
    // Clear any existing data
    data.Clear();

    // First, build the graph representation of the behavior tree
    GraphBuilder graphBuilder(data, behavior->GetCKContext());
    graphBuilder.BuildGraph(behavior);

    // Then, calculate the layout for the tree
    LayoutCalculator layoutCalculator(data,  behavior->GetCKContext());
    layoutCalculator.CalculateLayout(behavior);
}