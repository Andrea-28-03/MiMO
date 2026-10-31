#include "LiveAlbumView.h"
#include "MainComponent.h"
#include "BottomDock.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>

namespace mimo
{

void LiveAlbumView::paintPolaroidOverlay(juce::Graphics& g, MainComponent& comp)
{
    g.fillAll(MiMOLookAndFeel::fountainInk.withAlpha(0.45f));

    const auto card = comp.getPolaroidCardBounds();
    MiMOLookAndFeel::drawSketchBox(g, card, 910, juce::Colours::transparentBlack, 0.0f,
                                   MiMOLookAndFeel::crayonOchre, 2.4f, false, MiMOLookAndFeel::polaroidWhite);

    // Finestra fotografica superiore della Polaroid
    juce::Rectangle<float> photoArea(card.getX() + 18.0f, card.getY() + 18.0f, card.getWidth() - 36.0f, 220.0f);
    g.setColour(MiMOLookAndFeel::paperBg);
    g.fillRect(photoArea);
    if (comp.pendingPolaroid.previewImage.isValid())
    {
        g.drawImageWithin(comp.pendingPolaroid.previewImage,
                          static_cast<int>(photoArea.getX()), static_cast<int>(photoArea.getY()),
                          static_cast<int>(photoArea.getWidth()), static_cast<int>(photoArea.getHeight()),
                          juce::RectanglePlacement::stretchToFit);
    }
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.drawRoundedRectangle(photoArea, 4.0f, 2.0f);

    // Badge countdown 3s se non ha ancora digitato
    if (!comp.polaroidHasTyped)
    {
        const int secs = std::max(1, (comp.polaroidCountdownTicks + 5) / 6);
        juce::Rectangle<float> timerBadge(photoArea.getRight() - 185.0f, photoArea.getBottom() - 30.0f, 178.0f, 24.0f);
        MiMOLookAndFeel::drawSketchBox(g, timerBadge, 912, MiMOLookAndFeel::crayonOchre, 0.88f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawText("Auto-salvataggio in " + juce::String(secs) + "s...", timerBadge.toNearestInt(), juce::Justification::centred);
    }

    // Bordo bianco inferiore scrivibile con font stile pennarello
    juce::Rectangle<float> markerZone(card.getX() + 18.0f, photoArea.getBottom() + 12.0f, card.getWidth() - 36.0f, 54.0f);
    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(22.0f, juce::Font::bold));
    g.drawText(comp.pendingPolaroid.title + " |", markerZone.removeFromTop(32).toNearestInt(), juce::Justification::centred);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(11.5f, juce::Font::plain));
    g.drawText(comp.polaroidHasTyped
                   ? "Premi INVIO o clicca fuori per salvare nell'Album"
                   : "Digita sulla tastiera per scrivere a pennarello, o trascina sui 5 Slot sotto!",
               static_cast<int>(card.getX() + 12), static_cast<int>(photoArea.getBottom() + 46),
               static_cast<int>(card.getWidth() - 24), 20, juce::Justification::centred);

    // 5 pulsanti rapidi sulla Polaroid per assegnare agli Slot 1..5
    const float btnW = (card.getWidth() - 36.0f - 4.0f * 6.0f) / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        juce::Rectangle<float> sb(card.getX() + 18.0f + static_cast<float>(i) * (btnW + 6.0f),
                                  card.getBottom() - 44.0f, btnW, 28.0f);
        MiMOLookAndFeel::drawSketchBox(g, sb, 920 + i, MiMOLookAndFeel::crayonOchre, 0.45f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.5f, juce::Font::bold));
        g.drawText("-> S" + juce::String(i + 1), sb.toNearestInt(), juce::Justification::centred);
    }
}

