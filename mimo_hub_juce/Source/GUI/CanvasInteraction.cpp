#include "CanvasInteraction.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include "CanvasRenderer.h"
#include "MiMOEyeDrawer.h"
#include <algorithm>

namespace mimo
{

void CanvasInteraction::handleMouseMove(MainComponent& comp, const juce::MouseEvent& e)
{
    const auto worldPt = comp.screenToWorld(e.position);

    // Hover sui pin di connessione (per snap magnetico e glow)
    comp.hasHoveredPin = false;
    const auto allPins = comp.collectAllPins();
    for (const auto& pin : allPins)
    {
        const float snapDist = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond) ? 14.0f : 12.0f;
        if (worldPt.getDistanceFrom(pin.worldPos) <= snapDist)
        {
            comp.hoveredPinNodeId = pin.nodeId;
            comp.hoveredPinId = pin.pinId;
            comp.hoveredPinPos = pin.worldPos;
            comp.hasHoveredPin = true;
            comp.repaint();
            return;
        }
    }

    if (comp.hasHoveredPin)
    {
        comp.hasHoveredPin = false;
        comp.repaint();
    }
}

void CanvasInteraction::handleMouseDown(MainComponent& comp, const juce::MouseEvent& e)
{
    const auto pt = e.position;
    const auto worldPt = comp.screenToWorld(pt);
    comp.panVelocity = { 0.0f, 0.0f };

    // Tasto centrale del mouse (Middle Button) o Barra Spaziatrice -> Pan immediato ovunque!
    if (e.mods.isMiddleButtonDown() || juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
    {
        comp.selectedNodeId.clear();
        comp.syncAudioRoutes();
        comp.dragMode = MainComponent::DragMode::PanCanvas;
        comp.dragStartMouse = pt;
        comp.dragStartOffset = comp.canvasOffset;
        comp.repaint();
        return;
    }

    // 1. Gestione click sulle tendine aperte di selezione Webcam / Microfono nel Canvas
    if (comp.camDropdownOpen || comp.micDropdownOpen)
    {
        for (const auto& n : comp.nodes)
        {
            if (n.type != NodeType::HardwareModule)
                continue;

            const bool isMicNode = (n.isIntegratedMic || n.id == "hw-mic");
            const bool isCamNode = (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye");
            if (isMicNode && !comp.micDropdownOpen) continue;
            if (isCamNode && !comp.camDropdownOpen) continue;
            if (!isMicNode && !isCamNode) continue;

            const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);
            const auto devRow = MainComponent::getIntegratedDeviceRowBounds(box);
            const auto options = isMicNode ? comp.getMicDropdownOptions() : comp.getCameraDropdownOptions();
            const float itemH = 22.0f;
            const float menuH = static_cast<float>(options.size()) * itemH + 6.0f;
            const juce::Rectangle<float> menuBox(devRow.getX(), devRow.getBottom() + 2.0f, devRow.getWidth(), menuH);

            if (menuBox.contains(worldPt))
            {
                const int clickedIdx = juce::jlimit(0, options.size() - 1,
                    static_cast<int>((worldPt.y - (menuBox.getY() + 3.0f)) / itemH));

                if (isMicNode)
                {
                    comp.micDeviceIndex.store(clickedIdx);
                    {
                        const juce::ScopedLock sl(comp.micInfoLock);
                        if (clickedIdx < comp.micDeviceList.size())
                            comp.micDeviceName = comp.micDeviceList[clickedIdx];
                    }
                    if (comp.micWorker != nullptr && comp.isMicRecording)
                        comp.micWorker->requestReopen();
                    comp.micDropdownOpen = false;
                }
                else
                {
                    const auto items = comp.getVideoSourceItems();
                    if (clickedIdx >= 0 && clickedIdx < static_cast<int>(items.size()))
                    {
                        const auto& it = items[static_cast<size_t>(clickedIdx)];
                        comp.selectVideoSource(it.isSmartphone, it.pcDeviceIdx, it.label);
                        if (it.isSmartphone)
                        {
                            comp.eyeDrawerOpen = true;
                            comp.eyeDrawerTab = 1;
                        }
                    }
                    comp.camDropdownOpen = false;
                }
                comp.repaint();
                return;
            }
        }

        bool clickedOnDevRow = false;
        for (const auto& n : comp.nodes)
        {
            if (n.type == NodeType::HardwareModule && (n.isIntegratedMic || n.isIntegratedCamera || n.id == "hw-mic" || n.id == "hw-webcam" || n.id == "hw-eye"))
            {
                const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);
                if (MainComponent::getIntegratedDeviceRowBounds(box).contains(worldPt))
                    clickedOnDevRow = true;
            }
        }
        if (!clickedOnDevRow)
        {
            comp.camDropdownOpen = false;
            comp.micDropdownOpen = false;
            comp.repaint();
        }
    }

    // 2. Click sul pulsante [QR], [VID], [X] di rimozione, tendina dispositivo o sugli slider dei moduli nel Canvas
    for (auto& n : comp.nodes)
    {
        if (n.type == NodeType::HardwareModule)
        {
            const juce::Rectangle<float> box(n.pos.x, n.pos.y, n.size.x, n.size.y);

            if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
            {
                const auto cfgBtn = MiMOEyeDrawer::getEyeCardConfigButtonBounds(box);
                if (cfgBtn.contains(worldPt))
                {
                    comp.eyeDrawerOpen = true;
                    comp.eyeDrawerTab = 0;
                    comp.repaint();
                    return;
                }

                const auto vidBtn = MiMOEyeDrawer::getEyeCardViewToggleButtonBounds(box);
                if (vidBtn.contains(worldPt))
                {
                    comp.eyeShowVideo = !comp.eyeShowVideo;
                    comp.repaint();
                    return;
                }
            }

            if (n.isIntegratedMic || n.isIntegratedCamera || n.id == "hw-mic" || n.id == "hw-webcam" || n.id == "hw-eye")
            {
                const auto devRow = MainComponent::getIntegratedDeviceRowBounds(box);
                if (devRow.contains(worldPt))
                {
                    if (n.isIntegratedMic || n.id == "hw-mic")
                    {
                        comp.micDropdownOpen = !comp.micDropdownOpen;
                        comp.camDropdownOpen = false;
                    }
                    else
                    {
                        comp.camDropdownOpen = !comp.camDropdownOpen;
                        comp.micDropdownOpen = false;
                    }
                    comp.repaint();
                    return;
                }
            }

            if (n.isIntegratedMic || n.id == "hw-mic")
            {
                juce::Rectangle<float> slTrack(box.getX() + 12.0f, box.getY() + 68.0f, box.getWidth() - 24.0f, 14.0f);
                if (slTrack.expanded(0.0f, 6.0f).contains(worldPt))
                {
                    comp.dragMode = MainComponent::DragMode::DragCanvasMicSensitivity;
                    comp.micSensitivity = juce::jlimit(0.05f, 0.95f, (worldPt.x - slTrack.getX()) / slTrack.getWidth());
                    comp.repaint();
                    return;
                }

                juce::Rectangle<float> trigRow(box.getX() + 12.0f, box.getY() + 102.0f, box.getWidth() - 24.0f, 26.0f);
                juce::Rectangle<float> testBtn(trigRow.getRight() - 65.0f, trigRow.getY() + 3.0f, 40.0f, 20.0f);
                if (testBtn.contains(worldPt))
                {
                    comp.lastTriggerSourceCode.store(2);
                    comp.triggerPulseAlpha.store(1.0f);
                    comp.synthHitVelocity.store(0.85f);
                    comp.synthInstrumentType.store(0);
                    comp.synthTriggerPending.store(true);
                    comp.repaint();
                    return;
                }
            }
            else if (n.id == "hw-drum")
            {
                float my = box.getY() + 46.0f + static_cast<float>(n.modOutputs.size()) * 32.0f + 4.0f;
                juce::Rectangle<float> testBtn(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 24.0f);
                if (testBtn.contains(worldPt))
                {
                    comp.lastTriggerSourceCode.store(1);
                    comp.triggerPulseAlpha.store(1.0f);
                    comp.synthHitVelocity.store(0.9f);
                    comp.synthInstrumentType.store(0);
                    comp.synthTriggerPending.store(true);
                    comp.repaint();
                    return;
                }
            }

            // Click su ON/OFF e freccette accordion delle uscite
            if (!n.outputGroups.empty())
            {
                float curY = box.getY() + 168.0f;
                for (size_t gIdx = 0; gIdx < n.outputGroups.size(); ++gIdx)
                {
                    auto& grp = n.outputGroups[gIdx];
                    if (!grp.parentGroup.isEmpty())
                        continue;

                    const bool hasSubgroups = grp.items.empty();
                    const juce::Rectangle<float> hdrBox(box.getX() + 8.0f, curY, box.getWidth() - 16.0f, 24.0f);

                    const juce::Rectangle<float> switchBox(hdrBox.getX() + 5.0f, hdrBox.getCentreY() - 7.0f, 29.0f, 14.0f);
                    if (switchBox.contains(worldPt))
                    {
                        grp.isEnabled = !grp.isEnabled;
                        comp.updateHardwareDeviceStates();
                        comp.repaint();
                        return;
                    }

                    const juce::Rectangle<float> chevBox(switchBox.getRight() + 4.0f, hdrBox.getY(), 13.0f, hdrBox.getHeight());
                    if (chevBox.contains(worldPt) || (hdrBox.contains(worldPt) && !switchBox.contains(worldPt)))
                    {
                        grp.isExpanded = !grp.isExpanded;
                        n.size.y = CanvasGeometry::getNodeCalculatedHeight(n);
                        comp.repaint();
                        return;
                    }

                    curY += 26.0f;

                    if (hasSubgroups)
                    {
                        if (grp.isExpanded)
                        {
                            for (size_t sIdx = 0; sIdx < n.outputGroups.size(); ++sIdx)
                            {
                                auto& sub = n.outputGroups[sIdx];
                                if (sub.parentGroup != grp.id)
                                    continue;

                                const juce::Rectangle<float> subHdrBox(box.getX() + 14.0f, curY, box.getWidth() - 28.0f, 22.0f);
                                const juce::Rectangle<float> subSwitchBox(subHdrBox.getX() + 4.0f, subHdrBox.getCentreY() - 6.0f, 25.0f, 12.0f);

                                if (subSwitchBox.contains(worldPt))
                                {
                                    sub.isEnabled = !sub.isEnabled;
                                    comp.updateHardwareDeviceStates();
                                    comp.repaint();
                                    return;
                                }

                                if (subHdrBox.contains(worldPt))
                                {
                                    sub.isExpanded = !sub.isExpanded;
                                    n.size.y = CanvasGeometry::getNodeCalculatedHeight(n);
                                    comp.repaint();
                                    return;
                                }

                                curY += 23.0f;

                                if (sub.isExpanded)
                                {
                                    for (const auto& item : sub.items)
                                    {
                                        juce::Rectangle<float> row(box.getX() + 20.0f, curY, box.getWidth() - 36.0f, 22.0f);
                                        juce::Rectangle<float> discBtn(row.getRight() - 40.0f, row.getY() + 3.0f, 16.0f, 16.0f);
                                        if (discBtn.contains(worldPt))
                                        {
                                            comp.cables.erase(
                                                std::remove_if(comp.cables.begin(), comp.cables.end(),
                                                    [&](const CanvasCable& c) {
                                                        return c.sourceNodeId == n.id && c.sourcePinId == item.pinId;
                                                    }),
                                                comp.cables.end()
                                            );
                                            comp.syncAudioRoutes();
                                            comp.repaint();
                                            return;
                                        }
                                        curY += 24.0f;
                                    }
                                }
                            }
                        }
                    }
                    else
                    {
                        if (grp.isExpanded)
                        {
                            for (const auto& item : grp.items)
                            {
                                juce::Rectangle<float> row(box.getX() + 14.0f, curY, box.getWidth() - 28.0f, 22.0f);
                                juce::Rectangle<float> discBtn(row.getRight() - 40.0f, row.getY() + 3.0f, 16.0f, 16.0f);
                                if (discBtn.contains(worldPt))
                                {
                                    comp.cables.erase(
                                        std::remove_if(comp.cables.begin(), comp.cables.end(),
                                            [&](const CanvasCable& c) {
                                                return c.sourceNodeId == n.id && c.sourcePinId == item.pinId;
                                            }),
                                        comp.cables.end()
                                    );
                                    comp.syncAudioRoutes();
                                    comp.repaint();
                                    return;
                                }
                                curY += 24.0f;
                            }
                        }
                    }
                }
            }

            // Click sul pulsante [X] di disconnessione sulle righe di modulazione standard
            float my = (n.isIntegratedMic || n.id == "hw-mic") ? (box.getY() + 134.0f) : (box.getY() + 46.0f);
            for (const auto& m : n.modOutputs)
            {
                juce::Rectangle<float> row(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 26.0f);
                juce::Rectangle<float> discBtn(row.getRight() - 40.0f, row.getY() + 4.0f, 18.0f, 18.0f);
                if (discBtn.contains(worldPt))
                {
                    comp.cables.erase(
                        std::remove_if(comp.cables.begin(), comp.cables.end(),
                            [&](const CanvasCable& c) {
                                return c.sourceNodeId == n.id && (c.sourcePinId == m.key || c.sourcePinId == "mod:" + m.key);
                            }),
                        comp.cables.end()
                    );
                    comp.syncAudioRoutes();
                    comp.repaint();
                    return;
                }
                my += 32.0f;
            }
        }
    }

    // 3. Click su un Pin per collegare, staccare o selezionare un Cavo
    const auto allPins = comp.collectAllPins();
    for (const auto& pin : allPins)
    {
        const float snapDist = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond) ? 14.0f : 12.0f;
        if (worldPt.getDistanceFrom(pin.worldPos) <= snapDist)
        {
            if (e.mods.isRightButtonDown())
            {
                comp.cables.erase(
                    std::remove_if(comp.cables.begin(), comp.cables.end(),
                        [&](const CanvasCable& c) {
                            return (c.sourceNodeId == pin.nodeId && c.sourcePinId == pin.pinId) ||
                                   (c.targetNodeId == pin.nodeId && c.targetPinId == pin.pinId);
                        }),
                    comp.cables.end()
                );
                comp.syncAudioRoutes();
                comp.repaint();
                return;
            }

            // Cerca se esiste un cavo connesso a questo pin
            for (auto it = comp.cables.begin(); it != comp.cables.end(); ++it)
            {
                if (it->targetNodeId == pin.nodeId && it->targetPinId == pin.pinId)
                {
                    comp.isUnhookingExistingCable = true;
                    comp.unhookedOriginalCable = *it;
                    comp.unhookDragStartWorld = worldPt;

                    comp.dragMode = MainComponent::DragMode::DrawCable;
                    comp.cableSourceNodeId = it->sourceNodeId;
                    comp.cableSourcePinId = it->sourcePinId;
                    comp.cableSourceIsOutput = true;
                    comp.cableSourceIsMod = it->isModulation;
                    comp.cableSourceIsTrigger = it->isTrigger;
                    comp.cableSourceColour = it->colour;
                    comp.cableStartWorld = comp.getPinWorldPos(it->sourceNodeId, it->sourcePinId);
                    comp.cableCurrentWorld = worldPt;
                    comp.cableFreehandWorld.clear();

                    comp.cables.erase(it);
                    comp.syncAudioRoutes();
                    comp.repaint();
                    return;
                }
            }

            // Nuovo cavo in uscita
            comp.isUnhookingExistingCable = false;
            comp.dragMode = MainComponent::DragMode::DrawCable;
            comp.cableSourceNodeId = pin.nodeId;
            comp.cableSourcePinId = pin.pinId;
            comp.cableSourceIsOutput = pin.isOutput;
            comp.cableSourceIsMod = pin.isMod;
            comp.cableSourceIsTrigger = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
            comp.cableSourceShape = pin.shapeType;
            comp.cableSourceColour = pin.colour;
            comp.cableStartWorld = pin.worldPos;
            comp.cableCurrentWorld = worldPt;
            comp.cableFreehandWorld.clear();
            comp.cableFreehandWorld.push_back(pin.worldPos);
            comp.repaint();
            return;
        }
    }

    // 4. Click su un cavo (seleziona o elimina)
    const auto nearCableId = comp.findAudioCableNear(worldPt, 16.0f);
    if (nearCableId.isNotEmpty())
    {
        if (e.mods.isRightButtonDown())
        {
            comp.cables.erase(
                std::remove_if(comp.cables.begin(), comp.cables.end(),
                    [&](const CanvasCable& c) { return c.id == nearCableId; }),
                comp.cables.end()
            );
            if (comp.selectedCableId == nearCableId)
                comp.selectedCableId.clear();
            comp.syncAudioRoutes();
            comp.repaint();
            return;
        }
        else
        {
            comp.selectedCableId = nearCableId;
            comp.selectedNodeId.clear();
            comp.repaint();
            return;
        }
    }

    // 5. Click su una Manopola Pastello per ruotarla
    const auto knobs = comp.collectCanvasKnobs();
    for (const auto& kh : knobs)
    {
        if (kh.worldBounds.contains(worldPt))
        {
            comp.dragMode = MainComponent::DragMode::TurnKnob;
            comp.draggedNodeId = kh.nodeId;
            comp.draggedParamKey = kh.paramKey;
            comp.dragStartMouse = pt;

            for (const auto& n : comp.nodes)
            {
                if (n.id == kh.nodeId)
                {
                    for (const auto& p : n.directParams)
                    {
                        if (p.key == kh.paramKey)
                        {
                            comp.dragStartKnobNorm = p.getNormalized();
                            break;
                        }
                    }
                    break;
                }
            }
            return;
        }
    }

    // 6. Click su un Nodo per selezionarlo, chiuderlo, testarlo o spostarlo
    for (auto it = comp.nodes.rbegin(); it != comp.nodes.rend(); ++it)
    {
        const auto box = juce::Rectangle<float>(it->pos.x, it->pos.y, it->size.x, it->size.y);
        if (box.contains(worldPt))
        {
            // Tasto Elimina [X]
            const juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
            if (closeBtn.contains(worldPt))
            {
                comp.deleteNode(it->id);
                return;
            }

            // Tasto Bypass [PWR]
            if (it->type == NodeType::NestedEffect)
            {
                const juce::Rectangle<float> pwrBtn(box.getRight() - 48.0f, box.getY() + 8.0f, 18.0f, 18.0f);
                if (pwrBtn.contains(worldPt))
                {
                    it->isBypassed = !it->isBypassed;
                    comp.syncAudioRoutes();
                    comp.repaint();
                    return;
                }

                const juce::Rectangle<float> expBtn(box.getRight() - 71.0f, box.getY() + 8.0f, 18.0f, 18.0f);
                if (expBtn.contains(worldPt))
                {
                    comp.expandedNodeId = it->id;
                    comp.repaint();
                    return;
                }
            }

            // Tasto Test Suono Instrument
            if (it->type == NodeType::InstrumentNode)
            {
                const juce::Rectangle<float> testBtn(box.getX() + 12.0f, box.getY() + 42.0f, box.getWidth() - 24.0f, 20.0f);
                if (testBtn.contains(worldPt))
                {
                    comp.selectedNodeId = it->id;
                    comp.lastTriggerSourceCode.store(4);
                    comp.triggerPulseAlpha.store(1.0f);

                    int voiceId = 0;
                    if (it->id.contains("melodic")) voiceId = 4;
                    else if (it->id.contains("pluck")) voiceId = 6;
                    else if (it->id.contains("drum")) voiceId = 0;
                    comp.audioEngine.triggerInstrument(it->id, voiceId, 0.90f);

                    comp.repaint();
                    return;
                }
            }

            // Selezione e Spostamento Nodo
            comp.selectedNodeId = it->id;
            comp.selectedCableId.clear();
            comp.syncAudioRoutes();

            comp.dragMode = MainComponent::DragMode::MoveNode;
            comp.draggedNodeId = it->id;
            comp.dragStartMouse = pt;
            comp.dragStartOffset = it->pos;
            comp.repaint();
            return;
        }
    }

    // 7. Click su spazio vuoto: Pan libero (o selezione multipla lasso)
    comp.selectedNodeId.clear();
    comp.selectedCableId.clear();
    comp.syncAudioRoutes();

    comp.dragMode = MainComponent::DragMode::PanCanvas;
    comp.dragStartMouse = pt;
    comp.dragStartOffset = comp.canvasOffset;
    comp.repaint();
}

