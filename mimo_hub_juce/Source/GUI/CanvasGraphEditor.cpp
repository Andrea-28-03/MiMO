#include "CanvasGraphEditor.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace mimo
{

void CanvasGraphEditor::toggleDrumInCanvas(MainComponent& comp)
{
    if (comp.isDrumInCanvas())
    {
        comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
            [](const CanvasCable& c) { return c.sourceNodeId == "hw-drum" || c.targetNodeId == "hw-drum"; }), comp.cables.end());
        comp.nodes.erase(std::remove_if(comp.nodes.begin(), comp.nodes.end(),
            [](const CanvasNode& n) { return n.id == "hw-drum"; }), comp.nodes.end());
        if (comp.selectedNodeId == "hw-drum")
            comp.selectedNodeId.clear();
    }
    else
    {
        CanvasNode drum;
        drum.id = "hw-drum";
        drum.type = NodeType::HardwareModule;
        drum.label = "Modulo Percussioni Piezo";
        drum.subtitle = "1-5 Sensori Tattili Indipendenti";
        drum.portBadge = "P1";
        drum.pos = { -280.0f, 175.0f };
        drum.size = { 265.0f, 255.0f };
        drum.crayonColour = MiMOLookAndFeel::crayonSage;
        drum.modOutputs = {
            { "vel", "Velocity Dinamica", MiMOLookAndFeel::crayonCherry }
        };
        comp.nodes.push_back(drum);

        comp.cables.push_back({ "c-drum-pi", "hw-drum", "audio:out", "rockpi", "audio:p1", false, MiMOLookAndFeel::fountainInk, {} });
    }

    comp.updateHardwareDeviceStates();
    comp.syncAudioRoutes();
    comp.repaint();
}

void CanvasGraphEditor::toggleMicInCanvas(MainComponent& comp)
{
    comp.micDropdownOpen = false;
    if (comp.isMicInCanvas())
    {
        comp.stopRealMicrophone();
        comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
            [](const CanvasCable& c) { return c.sourceNodeId == "hw-mic" || c.targetNodeId == "hw-mic"; }), comp.cables.end());
        comp.nodes.erase(std::remove_if(comp.nodes.begin(), comp.nodes.end(),
            [](const CanvasNode& n) { return n.id == "hw-mic" || n.isIntegratedMic; }), comp.nodes.end());
        if (comp.selectedNodeId == "hw-mic")
            comp.selectedNodeId.clear();
    }
    else
    {
        CanvasNode micNode;
        micNode.id = "hw-mic";
        micNode.type = NodeType::HardwareModule;
        micNode.isIntegratedMic = true;
        micNode.label = "Microfono Live & Pitch";
        micNode.portBadge = "MIC";
        micNode.subtitle = "Trigger, Pitch CV & Envelope";
        micNode.pos = { -560.0f, 175.0f };
        micNode.size = { 260.0f, 260.0f };
        micNode.crayonColour = MiMOLookAndFeel::crayonOchre;
        micNode.modOutputs = {
            { "pitch", "Pitch CV (Voce)",       MiMOLookAndFeel::crayonCyan },
            { "env",   "Dynamic Env Follower",  MiMOLookAndFeel::crayonPeach },
            { "gate",  "Gate Voce (On/Off)",    MiMOLookAndFeel::crayonMint }
        };
        comp.nodes.push_back(micNode);
        comp.startRealMicrophone();
    }

    comp.updateHardwareDeviceStates();
    comp.syncAudioRoutes();
    comp.repaint();
}

void CanvasGraphEditor::toggleCameraInCanvas(MainComponent& comp)
{
    comp.camDropdownOpen = false;
    if (comp.isCameraInCanvas())
    {
        comp.stopRealCamera();
        comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
            [](const CanvasCable& c) {
                return c.sourceNodeId == "hw-eye" || c.targetNodeId == "hw-eye" ||
                       c.sourceNodeId == "hw-webcam" || c.targetNodeId == "hw-webcam";
            }), comp.cables.end());
        comp.nodes.erase(std::remove_if(comp.nodes.begin(), comp.nodes.end(),
            [](const CanvasNode& n) { return n.id == "hw-eye" || n.id == "hw-webcam" || n.isIntegratedCamera; }), comp.nodes.end());
        if (comp.selectedNodeId == "hw-eye" || comp.selectedNodeId == "hw-webcam")
            comp.selectedNodeId.clear();
    }
    else
    {
        CanvasNode eye;
        eye.id = "hw-eye";
        eye.type = NodeType::HardwareModule;
        eye.isIntegratedCamera = true;
        eye.label = "Modulo Occhio";
        eye.subtitle = "Tracking Mano 3D - 60 FPS";
        eye.portBadge = "P2";
        eye.pos = { 35.0f, 175.0f };
        eye.size = { 255.0f, 380.0f };
        eye.crayonColour = MiMOLookAndFeel::crayonSage;
        comp.nodes.push_back(eye);

        comp.cables.push_back({ "c-eye-pi", "hw-eye", "audio:out", "rockpi", "audio:p2", false, MiMOLookAndFeel::fountainInk, {} });
        comp.syncEyeNodeOutputsWithDetailSettings();
        comp.startRealCamera();
    }

    comp.updateHardwareDeviceStates();
    comp.syncAudioRoutes();
    comp.repaint();
}

