#include "CanvasRenderer.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include "MiMOEyeDrawer.h"
#include <algorithm>

namespace mimo
{

void CanvasRenderer::paintCanvasNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node)
{
    const juce::Rectangle<float> box(node.pos.x, node.pos.y, node.size.x, node.size.y);
    const int seed = node.id.hashCode();
    const bool isNodeSelected = comp.isNodeSelected(node.id);
    const juce::Colour glow = isNodeSelected ? MiMOLookAndFeel::crayonCherry
                                             : ((node.id == comp.hoveredHwNodeId) ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack);

    if (node.type == NodeType::RockPiCentral)
    {
        paintRockPiNode(g, comp, node, box, seed, isNodeSelected, glow);
    }
    else if (node.type == NodeType::HardwareModule)
    {
        paintHardwareNode(g, comp, node, box, seed, isNodeSelected, glow);
    }
    else if (node.type == NodeType::NestedEffect)
    {
        paintNestedEffectNode(g, comp, node, box, seed, isNodeSelected, glow);
    }
    else if (node.type == NodeType::InstrumentNode)
    {
        paintInstrumentNode(g, comp, node, box, seed, isNodeSelected, glow);
    }
    else if (node.type == NodeType::MasterOut)
    {
        paintMasterOutNode(g, comp, node, box, seed, isNodeSelected, glow);
    }
}

