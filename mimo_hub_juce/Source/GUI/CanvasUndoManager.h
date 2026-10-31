#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CanvasGraph.h"
#include <vector>
#include <map>

namespace mimo
{

struct CanvasSnapshot
{
    std::vector<CanvasNode> nodes;
    std::vector<CanvasCable> cables;
};

struct CanvasClipboard
{
    std::vector<CanvasNode> nodes;
    std::vector<CanvasCable> cables;
};

class CanvasUndoManager
{
public:
    explicit CanvasUndoManager(size_t maxHistorySize = 64);

    void pushSnapshot(const std::vector<CanvasNode>& nodes, const std::vector<CanvasCable>& cables);

    bool canUndo() const { return !undoStack.empty(); }
    bool canRedo() const { return !redoStack.empty(); }

    bool undo(std::vector<CanvasNode>& currentNodes, std::vector<CanvasCable>& currentCables);
    bool redo(std::vector<CanvasNode>& currentNodes, std::vector<CanvasCable>& currentCables);

    void copy(const std::vector<CanvasNode>& allNodes,
              const std::vector<CanvasCable>& allCables,
              const std::vector<juce::String>& selectedIds);

    bool hasClipboard() const { return !clipboard.nodes.empty(); }

    bool paste(std::vector<CanvasNode>& allNodes,
               std::vector<CanvasCable>& allCables,
               std::vector<juce::String>& outNewSelectedIds,
               int& fxCounter,
               juce::Point<float> offsetDelta = { 30.0f, 30.0f });

    void clear();

private:
    size_t maxHistory = 64;
    std::vector<CanvasSnapshot> undoStack;
    std::vector<CanvasSnapshot> redoStack;
    CanvasClipboard clipboard;
};

} // namespace mimo
