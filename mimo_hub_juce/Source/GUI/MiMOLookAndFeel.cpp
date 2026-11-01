#include "MiMOLookAndFeel.h"
#include <cmath>

namespace mimo
{

bool MiMOLookAndFeel::isDarkMode = true;

// Palette Notte "Blueprint / Charcoal Studio" (Predefinita di sistema)
juce::Colour MiMOLookAndFeel::paperBg       = juce::Colour(0xff16181d);
juce::Colour MiMOLookAndFeel::paperCard     = juce::Colour(0xff222631);
juce::Colour MiMOLookAndFeel::polaroidWhite = juce::Colour(0xff2d323e);
juce::Colour MiMOLookAndFeel::fountainInk   = juce::Colour(0xffedf1f7);
juce::Colour MiMOLookAndFeel::inkMuted      = juce::Colour(0xff9aa6b8);

juce::Colour MiMOLookAndFeel::crayonCherry  = juce::Colour(0xffff7575);
juce::Colour MiMOLookAndFeel::crayonOchre   = juce::Colour(0xfff5cb58);
juce::Colour MiMOLookAndFeel::crayonSage    = juce::Colour(0xff7de493);
juce::Colour MiMOLookAndFeel::crayonCyan    = juce::Colour(0xff68bbf8);
juce::Colour MiMOLookAndFeel::crayonLilac   = juce::Colour(0xffcca8f9);
juce::Colour MiMOLookAndFeel::crayonPeach   = juce::Colour(0xffff9f64);
juce::Colour MiMOLookAndFeel::crayonMint    = juce::Colour(0xff60d892);

void MiMOLookAndFeel::setDarkMode(bool dark)
{
    isDarkMode = dark;
    if (dark)
    {
        // Palette Notte "Blueprint / Charcoal Studio"
        paperBg       = juce::Colour(0xff16181d);
        paperCard     = juce::Colour(0xff222631);
        polaroidWhite = juce::Colour(0xff2d323e);
        fountainInk   = juce::Colour(0xffedf1f7);
        inkMuted      = juce::Colour(0xff9aa6b8);

        crayonCherry  = juce::Colour(0xffff7575);
        crayonOchre   = juce::Colour(0xfff5cb58);
        crayonSage    = juce::Colour(0xff7de493);
        crayonCyan    = juce::Colour(0xff68bbf8);
        crayonLilac   = juce::Colour(0xffcca8f9);
        crayonPeach   = juce::Colour(0xffff9f64);
        crayonMint    = juce::Colour(0xff60d892);
    }
    else
    {
        // Palette Giorno "Sketchbook / Cotton Paper"
        paperBg       = juce::Colour(0xfff3ecdc);
        paperCard     = juce::Colour(0xfff9f4e8);
        polaroidWhite = juce::Colour(0xfffcfaf2);
        fountainInk   = juce::Colour(0xff1f1a17);
        inkMuted      = juce::Colour(0xff6e6359);

        crayonCherry  = juce::Colour(0xffe76f6f);
        crayonOchre   = juce::Colour(0xffe9b949);
        crayonSage    = juce::Colour(0xff8fb996);
        crayonCyan    = juce::Colour(0xff5fb7d4);
        crayonLilac   = juce::Colour(0xffb39ddb);
        crayonPeach   = juce::Colour(0xfff4a261);
        crayonMint    = juce::Colour(0xff9fd8c4);
    }
}

MiMOLookAndFeel::MiMOLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, paperBg);
    setColour(juce::Label::textColourId, fountainInk);
    setColour(juce::TextButton::textColourOffId, fountainInk);
    setColour(juce::TextButton::textColourOnId, fountainInk);
}