void LiveAlbumView::paintLiveAlbumView(juce::Graphics& g, MainComponent& comp)
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

    // 1. Header Live & Telemetria Hardware Rock Pi
    {
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(19.0f, juce::Font::bold));
        g.drawText("WORKSPACE LIVE  //  PERFORMANCE SUL PALCO", 28, 14, 480, 24, juce::Justification::centredLeft);

        juce::Rectangle<float> rockPill(500.0f, 15.0f, 185.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, rockPill, 930, MiMOLookAndFeel::crayonSage, 0.70f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(11.0f, juce::Font::bold));
        g.drawText("ROCK PI I2C SINCRONIZZATA", rockPill.toNearestInt(), juce::Justification::centred);

        juce::Rectangle<float> helpPill(static_cast<float>(w - 480), 15.0f, 456.0f, 22.0f);
        MiMOLookAndFeel::drawSketchBox(g, helpPill, 931, MiMOLookAndFeel::paperCard, 0.60f);
        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(10.5f, juce::Font::plain));
        g.drawText("Tasti [1..5] = Suona  -  Hold (1s) = Assegna  -  [Spazio] = Looper  -  Doppio Clic = Quick-Edit",
                   helpPill.toNearestInt(), juce::Justification::centred);

        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(12.5f, juce::Font::plain));
        g.drawText("5 Grandi Slot Scena pilotabili istantaneamente (<1ms audio spillover) dai tasti fisici della scocca o da tastiera.",
                   28, 38, 800, 18, juce::Justification::centredLeft);
    }

    // 2. L'Album (Area Superiore / Centrale)
    juce::Rectangle<float> albumBox(24.0f, 62.0f, static_cast<float>(w - 48), static_cast<float>(h - 322));
    MiMOLookAndFeel::drawSketchBox(g, albumBox, 950, juce::Colours::transparentBlack, 0.0f, juce::Colours::transparentBlack, 1.8f);

    {
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText("L'ALBUM DELLE POLAROID (" + juce::String(comp.photoAlbum.size()) + ")",
                   static_cast<int>(albumBox.getX() + 16), static_cast<int>(albumBox.getY() + 8), 240, 20, juce::Justification::centredLeft);

        if (comp.selectedAlbumIndex >= 0 && comp.selectedAlbumIndex < static_cast<int>(comp.photoAlbum.size()))
        {
            juce::Rectangle<float> selBadge(albumBox.getX() + 270.0f, albumBox.getY() + 7.0f, 380.0f, 22.0f);
            MiMOLookAndFeel::drawSketchBox(g, selBadge, 952, MiMOLookAndFeel::crayonOchre, 0.85f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(11.0f, juce::Font::bold));
            g.drawText("[*] Clicca su uno Slot [S1..S5] sotto per assegnare '" + comp.photoAlbum[static_cast<size_t>(comp.selectedAlbumIndex)].title + "'",
                       selBadge.toNearestInt(), juce::Justification::centred);
        }

        float fx = albumBox.getRight() - 320.0f;
        auto drawFilterBtn = [&](const juce::String& name, float bw, int fid)
        {
            const bool active = (comp.filterEngine == name);
            juce::Rectangle<float> fb(fx, albumBox.getY() + 7.0f, bw, 22.0f);
            MiMOLookAndFeel::drawSketchBox(g, fb, fid, active ? MiMOLookAndFeel::crayonCyan : MiMOLookAndFeel::paperCard,
                                           active ? 0.70f : 0.40f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(10.5f, active ? juce::Font::bold : juce::Font::plain));
            g.drawText(name == "ALL" ? "TUTTI" : name, fb.toNearestInt(), juce::Justification::centred);
            fx += bw + 6.0f;
        };

        drawFilterBtn("ALL", 56.0f, 960);
        drawFilterBtn("sfizz", 116.0f, 961);
        drawFilterBtn("faust", 134.0f, 962);
    }

    // Griglia delle Polaroid nell'Album
    {
        const float cardW = 182.0f;
        const float cardH = 208.0f;
        const float gapX = 14.0f;
        const float gapY = 14.0f;
        float cx = albumBox.getX() + 16.0f;
        float cy = albumBox.getY() + 38.0f;

        for (size_t i = 0; i < comp.photoAlbum.size(); ++i)
        {
            const auto& pol = comp.photoAlbum[i];
            if (comp.filterEngine == "sfizz" && pol.engineType != "sfizz" && pol.engineType != "hybrid") continue;
            if (comp.filterEngine == "faust" && pol.engineType != "faust" && pol.engineType != "hybrid") continue;

            juce::Rectangle<float> cRect(cx, cy, cardW, cardH);
            const bool isSelected = (comp.selectedAlbumIndex == static_cast<int>(i));

            MiMOLookAndFeel::drawSketchBox(g, cRect, 970 + static_cast<int>(i),
                                           isSelected ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                                           isSelected ? 0.35f : 0.0f,
                                           isSelected ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                                           isSelected ? 2.6f : 1.6f, false, MiMOLookAndFeel::polaroidWhite);

            juce::Rectangle<float> pPhoto(cRect.getX() + 10.0f, cRect.getY() + 10.0f, cardW - 20.0f, 126.0f);
            g.setColour(MiMOLookAndFeel::paperBg);
            g.fillRect(pPhoto);
            if (pol.previewImage.isValid())
            {
                g.drawImageWithin(pol.previewImage,
                                  static_cast<int>(pPhoto.getX()), static_cast<int>(pPhoto.getY()),
                                  static_cast<int>(pPhoto.getWidth()), static_cast<int>(pPhoto.getHeight()),
                                  juce::RectanglePlacement::stretchToFit);
            }
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawRoundedRectangle(pPhoto, 3.0f, 1.4f);

            juce::Rectangle<float> engBadge(pPhoto.getX() + 4.0f, pPhoto.getY() + 4.0f, 52.0f, 16.0f);
            const juce::Colour engCol = (pol.engineType == "faust" ? MiMOLookAndFeel::crayonCherry
                                        : pol.engineType == "hybrid" ? MiMOLookAndFeel::crayonLilac : MiMOLookAndFeel::crayonSage);
            MiMOLookAndFeel::drawSketchBox(g, engBadge, 980 + static_cast<int>(i), engCol, 0.85f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(9.0f, juce::Font::bold));
            g.drawText(pol.engineType, engBadge.toNearestInt(), juce::Justification::centred);

            g.setColour(MiMOLookAndFeel::fountainInk);
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(pol.title, static_cast<int>(cRect.getX() + 10), static_cast<int>(pPhoto.getBottom() + 6), static_cast<int>(cardW - 20), 18, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.0f, juce::Font::plain));
            g.drawText(pol.engineType.toUpperCase() + "  -  " + pol.timestamp, static_cast<int>(cRect.getX() + 10), static_cast<int>(pPhoto.getBottom() + 24), static_cast<int>(cardW - 20), 14, juce::Justification::centredLeft);
            g.drawText(juce::String(pol.nodeCount) + " nodi, " + juce::String(pol.cableCount) + " cavi",
                       static_cast<int>(cRect.getX() + 10), static_cast<int>(pPhoto.getBottom() + 38), static_cast<int>(cardW - 20), 14, juce::Justification::centredLeft);

            cx += cardW + gapX;
            if (cx + cardW > albumBox.getRight() - 16.0f)
            {
                cx = albumBox.getX() + 16.0f;
                cy += cardH + gapY;
            }
        }
    }

    // 3. I 5 Grandi Slot Scena in Basso
    {
        for (int i = 0; i < 5; ++i)
        {
            auto slotBox = BottomDock::getSlotBounds(i, w, h, comp.activeTab);
            const bool isSnapped = (comp.snappedSlotIndex == i);
            if (isSnapped && comp.snapAnimScale > 1.01f)
            {
                const float dw = slotBox.getWidth() * (comp.snapAnimScale - 1.0f);
                const float dh = slotBox.getHeight() * (comp.snapAnimScale - 1.0f);
                slotBox.expand(dw * 0.5f, dh * 0.5f);
            }

            const bool isActive = (comp.activeSceneSlot == i);
            const bool assigned = comp.sceneSlots[i].isValid;
            const bool isHover = (comp.hoveredSlotIndex == i);

            MiMOLookAndFeel::drawSketchBox(g, slotBox, 1100 + i,
                                           assigned ? slotColours[i] : MiMOLookAndFeel::paperCard,
                                           assigned ? (isActive ? 0.78f : 0.45f) : 0.25f,
                                           (isHover || isActive) ? MiMOLookAndFeel::crayonOchre : juce::Colours::transparentBlack,
                                           isActive ? 3.0f : 1.8f);

            // Numero Slot Tasto Scocca Fisico [1..5]
            juce::Rectangle<float> numBadge(slotBox.getX() + 10.0f, slotBox.getY() + 10.0f, 32.0f, 32.0f);
            g.setColour(slotColours[i]);
            g.fillEllipse(numBadge);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawEllipse(numBadge, 1.8f);
            g.setFont(juce::Font(16.0f, juce::Font::bold));
            g.drawText(juce::String(i + 1), numBadge.toNearestInt(), juce::Justification::centred);

            // Miniatura preview Polaroid
            juce::Rectangle<float> thumb(slotBox.getX() + 48.0f, slotBox.getY() + 10.0f, slotBox.getWidth() - 58.0f, 76.0f);
            g.setColour(MiMOLookAndFeel::paperBg);
            g.fillRect(thumb);
            if (assigned && comp.sceneSlots[i].previewImage.isValid())
            {
                g.drawImageWithin(comp.sceneSlots[i].previewImage,
                                  static_cast<int>(thumb.getX()), static_cast<int>(thumb.getY()),
                                  static_cast<int>(thumb.getWidth()), static_cast<int>(thumb.getHeight()),
                                  juce::RectanglePlacement::stretchToFit);
            }
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawRoundedRectangle(thumb, 3.0f, 1.2f);

            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(assigned ? comp.sceneSlots[i].title : ("SLOT " + juce::String(i + 1) + " (VUOTO)"),
                       static_cast<int>(slotBox.getX() + 10), static_cast<int>(thumb.getBottom() + 6),
                       static_cast<int>(slotBox.getWidth() - 20), 18, juce::Justification::centredLeft);

            g.setColour(MiMOLookAndFeel::inkMuted);
            g.setFont(juce::Font(10.5f, juce::Font::plain));
            g.drawText(assigned ? (comp.sceneSlots[i].engineType.toUpperCase() + " - Tasto [" + juce::String(i + 1) + "]")
                                : "Trascina qui o seleziona Polaroid",
                       static_cast<int>(slotBox.getX() + 10), static_cast<int>(thumb.getBottom() + 24),
                       static_cast<int>(slotBox.getWidth() - 20), 16, juce::Justification::centredLeft);

            if (assigned)
            {
                g.drawText(juce::String(comp.sceneSlots[i].nodeCount) + " nodi, " + juce::String(comp.sceneSlots[i].cableCount) + " cavi",
                           static_cast<int>(slotBox.getX() + 10), static_cast<int>(thumb.getBottom() + 40),
                           static_cast<int>(slotBox.getWidth() - 20), 14, juce::Justification::centredLeft);
            }

            if (isActive)
            {
                juce::Rectangle<float> onAirBadge(slotBox.getX() + 10.0f, slotBox.getBottom() - 26.0f, slotBox.getWidth() - 20.0f, 18.0f);
                MiMOLookAndFeel::drawSketchBox(g, onAirBadge, 1200 + i, MiMOLookAndFeel::crayonSage, 0.85f);
                g.setColour(MiMOLookAndFeel::fountainInk);
                g.setFont(juce::Font(10.0f, juce::Font::bold));
                g.drawText("● ATTIVA IN USCITA MASTER", onAirBadge.toNearestInt(), juce::Justification::centred);
            }
        }

        // 4. 1-Button Looper Minimale
        const auto looperBox = BottomDock::getLooperBounds(w, h, comp.activeTab);
        MiMOLookAndFeel::drawSketchBox(g, looperBox, 1300, MiMOLookAndFeel::paperCard, 0.55f,
                                       juce::Colours::transparentBlack, 1.8f);

        const float lcx = looperBox.getCentreX();
        const float lcy = looperBox.getY() + 68.0f;
        const float lrad = 40.0f;

        juce::Colour loopFill = MiMOLookAndFeel::paperCard;
        juce::String loopTitle = "PRONTO";
        if (comp.looperState == LooperState::Recording)
        {
            loopFill = MiMOLookAndFeel::crayonCherry;
            loopTitle = "[REC]";
        }
        else if (comp.looperState == LooperState::Playing)
        {
            loopFill = MiMOLookAndFeel::crayonSage;
            loopTitle = "[PLAY]";
        }
        else if (comp.looperState == LooperState::Overdubbing)
        {
            loopFill = MiMOLookAndFeel::crayonOchre;
            loopTitle = "[+DUB]";
        }

        g.setColour(loopFill.withAlpha(comp.looperState == LooperState::Idle ? 0.15f : 0.88f));
        g.fillEllipse(lcx - lrad, lcy - lrad, lrad * 2.0f, lrad * 2.0f);

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.drawEllipse(lcx - lrad, lcy - lrad, lrad * 2.0f, lrad * 2.0f, comp.looperState != LooperState::Idle ? 2.4f : 1.8f);

        if (comp.looperState != LooperState::Idle && comp.looperSweepProgress > 0.01f)
        {
            juce::Path sweep;
            sweep.addCentredArc(lcx, lcy, lrad, lrad, 0.0f, 0.0f, comp.looperSweepProgress * juce::MathConstants<float>::twoPi, true);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.strokePath(sweep, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (comp.looperState == LooperState::Recording)
        {
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.fillEllipse(lcx - 11.0f, lcy - 11.0f, 22.0f, 22.0f);
        }
        else if (comp.looperState == LooperState::Playing)
        {
            juce::Path tri;
            tri.addTriangle(lcx - 9.0f, lcy - 13.0f,
                            lcx + 13.0f, lcy,
                            lcx - 9.0f, lcy + 13.0f);
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.fillPath(tri);
        }
        else if (comp.looperState == LooperState::Overdubbing)
        {
            g.setColour(MiMOLookAndFeel::fountainInk);
            g.drawLine(lcx - 13.0f, lcy, lcx + 13.0f, lcy, 3.8f);
            g.drawLine(lcx, lcy - 13.0f, lcx, lcy + 13.0f, 3.8f);
        }
        else
        {
            g.setColour(MiMOLookAndFeel::fountainInk.withAlpha(0.6f));
            g.drawEllipse(lcx - 11.0f, lcy - 11.0f, 22.0f, 22.0f, 2.0f);
        }

        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(13.5f, juce::Font::bold));
        g.drawText(loopTitle,
                   static_cast<int>(looperBox.getX()), static_cast<int>(lcy + lrad + 10.0f),
                   static_cast<int>(looperBox.getWidth()), 18, juce::Justification::centred);

        g.setColour(MiMOLookAndFeel::inkMuted);
        g.setFont(juce::Font(10.0f, juce::Font::plain));
        g.drawText("[Spazio] o Clic",
                   static_cast<int>(looperBox.getX()), static_cast<int>(lcy + lrad + 28.0f),
                   static_cast<int>(looperBox.getWidth()), 16, juce::Justification::centred);
        g.drawText("Dx: Stop",
                   static_cast<int>(looperBox.getX()), static_cast<int>(lcy + lrad + 44.0f),
                   static_cast<int>(looperBox.getWidth()), 16, juce::Justification::centred);
    }
}

