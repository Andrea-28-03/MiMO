#include "CanvasRenderer.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>

namespace mimo
{

void CanvasRenderer::paintStudioCanvas(juce::Graphics& g, MainComponent& comp)
{
    juce::Graphics::ScopedSaveState state(g);
    g.addTransform(juce::AffineTransform::scale(comp.canvasZoom).translated(comp.canvasOffset.x, comp.canvasOffset.y));

    const auto allPins = comp.collectAllPins();

    // 1. Disegna tutti i Cavi con evidenziazione selettiva
    paintCables(g, comp, allPins);

    // 2. Cavo in fase di disegno a mano libera
    paintLiveCable(g, comp);

    // 3. Disegna tutti i Nodi del Canvas
    for (const auto& node : comp.nodes)
        paintCanvasNode(g, comp, node);

    // 4. Disegna i Pin di Connessione con forme, color coding e animazioni
    paintPins(g, comp, allPins);

    // 5. Tendine interattive per la selezione diretta di Webcam / Microfono
    paintCanvasDropdowns(g, comp);
}

void CanvasRenderer::paintCables(juce::Graphics& g, MainComponent& comp, const std::vector<PinHit>& allPins)
{
    auto findPinInfo = [&allPins](const juce::String& nId, const juce::String& pId, juce::Point<float>& outPos, juce::Point<float>& outNorm)
    {
        for (const auto& p : allPins)
        {
            if (p.nodeId == nId && p.pinId == pId)
            {
                outPos = p.worldPos;
                outNorm = p.normal;
                return;
            }
        }
        outPos = {};
        outNorm = {};
    };

    const int trigSrc = comp.lastTriggerSourceCode.load();
    const float pulseVal = comp.triggerPulseAlpha.load();
    const bool isPulse = (pulseVal > 0.08f);

    for (size_t i = 0; i < comp.cables.size(); ++i)
    {
        const auto& c = comp.cables[i];
        juce::Point<float> p1, p2, n1, n2;
        findPinInfo(c.sourceNodeId, c.sourcePinId, p1, n1);
        findPinInfo(c.targetNodeId, c.targetPinId, p2, n2);
        const bool isHover = (c.id == comp.hoveredAudioCableId || c.id == comp.selectedCableId);
        const bool isTrigCable = (c.isTrigger || c.sourcePinId.startsWith("trigger:") || c.targetPinId.startsWith("trigger:"));

        bool cableMatches = false;
        if (isPulse)
        {
            if (trigSrc == 1) // hw-drum
            {
                if (c.sourceNodeId == "hw-drum" || (c.sourceNodeId == "rockpi" && c.sourcePinId == "audio:p1"))
                    cableMatches = true;
            }
            else if (trigSrc == 2) // hw-mic
            {
                if (c.sourceNodeId == "hw-mic" || (c.sourceNodeId == "rockpi" && c.sourcePinId == "audio:p4"))
                    cableMatches = true;
            }
            else if (trigSrc == 3) // hw-eye
            {
                if (c.sourceNodeId == "hw-eye" || c.sourceNodeId == "hw-webcam" || (c.sourceNodeId == "rockpi" && c.sourcePinId == "audio:p2"))
                {
                    const int fIdx = comp.lastTriggerFingerIdx.load();
                    const bool isSec = comp.lastTriggerIsSecondary.load();
                    if (fIdx >= 0)
                    {
                        if (!isSec)
                        {
                            if (fIdx == 0 && (c.sourcePinId == "trigger:dx_pinch_idx" || c.sourcePinId == "trigger:out")) cableMatches = true;
                            else if (fIdx == 1 && c.sourcePinId == "trigger:dx_pinch_mid") cableMatches = true;
                            else if (fIdx == 2 && c.sourcePinId == "trigger:dx_pinch_rng") cableMatches = true;
                            else if (fIdx == 3 && c.sourcePinId == "trigger:dx_pinch_pky") cableMatches = true;
                        }
                        else
                        {
                            if (fIdx == 0 && (c.sourcePinId == "trigger:sx_pinch_idx" || c.sourcePinId == "trigger:out")) cableMatches = true;
                            else if (fIdx == 1 && c.sourcePinId == "trigger:sx_pinch_mid") cableMatches = true;
                            else if (fIdx == 2 && c.sourcePinId == "trigger:sx_pinch_rng") cableMatches = true;
                            else if (fIdx == 3 && c.sourcePinId == "trigger:sx_pinch_pky") cableMatches = true;
                        }
                    }
                    else
                    {
                        cableMatches = true;
                    }
                }
            }
            else if (trigSrc == 4) // instrument
            {
                if (c.targetNodeId == comp.selectedNodeId || c.sourceNodeId == comp.selectedNodeId)
                    cableMatches = true;
            }
        }

        juce::Colour cableColor = c.colour;
        if (!c.isModulation && !isTrigCable)
        {
            for (const auto& n : comp.nodes)
            {
                if (n.id == c.sourceNodeId && (n.type == NodeType::NestedEffect || n.type == NodeType::InstrumentNode))
                {
                    cableColor = n.crayonColour;
                    break;
                }
            }
        }

        MiMOLookAndFeel::drawInkCable(g, p1, p2, c.freehandNorm,
                                      static_cast<int>(i) * 37 + 11, comp.boilTick,
                                      c.isModulation,
                                      cableColor,
                                      isHover, false,
                                      n1, n2,
                                      isTrigCable,
                                      (cableMatches && isTrigCable) ? pulseVal : 0.0f);
    }
}

void CanvasRenderer::paintLiveCable(juce::Graphics& g, MainComponent& comp)
{
    if (comp.dragMode == MainComponent::DragMode::DrawCable)
    {
        juce::Path livePath;
        livePath.startNewSubPath(comp.cableStartWorld);
        for (const auto& pt : comp.cableFreehandWorld)
            livePath.lineTo(pt);
        livePath.lineTo(comp.cableCurrentWorld);

        if (comp.cableSourceIsTrigger)
        {
            g.setColour(juce::Colour(0xFFFF7A18).withAlpha(0.9f));
            g.strokePath(livePath, juce::PathStrokeType(4.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (comp.cableSourceIsMod)
        {
            g.setColour(comp.cableSourceColour.withAlpha(0.85f));
            g.strokePath(livePath, juce::PathStrokeType(5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (comp.cableSourceColour != MiMOLookAndFeel::fountainInk && comp.cableSourceColour != juce::Colours::transparentBlack)
        {
            g.setColour(comp.cableSourceColour.withAlpha(0.85f));
            g.strokePath(livePath, juce::PathStrokeType(4.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.strokePath(livePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void CanvasRenderer::paintPins(juce::Graphics& g, MainComponent& comp, const std::vector<PinHit>& allPins)
{
    const int trigSrc = comp.lastTriggerSourceCode.load();
    const float pulseVal = comp.triggerPulseAlpha.load();
    const bool isPulse = (pulseVal > 0.08f);

    for (const auto& pin : allPins)
    {
        const float r = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond) ? 7.5f : (pin.isMod ? 6.5f : 6.0f);
        const bool isHovered = (comp.hasHoveredPin && comp.hoveredPinNodeId == pin.nodeId && comp.hoveredPinId == pin.pinId);
        const bool isSelected = (comp.selectedPinNodeId == pin.nodeId && comp.selectedPinId == pin.pinId);

        const bool pinIsTrig = (pin.shapeType == MiMOLookAndFeel::PinShapeType::TriggerDiamond || pin.pinId.startsWith("trigger:"));
        const bool typeCompatible = comp.cableSourceIsTrigger ? pinIsTrig : (comp.cableSourceIsMod ? pin.isMod : (!pin.isMod && !pinIsTrig));

        const bool isValidTarget = (comp.dragMode == MainComponent::DragMode::DrawCable && pin.nodeId != comp.cableSourceNodeId &&
                                    (pin.isOutput != comp.cableSourceIsOutput) && typeCompatible);

        bool pinMatches = false;
        if (isPulse)
        {
            if (trigSrc == 1 && pin.nodeId == "hw-drum") pinMatches = true;
            else if (trigSrc == 2 && (pin.nodeId == "hw-mic")) pinMatches = true;
            else if (trigSrc == 3 && (pin.nodeId == "hw-eye" || pin.nodeId == "hw-webcam"))
            {
                const int fIdx = comp.lastTriggerFingerIdx.load();
                const bool isSec = comp.lastTriggerIsSecondary.load();
                if (fIdx >= 0)
                {
                    if (!isSec)
                    {
                        if (fIdx == 0 && pin.pinId == "trigger:dx_pinch_idx") pinMatches = true;
                        else if (fIdx == 1 && pin.pinId == "trigger:dx_pinch_mid") pinMatches = true;
                        else if (fIdx == 2 && pin.pinId == "trigger:dx_pinch_rng") pinMatches = true;
                        else if (fIdx == 3 && pin.pinId == "trigger:dx_pinch_pky") pinMatches = true;
                    }
                    else
                    {
                        if (fIdx == 0 && pin.pinId == "trigger:sx_pinch_idx") pinMatches = true;
                        else if (fIdx == 1 && pin.pinId == "trigger:sx_pinch_mid") pinMatches = true;
                        else if (fIdx == 2 && pin.pinId == "trigger:sx_pinch_rng") pinMatches = true;
                        else if (fIdx == 3 && pin.pinId == "trigger:sx_pinch_pky") pinMatches = true;
                    }
                }
                else
                {
                    pinMatches = true;
                }
            }
        }
        const bool isFiring = (pinIsTrig && pinMatches && isPulse);

        MiMOLookAndFeel::drawPinJack(g, pin.worldPos, r, pin.shapeType, pin.colour,
                                     isHovered, isSelected, isValidTarget, isFiring);
    }

    // 5. Riquadro di Selezione Multipla Lasso Pastello (Task 1.1)
    if (comp.dragMode == MainComponent::DragMode::LassoSelection)
    {
        const auto lasso = comp.getLassoWorldRect();
        g.setColour(MiMOLookAndFeel::crayonCyan.withAlpha(0.18f));
        g.fillRect(lasso);

        g.setColour(MiMOLookAndFeel::crayonCyan.withAlpha(0.85f));
        const float dashPattern[2] = { 6.0f, 4.0f };
        g.drawDashedLine(juce::Line<float>(lasso.getTopLeft(), lasso.getTopRight()), dashPattern, 2, 1.6f);
        g.drawDashedLine(juce::Line<float>(lasso.getTopRight(), lasso.getBottomRight()), dashPattern, 2, 1.6f);
        g.drawDashedLine(juce::Line<float>(lasso.getBottomRight(), lasso.getBottomLeft()), dashPattern, 2, 1.6f);
        g.drawDashedLine(juce::Line<float>(lasso.getBottomLeft(), lasso.getTopLeft()), dashPattern, 2, 1.6f);
    }
}

void CanvasRenderer::paintCanvasDropdowns(juce::Graphics& g, MainComponent& comp)
{
    if (!comp.camDropdownOpen && !comp.micDropdownOpen)
        return;

    for (const auto& node : comp.nodes)
    {
        if (node.type != NodeType::HardwareModule)
            continue;

        const bool isMicNode = (node.isIntegratedMic || node.id == "hw-mic");
        const bool isCamNode = (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye");
        if (isMicNode && !comp.micDropdownOpen)
            continue;
        if (isCamNode && !comp.camDropdownOpen)
            continue;

        const juce::Rectangle<float> box(node.pos.x, node.pos.y, node.size.x, node.size.y);
        const auto devRow = MainComponent::getIntegratedDeviceRowBounds(box);
        const juce::StringArray options = isMicNode ? comp.getMicDropdownOptions() : comp.getCameraDropdownOptions();
        if (options.isEmpty())
            continue;

        const float itemH = 22.0f;
        const float totalH = static_cast<float>(options.size()) * itemH + 6.0f;
        const juce::Rectangle<float> dropdownBox(devRow.getX(), devRow.getBottom() + 3.0f, devRow.getWidth(), totalH);

        // Ombra sotto la tendina
        g.setColour(juce::Colours::black.withAlpha(comp.isDarkMode ? 0.40f : 0.18f));
        g.fillRoundedRectangle(dropdownBox.translated(0.0f, 3.0f), 5.0f);

        // Sfondo tendina
        g.setColour(MiMOLookAndFeel::paperBg);
        g.fillRoundedRectangle(dropdownBox, 4.0f);
        MiMOLookAndFeel::drawSketchBox(g, dropdownBox, 888, MiMOLookAndFeel::crayonCyan, 0.40f,
                                       juce::Colours::transparentBlack, 1.2f, false, MiMOLookAndFeel::paperBg);

        const int currentIdx = isMicNode ? comp.micDeviceIndex.load() : comp.cameraDeviceIndex.load();
        const bool isPhoneActive = isCamNode && comp.useSmartphoneEye;

        for (int i = 0; i < options.size(); ++i)
        {
            const juce::Rectangle<float> itemRect(dropdownBox.getX() + 3.0f, dropdownBox.getY() + 3.0f + static_cast<float>(i) * itemH,
                                                  dropdownBox.getWidth() - 6.0f, itemH);

            bool isSelected = false;
            if (isCamNode)
            {
                if (i == 0) isSelected = !isPhoneActive && (currentIdx == -1);
                else if (i == options.size() - 1) isSelected = isPhoneActive;
                else isSelected = !isPhoneActive && (currentIdx == (i - 1));
            }
            else
            {
                if (i == 0) isSelected = (currentIdx == -1);
                else isSelected = (currentIdx == (i - 1));
            }

            if (isSelected)
            {
                g.setColour(MiMOLookAndFeel::crayonCyan.withAlpha(0.25f));
                g.fillRoundedRectangle(itemRect, 3.0f);
            }

            if (i > 0)
            {
                g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.20f));
                g.drawLine(itemRect.getX() + 4.0f, itemRect.getY(), itemRect.getRight() - 4.0f, itemRect.getY(), 0.8f);
            }

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.5f, isSelected ? juce::Font::bold : juce::Font::plain));
            const juce::String prefix = isSelected ? "* " : "   ";
            g.drawText(prefix + options[i], itemRect.reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
        }
    }
}

void CanvasRenderer::paintNestedModal(juce::Graphics& g, MainComponent& comp, const CanvasNode& node)
{
    g.fillAll(MiMOLookAndFeel::fountainInk.withAlpha(0.58f));

    const auto modal = comp.getLocalBounds().toFloat().reduced(44.0f, 36.0f);
    MiMOLookAndFeel::drawSketchBox(g, modal, 600, node.crayonColour, 0.16f,
                                   juce::Colours::transparentBlack, 2.5f, false, MiMOLookAndFeel::paperBg);

    // Header Modale
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("ARCHITETTURA INTERNA (NODO NESTED FAUST DSP): " + node.label,
               static_cast<int>(modal.getX() + 24), static_cast<int>(modal.getY() + 16), 720, 26, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(13.5f, juce::Font::plain));
    g.drawText("Clicca su una manopola interna per esporla/nasconderla ([*] Esposto) sul nodo genitore nel Canvas STUDIO, o trascinala in verticale!",
               static_cast<int>(modal.getX() + 24), static_cast<int>(modal.getY() + 42), 820, 20, juce::Justification::centredLeft);

    // Pulsante Chiudi
    juce::Rectangle<float> closeBtn(modal.getRight() - 210.0f, modal.getY() + 18.0f, 186.0f, 38.0f);
    MiMOLookAndFeel::drawSketchBox(g, closeBtn, 601, MiMOLookAndFeel::crayonCherry, 0.78f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText("<- Torna al Canvas STUDIO", closeBtn.toNearestInt(), juce::Justification::centred);

    // Nodo Group Input a sinistra
    juce::Rectangle<float> groupIn(modal.getX() + 30.0f, modal.getY() + 160.0f, 190.0f, 140.0f);
    MiMOLookAndFeel::drawSketchBox(g, groupIn, 602, MiMOLookAndFeel::crayonOchre, 0.48f, juce::Colours::transparentBlack, 2.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText("INGRESSO NODO", groupIn.reduced(10, 8).removeFromTop(22).toNearestInt(), juce::Justification::centredLeft);
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("- Segnale Audio In\n- [*] Esponi Parametro\n  (collegato ai knob [*])", groupIn.reduced(12, 34).toNearestInt(), juce::Justification::topLeft);

    // Nodo Group Output a destra
    juce::Rectangle<float> groupOut(modal.getRight() - 190.0f, modal.getY() + 180.0f, 155.0f, 100.0f);
    MiMOLookAndFeel::drawSketchBox(g, groupOut, 603, MiMOLookAndFeel::crayonSage, 0.48f, juce::Colours::transparentBlack, 2.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText("USCITA NODO", groupOut.reduced(10, 8).removeFromTop(22).toNearestInt(), juce::Justification::centredLeft);
    g.setFont(juce::Font(12.0f, juce::Font::plain));
    g.drawText("- Audio Out Processato", groupOut.reduced(12, 34).toNearestInt(), juce::Justification::topLeft);

    // Disegna i blocchi Faust interni
    for (const auto& b : node.innerBlocks)
    {
        const float bw = std::max(210.0f, static_cast<float>(b.params.size()) * 95.0f + 30.0f);
        const juce::Rectangle<float> bRect(modal.getX() + b.pos.x, modal.getY() + b.pos.y, bw, 195.0f);

        MiMOLookAndFeel::drawSketchBox(g, bRect, b.id.hashCode(), node.crayonColour, 0.35f, juce::Colours::transparentBlack, 1.8f);

        // Header blocco
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(b.label, static_cast<int>(bRect.getX() + 12), static_cast<int>(bRect.getY() + 8), static_cast<int>(bw - 24), 18, juce::Justification::centredLeft);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(11.0f, juce::Font::plain));
        g.drawText("Faust DSP Block", static_cast<int>(bRect.getX() + 12), static_cast<int>(bRect.getY() + 26), static_cast<int>(bw - 24), 14, juce::Justification::centredLeft);

        // Manopole dei parametri interni
        const float slotW = (bw - 20.0f) / static_cast<float>(std::max(1, static_cast<int>(b.params.size())));
        for (size_t pi = 0; pi < b.params.size(); ++pi)
        {
            const auto& p = b.params[pi];
            const juce::Rectangle<float> kb(bRect.getX() + 10.0f + static_cast<float>(pi) * slotW,
                                            bRect.getY() + 46.0f, slotW, 88.0f);

            MiMOLookAndFeel::drawCrayonKnob(
                g, kb, p.getNormalized(), node.crayonColour,
                p.label, p.getFormattedValue(),
                (node.id + b.id + p.key).hashCode()
            );

            // Pulsante per esporre/nascondere il knob sul canvas padre
            const juce::Rectangle<float> expToggle(kb.getX() + 6.0f, kb.getBottom() + 2.0f, kb.getWidth() - 12.0f, 20.0f);
            MiMOLookAndFeel::drawSketchBox(g, expToggle, 700 + static_cast<int>(pi),
                                           p.isExposed ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                           p.isExposed ? 0.70f : 0.40f, juce::Colours::transparentBlack, 1.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.0f, p.isExposed ? juce::Font::bold : juce::Font::plain));
            g.drawText(p.isExposed ? "[*] Esposto" : "[ ] Nascosto", expToggle.toNearestInt(), juce::Justification::centred);
        }
    }
}

void CanvasRenderer::paintMarqueeSelection(juce::Graphics& g, MainComponent& comp)
{
    // Placeholder per Fase 1.1: Selezione Multipla (Lasso / Marquee Selection)
    // Box pastello tratteggiato renderizzato quando comp.dragMode == DragMode::MarqueeSelect
    juce::ignoreUnused(g, comp);
}

bool CanvasRenderer::handleNestedModalMouseDown(MainComponent& comp, const juce::Point<float>& pt)
{
    if (comp.expandedNodeId.isEmpty())
        return false;

    const auto modal = comp.getLocalBounds().toFloat().reduced(44.0f, 36.0f);
    juce::Rectangle<float> closeBtn(modal.getRight() - 210.0f, modal.getY() + 18.0f, 186.0f, 38.0f);
    if (closeBtn.contains(pt) || !modal.contains(pt))
    {
        comp.expandedNodeId.clear();
        comp.repaint();
        return true;
    }

    for (auto& n : comp.nodes)
    {
        if (n.id != comp.expandedNodeId)
            continue;

        for (auto& b : n.innerBlocks)
        {
            const float bw = std::max(210.0f, static_cast<float>(b.params.size()) * 95.0f + 30.0f);
            const juce::Rectangle<float> bRect(modal.getX() + b.pos.x, modal.getY() + b.pos.y, bw, 195.0f);
            const float slotW = (bw - 20.0f) / static_cast<float>(std::max(1, static_cast<int>(b.params.size())));

            for (size_t pi = 0; pi < b.params.size(); ++pi)
            {
                auto& p = b.params[pi];
                juce::Rectangle<float> kb(bRect.getX() + 10.0f + static_cast<float>(pi) * slotW,
                                          bRect.getY() + 64.0f, slotW, 88.0f);
                juce::Rectangle<float> expToggle(kb.getX() + 6.0f, kb.getBottom() + 2.0f, kb.getWidth() - 12.0f, 20.0f);

                if (expToggle.contains(pt))
                {
                    p.isExposed = !p.isExposed;
                    comp.repaint();
                    return true;
                }
                if (kb.contains(pt))
                {
                    comp.dragMode = MainComponent::DragMode::TurnInnerKnob;
                    comp.draggedNodeId = n.id;
                    comp.draggedBlockId = b.id;
                    comp.draggedParamKey = p.key;
                    comp.dragStartMouse = pt;
                    comp.dragStartKnobNorm = p.getNormalized();
                    return true;
                }
            }

            if (bRect.contains(pt))
            {
                comp.dragMode = MainComponent::DragMode::MoveInnerBlock;
                comp.draggedNodeId = n.id;
                comp.draggedBlockId = b.id;
                comp.dragStartMouse = pt;
                comp.dragStartOffset = b.pos;
                return true;
            }
        }
    }
    return true;
}

} // namespace mimo
