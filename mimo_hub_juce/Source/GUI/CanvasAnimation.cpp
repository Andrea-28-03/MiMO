#include "CanvasAnimation.h"
#include "MainComponent.h"
#include "MiMOLookAndFeel.h"
#include <cmath>
#include <algorithm>

namespace mimo
{

void CanvasAnimation::updateModulations(MainComponent& comp, bool& needsRepaint)
{
    if (comp.dragMode == MainComponent::DragMode::TurnKnob || comp.dragMode == MainComponent::DragMode::TurnInnerKnob)
        return;

    for (const auto& c : comp.cables)
    {
        if (!c.isModulation || (!c.targetPinId.startsWith("param:") && c.targetPinId != "inst:tone"))
            continue;

        float modVal = -1.0f;
        float dspMod = -1.0f;

        const CanvasNode* srcNode = nullptr;
        for (const auto& n : comp.nodes)
        {
            if (n.id == c.sourceNodeId)
            {
                srcNode = &n;
                break;
            }
        }

        if (srcNode != nullptr && (srcNode->isIntegratedCamera || srcNode->id == "hw-webcam" || srcNode->id == "hw-eye" ||
                                   srcNode->isIntegratedMic || srcNode->id == "hw-mic" || srcNode->id == "hw-drum"))
        {
            if ((srcNode->isIntegratedMic || srcNode->id == "hw-mic") && comp.isMicRecording)
            {
                if (c.sourcePinId == "mod:vel")
                    modVal = comp.realMicVelocity.load();
                else if (c.sourcePinId == "mod:press")
                    modVal = comp.realMicLevel.load();
            }
            else if (srcNode->isIntegratedCamera || srcNode->id == "hw-webcam" || srcNode->id == "hw-eye")
            {
                const bool dxOn = comp.isHandEnabled(false);
                const bool sxOn = comp.isHandEnabled(true);

                const bool hasPrimary = comp.latestEyeState.hasHand;
                const bool hasSecondary = comp.latestEyeState.hasSecondaryHand;

                if (hasPrimary || hasSecondary)
                {
                    const bool useSecondaryForDx = !hasPrimary && hasSecondary;
                    const bool usePrimaryForSx = !hasSecondary && hasPrimary;

                    const float pX = hasPrimary ? comp.latestEyeState.palmX : (useSecondaryForDx ? comp.latestEyeState.secondaryPalmX : -1.0f);
                    const float pY = hasPrimary ? comp.latestEyeState.palmY : (useSecondaryForDx ? comp.latestEyeState.secondaryPalmY : -1.0f);
                    const float pZ = hasPrimary ? comp.latestEyeState.palmZ : (useSecondaryForDx ? comp.latestEyeState.secondaryPalmZ : -1.0f);
                    const float pFist = hasPrimary ? comp.latestEyeState.fist : (useSecondaryForDx ? comp.latestEyeState.secondaryFist : -1.0f);
                    const float pTilt = hasPrimary ? comp.latestEyeState.handTilt : (useSecondaryForDx ? comp.latestEyeState.secondaryHandTilt : -1.0f);

                    const float sX = hasSecondary ? comp.latestEyeState.secondaryPalmX : (usePrimaryForSx ? comp.latestEyeState.palmX : -1.0f);
                    const float sY = hasSecondary ? comp.latestEyeState.secondaryPalmY : (usePrimaryForSx ? comp.latestEyeState.palmY : -1.0f);
                    const float sZ = hasSecondary ? comp.latestEyeState.secondaryPalmZ : (usePrimaryForSx ? comp.latestEyeState.palmZ : -1.0f);
                    const float sFist = hasSecondary ? comp.latestEyeState.secondaryFist : (usePrimaryForSx ? comp.latestEyeState.fist : -1.0f);
                    const float sTilt = hasSecondary ? comp.latestEyeState.secondaryHandTilt : (usePrimaryForSx ? comp.latestEyeState.handTilt : -1.0f);

                    auto getDxFinger = [&](int idx) -> float {
                        if (hasPrimary) return comp.latestEyeState.fingerFlex[static_cast<size_t>(idx)];
                        if (useSecondaryForDx) return comp.latestEyeState.secondaryFingerFlex[static_cast<size_t>(idx)];
                        return -1.0f;
                    };
                    auto getSxFinger = [&](int idx) -> float {
                        if (hasSecondary) return comp.latestEyeState.secondaryFingerFlex[static_cast<size_t>(idx)];
                        if (usePrimaryForSx) return comp.latestEyeState.fingerFlex[static_cast<size_t>(idx)];
                        return -1.0f;
                    };

                    if (dxOn)
                    {
                        if (c.sourcePinId == "mod:dx_x" || c.sourcePinId == "mod:x")
                            modVal = comp.isGroupEnabled("eye_dx_palm") ? pX : -1.0f;
                        else if (c.sourcePinId == "mod:dx_y" || c.sourcePinId == "mod:y")
                            modVal = comp.isGroupEnabled("eye_dx_palm") ? pY : -1.0f;
                        else if (c.sourcePinId == "mod:dx_z" || c.sourcePinId == "mod:z" || c.sourcePinId == "mod:mot")
                            modVal = comp.isGroupEnabled("eye_dx_palm") ? pZ : -1.0f;
                        else if (c.sourcePinId == "mod:dx_fist" || c.sourcePinId == "mod:fist")
                            modVal = comp.isGroupEnabled("eye_dx_palm") ? pFist : -1.0f;
                        else if (c.sourcePinId == "mod:dx_tilt" || c.sourcePinId == "mod:tilt")
                            modVal = comp.isGroupEnabled("eye_dx_palm") ? pTilt : -1.0f;
                        else if (c.sourcePinId == "mod:dx_thumb" || c.sourcePinId == "mod:thumb")
                            modVal = comp.isGroupEnabled("eye_dx_fingers") ? getDxFinger(0) : -1.0f;
                        else if (c.sourcePinId == "mod:dx_idx" || c.sourcePinId == "mod:idx" || c.sourcePinId == "mod:pinch_idx")
                            modVal = comp.isGroupEnabled("eye_dx_fingers") ? getDxFinger(1) : -1.0f;
                        else if (c.sourcePinId == "mod:dx_mid" || c.sourcePinId == "mod:mid" || c.sourcePinId == "mod:pinch_mid")
                            modVal = comp.isGroupEnabled("eye_dx_fingers") ? getDxFinger(2) : -1.0f;
                        else if (c.sourcePinId == "mod:dx_rng" || c.sourcePinId == "mod:rng" || c.sourcePinId == "mod:pinch_rng")
                            modVal = comp.isGroupEnabled("eye_dx_fingers") ? getDxFinger(3) : -1.0f;
                        else if (c.sourcePinId == "mod:dx_pky" || c.sourcePinId == "mod:pky" || c.sourcePinId == "mod:pinch_pky")
                            modVal = comp.isGroupEnabled("eye_dx_fingers") ? getDxFinger(4) : -1.0f;
                    }

                    if (sxOn && modVal < 0.0f)
                    {
                        if (c.sourcePinId == "mod:sx_x")
                            modVal = comp.isGroupEnabled("eye_sx_palm") ? sX : -1.0f;
                        else if (c.sourcePinId == "mod:sx_y")
                            modVal = comp.isGroupEnabled("eye_sx_palm") ? sY : -1.0f;
                        else if (c.sourcePinId == "mod:sx_z")
                            modVal = comp.isGroupEnabled("eye_sx_palm") ? sZ : -1.0f;
                        else if (c.sourcePinId == "mod:sx_fist")
                            modVal = comp.isGroupEnabled("eye_sx_palm") ? sFist : -1.0f;
                        else if (c.sourcePinId == "mod:sx_tilt")
                            modVal = comp.isGroupEnabled("eye_sx_palm") ? sTilt : -1.0f;
                        else if (c.sourcePinId == "mod:sx_thumb")
                            modVal = comp.isGroupEnabled("eye_sx_fingers") ? getSxFinger(0) : -1.0f;
                        else if (c.sourcePinId == "mod:sx_idx")
                            modVal = comp.isGroupEnabled("eye_sx_fingers") ? getSxFinger(1) : -1.0f;
                        else if (c.sourcePinId == "mod:sx_mid")
                            modVal = comp.isGroupEnabled("eye_sx_fingers") ? getSxFinger(2) : -1.0f;
                        else if (c.sourcePinId == "mod:sx_rng")
                            modVal = comp.isGroupEnabled("eye_sx_fingers") ? getSxFinger(3) : -1.0f;
                        else if (c.sourcePinId == "mod:sx_pky")
                            modVal = comp.isGroupEnabled("eye_sx_fingers") ? getSxFinger(4) : -1.0f;
                    }
                }
                else if (comp.isCameraRecording)
                {
                    const float optX = juce::jlimit(0.0f, 1.0f, (comp.realCameraCentroidX.load() + 1.0f) * 0.5f);
                    const float optY = juce::jlimit(0.0f, 1.0f, 1.0f - (comp.realCameraCentroidY.load() + 1.0f) * 0.5f);
                    float optZ = comp.realCameraMotion.load();
                    if (comp.cameraTrackingMode == CameraTrackingMode::Light)
                        optZ = comp.realCameraBrightness.load();
                    else if (comp.cameraTrackingMode == CameraTrackingMode::Presence)
                        optZ = juce::jlimit(0.0f, 1.0f, 1.0f - std::hypot(comp.realCameraCentroidX.load(), comp.realCameraCentroidY.load()) * 0.65f);

                    const float optFist = comp.realCameraMotion.load();

                    if (dxOn)
                    {
                        if (c.sourcePinId == "mod:dx_x" || c.sourcePinId == "mod:x")
                            modVal = optX;
                        else if (c.sourcePinId == "mod:dx_y" || c.sourcePinId == "mod:y")
                            modVal = optY;
                        else if (c.sourcePinId == "mod:dx_z" || c.sourcePinId == "mod:z" || c.sourcePinId == "mod:mot")
                            modVal = optZ;
                        else if (c.sourcePinId == "mod:dx_fist" || c.sourcePinId == "mod:fist")
                            modVal = optFist;
                        else if (c.sourcePinId == "mod:dx_tilt" || c.sourcePinId == "mod:tilt")
                            modVal = optX;
                        else if (c.sourcePinId.startsWith("mod:dx_"))
                            modVal = optFist;
                    }
                    if (sxOn && modVal < 0.0f)
                    {
                        if (c.sourcePinId == "mod:sx_x")
                            modVal = optX;
                        else if (c.sourcePinId == "mod:sx_y")
                            modVal = optY;
                        else if (c.sourcePinId == "mod:sx_z")
                            modVal = optZ;
                        else if (c.sourcePinId == "mod:sx_fist")
                            modVal = optFist;
                        else if (c.sourcePinId == "mod:sx_tilt")
                            modVal = optX;
                        else if (c.sourcePinId.startsWith("mod:sx_"))
                            modVal = optFist;
                    }
                }
            }
        }
        else
        {
            dspMod = comp.audioEngine.getNodeModulationValue(c.sourceNodeId, c.sourcePinId);
            if (dspMod >= 0.0f)
                modVal = dspMod;
        }

        if (modVal >= 0.0f)
        {
            modVal = juce::jlimit(0.0f, 1.0f, modVal);
            if (c.targetPinId == "inst:tone")
            {
                const float targetHz = 120.0f + modVal * 6800.0f;
                comp.synthFilterCutoffHz.store(targetHz);
            }
            else
            {
                const auto paramSpec = c.targetPinId.fromFirstOccurrenceOf("param:", false, false);
                juce::String blockId;
                juce::String paramKey;
                if (paramSpec.contains("."))
                {
                    blockId = paramSpec.upToFirstOccurrenceOf(".", false, false);
                    paramKey = paramSpec.fromFirstOccurrenceOf(".", false, false);
                }
                else
                {
                    blockId = "";
                    paramKey = paramSpec;
                }

                for (auto& dst : comp.nodes)
                {
                    if (dst.id != c.targetNodeId)
                        continue;

                    for (auto& dp : dst.directParams)
                    {
                        if (dp.key == paramKey)
                        {
                            const float curNorm = dp.getNormalized();
                            const float nextNorm = (dspMod >= 0.0f) ? modVal : (curNorm * 0.35f + modVal * 0.65f);
                            if (std::abs(nextNorm - curNorm) > 0.0005f)
                            {
                                dp.setFromNormalized(nextNorm);
                                comp.audioEngine.setModulation(dst.id, dp.key, nextNorm);
                                needsRepaint = true;
                            }
                        }
                    }

                    for (auto& b : dst.innerBlocks)
                    {
                        if (!blockId.isEmpty() && blockId != "direct" && b.id != blockId)
                            continue;
                        for (auto& p : b.params)
                        {
                            if (p.key == paramKey)
                            {
                                const float curNorm = p.getNormalized();
                                const float nextNorm = (dspMod >= 0.0f) ? modVal : (curNorm * 0.35f + modVal * 0.65f);
                                if (std::abs(nextNorm - curNorm) > 0.0005f)
                                {
                                    p.setFromNormalized(nextNorm);
                                    comp.audioEngine.setModulation(dst.id, p.key, nextNorm);
                                    needsRepaint = true;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void CanvasAnimation::updateTimerTick(MainComponent& comp, bool& needsRepaint)
{
    ++comp.frameCounter;

    // 0. Decelerazione cinetica e inerzia fisica a 60 FPS al rilascio del touchpad/rotellina
    comp.timeSinceLastWheel += 1.0f / 60.0f;
    if (comp.timeSinceLastWheel > 0.032f)
    {
        const float currentSpeed = std::hypot(comp.panVelocity.x, comp.panVelocity.y);
        if (currentSpeed > 0.12f)
        {
            comp.canvasOffset.x += comp.panVelocity.x;
            comp.canvasOffset.y += comp.panVelocity.y;
            comp.panVelocity *= 0.90f;
            needsRepaint = true;
        }
        else if (!comp.panVelocity.isOrigin())
        {
            comp.panVelocity = { 0.0f, 0.0f };
        }
    }

    // 1. Animazione allargamento fluido dei nodi (Spring / LERP a 60 FPS)
    bool anyNodeAnimating = false;
    for (auto& n : comp.nodes)
    {
        if (n.isAnimatingPos)
        {
            const float nDiffX = n.targetPos.x - n.pos.x;
            const float nDiffY = n.targetPos.y - n.pos.y;
            if (std::abs(nDiffX) > 0.8f || std::abs(nDiffY) > 0.8f)
            {
                n.pos.x += nDiffX * 0.32f;
                n.pos.y += nDiffY * 0.32f;
                anyNodeAnimating = true;
            }
            else
            {
                n.pos = n.targetPos;
                n.isAnimatingPos = false;
            }
        }
    }
    if (anyNodeAnimating)
        needsRepaint = true;

    // 2. Countdown Polaroid nello Studio
    if (comp.hasPendingPolaroid && !comp.polaroidHasTyped)
    {
        if (comp.frameCounter % 10 == 0)
        {
            --comp.polaroidCountdownTicks;
            if (comp.polaroidCountdownTicks <= 0)
                comp.commitPolaroidToAlbum(comp.pendingPolaroid);
            needsRepaint = true;
        }
    }

    // 3. Line-boil dei cavi d'inchiostro
    if (comp.activeTab == MainComponent::ActiveTab::Studio && !comp.cables.empty())
    {
        if (comp.frameCounter % 5 == 0)
        {
            comp.boilTick = (comp.boilTick + 1) % 12;
            needsRepaint = true;
        }
    }

    // 4. Animazioni Workspace LIVE
    if (comp.activeTab == MainComponent::ActiveTab::Live)
    {
        if (comp.looperState != MainComponent::LooperState::Idle)
        {
            const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
            const double elapsed = now - comp.looperStartTime;
            comp.looperSweepProgress = static_cast<float>(std::fmod(elapsed, comp.looperDurationSec) / comp.looperDurationSec);
            needsRepaint = true;
        }

        if (comp.holdSlotIndex >= 0)
        {
            comp.holdProgress += 1.0f / 60.0f;
            if (comp.holdProgress >= 1.0f)
            {
                if (comp.selectedAlbumIndex >= 0 && comp.selectedAlbumIndex < static_cast<int>(comp.photoAlbum.size()))
                {
                    comp.assignPolaroidToSlot(comp.holdSlotIndex, comp.photoAlbum[static_cast<size_t>(comp.selectedAlbumIndex)]);
                    comp.selectedAlbumIndex = -1;
                }
                else
                {
                    comp.triggerSceneSlot(comp.holdSlotIndex);
                }
                comp.holdSlotIndex = -1;
                comp.holdProgress = 0.0f;
            }
            needsRepaint = true;
        }

        if (comp.snapAnimScale > 1.002f)
        {
            comp.snapAnimScale += (1.0f - comp.snapAnimScale) * 0.16f;
            needsRepaint = true;
        }
    }

    // 5. Hardware & Parametri Synth
    comp.updateHardwareDeviceStates();
    comp.micThresholdAtomic.store(comp.getMicThreshold());

    for (const auto& n : comp.nodes)
    {
        if (n.isIntegratedMic || n.id == "hw-mic" || n.id == "hw-drum")
        {
            if (n.instrumentName.containsIgnoreCase("Giocattolo"))
                comp.synthInstrumentType.store(1);
            else if (n.instrumentName.containsIgnoreCase("Piano") || n.instrumentName.containsIgnoreCase("Carillon"))
                comp.synthInstrumentType.store(2);
            else if (n.instrumentName.containsIgnoreCase("Theremin") || n.instrumentName.containsIgnoreCase("Pad"))
                comp.synthInstrumentType.store(3);
            else
                comp.synthInstrumentType.store(0);
        }
        else if (n.type == NodeType::NestedEffect)
        {
            for (const auto& b : n.innerBlocks)
            {
                for (const auto& p : b.params)
                {
                    if (p.key == "cutoff" || p.unit.containsIgnoreCase("Hz"))
                        comp.synthFilterCutoffHz.store(p.value);
                    else if (p.key == "res" || p.key == "q")
                        comp.synthFilterReso.store(p.getNormalized());
                }
            }
        }
        else if (n.type == NodeType::MasterOut && !n.directParams.empty())
        {
            comp.synthMasterGain.store(juce::Decibels::decibelsToGain(n.directParams[0].value, -48.0f));
        }
    }

    {
        static int lastMicSt = -1;
        const int ms = comp.micStateAtomic.load();
        if (ms != lastMicSt)
        {
            lastMicSt = ms;
            needsRepaint = true;
        }
    }

    if (comp.isMicRecording)
    {
        bool micChanged = false;
        if (comp.realMicTriggered.load())
        {
            comp.micTriggerFired = true;
            comp.micTriggerTicks = 8;
            comp.realMicTriggered.store(false);
            micChanged = true;
        }
        else if (comp.micTriggerTicks > 0)
        {
            --comp.micTriggerTicks;
            if (comp.micTriggerTicks == 0)
            {
                comp.micTriggerFired = false;
                micChanged = true;
            }
        }

        if (micChanged)
        {
            for (auto& n : comp.nodes)
                if (n.isIntegratedMic || n.id == "hw-mic")
                    n.crayonColour = comp.micTriggerFired ? MiMOLookAndFeel::crayonCherry : MiMOLookAndFeel::crayonOchre;
        }

        const float lvl = comp.realMicLevel.load();
        if (micChanged || std::abs(lvl - comp.lastDrawnMicLevel) > 0.010f)
        {
            comp.lastDrawnMicLevel = lvl;
            needsRepaint = true;
        }
    }
    else
    {
        comp.micTriggerFired = false;
        comp.micTriggerTicks = 0;
    }

    {
        static int lastCamState = -1;
        const int cs = comp.cameraState.load();
        if (cs != lastCamState)
        {
            lastCamState = cs;
            needsRepaint = true;
        }
    }

    if (comp.isCameraRecording && comp.hasNewWebcamFrame.load())
    {
        comp.hasNewWebcamFrame.store(false);
        needsRepaint = true;
    }

    if (comp.eyeServer != nullptr)
    {
        if (comp.eyeServer->isMediaPipeActive())
            comp.latestEyeState = comp.eyeServer->getLatestState();
        else
            comp.latestEyeState = comp.smoothedHandState;

        auto pinchTrigs = comp.eyeServer->consumePinchTriggers();
        if (comp.eyeGesturePinchTrig && comp.eyePhalanxDetail >= 1 && !pinchTrigs.empty())
            needsRepaint = true;

        if ((comp.latestEyeState.hasHand && comp.isCameraInCanvas()) ||
            (comp.isCameraRecording && comp.isCameraInCanvas()) ||
            (comp.eyeDrawerOpen && comp.frameCounter % 10 == 0))
        {
            needsRepaint = true;
        }

        if (comp.isMicInCanvas() && comp.isMicRecording && (comp.realMicLevel.load() > 0.005f || comp.realMicVelocity.load() > 0.005f))
            needsRepaint = true;
    }

    updateModulations(comp, needsRepaint);

    for (auto& n : comp.nodes)
    {
        const float pk = comp.audioEngine.getNodePeak(n.id);
        const float prev = n.peakLevel;
        n.peakLevel = std::max(pk, prev * 0.82f);
        if (std::abs(n.peakLevel - prev) > 0.012f)
            needsRepaint = true;
    }

    if (comp.latestEyeState.hasHand)
        comp.audioEngine.setHand3D(comp.latestEyeState.palmX, comp.latestEyeState.palmY, comp.latestEyeState.palmZ, true);
    else
        comp.audioEngine.setHand3D(0.5f, 0.5f, 0.5f, false);

    const float pulse = comp.triggerPulseAlpha.load();
    if (pulse > 0.02f)
    {
        comp.triggerPulseAlpha.store(pulse * 0.88f);
        needsRepaint = true;
    }
}

} // namespace mimo