void LiveAlbumView::paintCommunityView(juce::Graphics& g, MainComponent& comp)
{
    juce::Rectangle<float> box(48.0f, 86.0f, static_cast<float>(comp.getWidth() - 96), static_cast<float>(comp.getHeight() - 215));
    MiMOLookAndFeel::drawSketchBox(g, box, 990, MiMOLookAndFeel::crayonCyan, 0.18f, juce::Colours::transparentBlack, 2.2f);

    g.setColour(MiMOLookAndFeel::fountainInk);
    g.setFont(juce::Font(20.0f, juce::Font::bold));
    g.drawText("COMMUNITY MARKETPLACE & LIBRERIA PACCHETTI .MIMO",
               static_cast<int>(box.getX() + 24), static_cast<int>(box.getY() + 18), 640, 26, juce::Justification::centredLeft);

    g.setColour(MiMOLookAndFeel::inkMuted);
    g.setFont(juce::Font(13.5f, juce::Font::plain));
    g.drawText("Pacchetti .mimo validati via manifest.json (motori sfizz campionati e sintesi/effetti Faust DSP).",
               static_cast<int>(box.getX() + 24), static_cast<int>(box.getY() + 44), 760, 20, juce::Justification::centredLeft);

    float y = box.getY() + 80.0f;
    for (size_t i = 0; i < comp.instrumentCatalog.size(); ++i)
    {
        const auto& item = comp.instrumentCatalog[i];
        juce::Rectangle<float> row(box.getX() + 24.0f, y, box.getWidth() - 48.0f, 48.0f);
        MiMOLookAndFeel::drawSketchBox(g, row, 995 + static_cast<int>(i), item.colour, 0.45f);
        g.setColour(MiMOLookAndFeel::fountainInk);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(item.name + "  [" + item.engine + "]  -  " + item.hint,
                   row.reduced(16, 0).toNearestInt(), juce::Justification::centredLeft);
        y += 56.0f;
    }
}

