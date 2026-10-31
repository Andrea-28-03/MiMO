#include "BottomDock.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>

namespace mimo
{

juce::Rectangle<float> BottomDock::getDockTabBounds(int tabIdx, int compWidth, int compHeight)
{
    const float dockW = 450.0f;
    const float startX = (static_cast<float>(compWidth) - dockW) * 0.5f + 10.0f;
    const float tabW = (dockW - 20.0f) / 3.0f;
    const float y = static_cast<float>(compHeight - 48);
    return { startX + static_cast<float>(tabIdx) * tabW, y, tabW - 6.0f, 34.0f };
}

juce::Rectangle<float> BottomDock::getSlotBounds(int slotIdx, int compWidth, int compHeight, ActiveTab activeTab)
{
    if (activeTab == ActiveTab::Live)
    {
        const float totalW = static_cast<float>(compWidth - 170); // Riserva spazio per il Looper
        const float startX = 24.0f;
        const float gap = 12.0f;
        const float slotW = (totalW - 4.0f * gap) / 5.0f;
        const float y = static_cast<float>(compHeight - 250);
        const float slotH = 186.0f;
        return { startX + static_cast<float>(slotIdx) * (slotW + gap), y, slotW, slotH };
    }

    const float totalW = std::min(880.0f, static_cast<float>(compWidth - 40));
    const float startX = (static_cast<float>(compWidth) - totalW) * 0.5f;
    const float slotW = (totalW - 4.0f * 10.0f) / 5.0f;
    const float y = static_cast<float>(compHeight - 108);
    return { startX + static_cast<float>(slotIdx) * (slotW + 10.0f), y, slotW, 48.0f };
}

juce::Rectangle<float> BottomDock::getLooperBounds(int compWidth, int compHeight, ActiveTab activeTab)
{
    if (activeTab == ActiveTab::Live)
    {
        const float x = static_cast<float>(compWidth - 142);
        const float y = static_cast<float>(compHeight - 250);
        return { x, y, 118.0f, 186.0f };
    }
    return { static_cast<float>(compWidth) * 0.5f - 230.0f, static_cast<float>(compHeight - 166), 460.0f, 46.0f };
}

void BottomDock::paintBottomDockAndSlots(juce::Graphics& g, MainComponent& comp)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    const juce::Colour slotColours[5] = {
        MiMOLookAndFeel::crayonCherry,
        MiMOLookAndFeel::crayonCyan,
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonSage,
        MiMOLookAndFeel::crayonLilac
    };

