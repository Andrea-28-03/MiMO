#include "SidebarComponent.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>

namespace mimo
{

// =============================================================================
// Calcoli dei Rettangoli di Layout della Sidebar
// =============================================================================

juce::Rectangle<float> SidebarComponent::getSidebarToggleBounds(int compWidth, int /*compHeight*/, bool sidebarOpen)
{
    if (sidebarOpen)
        return { static_cast<float>(compWidth - 300), 16.0f, 32.0f, 32.0f };
    return { static_cast<float>(compWidth - 44), 16.0f, 32.0f, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getCameraButtonBounds(int compWidth, int /*compHeight*/, bool /*sidebarOpen*/)
{
    const float x = static_cast<float>(compWidth - 300) + 38.0f;
    const float w = 284.0f - 38.0f - 38.0f;
    return { x, 16.0f, w, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsButtonBounds(int compWidth, int /*compHeight*/, bool /*sidebarOpen*/)
{
    return { static_cast<float>(compWidth - 48), 16.0f, 32.0f, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsPanelBounds(int compWidth, int /*compHeight*/)
{
    return { static_cast<float>(compWidth - 316), 54.0f, 300.0f, 240.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsVolumeTrackBounds(int compWidth, int compHeight)
{
    const auto p = getSettingsPanelBounds(compWidth, compHeight);
    return { p.getX() + 14.0f, p.getY() + 102.0f, p.getWidth() - 28.0f, 20.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsVolDownBtnBounds(int compWidth, int compHeight)
{
    const auto p = getSettingsPanelBounds(compWidth, compHeight);
    return { p.getX() + 14.0f, p.getY() + 130.0f, 54.0f, 26.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsVol100BtnBounds(int compWidth, int compHeight)
{
    const auto p = getSettingsPanelBounds(compWidth, compHeight);
    return { p.getX() + 74.0f, p.getY() + 130.0f, 68.0f, 26.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsVol150BtnBounds(int compWidth, int compHeight)
{
    const auto p = getSettingsPanelBounds(compWidth, compHeight);
    return { p.getX() + 148.0f, p.getY() + 130.0f, 78.0f, 26.0f };
}

juce::Rectangle<float> SidebarComponent::getSettingsVolUpBtnBounds(int compWidth, int compHeight)
{
    const auto p = getSettingsPanelBounds(compWidth, compHeight);
    return { p.getX() + 232.0f, p.getY() + 130.0f, 54.0f, 26.0f };
}

juce::Rectangle<float> SidebarComponent::getIntegratedHeaderBounds(int compWidth, int /*compHeight*/, float sidebarScrollY)
{
    const float x = static_cast<float>(compWidth - 300);
    const float y = 58.0f - sidebarScrollY;
    return { x, y, 284.0f, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getIntegratedBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen)
{
    const auto header = getIntegratedHeaderBounds(compWidth, compHeight, sidebarScrollY);
    const float h = cameraSettingsOpen ? 300.0f : 156.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> SidebarComponent::getDrumSwitchBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen)
{
    const auto body = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, cameraSettingsOpen);
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float drumY = body.getY() + 6.0f;
    return { cardX + cardW - 74.0f, drumY + 5.0f, 68.0f, 18.0f };
}

juce::Rectangle<float> SidebarComponent::getMicSwitchBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen)
{
    const auto body = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, cameraSettingsOpen);
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float micY = body.getY() + 54.0f;
    return { cardX + cardW - 74.0f, micY + 5.0f, 68.0f, 18.0f };
}

juce::Rectangle<float> SidebarComponent::getCameraSwitchBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen)
{
    const auto body = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, cameraSettingsOpen);
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float camY = body.getY() + 102.0f;
    return { cardX + cardW - 74.0f, camY + 5.0f, 68.0f, 18.0f };
}

juce::Rectangle<float> SidebarComponent::getCameraSettingsButtonBounds(int compWidth, int compHeight, float sidebarScrollY, bool cameraSettingsOpen)
{
    const auto body = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, cameraSettingsOpen);
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float camY = body.getY() + 102.0f;
    return { cardX + cardW - 74.0f, camY + 24.0f, 68.0f, 16.0f };
}

juce::Rectangle<float> SidebarComponent::getCameraModePillBounds(int compWidth, int compHeight, float sidebarScrollY, int modeIdx)
{
    const auto body = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, true);
    const float cardX = body.getX() + 8.0f;
    const float camY = body.getY() + 102.0f;
    if (modeIdx == 0) return { cardX + 66.0f,  camY + 46.0f, 62.0f, 18.0f };
    if (modeIdx == 1) return { cardX + 132.0f, camY + 46.0f, 44.0f, 18.0f };
    return { cardX + 180.0f, camY + 46.0f, 78.0f, 18.0f };
}

juce::Rectangle<float> SidebarComponent::getCameraMirrorCheckboxBounds(int compWidth, int compHeight, float sidebarScrollY)
{
    const auto body = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, true);
    const float cardX = body.getX() + 8.0f;
    const float cardW = body.getWidth() - 16.0f;
    const float camY = body.getY() + 102.0f;
    return { cardX + 10.0f, camY + 70.0f, cardW - 20.0f, 16.0f };
}

juce::Rectangle<float> SidebarComponent::getInstrumentsHeaderBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen)
{
    const float x = static_cast<float>(compWidth - 300);
    const auto integH = getIntegratedHeaderBounds(compWidth, compHeight, sidebarScrollY);
    float y = integH.getBottom();
    if (integratedOpen)
    {
        const auto integB = getIntegratedBodyBounds(compWidth, compHeight, sidebarScrollY, cameraSettingsOpen);
        y = integB.getBottom();
    }
    y += 10.0f;
    return { x, y, 284.0f, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getInstrumentsBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, size_t instrumentCount)
{
    const auto header = getInstrumentsHeaderBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen);
    const float count = static_cast<float>(instrumentCount);
    const float h = count * 40.0f + 8.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> SidebarComponent::getEffectsHeaderBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount)
{
    const auto instH = getInstrumentsHeaderBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen);
    float y = instH.getBottom();
    if (instOpen)
    {
        const auto instB = getInstrumentsBodyBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen, instrumentCount);
        y = instB.getBottom();
    }
    y += 10.0f;
    return { instH.getX(), y, 284.0f, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getEffectsBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount, size_t effectCount)
{
    const auto header = getEffectsHeaderBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen, instOpen, instrumentCount);
    const float count = static_cast<float>(effectCount);
    const float h = count * 42.0f + 8.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

juce::Rectangle<float> SidebarComponent::getUtilitiesHeaderBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount, bool fxOpen, size_t effectCount)
{
    const auto fxH = getEffectsHeaderBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen, instOpen, instrumentCount);
    float y = fxH.getBottom();
    if (fxOpen)
    {
        const auto fxB = getEffectsBodyBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen, instOpen, instrumentCount, effectCount);
        y = fxB.getBottom();
    }
    y += 10.0f;
    return { fxH.getX(), y, 284.0f, 32.0f };
}

juce::Rectangle<float> SidebarComponent::getUtilitiesBodyBounds(int compWidth, int compHeight, float sidebarScrollY, bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instrumentCount, bool fxOpen, size_t effectCount, size_t utilCount)
{
    const auto header = getUtilitiesHeaderBounds(compWidth, compHeight, sidebarScrollY, integratedOpen, cameraSettingsOpen, instOpen, instrumentCount, fxOpen, effectCount);
    const float count = static_cast<float>(utilCount);
    const float h = count * 40.0f + 8.0f;
    return { header.getX(), header.getBottom() + 4.0f, header.getWidth(), h };
}

float SidebarComponent::getSidebarContentHeight(bool integratedOpen, bool cameraSettingsOpen, bool instOpen, size_t instCount, bool fxOpen, size_t fxCount, bool utilOpen, size_t utilCount)
{
    float h = 0.0f;
    h += 32.0f + 4.0f + (integratedOpen ? (cameraSettingsOpen ? 300.0f : 156.0f) : 0.0f) + 10.0f;
    h += 32.0f + 4.0f + (instOpen ? (static_cast<float>(instCount) * 40.0f + 8.0f) : 0.0f) + 10.0f;
    h += 32.0f + 4.0f + (fxOpen ? (static_cast<float>(fxCount) * 42.0f + 8.0f) : 0.0f) + 10.0f;
    h += 32.0f + 4.0f + (utilOpen ? (static_cast<float>(utilCount) * 40.0f + 8.0f) : 0.0f) + 24.0f;
    return h;
}

float SidebarComponent::getSidebarMaxScroll(int compHeight, float contentHeight)
{
    const float viewportH = static_cast<float>(compHeight) - 58.0f - 16.0f;
    return std::max(0.0f, contentHeight - viewportH);
}

juce::Rectangle<float> SidebarComponent::getSidebarViewportBounds(int compWidth, int compHeight)
{
    const float x = static_cast<float>(compWidth - 308);
    const float y = 54.0f;
    const float w = 308.0f;
    const float h = std::max(10.0f, static_cast<float>(compHeight) - 54.0f);
    return { x, y, w, h };
}

// =============================================================================
// Rendering della Sidebar Destra
// =============================================================================

void SidebarComponent::paintRightSidebar(juce::Graphics& g, MainComponent& comp)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    // 1. Quadratino con freccina a sinistra
    const auto toggleRect = getSidebarToggleBounds(w, h, comp.sidebarOpen);
    MiMOLookAndFeel::drawSketchBox(g, toggleRect, 301,
                                   comp.sidebarOpen ? MiMOLookAndFeel::paperCard : MiMOLookAndFeel::crayonOchre,
                                   comp.sidebarOpen ? 0.35f : 0.65f, juce::Colours::transparentBlack, 1.6f);

    {
        const float cx = toggleRect.getCentreX();
        const float cy = toggleRect.getCentreY();
        juce::Path p;
        if (comp.sidebarOpen)
        {
            p.addTriangle(cx - 3.5f, cy - 5.5f, cx - 3.5f, cy + 5.5f, cx + 4.5f, cy);
        }
        else
        {
            p.addTriangle(cx + 3.5f, cy - 5.5f, cx + 3.5f, cy + 5.5f, cx - 4.5f, cy);
        }
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.fillPath(p);
    }

    if (!comp.sidebarOpen)
        return;

    // 2. Pulsante Fotocamera Polaroid
    const auto camBtn = getCameraButtonBounds(w, h, comp.sidebarOpen);
    MiMOLookAndFeel::drawSketchBox(g, camBtn, 302, MiMOLookAndFeel::crayonOchre, 0.60f, juce::Colours::transparentBlack, 1.8f);
    const juce::Point<float> c = camBtn.getCentre();
    MiMOLookAndFeel::drawCameraIcon(g, { c.x - 14.0f, c.y - 11.0f, 28.0f, 22.0f });

    // 3. Pulsante Ingranaggio Impostazioni
    const auto gearBtn = getSettingsButtonBounds(w, h, comp.sidebarOpen);
    MiMOLookAndFeel::drawSketchBox(g, gearBtn, 304,
                                   comp.settingsOpen ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                   comp.settingsOpen ? 0.60f : 0.35f, juce::Colours::transparentBlack, 1.6f);

    {
        const float gx = gearBtn.getCentreX();
        const float gy = gearBtn.getCentreY();
        const float rOut = 9.0f;
        const float rIn = 6.0f;
        juce::Path gear;
        for (int tooth = 0; tooth < 8; ++tooth)
        {
            const float a0 = static_cast<float>(tooth) * juce::MathConstants<float>::twoPi / 8.0f;
            const float a1 = a0 + 0.35f;
            const float a2 = a0 + juce::MathConstants<float>::twoPi / 8.0f * 0.5f;
            const float a3 = a2 + 0.35f;

            if (tooth == 0)
                gear.startNewSubPath(gx + rIn * std::cos(a0), gy + rIn * std::sin(a0));
            else
                gear.lineTo(gx + rIn * std::cos(a0), gy + rIn * std::sin(a0));

            gear.lineTo(gx + rOut * std::cos(a1), gy + rOut * std::sin(a1));
            gear.lineTo(gx + rOut * std::cos(a2), gy + rOut * std::sin(a2));
            gear.lineTo(gx + rIn * std::cos(a3), gy + rIn * std::sin(a3));
        }
        gear.closeSubPath();
        g.setColour(comp.settingsOpen ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::fountainInk);
        g.strokePath(gear, juce::PathStrokeType(1.6f));
        g.drawEllipse(gx - 3.0f, gy - 3.0f, 6.0f, 6.0f, 1.4f);
    }

    // 4. Clip Region e Viewport di Scorrimento
    const auto viewportBounds = getSidebarViewportBounds(w, h);
    juce::Graphics::ScopedSaveState clipState(g);
    g.reduceClipRegion(viewportBounds.toNearestInt());

    // Sezione 0: Moduli Integrati
    {
        const auto hdr = getIntegratedHeaderBounds(w, h, comp.sidebarScrollY);
        MiMOLookAndFeel::drawSketchBox(g, hdr, 310, MiMOLookAndFeel::crayonCyan, 0.40f, juce::Colours::transparentBlack, 1.6f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(comp.integratedSectionOpen ? "v  MODULI HARDWARE PC" : ">  MODULI HARDWARE PC",
                   static_cast<int>(hdr.getX() + 10), static_cast<int>(hdr.getY()), static_cast<int>(hdr.getWidth() - 20), static_cast<int>(hdr.getHeight()),
                   juce::Justification::centredLeft);

        if (comp.integratedSectionOpen)
        {
            const auto body = getIntegratedBodyBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen);
            MiMOLookAndFeel::drawSketchBox(g, body, 311, MiMOLookAndFeel::paperBg, 0.50f, juce::Colours::transparentBlack, 1.2f);

            const float cardX = body.getX() + 8.0f;
            const float cardW = body.getWidth() - 16.0f;

            // Scheda 1: Batteria Percussiva
            const float drumY = body.getY() + 6.0f;
            juce::Rectangle<float> drumCard(cardX, drumY, cardW, 42.0f);
            MiMOLookAndFeel::drawSketchBox(g, drumCard, 312, MiMOLookAndFeel::paperCard, 0.45f, juce::Colours::transparentBlack, 1.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("Batteria MIDI / Tastiera", static_cast<int>(cardX + 8), static_cast<int>(drumY + 5), 180, 16, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText("4 Pad / Tasti QWER", static_cast<int>(cardX + 8), static_cast<int>(drumY + 22), 180, 14, juce::Justification::centredLeft);

            const auto drumSw = getDrumSwitchBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen);
            const bool drumOn = comp.isDrumInCanvas();
            MiMOLookAndFeel::drawSketchBox(g, drumSw, 313, drumOn ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperCard,
                                           drumOn ? 0.70f : 0.30f);
            g.setColour(drumOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText(drumOn ? "[ON] Canvas" : "+ Aggiungi", drumSw.toNearestInt(), juce::Justification::centred);

            // Scheda 2: Microfono Reale del Laptop
            const float micY = body.getY() + 54.0f;
            juce::Rectangle<float> micCard(cardX, micY, cardW, 42.0f);
            MiMOLookAndFeel::drawSketchBox(g, micCard, 314, MiMOLookAndFeel::paperCard, 0.45f, juce::Colours::transparentBlack, 1.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("Microfono Laptop", static_cast<int>(cardX + 8), static_cast<int>(micY + 5), 180, 16, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText(comp.getMicDeviceLabel(), static_cast<int>(cardX + 8), static_cast<int>(micY + 22), 180, 14, juce::Justification::centredLeft);

            const auto micSw = getMicSwitchBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen);
            const bool micOn = comp.isMicInCanvas();
            MiMOLookAndFeel::drawSketchBox(g, micSw, 315, micOn ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperCard,
                                           micOn ? 0.70f : 0.30f);
            g.setColour(micOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText(micOn ? "[ON] Canvas" : "+ Aggiungi", micSw.toNearestInt(), juce::Justification::centred);

            // Scheda 3: Videocamera Laptop (MiMO Eye)
            const float camY = body.getY() + 102.0f;
            const float camCardH = comp.cameraSettingsOpen ? 188.0f : 44.0f;
            juce::Rectangle<float> camCard(cardX, camY, cardW, camCardH);
            MiMOLookAndFeel::drawSketchBox(g, camCard, 316, MiMOLookAndFeel::paperCard, 0.45f, juce::Colours::transparentBlack, 1.2f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(12.0f, juce::Font::bold));
            g.drawText("MiMO Eye (Webcam)", static_cast<int>(cardX + 8), static_cast<int>(camY + 5), 180, 16, juce::Justification::centredLeft);
            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText(comp.getCameraDeviceLabel(), static_cast<int>(cardX + 8), static_cast<int>(camY + 22), 180, 14, juce::Justification::centredLeft);

            const auto camSw = getCameraSwitchBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen);
            const bool camOn = comp.isCameraInCanvas();
            MiMOLookAndFeel::drawSketchBox(g, camSw, 317, camOn ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperCard,
                                           camOn ? 0.70f : 0.30f);
            g.setColour(camOn ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(9.5f, juce::Font::bold));
            g.drawText(camOn ? "[ON] Canvas" : "+ Aggiungi", camSw.toNearestInt(), juce::Justification::centred);

            const auto camSetBtn = getCameraSettingsButtonBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen);
            MiMOLookAndFeel::drawSketchBox(g, camSetBtn, 318, MiMOLookAndFeel::crayonCyan, comp.cameraSettingsOpen ? 0.60f : 0.30f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(8.5f, juce::Font::bold));
            g.drawText(comp.cameraSettingsOpen ? "Chiudi" : "Opzioni", camSetBtn.toNearestInt(), juce::Justification::centred);

            if (comp.cameraSettingsOpen)
            {
                g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.25f));
                g.drawLine(cardX + 6.0f, camY + 44.0f, cardX + cardW - 6.0f, camY + 44.0f, 1.0f);

                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(10.5f, juce::Font::bold));
                g.drawText("Modalità:", static_cast<int>(cardX + 8), static_cast<int>(camY + 48), 54, 16, juce::Justification::centredLeft);

                const char* modeNames[3] = { "Movimento", "Luce", "Presenza" };
                for (int m = 0; m < 3; ++m)
                {
                    const auto pill = getCameraModePillBounds(w, h, comp.sidebarScrollY, m);
                    const bool isSel = (static_cast<int>(comp.cameraTrackingMode) == m);
                    MiMOLookAndFeel::drawSketchBox(g, pill, 320 + m, isSel ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperBg,
                                                   isSel ? 0.65f : 0.40f);
                    g.setColour(isSel ? MiMOLookAndFeel::fountainInk : MiMOLookAndFeel::inkMuted);
                    g.setFont(juce::Font(9.0f, isSel ? juce::Font::bold : juce::Font::plain));
                    g.drawText(modeNames[m], pill.toNearestInt(), juce::Justification::centred);
                }

                const auto mirBox = getCameraMirrorCheckboxBounds(w, h, comp.sidebarScrollY);
                juce::Rectangle<float> chk(mirBox.getX(), mirBox.getY() + 1.0f, 14.0f, 14.0f);
                MiMOLookAndFeel::drawSketchBox(g, chk, 325, comp.cameraMirror ? MiMOLookAndFeel::crayonSage : MiMOLookAndFeel::paperBg,
                                               comp.cameraMirror ? 0.70f : 0.35f);
                if (comp.cameraMirror)
                {
                    g.setColour(MiMOLookAndFeel::fountainInk);
                    g.setFont(juce::Font(10.0f, juce::Font::bold));
                    g.drawText("v", chk.toNearestInt(), juce::Justification::centred);
                }
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(10.0f, juce::Font::plain));
                g.drawText("Specchia orizzontalmente (Mirror)", static_cast<int>(mirBox.getX() + 20), static_cast<int>(mirBox.getY()),
                           static_cast<int>(mirBox.getWidth() - 20), 16, juce::Justification::centredLeft);
            }
        }
    }