bool LiveAlbumView::handlePolaroidMouseDown(MainComponent& comp, const juce::Point<float>& pt)
{
    if (!comp.hasPendingPolaroid)
        return false;

    const auto card = comp.getPolaroidCardBounds();
    if (!card.contains(pt))
    {
        comp.commitPolaroidToAlbum(comp.pendingPolaroid);
        return true;
    }

    const float btnW = (card.getWidth() - 36.0f - 4.0f * 6.0f) / 5.0f;
    for (int i = 0; i < 5; ++i)
    {
        juce::Rectangle<float> sb(card.getX() + 18.0f + static_cast<float>(i) * (btnW + 6.0f),
                                  card.getBottom() - 44.0f, btnW, 28.0f);
        if (sb.contains(pt))
        {
            comp.assignPolaroidToSlot(i, comp.pendingPolaroid);
            return true;
        }
    }

    comp.polaroidHasTyped = true;
    comp.draggedPolaroid = comp.pendingPolaroid;
    comp.dragMode = MainComponent::DragMode::DragPolaroid;
    return true;
}

bool LiveAlbumView::handleLiveMouseDown(MainComponent& comp, const juce::Point<float>& pt, const juce::MouseEvent& e)
{
    const int w = comp.getWidth();
    const int h = comp.getHeight();

    // A. Clic sul 1-Button Looper
    if (BottomDock::getLooperBounds(w, h, comp.activeTab).contains(pt))
    {
        if (e.mods.isRightButtonDown())
            comp.clearLooper();
        else
            comp.toggleLooper();
        return true;
    }

    // B. Clic su uno dei 5 Slot Scena
    for (int i = 0; i < 5; ++i)
    {
        if (BottomDock::getSlotBounds(i, w, h, comp.activeTab).contains(pt))
        {
            if (comp.selectedAlbumIndex >= 0 && comp.selectedAlbumIndex < static_cast<int>(comp.photoAlbum.size()))
            {
                comp.assignPolaroidToSlot(i, comp.photoAlbum[static_cast<size_t>(comp.selectedAlbumIndex)]);
                comp.selectedAlbumIndex = -1;
            }
            else
            {
                comp.triggerSceneSlot(i);
                comp.holdSlotIndex = i;
                comp.holdProgress = 0.0f;
            }
            return true;
        }
    }

    // C. Filtri Motore (TUTTI / SFIZZ / FAUST)
    juce::Rectangle<float> albumBox(24.0f, 62.0f, static_cast<float>(w - 48), static_cast<float>(h - 322));
    float fx = albumBox.getRight() - 320.0f;
    for (int fi = 0; fi < 3; ++fi)
    {
        const juce::String fName = (fi == 0 ? "ALL" : fi == 1 ? "sfizz" : "faust");
        juce::Rectangle<float> fb(fx, albumBox.getY() + 7.0f, (fi == 0 ? 56.0f : fi == 1 ? 116.0f : 134.0f), 22.0f);
        if (fb.contains(pt))
        {
            comp.filterEngine = fName;
            comp.selectedAlbumIndex = -1;
            comp.repaint();
            return true;
        }
        fx += fb.getWidth() + 6.0f;
    }

    // D. Selezione / Drag Polaroid nell'Album
    const float cardW = 182.0f;
    const float cardH = 208.0f;
    const float gapX = 14.0f;
    const float gapY = 14.0f;
    float cx = albumBox.getX() + 16.0f;
    float cy = albumBox.getY() + 38.0f;

    for (size_t i = 0; i < comp.photoAlbum.size(); ++i)
    {
        const auto& pol = comp.photoAlbum[i];
        if (comp.filterEngine == "sfizz" && pol.engineType != "sfizz" && pol.engineType != "hybrid") continue;
        if (comp.filterEngine == "faust" && pol.engineType != "faust" && pol.engineType != "hybrid") continue;

        juce::Rectangle<float> cRect(cx, cy, cardW, cardH);
        if (cRect.contains(pt))
        {
            comp.selectedAlbumIndex = (comp.selectedAlbumIndex == static_cast<int>(i) ? -1 : static_cast<int>(i));
            comp.draggedPolaroid = pol;
            comp.dragMode = MainComponent::DragMode::DragPolaroid;
            comp.repaint();
            return true;
        }

        cx += cardW + gapX;
        if (cx + cardW > albumBox.getRight() - 16.0f)
        {
            cx = albumBox.getX() + 16.0f;
            cy += cardH + gapY;
        }
    }

    return true;
}

