#include "MiMOEyeDrawer.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include "../Utils/qrcodegen.hpp"
#include <algorithm>

namespace mimo
{
namespace MiMOEyeDrawer
{

juce::Rectangle<float> getEyeModalBounds(int compWidth, int compHeight)
{
    const float mw = std::min(780.0f, static_cast<float>(compWidth) - 36.0f);
    const float mh = std::min(535.0f, static_cast<float>(compHeight) - 36.0f);
    return { (static_cast<float>(compWidth) - mw) * 0.5f,
             (static_cast<float>(compHeight) - mh) * 0.5f,
             mw, mh };
}

juce::Rectangle<float> getEyeModalCloseBounds(int compWidth, int compHeight)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    return { mb.getRight() - 36.0f, mb.getY() + 14.0f, 22.0f, 22.0f };
}

juce::Rectangle<float> getEyeModalTabBounds(int compWidth, int compHeight, int tabIdx)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float tabW = 265.0f;
    return { mb.getX() + 20.0f + static_cast<float>(tabIdx) * (tabW + 10.0f), mb.getY() + 62.0f, tabW, 28.0f };
}

juce::Rectangle<float> getEyeModalUrlButtonBounds(int compWidth, int compHeight)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, mb.getBottom() - contentY - 18.0f);
    const juce::Rectangle<float> qrBox(leftPanel.getCentreX() - 75.0f, leftPanel.getY() + 10.0f, 150.0f, 150.0f);
    const juce::Rectangle<float> urlPill(leftPanel.getX() + 12.0f, qrBox.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 22.0f);
    return { leftPanel.getX() + 12.0f, urlPill.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 25.0f };
}

juce::Rectangle<float> getEyeModalActivatePhoneButtonBounds(int compWidth, int compHeight)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, mb.getBottom() - contentY - 18.0f);
    const auto urlBtn = getEyeModalUrlButtonBounds(compWidth, compHeight);
    return { leftPanel.getX() + 12.0f, urlBtn.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 26.0f };
}

juce::Rectangle<float> getEyeModalHandFilterBounds(int compWidth, int compHeight, int filterIdx)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float contentH = mb.getBottom() - contentY - 18.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, contentH);
    const juce::Rectangle<float> rightPanel(leftPanel.getRight() + 14.0f, contentY, mb.getRight() - leftPanel.getRight() - 32.0f, contentH);

    const float cfgStartY = rightPanel.getY() + 10.0f + 22.0f + 8.0f + 152.0f + 8.0f;
    const float startX = rightPanel.getX() + 162.0f;
    const float availW = rightPanel.getRight() - 14.0f - startX;
    const float btnW = (availW - 12.0f) / 3.0f;
    return { startX + static_cast<float>(filterIdx) * (btnW + 6.0f), cfgStartY, btnW, 22.0f };
}

juce::Rectangle<float> getEyeModalPhalanxBounds(int compWidth, int compHeight, int detailIdx)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float contentH = mb.getBottom() - contentY - 18.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, contentH);
    const juce::Rectangle<float> rightPanel(leftPanel.getRight() + 14.0f, contentY, mb.getRight() - leftPanel.getRight() - 32.0f, contentH);

    const float phalY = rightPanel.getY() + 10.0f + 22.0f + 8.0f + 152.0f + 8.0f + 28.0f;
    const float startX = rightPanel.getX() + 162.0f;
    const float availW = rightPanel.getRight() - 14.0f - startX;
    const float btnW = (availW - 12.0f) / 3.0f;
    return { startX + static_cast<float>(detailIdx) * (btnW + 6.0f), phalY, btnW, 22.0f };
}

juce::Rectangle<float> getEyeModalGestureBounds(int compWidth, int compHeight, int gestureIdx)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float contentH = mb.getBottom() - contentY - 18.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, contentH);
    const juce::Rectangle<float> rightPanel(leftPanel.getRight() + 14.0f, contentY, mb.getRight() - leftPanel.getRight() - 32.0f, contentH);

    const float gestGridY = rightPanel.getY() + 10.0f + 22.0f + 8.0f + 152.0f + 8.0f + 28.0f + 26.0f + 18.0f;
    const float colW = (rightPanel.getWidth() - 28.0f - 8.0f) * 0.5f;
    const int col = gestureIdx % 2;
    const int row = gestureIdx / 2;
    return { rightPanel.getX() + 14.0f + static_cast<float>(col) * (colW + 8.0f),
             gestGridY + static_cast<float>(row) * 28.0f,
             colW, 24.0f };
}

juce::Rectangle<float> getEyeModalPinchTestBounds(int compWidth, int compHeight, int pinchIdx)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float contentH = mb.getBottom() - contentY - 18.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, contentH);
    const juce::Rectangle<float> rightPanel(leftPanel.getRight() + 14.0f, contentY, mb.getRight() - leftPanel.getRight() - 32.0f, contentH);

    const float testY = rightPanel.getBottom() - 36.0f;
    const float btnW = (rightPanel.getWidth() - 28.0f - 18.0f) / 4.0f;
    return { rightPanel.getX() + 14.0f + static_cast<float>(pinchIdx) * (btnW + 6.0f), testY, btnW, 26.0f };
}

juce::Rectangle<float> getEyeModalCameraOptionBounds(int compWidth, int compHeight, int optIdx)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, mb.getBottom() - contentY - 18.0f);
    const float startY = leftPanel.getY() + 34.0f + static_cast<float>(optIdx) * 29.0f;
    return { leftPanel.getX() + 12.0f, startY, leftPanel.getWidth() - 24.0f, 25.0f };
}

