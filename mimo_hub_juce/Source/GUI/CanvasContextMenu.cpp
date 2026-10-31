#include "CanvasContextMenu.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <algorithm>

namespace mimo
{

namespace
{
    void addAction(juce::PopupMenu& menu, const juce::String& text, bool isEnabled, std::function<void()> action)
    {
        juce::PopupMenu::Item item;
        item.text = text;
        item.isEnabled = isEnabled;
        item.action = std::move(action);
        menu.addItem(item);
    }
}

juce::PopupMenu CanvasContextMenu::buildInstrumentsSubMenu(MainComponent& comp, juce::Point<float> worldPos)
{
    juce::PopupMenu m;

    for (const auto& item : comp.instrumentCatalog)
    {
        addAction(m, item.name + " (" + item.engine + ")", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), item, worldPos]()
        {
            if (auto* c = safeComp.getComponent())
            {
                c->pushUndoSnapshot();
                auto node = c->createInstrumentNodeFromCatalog(item, worldPos - juce::Point<float>{ 140.0f, 90.0f }, c->fxCounter++);
                c->nodes.push_back(node);

                // Auto-connessione all'uscita master se presente
                for (const auto& n : c->nodes)
                {
                    if (n.type == NodeType::MasterOut)
                    {
                        c->cables.push_back({
                            "c-" + juce::String(juce::Time::getMillisecondCounter()) + "-" + juce::String(rand() % 1000),
                            node.id, "audio:out",
                            n.id, "audio:in",
                            false, MiMOLookAndFeel::fountainInk, {}
                        });
                        break;
                    }
                }

                c->selectSingleNode(node.id);
                c->syncAudioRoutes();
                c->repaint();
            }
        });
    }

    return m;
}

juce::PopupMenu CanvasContextMenu::buildEffectsSubMenu(MainComponent& comp, juce::Point<float> worldPos)
{
    juce::PopupMenu m;

    for (const auto& item : comp.effectCatalog)
    {
        addAction(m, item.name + "  [" + item.category + "]", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), item, worldPos]()
        {
            if (auto* c = safeComp.getComponent())
            {
                c->pushUndoSnapshot();
                auto node = c->createFxNodeFromCatalog(item, worldPos - juce::Point<float>{ 130.0f, 85.0f }, c->fxCounter++, false);
                c->nodes.push_back(node);
                c->selectSingleNode(node.id);
                c->syncAudioRoutes();
                c->repaint();
            }
        });
    }

    return m;
}

juce::PopupMenu CanvasContextMenu::buildHardwareSubMenu(MainComponent& comp, juce::Point<float> worldPos)
{
    juce::PopupMenu m;

    addAction(m, "Modulo Batteria Piezo (P1)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), worldPos]()
    {
        if (auto* c = safeComp.getComponent())
        {
            c->pushUndoSnapshot();
            if (!c->isDrumInCanvas())
                c->toggleDrumInCanvas();

            for (auto& n : c->nodes)
            {
                if (n.id == "hw-drum")
                {
                    n.pos = worldPos - n.size * 0.5f;
                    n.targetPos = n.pos;
                    n.isAnimatingPos = false;
                    break;
                }
            }
            c->repaint();
        }
    });

    addAction(m, "Modulo Occhio 3D (Camera P2)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), worldPos]()
    {
        if (auto* c = safeComp.getComponent())
        {
            c->pushUndoSnapshot();
            if (!c->isCameraInCanvas())
                c->toggleCameraInCanvas();

            for (auto& n : c->nodes)
            {
                if (n.isIntegratedCamera || n.id == "hw-webcam" || n.id == "hw-eye")
                {
                    n.pos = worldPos - n.size * 0.5f;
                    n.targetPos = n.pos;
                    n.isAnimatingPos = false;
                    break;
                }
            }
            c->repaint();
        }
    });

    addAction(m, "Modulo Microfono Dinamico", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), worldPos]()
    {
        if (auto* c = safeComp.getComponent())
        {
            c->pushUndoSnapshot();
            if (!c->isMicInCanvas())
                c->toggleMicInCanvas();

            for (auto& n : c->nodes)
            {
                if (n.id == "hw-mic" || n.isIntegratedMic)
                {
                    n.pos = worldPos - n.size * 0.5f;
                    n.targetPos = n.pos;
                    n.isAnimatingPos = false;
                    break;
                }
            }
            c->repaint();
        }
    });

    return m;
}

juce::PopupMenu CanvasContextMenu::buildUtilitySubMenu(MainComponent& comp, juce::Point<float> worldPos)
{
    juce::PopupMenu m;

    for (const auto& item : comp.utilityCatalog)
    {
        addAction(m, item.name + " (" + item.category + ")", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), item, worldPos]()
        {
            if (auto* c = safeComp.getComponent())
            {
                c->pushUndoSnapshot();
                auto node = c->createInstrumentNodeFromCatalog(item, worldPos - juce::Point<float>{ 130.0f, 85.0f }, c->fxCounter++);
                c->nodes.push_back(node);
                c->selectSingleNode(node.id);
                c->syncAudioRoutes();
                c->repaint();
            }
        });
    }

    return m;
}