void LiveAlbumView::triggerSnapPolaroid(MainComponent& comp)
{
    PolaroidItem pol;
    pol.id = "pol-" + juce::String(juce::Time::getMillisecondCounter());
    pol.timestamp = juce::Time::getCurrentTime().formatted("%H:%M");
    pol.nodeCount = static_cast<int>(comp.nodes.size());
    pol.cableCount = static_cast<int>(comp.cables.size());
    pol.modCount = 0;
    for (const auto& c : comp.cables)
        if (c.isModulation)
            ++pol.modCount;

    juce::StringArray parts;
    for (const auto& n : comp.nodes)
    {
        if (n.type == NodeType::HardwareModule && n.hasInstrument)
        {
            parts.add(n.instrumentName.upToFirstOccurrenceOf(" ", false, false));
            pol.badges.push_back({ n.instrumentName, n.crayonColour });
        }
        else if (n.type == NodeType::NestedEffect)
        {
            parts.add(n.label.upToFirstOccurrenceOf(" ", false, false));
            pol.badges.push_back({ n.label, n.crayonColour });
        }
    }

    pol.autoName = parts.isEmpty() ? "Scena Studio Naif" : parts.joinIntoString(" + ");
    pol.title = pol.autoName;
    pol.snapshot = { comp.nodes, comp.cables };
    pol.isValid = true;

    const juce::Rectangle<int> captureArea(20, 70, std::max(200, comp.getWidth() - 340), std::max(160, comp.getHeight() - 190));
    pol.previewImage = comp.createComponentSnapshot(captureArea, true, 0.5f);

    comp.pendingPolaroid = pol;
    comp.hasPendingPolaroid = true;
    comp.polaroidHasTyped = false;
    comp.polaroidCountdownTicks = 18;
    comp.grabKeyboardFocus();
    comp.repaint();
}