void CanvasGraphEditor::insertEffectOnCable(MainComponent& comp, const CatalogItem& fx, const juce::String& cableId, juce::Point<float> worldPos)
{
    juce::ignoreUnused(worldPos);
    CanvasCable orig;
    bool foundCable = false;
    for (auto it = comp.cables.begin(); it != comp.cables.end(); ++it)
    {
        if (it->id == cableId)
        {
            orig = *it;
            comp.cables.erase(it);
            foundCable = true;
            break;
        }
    }

    if (!foundCable)
        return;

    CanvasNode* sourceNode = nullptr;
    CanvasNode* targetNode = nullptr;
    for (auto& n : comp.nodes)
    {
        if (n.id == orig.sourceNodeId) sourceNode = &n;
        if (n.id == orig.targetNodeId) targetNode = &n;
    }

    if (sourceNode == nullptr || targetNode == nullptr)
        return;

    juce::Point<float> rockPiPos { -145.0f, -150.0f };
    juce::Point<float> rockPiSize { 290.0f, 260.0f };
    for (const auto& n : comp.nodes)
    {
        if (n.id == "rockpi")
        {
            rockPiPos = n.pos;
            rockPiSize = n.size;
            break;
        }
    }

    bool isVerticalCable = false;
    if (orig.targetPinId.startsWith("audio:p") && orig.targetNodeId == "rockpi")
    {
        isVerticalCable = true;
    }
    else if (sourceNode->type == NodeType::HardwareModule || (sourceNode->type == NodeType::NestedEffect && sourceNode->isVerticalFlow))
    {
        isVerticalCable = true;
    }
    else
    {
        const auto p1 = comp.getPinWorldPos(orig.sourceNodeId, orig.sourcePinId);
        const auto p2 = comp.getPinWorldPos(orig.targetNodeId, orig.targetPinId);
        if (std::abs(p2.y - p1.y) > std::abs(p2.x - p1.x) * 1.1f && (p1.y > rockPiPos.y || p2.y > rockPiPos.y))
            isVerticalCable = true;
    }

    juce::Point<float> insertPos;

    juce::String createdNodeId;
    if (isVerticalCable)
    {
        const float effectHeight = 165.0f;
        const float spacingY = 45.0f;
        const float shiftAmountY = effectHeight + spacingY;

        insertPos.x = sourceNode->pos.x;
        if (targetNode->id == "rockpi")
            insertPos.y = (rockPiPos.y + rockPiSize.y) + 40.0f;
        else
            insertPos.y = targetNode->pos.y + targetNode->size.y + 40.0f;

        for (auto& n : comp.nodes)
        {
            if (n.id == "rockpi")
                continue;

            if (std::abs(n.pos.x - sourceNode->pos.x) < 120.0f && n.pos.y >= insertPos.y - 10.0f)
            {
                n.targetPos.y = n.pos.y + shiftAmountY;
                n.isAnimatingPos = true;
            }
        }

        CanvasNode newNode = CanvasGraphHelper::createFxNodeFromCatalog(fx, insertPos, comp.fxCounter++, true);
        newNode.targetPos = insertPos;
        newNode.isAnimatingPos = false;
        createdNodeId = newNode.id;
        comp.nodes.push_back(newNode);

        comp.cables.push_back({ "c-" + newNode.id + "-in",  orig.sourceNodeId, orig.sourcePinId, newNode.id, "audio:in", false, MiMOLookAndFeel::fountainInk, {} });
        comp.cables.push_back({ "c-" + newNode.id + "-out", newNode.id, "audio:out", orig.targetNodeId, orig.targetPinId, false, newNode.crayonColour, {} });
    }
    else
    {
        const float spacing = 50.0f;
        const float effectWidth = 260.0f;
        const float shiftAmount = effectWidth + spacing;

        const bool isLeftSide = (targetNode->id == "rockpi" || (sourceNode->pos.x < rockPiPos.x && targetNode->pos.x <= rockPiPos.x));

        if (isLeftSide)
        {
            insertPos.x = targetNode->pos.x - spacing - effectWidth;
            insertPos.y = sourceNode->pos.y + (sourceNode->size.y - 165.0f) * 0.5f;

            for (auto& n : comp.nodes)
            {
                if (n.id == "rockpi" || n.pos.x >= rockPiPos.x)
                    continue;

                if (n.pos.x <= sourceNode->pos.x + 10.0f)
                {
                    n.targetPos.x = n.pos.x - shiftAmount;
                    n.isAnimatingPos = true;
                }
            }
        }
        else
        {
            if (sourceNode->id == "rockpi")
            {
                insertPos.x = sourceNode->pos.x + sourceNode->size.x + spacing;
                insertPos.y = -95.0f;
            }
            else
            {
                insertPos.x = sourceNode->pos.x + sourceNode->size.x + spacing;
                insertPos.y = sourceNode->pos.y;
            }

            for (auto& n : comp.nodes)
            {
                if (n.id == "rockpi" || n.pos.x <= rockPiPos.x)
                    continue;

                if (n.pos.x >= targetNode->pos.x - 10.0f)
                {
                    n.targetPos.x = n.pos.x + shiftAmount;
                    n.isAnimatingPos = true;
                }
            }
        }

        CanvasNode newNode = CanvasGraphHelper::createFxNodeFromCatalog(fx, insertPos, comp.fxCounter++, false);
        newNode.targetPos = insertPos;
        newNode.isAnimatingPos = false;
        createdNodeId = newNode.id;
        comp.nodes.push_back(newNode);

        comp.cables.push_back({ "c-" + newNode.id + "-in",  orig.sourceNodeId, orig.sourcePinId, newNode.id, "audio:in", false, MiMOLookAndFeel::fountainInk, {} });
        comp.cables.push_back({ "c-" + newNode.id + "-out", newNode.id, "audio:out", orig.targetNodeId, orig.targetPinId, false, newNode.crayonColour, {} });
    }

    if (createdNodeId.isNotEmpty())
        comp.selectSingleNode(createdNodeId);
    comp.pushUndoSnapshot();
    comp.syncAudioRoutes();
    comp.startTimerHz(60);
    comp.repaint();
}

