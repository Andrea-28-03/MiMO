#include "CanvasUndoManager.h"
#include <algorithm>

namespace mimo
{

CanvasUndoManager::CanvasUndoManager(size_t maxHistorySize)
    : maxHistory(maxHistorySize)
{
}

void CanvasUndoManager::pushSnapshot(const std::vector<CanvasNode>& nodes, const std::vector<CanvasCable>& cables)
{
    undoStack.push_back({ nodes, cables });
    if (undoStack.size() > maxHistory)
        undoStack.erase(undoStack.begin());
    redoStack.clear();
}

bool CanvasUndoManager::undo(std::vector<CanvasNode>& currentNodes, std::vector<CanvasCable>& currentCables)
{
    if (undoStack.empty())
        return false;

    // Salva lo stato attuale nel redo stack prima di ripristinare
    redoStack.push_back({ currentNodes, currentCables });

    auto snapshot = undoStack.back();
    undoStack.pop_back();

    currentNodes = std::move(snapshot.nodes);
    currentCables = std::move(snapshot.cables);
    return true;
}

bool CanvasUndoManager::redo(std::vector<CanvasNode>& currentNodes, std::vector<CanvasCable>& currentCables)
{
    if (redoStack.empty())
        return false;

    // Salva lo stato attuale nell'undo stack prima di ripristinare
    undoStack.push_back({ currentNodes, currentCables });

    auto snapshot = redoStack.back();
    redoStack.pop_back();

    currentNodes = std::move(snapshot.nodes);
    currentCables = std::move(snapshot.cables);
    return true;
}

void CanvasUndoManager::copy(const std::vector<CanvasNode>& allNodes,
                             const std::vector<CanvasCable>& allCables,
                             const std::vector<juce::String>& selectedIds)
{
    clipboard.nodes.clear();
    clipboard.cables.clear();

    if (selectedIds.empty())
        return;

    for (const auto& n : allNodes)
    {
        // Non duplicare RockPiCentral (modulo centrale fisso)
        if (n.type == NodeType::RockPiCentral)
            continue;

        if (std::find(selectedIds.begin(), selectedIds.end(), n.id) != selectedIds.end())
            clipboard.nodes.push_back(n);
    }

    if (clipboard.nodes.empty())
        return;

    // Cavi interni tra i nodi selezionati
    for (const auto& c : allCables)
    {
        const bool srcIn = std::find(selectedIds.begin(), selectedIds.end(), c.sourceNodeId) != selectedIds.end();
        const bool dstIn = std::find(selectedIds.begin(), selectedIds.end(), c.targetNodeId) != selectedIds.end();
        if (srcIn && dstIn)
            clipboard.cables.push_back(c);
    }
}

bool CanvasUndoManager::paste(std::vector<CanvasNode>& allNodes,
                              std::vector<CanvasCable>& allCables,
                              std::vector<juce::String>& outNewSelectedIds,
                              int& fxCounter,
                              juce::Point<float> offsetDelta)
{
    if (clipboard.nodes.empty())
        return false;

    outNewSelectedIds.clear();
    std::map<juce::String, juce::String> idMap;

    for (auto n : clipboard.nodes)
    {
        const juce::String oldId = n.id;
        const juce::String newId = n.id + "_copy" + juce::String(++fxCounter);
        n.id = newId;
        n.pos += offsetDelta;
        n.targetPos = n.pos;
        n.isAnimatingPos = false;
        idMap[oldId] = newId;

        allNodes.push_back(n);
        outNewSelectedIds.push_back(newId);
    }

    for (auto c : clipboard.cables)
    {
        auto itSrc = idMap.find(c.sourceNodeId);
        auto itDst = idMap.find(c.targetNodeId);
        if (itSrc != idMap.end() && itDst != idMap.end())
        {
            c.id = "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000);
            c.sourceNodeId = itSrc->second;
            c.targetNodeId = itDst->second;
            allCables.push_back(c);
        }
    }

    return true;
}

void CanvasUndoManager::clear()
{
    undoStack.clear();
    redoStack.clear();
    clipboard.nodes.clear();
    clipboard.cables.clear();
}

} // namespace mimo