void CanvasRenderer::paintRockPiNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node,
                                     const juce::Rectangle<float>& box, int seed, bool /*isNodeSelected*/, juce::Colour glow)
{
    if (comp.mimoImage.isValid())
    {
        if (node.id == comp.hoveredHwNodeId)
        {
            g.setColour(glow.withAlpha(0.35f));
            g.fillRoundedRectangle(box.expanded(8.0f), 18.0f);
        }

        g.setColour(juce::Colours::black.withAlpha(comp.isDarkMode ? 0.45f : 0.22f));
        g.fillRoundedRectangle(box.translated(0.0f, 6.0f).reduced(2.0f), 16.0f);

        g.setColour(juce::Colour(0xFF1E1E1E));
        g.fillRoundedRectangle(box.reduced(1.0f), 14.0f);

        if (comp.isDarkMode)
        {
            g.setColour(juce::Colours::white.withAlpha(0.15f));
            g.drawRoundedRectangle(box.expanded(1.0f), 16.0f, 1.2f);
        }

        g.setOpacity(1.0f);
        g.drawImage(comp.mimoImage, box, juce::RectanglePlacement::stretchToFit);

        // Badge porte P1..P5 sotto ai tasti
        static constexpr float keyXFractions[5] = { 0.1490f, 0.2958f, 0.4426f, 0.5895f, 0.7349f };
        for (int i = 0; i < 5; ++i)
        {
            const float cx = box.getX() + box.getWidth() * keyXFractions[i];
            const float cy = box.getBottom() + 4.0f;
            const juce::String portPinId = "audio:p" + juce::String(i + 1);

            bool isConnected = false;
            juce::Colour connColour = MiMOLookAndFeel::crayonOchre;
            for (const auto& c : comp.cables)
            {
                if (c.targetNodeId == "rockpi" && c.targetPinId == portPinId)
                {
                    isConnected = true;
                    for (const auto& n : comp.nodes)
                    {
                        if (n.id == c.sourceNodeId)
                        {
                            connColour = n.hasInstrument ? n.crayonColour : MiMOLookAndFeel::crayonOchre;
                            break;
                        }
                    }
                    break;
                }
            }

            g.setColour(isConnected ? connColour : MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText("P" + juce::String(i + 1), static_cast<int>(cx - 12), static_cast<int>(cy + 8), 24, 12, juce::Justification::centred);
        }

        // Badge OUT sul lato destro
        const float outX = box.getRight() + 4.0f;
        const float outY = box.getY() + box.getHeight() * 0.52f;
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("OUT", static_cast<int>(outX - 12), static_cast<int>(outY + 8), 24, 12, juce::Justification::centred);
    }
    else
    {
        MiMOLookAndFeel::drawSketchBox(g, box, seed, MiMOLookAndFeel::crayonOchre, 0.25f, glow, 2.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText("MiMO Central Hub", box.reduced(10).toNearestInt(), juce::Justification::centred);
    }
}

void CanvasRenderer::paintHardwareNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node,
                                       const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow)
{
    g.setColour(MiMOLookAndFeel::paperCard);
    g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

    const juce::Colour fillCol = node.hasInstrument ? node.crayonColour : juce::Colours::transparentBlack;
    MiMOLookAndFeel::drawSketchBox(g, box, seed, fillCol, node.hasInstrument ? 0.72f : 0.0f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

    // Presa fisica superiore verso MiMO
    juce::Rectangle<float> plug(box.getX() + 18.0f, box.getY() - 4.0f, 22.0f, 8.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.fillRoundedRectangle(plug, 2.0f);

    // Badge Porta
    juce::Rectangle<float> badge(box.getX() + 12.0f, box.getY() + 10.0f, 34.0f, 22.0f);
    const juce::Colour badgeCol = (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
        ? MiMOLookAndFeel::crayonCyan
        : MiMOLookAndFeel::crayonOchre;
    MiMOLookAndFeel::drawSketchBox(g, badge, seed + 1, badgeCol, 0.45f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(11.5f, juce::Font::bold));
    g.drawText(node.portBadge, badge.toNearestInt(), juce::Justification::centred);

    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText(node.label, static_cast<int>(box.getX() + 52), static_cast<int>(box.getY() + 8), 135, 20, juce::Justification::centredLeft);
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText(node.subtitle, static_cast<int>(box.getX() + 52), static_cast<int>(box.getY() + 25), 140, 16, juce::Justification::centredLeft);

    // Pulsante di eliminazione [X] per TUTTI i moduli hardware
    juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
    MiMOLookAndFeel::drawSketchBox(g, closeBtn, seed + 99, MiMOLookAndFeel::crayonCherry, 0.40f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

    // Modulo Occhio / Webcam pulsanti QR e VID
    if (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
    {
        const auto cfgBtn = MiMOEyeDrawer::getEyeCardConfigButtonBounds(box);
        MiMOLookAndFeel::drawSketchBox(g, cfgBtn, seed + 98, MiMOLookAndFeel::crayonCyan, 0.50f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText("QR", cfgBtn.toNearestInt(), juce::Justification::centred);

        const auto vidBtn = MiMOEyeDrawer::getEyeCardViewToggleButtonBounds(box);
        MiMOLookAndFeel::drawSketchBox(g, vidBtn, seed + 97,
                                       comp.eyeShowVideo ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                       comp.eyeShowVideo ? 0.60f : 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(8.0f, juce::Font::bold));
        g.drawText(comp.eyeShowVideo ? "VID" : "SKEL", vidBtn.toNearestInt(), juce::Justification::centred);
    }

    if (node.isIntegratedMic || node.id == "hw-mic")
    {
        // VU Meter Microfono reale
        juce::Rectangle<float> vuBox(box.getX() + 12.0f, box.getY() + 46.0f, box.getWidth() - 24.0f, 18.0f);
        MiMOLookAndFeel::drawSketchBox(g, vuBox, seed + 101, comp.micTriggerFired ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::paperBg,
                                       comp.micTriggerFired ? 0.75f : 0.40f, juce::Colours::transparentBlack, 1.2f);
        if (comp.micTriggerFired)
        {
            g.setColour(MiMOLookAndFeel::polaroidWhite);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText("TRIG! (COLPO RILEVATO)", vuBox.toNearestInt(), juce::Justification::centred);
        }
        else
        {
            const float curLevel = comp.realMicLevel.load();
            const float lvlW = (vuBox.getWidth() - 4.0f) * juce::jlimit(0.0f, 1.0f, curLevel);
            if (lvlW > 1.0f)
            {
                g.setColour(curLevel > 0.6f ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::crayonSage);
                g.fillRect(vuBox.getX() + 2.0f, vuBox.getY() + 2.0f, lvlW, vuBox.getHeight() - 4.0f);
            }
            const float thX = vuBox.getX() + 2.0f + (vuBox.getWidth() - 4.0f) * comp.getMicThreshold();
            g.setColour(MiMOLookAndFeel::crayonCherry);
            g.drawLine(thX, vuBox.getY() + 1.0f, thX, vuBox.getBottom() - 1.0f, 1.5f);
        }

        // Slider sensibilità microfono
        juce::Rectangle<float> slTrack(box.getX() + 12.0f, box.getY() + 68.0f, box.getWidth() - 24.0f, 14.0f);
        MiMOLookAndFeel::drawSketchBox(g, slTrack, seed + 102, MiMOLookAndFeel::paperBg, 0.5f, juce::Colours::transparentBlack, 1.0f);
        const float fillW = slTrack.getWidth() * comp.micSensitivity;
        if (fillW > 2.0f)
        {
            juce::Rectangle<float> fillRect(slTrack.getX(), slTrack.getY(), fillW, slTrack.getHeight());
            g.setColour(MiMOLookAndFeel::crayonOchre.withAlpha(0.65f));
            g.fillRoundedRectangle(fillRect, 3.0f);
        }
        const float knobX = slTrack.getX() + fillW;
        const float knobY = slTrack.getCentreY();
        g.setColour(MiMOLookAndFeel::paperCard);
        g.fillEllipse(knobX - 6.0f, knobY - 6.0f, 12.0f, 12.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.drawEllipse(knobX - 6.0f, knobY - 6.0f, 12.0f, 12.0f, 1.4f);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText("Sens: " + juce::String(static_cast<int>(comp.micSensitivity * 100)) + "%  |  " + comp.getMicStatusText(),
                   static_cast<int>(box.getX() + 12), static_cast<int>(box.getY() + 84), static_cast<int>(box.getWidth() - 24), 13, juce::Justification::centredLeft);

        // Riga Trigger Out
        juce::Rectangle<float> trigRow(box.getX() + 12.0f, box.getY() + 102.0f, box.getWidth() - 24.0f, 26.0f);
        g.setColour(juce::Colour(0xFFFF7A18).withAlpha(0.18f));
        g.fillRoundedRectangle(trigRow, 4.0f);
        g.setColour(juce::Colour(0xFFFF7A18));
        g.drawRoundedRectangle(trigRow, 4.0f, 1.4f);

        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("Trigger Colpo", static_cast<int>(trigRow.getX() + 8), static_cast<int>(trigRow.getY()), 110, static_cast<int>(trigRow.getHeight()), juce::Justification::centredLeft);

        // Tasto Test
        juce::Rectangle<float> testBtn(trigRow.getRight() - 65.0f, trigRow.getY() + 3.0f, 40.0f, 20.0f);
        MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 105, MiMOLookAndFeel::crayonOchre, 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText("TEST", testBtn.toNearestInt(), juce::Justification::centred);

        // Righe Modulazione (Velocity e Pressione)
        float my = box.getY() + 134.0f;
        for (const auto& m : node.modOutputs)
        {
            juce::Rectangle<float> row(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 26.0f);
            g.setColour(m.colour.withAlpha(0.20f));
            g.fillRoundedRectangle(row, 4.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawRoundedRectangle(row, 4.0f, 1.2f);

            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawText(m.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 90, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);
            g.setFont(juce::Font(9.5f, juce::Font::plain));
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.drawText("(verso manopola)", static_cast<int>(row.getX() + 90), static_cast<int>(row.getY()), static_cast<int>(row.getWidth() - 95), static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

            my += 32.0f;
        }
    }
    else if (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
    {
        juce::Rectangle<float> camBox(box.getX() + 12.0f, box.getY() + 46.0f, box.getWidth() - 24.0f, 85.0f);
        MiMOLookAndFeel::drawSketchBox(g, camBox, seed + 201, MiMOLookAndFeel::paperBg, 0.85f, juce::Colours::transparentBlack, 1.3f);

        juce::Image camFrame;
        {
            juce::ScopedLock sl(comp.cameraLock);
            if (comp.latestWebcamImage.isValid())
                camFrame = comp.latestWebcamImage.createCopy();
        }

        const auto innerCamBox = camBox.reduced(2.0f);

        if (comp.eyeShowVideo && camFrame.isValid())
        {
            const float imgW = static_cast<float>(camFrame.getWidth());
            const float imgH = static_cast<float>(camFrame.getHeight());
            const auto videoBox = juce::RectanglePlacement(juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize)
                .appliedTo(juce::Rectangle<float>(0.0f, 0.0f, imgW, imgH), innerCamBox);

            g.saveState();
            g.reduceClipRegion(innerCamBox.toNearestInt());
            g.setColour(juce::Colour(0xff181a20));
            g.fillRect(innerCamBox);
            g.drawImage(camFrame, videoBox, juce::RectanglePlacement::centred);
            MiMOEyeDrawer::drawHandSkeleton(g, videoBox, comp.latestEyeState, true,
                                           comp.isHandEnabled(false), comp.isHandEnabled(true),
                                           comp.isHandTriggersEnabled(false), comp.isHandTriggersEnabled(true));
            g.restoreState();
        }
        else if (comp.latestEyeState.hasHand)
        {
            g.saveState();
            g.reduceClipRegion(innerCamBox.toNearestInt());
            MiMOEyeDrawer::drawHandSkeleton(g, innerCamBox, comp.latestEyeState, false,
                                           comp.isHandEnabled(false), comp.isHandEnabled(true),
                                           comp.isHandTriggersEnabled(false), comp.isHandTriggersEnabled(true));
            g.restoreState();
        }
        else
        {
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::italic));
            g.drawText(comp.getCameraStatusText(), camBox.reduced(6.0f).toNearestInt(), juce::Justification::centred);
        }

        // Render uscite modulo
        if (!node.outputGroups.empty())
        {
            float curY = box.getY() + 168.0f;
            for (size_t gIdx = 0; gIdx < node.outputGroups.size(); ++gIdx)
            {
                const auto& grp = node.outputGroups[gIdx];
                if (!grp.parentGroup.isEmpty())
                    continue;

                const bool hasSubgroups = grp.items.empty();
                const bool handOn = grp.isEnabled;
                juce::Rectangle<float> hdrBox(box.getX() + 8.0f, curY, box.getWidth() - 16.0f, 24.0f);

                int totalItemsInTree = 0;
                int activeCablesInTree = 0;

                if (hasSubgroups)
                {
                    for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                    {
                        const auto& sub = node.outputGroups[sIdx];
                        if (sub.parentGroup == grp.id)
                        {
                            totalItemsInTree += static_cast<int>(sub.items.size());
                            for (const auto& item : sub.items)
                                for (const auto& c : comp.cables)
                                    if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                        activeCablesInTree++;
                        }
                    }
                }
                else
                {
                    totalItemsInTree = static_cast<int>(grp.items.size());
                    for (const auto& item : grp.items)
                        for (const auto& c : comp.cables)
                            if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                activeCablesInTree++;
                }

                // Top-Level Header
                g.setColour(juce::Colour(0xFF1E2129));
                g.fillRoundedRectangle(hdrBox, 4.0f);
                g.setColour(activeCablesInTree > 0 ? MiMOLookAndFeel::crayonOchre : (comp.isDarkMode ? juce::Colours::white.withAlpha(0.24f) : juce::Colours::black.withAlpha(0.20f)));
                g.drawRoundedRectangle(hdrBox, 4.0f, 1.2f);

                // Interruttore ON/OFF
                const juce::Rectangle<float> switchBox(hdrBox.getX() + 5.0f, hdrBox.getCentreY() - 7.0f, 29.0f, 14.0f);
                g.setColour(handOn ? juce::Colour(0xFF2E8540) : juce::Colour(0xFF333842));
                g.fillRoundedRectangle(switchBox, 7.0f);
                g.setColour(handOn ? juce::Colour(0xFF55E678).withAlpha(0.6f) : juce::Colours::white.withAlpha(0.15f));
                g.drawRoundedRectangle(switchBox, 7.0f, 1.0f);

                const float knobRadius = 5.0f;
                const float knobX = handOn ? (switchBox.getRight() - knobRadius - 2.0f) : (switchBox.getX() + knobRadius + 2.0f);
                g.setColour(handOn ? juce::Colours::white : juce::Colour(0xFF888E9B));
                g.fillEllipse(knobX - knobRadius, switchBox.getCentreY() - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);

                g.setFont(juce::Font(7.5f, juce::Font::bold));
                g.setColour(handOn ? juce::Colours::white : juce::Colour(0xFF757B85));
                if (handOn)
                    g.drawText("ON", static_cast<int>(switchBox.getX() + 2), static_cast<int>(switchBox.getY()), 13, static_cast<int>(switchBox.getHeight()), juce::Justification::centred);
                else
                    g.drawText("OFF", static_cast<int>(switchBox.getRight() - 15), static_cast<int>(switchBox.getY()), 13, static_cast<int>(switchBox.getHeight()), juce::Justification::centred);

                // Freccia toggle espandi/comprimi
                const juce::Rectangle<float> chevBox(switchBox.getRight() + 4.0f, hdrBox.getY(), 13.0f, hdrBox.getHeight());
                g.setColour(handOn ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.0f, juce::Font::bold));
                g.drawText(grp.isExpanded ? juce::String::charToString(0x25BC) : juce::String::charToString(0x25B6),
                           chevBox.toNearestInt(), juce::Justification::centred);

                const float titleX = chevBox.getRight() + 3.0f;
                g.setColour(handOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                juce::String mainTitle = grp.name;
                if (!handOn)
                    mainTitle += " (SPENTA)";
                g.drawText(mainTitle, static_cast<int>(titleX), static_cast<int>(hdrBox.getY()),
                           static_cast<int>(hdrBox.getWidth() - 95.0f), static_cast<int>(hdrBox.getHeight()), juce::Justification::centredLeft);

                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(9.0f, juce::Font::plain));
                g.drawText("(" + juce::String(totalItemsInTree) + ")",
                           static_cast<int>(hdrBox.getRight() - 52), static_cast<int>(hdrBox.getY()), 24, static_cast<int>(hdrBox.getHeight()), juce::Justification::centredRight);

                if (!grp.isExpanded && activeCablesInTree > 0)
                {
                    g.setColour(MiMOLookAndFeel::crayonOchre);
                    g.setFont(juce::Font(8.5f, juce::Font::bold));
                    g.drawText(juce::String(activeCablesInTree) + " cavi",
                               static_cast<int>(hdrBox.getRight() - 28), static_cast<int>(hdrBox.getY()), 26, static_cast<int>(hdrBox.getHeight()), juce::Justification::centred);
                }

                curY += 26.0f;

                if (hasSubgroups)
                {
                    if (grp.isExpanded)
                    {
                        for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                        {
                            const auto& sub = node.outputGroups[sIdx];
                            if (sub.parentGroup != grp.id)
                                continue;

                            juce::Rectangle<float> subHdrBox(box.getX() + 14.0f, curY, box.getWidth() - 28.0f, 22.0f);
                            int activeSubCables = 0;
                            for (const auto& item : sub.items)
                                for (const auto& c : comp.cables)
                                    if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                        activeSubCables++;

                            const bool subEnabled = sub.isEnabled && handOn;

                            g.setColour(juce::Colour(0xFF14171E));
                            g.fillRoundedRectangle(subHdrBox, 3.0f);
                            g.setColour(activeSubCables > 0 ? MiMOLookAndFeel::crayonCyan : (comp.isDarkMode ? juce::Colours::white.withAlpha(0.18f) : juce::Colours::black.withAlpha(0.15f)));
                            g.drawRoundedRectangle(subHdrBox, 3.0f, 1.0f);

                            // Switch ON/OFF sottogruppo
                            const juce::Rectangle<float> subSwitchBox(subHdrBox.getX() + 4.0f, subHdrBox.getCentreY() - 6.0f, 25.0f, 12.0f);
                            g.setColour(subEnabled ? juce::Colour(0xFF2E8540) : juce::Colour(0xFF2B2E37));
                            g.fillRoundedRectangle(subSwitchBox, 6.0f);
                            g.setColour(subEnabled ? juce::Colour(0xFF55E678).withAlpha(0.6f) : juce::Colours::white.withAlpha(0.12f));
                            g.drawRoundedRectangle(subSwitchBox, 6.0f, 0.8f);

                            const float subKnobRadius = 4.2f;
                            const float subKnobX = subEnabled ? (subSwitchBox.getRight() - subKnobRadius - 1.8f) : (subSwitchBox.getX() + subKnobRadius + 1.8f);
                            g.setColour(subEnabled ? juce::Colours::white : juce::Colour(0xFF888E9B));
                            g.fillEllipse(subKnobX - subKnobRadius, subSwitchBox.getCentreY() - subKnobRadius, subKnobRadius * 2.0f, subKnobRadius * 2.0f);

                            const juce::Rectangle<float> subChevBox(subSwitchBox.getRight() + 3.0f, subHdrBox.getY(), 11.0f, subHdrBox.getHeight());
                            g.setColour(subEnabled ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::inkMuted);
                            g.setFont(juce::Font(9.0f, juce::Font::bold));
                            g.drawText(sub.isExpanded ? juce::String::charToString(0x25BC) : juce::String::charToString(0x25B6),
                                       subChevBox.toNearestInt(), juce::Justification::centred);

                            const float subTitleX = subChevBox.getRight() + 2.0f;
                            g.setColour(subEnabled ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                            g.setFont(juce::Font(9.5f, juce::Font::bold));
                            juce::String subTitle = sub.name;
                            if (!subEnabled)
                                subTitle += " (OFF)";
                            g.drawText(subTitle, static_cast<int>(subTitleX), static_cast<int>(subHdrBox.getY()),
                                       static_cast<int>(subHdrBox.getWidth() - 85.0f), static_cast<int>(subHdrBox.getHeight()), juce::Justification::centredLeft);

                            g.setColour(MiMOLookAndFeel::inkMuted);
                            g.setFont(juce::Font(8.5f, juce::Font::plain));
                            g.drawText("(" + juce::String(sub.items.size()) + ")",
                                       static_cast<int>(subHdrBox.getRight() - 48), static_cast<int>(subHdrBox.getY()), 20, static_cast<int>(subHdrBox.getHeight()), juce::Justification::centredRight);

                            if (!sub.isExpanded && activeSubCables > 0)
                            {
                                g.setColour(MiMOLookAndFeel::crayonCyan);
                                g.setFont(juce::Font(8.0f, juce::Font::bold));
                                g.drawText(juce::String(activeSubCables) + " cavi",
                                           static_cast<int>(subHdrBox.getRight() - 26), static_cast<int>(subHdrBox.getY()), 24, static_cast<int>(subHdrBox.getHeight()), juce::Justification::centred);
                            }

                            curY += 23.0f;

                            if (sub.isExpanded)
                            {
                                for (const auto& item : sub.items)
                                {
                                    juce::Rectangle<float> row(box.getX() + 20.0f, curY, box.getWidth() - 36.0f, 22.0f);
                                    g.setColour(item.colour.withAlpha(subEnabled ? 0.18f : 0.06f));
                                    g.fillRoundedRectangle(row, 3.0f);
                                    g.setColour(comp.isDarkMode ? juce::Colours::white.withAlpha(subEnabled ? 0.16f : 0.05f) : juce::Colours::black.withAlpha(subEnabled ? 0.12f : 0.04f));
                                    g.drawRoundedRectangle(row, 3.0f, 0.8f);

                                    g.setFont(juce::Font(9.5f, juce::Font::bold));
                                    g.setColour(subEnabled ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                                    g.drawText(item.label, static_cast<int>(row.getX() + 6), static_cast<int>(row.getY()), 135, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                                    bool isConn = false;
                                    for (const auto& c : comp.cables)
                                    {
                                        if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                        {
                                            isConn = true;
                                            break;
                                        }
                                    }

                                    if (isConn)
                                    {
                                        juce::Rectangle<float> discBtn(row.getRight() - 40.0f, row.getY() + 3.0f, 16.0f, 16.0f);
                                        g.setColour(MiMOLookAndFeel::crayonCherry.withAlpha(0.70f));
                                        g.fillRoundedRectangle(discBtn, 2.0f);
                                        g.setColour(MiMOLookAndFeel::polaroidWhite);
                                        g.setFont(juce::Font(9.0f, juce::Font::bold));
                                        g.drawText("X", discBtn.toNearestInt(), juce::Justification::centred);
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
                            g.setColour(item.colour.withAlpha(handOn ? 0.20f : 0.07f));
                            g.fillRoundedRectangle(row, 3.0f);
                            g.setColour(comp.isDarkMode ? juce::Colours::white.withAlpha(handOn ? 0.18f : 0.06f) : juce::Colours::black.withAlpha(handOn ? 0.14f : 0.04f));
                            g.drawRoundedRectangle(row, 3.0f, 0.8f);

                            g.setFont(juce::Font(10.0f, juce::Font::bold));
                            g.setColour(handOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                            g.drawText(item.label, static_cast<int>(row.getX() + 6), static_cast<int>(row.getY()), 140, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

                            bool isConn = false;
                            for (const auto& c : comp.cables)
                            {
                                if (c.sourceNodeId == node.id && c.sourcePinId == item.pinId)
                                {
                                    isConn = true;
                                    break;
                                }
                            }

                            if (isConn)
                            {
                                juce::Rectangle<float> discBtn(row.getRight() - 40.0f, row.getY() + 3.0f, 16.0f, 16.0f);
                                g.setColour(MiMOLookAndFeel::crayonCherry.withAlpha(0.70f));
                                g.fillRoundedRectangle(discBtn, 2.0f);
                                g.setColour(MiMOLookAndFeel::polaroidWhite);
                                g.setFont(juce::Font(9.0f, juce::Font::bold));
                                g.drawText("X", discBtn.toNearestInt(), juce::Justification::centred);
                            }

                            curY += 24.0f;
                        }
                    }
                }
            }
        }
    }
    else
    {
        // Modulo Hardware standard (es. hw-drum)
        float my = box.getY() + 46.0f;
        for (const auto& m : node.modOutputs)
        {
            juce::Rectangle<float> row(box.getX() + 12.0f, my, box.getWidth() - 24.0f, 26.0f);
            g.setColour(m.colour.withAlpha(0.20f));
            g.fillRoundedRectangle(row, 4.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawRoundedRectangle(row, 4.0f, 1.2f);

            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawText(m.label, static_cast<int>(row.getX() + 8), static_cast<int>(row.getY()), 130, static_cast<int>(row.getHeight()), juce::Justification::centredLeft);

            // Icona stacco cavo se connesso
            for (const auto& c : comp.cables)
            {
                if (c.sourceNodeId == node.id && (c.sourcePinId == m.key || c.sourcePinId == "mod:" + m.key))
                {
                    juce::Rectangle<float> discBtn(row.getRight() - 40.0f, row.getY() + 4.0f, 18.0f, 18.0f);
                    MiMOLookAndFeel::drawSketchBox(g, discBtn, seed + 888, MiMOLookAndFeel::crayonCherry, 0.40f);
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.setFont(juce::Font(9.0f, juce::Font::bold));
                    g.drawText("X", discBtn.toNearestInt(), juce::Justification::centred);
                    break;
                }
            }

            my += 32.0f;
        }

        // Tasto Test
        juce::Rectangle<float> testBtn(box.getX() + 12.0f, my + 4.0f, box.getWidth() - 24.0f, 24.0f);
        MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 10, MiMOLookAndFeel::crayonOchre, 0.40f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText("TEST TRIGGER", testBtn.toNearestInt(), juce::Justification::centred);
    }

    // Tendina selettore dispositivo per moduli integrati
    if (node.isIntegratedMic || node.isIntegratedCamera || node.id == "hw-mic" || node.id == "hw-webcam" || node.id == "hw-eye")
    {
        const auto devRow = MainComponent::getIntegratedDeviceRowBounds(box);
        const bool isMicNode = (node.isIntegratedMic || node.id == "hw-mic");
        const bool isOpen = isMicNode ? comp.micDropdownOpen : comp.camDropdownOpen;
        const juce::String activeCamLabel = comp.useSmartphoneEye ? juce::String("Smartphone (QR AI)") : comp.getCameraDeviceLabel();
        const juce::String devLabel = isMicNode ? ("Mic: " + comp.getMicDeviceLabel()) : ("Cam: " + activeCamLabel);

        MiMOLookAndFeel::drawSketchBox(g, devRow, seed + 303,
                                       isOpen ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg,
                                       isOpen ? 0.45f : 0.75f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, juce::Font::bold));
        g.drawText(devLabel, static_cast<int>(devRow.getX() + 6.0f), static_cast<int>(devRow.getY()),
                   static_cast<int>(devRow.getWidth() - 24.0f), static_cast<int>(devRow.getHeight()),
                   juce::Justification::centredLeft);

        juce::Path arrow;
        const float ax = devRow.getRight() - 11.0f;
        const float ay = devRow.getCentreY();
        if (isOpen)
            arrow.addTriangle(ax - 4.0f, ay + 2.5f, ax + 4.0f, ay + 2.5f, ax, ay - 2.5f);
        else
            arrow.addTriangle(ax - 4.0f, ay - 2.5f, ax + 4.0f, ay - 2.5f, ax, ay + 2.5f);
        g.fillPath(arrow);
    }
}

void CanvasRenderer::paintNestedEffectNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node,
                                           const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow)
{
    g.setColour(MiMOLookAndFeel::paperCard);
    g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

    MiMOLookAndFeel::drawSketchBox(g, box, seed, node.crayonColour, 0.52f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

    if (node.isVerticalFlow)
    {
        juce::Rectangle<float> topPlug(box.getX() + 18.0f, box.getY() - 4.0f, 22.0f, 8.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillRoundedRectangle(topPlug, 2.0f);

        juce::Rectangle<float> botPlug(box.getX() + 18.0f, box.getBottom() - 4.0f, 22.0f, 8.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillRoundedRectangle(botPlug, 2.0f);
    }

    g.setFont(juce::Font(9.5f, juce::Font::bold));
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.drawText("IN", static_cast<int>(box.getX() + 8), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredLeft);
    g.drawText("OUT", static_cast<int>(box.getRight() - 32), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredRight);

    // Titolo e sottotitolo
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText(node.label, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 8), static_cast<int>(box.getWidth() - 80), 20, juce::Justification::centredLeft);
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText(node.subtitle, static_cast<int>(box.getX() + 14), static_cast<int>(box.getY() + 26), static_cast<int>(box.getWidth() - 80), 16, juce::Justification::centredLeft);

    // Tasto Elimina Effetto [X]
    juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
    MiMOLookAndFeel::drawSketchBox(g, closeBtn, seed + 99, MiMOLookAndFeel::crayonCherry, 0.40f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

    // Tasto Bypass [PWR]
    juce::Rectangle<float> pwrBtn(box.getRight() - 48.0f, box.getY() + 8.0f, 18.0f, 18.0f);
    MiMOLookAndFeel::drawSketchBox(g, pwrBtn, seed + 98, node.isBypassed ? MiMOLookAndFeel::paperBg : MiMOLookAndFeel::crayonSage,
                                   node.isBypassed ? 0.35f : 0.70f);
    g.setColour(node.isBypassed ? MiMOLookAndFeel::inkMuted : MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(9.0f, juce::Font::bold));
    g.drawText(node.isBypassed ? "BYP" : "ON", pwrBtn.toNearestInt(), juce::Justification::centred);

    // Tasto Espandi Struttura Interna [EXP]
    juce::Rectangle<float> expBtn(box.getRight() - 71.0f, box.getY() + 8.0f, 18.0f, 18.0f);
    MiMOLookAndFeel::drawSketchBox(g, expBtn, seed + 97, MiMOLookAndFeel::crayonCyan, 0.45f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(8.0f, juce::Font::bold));
    g.drawText("EXP", expBtn.toNearestInt(), juce::Justification::centred);

    // Manopole dei parametri
    const auto canvasKnobs = comp.collectCanvasKnobs();
    for (const auto& kh : canvasKnobs)
    {
        if (kh.nodeId != node.id)
            continue;

        bool found = false;
        for (const auto& b : node.innerBlocks)
        {
            if (b.id != kh.blockId)
                continue;

            for (const auto& p : b.params)
            {
                if (p.key != kh.paramKey)
                    continue;

                bool isModulated = false;
                juce::Colour modCol = MiMOLookAndFeel::crayonOchre;
                const juce::String targetPin = "param:" + b.id + "." + p.key;
                for (const auto& c : comp.cables)
                {
                    if (c.targetNodeId == node.id && (c.targetPinId == targetPin || c.targetPinId == ("mod:" + p.key)))
                    {
                        isModulated = true;
                        modCol = c.colour;
                        break;
                    }
                }

                MiMOLookAndFeel::drawCrayonKnob(
                    g, kh.worldBounds, p.getNormalized(), node.crayonColour,
                    p.label, p.getFormattedValue(),
                    (node.id + p.key).hashCode(), isModulated, modCol
                );
                found = true;
                break;
            }
            if (found)
                break;
        }

        if (!found)
        {
            for (const auto& p : node.directParams)
            {
                if (p.key != kh.paramKey)
                    continue;

                bool isModulated = false;
                juce::Colour modCol = MiMOLookAndFeel::crayonCyan;
                for (const auto& c : comp.cables)
                {
                    if (c.targetNodeId == node.id && c.targetPinId == ("mod:" + p.key))
                    {
                        isModulated = true;
                        modCol = c.colour;
                        break;
                    }
                }

                MiMOLookAndFeel::drawCrayonKnob(
                    g, kh.worldBounds, p.getNormalized(), node.crayonColour,
                    p.label, p.getFormattedValue(),
                    (node.id + p.key).hashCode(), isModulated, modCol
                );
                break;
            }
        }
    }
}

void CanvasRenderer::paintInstrumentNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node,
                                         const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow)
{
    g.setColour(MiMOLookAndFeel::paperCard);
    g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

    MiMOLookAndFeel::drawSketchBox(g, box, seed, node.crayonColour, 0.58f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

    // Badge Hardware
    juce::Rectangle<float> badge(box.getX() + 10.0f, box.getY() + 10.0f, 32.0f, 20.0f);
    MiMOLookAndFeel::drawSketchBox(g, badge, seed + 1, MiMOLookAndFeel::crayonOchre, 0.40f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText(node.portBadge, badge.toNearestInt(), juce::Justification::centred);

    g.setFont(juce::Font(14.0f, juce::Font::bold));
    g.drawText(node.label, static_cast<int>(box.getX() + 48), static_cast<int>(box.getY() + 8), static_cast<int>(box.getWidth() - 80), 18, juce::Justification::centredLeft);
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText(node.subtitle, static_cast<int>(box.getX() + 48), static_cast<int>(box.getY() + 24), static_cast<int>(box.getWidth() - 80), 14, juce::Justification::centredLeft);

    // Tasto Elimina Strumento [X]
    juce::Rectangle<float> closeBtn(box.getRight() - 25.0f, box.getY() + 8.0f, 18.0f, 18.0f);
    MiMOLookAndFeel::drawSketchBox(g, closeBtn, seed + 99, MiMOLookAndFeel::crayonCherry, 0.40f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

    // Tasto Test
    juce::Rectangle<float> testBtn(box.getX() + 12.0f, box.getY() + 42.0f, box.getWidth() - 24.0f, 20.0f);
    MiMOLookAndFeel::drawSketchBox(g, testBtn, seed + 106, node.crayonColour, 0.35f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(9.5f, juce::Font::bold));
    g.drawText("SUONA NOTA / TEST", testBtn.toNearestInt(), juce::Justification::centred);

    // Badge OUT
    g.setFont(juce::Font(9.5f, juce::Font::bold));
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.drawText("OUT", static_cast<int>(box.getRight() - 32), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredRight);

    // Manopole parametri strumento
    const auto canvasKnobs = comp.collectCanvasKnobs();
    for (const auto& kh : canvasKnobs)
    {
        if (kh.nodeId != node.id)
            continue;

        for (const auto& p : node.directParams)
        {
            if (p.key != kh.paramKey)
                continue;

            bool isModulated = false;
            juce::Colour modCol = MiMOLookAndFeel::crayonCyan;
            for (const auto& c : comp.cables)
            {
                if (c.targetNodeId == node.id && c.targetPinId == ("mod:" + p.key))
                {
                    isModulated = true;
                    modCol = c.colour;
                    break;
                }
            }

            MiMOLookAndFeel::drawCrayonKnob(
                g, kh.worldBounds, p.getNormalized(), node.crayonColour,
                p.label, p.getFormattedValue(),
                (node.id + p.key).hashCode(), isModulated, modCol
            );
        }
    }
}

void CanvasRenderer::paintMasterOutNode(juce::Graphics& g, MainComponent& comp, const CanvasNode& node,
                                        const juce::Rectangle<float>& box, int seed, bool isNodeSelected, juce::Colour glow)
{
    g.setColour(MiMOLookAndFeel::paperCard);
    g.fillRoundedRectangle(box.reduced(1.0f), 8.0f);

    MiMOLookAndFeel::drawSketchBox(g, box, seed, node.crayonColour, 0.45f, glow, isNodeSelected ? 3.0f : 2.0f, false, MiMOLookAndFeel::paperCard);

    g.setFont(juce::Font(9.5f, juce::Font::bold));
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.drawText("IN", static_cast<int>(box.getX() + 8), static_cast<int>(box.getY() + 26), 24, 14, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(14.5f, juce::Font::bold));
    g.drawText(node.label, static_cast<int>(box.getX() + 12), static_cast<int>(box.getY() + 10), 140, 18, juce::Justification::centred);
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText(node.subtitle, static_cast<int>(box.getX() + 12), static_cast<int>(box.getY() + 27), 140, 15, juce::Justification::centred);

    if (!node.directParams.empty())
    {
        const auto& p = node.directParams[0];
        juce::Rectangle<float> kb(box.getX() + 25.0f, box.getY() + 52.0f, box.getWidth() - 50.0f, 88.0f);
        MiMOLookAndFeel::drawCrayonKnob(g, kb, p.getNormalized(), node.crayonColour,
                                        p.label, p.getFormattedValue(), seed + 9);
    }
}

} // namespace mimo