void CanvasGraphEditor::deleteNode(MainComponent& comp, const juce::String& nodeId)
{
    if (nodeId.isEmpty() || nodeId == "rockpi" || nodeId == "master")
        return;

    auto it = std::find_if(comp.nodes.begin(), comp.nodes.end(), [&](const CanvasNode& n) { return n.id == nodeId; });
    if (it == comp.nodes.end())
        return;

    const auto nodeToDelete = *it;

    if (nodeToDelete.id == "hw-drum")
    {
        toggleDrumInCanvas(comp);
        if (comp.selectedNodeId == nodeId)
            comp.selectedNodeId.clear();
        comp.syncAudioRoutes();
        comp.repaint();
        return;
    }
    if (nodeToDelete.isIntegratedMic || nodeToDelete.id == "hw-mic")
    {
        toggleMicInCanvas(comp);
        if (comp.selectedNodeId == nodeId)
            comp.selectedNodeId.clear();
        comp.syncAudioRoutes();
        comp.repaint();
        return;
    }
    if (nodeToDelete.isIntegratedCamera || nodeToDelete.id == "hw-webcam" || nodeToDelete.id == "hw-eye")
    {
        toggleCameraInCanvas(comp);
        if (comp.selectedNodeId == nodeId)
            comp.selectedNodeId.clear();
        comp.syncAudioRoutes();
        comp.repaint();
        return;
    }

    if (nodeToDelete.type == NodeType::NestedEffect)
    {
        juce::String prevSourceNode, prevSourcePin;
        juce::String nextTargetNode, nextTargetPin;

        for (const auto& c : comp.cables)
        {
            if (!c.isModulation)
            {
                if (c.targetNodeId == nodeId && c.targetPinId == "audio:in")
                {
                    prevSourceNode = c.sourceNodeId;
                    prevSourcePin = c.sourcePinId;
                }
                else if (c.sourceNodeId == nodeId && c.sourcePinId == "audio:out")
                {
                    nextTargetNode = c.targetNodeId;
                    nextTargetPin = c.targetPinId;
                }
            }
        }

        comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
            [&](const CanvasCable& c) { return c.sourceNodeId == nodeId || c.targetNodeId == nodeId; }),
            comp.cables.end());

        if (prevSourceNode.isNotEmpty() && nextTargetNode.isNotEmpty())
        {
            juce::Colour prevColour = MiMOLookAndFeel::fountainInk;
            for (const auto& n : comp.nodes)
            {
                if (n.id == prevSourceNode && (n.type == NodeType::NestedEffect || n.type == NodeType::InstrumentNode))
                {
                    prevColour = n.crayonColour;
                    break;
                }
            }

            comp.cables.push_back({
                "c-bypass-" + juce::String(juce::Time::getMillisecondCounter()),
                prevSourceNode, prevSourcePin,
                nextTargetNode, nextTargetPin,
                false, prevColour, {}
            });
        }

        // Ripple Delete: richiudi lo spazio vuoto riavvicinando i nodi a valle
        if (nodeToDelete.isVerticalFlow)
        {
            const float shiftAmountY = nodeToDelete.size.y + 40.0f;
            for (auto& n : comp.nodes)
            {
                if (n.id != "rockpi" && n.id != nodeId && n.pos.y > nodeToDelete.pos.y)
                {
                    n.targetPos.y = n.pos.y - shiftAmountY;
                    n.isAnimatingPos = true;
                }
            }
        }
        else
        {
            const float shiftAmountX = nodeToDelete.size.x + 50.0f;
            for (auto& n : comp.nodes)
            {
                if (n.id != "rockpi" && n.id != nodeId && n.pos.x > nodeToDelete.pos.x)
                {
                    n.targetPos.x = n.pos.x - shiftAmountX;
                    n.isAnimatingPos = true;
                }
            }
        }
        comp.startTimerHz(60);
    }
    else
    {
        comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
            [&](const CanvasCable& c) { return c.sourceNodeId == nodeId || c.targetNodeId == nodeId; }),
            comp.cables.end());
    }

    comp.nodes.erase(std::remove_if(comp.nodes.begin(), comp.nodes.end(),
        [&](const CanvasNode& n) { return n.id == nodeId; }),
        comp.nodes.end());

    if (comp.selectedNodeId == nodeId)
        comp.selectedNodeId.clear();

    comp.syncAudioRoutes();
    comp.repaint();
}

