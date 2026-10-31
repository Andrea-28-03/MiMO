#include "ShortcutsModal.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>

namespace mimo
{

juce::Rectangle<float> ShortcutsModal::getModalBounds(int compWidth, int compHeight)
{
    const float w = std::min(680.0f, static_cast<float>(compWidth) - 40.0f);
    const float h = std::min(520.0f, static_cast<float>(compHeight) - 60.0f);
    const float x = (static_cast<float>(compWidth) - w) * 0.5f;
    const float y = (static_cast<float>(compHeight) - h) * 0.5f;
    return { x, y, w, h };
}

void ShortcutsModal::paint(juce::Graphics& g, MainComponent& comp)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    // 1. Sfondo oscurato semitrasparente
    g.setColour(juce::Colours::black.withAlpha(0.65f));
    g.fillRect(0, 0, w, h);

    // 2. Card centrale modale
    const auto modal = getModalBounds(w, h);
    MiMOLookAndFeel::drawSketchBox(g, modal, 950, MiMOLookAndFeel::crayonCyan, 0.35f,
                                   MiMOLookAndFeel::fountainInk, 2.0f, false, MiMOLookAndFeel::paperBg);

    // 3. Header Modale
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(16.0f, juce::Font::bold));
    g.drawText("SCORCIATOIE DA TASTIERA & GESTI CANVAS",
               static_cast<int>(modal.getX() + 24), static_cast<int>(modal.getY() + 16),
               static_cast<int>(modal.getWidth() - 80), 22, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.5f, juce::Font::plain));
    g.drawText("Tutte le scorciatoie di MiMO Hub per una composizione fluida e professionale",
               static_cast<int>(modal.getX() + 24), static_cast<int>(modal.getY() + 38),
               static_cast<int>(modal.getWidth() - 80), 16, juce::Justification::centredLeft);

    // Pulsante di chiusura [X]
    juce::Rectangle<float> closeBtn(modal.getRight() - 38.0f, modal.getY() + 16.0f, 24.0f, 24.0f);
    MiMOLookAndFeel::drawSketchBox(g, closeBtn, 951, MiMOLookAndFeel::crayonCherry, 0.40f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(13.0f, juce::Font::bold));
    g.drawText("X", closeBtn.toNearestInt(), juce::Justification::centred);

    // Linea divisoria pastello
    g.setColour(MiMOLookAndFeel::crayonCyan.withAlpha(0.50f));
    g.drawLine(modal.getX() + 20.0f, modal.getY() + 60.0f, modal.getRight() - 20.0f, modal.getY() + 60.0f, 1.5f);

    // 4. Sezioni in Griglia 2 Colonne
    const float colW = (modal.getWidth() - 56.0f) * 0.5f;
    const float col1X = modal.getX() + 20.0f;
    const float col2X = col1X + colW + 16.0f;
    const float startY = modal.getY() + 72.0f;

    struct ShortcutEntry {
        juce::String key;
        juce::String desc;
    };

    auto drawSection = [&](float x, float y, float width, const juce::String& title, juce::Colour headerCol, const std::vector<ShortcutEntry>& items) -> float
    {
        const float headerH = 24.0f;
        const float itemH = 22.0f;
        const float totalH = headerH + static_cast<float>(items.size()) * itemH + 12.0f;

        juce::Rectangle<float> secBox(x, y, width, totalH);
        MiMOLookAndFeel::drawSketchBox(g, secBox, title.hashCode(), MiMOLookAndFeel::paperCard, 0.40f,
                                       headerCol.withAlpha(0.60f), 1.2f);

        // Header sezione
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(12.5f, juce::Font::bold));
        g.drawText(title, static_cast<int>(x + 10), static_cast<int>(y + 4), static_cast<int>(width - 20), 20, juce::Justification::centredLeft);

        float curY = y + headerH + 2.0f;
        for (const auto& it : items)
        {
            // Badge del tasto
            const float badgeW = std::max(60.0f, static_cast<float>(it.key.length()) * 8.5f + 14.0f);
            juce::Rectangle<float> badgeRect(x + 10.0f, curY, badgeW, 18.0f);
            MiMOLookAndFeel::drawSketchBox(g, badgeRect, it.key.hashCode(), headerCol, 0.35f,
                                           MiMOLookAndFeel::fountainInk.withAlpha(0.40f), 1.0f);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, juce::Font::bold));
            g.drawText(it.key, badgeRect.toNearestInt(), juce::Justification::centred);

            // Descrizione
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(11.0f, juce::Font::plain));
            const float descX = badgeRect.getRight() + 8.0f;
            const float descW = width - (descX - x) - 8.0f;
            g.drawText(it.desc, static_cast<int>(descX), static_cast<int>(curY), static_cast<int>(descW), 18, juce::Justification::centredLeft);

            curY += itemH;
        }

        return totalH;
    };

    // Colonna 1 - Creazione & Editing
    const std::vector<ShortcutEntry> editShortcuts = {
        { "Shift + A", "Quick-Add Menu (crea nodi al volo)" },
        { "Click Destro", "Menu Contestuale intelligente" },
        { "Ctrl + C / V", "Copia e Incolla nodi e cavi" },
        { "Ctrl + Z / Y", "Annulla e Ripristina storico (Undo/Redo)" },
        { "Ctrl + A", "Seleziona tutti i nodi del canvas" },
        { "Canc / Back", "Elimina elementi selezionati" }
    };
    const float hSec1 = drawSection(col1X, startY, colW, "CREAZIONE & EDITING", MiMOLookAndFeel::crayonCyan, editShortcuts);

    // Colonna 1 - Sub-Canvas
    const std::vector<ShortcutEntry> subCanvasShortcuts = {
        { "Doppio Click", "Ispeziona circuito interno nodo" },
        { "TAB", "Entra / Esci dal Sub-Canvas" },
        { "ESC", "Deseleziona tutto o chiudi finestra" }
    };
    drawSection(col1X, startY + hSec1 + 10.0f, colW, "SUB-CANVAS & STRUTTURA", MiMOLookAndFeel::crayonLilac, subCanvasShortcuts);

    // Colonna 2 - Navigazione & Zoom
    const std::vector<ShortcutEntry> navShortcuts = {
        { "Rotella Mouse", "Zoom continuo centrato sul cursore" },
        { "Due Dita / Drag DX", "Panning continuo 2D a 360 gradi" },
        { "Shift + Drag", "Selezione rettangolare (Lasso pastello)" },
        { "Shift + Click", "Aggiungi/rimuovi nodo dalla selezione" }
    };
    const float hSec3 = drawSection(col2X, startY, colW, "NAVIGAZIONE & ZOOM", MiMOLookAndFeel::crayonSage, navShortcuts);

    // Colonna 2 - Live & Performance
    const std::vector<ShortcutEntry> perfShortcuts = {
        { "Spazio", "1-Button Tape Looper (Rec/Play/Overdub)" },
        { "1 .. 5", "Attiva Scena Live / Assegna Polaroid" },
        { "N", "Alterna Modalita Giorno / Notte" }
    };
    drawSection(col2X, startY + hSec3 + 10.0f, colW, "PERFORMANCE & TEMA", MiMOLookAndFeel::crayonOchre, perfShortcuts);

    // Footer
    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(10.5f, juce::Font::italic));
    g.drawText("Premi ESC o fai click all'esterno della finestra per chiudere questa guida.",
               static_cast<int>(modal.getX()), static_cast<int>(modal.getBottom() - 28),
               static_cast<int>(modal.getWidth()), 20, juce::Justification::centred);
}

bool ShortcutsModal::handleMouseDown(MainComponent& comp, const juce::Point<float>& pt)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();
    const auto modal = getModalBounds(w, h);

    if (!modal.contains(pt))
    {
        comp.shortcutsModalOpen = false;
        comp.repaint();
        return true;
    }

    // Tasto [X]
    juce::Rectangle<float> closeBtn(modal.getRight() - 38.0f, modal.getY() + 16.0f, 24.0f, 24.0f);
    if (closeBtn.contains(pt))
    {
        comp.shortcutsModalOpen = false;
        comp.repaint();
        return true;
    }

    return true; // Consuma il click all'interno della modale per non toccare il canvas sotto
}

} // namespace mimo
