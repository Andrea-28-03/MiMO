#include "MiMOEyeDrawer.h"
#include "MainComponent.h"
#include "../Utils/qrcodegen.hpp"

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

        // Icona Occhio Vettoriale al centro del QR
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
                              float palmZ,
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
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 4 },       // Pollice
            { 0, 5 }, { 5, 6 }, { 6, 7 }, { 7, 8 },       // Indice
            { 5, 9 }, { 9, 10 }, { 10, 11 }, { 11, 12 },  // Medio
            { 9, 13 }, { 13, 14 }, { 14, 15 }, { 15, 16 },// Anulare
            { 13, 17 }, { 17, 18 }, { 18, 19 }, { 19, 20 },// Mignolo
            { 0, 17 }                                      // Base palmo
        };

        for (int b = 0; b < 21; ++b)
        {
            const auto pA = pts[static_cast<size_t>(bones[b][0])];
            const auto pB = pts[static_cast<size_t>(bones[b][1])];
            g.setColour(boneColour.withAlpha(overVideo ? 0.85f : 0.70f));
            g.drawLine(pA.x, pA.y, pB.x, pB.y, overVideo ? 2.2f : 1.8f);
        }

        // Punti delle giunture (falangi)
        for (size_t i = 0; i < 21; ++i)
        {
            const bool isTip = (i == 4 || i == 8 || i == 12 || i == 16 || i == 20);
            const float r = isTip ? 4.2f : 2.6f;
            const auto ptCol = isTip ? MiMOLookAndFeel::crayonOchre : boneColour;
            g.setColour(ptCol);
            g.fillEllipse(pts[i].x - r, pts[i].y - r, r * 2.0f, r * 2.0f);
        }

        // Indicatori Trigger Pinch (Pollice + Dita)
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
        // 1. TOP-LEVEL: MANO DESTRA (DX)
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

        // 2. TOP-LEVEL: MANO SINISTRA (SX)
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

} // namespace MiMOEyeDrawer
} // namespace mimo