void CanvasGraphEditor::deleteSelectedEffect(MainComponent& comp)
{
    if (comp.selectedNodeId.isNotEmpty())
        deleteNode(comp, comp.selectedNodeId);
}

void CanvasGraphEditor::assignInstrumentToHardware(MainComponent& comp, const juce::String& hwNodeId, const CatalogItem& item)
{
    CanvasNode* hwNode = nullptr;
    for (auto& n : comp.nodes)
    {
        if (n.id == hwNodeId)
        {
            hwNode = &n;
            break;
        }
    }
    if (hwNode == nullptr)
        return;

    comp.pushUndoSnapshot();

    // Calcola la posizione libera immediatamente alla destra del modulo hardware (Task 3.5)
    const float spacingX = 45.0f;
    const juce::Point<float> instPos { hwNode->pos.x + hwNode->size.x + spacingX, hwNode->pos.y };

    // Fai spazio ai nodi che si trovano a destra
    const float shiftWidth = 330.0f + spacingX;
    for (auto& n : comp.nodes)
    {
        if (n.id != hwNodeId && n.id != "rockpi" && n.pos.x >= instPos.x - 20.0f && std::abs(n.pos.y - hwNode->pos.y) < 220.0f)
        {
            n.targetPos.x = n.pos.x + shiftWidth;
            n.isAnimatingPos = true;
        }
    }

    // Istanzia lo strumento completo visibile ed editabile sul canvas
    auto instNode = CanvasGraphHelper::createInstrumentNodeFromCatalog(item, instPos, comp.fxCounter++);
    instNode.targetPos = instPos;
    instNode.isAnimatingPos = false;
    comp.nodes.push_back(instNode);

    // Collega cavi di trigger e modulazione dal controller allo strumento
    if (hwNode->id == "hw-drum")
    {
        comp.cables.push_back({
            "c-trig-" + instNode.id,
            hwNode->id, "trigger:out",
            instNode.id, "trigger:in",
            false, juce::Colour(0xFFFF7A18), {}, true
        });
        comp.cables.push_back({
            "c-vel-" + instNode.id,
            hwNode->id, "mod:vel",
            instNode.id, "param:direct.vol",
            true, MiMOLookAndFeel::crayonCherry, {}
        });
    }
    else if (hwNode->isIntegratedCamera || hwNode->id == "hw-webcam" || hwNode->id == "hw-eye")
    {
        comp.cables.push_back({
            "c-trig-" + instNode.id,
            hwNode->id, "trigger:dx_pinch_idx",
            instNode.id, "trigger:in",
            false, juce::Colour(0xFFFF7A18), {}, true
        });
        comp.cables.push_back({
            "c-mod-" + instNode.id,
            hwNode->id, "mod:dx_y",
            instNode.id, "param:direct.cutoff",
            true, MiMOLookAndFeel::crayonOchre, {}
        });
    }
    else // hw-mic
    {
        comp.cables.push_back({
            "c-trig-" + instNode.id,
            hwNode->id, "trigger:out",
            instNode.id, "trigger:in",
            false, juce::Colour(0xFFFF7A18), {}, true
        });
    }

    hwNode->hasInstrument = true;
    hwNode->instrumentName = item.name;
    hwNode->instrumentEngine = item.engine;
    hwNode->crayonColour = item.colour;

    comp.selectSingleNode(instNode.id);
    comp.syncAudioRoutes();
    comp.startTimerHz(60);
    comp.repaint();
}