juce::Rectangle<float> getEyeModalViewToggleBounds(int compWidth, int compHeight)
{
    const auto mb = getEyeModalBounds(compWidth, compHeight);
    const float contentY = mb.getY() + 100.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(mb.getX() + 18.0f, contentY, leftW, mb.getBottom() - contentY - 18.0f);
    return { leftPanel.getX() + 12.0f, leftPanel.getY() + 185.0f, leftPanel.getWidth() - 24.0f, 26.0f };
}

juce::Rectangle<float> getEyeCardConfigButtonBounds(const juce::Rectangle<float>& nodeBox)
{
    return { nodeBox.getRight() - 50.0f, nodeBox.getY() + 7.0f, 22.0f, 18.0f };
}

juce::Rectangle<float> getEyeCardViewToggleButtonBounds(const juce::Rectangle<float>& nodeBox)
{
    return { nodeBox.getRight() - 84.0f, nodeBox.getY() + 7.0f, 30.0f, 18.0f };
}

void drawQrCode(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> bounds)
{
    g.setColour(juce::Colours::white);
    g.fillRoundedRectangle(bounds, 8.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.drawRoundedRectangle(bounds, 8.0f, 1.4f);

    if (text.isEmpty())
        return;

    try
    {
        qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.toRawUTF8(), qrcodegen::QrCode::Ecc::MEDIUM);
        const int qrSize = qr.getSize();
        if (qrSize <= 0)
            return;

        const float quietZone = 2.0f;
        const float totalModules = static_cast<float>(qrSize) + quietZone * 2.0f;
        const float moduleSize = bounds.getWidth() / totalModules;

        g.setColour(juce::Colour(0xFF141413));
        for (int y = 0; y < qrSize; ++y)
        {
            for (int x = 0; x < qrSize; ++x)
            {
                if (qr.getModule(x, y))
                {
                    const float mx = bounds.getX() + (static_cast<float>(x) + quietZone) * moduleSize;
                    const float my = bounds.getY() + (static_cast<float>(y) + quietZone) * moduleSize;
                    g.fillRect(mx, my, moduleSize + 0.4f, moduleSize + 0.4f);
                }
            }
        }

        const float logoSize = bounds.getWidth() * 0.18f;
        const auto logoBox = bounds.withSizeKeepingCentre(logoSize, logoSize);
        g.setColour(juce::Colours::white);
        g.fillRoundedRectangle(logoBox, 4.0f);
        g.setColour(juce::Colour(0xFF141413));
        g.drawRoundedRectangle(logoBox, 4.0f, 1.4f);

        const float cx = logoBox.getCentreX();
        const float cy = logoBox.getCentreY();
        juce::Path eyeP;
        eyeP.startNewSubPath(cx - 8.0f, cy);
        eyeP.quadraticTo(cx, cy - 5.5f, cx + 8.0f, cy);
        eyeP.quadraticTo(cx, cy + 5.5f, cx - 8.0f, cy);
        eyeP.closeSubPath();
        g.strokePath(eyeP, juce::PathStrokeType(1.4f));
        g.fillEllipse(cx - 2.5f, cy - 2.5f, 5.0f, 5.0f);
    }
    catch (...)
    {
    }
}