void CanvasInteraction::handleMouseDoubleClick(MainComponent& comp, const juce::MouseEvent& e)
{
    const auto worldPt = comp.screenToWorld(e.position);

    for (const auto& n : comp.nodes)
    {
        const auto box = juce::Rectangle<float>(n.pos.x, n.pos.y, n.size.x, n.size.y);
        if (box.contains(worldPt))
        {
            if (n.type == NodeType::NestedEffect && !n.innerBlocks.empty())
            {
                comp.expandedNodeId = n.id;
                comp.repaint();
                return;
            }
        }
    }
}

void CanvasInteraction::handleMouseWheelMove(MainComponent& comp, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const auto pt = e.position;
    const auto worldBefore = comp.screenToWorld(pt);

    // Zoom fluido continuo con rotella del mouse
    const float zoomFactor = (wheel.deltaY > 0.0f) ? 1.08f : (1.0f / 1.08f);
    const float newZoom = juce::jlimit(0.20f, 3.50f, comp.canvasZoom * zoomFactor);

    comp.canvasOffset = pt - worldBefore * newZoom;
    comp.canvasZoom = newZoom;
    comp.timeSinceLastWheel = 0.0f;
    comp.repaint();
}

void CanvasInteraction::handleMouseMagnify(MainComponent& comp, const juce::MouseEvent& e, float scaleFactor)
{
    const auto pt = e.position;
    const auto worldBefore = comp.screenToWorld(pt);
    const float newZoom = juce::jlimit(0.20f, 3.50f, comp.canvasZoom * scaleFactor);

    comp.canvasOffset = pt - worldBefore * newZoom;
    comp.canvasZoom = newZoom;
    comp.repaint();
}

