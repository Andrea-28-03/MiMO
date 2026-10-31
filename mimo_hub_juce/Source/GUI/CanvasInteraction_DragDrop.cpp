#include "CanvasInteraction.h"
#include "CanvasContextMenu.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include "SidebarComponent.h"
#include "BottomDock.h"
#include <algorithm>

namespace mimo
{

void CanvasInteraction::handleMouseDrag(MainComponent& comp, const juce::MouseEvent& e)
{
    const auto pt = e.position;
    comp.currentMousePos = pt;

    if (comp.dragMode == MainComponent::DragMode::PanCanvas)
    {
        if (pt.getDistanceFrom(comp.rightMouseDownPos) > 4.0f)
            comp.isRightClickPan = true;

        comp.canvasOffset = comp.dragStartOffset + (pt - comp.dragStartMouse);
        comp.panVelocity = { 0.0f, 0.0f };
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::MoveNode)
    {
        const auto worldPt = comp.screenToWorld(pt);
        const auto delta = worldPt - comp.dragStartMouse;

        for (auto& n : comp.nodes)
        {
            if (comp.isNodeSelected(n.id))
            {
                auto itOffset = comp.dragStartNodeOffsets.find(n.id);
                if (itOffset != comp.dragStartNodeOffsets.end())
                    n.pos = itOffset->second + delta;
                else if (n.id == comp.draggedNodeId)
                    n.pos = comp.dragStartOffset + delta;

                n.targetPos = n.pos;
                n.isAnimatingPos = false;
            }
        }
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::LassoSelection)
    {
        comp.lassoCurrentWorld = comp.screenToWorld(pt);
        const auto lassoRect = comp.getLassoWorldRect();

        for (const auto& n : comp.nodes)
        {
            if (n.type == NodeType::RockPiCentral)
                continue;

            const auto nodeBox = juce::Rectangle<float>(n.pos.x, n.pos.y, n.size.x, n.size.y);
            if (lassoRect.intersects(nodeBox))
            {
                if (!comp.isNodeSelected(n.id))
                    comp.selectedNodeIds.push_back(n.id);
            }
            else if (!e.mods.isShiftDown())
            {
                auto it = std::find(comp.selectedNodeIds.begin(), comp.selectedNodeIds.end(), n.id);
                if (it != comp.selectedNodeIds.end())
                    comp.selectedNodeIds.erase(it);
            }
        }

        if (!comp.selectedNodeIds.empty())
            comp.selectedNodeId = comp.selectedNodeIds.back();
        else
            comp.selectedNodeId.clear();

        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::MoveInnerBlock)
    {
        for (auto& n : comp.nodes)
            if (n.id == comp.draggedNodeId)
                for (auto& b : n.innerBlocks)
                    if (b.id == comp.draggedBlockId)
                        b.pos = comp.dragStartOffset + (pt - comp.dragStartMouse);
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::TurnKnob || comp.dragMode == MainComponent::DragMode::TurnInnerKnob)
    {
        const float dy = comp.dragStartMouse.y - pt.y;
        const float newNorm = juce::jlimit(0.0f, 1.0f, comp.dragStartKnobNorm + dy / 150.0f);
        for (auto& n : comp.nodes)
        {
            if (n.id != comp.draggedNodeId)
                continue;
            if (comp.draggedBlockId == "direct" && !n.directParams.empty())
            {
                for (auto& p : n.directParams)
                {
                    if (p.key == comp.draggedParamKey)
                    {
                        p.setFromNormalized(newNorm);
                        if (n.type == NodeType::MasterOut)
                        {
                            const float g = juce::Decibels::decibelsToGain(p.value);
                            comp.audioEngine.setMasterVolume(g);
                            comp.synthMasterGain.store(g);
                            comp.appMasterVolume.store(g);
                        }
                        else
                            comp.audioEngine.setModulation(n.id, p.key, newNorm);
                        break;
                    }
                }
            }
            else
            {
                for (auto& b : n.innerBlocks)
                {
                    if (b.id == comp.draggedBlockId)
                    {
                        for (auto& p : b.params)
                        {
                            if (p.key == comp.draggedParamKey)
                            {
                                p.setFromNormalized(newNorm);
                                comp.audioEngine.setModulation(n.id, p.key, newNorm);
                                break;
                            }
                        }
                    }
                }
            }
        }
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::DrawCable)
    {
        comp.cableCurrentWorld = comp.screenToWorld(pt);

        // Snap magnetico verso la presa compatibile per tipo segnale
        const PinHit* snapPin = nullptr;
        float snapDist = 28.0f;
        for (const auto& pin : comp.collectAllPins())
        {
            const bool validDir = (pin.isOutput != comp.cableSourceIsOutput);
            const bool pinIsTrig = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            const bool typeCompatible = comp.cableSourceIsTrigger ? pinIsTrig : (comp.cableSourceIsMod ? pin.isMod : (!pin.isMod && !pinIsTrig));

            if (validDir && pin.nodeId != comp.cableSourceNodeId && typeCompatible)
            {
                const float d = pin.worldPos.getDistanceFrom(comp.cableCurrentWorld);
                if (d <= snapDist)
                {
                    snapDist = d;
                    snapPin = &pin;
                }
            }
        }
        if (snapPin != nullptr)
            comp.cableCurrentWorld = snapPin->worldPos;

        if (comp.cableFreehandWorld.empty() || comp.cableFreehandWorld.back().getDistanceFrom(comp.cableCurrentWorld) >= 10.0f)
            comp.cableFreehandWorld.push_back(comp.cableCurrentWorld);
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::DragSidebarItem)
    {
        const auto worldPt = comp.screenToWorld(pt);
        if (!comp.draggedCatalogItem.isEffect)
        {
            comp.hoveredHwNodeId = comp.findHwModuleAt(worldPt);
            comp.hoveredAudioCableId.clear();
        }
        else
        {
            comp.hoveredAudioCableId = comp.findAudioCableNear(worldPt, 45.0f);
            comp.hoveredHwNodeId.clear();
        }
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::DragPolaroid)
    {
        comp.hoveredSlotIndex = -1;
        for (int i = 0; i < 5; ++i)
            if (comp.getSlotBounds(i).contains(pt))
                comp.hoveredSlotIndex = i;
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::DragCanvasMicSensitivity)
    {
        for (auto& n : comp.nodes)
        {
            if (n.isIntegratedMic || n.id == "hw-mic")
            {
                const auto worldPt = comp.screenToWorld(pt);
                juce::Rectangle<float> slTrack(n.pos.x + 12.0f, n.pos.y + 68.0f, n.size.x - 24.0f, 14.0f);
                if (slTrack.getWidth() > 0.0f)
                {
                    comp.micSensitivity = juce::jlimit(0.05f, 0.95f, (worldPt.x - slTrack.getX()) / slTrack.getWidth());
                    comp.repaint();
                }
                break;
            }
        }
    }
}

void CanvasInteraction::handleMouseUp(MainComponent& comp, const juce::MouseEvent& e)
{
    const auto pt = e.position;
    const auto worldPt = comp.screenToWorld(pt);

    if (comp.dragMode == MainComponent::DragMode::DrawCable)
    {
        const PinHit* bestPin = nullptr;
        float bestDist = 32.0f;
        const auto allPins = comp.collectAllPins();
        for (const auto& pin : allPins)
        {
            const bool validDir = (pin.isOutput != comp.cableSourceIsOutput);
            const bool pinIsTrig = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            const bool typeCompatible = comp.cableSourceIsTrigger ? pinIsTrig : (comp.cableSourceIsMod ? pin.isMod : (!pin.isMod && !pinIsTrig));

            if (validDir && pin.nodeId != comp.cableSourceNodeId && typeCompatible)
            {
                const float dist = pin.worldPos.getDistanceFrom(worldPt);
                if (dist <= bestDist)
                {
                    bestDist = dist;
                    bestPin = &pin;
                }
            }
        }

        if (bestPin != nullptr)
        {
            const auto& pin = *bestPin;
            const auto& srcNode = comp.cableSourceIsOutput ? comp.cableSourceNodeId : pin.nodeId;
            const auto& srcPin  = comp.cableSourceIsOutput ? comp.cableSourcePinId  : pin.pinId;
            const auto& dstNode = comp.cableSourceIsOutput ? pin.nodeId        : comp.cableSourceNodeId;
            const auto& dstPin  = comp.cableSourceIsOutput ? pin.pinId         : comp.cableSourcePinId;
            const bool isMod = comp.cableSourceIsMod || pin.isMod;
            const bool isTrig = comp.cableSourceIsTrigger || (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            const juce::Colour cableCol = isTrig ? juce::Colour(0xFFFF7A18) : (isMod ? (comp.cableSourceIsOutput ? comp.cableSourceColour : pin.colour) : MiMOLookAndFeel::fountainInk);

            comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
                [&](const CanvasCable& c) {
                    if (c.targetNodeId == dstNode && c.targetPinId == dstPin)
                        return true;
                    if (c.sourceNodeId == srcNode && c.sourcePinId == srcPin &&
                        c.targetNodeId == dstNode && c.targetPinId == dstPin)
                        return true;
                    if (c.sourceNodeId == dstNode && c.sourcePinId == dstPin &&
                        c.targetNodeId == srcNode && c.targetPinId == srcPin)
                        return true;
                    return false;
                }),
                comp.cables.end());

            comp.cables.push_back({
                "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000),
                srcNode, srcPin,
                dstNode, dstPin,
                isMod, cableCol, {},
                isTrig
            });

            comp.syncAudioRoutes();
            comp.pushUndoSnapshot();
        }

        comp.isUnhookingExistingCable = false;
        comp.cableFreehandWorld.clear();
        comp.dragMode = MainComponent::DragMode::None;
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::LassoSelection)
    {
        comp.dragMode = MainComponent::DragMode::None;
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::MoveNode)
    {
        const float sidebarThreshold = static_cast<float>(comp.getWidth()) - (comp.sidebarOpen ? 280.0f : 50.0f);
        if (pt.x >= sidebarThreshold)
        {
            comp.pushUndoSnapshot();
            comp.deleteNode(comp.draggedNodeId);
            comp.dragMode = MainComponent::DragMode::None;
            comp.draggedNodeId.clear();
            comp.repaint();
            return;
        }

        for (const auto& n : comp.nodes)
        {
            if (n.id == comp.draggedNodeId && n.type == NodeType::HardwareModule)
            {
                bool hasExistingCable = false;
                for (const auto& c : comp.cables)
                {
                    if (c.sourceNodeId == n.id && c.sourcePinId == "audio:out")
                    {
                        hasExistingCable = true;
                        break;
                    }
                }

                if (!hasExistingCable)
                {
                    for (const auto& rPi : comp.nodes)
                    {
                        if (rPi.id == "rockpi")
                        {
                            static constexpr float keyXFractions[5] = { 0.1490f, 0.2958f, 0.4426f, 0.5895f, 0.7349f };
                            for (int i = 0; i < 5; ++i)
                            {
                                const float targetX = rPi.pos.x + rPi.size.x * keyXFractions[i];
                                const float targetY = rPi.pos.y + rPi.size.y + 4.0f;
                                const auto plugPos = juce::Point<float>(n.pos.x + 29.0f, n.pos.y);

                                if (std::abs(plugPos.x - targetX) <= 50.0f && std::abs(plugPos.y - targetY) <= 120.0f)
                                {
                                    const juce::String targetPin = "audio:p" + juce::String(i + 1);
                                    bool slotOccupied = false;
                                    for (const auto& c : comp.cables)
                                    {
                                        if (c.targetNodeId == "rockpi" && c.targetPinId == targetPin)
                                        {
                                            slotOccupied = true;
                                            break;
                                        }
                                    }

                                    if (!slotOccupied)
                                    {
                                        comp.cables.push_back({
                                            "c-" + juce::String(juce::Time::getMillisecondCounter()),
                                            n.id, "audio:out",
                                            "rockpi", targetPin,
                                            false, MiMOLookAndFeel::fountainInk, {}
                                        });
                                    }
                                    break;
                                }
                            }
                            break;
                        }
                    }
                }
                break;
            }
        }
        comp.pushUndoSnapshot();
        comp.dragMode = MainComponent::DragMode::None;
        comp.draggedNodeId.clear();
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::DragSidebarItem)
    {
        const float sidebarThreshold = static_cast<float>(comp.getWidth()) - (comp.sidebarOpen ? 308.0f : 44.0f);
        if (pt.x < sidebarThreshold)
        {
            if (!comp.draggedCatalogItem.isEffect)
            {
                if (comp.draggedCatalogItem.id == "hw-drum")
                {
                    if (!comp.isDrumInCanvas())
                        comp.toggleDrumInCanvas();

                    for (auto& n : comp.nodes)
                    {
                        if (n.id == "hw-drum")
                        {
                            n.pos = worldPt - n.size * 0.5f;
                            n.targetPos = n.pos;
                            n.isAnimatingPos = false;
                            break;
                        }
                    }
                }
                else if (comp.draggedCatalogItem.id == "integrated-mic")
                {
                    if (!comp.isMicInCanvas())
                        comp.toggleMicInCanvas();

                    for (auto& n : comp.nodes)
                    {
                        if (n.isIntegratedMic || n.id == "hw-mic")
                        {
                            n.pos = worldPt - n.size * 0.5f;
                            n.targetPos = n.pos;
                            n.isAnimatingPos = false;
                            break;
                        }
                    }
                }
                else if (comp.draggedCatalogItem.id == "integrated-webcam" || comp.draggedCatalogItem.id == "integrated-cam")
                {
                    if (!comp.isCameraInCanvas())
                        comp.toggleCameraInCanvas();

                    for (auto& n : comp.nodes)
                    {
                        if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
                        {
                            n.pos = worldPt - n.size * 0.5f;
                            n.targetPos = n.pos;
                            n.isAnimatingPos = false;
                            break;
                        }
                    }
                }
                else
                {
                    const juce::String targetHwId = comp.findHwModuleAt(worldPt);
                    if (targetHwId.isNotEmpty())
                    {
                        for (auto& n : comp.nodes)
                        {
                            if (n.id == targetHwId)
                            {
                                n.hasInstrument = true;
                                n.instrumentName = comp.draggedCatalogItem.name;
                                n.instrumentEngine = comp.draggedCatalogItem.engine;
                                n.crayonColour = comp.draggedCatalogItem.colour;
                                break;
                            }
                        }
                        comp.syncAudioRoutes();
                    }
                    else
                    {
                        auto instNode = comp.createInstrumentNodeFromCatalog(comp.draggedCatalogItem, worldPt - juce::Point<float>{ 115.0f, 60.0f }, comp.fxCounter++);
                        comp.nodes.push_back(instNode);

                        for (const auto& n : comp.nodes)
                        {
                            if (n.type == NodeType::MasterOut)
                            {
                                comp.cables.push_back({
                                    "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000),
                                    instNode.id, "audio:out",
                                    n.id, "audio:in",
                                    false, MiMOLookAndFeel::fountainInk, {}
                                });
                                break;
                            }
                        }
                        comp.syncAudioRoutes();
                    }
                }
            }
            else
            {
                juce::String cableId = comp.findAudioCableNear(worldPt, 45.0f);
                if (cableId.isNotEmpty())
                {
                    comp.insertEffectOnCable(comp.draggedCatalogItem, cableId, worldPt);
                    comp.syncAudioRoutes();
                }
                else
                {
                    auto fxNode = comp.createFxNodeFromCatalog(comp.draggedCatalogItem, worldPt - juce::Point<float>{ 130.0f, 80.0f }, comp.fxCounter++, false);
                    comp.nodes.push_back(fxNode);
                    comp.syncAudioRoutes();
                }
            }
            comp.pushUndoSnapshot();
        }
        comp.hoveredHwNodeId.clear();
        comp.hoveredAudioCableId.clear();
        comp.dragMode = MainComponent::DragMode::None;
        comp.repaint();
    }
    else if (comp.dragMode == MainComponent::DragMode::DragPolaroid)
    {
        for (int i = 0; i < 5; ++i)
        {
            if (comp.getSlotBounds(i).contains(pt))
            {
                comp.assignPolaroidToSlot(i, comp.pendingPolaroid);
                break;
            }
        }
        comp.hasPendingPolaroid = false;
        comp.hoveredSlotIndex = -1;
        comp.dragMode = MainComponent::DragMode::None;
        comp.repaint();
    }
    else
    {
        if (comp.dragMode == MainComponent::DragMode::PanCanvas)
        {
            if (!comp.isRightClickPan && comp.rightMouseDownPos.getDistanceFrom({ 0.0f, 0.0f }) > 0.001f)
            {
                comp.dragMode = MainComponent::DragMode::None;
                comp.rightMouseDownPos = { 0.0f, 0.0f };
                comp.isRightClickPan = false;
                CanvasContextMenu::showContextMenu(comp, pt);
                comp.repaint();
                return;
            }
        }

        if (comp.dragMode == MainComponent::DragMode::DragSettingsVolume)
            comp.saveSettings();

        comp.dragMode = MainComponent::DragMode::None;
        comp.rightMouseDownPos = { 0.0f, 0.0f };
        comp.isRightClickPan = false;
        comp.repaint();
    }
}

} // namespace mimo