    // Sezione 1: Catalogo Strumenti
    {
        const auto hdr = getInstrumentsHeaderBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen);
        MiMOLookAndFeel::drawSketchBox(g, hdr, 330, MiMOLookAndFeel::crayonOchre, 0.40f, juce::Colours::transparentBlack, 1.6f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(comp.instrumentsSectionOpen ? "v  STRUMENTI SONORI" : ">  STRUMENTI SONORI",
                   static_cast<int>(hdr.getX() + 10), static_cast<int>(hdr.getY()), static_cast<int>(hdr.getWidth() - 20), static_cast<int>(hdr.getHeight()),
                   juce::Justification::centredLeft);

        if (comp.instrumentsSectionOpen)
        {
            const auto body = getInstrumentsBodyBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentCatalog.size());
            MiMOLookAndFeel::drawSketchBox(g, body, 331, MiMOLookAndFeel::paperBg, 0.50f, juce::Colours::transparentBlack, 1.2f);

            float iy = body.getY() + 4.0f;
            for (const auto& item : comp.instrumentCatalog)
            {
                juce::Rectangle<float> itemCard(body.getX() + 8.0f, iy, body.getWidth() - 16.0f, 34.0f);
                MiMOLookAndFeel::drawSketchBox(g, itemCard, item.id.hashCode(), item.colour, 0.45f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(11.5f, juce::Font::bold));
                g.drawText(item.name, static_cast<int>(itemCard.getX() + 8), static_cast<int>(itemCard.getY() + 3), 160, 15, juce::Justification::centredLeft);
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(9.5f, juce::Font::plain));
                g.drawText(item.hint, static_cast<int>(itemCard.getX() + 8), static_cast<int>(itemCard.getY() + 17), 160, 13, juce::Justification::centredLeft);

                g.setFont(juce::Font(9.0f, juce::Font::italic));
                g.drawText("Trascina", static_cast<int>(itemCard.getRight() - 56), static_cast<int>(itemCard.getY()), 48, static_cast<int>(itemCard.getHeight()), juce::Justification::centredRight);

                iy += 40.0f;
            }
        }
    }

    // Sezione 2: Catalogo Effetti
    {
        const auto hdr = getEffectsHeaderBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size());
        MiMOLookAndFeel::drawSketchBox(g, hdr, 340, MiMOLookAndFeel::crayonCherry, 0.40f, juce::Colours::transparentBlack, 1.6f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(comp.effectsSectionOpen ? "v  EFFETTI & FILTRI" : ">  EFFETTI & FILTRI",
                   static_cast<int>(hdr.getX() + 10), static_cast<int>(hdr.getY()), static_cast<int>(hdr.getWidth() - 20), static_cast<int>(hdr.getHeight()),
                   juce::Justification::centredLeft);

        if (comp.effectsSectionOpen)
        {
            const auto body = getEffectsBodyBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectCatalog.size());
            MiMOLookAndFeel::drawSketchBox(g, body, 341, MiMOLookAndFeel::paperBg, 0.50f, juce::Colours::transparentBlack, 1.2f);

            float ey = body.getY() + 4.0f;
            for (const auto& item : comp.effectCatalog)
            {
                juce::Rectangle<float> itemCard(body.getX() + 8.0f, ey, body.getWidth() - 16.0f, 36.0f);
                MiMOLookAndFeel::drawSketchBox(g, itemCard, item.id.hashCode(), item.colour, 0.45f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(11.5f, juce::Font::bold));
                g.drawText(item.name, static_cast<int>(itemCard.getX() + 8), static_cast<int>(itemCard.getY() + 3), 160, 15, juce::Justification::centredLeft);
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(9.5f, juce::Font::plain));
                g.drawText(item.hint, static_cast<int>(itemCard.getX() + 8), static_cast<int>(itemCard.getY() + 18), 160, 13, juce::Justification::centredLeft);

                g.setFont(juce::Font(9.0f, juce::Font::italic));
                g.drawText("Trascina", static_cast<int>(itemCard.getRight() - 56), static_cast<int>(itemCard.getY()), 48, static_cast<int>(itemCard.getHeight()), juce::Justification::centredRight);

                ey += 42.0f;
            }
        }
    }

    // Sezione 3: Moduli di Utility
    {
        const auto hdr = getUtilitiesHeaderBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectsSectionOpen, comp.effectCatalog.size());
        MiMOLookAndFeel::drawSketchBox(g, hdr, 350, MiMOLookAndFeel::crayonSage, 0.40f, juce::Colours::transparentBlack, 1.6f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.0f, juce::Font::bold));
        g.drawText(comp.utilitiesSectionOpen ? "v  UTILITY & CONTROLLO" : ">  UTILITY & CONTROLLO",
                   static_cast<int>(hdr.getX() + 10), static_cast<int>(hdr.getY()), static_cast<int>(hdr.getWidth() - 20), static_cast<int>(hdr.getHeight()),
                   juce::Justification::centredLeft);

        if (comp.utilitiesSectionOpen)
        {
            const auto body = getUtilitiesBodyBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectsSectionOpen, comp.effectCatalog.size(), comp.utilityCatalog.size());
            MiMOLookAndFeel::drawSketchBox(g, body, 351, MiMOLookAndFeel::paperBg, 0.50f, juce::Colours::transparentBlack, 1.2f);

            float uy = body.getY() + 4.0f;
            for (const auto& item : comp.utilityCatalog)
            {
                juce::Rectangle<float> itemCard(body.getX() + 8.0f, uy, body.getWidth() - 16.0f, 34.0f);
                MiMOLookAndFeel::drawSketchBox(g, itemCard, item.id.hashCode(), item.colour, 0.45f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(11.5f, juce::Font::bold));
                g.drawText(item.name, static_cast<int>(itemCard.getX() + 8), static_cast<int>(itemCard.getY() + 3), 160, 15, juce::Justification::centredLeft);
                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(9.5f, juce::Font::plain));
                g.drawText(item.hint, static_cast<int>(itemCard.getX() + 8), static_cast<int>(itemCard.getY() + 17), 160, 13, juce::Justification::centredLeft);

                g.setFont(juce::Font(9.0f, juce::Font::italic));
                g.drawText("Trascina", static_cast<int>(itemCard.getRight() - 56), static_cast<int>(itemCard.getY()), 48, static_cast<int>(itemCard.getHeight()), juce::Justification::centredRight);

                uy += 40.0f;
            }
        }
    }

    // 5. Barra di scorrimento laterale pastello
    const float contentH = getSidebarContentHeight(comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectsSectionOpen, comp.effectCatalog.size(), comp.utilitiesSectionOpen, comp.utilityCatalog.size());
    const float maxScroll = getSidebarMaxScroll(h, contentH);
    if (maxScroll > 0.0f)
    {
        const float sbX = static_cast<float>(w - 8);
        const float sbY = viewportBounds.getY() + 4.0f;
        const float sbH = viewportBounds.getHeight() - 8.0f;
        const float thumbH = std::max(24.0f, sbH * (viewportBounds.getHeight() / contentH));
        const float thumbY = sbY + (comp.sidebarScrollY / maxScroll) * (sbH - thumbH);

        g.setColour(MiMOLookAndFeel::inkMuted.withAlpha(0.20f));
        g.fillRoundedRectangle(sbX, sbY, 4.0f, sbH, 2.0f);
        g.setColour(MiMOLookAndFeel::crayonOchre.withAlpha(0.75f));
        g.fillRoundedRectangle(sbX, thumbY, 4.0f, thumbH, 2.0f);
    }

    // 6. Pannello Popup Impostazioni
    if (comp.settingsOpen)
        paintSettingsPanel(g, comp);
}