bool CanvasInteraction::handleKeyPressed(MainComponent& comp, const juce::KeyPress& key)
{
    if (comp.hasPendingPolaroid)
    {
        if (key == juce::KeyPress::returnKey || key == juce::KeyPress::escapeKey)
        {
            comp.commitPolaroidToAlbum(comp.pendingPolaroid);
            return true;
        }
        if (key == juce::KeyPress::backspaceKey)
        {
            if (!comp.polaroidHasTyped)
            {
                comp.polaroidHasTyped = true;
                comp.pendingPolaroid.title.clear();
            }
            else
            {
                comp.pendingPolaroid.title = comp.pendingPolaroid.title.dropLastCharacters(1);
            }
            comp.repaint();
            return true;
        }

        const juce::juce_wchar ch = key.getTextCharacter();
        if (ch >= 32 && ch < 127)
        {
            if (!comp.polaroidHasTyped)
            {
                comp.polaroidHasTyped = true;
                comp.pendingPolaroid.title.clear();
            }
            if (comp.pendingPolaroid.title.length() < 26)
                comp.pendingPolaroid.title += ch;
            comp.repaint();
            return true;
        }
    }

    // Scorciatoia Modalita Notte ('N' o 'n')
    if (key.getTextCharacter() == 'n' || key.getTextCharacter() == 'N')
    {
        comp.toggleDarkMode();
        return true;
    }

    // Tasti Hardware 1..5 (Selezione ed Esecuzione Scene o Assegnazione Rapida)
    const juce::juce_wchar kch = key.getTextCharacter();
    if (kch >= '1' && kch <= '5')
    {
        const int sIdx = kch - '1';
        if (comp.activeTab == MainComponent::ActiveTab::Live)
        {
            if (comp.selectedAlbumIndex >= 0 && comp.selectedAlbumIndex < static_cast<int>(comp.photoAlbum.size()))
            {
                comp.assignPolaroidToSlot(sIdx, comp.photoAlbum[static_cast<size_t>(comp.selectedAlbumIndex)]);
                comp.selectedAlbumIndex = -1;
            }
            else
            {
                comp.triggerSceneSlot(sIdx);
            }
            return true;
        }
        if (comp.activeTab == MainComponent::ActiveTab::Studio && comp.editingSlotIndex == sIdx)
        {
            comp.sceneSlots[comp.editingSlotIndex].snapshot = { comp.nodes, comp.cables };
            comp.editingSlotIndex = -1;
            comp.activeTab = MainComponent::ActiveTab::Live;
            comp.repaint();
            return true;
        }
    }

    // Tasto Spazio: 1-Button Looper (REC / PLAY / OVERDUB)
    if (key == juce::KeyPress::spaceKey && comp.activeTab == MainComponent::ActiveTab::Live)
    {
        comp.toggleLooper();
        return true;
    }

    // Eliminazione nodo, cavo o pin con tasto CANC (Delete o Backspace)
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (comp.selectedCableId.isNotEmpty())
        {
            comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
                [&](const CanvasCable& c) { return c.id == comp.selectedCableId; }),
                comp.cables.end());
            comp.selectedCableId.clear();
            comp.syncAudioRoutes();
            comp.repaint();
            return true;
        }
        if (comp.selectedPinNodeId.isNotEmpty())
        {
            comp.cables.erase(std::remove_if(comp.cables.begin(), comp.cables.end(),
                [&](const CanvasCable& c) {
                    return (c.sourceNodeId == comp.selectedPinNodeId && c.sourcePinId == comp.selectedPinId) ||
                           (c.targetNodeId == comp.selectedPinNodeId && c.targetPinId == comp.selectedPinId);
                }),
                comp.cables.end());
            comp.selectedPinNodeId.clear();
            comp.selectedPinId.clear();
            comp.syncAudioRoutes();
            comp.repaint();
            return true;
        }
        if (comp.selectedNodeId.isNotEmpty())
        {
            comp.deleteNode(comp.selectedNodeId);
            return true;
        }
    }

    if (key == juce::KeyPress::escapeKey)
    {
        if (comp.eyeDrawerOpen)
        {
            comp.eyeDrawerOpen = false;
            comp.repaint();
            return true;
        }
        if (comp.editingSlotIndex >= 0)
        {
            comp.sceneSlots[comp.editingSlotIndex].snapshot = { comp.nodes, comp.cables };
            comp.editingSlotIndex = -1;
            comp.activeTab = MainComponent::ActiveTab::Live;
            comp.repaint();
            return true;
        }
        if (comp.selectedAlbumIndex >= 0)
        {
            comp.selectedAlbumIndex = -1;
            comp.repaint();
            return true;
        }
        if (comp.expandedNodeId.isNotEmpty())
        {
            comp.expandedNodeId.clear();
            comp.repaint();
            return true;
        }
        if (comp.selectedNodeId.isNotEmpty() || comp.selectedCableId.isNotEmpty() || comp.selectedPinNodeId.isNotEmpty())
        {
            comp.selectedNodeId.clear();
            comp.selectedCableId.clear();
            comp.selectedPinNodeId.clear();
            comp.selectedPinId.clear();
            comp.syncAudioRoutes();
            comp.repaint();
            return true;
        }
    }

    return false;
}

} // namespace mimo