void CanvasContextMenu::showQuickAddMenu(MainComponent& comp, juce::Point<float> screenPos)
{
    const auto worldPos = comp.screenToWorld(screenPos);

    juce::PopupMenu m;
    m.addSubMenu("Strumenti Sonori", buildInstrumentsSubMenu(comp, worldPos));
    m.addSubMenu("Effetti & Filtri", buildEffectsSubMenu(comp, worldPos));
    m.addSubMenu("Moduli Hardware", buildHardwareSubMenu(comp, worldPos));
    m.addSubMenu("Utility & Controllo", buildUtilitySubMenu(comp, worldPos));

    juce::PopupMenu::Options opt;
    opt = opt.withTargetScreenArea(juce::Rectangle<int>(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 1, 1));
    m.showMenuAsync(opt);
}

void CanvasContextMenu::showContextMenu(MainComponent& comp, juce::Point<float> screenPos)
{
    const auto worldPos = comp.screenToWorld(screenPos);

    // 1. Cerca se il click destro e' avvenuto sopra un Nodo
    for (auto it = comp.nodes.rbegin(); it != comp.nodes.rend(); ++it)
    {
        const auto box = juce::Rectangle<float>(it->pos.x, it->pos.y, it->size.x, it->size.y);
        if (box.contains(worldPos))
        {
            const juce::String clickedNodeId = it->id;
            const auto nodeType = it->type;
            const bool isBypassed = it->isBypassed;
            const juce::String nodeLabel = it->label;
            const bool hasInnerBlocks = !it->innerBlocks.empty();

            juce::PopupMenu m;
            m.addItem(1, "Nodo: " + nodeLabel, false); // Header disabilitato
            m.addSeparator();

            if (nodeType == NodeType::RockPiCentral)
            {
                juce::PopupMenu volMenu;
                addAction(volMenu, "50% (-6 dB)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent()) { c->appMasterVolume.store(0.50f); c->repaint(); }
                });
                addAction(volMenu, "75% (-2.5 dB)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent()) { c->appMasterVolume.store(0.75f); c->repaint(); }
                });
                addAction(volMenu, "100% (0 dB Nominale)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent()) { c->appMasterVolume.store(1.00f); c->repaint(); }
                });
                addAction(volMenu, "125% (+1.9 dB Boost)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent()) { c->appMasterVolume.store(1.25f); c->repaint(); }
                });
                addAction(volMenu, "150% (+3.5 dB Boost)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent()) { c->appMasterVolume.store(1.50f); c->repaint(); }
                });

                m.addSubMenu("Livello Master", volMenu);

                addAction(m, "Centra MiMO Core nel Canvas", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent())
                    {
                        const float midW = static_cast<float>(c->getWidth()) * 0.5f;
                        const float midH = static_cast<float>(c->getHeight()) * 0.5f;
                        c->canvasOffset.x = (midW / c->canvasZoom);
                        c->canvasOffset.y = (midH / c->canvasZoom);
                        c->repaint();
                    }
                });

                addAction(m, "Pulisci Cavi Scollegati", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
                    if (auto* c = safeComp.getComponent())
                    {
                        c->pushUndoSnapshot();
                        auto isNodePresent = [&](const juce::String& nId) {
                            for (const auto& n : c->nodes) if (n.id == nId) return true;
                            return false;
                        };
                        c->cables.erase(std::remove_if(c->cables.begin(), c->cables.end(),
                            [&](const CanvasCable& cable) {
                                return !isNodePresent(cable.sourceNodeId) || !isNodePresent(cable.targetNodeId);
                            }), c->cables.end());
                        c->syncAudioRoutes();
                        c->repaint();
                    }
                });
            }
            else if (nodeType == NodeType::HardwareModule)
            {
                juce::PopupMenu assignMenu;
                for (const auto& item : comp.instrumentCatalog)
                {
                    addAction(assignMenu, item.name, true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), clickedNodeId, item]() {
                        if (auto* c = safeComp.getComponent())
                        {
                            c->assignInstrumentToHardware(clickedNodeId, item);
                        }
                    });
                }
                m.addSubMenu("Assegna Strumento Sonoro", assignMenu);

                addAction(m, "Rimuovi Modulo dal Canvas", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), clickedNodeId]() {
                    if (auto* c = safeComp.getComponent())
                    {
                        c->pushUndoSnapshot();
                        c->deleteNode(clickedNodeId);
                    }
                });
            }
            else
            {
                // Strumento, Effetto o Utility
                addAction(m, "Duplica Elemento (Ctrl+C / Ctrl+V)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), clickedNodeId]() {
                    if (auto* c = safeComp.getComponent())
                    {
                        c->selectSingleNode(clickedNodeId);
                        c->copySelectedNodes();
                        c->pasteNodes();
                    }
                });

                addAction(m, isBypassed ? "Attiva Audio (PWR)" : "Bypassa Audio (BYP)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), clickedNodeId]() {
                    if (auto* c = safeComp.getComponent())
                    {
                        c->pushUndoSnapshot();
                        for (auto& n : c->nodes)
                        {
                            if (n.id == clickedNodeId)
                            {
                                n.isBypassed = !n.isBypassed;
                                break;
                            }
                        }
                        c->syncAudioRoutes();
                        c->repaint();
                    }
                });

                if (nodeType == NodeType::NestedEffect && hasInnerBlocks)
                {
                    addAction(m, "Ispeziona Sub-Canvas (Doppio Click / TAB)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), clickedNodeId]() {
                        if (auto* c = safeComp.getComponent())
                        {
                            c->expandedNodeId = clickedNodeId;
                            c->repaint();
                        }
                    });
                }

                addAction(m, "Elimina Nodo (CANC)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp), clickedNodeId]() {
                    if (auto* c = safeComp.getComponent())
                    {
                        c->pushUndoSnapshot();
                        c->deleteNode(clickedNodeId);
                    }
                });
            }

            juce::PopupMenu::Options opt;
            opt = opt.withTargetScreenArea(juce::Rectangle<int>(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 1, 1));
            m.showMenuAsync(opt);
            return;
        }
    }

    // 2. Click destro su Canvas Libero
    juce::PopupMenu m;
    m.addSubMenu("Aggiungi Nodo... (Shift+A)", [&comp, worldPos]()
    {
        juce::PopupMenu addMenu;
        addMenu.addSubMenu("Strumenti Sonori", buildInstrumentsSubMenu(comp, worldPos));
        addMenu.addSubMenu("Effetti & Filtri", buildEffectsSubMenu(comp, worldPos));
        addMenu.addSubMenu("Moduli Hardware", buildHardwareSubMenu(comp, worldPos));
        addMenu.addSubMenu("Utility & Controllo", buildUtilitySubMenu(comp, worldPos));
        return addMenu;
    }());

    addAction(m, "Incolla (Ctrl+V)", comp.undoManager.hasClipboard(), [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
        if (auto* c = safeComp.getComponent())
            c->pasteNodes();
    });

    addAction(m, "Seleziona Tutti (Ctrl+A)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
        if (auto* c = safeComp.getComponent())
        {
            c->selectedNodeIds.clear();
            for (const auto& n : c->nodes)
                if (n.type != NodeType::RockPiCentral)
                    c->selectedNodeIds.push_back(n.id);
            if (!c->selectedNodeIds.empty())
                c->selectedNodeId = c->selectedNodeIds.back();
            c->repaint();
        }
    });

    m.addSeparator();

    addAction(m, "Resetta Vista (100% Zoom)", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
        if (auto* c = safeComp.getComponent())
        {
            c->canvasZoom = 1.0f;
            c->canvasOffset = { 0.0f, 0.0f };
            c->repaint();
        }
    });

    addAction(m, "Centra MiMO Core", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
        if (auto* c = safeComp.getComponent())
        {
            const float midW = static_cast<float>(c->getWidth()) * 0.5f;
            const float midH = static_cast<float>(c->getHeight()) * 0.5f;
            c->canvasOffset.x = (midW / c->canvasZoom);
            c->canvasOffset.y = (midH / c->canvasZoom);
            c->repaint();
        }
    });

    addAction(m, "Pulisci Cavi Scollegati", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
        if (auto* c = safeComp.getComponent())
        {
            c->pushUndoSnapshot();
            auto isNodePresent = [&](const juce::String& nId) {
                for (const auto& n : c->nodes) if (n.id == nId) return true;
                return false;
            };
            c->cables.erase(std::remove_if(c->cables.begin(), c->cables.end(),
                [&](const CanvasCable& cable) {
                    return !isNodePresent(cable.sourceNodeId) || !isNodePresent(cable.targetNodeId);
                }), c->cables.end());
            c->syncAudioRoutes();
            c->repaint();
        }
    });

    m.addSeparator();

    addAction(m, "Scorciatoie da Tastiera...", true, [safeComp = juce::Component::SafePointer<MainComponent>(&comp)]() {
        if (auto* c = safeComp.getComponent())
        {
            c->shortcutsModalOpen = true;
            c->repaint();
        }
    });

    juce::PopupMenu::Options opt;
    opt = opt.withTargetScreenArea(juce::Rectangle<int>(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 1, 1));
    m.showMenuAsync(opt);
}

} // namespace mimo