float MiMOLookAndFeel::seededFloat(int seed, int index)
{
    unsigned int h = static_cast<unsigned int>(seed * 374761393 + index * 668265263);
    h = (h ^ (h >> 13)) * 1274126177u;
    h = h ^ (h >> 16);
    return static_cast<float>(h & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

juce::Path MiMOLookAndFeel::createRoughLine(juce::Point<float> p1, juce::Point<float> p2,
                                            int seed, float amp, float overshoot)
{
    juce::ignoreUnused(seed, amp, overshoot);
    juce::Path path;
    path.startNewSubPath(p1);
    path.lineTo(p2);
    return path;
}

juce::Path MiMOLookAndFeel::createSketchRect(juce::Rectangle<float> bounds, int seed, float amp)
{
    juce::ignoreUnused(seed, amp);
    juce::Path p;
    p.addRoundedRectangle(bounds.reduced(1.0f), 6.0f);
    return p;
}

juce::Path MiMOLookAndFeel::createSketchEllipse(juce::Point<float> centre, float rx, float ry,
                                                int seed, float amp)
{
    juce::ignoreUnused(seed, amp);
    juce::Path p;
    p.addEllipse(centre.x - rx, centre.y - ry, rx * 2.0f, ry * 2.0f);
    return p;
}

juce::Path MiMOLookAndFeel::createScribbleFill(juce::Rectangle<float> bounds, int seed, float gap)
{
    juce::ignoreUnused(bounds, seed, gap);
    return {}; // Nessun tratto disordinato a mano
}

void MiMOLookAndFeel::drawPaperTexture(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Image& cachedTexture)
{
    if (!cachedTexture.isValid() || cachedTexture.getWidth() != bounds.getWidth() || cachedTexture.getHeight() != bounds.getHeight())
    {
        cachedTexture = juce::Image(juce::Image::RGB, std::max(10, bounds.getWidth()), std::max(10, bounds.getHeight()), false);
        juce::Graphics tg(cachedTexture);

        tg.fillAll(paperBg);

        const juce::Colour topCol = isDarkMode ? juce::Colour(0xff222631) : juce::Colour(0xfffaf6ee);
        const juce::Colour botCol = isDarkMode ? juce::Colour(0xff121418) : juce::Colour(0xffece3d0);

        juce::ColourGradient vignette(
            topCol, static_cast<float>(bounds.getCentreX()), static_cast<float>(bounds.getCentreY()),
            botCol, 0.0f, 0.0f, true
        );
        tg.setGradientFill(vignette);
        tg.fillRect(bounds);
    }

    g.drawImageAt(cachedTexture, 0, 0);
}

void MiMOLookAndFeel::drawSketchBox(juce::Graphics& g, juce::Rectangle<float> bounds, int seed,
                                    juce::Colour crayonFill, float fillOpacity,
                                    juce::Colour glowColour, float strokeWidth,
                                    bool dashed, juce::Colour basePaper)
{
    juce::ignoreUnused(seed);
    const float cornerRadius = 6.0f;
    const auto r = bounds.reduced(1.0f);

    // Alone esterno per evidenziazione / selezione
    if (!glowColour.isTransparent())
    {
        g.setColour(glowColour.withAlpha(0.40f));
        g.drawRoundedRectangle(r.expanded(2.0f), cornerRadius + 2.0f, strokeWidth + 2.0f);
    }

    // Fondo solido pulito
    if (!basePaper.isTransparent())
    {
        g.setColour(basePaper);
        g.fillRoundedRectangle(r, cornerRadius);
    }

    // Campitura a colore pieno e pulito (senza scarabocchi a zig-zag)
    if (!crayonFill.isTransparent() && fillOpacity > 0.01f)
    {
        g.setColour(crayonFill.withAlpha(std::min(1.0f, fillOpacity * 0.85f)));
        g.fillRoundedRectangle(r, cornerRadius);
    }

    // Bordo pulito e geometrico
    g.setColour(fountainInk);
    if (dashed)
    {
        juce::Path p;
        p.addRoundedRectangle(r, cornerRadius);
        juce::Path dashedPath;
        const float dashLengths[2] = { 6.0f, 6.0f };
        juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createDashedStroke(dashedPath, p, dashLengths, 2);
        g.fillPath(dashedPath);
    }
    else
    {
        g.drawRoundedRectangle(r, cornerRadius, strokeWidth);
    }
}

void MiMOLookAndFeel::drawCrayonHighlight(juce::Graphics& g, juce::Rectangle<float> bounds,
                                          juce::Colour colour, int seed)
{
    juce::ignoreUnused(seed);
    g.setColour(colour.withAlpha(0.65f));
    g.fillRoundedRectangle(bounds, 4.0f);
}

juce::Path MiMOLookAndFeel::createInkCablePath(juce::Point<float> start, juce::Point<float> end,
                                               const std::vector<juce::Point<float>>& freehandNorm,
                                               int seed, int boilTick, float amp,
                                               juce::Point<float> startNormal,
                                               juce::Point<float> endNormal)
{
    juce::ignoreUnused(freehandNorm);
    juce::Path path;
    path.startNewSubPath(start);

    const float dx = end.x - start.x;
    const float dy = end.y - start.y;
    const float dist = std::max(10.0f, std::hypot(dx, dy));

    // Determina le normali di uscita e di ingresso se non esplicitate
    juce::Point<float> n1 = startNormal;
    juce::Point<float> n2 = endNormal;

    if (n1.isOrigin() && n2.isOrigin())
    {
        if (std::abs(dy) > std::abs(dx) * 1.15f)
        {
            // Cavo prevalentemente verticale
            n1 = (dy > 0.0f) ? juce::Point<float>(0.0f, 1.0f) : juce::Point<float>(0.0f, -1.0f);
            n2 = (dy > 0.0f) ? juce::Point<float>(0.0f, -1.0f) : juce::Point<float>(0.0f, 1.0f);
        }
        else
        {
            // Cavo prevalentemente orizzontale
            n1 = (dx >= 0.0f) ? juce::Point<float>(1.0f, 0.0f) : juce::Point<float>(-1.0f, 0.0f);
            n2 = (dx >= 0.0f) ? juce::Point<float>(-1.0f, 0.0f) : juce::Point<float>(1.0f, 0.0f);
        }
    }
    else if (n1.isOrigin())
    {
        n1 = -n2;
    }
    else if (n2.isOrigin())
    {
        n2 = -n1;
    }

    const float k1 = juce::jlimit(30.0f, 140.0f, dist * 0.45f);
    const float k2 = juce::jlimit(30.0f, 140.0f, dist * 0.45f);

    const juce::Point<float> c1 = start + n1 * k1;
    const juce::Point<float> c2 = end + n2 * k2;

    // Linea viva con jittering organico controllato (pulsazione a 6 Hz lungo la campata)
    const int steps = juce::jlimit(12, 28, static_cast<int>(dist / 24.0f));

    for (int i = 1; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float mt = 1.0f - t;

        const float x = mt * mt * mt * start.x
                      + 3.0f * mt * mt * t * c1.x
                      + 3.0f * mt * t * t * c2.x
                      + t * t * t * end.x;
        const float y = mt * mt * mt * start.y
                      + 3.0f * mt * mt * t * c1.y
                      + 3.0f * mt * t * t * c2.y
                      + t * t * t * end.y;

        const float tx = 3.0f * mt * mt * (c1.x - start.x)
                       + 6.0f * mt * t * (c2.x - c1.x)
                       + 3.0f * t * t * (end.x - c2.x);
        const float ty = 3.0f * mt * mt * (c1.y - start.y)
                       + 6.0f * mt * t * (c2.y - c1.y)
                       + 3.0f * t * t * (end.y - c2.y);
        const float tlen = std::max(1.0f, std::hypot(tx, ty));
        const float nx = -ty / tlen;
        const float ny = tx / tlen;

        // Jittering organico: gli estremi restano fissi sui pin (env = 0),
        // mentre la linea vibra con micro-irregolarità da inchiostro vivo a 6 Hz
        const float env = std::sin(juce::MathConstants<float>::pi * t);
        const float boilJitter = (seededFloat(seed + boilTick * 41, i * 3) - 0.5f) * amp * 1.6f;
        const float off = boilJitter * env;

        path.lineTo(x + nx * off, y + ny * off);
    }

    return path;
}

void MiMOLookAndFeel::drawPinJack(juce::Graphics& g, juce::Point<float> pos, float radius,
                                  PinShapeType shapeType, juce::Colour colour,
                                  bool isHovered, bool isSelected,
                                  bool isValidDropTarget, bool isFiringPulse)
{
    float r = radius;

    // Alone verde/salvia se è un target valido durante il trascinamento del cavo
    if (isValidDropTarget)
    {
        g.setColour(crayonSage.withAlpha(0.40f));
        g.fillEllipse(pos.x - r - 6.0f, pos.y - r - 6.0f, (r + 6.0f) * 2.0f, (r + 6.0f) * 2.0f);
        g.setColour(crayonSage);
        g.drawEllipse(pos.x - r - 6.0f, pos.y - r - 6.0f, (r + 6.0f) * 2.0f, (r + 6.0f) * 2.0f, 1.8f);
    }
    // Alone rosso/ciliegia se è selezionato
    else if (isSelected)
    {
        g.setColour(crayonCherry.withAlpha(0.45f));
        g.fillEllipse(pos.x - r - 7.0f, pos.y - r - 7.0f, (r + 7.0f) * 2.0f, (r + 7.0f) * 2.0f);
        g.setColour(crayonCherry);
        g.drawEllipse(pos.x - r - 7.0f, pos.y - r - 7.0f, (r + 7.0f) * 2.0f, (r + 7.0f) * 2.0f, 2.0f);
    }
    // Glow di hover quando il mouse passa sopra
    else if (isHovered)
    {
        g.setColour(crayonOchre.withAlpha(0.50f));
        g.fillEllipse(pos.x - r - 5.0f, pos.y - r - 5.0f, (r + 5.0f) * 2.0f, (r + 5.0f) * 2.0f);
    }

    if (shapeType == PinShapeType::TriggerDiamond)
    {
        // -------------------------------------------------------------
        // FORMA: ROMBO / DIAMOND (TRIGGER & IMPULSI)
        // -------------------------------------------------------------
        if (isFiringPulse)
            r *= 1.35f;

        juce::Path diamond;
        diamond.startNewSubPath(pos.x, pos.y - r);
        diamond.lineTo(pos.x + r, pos.y);
        diamond.lineTo(pos.x, pos.y + r);
        diamond.lineTo(pos.x - r, pos.y);
        diamond.closeSubPath();

        // Flash luminoso se il trigger è scattato in questo momento
        if (isFiringPulse)
        {
            g.setColour(colour.withAlpha(0.60f));
            g.strokePath(diamond, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Base opaca scura/carta
        g.setColour(isDarkMode ? juce::Colour(0xFF1E2126) : juce::Colour(0xFFECE7DE));
        g.fillPath(diamond);

        // Riempimento interno colore arancio ambra
        g.setColour(colour);
        juce::Path innerDiamond;
        const float ir = r * 0.58f;
        innerDiamond.startNewSubPath(pos.x, pos.y - ir);
        innerDiamond.lineTo(pos.x + ir, pos.y);
        innerDiamond.lineTo(pos.x, pos.y + ir);
        innerDiamond.lineTo(pos.x - ir, pos.y);
        innerDiamond.closeSubPath();
        g.fillPath(innerDiamond);

        // Bordo stilografico scuro o evidenziato
        g.setColour(isHovered ? crayonOchre : (isSelected ? crayonCherry : fountainInk));
        g.strokePath(diamond, juce::PathStrokeType(isSelected ? 2.2f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (shapeType == PinShapeType::ModulationTri)
    {
        // -------------------------------------------------------------
        // FORMA: TRIANGOLO (MODULAZIONE KNOB / CV)
        // -------------------------------------------------------------
        juce::Path tri;
        tri.startNewSubPath(pos.x, pos.y - r * 1.15f);
        tri.lineTo(pos.x + r * 1.05f, pos.y + r * 0.75f);
        tri.lineTo(pos.x - r * 1.05f, pos.y + r * 0.75f);
        tri.closeSubPath();

        // Base opaca
        g.setColour(isDarkMode ? juce::Colour(0xFF1E2126) : juce::Colour(0xFFECE7DE));
        g.fillPath(tri);

        // Riempimento interno pastello crayon
        g.setColour(colour);
        juce::Path innerTri;
        const float ir = r * 0.55f;
        innerTri.startNewSubPath(pos.x, pos.y - ir * 1.15f);
        innerTri.lineTo(pos.x + ir * 1.05f, pos.y + ir * 0.75f);
        innerTri.lineTo(pos.x - ir * 1.05f, pos.y + ir * 0.75f);
        innerTri.closeSubPath();
        g.fillPath(innerTri);

        // Bordo stilografico
        g.setColour(isHovered ? crayonOchre : (isSelected ? crayonCherry : fountainInk));
        g.strokePath(tri, juce::PathStrokeType(isSelected ? 2.2f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else
    {
        // -------------------------------------------------------------
        // FORMA: CERCHIO (AUDIO STEREO)
        // -------------------------------------------------------------
        // Base presa jack
        if (isFiringPulse)
        {
            r *= 1.30f;
            g.setColour(colour.withAlpha(0.60f));
            g.drawEllipse(pos.x - r - 2.0f, pos.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 3.0f);
        }
        g.setColour(isDarkMode ? juce::Colour(0xFF1E2126) : juce::Colour(0xFFECE7DE));
        g.fillEllipse(pos.x - r, pos.y - r, r * 2.0f, r * 2.0f);

        // Contatto interno inchiostro
        g.setColour(colour);
        const float ir = r * 0.55f;
        g.fillEllipse(pos.x - ir, pos.y - ir, ir * 2.0f, ir * 2.0f);

        // Bordo esterno presa jack
        g.setColour(isHovered ? crayonOchre : (isSelected ? crayonCherry : fountainInk));
        g.drawEllipse(pos.x - r, pos.y - r, r * 2.0f, r * 2.0f, isSelected ? 2.2f : 1.6f);
    }
}

void MiMOLookAndFeel::drawInkCable(juce::Graphics& g, juce::Point<float> start, juce::Point<float> end,
                                   const std::vector<juce::Point<float>>& freehandNorm,
                                   int seed, int boilTick, bool isModulation,
                                   juce::Colour modColour, bool isHighlighted,
                                   bool isExposeCable,
                                   juce::Point<float> startNormal,
                                   juce::Point<float> endNormal,
                                   bool isTrigger,
                                   float pulseAlpha)
{
    const float amp = (isModulation || isTrigger) ? 2.3f : 1.7f;
    const auto d1 = createInkCablePath(start, end, freehandNorm, seed, boilTick, amp, startNormal, endNormal);

    if (isHighlighted)
    {
        g.setColour(crayonCherry.withAlpha(0.65f));
        g.strokePath(d1, juce::PathStrokeType(14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (isTrigger)
    {
        // Cavo Trigger: linea arancio fiamma con bagliore all'impulso
        const juce::Colour trigCol = juce::Colour(0xFFFF7A18);
        if (pulseAlpha > 0.05f)
        {
            g.setColour(trigCol.withAlpha(pulseAlpha * 0.70f));
            g.strokePath(d1, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour(trigCol.withAlpha(0.90f));
        g.strokePath(d1, juce::PathStrokeType(4.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Tratteggio stilografico per evidenziare il flusso a impulsi
        juce::Path dashed;
        const float dashLen[2] = { 8.0f, 6.0f };
        juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createDashedStroke(dashed, d1, dashLen, 2);
        g.setColour(fountainInk);
        g.fillPath(dashed);
    }
    else if (!isModulation && !isExposeCable)
    {
        // Cavo Audio: se possiede un colore timbrico caratteristico, disegna la guaina pastello con anima a inchiostro
        if (modColour != fountainInk && modColour != juce::Colours::transparentBlack && modColour != juce::Colour(0x00000000))
        {
            g.setColour(modColour.withAlpha(0.88f));
            g.strokePath(d1, juce::PathStrokeType(4.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            g.setColour(fountainInk);
            g.strokePath(d1, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            g.setColour(fountainInk);
            g.strokePath(d1, juce::PathStrokeType(2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }
    else
    {
        // Cavo di Modulazione o Esposizione: traccia a pastello + tratteggio
        g.setColour(modColour.withAlpha(0.85f));
        g.strokePath(d1, juce::PathStrokeType(5.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path dashed;
        const float dashLen[2] = { isExposeCable ? 3.0f : 7.0f, 5.0f };
        juce::PathStrokeType(1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)
            .createDashedStroke(dashed, d1, dashLen, 2);
        g.setColour(fountainInk);
        g.fillPath(dashed);
    }
}

void MiMOLookAndFeel::drawCrayonKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                                     float normValue, juce::Colour crayonColour,
                                     const juce::String& label, const juce::String& valueText,
                                     int seed, bool isModulated, juce::Colour modColour)
{
    const float knobDiameter = std::min(bounds.getWidth() - 8.0f, bounds.getHeight() - 28.0f);
    const float radius = std::max(14.0f, knobDiameter * 0.5f);
    const juce::Point<float> centre(bounds.getCentreX(), bounds.getY() + radius + 4.0f);

    const float startAngle = -0.75f * juce::MathConstants<float>::pi;
    const float endAngle   =  0.75f * juce::MathConstants<float>::pi;
    const float clamped    = juce::jlimit(0.0f, 1.0f, normValue);
    const float curAngle   = startAngle + clamped * (endAngle - startAngle);

    // 1. Anello se modulato
    if (isModulated)
    {
        g.setColour(modColour);
        g.drawEllipse(centre.x - radius - 3.0f, centre.y - radius - 3.0f, (radius + 3.0f) * 2.0f, (radius + 3.0f) * 2.0f, 2.0f);
    }

    // 2. Traccia di fondo a inchiostro
    juce::Path bgTrack;
    bgTrack.addCentredArc(centre.x, centre.y, radius - 1.0f, radius - 1.0f, 0.0f, startAngle, endAngle, true);
    g.setColour(fountainInk.withAlpha(0.25f));
    g.strokePath(bgTrack, juce::PathStrokeType(1.5f));

    // 3. Arco pastello attivo
    if (clamped > 0.01f)
    {
        juce::Path activeArc;
        activeArc.addCentredArc(centre.x, centre.y, radius - 1.5f, radius - 1.5f, 0.0f, startAngle, curAngle, true);
        g.setColour(crayonColour.withAlpha(0.88f));
        g.strokePath(activeArc, juce::PathStrokeType(6.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 4. Disco interno
    const float innerR = radius - 6.0f;
    g.setColour(paperCard);
    g.fillEllipse(centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f);

    g.setColour(fountainInk);
    g.drawEllipse(centre.x - innerR, centre.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.5f);

    // 5. Lancetta a inchiostro
    const juce::Point<float> p1(
        centre.x + std::sin(curAngle) * (innerR * 0.15f),
        centre.y - std::cos(curAngle) * (innerR * 0.15f)
    );
    const juce::Point<float> p2(
        centre.x + std::sin(curAngle) * (innerR - 2.0f),
        centre.y - std::cos(curAngle) * (innerR - 2.0f)
    );
    g.drawLine(p1.x, p1.y, p2.x, p2.y, 2.0f);

    // 6. Testi
    const int textY = static_cast<int>(centre.y + radius + 3.0f);
    g.setColour(fountainInk);
    g.setFont(juce::Font(12.5f, juce::Font::bold));
    g.drawFittedText(label, static_cast<int>(bounds.getX()), textY, static_cast<int>(bounds.getWidth()), 14,
                     juce::Justification::centred, 1);

    g.setColour(inkMuted);
    g.setFont(juce::Font(11.0f, juce::Font::plain));
    g.drawFittedText(valueText, static_cast<int>(bounds.getX()), textY + 13, static_cast<int>(bounds.getWidth()), 13,
                     juce::Justification::centred, 1);
}

void MiMOLookAndFeel::drawCameraIcon(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    const auto b = bounds.reduced(3.0f);
    juce::Rectangle<float> body(b.getX(), b.getY() + b.getHeight() * 0.22f, b.getWidth(), b.getHeight() * 0.72f);
    g.setColour(paperCard);
    g.fillRoundedRectangle(body, 3.0f);

    g.setColour(fountainInk);
    g.drawRoundedRectangle(body, 3.0f, 1.6f);

    juce::Path top;
    top.startNewSubPath(body.getX() + body.getWidth() * 0.28f, body.getY());
    top.lineTo(body.getX() + body.getWidth() * 0.36f, b.getY());
    top.lineTo(body.getX() + body.getWidth() * 0.64f, b.getY());
    top.lineTo(body.getX() + body.getWidth() * 0.72f, body.getY());
    g.strokePath(top, juce::PathStrokeType(1.6f));

    const float lr = std::min(body.getWidth(), body.getHeight()) * 0.28f;
    g.setColour(crayonCyan.withAlpha(0.65f));
    g.fillEllipse(body.getCentreX() - lr, body.getCentreY() - lr, lr * 2.0f, lr * 2.0f);
    g.setColour(fountainInk);
    g.drawEllipse(body.getCentreX() - lr, body.getCentreY() - lr, lr * 2.0f, lr * 2.0f, 1.5f);
}

void MiMOLookAndFeel::drawStudioIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    const auto b = bounds.reduced(3.0f);
    drawSketchBox(g, b, 801, accent, 0.55f, juce::Colours::transparentBlack, 1.5f);
    g.setColour(fountainInk);
    g.drawLine(b.getX() + 4.0f, b.getCentreY(), b.getRight() - 4.0f, b.getCentreY(), 1.4f);
}

void MiMOLookAndFeel::drawLiveIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    const auto b = bounds.reduced(3.0f);
    juce::Path bolt;
    bolt.startNewSubPath(b.getCentreX() + 2.0f, b.getY());
    bolt.lineTo(b.getX() + 3.0f, b.getCentreY() + 1.0f);
    bolt.lineTo(b.getCentreX() + 1.0f, b.getCentreY() + 1.0f);
    bolt.lineTo(b.getCentreX() - 2.0f, b.getBottom());
    bolt.lineTo(b.getRight() - 3.0f, b.getCentreY() - 1.0f);
    bolt.lineTo(b.getCentreX() - 1.0f, b.getCentreY() - 1.0f);
    bolt.closeSubPath();

    g.setColour(accent.withAlpha(0.8f));
    g.fillPath(bolt);
    g.setColour(fountainInk);
    g.strokePath(bolt, juce::PathStrokeType(1.5f));
}

void MiMOLookAndFeel::drawCommunityIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent)
{
    const auto b = bounds.reduced(3.0f);
    const float r = b.getWidth() * 0.22f;
    g.setColour(accent.withAlpha(0.75f));
    g.fillEllipse(b.getCentreX() - r, b.getY() + 1.0f, r * 2.0f, r * 2.0f);
    g.setColour(fountainInk);
    g.drawEllipse(b.getCentreX() - r, b.getY() + 1.0f, r * 2.0f, r * 2.0f, 1.5f);
    g.drawRoundedRectangle(b.getX() + 2.0f, b.getCentreY() + 1.0f, b.getWidth() - 4.0f, b.getHeight() * 0.42f, 4.0f, 1.5f);
}

void MiMOLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                       float sliderPosProportional, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider& slider)
{
    juce::ignoreUnused(rotaryStartAngle, rotaryEndAngle);
    const juce::Rectangle<float> bounds(static_cast<float>(x), static_cast<float>(y),
                                        static_cast<float>(width), static_cast<float>(height));
    drawCrayonKnob(g, bounds, sliderPosProportional, crayonOchre,
                   slider.getName(), slider.getTextFromValue(slider.getValue()),
                   slider.getName().hashCode());
}

void MiMOLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                           const juce::Colour& backgroundColour,
                                           bool shouldDrawButtonAsHighlighted,
                                           bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(1.5f);
    auto fill = backgroundColour.isTransparent() ? crayonOchre : backgroundColour;
    float alpha = shouldDrawButtonAsDown ? 0.85f : (shouldDrawButtonAsHighlighted ? 0.65f : 0.42f);
    drawSketchBox(g, bounds, button.getButtonText().hashCode(), fill, alpha,
                  juce::Colours::transparentBlack, 1.6f);
}

void MiMOLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                     bool shouldDrawButtonAsHighlighted,
                                     bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    g.setFont(juce::Font(13.5f, juce::Font::bold));
    g.setColour(fountainInk);
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(6, 2),
                     juce::Justification::centred, 1);
}

} // namespace mimo
