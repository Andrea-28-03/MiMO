#include "HandTracker.h"
#include "HardwareWorkers.h"
#include "../GUI/MainComponent.h"
#include <cmath>
#include <algorithm>

namespace mimo
{

HandTrackingState HandTracker::analyzeHandFrame(MainComponent& comp, const juce::Image& frame)
{
    HandTrackingState st;
    if (!frame.isValid())
    {
        comp.hasSmoothedHandState = false;
        comp.smoothedHandState.hasHand = false;
        return st;
    }

    // Se il motore neurale Google MediaPipe e' attivo, usa direttamente l'analisi IA ufficiale
    if (comp.eyeServer != nullptr && comp.eyeServer->isMediaPipeActive())
    {
        return comp.eyeServer->getLatestState();
    }

    juce::Image small = frame.rescaled(160, 120, juce::Graphics::lowResamplingQuality);
    const int w = small.getWidth();
    const int h = small.getHeight();
    if (w <= 0 || h <= 0)
    {
        comp.hasSmoothedHandState = false;
        comp.smoothedHandState.hasHand = false;
        return st;
    }

    juce::Image::BitmapData bmp(small, juce::Image::BitmapData::readOnly);

    double sumX = 0.0;
    double sumY = 0.0;
    int skinPixelCount = 0;

    int minSkinX = w, maxSkinX = 0;
    int minSkinY = h, maxSkinY = 0;

    for (int y = 0; y < h; y += 2)
    {
        for (int x = 0; x < w; x += 2)
        {
            const auto col = bmp.getPixelColour(x, y);
            const int r = col.getRed();
            const int g = col.getGreen();
            const int b = col.getBlue();

            // Rilevamento colore pelle umana robusto (YCbCr + fallback RGB)
            const bool isSkin = isSkinPixel(r, g, b);

            if (isSkin)
            {
                sumX += x;
                sumY += y;
                skinPixelCount++;

                if (x < minSkinX) minSkinX = x;
                if (x > maxSkinX) maxSkinX = x;
                if (y < minSkinY) minSkinY = y;
                if (y > maxSkinY) maxSkinY = y;
            }
        }
    }

    const int totalSampledPixels = (w / 2) * (h / 2);
    const float skinRatio = static_cast<float>(skinPixelCount) / static_cast<float>(std::max(1, totalSampledPixels));

    if (skinRatio < 0.012f || skinPixelCount < 35)
    {
        st.hasHand = false;
        comp.hasSmoothedHandState = false;
        comp.smoothedHandState.hasHand = false;
        return st;
    }

    // Baricentro del candidato normalizzato (0.0 .. 1.0)
    const float rawPalmX = static_cast<float>(sumX / skinPixelCount) / static_cast<float>(w);
    const float rawPalmY = static_cast<float>(sumY / skinPixelCount) / static_cast<float>(h);

    const float spanX = static_cast<float>(std::max(1, maxSkinX - minSkinX)) / static_cast<float>(w);
    const float spanY = static_cast<float>(std::max(1, maxSkinY - minSkinY)) / static_cast<float>(h);
    const float palmSpan = std::max(spanX, spanY);
    const float rawPalmZ = juce::jlimit(0.12f, 0.95f, palmSpan * 1.8f);

    // =========================================================================
    // DISCRIMINATORE GEOMETRICO: MANO VS VOLTO / COLLO / CORPO
    // Una mano aperta presenta dita allungate con valli interdigitali evidenti.
    // Un volto e una forma solida, convessa ed ellittica priva di dita sporgenti.
    // =========================================================================
    constexpr int numRays = 16;
    float rayRadii[numRays];
    float maxRayR = 0.0f;
    float minRayR = 999.0f;

    for (int k = 0; k < numRays; ++k)
    {
        const float ang = static_cast<float>(k) * (juce::MathConstants<float>::twoPi / static_cast<float>(numRays));
        const float dx = std::cos(ang);
        const float dy = std::sin(ang);

        float lastSkinR = 2.0f;
        const float maxSearchR = std::min(static_cast<float>(std::min(w, h)) * 0.45f, 48.0f);

        for (float r = 3.0f; r <= maxSearchR; r += 1.5f)
        {
            const int px = static_cast<int>(rawPalmX * w + dx * r);
            const int py = static_cast<int>(rawPalmY * h + dy * r);
            if (px < 0 || px >= w || py < 0 || py >= h)
                break;

            const auto col = bmp.getPixelColour(px, py);
            if (isSkinPixel(col.getRed(), col.getGreen(), col.getBlue()))
            {
                lastSkinR = r;
            }
            else
            {
                const int px2 = static_cast<int>(rawPalmX * w + dx * (r + 3.0f));
                const int py2 = static_cast<int>(rawPalmY * h + dy * (r + 3.0f));
                if (px2 >= 0 && px2 < w && py2 >= 0 && py2 < h)
                {
                    const auto c2 = bmp.getPixelColour(px2, py2);
                    if (!isSkinPixel(c2.getRed(), c2.getGreen(), c2.getBlue()))
                        break;
                }
                else
                {
                    break;
                }
            }
        }

        rayRadii[k] = lastSkinR;
        if (lastSkinR > maxRayR) maxRayR = lastSkinR;
        if (lastSkinR < minRayR) minRayR = lastSkinR;
    }

    int fingerPeaks = 0;
    for (int k = 0; k < numRays; ++k)
    {
        const float prevR = rayRadii[(k + numRays - 1) % numRays];
        const float nextR = rayRadii[(k + 1) % numRays];
        const float curR = rayRadii[k];

        if (curR > prevR && curR > nextR)
        {
            const float valleyR = std::min(rayRadii[(k + numRays - 2) % numRays], rayRadii[(k + 2) % numRays]);
            if (curR >= valleyR * 1.32f && curR >= 9.0f)
            {
                fingerPeaks++;
            }
        }
    }

    const bool isCenterTopFaceArea = (rawPalmY < 0.68f && rawPalmX > 0.22f && rawPalmX < 0.78f);
    const bool isVeryLargeSkinBlob = (skinRatio > 0.080f);

    // Solo una sagoma massiccia priva di dita puo' essere considerata un volto
    if (isVeryLargeSkinBlob || (isCenterTopFaceArea && skinRatio > 0.050f && fingerPeaks < 1))
    {
        st.hasHand = false;
        comp.hasSmoothedHandState = false;
        comp.smoothedHandState.hasHand = false;
        return st;
    }

    // Se non ci sono dita sporgenti (fingerPeaks < 1), verifica se e' un pugno chiuso compatto
    if (fingerPeaks < 1)
    {
        if (skinPixelCount >= 50 && skinPixelCount <= 5500)
        {
            st.hasHand = true;
            st.palmX = rawPalmX;
            st.palmY = rawPalmY;
            st.palmZ = rawPalmZ;
            st.fist = 1.0f; // Pugno chiuso
            st.handTilt = 0.5f;
            for (int f = 0; f < 5; ++f)
                st.fingerFlex[f] = 1.0f;
            st.handedness = "Right";

            if (comp.hasSmoothedHandState)
            {
                const float alpha = 0.65f;
                st.palmX = comp.smoothedHandState.palmX * (1.0f - alpha) + st.palmX * alpha;
                st.palmY = comp.smoothedHandState.palmY * (1.0f - alpha) + st.palmY * alpha;
                st.palmZ = comp.smoothedHandState.palmZ * (1.0f - alpha) + st.palmZ * alpha;
                st.fist  = comp.smoothedHandState.fist  * (1.0f - alpha) + st.fist  * alpha;
            }
            comp.smoothedHandState = st;
            comp.hasSmoothedHandState = true;
            return st;
        }
        else
        {
            st.hasHand = false;
            comp.hasSmoothedHandState = false;
            comp.smoothedHandState.hasHand = false;
            return st;
        }
    }

    // Superati i test: mano rilevata!
    st.hasHand = true;

    // Momenti del secondo ordine per orientamento / asse principale
    double m11 = 0.0, m20 = 0.0, m02 = 0.0;
    const double cx = sumX / skinPixelCount;
    const double cy = sumY / skinPixelCount;

    for (int y = minSkinY; y <= maxSkinY; y += 3)
    {
        for (int x = minSkinX; x <= maxSkinX; x += 3)
        {
            const auto col = bmp.getPixelColour(x, y);
            if (isSkinPixel(col.getRed(), col.getGreen(), col.getBlue()))
            {
                const double dx = x - cx;
                const double dy = y - cy;
                m20 += dx * dx;
                m02 += dy * dy;
                m11 += dx * dy;
            }
        }
    }

    const double theta = 0.5 * std::atan2(2.0 * m11, m20 - m02);
    const float rawTilt = juce::jlimit(0.0f, 1.0f, static_cast<float>(0.5 + (theta / juce::MathConstants<double>::pi)));

    float mainAng = static_cast<float>(theta) - juce::MathConstants<float>::halfPi;
    // In coordinate immagine (Y verso il basso), una mano sollevata davanti alla webcam ha le dita verso l'alto (-Y)
    if (std::sin(mainAng) > 0.15f)
        mainAng += juce::MathConstants<float>::pi;

    const float baseHandRadius = std::max(0.09f, palmSpan * 0.45f);

    // Angoli delle 5 dita rispetto alla direzione principale (Thumb, Index, Middle, Ring, Pinky)
    const float fingerAngles[5] = { -0.92f, -0.40f, 0.0f, 0.38f, 0.76f };
    const float baseTipLengths[5] = { 0.85f, 1.05f, 1.15f, 1.02f, 0.82f };

    float fingerExt[5] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    juce::Point<float> rawTips[5];

    for (int f = 0; f < 5; ++f)
    {
        const float ang = mainAng + fingerAngles[f];
        const float dirX = std::cos(ang);
        const float dirY = std::sin(ang);

        const float maxR = baseHandRadius * baseTipLengths[f];
        float bestR = baseHandRadius * 0.48f;

        for (float rStep = 0.04f; rStep <= maxR * 1.45f; rStep += 0.02f)
        {
            const int px = static_cast<int>((rawPalmX + dirX * rStep) * w);
            const int py = static_cast<int>((rawPalmY + dirY * rStep) * h);

            if (px >= 0 && px < w && py >= 0 && py < h)
            {
                const auto col = bmp.getPixelColour(px, py);
                if (isSkinPixel(col.getRed(), col.getGreen(), col.getBlue()))
                {
                    bestR = rStep;
                }
            }
        }

        const float normExt = juce::jlimit(0.25f, 1.0f, bestR / maxR);
        fingerExt[f] = normExt;
        rawTips[f] = {
            juce::jlimit(0.03f, 0.97f, rawPalmX + dirX * bestR),
            juce::jlimit(0.03f, 0.97f, rawPalmY + dirY * bestR)
        };
        st.fingerFlex[f] = juce::jlimit(0.0f, 1.0f, (1.0f - normExt) * 1.4f);
    }

    st.fist = juce::jlimit(0.0f, 1.0f, (st.fingerFlex[1] + st.fingerFlex[2] + st.fingerFlex[3] + st.fingerFlex[4]) * 0.25f);

    const float wristAng = mainAng + juce::MathConstants<float>::pi;
    const juce::Point<float> rawWrist = {
        juce::jlimit(0.03f, 0.97f, rawPalmX + std::cos(wristAng) * baseHandRadius * 0.88f),
        juce::jlimit(0.03f, 0.97f, rawPalmY + std::sin(wristAng) * baseHandRadius * 0.88f)
    };

    // 0: Polso
    st.landmarks[0] = { rawWrist.x, rawWrist.y, 0.0f };

    const int tipIndices[5] = { 4, 8, 12, 16, 20 };
    const int mcpIndices[5] = { 1, 5, 9, 13, 17 };

    for (int f = 0; f < 5; ++f)
    {
        const int mcpIdx = mcpIndices[f];
        const int tipIdx = tipIndices[f];

        const float mcpFrac = (f == 0) ? 0.35f : 0.44f;
        const juce::Point<float> mcpPt = {
            rawWrist.x * (1.0f - mcpFrac) + rawTips[f].x * mcpFrac,
            rawWrist.y * (1.0f - mcpFrac) + rawTips[f].y * mcpFrac
        };
        st.landmarks[mcpIdx] = { mcpPt.x, mcpPt.y, 0.0f };

        const juce::Point<float> pipPt = {
            mcpPt.x * 0.60f + rawTips[f].x * 0.40f,
            mcpPt.y * 0.60f + rawTips[f].y * 0.40f
        };
        st.landmarks[mcpIdx + 1] = { pipPt.x, pipPt.y, 0.0f };

        const juce::Point<float> dipPt = {
            mcpPt.x * 0.25f + rawTips[f].x * 0.75f,
            mcpPt.y * 0.25f + rawTips[f].y * 0.75f
        };
        st.landmarks[mcpIdx + 2] = { dipPt.x, dipPt.y, 0.0f };

        st.landmarks[tipIdx] = { rawTips[f].x, rawTips[f].y, 0.0f };
    }

    // 4 Pinch indipendenti (Pollice [4] con Indice [8], Medio [12], Anulare [16], Mignolo [20])
    const juce::Point<float> thumbTip(st.landmarks[4].x, st.landmarks[4].y);
    for (int p = 0; p < 4; ++p)
    {
        const int targetTipIdx = tipIndices[p + 1];
        const juce::Point<float> targetTip(st.landmarks[targetTipIdx].x, st.landmarks[targetTipIdx].y);
        const float dist = thumbTip.getDistanceFrom(targetTip);

        const float pinchStrength = juce::jlimit(0.0f, 1.0f, 1.0f - (dist / 0.09f));
        st.pinches[p] = pinchStrength;
        st.pinchActive[p] = (pinchStrength > 0.52f);
    }

    st.palmX = rawPalmX;
    st.palmY = rawPalmY;
    st.palmZ = rawPalmZ;
    st.handTilt = rawTilt;
    st.handedness = (rawTips[0].x < rawTips[4].x) ? "Right" : "Left";

    // Filtro Mano Destra / Sinistra impostato dall'utente
    if (comp.eyeHandFilter == 1 && st.handedness != "Right")
    {
        st.hasHand = false;
        comp.hasSmoothedHandState = false;
        comp.smoothedHandState.hasHand = false;
        return st;
    }
    if (comp.eyeHandFilter == 2 && st.handedness != "Left")
    {
        st.hasHand = false;
        comp.hasSmoothedHandState = false;
        comp.smoothedHandState.hasHand = false;
        return st;
    }

    // Temporal Smoothing (EMA) per fluidita estrema e zero jitter a 60 FPS
    if (comp.hasSmoothedHandState)
    {
        const float alpha = 0.65f;
        st.palmX = comp.smoothedHandState.palmX * (1.0f - alpha) + st.palmX * alpha;
        st.palmY = comp.smoothedHandState.palmY * (1.0f - alpha) + st.palmY * alpha;
        st.palmZ = comp.smoothedHandState.palmZ * (1.0f - alpha) + st.palmZ * alpha;
        st.fist  = comp.smoothedHandState.fist  * (1.0f - alpha) + st.fist  * alpha;
        st.handTilt = comp.smoothedHandState.handTilt * (1.0f - alpha) + st.handTilt * alpha;

        for (size_t i = 0; i < 21; ++i)
        {
            st.landmarks[i].x = comp.smoothedHandState.landmarks[i].x * (1.0f - alpha) + st.landmarks[i].x * alpha;
            st.landmarks[i].y = comp.smoothedHandState.landmarks[i].y * (1.0f - alpha) + st.landmarks[i].y * alpha;
            st.landmarks[i].z = comp.smoothedHandState.landmarks[i].z * (1.0f - alpha) + st.landmarks[i].z * alpha;
        }

        for (size_t p = 0; p < 4; ++p)
        {
            if (st.pinchActive[p] && !comp.smoothedHandState.pinchActive[p])
            {
                st.pinchTrigger[p] = true;
                st.pinchVelocity[p] = juce::jlimit(0.4f, 1.0f, 0.7f + st.pinches[p] * 0.3f);

                if (comp.eyeGesturePinchTrig && comp.isHandTriggersEnabled(false))
                {
                    comp.audioEngine.triggerFromSource("hw-eye", static_cast<int>(p), st.pinchVelocity[p], false);
                    comp.lastTriggerSourceCode.store(3);
                    comp.lastTriggerFingerIdx.store(static_cast<int>(p));
                    comp.lastTriggerIsSecondary.store(false);
                    comp.triggerPulseAlpha.store(1.0f);
                }
            }
        }
    }

    comp.smoothedHandState = st;
    comp.hasSmoothedHandState = true;
    return st;
}

} // namespace mimo