void SidebarComponent::paintSettingsPanel(juce::Graphics& g, MainComponent& comp)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();
    const auto panel = getSettingsPanelBounds(w, h);

    MiMOLookAndFeel::drawSketchBox(g, panel, 850, MiMOLookAndFeel::crayonCyan, 0.20f,
                                   MiMOLookAndFeel::crayonCyan, 2.0f, false, MiMOLookAndFeel::paperBg);

    // 1. Header Impostazioni
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.5f, juce::Font::bold));
    g.drawText("IMPOSTAZIONI HUB", static_cast<int>(panel.getX() + 14), static_cast<int>(panel.getY() + 10), 180, 20, juce::Justification::centredLeft);

    // Tasto X chiudi
    juce::Rectangle<float> closeBtn(panel.getRight() - 28.0f, panel.getY() + 8.0f, 20.0f, 20.0f);
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

    // 2. Pulsante Modalita Giorno / Notte
    juce::Rectangle<float> themeBtn(panel.getX() + 14.0f, panel.getY() + 34.0f, panel.getWidth() - 28.0f, 32.0f);
    MiMOLookAndFeel::drawSketchBox(g, themeBtn, 855,
                                   comp.isDarkMode ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::crayonOchre, 0.55f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(comp.isDarkMode ? "Modalita Notte [N] (Attiva)" : "Modalita Giorno [N] (Attiva)",
               themeBtn.toNearestInt(), juce::Justification::centred);

    // 3. Sezione Volume Interno Applicazione
    const float curVol = comp.appMasterVolume.load();
    const int volPercent = static_cast<int>(std::round(curVol * 100.0f));
    const float volDb = juce::Decibels::gainToDecibels(curVol, -48.0f);

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(11.5f, juce::Font::bold));
    g.drawText("Volume Master Hub", static_cast<int>(panel.getX() + 14), static_cast<int>(panel.getY() + 74), 140, 18, juce::Justification::centredLeft);

    g.setColour(curVol > 1.05f ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    const juce::String volStr = juce::String(volPercent) + "% (" + (volDb >= 0.0f ? "+" : "") + juce::String(volDb, 1) + " dB)";
    g.drawText(volStr, static_cast<int>(panel.getRight() - 14.0f - 120.0f), static_cast<int>(panel.getY() + 74), 120, 18, juce::Justification::centredRight);

    // Barra Traccia Volume
    const auto track = getSettingsVolumeTrackBounds(w, h);
    MiMOLookAndFeel::drawSketchBox(g, track, 860, MiMOLookAndFeel::paperBg, 0.65f, juce::Colours::transparentBlack, 1.2f);

    const float norm = juce::jlimit(0.0f, 1.0f, curVol / 2.0f);
    const float fillW = track.getWidth() * norm;
    if (fillW > 1.0f)
    {
        juce::Rectangle<float> fillRect(track.getX(), track.getY(), fillW, track.getHeight());
        g.setColour(curVol > 1.05f ? MiMOLookAndFeel::crayonCherry.withAlpha(0.70f) : MiMOLookAndFeel::crayonSage.withAlpha(0.70f));
        g.fillRoundedRectangle(fillRect, 3.0f);
    }

    // Linea marcatore 100% (gain unitario nominale)
    const float unityX = track.getX() + track.getWidth() * 0.50f;
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.drawLine(unityX, track.getY() - 2.0f, unityX, track.getBottom() + 2.0f, 1.5f);

    // Cursore circolare pastello
    const float knobX = track.getX() + fillW;
    const float knobY = track.getCentreY();
    g.setColour(MiMOLookAndFeel::paperCard);
    g.fillEllipse(knobX - 8.0f, knobY - 8.0f, 16.0f, 16.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.drawEllipse(knobX - 8.0f, knobY - 8.0f, 16.0f, 16.0f, 1.6f);

    // 4. Pulsanti Rapidi Preset Volume
    auto drawPresetBtn = [&](juce::Rectangle<float> r, const juce::String& text, int id)
    {
        MiMOLookAndFeel::drawSketchBox(g, r, id, MiMOLookAndFeel::paperCard, 0.40f, juce::Colours::transparentBlack, 1.2f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        g.drawText(text, r.toNearestInt(), juce::Justification::centred);
    };

    drawPresetBtn(getSettingsVolDownBtnBounds(w, h), "-10%", 870);
    drawPresetBtn(getSettingsVol100BtnBounds(w, h), "100%", 871);
    drawPresetBtn(getSettingsVol150BtnBounds(w, h), "Boost 150%", 872);
    drawPresetBtn(getSettingsVolUpBtnBounds(w, h), "+10%", 873);

    // 5. Pulsante Guida Scorciatoie da Tastiera
    juce::Rectangle<float> shortcutsBtn(panel.getX() + 14.0f, panel.getY() + 164.0f, panel.getWidth() - 28.0f, 28.0f);
    MiMOLookAndFeel::drawSketchBox(g, shortcutsBtn, 888, MiMOLookAndFeel::crayonCyan, 0.40f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(11.0f, juce::Font::bold));
    g.drawText("Guida Scorciatoie da Tastiera...", shortcutsBtn.toNearestInt(), juce::Justification::centred);

    // 6. Nota di sicurezza DSP
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(9.0f, juce::Font::plain));
    g.drawText("Limitatore analogico tanh attivo. Valori > 100% amplificano i suoni deboli.",
               static_cast<int>(panel.getX() + 14), static_cast<int>(panel.getY() + 198), static_cast<int>(panel.getWidth() - 28), 16, juce::Justification::centred);
}

// =============================================================================
// Gestione Eventi Mouse della Sidebar
// =============================================================================

bool SidebarComponent::handleMouseDown(MainComponent& comp, const juce::Point<float>& pt, const juce::MouseEvent& e)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    // 1. Toggle Quadratino Sidebar
    if (getSidebarToggleBounds(w, h, comp.sidebarOpen).contains(pt))
    {
        comp.sidebarOpen = !comp.sidebarOpen;
        if (!comp.sidebarOpen)
            comp.settingsOpen = false;
        comp.repaint();
        return true;
    }

    if (!comp.sidebarOpen)
        return false;

    // 2. Se il pannello impostazioni e' aperto
    if (comp.settingsOpen)
    {
        const auto panel = getSettingsPanelBounds(w, h);
        if (panel.contains(pt))
        {
            juce::Rectangle<float> closeBtn(panel.getRight() - 28.0f, panel.getY() + 8.0f, 20.0f, 20.0f);
            if (closeBtn.contains(pt))
            {
                comp.settingsOpen = false;
                comp.saveSettings();
                comp.repaint();
                return true;
            }

            juce::Rectangle<float> themeBtn(panel.getX() + 14.0f, panel.getY() + 34.0f, panel.getWidth() - 28.0f, 32.0f);
            if (themeBtn.contains(pt))
            {
                comp.toggleDarkMode();
                comp.saveSettings();
                return true;
            }

            const auto volTrack = getSettingsVolumeTrackBounds(w, h);
            if (volTrack.expanded(2.0f, 6.0f).contains(pt))
            {
                comp.dragMode = MainComponent::DragMode::DragSettingsVolume;
                const float norm = juce::jlimit(0.0f, 1.0f, (pt.x - volTrack.getX()) / volTrack.getWidth());
                comp.setAppMasterVolume(norm * 2.0f);
                return true;
            }

            if (getSettingsVolDownBtnBounds(w, h).contains(pt))
            {
                comp.setAppMasterVolume(comp.appMasterVolume.load() - 0.10f);
                comp.saveSettings();
                return true;
            }
            if (getSettingsVol100BtnBounds(w, h).contains(pt))
            {
                comp.setAppMasterVolume(1.0f);
                comp.saveSettings();
                return true;
            }
            if (getSettingsVol150BtnBounds(w, h).contains(pt))
            {
                comp.setAppMasterVolume(1.5f);
                comp.saveSettings();
                return true;
            }
            if (getSettingsVolUpBtnBounds(w, h).contains(pt))
            {
                comp.setAppMasterVolume(comp.appMasterVolume.load() + 0.10f);
                comp.saveSettings();
                return true;
            }

            juce::Rectangle<float> shortcutsBtn(panel.getX() + 14.0f, panel.getY() + 164.0f, panel.getWidth() - 28.0f, 28.0f);
            if (shortcutsBtn.contains(pt))
            {
                comp.settingsOpen = false;
                comp.shortcutsModalOpen = true;
                comp.repaint();
                return true;
            }

            return true;
        }
        else
        {
            comp.settingsOpen = false;
            comp.saveSettings();
            comp.repaint();
        }
    }

    // 3. Tasto Fotocamera Polaroid
    if (getCameraButtonBounds(w, h, comp.sidebarOpen).contains(pt))
    {
        comp.triggerSnapPolaroid();
        return true;
    }

    // 4. Tasto Ingranaggio Impostazioni
    if (getSettingsButtonBounds(w, h, comp.sidebarOpen).contains(pt))
    {
        comp.settingsOpen = !comp.settingsOpen;
        comp.repaint();
        return true;
    }

    // 5. Header e Switch Sezione Moduli Integrati
    if (getIntegratedHeaderBounds(w, h, comp.sidebarScrollY).contains(pt))
    {
        comp.integratedSectionOpen = !comp.integratedSectionOpen;
        comp.repaint();
        return true;
    }

    if (comp.integratedSectionOpen)
    {
        if (getDrumSwitchBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen).contains(pt))
        {
            comp.toggleDrumInCanvas();
            return true;
        }
        if (getMicSwitchBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen).contains(pt))
        {
            comp.toggleMicInCanvas();
            return true;
        }
        if (getCameraSwitchBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen).contains(pt))
        {
            comp.toggleCameraInCanvas();
            return true;
        }
        if (getCameraSettingsButtonBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen).contains(pt))
        {
            comp.cameraSettingsOpen = !comp.cameraSettingsOpen;
            comp.repaint();
            return true;
        }
        if (comp.cameraSettingsOpen)
        {
            for (int m = 0; m < 3; ++m)
            {
                if (getCameraModePillBounds(w, h, comp.sidebarScrollY, m).contains(pt))
                {
                    comp.cameraTrackingMode = static_cast<MainComponent::CameraTrackingMode>(m);
                    comp.repaint();
                    return true;
                }
            }
            if (getCameraMirrorCheckboxBounds(w, h, comp.sidebarScrollY).contains(pt))
            {
                comp.cameraMirror = !comp.cameraMirror;
                comp.repaint();
                return true;
            }
        }

        const auto integBody = getIntegratedBodyBounds(w, h, comp.sidebarScrollY, comp.cameraSettingsOpen);
        const float cardX = integBody.getX() + 8.0f;
        const float cardW = integBody.getWidth() - 16.0f;

        // Clic sulla card della Batteria Hardware -> Trascina Modulo Batteria nel Canvas!
        const juce::Rectangle<float> drumCard(cardX, integBody.getY() + 6.0f, cardW, 44.0f);
        if (drumCard.contains(pt))
        {
            comp.draggedCatalogItem = { "hw-drum", "Hardware P1", "Modulo Percussioni Piezo", "1-5 Sensori", "Trigger piezo I2C", MiMOLookAndFeel::crayonSage, false, false, {} };
            comp.dragMode = MainComponent::DragMode::DragSidebarItem;
            comp.currentMousePos = pt;
            comp.repaint();
            return true;
        }

        // Clic sulla card del Microfono -> Trascina Microfono nel Canvas!
        const juce::Rectangle<float> micCard(cardX, integBody.getY() + 54.0f, cardW, 44.0f);
        if (micCard.contains(pt))
        {
            comp.draggedCatalogItem = { "integrated-mic", "Integrato", "Microfono Live & Pitch", "Audio In", "Trigger audio & Pitch CV", MiMOLookAndFeel::crayonOchre, false, true, {} };
            comp.dragMode = MainComponent::DragMode::DragSidebarItem;
            comp.currentMousePos = pt;
            comp.repaint();
            return true;
        }

        // Clic sulla card della Webcam -> Trascina Webcam nel Canvas!
        const juce::Rectangle<float> camCard(cardX, integBody.getY() + 102.0f, cardW, comp.cameraSettingsOpen ? 186.0f : 44.0f);
        if (camCard.contains(pt))
        {
            comp.draggedCatalogItem = { "integrated-webcam", "Integrato", "Webcam PC (Occhio)", "Laptop", "Sensore ottico webcam", MiMOLookAndFeel::crayonCyan, false, true, {} };
            comp.dragMode = MainComponent::DragMode::DragSidebarItem;
            comp.currentMousePos = pt;
            comp.repaint();
            return true;
        }

        if (integBody.contains(pt))
            return true;
    }

    // 6. Header e Catalogo Strumenti
    if (getInstrumentsHeaderBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen).contains(pt))
    {
        comp.instrumentsSectionOpen = !comp.instrumentsSectionOpen;
        comp.repaint();
        return true;
    }

    if (comp.instrumentsSectionOpen)
    {
        const auto body = getInstrumentsBodyBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentCatalog.size());
        float iy = body.getY() + 4.0f;
        for (const auto& item : comp.instrumentCatalog)
        {
            juce::Rectangle<float> itemCard(body.getX() + 8.0f, iy, body.getWidth() - 16.0f, 34.0f);
            if (itemCard.contains(pt))
            {
                comp.dragMode = MainComponent::DragMode::DragSidebarItem;
                comp.draggedCatalogItem = item;
                comp.currentMousePos = pt;
                comp.repaint();
                return true;
            }
            iy += 40.0f;
        }
    }

    // 7. Header e Catalogo Effetti
    if (getEffectsHeaderBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size()).contains(pt))
    {
        comp.effectsSectionOpen = !comp.effectsSectionOpen;
        comp.repaint();
        return true;
    }

    if (comp.effectsSectionOpen)
    {
        const auto body = getEffectsBodyBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectCatalog.size());
        float ey = body.getY() + 4.0f;
        for (const auto& item : comp.effectCatalog)
        {
            juce::Rectangle<float> itemCard(body.getX() + 8.0f, ey, body.getWidth() - 16.0f, 36.0f);
            if (itemCard.contains(pt))
            {
                comp.dragMode = MainComponent::DragMode::DragSidebarItem;
                comp.draggedCatalogItem = item;
                comp.currentMousePos = pt;
                comp.repaint();
                return true;
            }
            ey += 42.0f;
        }
    }

    // 8. Header e Catalogo Utility
    if (getUtilitiesHeaderBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectsSectionOpen, comp.effectCatalog.size()).contains(pt))
    {
        comp.utilitiesSectionOpen = !comp.utilitiesSectionOpen;
        comp.repaint();
        return true;
    }

    if (comp.utilitiesSectionOpen)
    {
        const auto body = getUtilitiesBodyBounds(w, h, comp.sidebarScrollY, comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectsSectionOpen, comp.effectCatalog.size(), comp.utilityCatalog.size());
        float uy = body.getY() + 4.0f;
        for (const auto& item : comp.utilityCatalog)
        {
            juce::Rectangle<float> itemCard(body.getX() + 8.0f, uy, body.getWidth() - 16.0f, 34.0f);
            if (itemCard.contains(pt))
            {
                comp.dragMode = MainComponent::DragMode::DragSidebarItem;
                comp.draggedCatalogItem = item;
                comp.currentMousePos = pt;
                comp.repaint();
                return true;
            }
            uy += 40.0f;
        }
    }

    // Se il click è comunque dentro il viewport della sidebar
    if (getSidebarViewportBounds(w, h).contains(pt))
        return true;

    return false;
}

