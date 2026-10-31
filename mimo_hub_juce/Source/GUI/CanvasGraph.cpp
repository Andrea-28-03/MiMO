#include "CanvasGraph.h"

namespace mimo
{

// =============================================================================
// KnobParam
// =============================================================================
float KnobParam::getNormalized() const
{
    if (maxVal <= minVal)
        return 0.0f;
    if (unit.trim() == "Hz" && minVal > 0.0f)
        return std::log(value / minVal) / std::log(maxVal / minVal);
    return (value - minVal) / (maxVal - minVal);
}

void KnobParam::setFromNormalized(float norm)
{
    const float n = juce::jlimit(0.0f, 1.0f, norm);
    if (unit.trim() == "Hz" && minVal > 0.0f)
        value = minVal * std::pow(maxVal / minVal, n);
    else
        value = minVal + n * (maxVal - minVal);
}

juce::String KnobParam::getFormattedValue() const
{
    if (key == "wave" || key == "shape")
    {
        const int s = juce::jlimit(0, 3, juce::roundToInt(value));
        static const char* shapes[] = { "Sin", "Tri", "Saw", "S&H" };
        return shapes[s];
    }
    const float span = maxVal - minVal;
    if (span >= 100.0f)
        return juce::String(juce::roundToInt(value)) + unit;
    if (span >= 10.0f)
        return juce::String(value, 1) + unit;
    return juce::String(value, 2) + unit;
}

// =============================================================================
// CanvasGeometry
// =============================================================================
juce::Point<float> CanvasGeometry::screenToWorld(juce::Point<float> screenPt, juce::Point<float> offset, float zoom) noexcept
{
    return (screenPt - offset) / zoom;
}

juce::Point<float> CanvasGeometry::worldToScreen(juce::Point<float> worldPt, juce::Point<float> offset, float zoom) noexcept
{
    return worldPt * zoom + offset;
}

juce::Rectangle<float> CanvasGeometry::worldToScreenRect(juce::Rectangle<float> worldRect, juce::Point<float> offset, float zoom) noexcept
{
    const auto tl = worldToScreen(worldRect.getTopLeft(), offset, zoom);
    return { tl.x, tl.y, worldRect.getWidth() * zoom, worldRect.getHeight() * zoom };
}

float CanvasGeometry::getNodeCalculatedHeight(const CanvasNode& node)
{
    if (node.type != NodeType::HardwareModule || node.outputGroups.empty())
        return node.size.y;

    const bool isEye = (node.isIntegratedCamera || node.id == "hw-eye" || node.id == "hw-webcam");
    const bool isMic = (node.isIntegratedMic || node.id == "hw-mic" || node.id == "hw-drum");

    float h = isEye ? 168.0f : (isMic ? 104.0f : 96.0f);

    for (size_t gIdx = 0; gIdx < node.outputGroups.size(); ++gIdx)
    {
        const auto& grp = node.outputGroups[gIdx];
        if (!grp.parentGroup.isEmpty())
            continue; // Subgroup conteggiato sotto al proprio genitore

        h += 26.0f; // Top-Level Header
        if (grp.items.empty()) // Ha sottogruppi
        {
            if (grp.isExpanded)
            {
                for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                {
                    const auto& sub = node.outputGroups[sIdx];
                    if (sub.parentGroup == grp.id)
                    {
                        h += 24.0f; // Subheader
                        if (sub.isExpanded)
                            h += static_cast<float>(sub.items.size()) * 26.0f;
                    }
                }
            }
        }
        else // Gruppo lineare diretto
        {
            if (grp.isExpanded)
                h += static_cast<float>(grp.items.size()) * 26.0f;
        }
        h += 4.0f;
    }

    if (isEye || isMic)
        h += 34.0f; // Barra dropdown selettore in basso

    h += 8.0f;
    return std::max(h, isEye ? 240.0f : 180.0f);
}

std::vector<KnobHit> CanvasGeometry::collectCanvasKnobs(const std::vector<CanvasNode>& nodes)
{
    std::vector<KnobHit> list;

    for (const auto& node : nodes)
    {
        if (node.type == NodeType::NestedEffect)
        {
            std::vector<std::pair<juce::String, const KnobParam*>> exposed;
            for (const auto& b : node.innerBlocks)
                for (const auto& p : b.params)
                    if (p.isExposed)
                        exposed.push_back({ b.id, &p });

            const int count = std::max(1, static_cast<int>(exposed.size()));
            const float slotW = (node.size.x - 20.0f) / static_cast<float>(count);
            for (int i = 0; i < static_cast<int>(exposed.size()); ++i)
            {
                const float kx = node.pos.x + 10.0f + static_cast<float>(i) * slotW;
                const float ky = node.pos.y + 62.0f;
                list.push_back({ node.id, exposed[static_cast<size_t>(i)].first,
                                 exposed[static_cast<size_t>(i)].second->key,
                                 { kx, ky, slotW, 88.0f } });
            }
        }
        else if (node.type == NodeType::InstrumentNode && !node.directParams.empty())
        {
            const int count = static_cast<int>(node.directParams.size());
            const bool isPerc = (node.id.containsIgnoreCase("drum") || node.id.containsIgnoreCase("percuss"));
            const float marginL = isPerc ? 56.0f : 38.0f;
            const float marginR = 18.0f;
            const float availW = node.size.x - marginL - marginR;
            const float slotW = availW / static_cast<float>(count);
            const float ky = isPerc ? (node.isDrumAdvanced ? (node.pos.y + 94.0f) : (node.pos.y + 80.0f)) : (node.pos.y + 68.0f);
            for (int i = 0; i < count; ++i)
            {
                const float kx = node.pos.x + marginL + static_cast<float>(i) * slotW;
                list.push_back({ node.id, "direct",
                                 node.directParams[static_cast<size_t>(i)].key,
                                 { kx, ky, slotW, 84.0f } });
            }
        }
        else if (node.type == NodeType::MasterOut && !node.directParams.empty())
        {
            list.push_back({ node.id, "direct", node.directParams[0].key,
                             { node.pos.x + 25.0f, node.pos.y + 52.0f, node.size.x - 50.0f, 88.0f } });
        }
    }
    return list;
}

std::vector<PinHit> CanvasGeometry::collectAllPins(const std::vector<CanvasNode>& nodes)
{
    std::vector<PinHit> pins;

    for (const auto& node : nodes)
    {
        if (node.type == NodeType::RockPiCentral)
        {
            // 5 Prese di connessione sotto ciascuno dei 5 tasti fisici
            static constexpr float keyXFractions[5] = { 0.1490f, 0.2958f, 0.4426f, 0.5895f, 0.7349f };
            for (int i = 0; i < 5; ++i)
            {
                const float cx = node.pos.x + node.size.x * keyXFractions[i];
                pins.push_back({ node.id, "audio:p" + juce::String(i + 1), false, false,
                                 MiMOLookAndFeel::fountainInk, { cx, node.pos.y + node.size.y + 4.0f },
                                 { 0.0f, 1.0f }, MiMOLookAndFeel::PinShapeType::AudioCircle });
            }
            pins.push_back({ node.id, "audio:out", true, false,
                             MiMOLookAndFeel::fountainInk, { node.pos.x + node.size.x + 4.0f, node.pos.y + node.size.y * 0.52f },
                             { 1.0f, 0.0f }, MiMOLookAndFeel::PinShapeType::AudioCircle });
        }
        else if (node.type == NodeType::HardwareModule)
        {
            // Presa fisica di connessione sul bordo superiore del modulo (audio out verso MiMO)
            pins.push_back({ node.id, "audio:out", true, false,
                             MiMOLookAndFeel::fountainInk, { node.pos.x + 29.0f, node.pos.y },
                             { 0.0f, -1.0f }, MiMOLookAndFeel::PinShapeType::AudioCircle });

            if (!node.outputGroups.empty())
            {
                const bool isEye = (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye");
                const bool isMic = (node.isIntegratedMic || node.id == "hw-mic" || node.id == "hw-drum");
                float curY = isEye ? (node.pos.y + 168.0f) : (isMic ? (node.pos.y + 104.0f) : (node.pos.y + 96.0f));

                for (size_t gIdx = 0; gIdx < node.outputGroups.size(); ++gIdx)
                {
                    const auto& grp = node.outputGroups[gIdx];
                    if (!grp.parentGroup.isEmpty())
                        continue; // Processato nel contenitore genitore

                    const float topHeaderCentreY = curY + 13.0f;
                    curY += 26.0f;

                    if (grp.items.empty()) // Contenitore con sottogruppi (Mano DX / Mano SX)
                    {
                        if (!grp.isExpanded)
                        {
                            // Gruppo principale chiuso: tutti i pin dei sottogruppi si attestano sull'header principale
                            for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                            {
                                const auto& sub = node.outputGroups[sIdx];
                                if (sub.parentGroup == grp.id)
                                {
                                    for (const auto& item : sub.items)
                                    {
                                        const auto shape = item.isTrigger ? MiMOLookAndFeel::PinShapeType::TriggerDiamond
                                                                          : MiMOLookAndFeel::PinShapeType::ModulationTri;
                                        pins.push_back({ node.id, item.pinId, true, !item.isTrigger,
                                                         item.colour, { node.pos.x + node.size.x, topHeaderCentreY },
                                                         { 1.0f, 0.0f }, shape });
                                    }
                                }
                            }
                        }
                        else
                        {
                            // Gruppo principale aperto: scorri i singoli sottogruppi
                            for (size_t sIdx = 0; sIdx < node.outputGroups.size(); ++sIdx)
                            {
                                const auto& sub = node.outputGroups[sIdx];
                                if (sub.parentGroup != grp.id)
                                    continue;

                                const float subHeaderCentreY = curY + 12.0f;
                                curY += 24.0f;

                                if (sub.isExpanded)
                                {
                                    for (const auto& item : sub.items)
                                    {
                                        const float rowCentreY = curY + 13.0f;
                                        const auto shape = item.isTrigger ? MiMOLookAndFeel::PinShapeType::TriggerDiamond
                                                                          : MiMOLookAndFeel::PinShapeType::ModulationTri;
                                        pins.push_back({ node.id, item.pinId, true, !item.isTrigger,
                                                         item.colour, { node.pos.x + node.size.x, rowCentreY },
                                                         { 1.0f, 0.0f }, shape });
                                        curY += 26.0f;
                                    }
                                }
                                else
                                {
                                    // Sottogruppo chiuso: i suoi pin si attestano sul sub-header
                                    for (const auto& item : sub.items)
                                    {
                                        const auto shape = item.isTrigger ? MiMOLookAndFeel::PinShapeType::TriggerDiamond
                                                                          : MiMOLookAndFeel::PinShapeType::ModulationTri;
                                        pins.push_back({ node.id, item.pinId, true, !item.isTrigger,
                                                         item.colour, { node.pos.x + node.size.x, subHeaderCentreY },
                                                         { 1.0f, 0.0f }, shape });
                                    }
                                }
                            }
                        }
                    }
                    else // Gruppo lineare diretto (es. Modulo Batteria)
                    {
                        if (grp.isExpanded)
                        {
                            for (const auto& item : grp.items)
                            {
                                const float rowCentreY = curY + 13.0f;
                                const auto shape = item.isTrigger ? MiMOLookAndFeel::PinShapeType::TriggerDiamond
                                                                  : MiMOLookAndFeel::PinShapeType::ModulationTri;
                                pins.push_back({ node.id, item.pinId, true, !item.isTrigger,
                                                 item.colour, { node.pos.x + node.size.x, rowCentreY },
                                                 { 1.0f, 0.0f }, shape });
                                curY += 26.0f;
                            }
                        }
                        else
                        {
                            for (const auto& item : grp.items)
                            {
                                const auto shape = item.isTrigger ? MiMOLookAndFeel::PinShapeType::TriggerDiamond
                                                                  : MiMOLookAndFeel::PinShapeType::ModulationTri;
                                pins.push_back({ node.id, item.pinId, true, !item.isTrigger,
                                                 item.colour, { node.pos.x + node.size.x, topHeaderCentreY },
                                                 { 1.0f, 0.0f }, shape });
                            }
                        }
                    }
                    curY += 4.0f;
                }
            }

            if (node.outputGroups.empty())
            {
                if (node.id == "hw-drum" || node.id == "hw-mic" || node.isIntegratedMic)
                {
                    // Trigger Out (Impulso Colpo) -> Rombo Arancio Fiamma
                    const float trigY = node.isIntegratedMic ? (node.pos.y + 115.0f) : (node.pos.y + 109.0f);
                    pins.push_back({ node.id, "trigger:out", true, false,
                                     juce::Colour(0xFFFF7A18), { node.pos.x + node.size.x, trigY },
                                     { 1.0f, 0.0f }, MiMOLookAndFeel::PinShapeType::TriggerDiamond });

                    // Modulazioni CV (Velocity, Pressione) -> Triangoli orientati verso destra
                    float my = node.isIntegratedMic ? (node.pos.y + 147.0f) : (node.pos.y + 141.0f);
                    for (const auto& m : node.modOutputs)
                    {
                        pins.push_back({ node.id, "mod:" + m.key, true, true, m.colour,
                                         { node.pos.x + node.size.x, my },
                                         { 1.0f, 0.0f }, MiMOLookAndFeel::PinShapeType::ModulationTri });
                        my += 32.0f;
                    }
                }
                else if (node.isIntegratedCamera || node.id == "hw-webcam" || node.id == "hw-eye")
                {
                    // Modulo Occhio: Uscite assi MediaPipe
                    float my = node.pos.y + 138.0f + 12.5f;
                    for (const auto& m : node.modOutputs)
                    {
                        pins.push_back({ node.id, "mod:" + m.key, true, true, m.colour,
                                         { node.pos.x + node.size.x, my },
                                         { 1.0f, 0.0f }, MiMOLookAndFeel::PinShapeType::ModulationTri });
                        my += 30.0f;
                    }
                }
                else
                {
                    float my = node.pos.y + 118.0f;
                    for (const auto& m : node.modOutputs)
                    {
                        pins.push_back({ node.id, "mod:" + m.key, true, true, m.colour,
                                         { node.pos.x + node.size.x, my },
                                         { 1.0f, 0.0f }, MiMOLookAndFeel::PinShapeType::ModulationTri });
                        my += 28.0f;
                    }
                }
            }
        }
        else if (node.type == NodeType::NestedEffect)
        {
            if (node.isVerticalFlow)
            {
                pins.push_back({ node.id, "audio:in",  false, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x + 29.0f, node.pos.y + node.size.y }, { 0.0f, 1.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
                pins.push_back({ node.id, "audio:out", true,  false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x + 29.0f, node.pos.y }, { 0.0f, -1.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
            }
            else
            {
                pins.push_back({ node.id, "audio:in",  false, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x, node.pos.y + 32.0f }, { -1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
                pins.push_back({ node.id, "audio:out", true,  false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x + node.size.x, node.pos.y + 32.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
            }
        }
        else if (node.type == NodeType::InstrumentNode)
        {
            const bool isPerc = (node.id.containsIgnoreCase("drum") || node.id.containsIgnoreCase("percuss") || node.id.containsIgnoreCase("kit"));
            const bool isScale = (node.id.containsIgnoreCase("scale") || node.id.containsIgnoreCase("quantiz"));
            const bool isFollower = (node.id.containsIgnoreCase("follower") || node.id.containsIgnoreCase("env"));
            const bool isLooper = (node.id.containsIgnoreCase("looper") || node.id.containsIgnoreCase("tape_loop"));
            const bool isLfo = (node.id.containsIgnoreCase("lfo") || node.id.containsIgnoreCase("mod_lfo"));

            if (isScale)
            {
                // Modulazione In (CV da mano / LFO) -> Triangolo
                pins.push_back({ node.id, "mod:in", false, true, MiMOLookAndFeel::crayonMint,
                                 { node.pos.x, node.pos.y + 36.0f }, { -1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::ModulationTri });
                // Modulazione Out (CV quantizzato per intonazione) -> Triangolo
                pins.push_back({ node.id, "mod:out", true, true, MiMOLookAndFeel::crayonMint,
                                 { node.pos.x + node.size.x, node.pos.y + 36.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::ModulationTri });
            }
            else if (isFollower)
            {
                // Audio In -> Cerchio
                pins.push_back({ node.id, "audio:in", false, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x, node.pos.y + 28.0f }, { -1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
                // Modulazione Out (Inviluppo CV dinamico) -> Triangolo
                pins.push_back({ node.id, "mod:out", true, true, MiMOLookAndFeel::crayonOchre,
                                 { node.pos.x + node.size.x, node.pos.y + 36.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::ModulationTri });
            }
            else if (isLooper)
            {
                // Audio In -> Cerchio
                pins.push_back({ node.id, "audio:in", false, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x, node.pos.y + 28.0f }, { -1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
                // Trigger In (Rec / Play / Overdub) -> Rombo
                pins.push_back({ node.id, "trigger:in", false, false, juce::Colour(0xFFFF7A18),
                                 { node.pos.x, node.pos.y + 58.0f }, { -1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                // Audio Out -> Cerchio
                pins.push_back({ node.id, "audio:out", true, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x + node.size.x, node.pos.y + 28.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
            }
            else if (isLfo)
            {
                // Modulazione Out (LFO CV oscillante continuo) -> Triangolo a destra
                pins.push_back({ node.id, "mod:out", true, true, MiMOLookAndFeel::crayonPeach,
                                 { node.pos.x + node.size.x, node.pos.y + 36.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::ModulationTri });

                // Clock Trigger Out (impulso ad ogni ciclo completato) -> Rombo Arancio Fiamma
                pins.push_back({ node.id, "trigger:out", true, false, juce::Colour(0xFFFF7A18),
                                 { node.pos.x + node.size.x, node.pos.y + 68.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::TriggerDiamond });
            }
            else
            {
                // Presa Audio In (Cerchio)
                pins.push_back({ node.id, "audio:in", false, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x, node.pos.y + 28.0f }, { -1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });

                if (isPerc)
                {
                    pins.push_back({ node.id, "trigger:kick", false, false, MiMOLookAndFeel::crayonCherry,
                                     { node.pos.x, node.pos.y + 50.0f }, { -1.0f, 0.0f },
                                     MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                    pins.push_back({ node.id, "trigger:snare", false, false, MiMOLookAndFeel::crayonPeach,
                                     { node.pos.x, node.pos.y + 72.0f }, { -1.0f, 0.0f },
                                     MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                    pins.push_back({ node.id, "trigger:hat", false, false, MiMOLookAndFeel::crayonCyan,
                                     { node.pos.x, node.pos.y + 94.0f }, { -1.0f, 0.0f },
                                     MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                    pins.push_back({ node.id, "trigger:tom", false, false, MiMOLookAndFeel::crayonMint,
                                     { node.pos.x, node.pos.y + 116.0f }, { -1.0f, 0.0f },
                                     MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                    pins.push_back({ node.id, "trigger:in", false, false, juce::Colour(0xFFFF7A18),
                                     { node.pos.x, node.pos.y + 138.0f }, { -1.0f, 0.0f },
                                     MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                }
                else
                {
                    pins.push_back({ node.id, "trigger:in", false, false, juce::Colour(0xFFFF7A18),
                                     { node.pos.x, node.pos.y + 58.0f }, { -1.0f, 0.0f },
                                     MiMOLookAndFeel::PinShapeType::TriggerDiamond });
                }

                // Presa Audio Out (Cerchio)
                pins.push_back({ node.id, "audio:out", true, false, MiMOLookAndFeel::fountainInk,
                                 { node.pos.x + node.size.x, node.pos.y + 28.0f }, { 1.0f, 0.0f },
                                 MiMOLookAndFeel::PinShapeType::AudioCircle });
            }
        }
        else if (node.type == NodeType::MasterOut)
        {
            pins.push_back({ node.id, "audio:in", false, false, MiMOLookAndFeel::fountainInk,
                             { node.pos.x, node.pos.y + 32.0f }, { -1.0f, 0.0f },
                             MiMOLookAndFeel::PinShapeType::AudioCircle });
        }
    }

    // Pin di ingresso modulazione sotto a ciascuna manopola esposta
    for (const auto& kh : collectCanvasKnobs(nodes))
    {
        for (const auto& n : nodes)
        {
            if (n.id == kh.nodeId)
            {
                const juce::String pinId = "param:" + kh.blockId + "." + kh.paramKey;
                const juce::Colour pinCol = juce::Colour(0xFFA09B91);

                pins.push_back({ kh.nodeId, pinId, false, true, pinCol,
                                 { kh.worldBounds.getCentreX(), n.pos.y + n.size.y },
                                 { 0.0f, 1.0f }, MiMOLookAndFeel::PinShapeType::ModulationTri });
                break;
            }
        }
    }

    return pins;
}

juce::Point<float> CanvasGeometry::getPinWorldPos(const std::vector<CanvasNode>& nodes, const juce::String& nodeId, const juce::String& pinId)
{
    for (const auto& p : collectAllPins(nodes))
        if (p.nodeId == nodeId && p.pinId == pinId)
            return p.worldPos;
    return {};
}

juce::Point<float> CanvasGeometry::getPinNormal(const std::vector<CanvasNode>& nodes, const juce::String& nodeId, const juce::String& pinId)
{
    for (const auto& p : collectAllPins(nodes))
        if (p.nodeId == nodeId && p.pinId == pinId)
            return p.normal;
    return { 0.0f, 0.0f };
}

juce::String CanvasGeometry::findHwModuleAt(const std::vector<CanvasNode>& nodes, juce::Point<float> worldPt)
{
    for (const auto& n : nodes)
        if (n.type == NodeType::HardwareModule)
            if (juce::Rectangle<float>(n.pos.x, n.pos.y, n.size.x, n.size.y).contains(worldPt))
                return n.id;
    return {};
}

juce::String CanvasGeometry::findAudioCableNear(const std::vector<CanvasCable>& cables, const std::vector<CanvasNode>& nodes,
                                                juce::Point<float> worldPt, float maxDist)
{
    float closestDist = maxDist;
    juce::String closestId;
    const auto allPins = collectAllPins(nodes);

    auto findPinPos = [&allPins](const juce::String& nId, const juce::String& pId) -> juce::Point<float>
    {
        for (const auto& p : allPins)
            if (p.nodeId == nId && p.pinId == pId)
                return p.worldPos;
        return {};
    };

    for (const auto& c : cables)
    {
        const auto p1 = findPinPos(c.sourceNodeId, c.sourcePinId);
        const auto p2 = findPinPos(c.targetNodeId, c.targetPinId);
        const float dx = p2.x - p1.x;
        const float dy = p2.y - p1.y;
        const float lenSq = dx * dx + dy * dy;
        if (lenSq < 1.0f)
            continue;

        const float t = juce::jlimit(0.0f, 1.0f, ((worldPt.x - p1.x) * dx + (worldPt.y - p1.y) * dy) / lenSq);
        const juce::Point<float> proj = p1 + juce::Point<float>(dx * t, dy * t);
        const float d = worldPt.getDistanceFrom(proj);
        if (d < closestDist)
        {
            closestDist = d;
            closestId = c.id;
        }
    }
    return closestId;
}

// =============================================================================
// CanvasGraphHelper
// =============================================================================
CanvasNode CanvasGraphHelper::createFxNodeFromCatalog(const CatalogItem& fx, juce::Point<float> worldPos, int uniqueSeed, bool isVertical)
{
    CanvasNode node;
    node.id = fx.id + "-" + juce::String(uniqueSeed);
    node.type = NodeType::NestedEffect;
    node.label = fx.name;
    node.subtitle = fx.category.isNotEmpty() ? (fx.category + " - " + fx.hint) : "Effetto Modulare V2";
    node.pos = worldPos;
    node.targetPos = worldPos;
    node.isAnimatingPos = false;
    node.isVerticalFlow = isVertical;
    node.size = isVertical ? juce::Point<float>{ 245.0f, 175.0f } : juce::Point<float>{ 265.0f, 175.0f };
    node.crayonColour = fx.colour;
    node.innerBlocks = fx.templateBlocks;
    node.isBypassed = false;
    node.peakLevel = 0.0f;
    return node;
}

CanvasNode CanvasGraphHelper::createInstrumentNodeFromCatalog(const CatalogItem& item, juce::Point<float> worldPos, int uniqueSeed)
{
    CanvasNode node;
    node.id = item.id + "-" + juce::String(uniqueSeed);
    node.type = NodeType::InstrumentNode;
    node.label = item.name;
    node.subtitle = item.category + " (" + item.engine + ")";
    node.pos = worldPos;
    node.targetPos = worldPos;
    node.isAnimatingPos = false;
    node.isVerticalFlow = false;
    node.size = { 285.0f, 175.0f };
    node.crayonColour = item.colour;
    node.hasInstrument = true;
    node.instrumentName = item.name;
    node.instrumentEngine = item.engine;
    node.isBypassed = false;
    node.peakLevel = 0.0f;

    // Macro Flat Pastello dedicate per strumento V2
    if (item.category.containsIgnoreCase("Percussion") || item.id.containsIgnoreCase("kit") || item.id.containsIgnoreCase("drum") || item.id.containsIgnoreCase("percuss"))
    {
        node.label = "Percussioni";
        node.subtitle = "4 Voci discrete con trigger dedicati";
        node.size = { 330.0f, 185.0f };
        node.isDrumAdvanced = false;
        node.drumEditVoice = 0;
        node.drumPresetIdx = 0;
        node.directParams = {
            { "vol",   "Volume", 0.0f, 1.5f, 1.0f, 1.0f, "",     true },
            { "pitch", "Pitch", -12.0f, 12.0f, 0.0f, 0.0f, " st", true },
            { "decay", "Decay",  0.25f, 2.5f, 1.0f, 1.0f, " x",  true }
        };
    }
    else if (item.id.containsIgnoreCase("felt") || item.id.containsIgnoreCase("piano"))
    {
        node.directParams = {
            { "vol",    "Volume", 0.0f, 1.5f, 1.0f, 1.0f, "",     true },
            { "cutoff", "Cutoff", 200.0f, 12000.0f, 3600.0f, 3600.0f, " Hz", true },
            { "decay",  "Decay",  0.08f, 4.2f, 1.35f, 1.35f, " s",  true }
        };
    }
    else if (item.id.containsIgnoreCase("theremin"))
    {
        node.directParams = {
            { "vol",   "Volume", 0.0f, 1.5f, 1.0f, 1.0f, "",    true },
            { "scale", "Scala",  0.0f, 5.0f, 1.0f, 1.0f, "",    true },
            { "decay", "Glide",  0.01f, 0.2f, 0.045f, 0.045f, " s", true }
        };
    }
    else if (item.id.containsIgnoreCase("karplus") || item.id.containsIgnoreCase("pluck") || item.id.containsIgnoreCase("box"))
    {
        node.directParams = {
            { "vol",    "Volume",     0.0f, 1.5f, 1.0f, 1.0f, "",     true },
            { "decay",  "Damping",    0.1f, 4.0f, 1.5f, 1.5f, " s",   true },
            { "cutoff", "Brilliance", 200.0f, 12000.0f, 4500.0f, 4500.0f, " Hz", true }
        };
    }
    else if (item.id.containsIgnoreCase("scale") || item.id.containsIgnoreCase("quantiz"))
    {
        node.directParams = {
            { "scale",   "Scala",   0.0f, 6.0f, 0.0f, 0.0f, "",   true },
            { "root",    "Tonica",  0.0f, 11.0f, 0.0f, 0.0f, "",  true },
            { "octaves", "Ottave",  1.0f, 4.0f, 2.0f, 2.0f, "",   true }
        };
    }
    else if (item.id.containsIgnoreCase("looper") || item.id.containsIgnoreCase("tape_loop"))
    {
        node.directParams = {
            { "vol",      "Volume",  0.0f, 1.5f, 1.0f, 1.0f, "",   true },
            { "feedback", "Overdub", 0.0f, 1.0f, 0.85f, 0.85f, " %", true }
        };
    }
    else if (item.id.containsIgnoreCase("follower") || item.id.containsIgnoreCase("env"))
    {
        node.directParams = {
            { "attack",  "Attacco",  1.0f, 100.0f, 10.0f, 10.0f, " ms", true },
            { "release", "Rilascio", 10.0f, 800.0f, 120.0f, 120.0f, " ms", true },
            { "gain",    "Sensib.",  0.2f, 5.0f, 1.0f, 1.0f, " x", true }
        };
    }
    else if (item.id.containsIgnoreCase("lfo") || item.id.containsIgnoreCase("mod_lfo"))
    {
        node.size = { 285.0f, 185.0f };
        node.directParams = {
            { "rate",  "Velocita", 0.05f, 20.0f, 1.5f, 1.5f, " Hz", true },
            { "wave",  "Forma",    0.0f, 3.0f, 0.0f, 0.0f, "",      true },
            { "depth", "Ampiezza", 0.0f, 100.0f, 80.0f, 80.0f, " %", true }
        };
    }
    else // Synth VA
    {
        node.directParams = {
            { "vol",    "Volume",    0.0f, 1.5f, 1.0f, 1.0f, "",     true },
            { "cutoff", "Cutoff",    40.0f, 14000.0f, 3200.0f, 3200.0f, " Hz", true },
            { "res",    "Risonanza", 0.1f, 10.0f, 1.4f, 1.4f, "",    true },
            { "decay",  "Decay",     0.05f, 3.0f, 0.65f, 0.65f, " s",  true }
        };
    }
    return node;
}

} // namespace mimo
