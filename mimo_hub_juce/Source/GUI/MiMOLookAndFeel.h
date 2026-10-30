#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>

namespace mimo
{

class MiMOLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MiMOLookAndFeel();
    ~MiMOLookAndFeel() override = default;

    // =========================================================================
    // Tavolozza Ufficiale "Sketchbook & Crayon Naif" (Dinamica Giorno / Notte)
    // =========================================================================
    static bool isDarkMode;
    static void setDarkMode(bool dark);

    static juce::Colour paperBg;      // Sfondo carta
    static juce::Colour paperCard;    // Cartoncino contenitori
    static juce::Colour polaroidWhite;// Cornice Polaroid
    static juce::Colour fountainInk;  // Inchiostro testo e bordi
    static juce::Colour inkMuted;     // Inchiostro secondario

    // Pastelli a Cera (Crayon Naif)
    static juce::Colour crayonCherry; // Rosso ciliegia
    static juce::Colour crayonOchre;  // Giallo ocra
    static juce::Colour crayonSage;   // Verde salvia
    static juce::Colour crayonCyan;   // Blu ciano
    static juce::Colour crayonLilac;  // Lilla
    static juce::Colour crayonPeach;  // Pesca
    static juce::Colour crayonMint;   // Menta

    // =========================================================================
    // Primitive di Disegno "Sketchbook & Crayon Naif" (Ottimizzate ad Alte Prestazioni)
    // =========================================================================
    static float seededFloat(int seed, int index);

    static juce::Path createRoughLine(juce::Point<float> p1, juce::Point<float> p2,
                                      int seed, float amp = 1.4f, float overshoot = 1.8f);

    static juce::Path createSketchRect(juce::Rectangle<float> bounds, int seed, float amp = 1.4f);

    static juce::Path createSketchEllipse(juce::Point<float> centre, float rx, float ry,
                                          int seed, float amp = 1.0f);

    static juce::Path createScribbleFill(juce::Rectangle<float> bounds, int seed, float gap = 8.0f);

    static void drawPaperTexture(juce::Graphics& g, juce::Rectangle<int> bounds, juce::Image& cachedTexture);

    static void drawSketchBox(juce::Graphics& g, juce::Rectangle<float> bounds, int seed,
                              juce::Colour crayonFill = juce::Colours::transparentBlack,
                              float fillOpacity = 0.55f,
                              juce::Colour glowColour = juce::Colours::transparentBlack,
                              float strokeWidth = 1.8f,
                              bool dashed = false,
                              juce::Colour basePaper = paperCard);

    static void drawCrayonHighlight(juce::Graphics& g, juce::Rectangle<float> bounds,
                                    juce::Colour colour, int seed);

    static juce::Path createInkCablePath(juce::Point<float> start, juce::Point<float> end,
                                         const std::vector<juce::Point<float>>& freehandNorm,
                                         int seed, int boilTick, float amp = 2.2f,
                                         juce::Point<float> startNormal = { 0.0f, 0.0f },
                                         juce::Point<float> endNormal = { 0.0f, 0.0f });

    static void drawInkCable(juce::Graphics& g, juce::Point<float> start, juce::Point<float> end,
                             const std::vector<juce::Point<float>>& freehandNorm,
                             int seed, int boilTick, bool isModulation,
                             juce::Colour modColour, bool isHighlighted = false,
                             bool isExposeCable = false,
                             juce::Point<float> startNormal = { 0.0f, 0.0f },
                             juce::Point<float> endNormal = { 0.0f, 0.0f });

    static void drawCrayonKnob(juce::Graphics& g, juce::Rectangle<float> bounds,
                               float normValue, juce::Colour crayonColour,
                               const juce::String& label, const juce::String& valueText,
                               int seed, bool isModulated = false,
                               juce::Colour modColour = crayonOchre);

    // Icone tracciate a mano libera
    static void drawCameraIcon(juce::Graphics& g, juce::Rectangle<float> bounds);
    static void drawStudioIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent);
    static void drawLiveIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent);
    static void drawCommunityIcon(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Colour accent);

    // Override standard JUCE LookAndFeel
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
};

} // namespace mimo