void CanvasGraphEditor::autoConnectEffectFromLibrary(MainComponent& comp, const CatalogItem& fx)
{
    comp.pushUndoSnapshot();

    CanvasNode* selNode = nullptr;
    if (comp.selectedNodeId.isNotEmpty())
    {
        for (auto& n : comp.nodes)
        {
            if (n.id == comp.selectedNodeId)
            {
                selNode = &n;
                break;
            }
        }
    }

    if (selNode == nullptr)
    {
        for (auto it = comp.nodes.rbegin(); it != comp.nodes.rend(); ++it)
        {
            if (it->type == NodeType::NestedEffect)
            {
                selNode = &(*it);
                break;
            }
        }
    }

    if (selNode != nullptr && selNode->type != NodeType::MasterOut && selNode->type != NodeType::InstrumentNode)
    {
        juce::String outgoingCableId;
        for (const auto& c : comp.cables)
        {
            if (!c.isModulation && !c.isTrigger && c.sourceNodeId == selNode->id && c.sourcePinId == "audio:out")
            {
                outgoingCableId = c.id;
                break;
            }
        }

        if (outgoingCableId.isNotEmpty())
        {
            insertEffectOnCable(comp, fx, outgoingCableId, selNode->pos + juce::Point<float>{ 150.0f, 0.0f });
            return;
        }
        else
        {
            const float spacing = 45.0f;
            const juce::Point<float> insertPos { selNode->pos.x + selNode->size.x + spacing, selNode->pos.y };

            for (auto& n : comp.nodes)
            {
                if (n.id != selNode->id && n.id != "rockpi" && n.pos.x >= insertPos.x - 20.0f)
                {
                    n.targetPos.x = n.pos.x + 280.0f;
                    n.isAnimatingPos = true;
                }
            }

            auto newNode = CanvasGraphHelper::createFxNodeFromCatalog(fx, insertPos, comp.fxCounter++, false);
            newNode.targetPos = insertPos;
            newNode.isAnimatingPos = false;
            comp.nodes.push_back(newNode);

            comp.cables.push_back({
                "c-" + newNode.id + "-in",
                selNode->id, "audio:out",
                newNode.id, "audio:in",
                false, selNode->crayonColour, {}
            });

            for (const auto& n : comp.nodes)
            {
                if (n.type == NodeType::MasterOut)
                {
                    comp.cables.push_back({
                        "c-" + newNode.id + "-out",
                        newNode.id, "audio:out",
                        n.id, "audio:in",
                        false, newNode.crayonColour, {}
                    });
                    break;
                }
            }

            comp.selectSingleNode(newNode.id);
            comp.syncAudioRoutes();
            comp.startTimerHz(60);
            comp.repaint();
            return;
        }
    }

    const float midX = static_cast<float>(comp.getWidth()) * 0.40f;
    const float midY = static_cast<float>(comp.getHeight()) * 0.45f;
    const auto worldPos = comp.screenToWorld({ midX, midY });
    auto newNode = CanvasGraphHelper::createFxNodeFromCatalog(fx, worldPos, comp.fxCounter++, false);
    comp.nodes.push_back(newNode);
    comp.selectSingleNode(newNode.id);
    comp.syncAudioRoutes();
    comp.repaint();
}

} // namespace mimo