bool SidebarComponent::handleMouseWheel(MainComponent& comp, const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    if (comp.sidebarOpen && getSidebarViewportBounds(w, h).contains(e.position))
    {
        const float contentH = getSidebarContentHeight(comp.integratedSectionOpen, comp.cameraSettingsOpen, comp.instrumentsSectionOpen, comp.instrumentCatalog.size(), comp.effectsSectionOpen, comp.effectCatalog.size(), comp.utilitiesSectionOpen, comp.utilityCatalog.size());
        const float maxScroll = getSidebarMaxScroll(h, contentH);
        if (maxScroll > 0.0f)
        {
            comp.sidebarScrollY = juce::jlimit(0.0f, maxScroll, comp.sidebarScrollY - wheel.deltaY * 120.0f);
            comp.repaint();
            return true;
        }
    }
    return false;
}

bool SidebarComponent::handleMouseDrag(MainComponent& comp, const juce::Point<float>& pt)
{
    if (comp.dragMode == MainComponent::DragMode::DragSettingsVolume)
    {
        const auto track = getSettingsVolumeTrackBounds(comp.getWidth(), comp.getHeight());
        const float norm = juce::jlimit(0.0f, 1.0f, (pt.x - track.getX()) / track.getWidth());
        comp.setAppMasterVolume(norm * 2.0f);
        return true;
    }
    return false;
}

} // namespace mimo