    // 1. I 5 Slot Scena Hardware in basso (mostrati in Studio quando compare la Polaroid)
    if (comp.activeTab != ActiveTab::Live)
    {
        const bool showSlots = comp.hasPendingPolaroid || (comp.dragMode == MainComponent::DragMode::DragPolaroid);
        if (showSlots)
        {
            for (int i = 0; i < 5; ++i)
            {
                const auto r = getSlotBounds(i, w, h, comp.activeTab);
                const bool assigned = comp.sceneSlots[i].isValid;
                const bool isHover = (comp.hoveredSlotIndex == i);

                MiMOLookAndFeel::drawSketchBox(
                    g, r, 1000 + i,
                    assigned ? slotColours[i] : juce::Colours::transparentBlack,
                    assigned ? 0.65f : 0.0f,
                    isHover ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                    (comp.activeSceneSlot == i && assigned) ? 2.4f : 1.7f
                );

                g.setColour(slotColours[i]);
                g.fillEllipse(r.getX() + 10.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.drawEllipse(r.getX() + 10.0f, r.getCentreY() - 8.0f, 16.0f, 16.0f, 1.4f);

                g.setFont(juce::Font(12.5f, juce::Font::bold));
                g.drawText(assigned ? ("S" + juce::String(i + 1) + ": " + comp.sceneSlots[i].title)
                                    : ("SLOT SCENA " + juce::String(i + 1)),
                           static_cast<int>(r.getX() + 32), static_cast<int>(r.getY() + 6),
                           static_cast<int>(r.getWidth() - 38), 18, juce::Justification::centredLeft);

                g.setColour(MiMOLookAndFeel::inkMuted);
                g.setFont(juce::Font(10.5f, juce::Font::plain));
                g.drawText(assigned ? "Assegna qui" : "Trascina Polaroid qui",
                           static_cast<int>(r.getX() + 32), static_cast<int>(r.getY() + 24),
                           static_cast<int>(r.getWidth() - 38), 16, juce::Justification::centredLeft);
            }
        }
    }

    // 2. Dock Principale in Basso al Centro (Home/Studio, Live, Community)
    const float dockW = 450.0f;
    const juce::Rectangle<float> dockRect((static_cast<float>(w) - dockW) * 0.5f,
                                          static_cast<float>(h - 52), dockW, 42.0f);
    MiMOLookAndFeel::drawSketchBox(g, dockRect, 1010, juce::Colours::transparentBlack, 0.0f,
                                   juce::Colours::transparentBlack, 2.1f);

    const char* tabLabels[3] = { "Home / Studio", "Live (Album)", "Community" };
    const juce::Colour tabCols[3] = {
        MiMOLookAndFeel::crayonOchre,
        MiMOLookAndFeel::crayonCherry,
        MiMOLookAndFeel::crayonCyan
    };

    for (int i = 0; i < 3; ++i)
    {
        const auto tb = getDockTabBounds(i, w, h);
        const bool isActive = (static_cast<int>(comp.activeTab) == i);
        if (isActive)
            MiMOLookAndFeel::drawSketchBox(g, tb, 1020 + i, tabCols[i], 0.72f, juce::Colours::transparentBlack, 1.6f);

        juce::Rectangle<float> iconBox(tb.getX() + 8.0f, tb.getY() + 5.0f, 24.0f, 24.0f);
        if (i == 0)
            MiMOLookAndFeel::drawStudioIcon(g, iconBox, tabCols[i]);
        else if (i == 1)
            MiMOLookAndFeel::drawLiveIcon(g, iconBox, tabCols[i]);
        else
            MiMOLookAndFeel::drawCommunityIcon(g, iconBox, tabCols[i]);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, isActive ? juce::Font::bold : juce::Font::plain));
        g.drawText(tabLabels[i], static_cast<int>(tb.getX() + 34), static_cast<int>(tb.getY()),
                   static_cast<int>(tb.getWidth() - 38), static_cast<int>(tb.getHeight()),
                   juce::Justification::centredLeft);
    }
}

bool BottomDock::handleDockMouseDown(MainComponent& comp, const juce::Point<float>& pt)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    // 1. Controllo Click sul Dock di Navigazione Inferiore
    for (int i = 0; i < 3; ++i)
    {
        if (getDockTabBounds(i, w, h).contains(pt))
        {
            if (comp.editingSlotIndex >= 0 && i == 1)
            {
                comp.sceneSlots[comp.editingSlotIndex].snapshot = { comp.nodes, comp.cables };
                comp.editingSlotIndex = -1;
            }
            comp.activeTab = static_cast<ActiveTab>(i);
            comp.repaint();
            return true;
        }
    }

    // 2. Controllo Click sui 5 Slot Scena in basso
    const bool showSlots = comp.hasPendingPolaroid || (comp.dragMode == MainComponent::DragMode::DragPolaroid);
    if (showSlots && comp.activeTab != ActiveTab::Live)
    {
        for (int i = 0; i < 5; ++i)
        {
            if (getSlotBounds(i, w, h, comp.activeTab).contains(pt))
            {
                comp.activeSceneSlot = i;
                if (comp.hasPendingPolaroid)
                {
                    comp.assignPolaroidToSlot(i, comp.pendingPolaroid);
                    comp.hasPendingPolaroid = false;
                }
                else if (comp.sceneSlots[i].isValid)
                {
                    comp.nodes = comp.sceneSlots[i].snapshot.nodes;
                    comp.cables = comp.sceneSlots[i].snapshot.cables;
                }
                comp.repaint();
                return true;
            }
        }
    }

    // 3. Eventuale pulsante Quick-Edit "TORNA AL LIVE"
    if (comp.editingSlotIndex >= 0)
    {
        juce::Rectangle<float> saveBtn(static_cast<float>(w) * 0.5f - 195.0f, 16.0f, 390.0f, 44.0f);
        if (saveBtn.contains(pt))
        {
            comp.sceneSlots[comp.editingSlotIndex].snapshot = { comp.nodes, comp.cables };
            comp.editingSlotIndex = -1;
            comp.activeTab = ActiveTab::Live;
            comp.repaint();
            return true;
        }
    }

    return false;
}

} // namespace mimo