void drawHandSkeleton(juce::Graphics& g, juce::Rectangle<float> bounds,
                      const mimo::HandTrackingState& state, bool overVideo,
                      bool handEnabledPrim, bool handEnabledSec,
                      bool trigsEnabledPrim, bool trigsEnabledSec)
{
    if (!overVideo)
    {
        g.setColour(MiMOLookAndFeel::paperBg);
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(MiMOLookAndFeel::fountainInk.withAlpha(0.35f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    if (!state.hasHand)
    {
        g.setColour(overVideo ? juce::Colours::white.withAlpha(0.85f) : MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText(overVideo ? "Mostra le mani alla videocamera (MediaPipe IA pronto)..."
                             : "In attesa delle mani nell'inquadratura...",
                   bounds.reduced(6.0f).toNearestInt(), juce::Justification::centred);
        return;
    }

    const float pad = overVideo ? 0.0f : std::min(14.0f, bounds.getHeight() * 0.10f);
    const float innerW = bounds.getWidth() - pad * 2.0f;
    const float innerH = bounds.getHeight() - pad * 2.0f;

    auto drawSingleHand = [&](const std::array<mimo::HandLandmark, 21>& lms,
                              const juce::String& handedness,
                              float /*palmZ*/,
                              const std::array<bool, 4>& pinchesAct,
                              bool isSecondary)
    {
        const bool handEnabled = isSecondary ? handEnabledSec : handEnabledPrim;
        const bool trigsEnabled = isSecondary ? trigsEnabledSec : trigsEnabledPrim;
        const float alphaMult = handEnabled ? 1.0f : 0.22f;

        std::array<juce::Point<float>, 21> pts;
        for (size_t i = 0; i < 21; ++i)
        {
            pts[i] = {
                bounds.getX() + pad + lms[i].x * innerW,
                bounds.getY() + pad + lms[i].y * innerH
            };
        }

        const juce::Point<float> palmPt = (pts[0] + pts[9]) * 0.5f;

        const bool isRight = (handedness != "Left");
        const auto baseBoneColour = isRight ? (isSecondary ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonCyan)
                                            : (isSecondary ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::crayonPeach);
        const auto boneColour = baseBoneColour.withMultipliedAlpha(alphaMult);

        // Linee guida del palmo
        g.setColour(boneColour.withAlpha((overVideo ? 0.28f : 0.18f) * alphaMult));
        g.drawLine(bounds.getX() + 2.0f, palmPt.y, bounds.getRight() - 2.0f, palmPt.y, 1.0f);
        g.drawLine(palmPt.x, bounds.getY() + 2.0f, palmPt.x, bounds.getBottom() - 2.0f, 1.0f);

        // Ossa tra le 21 falangi MediaPipe
        static const int bones[21][2] = {
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 },
            { 0, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 },
            { 5, 9 }, { 9, 10 }, { 10, 11 }, { 11, 12 },
            { 9, 13 }, { 13, 14 }, { 14, 15 }, { 15, 16 },
            { 13, 17 }, { 17, 18 }, { 18, 19 }, { 19, 20 },
            { 0, 17 }
        };

        for (int b = 0; b < 21; ++b)
        {
            const auto pA = pts[static_cast<size_t>(bones[b][0])];
            const auto pB = pts[static_cast<size_t>(bones[b][1])];
            g.setColour(boneColour.withAlpha(overVideo ? 0.85f : 0.70f));
            g.drawLine(pA.x, pA.y, pB.x, pB.y, overVideo ? 2.2f : 1.8f);
        }

        for (size_t i = 0; i < 21; ++i)
        {
            const bool isTip = (i == 4 || i == 8 || i == 12 || i == 16 || i == 20);
            const float r = isTip ? 4.2f : 2.6f;
            const auto ptCol = isTip ? MiMOLookAndFeel::crayonOchre : boneColour;
            g.setColour(ptCol);
            g.fillEllipse(pts[i].x - r, pts[i].y - r, r * 2.0f, r * 2.0f);
        }

        static const int tipIdxs[4] = { 8, 12, 16, 20 };
        for (int p = 0; p < 4; ++p)
        {
            if (pinchesAct[static_cast<size_t>(p)] && trigsEnabled)
            {
                const auto tPt = pts[static_cast<size_t>(tipIdxs[p])];
                g.setColour(juce::Colour(0xFFFF7A18));
                g.drawEllipse(tPt.x - 7.0f, tPt.y - 7.0f, 14.0f, 14.0f, 2.0f);
            }
        }
    };

    drawSingleHand(state.landmarks, state.handedness, state.palmZ, state.pinchActive, false);

    if (state.hasSecondaryHand)
    {
        drawSingleHand(state.secondaryLandmarks, state.secondaryHandedness,
                       state.secondaryPalmZ, state.secondaryPinchActive, true);
    }
}

void syncEyeNodeOutputsWithDetailSettings(CanvasNode& eyeNode,
                                         int /*detail*/, int /*filter*/,
                                         bool /*pos3D*/, bool /*fistTilt*/,
                                         bool /*fingerFlex*/, bool /*pinchTrig*/)
{
    if (eyeNode.outputGroups.size() != 8)
    {
        NodeOutputGroup grpDxRoot;
        grpDxRoot.id = "eye_dx";
        grpDxRoot.name = "Mano Destra (DX)";
        grpDxRoot.parentGroup = "";
        grpDxRoot.isExpanded = true;

        NodeOutputGroup grpDxPalm;
        grpDxPalm.id = "eye_dx_palm";
        grpDxPalm.name = "Palmo 3D";
        grpDxPalm.parentGroup = "eye_dx";
        grpDxPalm.isExpanded = true;
        grpDxPalm.items = {
            { "mod:dx_x",    "Posizione Orizz. X",  MiMOLookAndFeel::crayonCyan,   false, "(verso manopola)" },
            { "mod:dx_y",    "Altezza Vert. Y",   MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_z",    "Distanza / Prof. Z",     MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" },
            { "mod:dx_fist", "Apertura / Pugno",   MiMOLookAndFeel::crayonSage,   false, "(verso manopola)" },
            { "mod:dx_tilt", "Inclinazione Polso",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        NodeOutputGroup grpDxFingers;
        grpDxFingers.id = "eye_dx_fingers";
        grpDxFingers.name = "Dita & Falangi";
        grpDxFingers.parentGroup = "eye_dx";
        grpDxFingers.isExpanded = false;
        grpDxFingers.items = {
            { "mod:dx_thumb", "Chiusura Pollice",  MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_idx",   "Chiusura Indice",   MiMOLookAndFeel::crayonCyan,   false, "(verso manopola)" },
            { "mod:dx_mid",   "Chiusura Medio",    MiMOLookAndFeel::crayonSage,   false, "(verso manopola)" },
            { "mod:dx_rng",   "Chiusura Anulare",  MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_pky",   "Chiusura Mignolo",  MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" }
        };

        NodeOutputGroup grpDxPinches;
        grpDxPinches.id = "eye_dx_pinches";
        grpDxPinches.name = "Trigger Pinch";
        grpDxPinches.parentGroup = "eye_dx";
        grpDxPinches.isExpanded = false;
        grpDxPinches.items = {
            { "trigger:dx_pinch_idx", "Pinch Indice",  juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_mid", "Pinch Medio",   juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_rng", "Pinch Anulare", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_pky", "Pinch Mignolo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        NodeOutputGroup grpSxRoot;
        grpSxRoot.id = "eye_sx";
        grpSxRoot.name = "Mano Sinistra (SX)";
        grpSxRoot.parentGroup = "";
        grpSxRoot.isExpanded = false;

        NodeOutputGroup grpSxPalm;
        grpSxPalm.id = "eye_sx_palm";
        grpSxPalm.name = "Palmo 3D";
        grpSxPalm.parentGroup = "eye_sx";
        grpSxPalm.isExpanded = true;
        grpSxPalm.items = {
            { "mod:sx_x",    "Posizione Orizz. X",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_y",    "Altezza Vert. Y",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_z",    "Distanza / Prof. Z",     MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_fist", "Apertura / Pugno",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_tilt", "Inclinazione Polso",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        NodeOutputGroup grpSxFingers;
        grpSxFingers.id = "eye_sx_fingers";
        grpSxFingers.name = "Dita & Falangi";
        grpSxFingers.parentGroup = "eye_sx";
        grpSxFingers.isExpanded = false;
        grpSxFingers.items = {
            { "mod:sx_thumb", "Chiusura Pollice",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_idx",   "Chiusura Indice",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_mid",   "Chiusura Medio",    MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_rng",   "Chiusura Anulare",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_pky",   "Chiusura Mignolo",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        NodeOutputGroup grpSxPinches;
        grpSxPinches.id = "eye_sx_pinches";
        grpSxPinches.name = "Trigger Pinch";
        grpSxPinches.parentGroup = "eye_sx";
        grpSxPinches.isExpanded = false;
        grpSxPinches.items = {
            { "trigger:sx_pinch_idx", "Pinch Indice",  juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_mid", "Pinch Medio",   juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_rng", "Pinch Anulare", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_pky", "Pinch Mignolo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        eyeNode.outputGroups = {
            grpDxRoot, grpDxPalm, grpDxFingers, grpDxPinches,
            grpSxRoot, grpSxPalm, grpSxFingers, grpSxPinches
        };
    }
    eyeNode.size.y = CanvasGeometry::getNodeCalculatedHeight(eyeNode);
}

void drawModernFlatEyeModal(juce::Graphics& g, MainComponent& comp)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    // Backdrop oscurato coerente con paintNestedModal
    g.fillAll(MiMOLookAndFeel::fountainInk.withAlpha(0.55f));

    const auto modalBox = getEyeModalBounds(w, h);

    // Ombra morbida
    g.setColour(juce::Colours::black.withAlpha(comp.isDarkMode ? 0.55f : 0.25f));
    g.fillRoundedRectangle(modalBox.translated(0.0f, 6.0f), 12.0f);

    // Finestra Modale con lo stesso stile MiMOLookAndFeel
    g.setColour(MiMOLookAndFeel::paperBg);
    g.fillRoundedRectangle(modalBox, 10.0f);
    MiMOLookAndFeel::drawSketchBox(g, modalBox, 680, MiMOLookAndFeel::crayonCyan, 0.14f,
                                   juce::Colours::transparentBlack, 2.2f, false, MiMOLookAndFeel::paperBg);

    // Icona Occhio Vettoriale nell'Header
    {
        const float eyeCx = modalBox.getX() + 32.0f;
        const float eyeCy = modalBox.getY() + 26.0f;
        juce::Path eyePath;
        eyePath.startNewSubPath(eyeCx - 11.0f, eyeCy);
        eyePath.quadraticTo(eyeCx, eyeCy - 7.5f, eyeCx + 11.0f, eyeCy);
        eyePath.quadraticTo(eyeCx, eyeCy + 7.5f, eyeCx - 11.0f, eyeCy);
        eyePath.closeSubPath();
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.strokePath(eyePath, juce::PathStrokeType(1.8f));
        g.setColour(MiMOLookAndFeel::crayonCyan);
        g.fillEllipse(eyeCx - 4.5f, eyeCy - 4.5f, 9.0f, 9.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillEllipse(eyeCx - 2.0f, eyeCy - 2.0f, 4.0f, 4.0f);
    }

    // Header
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("MiMO Eye  -  Analisi Mano MediaPipe su PC & Impostazioni Dettaglio",
               static_cast<int>(modalBox.getX() + 50), static_cast<int>(modalBox.getY() + 14),
               static_cast<int>(modalBox.getWidth() - 95), 22, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawText("Usa lo Smartphone come semplice Videocamera remota oppure la Webcam PC. L'analisi 21-Landmark viene eseguita sul PC!",
               static_cast<int>(modalBox.getX() + 20), static_cast<int>(modalBox.getY() + 38),
               static_cast<int>(modalBox.getWidth() - 60), 16, juce::Justification::centredLeft);

    // Pulsante Chiudi [X]
    const auto closeBox = getEyeModalCloseBounds(w, h);
    MiMOLookAndFeel::drawSketchBox(g, closeBox, 681, MiMOLookAndFeel::crayonCherry, 0.70f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText("X", closeBox.toNearestInt(), juce::Justification::centred);

    // Tabs / Pills Switcher (Sorgente Video)
    for (int t = 0; t < 2; ++t)
    {
        const auto tabBox = getEyeModalTabBounds(w, h, t);
        const bool isSel = (comp.eyeDrawerTab == t);
        MiMOLookAndFeel::drawSketchBox(g, tabBox, 682 + t,
                                       isSel ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                       isSel ? 0.65f : 0.35f, juce::Colours::transparentBlack,
                                       isSel ? 1.8f : 1.2f);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, isSel ? juce::Font::bold : juce::Font::plain));
        const juce::String tabText = (t == 0)
            ? "[CAM] Selettore Videocamere (PC & Smartphone)"
            : "[QR] Connetti Smartphone (Wi-Fi)";
        g.drawText(tabText, tabBox.toNearestInt(), juce::Justification::centred);
    }

    const float contentY = modalBox.getY() + 100.0f;
    const float contentH = modalBox.getBottom() - contentY - 18.0f;
    const float leftW = 275.0f;
    const juce::Rectangle<float> leftPanel(modalBox.getX() + 18.0f, contentY, leftW, contentH);
    const juce::Rectangle<float> rightPanel(leftPanel.getRight() + 14.0f, contentY, modalBox.getRight() - leftPanel.getRight() - 32.0f, contentH);

    MiMOLookAndFeel::drawSketchBox(g, leftPanel, 685, MiMOLookAndFeel::paperCard, 0.85f,
                                   juce::Colours::transparentBlack, 1.5f, false, MiMOLookAndFeel::paperCard);

    if (comp.eyeDrawerTab == 0)
    {
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("Sorgente Video per Analisi Mani:",
                   static_cast<int>(leftPanel.getX() + 14), static_cast<int>(leftPanel.getY() + 10),
                   static_cast<int>(leftPanel.getWidth() - 28), 18, juce::Justification::centredLeft);

        const auto items = comp.getVideoSourceItems();
        const int curDevIdx = comp.cameraDeviceIndex.load();
        const int maxToShow = std::min(5, static_cast<int>(items.size()));

        for (int i = 0; i < maxToShow; ++i)
        {
            const auto optBox = getEyeModalCameraOptionBounds(w, h, i);
            const auto& it = items[i];
            const bool isAct = it.isSmartphone
                ? comp.useSmartphoneEye
                : (!comp.useSmartphoneEye && (curDevIdx == it.pcDeviceIdx || (i == 0 && curDevIdx < 0)));

            MiMOLookAndFeel::drawSketchBox(g, optBox, 701 + i,
                                           isAct ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                           isAct ? 0.75f : 0.40f, juce::Colours::transparentBlack,
                                           isAct ? 1.8f : 1.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.0f, isAct ? juce::Font::bold : juce::Font::plain));
            const juce::String badgeStr = isAct ? " [ATTIVA]" : "";
            g.drawText(it.label + badgeStr, optBox.reduced(6.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
        }

        const auto viewToggleBox = getEyeModalViewToggleBounds(w, h);
        MiMOLookAndFeel::drawSketchBox(g, viewToggleBox, 705,
                                       comp.eyeShowVideo ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::crayonOchre,
                                       0.55f, juce::Colours::transparentBlack, 1.4f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        const juce::String vtText = comp.eyeShowVideo
            ? "Vista: Video Reale + Scheletro [ON]"
            : "Vista: Solo Scheletro Vettoriale";
        g.drawText(vtText, viewToggleBox.toNearestInt(), juce::Justification::centred);

        juce::Rectangle<float> guideBox(leftPanel.getX() + 14.0f, viewToggleBox.getBottom() + 10.0f, leftPanel.getWidth() - 28.0f, 170.0f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        g.drawText("Sorgenti Supportate:\n"
                   "- Webcam PC (HP Wide Vision HD): Predefinita nativa a 0 latenza.\n"
                   "- Smartphone Web App: Connessione Wi-Fi dal tab [QR].\n"
                   "- Camo: Usa lo smartphone con cavo USB a 60 FPS.\n"
                   "- Esclusivita assoluta: nessuna interferenza o sfarfallio tra sorgenti!",
                   guideBox.toNearestInt(), juce::Justification::topLeft);
    }
    else
    {
        const juce::String urlStr = comp.eyeServer ? comp.eyeServer->getEyeUrl() : "http://localhost:8088/eye";
        const bool isHttps = (comp.eyeServer != nullptr && comp.eyeServer->hasHttpsTunnel());
        const bool isPhoneStreaming = (comp.eyeServer != nullptr && comp.eyeServer->isSmartphoneStreaming());
        const bool isClientConnected = (comp.eyeServer != nullptr && comp.eyeServer->isClientConnected());

        const juce::Rectangle<float> qrBox(leftPanel.getCentreX() - 75.0f, leftPanel.getY() + 10.0f, 150.0f, 150.0f);

        if (isHttps)
        {
            drawQrCode(g, urlStr, qrBox);
        }
        else
        {
            MiMOLookAndFeel::drawSketchBox(g, qrBox, 686, MiMOLookAndFeel::paperBg, 0.85f, juce::Colours::transparentBlack, 1.4f);
            g.setColour(MiMOLookAndFeel::crayonOchre);
            g.fillEllipse(qrBox.getCentreX() - 14.0f, qrBox.getCentreY() - 32.0f, 28.0f, 28.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("SSL", qrBox.getCentreX() - 14.0f, qrBox.getCentreY() - 32.0f, 28.0f, 28.0f, juce::Justification::centred);

            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText("Generazione Tunnel HTTPS...",
                       static_cast<int>(qrBox.getX() + 6), static_cast<int>(qrBox.getCentreY() + 4.0f),
                       static_cast<int>(qrBox.getWidth() - 12), 20, juce::Justification::centred);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.0f, juce::Font::plain));
            g.drawText("Attendi ~2s per il link protetto,\nnecessario per la fotocamera su Android",
                       static_cast<int>(qrBox.getX() + 6), static_cast<int>(qrBox.getCentreY() + 24.0f),
                       static_cast<int>(qrBox.getWidth() - 12), 26, juce::Justification::centred);
        }

        const juce::Rectangle<float> urlPill(leftPanel.getX() + 12.0f, qrBox.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, urlPill, 686, isHttps ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg, isHttps ? 0.45f : 0.90f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        g.drawText(urlStr, urlPill.toNearestInt(), juce::Justification::centred);

        const auto openBtn = getEyeModalUrlButtonBounds(w, h);
        MiMOLookAndFeel::drawSketchBox(g, openBtn, 687, MiMOLookAndFeel::crayonCyan, 0.55f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        g.drawText("Apri Link nel Browser", openBtn.toNearestInt(), juce::Justification::centred);

        const auto actBtn = getEyeModalActivatePhoneButtonBounds(w, h);
        MiMOLookAndFeel::drawSketchBox(g, actBtn, 693,
                                       comp.useSmartphoneEye ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::crayonPeach,
                                       0.70f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.0f, juce::Font::bold));
        const juce::String actText = comp.useSmartphoneEye
            ? "[FOTOCAMERA SMARTPHONE ATTIVA]"
            : "[USA FOTOCAMERA SMARTPHONE]";
        g.drawText(actText, actBtn.toNearestInt(), juce::Justification::centred);

        const juce::Rectangle<float> phoneStatusBox(leftPanel.getX() + 12.0f, actBtn.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 22.0f);
        const juce::Colour stCol = isPhoneStreaming ? MiMOLookAndFeel::crayonSage
                                                    : (isClientConnected ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg);
        MiMOLookAndFeel::drawSketchBox(g, phoneStatusBox, 694, stCol, 0.60f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        juce::String phoneStText;
        if (isPhoneStreaming)
            phoneStText = "[CONNESSO] Videocamera streaming attiva";
        else if (isClientConnected)
            phoneStText = "[COLLEGATO] Telefono pronto - tocca Consenti";
        else
            phoneStText = "[IN ATTESA] Inquadra il QR col telefono...";
        g.drawText(phoneStText, phoneStatusBox.toNearestInt(), juce::Justification::centred);

        juce::Rectangle<float> guideBox(leftPanel.getX() + 12.0f, phoneStatusBox.getBottom() + 6.0f, leftPanel.getWidth() - 24.0f, 85.0f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(9.0f, juce::Font::plain));
        g.drawText("Istruzioni di Connessione:\n"
                   "1. Inquadra il codice QR con lo smartphone.\n"
                   "2. Tocca 'Consenti & Attiva' per autorizzare la fotocamera.\n"
                   "3. Il PC riceve il video ed esegue l'analisi MediaPipe.",
                   guideBox.toNearestInt(), juce::Justification::topLeft);
    }

    MiMOLookAndFeel::drawSketchBox(g, rightPanel, 688, MiMOLookAndFeel::paperCard, 0.85f,
                                   juce::Colours::transparentBlack, 1.5f, false, MiMOLookAndFeel::paperCard);

    const bool isPhoneStream = (comp.eyeServer != nullptr && comp.eyeServer->isSmartphoneStreaming());
    const bool hasHandNow = comp.latestEyeState.hasHand;
    juce::Rectangle<float> statusRow(rightPanel.getX() + 14.0f, rightPanel.getY() + 10.0f, rightPanel.getWidth() - 28.0f, 22.0f);
    MiMOLookAndFeel::drawSketchBox(g, statusRow, 689,
                                   (hasHandNow || isPhoneStream) ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                   (hasHandNow || isPhoneStream) ? 0.55f : 0.70f);

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    juce::String handLabel;
    if (comp.latestEyeState.hasHand && comp.latestEyeState.hasSecondaryHand)
        handLabel = "2 Mani (DX + SX)";
    else if (comp.latestEyeState.hasHand)
        handLabel = (comp.latestEyeState.handedness == "Left") ? "Mano Sinistra" : "Mano Destra";
    else
        handLabel = "Nessuna Mano";

    const bool isMp = (comp.eyeServer != nullptr && comp.eyeServer->isMediaPipeActive());
    const juce::String engineTag = isMp ? " [IA MediaPipe]" : " [Sensore]";
    juce::String stStr;
    if (hasHandNow)
        stStr = "[TRACCIAMENTO ATTIVO" + engineTag + "] " + (comp.useSmartphoneEye ? juce::String("Camera Smartphone") : juce::String("HP Wide Vision HD (PC)")) + "  |  " + handLabel + "  |  " + (comp.eyePhalanxDetail == 0 ? "1 Pt (Palmo)" : comp.eyePhalanxDetail == 1 ? "6 Pt (5 Punte)" : "21 Falangi");
    else if (comp.useSmartphoneEye && isPhoneStream)
        stStr = "[CONNESSA] Videocamera Smartphone attiva - Mostra la mano nell'inquadratura...";
    else if (comp.useSmartphoneEye)
        stStr = "[ATTESA STREAM] Fotocamera Smartphone attiva - Tocca 'Consenti' sul telefono...";
    else
        stStr = "[ATTIVA] HP Wide Vision HD (PC) - Mostra la mano nell'inquadratura...";
    g.drawText(stStr, statusRow.toNearestInt(), juce::Justification::centred);

    const juce::Rectangle<float> skelBox(rightPanel.getX() + 14.0f, statusRow.getBottom() + 8.0f, rightPanel.getWidth() - 28.0f, 152.0f);
    {
        juce::Image liveImg;
        {
            juce::ScopedLock sl(comp.cameraLock);
            if (comp.latestWebcamImage.isValid())
                liveImg = comp.latestWebcamImage.createCopy();
        }

        MiMOLookAndFeel::drawSketchBox(g, skelBox, 690, MiMOLookAndFeel::paperBg, 0.90f, juce::Colours::transparentBlack, 1.2f);
        const auto innerSkelBox = skelBox.reduced(2.0f);

        if (comp.eyeShowVideo && liveImg.isValid())
        {
            const float imgW = static_cast<float>(liveImg.getWidth());
            const float imgH = static_cast<float>(liveImg.getHeight());
            const auto videoBox = juce::RectanglePlacement(juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize)
                .appliedTo(juce::Rectangle<float>(0.0f, 0.0f, imgW, imgH), innerSkelBox);

            g.saveState();
            g.reduceClipRegion(innerSkelBox.toNearestInt());
            g.setColour(juce::Colour(0xff181a20));
            g.fillRect(innerSkelBox);
            g.drawImage(liveImg, videoBox, juce::RectanglePlacement::centred);
            drawHandSkeleton(g, videoBox, comp.latestEyeState, true,
                             comp.isHandEnabled(false), comp.isHandEnabled(true),
                             comp.isHandTriggersEnabled(false), comp.isHandTriggersEnabled(true));
            g.restoreState();
        }
        else
        {
            g.saveState();
            g.reduceClipRegion(innerSkelBox.toNearestInt());
            drawHandSkeleton(g, innerSkelBox, comp.latestEyeState, false,
                             comp.isHandEnabled(false), comp.isHandEnabled(true),
                             comp.isHandTriggersEnabled(false), comp.isHandTriggersEnabled(true));
            g.restoreState();
        }
    }

    const float cfgStartY = skelBox.getBottom() + 8.0f;

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("1. MANO DA TRACCIARE:",
               static_cast<int>(rightPanel.getX() + 14), static_cast<int>(cfgStartY),
               145, 22, juce::Justification::centredLeft);

    static const char* handFilterLabels[3] = { "Entrambe", "Mano Destra", "Mano Sinistra" };
    for (int hf = 0; hf < 3; ++hf)
    {
        const auto b = getEyeModalHandFilterBounds(w, h, hf);
        const bool sel = (comp.eyeHandFilter == hf);
        MiMOLookAndFeel::drawSketchBox(g, b, 710 + hf,
                                       sel ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg,
                                       sel ? 0.70f : 0.45f, juce::Colours::transparentBlack,
                                       sel ? 1.6f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, sel ? juce::Font::bold : juce::Font::plain));
        g.drawText(handFilterLabels[hf], b.toNearestInt(), juce::Justification::centred);
    }

    const float phalY = cfgStartY + 28.0f;
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("2. DETTAGLIO FALANGI:",
               static_cast<int>(rightPanel.getX() + 14), static_cast<int>(phalY),
               145, 22, juce::Justification::centredLeft);

    static const char* phalanxLabels[3] = { "1 pt: Solo Palmo", "6 pt: 5 Punte Dita", "21 pt: Tutte Falangi" };
    for (int pd = 0; pd < 3; ++pd)
    {
        const auto b = getEyeModalPhalanxBounds(w, h, pd);
        const bool sel = (comp.eyePhalanxDetail == pd);
        MiMOLookAndFeel::drawSketchBox(g, b, 715 + pd,
                                       sel ? MiMOLookAndFeel::crayonOchre : MiMOLookAndFeel::paperBg,
                                       sel ? 0.70f : 0.45f, juce::Colours::transparentBlack,
                                       sel ? 1.6f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.5f, sel ? juce::Font::bold : juce::Font::plain));
        g.drawText(phalanxLabels[pd], b.toNearestInt(), juce::Justification::centred);
    }

    const float gestTitleY = phalY + 26.0f;
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawText("3. GESTURE D'INTERAZIONE (Uscite Cavi nel Canvas per Strumenti ed Effetti):",
               static_cast<int>(rightPanel.getX() + 14), static_cast<int>(gestTitleY),
               static_cast<int>(rightPanel.getWidth() - 28), 16, juce::Justification::centredLeft);

    const bool gestStates[4] = {
        comp.eyeGesturePos3D,
        comp.eyeGestureFistTilt && (comp.eyePhalanxDetail >= 1),
        comp.eyeGestureFingerFlex && (comp.eyePhalanxDetail >= 1),
        comp.eyeGesturePinchTrig && (comp.eyePhalanxDetail >= 1)
    };
    static const char* gestLabels[4] = {
        "[X/Y/Z] Posizione Mano 3D (3 Uscite)",
        "[Pugno/Rot] Pugno + Rotazione (2 Uscite)",
        "[Falangi] 4 Dita Indipendenti (4 Uscite)",
        "[Pinch] Tocca Pollice+Dita -> Suona Note"
    };
    const juce::Colour gestCols[4] = {
        MiMOLookAndFeel::crayonCyan,
        MiMOLookAndFeel::crayonSage,
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonPeach
    };

    for (int gIdx = 0; gIdx < 4; ++gIdx)
    {
        const auto b = getEyeModalGestureBounds(w, h, gIdx);
        const bool on = gestStates[gIdx];
        MiMOLookAndFeel::drawSketchBox(g, b, 720 + gIdx,
                                       on ? gestCols[gIdx] : MiMOLookAndFeel::paperBg,
                                       on ? 0.68f : 0.40f, juce::Colours::transparentBlack,
                                       on ? 1.6f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.2f, on ? juce::Font::bold : juce::Font::plain));
        const juce::String prefix = on ? "[ON] " : "[  ] ";
        g.drawText(prefix + gestLabels[gIdx], b.reduced(4.0f, 0.0f).toNearestInt(), juce::Justification::centredLeft);
    }

    static const char* fingerLabels[4] = { "Test Indice", "Test Medio", "Test Anulare", "Test Mignolo" };
    for (int i = 0; i < 4; ++i)
    {
        const auto btn = getEyeModalPinchTestBounds(w, h, i);
        const bool isAct = comp.latestEyeState.pinchActive[static_cast<size_t>(i)];
        MiMOLookAndFeel::drawSketchBox(g, btn, 695 + i,
                                       isAct ? gestCols[i] : MiMOLookAndFeel::paperBg,
                                       isAct ? 0.80f : 0.50f, juce::Colours::transparentBlack,
                                       isAct ? 1.8f : 1.0f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(9.0f, juce::Font::bold));
        const int flexPct = juce::roundToInt(comp.latestEyeState.fingerFlex[static_cast<size_t>(i + 1)] * 100.0f);
        g.drawText(juce::String(fingerLabels[i]) + " (" + juce::String(flexPct) + "%)",
                   btn.toNearestInt(), juce::Justification::centred);
    }
}

bool handleMouseDown(MainComponent& comp, const juce::Point<float>& pt)
{
    if (!comp.eyeDrawerOpen)
        return false;

    const int w = comp.getWidth();
    const int h = comp.getHeight();

    const auto mb = getEyeModalBounds(w, h);
    if (!mb.contains(pt) || getEyeModalCloseBounds(w, h).contains(pt))
    {
        comp.eyeDrawerOpen = false;
        comp.repaint();
        return true;
    }

    for (int t = 0; t < 2; ++t)
    {
        if (getEyeModalTabBounds(w, h, t).contains(pt))
        {
            comp.eyeDrawerTab = t;
            comp.repaint();
            return true;
        }
    }

    for (int hf = 0; hf < 3; ++hf)
    {
        if (getEyeModalHandFilterBounds(w, h, hf).contains(pt))
        {
            comp.eyeHandFilter = hf;
            comp.syncEyeNodeOutputsWithDetailSettings();
            comp.repaint();
            return true;
        }
    }

    for (int pd = 0; pd < 3; ++pd)
    {
        if (getEyeModalPhalanxBounds(w, h, pd).contains(pt))
        {
            comp.eyePhalanxDetail = pd;
            comp.syncEyeNodeOutputsWithDetailSettings();
            comp.repaint();
            return true;
        }
    }

    for (int gIdx = 0; gIdx < 4; ++gIdx)
    {
        if (getEyeModalGestureBounds(w, h, gIdx).contains(pt))
        {
            if (gIdx == 0) comp.eyeGesturePos3D = !comp.eyeGesturePos3D;
            else if (gIdx == 1) comp.eyeGestureFistTilt = !comp.eyeGestureFistTilt;
            else if (gIdx == 2) comp.eyeGestureFingerFlex = !comp.eyeGestureFingerFlex;
            else if (gIdx == 3) comp.eyeGesturePinchTrig = !comp.eyeGesturePinchTrig;

            if (gIdx == 1 || gIdx == 2 || gIdx == 3)
            {
                if (comp.eyePhalanxDetail == 0)
                    comp.eyePhalanxDetail = 2;
            }

            comp.syncEyeNodeOutputsWithDetailSettings();
            comp.repaint();
            return true;
        }
    }

    for (int p = 0; p < 4; ++p)
    {
        if (getEyeModalPinchTestBounds(w, h, p).contains(pt))
        {
            if (comp.eyeServer != nullptr)
                comp.eyeServer->injectTestPinch(p, 0.9f);
            comp.repaint();
            return true;
        }
    }

    if (comp.eyeDrawerTab == 0)
    {
        const auto items = comp.getVideoSourceItems();
        const int maxToShow = std::min(5, static_cast<int>(items.size()));
        for (int i = 0; i < maxToShow; ++i)
        {
            if (getEyeModalCameraOptionBounds(w, h, i).contains(pt))
            {
                const auto& it = items[i];
                comp.selectVideoSource(it.isSmartphone, it.pcDeviceIdx, it.label);
                return true;
            }
        }

        if (getEyeModalViewToggleBounds(w, h).contains(pt))
        {
            comp.eyeShowVideo = !comp.eyeShowVideo;
            comp.repaint();
            return true;
        }
    }
    else
    {
        if (getEyeModalUrlButtonBounds(w, h).contains(pt))
        {
            if (comp.eyeServer != nullptr)
                juce::URL(comp.eyeServer->getEyeUrl()).launchInDefaultBrowser();
            return true;
        }

        if (getEyeModalActivatePhoneButtonBounds(w, h).contains(pt))
        {
            comp.selectVideoSource(true);
            comp.eyeDrawerTab = 0;
            return true;
        }
    }

    return true;
}

} // namespace MiMOEyeDrawer
} // namespace mimo