void LiveAlbumView::commitPolaroidToAlbum(MainComponent& comp, const PolaroidItem& item)
{
    PolaroidItem saved = item;
    if (saved.title.trim().isEmpty())
        saved.title = saved.autoName;

    bool exists = false;
    for (auto& existing : comp.photoAlbum)
    {
        if (existing.id == saved.id)
        {
            existing = saved;
            exists = true;
            break;
        }
    }
    if (!exists)
        comp.photoAlbum.insert(comp.photoAlbum.begin(), saved);

    comp.hasPendingPolaroid = false;
    comp.repaint();
}

void LiveAlbumView::assignPolaroidToSlot(MainComponent& comp, int slotIdx, const PolaroidItem& item)
{
    if (slotIdx < 0 || slotIdx >= 5)
        return;

    const auto oldItem = comp.sceneSlots[slotIdx];

    PolaroidItem saved = item;
    if (saved.title.trim().isEmpty())
        saved.title = saved.autoName;
    saved.isValid = true;

    comp.sceneSlots[slotIdx] = saved;
    comp.activeSceneSlot = slotIdx;
    comp.snappedSlotIndex = slotIdx;
    comp.snapAnimScale = 1.18f;

    if (oldItem.isValid && oldItem.id != saved.id)
    {
        bool inAlbum = false;
        for (const auto& p : comp.photoAlbum)
        {
            if (p.id == oldItem.id) { inAlbum = true; break; }
        }
        if (!inAlbum)
            comp.photoAlbum.insert(comp.photoAlbum.begin(), oldItem);
    }

    commitPolaroidToAlbum(comp, saved);
}

} // namespace mimo
