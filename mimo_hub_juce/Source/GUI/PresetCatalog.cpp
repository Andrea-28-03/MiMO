#include "PresetCatalog.h"
#include "MiMOLookAndFeel.h"

namespace mimo
{

void PresetCatalog::initCatalogs(std::vector<CatalogItem>& instrumentCatalog,
                                 std::vector<CatalogItem>& effectCatalog,
                                 std::vector<CatalogItem>& utilityCatalog)
{
    instrumentCatalog = {
        { "ins-drum-analog",   "Percussioni",       "Percussioni",          "Faust", "4 voci discrete con sound design granulare", MiMOLookAndFeel::crayonCherry, false, {} },
        { "ins-theremin-3d",   "Sintesi Gestuale",  "Theremin 3D",          "Faust", "Controllo spaziale fluido",                  MiMOLookAndFeel::crayonCyan,   false, {} },
        { "ins-karplus-pluck", "Corde & Lamelle",   "Kalimba & Pluck",      "Faust", "Waveguide fisico a corda",                   MiMOLookAndFeel::crayonMint,   false, {} },
        { "ins-felt-piano",    "Tastiere & Feltro", "Piano Feltro",         "Faust", "Pianoforte intimo con feltro & decay naturale", MiMOLookAndFeel::crayonLilac, false, {} },
        { "ins-synth-va",      "Sintesi VA",        "Virtual Analog Synth", "Faust", "PolyBLEP lead & sub-bass",                  MiMOLookAndFeel::crayonPeach,  false, {} }
    };

    utilityCatalog = {
        { "ut-scale-lock",    "Armonia & Scale",  "Smart Scale Lock",  "Faust", "Quantizzatore tonale anti-stonatura", MiMOLookAndFeel::crayonMint,   false, {} },
        { "ut-looper-tape",   "Performance",      "Tape Looper",       "Faust", "1-Button Looper a nastro (Sound-on-Sound)", MiMOLookAndFeel::crayonCherry, false, {} },
        { "mod-env-follower", "Modulazione",      "Envelope Follower", "Faust", "Seguitore dinamico inviluppo audio", MiMOLookAndFeel::crayonOchre,  false, {} },
        { "mod-lfo-poly",     "Modulazione",      "LFO Multi-Onda",    "Faust", "Oscillatore CV continuo multi-forma", MiMOLookAndFeel::crayonPeach,  false, {} }
    };

    effectCatalog.clear();

    // Catalogo Effetti Standard V2 (Mix Dry/Wet e Volume su TUTTI gli effetti)
    {
        CatalogItem filtro;
        filtro.id = "fx-filtro-svf";
        filtro.category = "Filtri";
        filtro.name = "Filtro ZDF SVF";
        filtro.engine = "Faust";
        filtro.hint = "SVF risonante analogico";
        filtro.colour = MiMOLookAndFeel::crayonCyan;
        filtro.isEffect = true;
        filtro.templateBlocks = {
            { "b0", "SVF Filtro", "process = fi.resonlp(cutoff, q, 1);", MiMOLookAndFeel::crayonCyan, false, { 320.0f, 150.0f },
              { { "cutoff", "Cutoff", 20.0f, 18000.0f, 1800.0f, 1800.0f, " Hz", true },
                { "res",    "Risonanza", 0.1f, 10.0f, 1.4f, 1.4f, "", true } } },
            { "b1", "Mix & Vol", "process = _ <: *(1-mix), *(mix) :> _ * vol;", MiMOLookAndFeel::crayonSage, false, { 620.0f, 160.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 100.0f, 100.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(filtro);
    }

    {
        CatalogItem sat;
        sat.id = "fx-saturazione-soft";
        sat.category = "Colore";
        sat.name = "Saturazione Pastello";
        sat.engine = "Faust";
        sat.hint = "Overdrive a nastro morbido";
        sat.colour = MiMOLookAndFeel::crayonPeach;
        sat.isEffect = true;
        sat.templateBlocks = {
            { "b0", "Saturatore", "process = ef.cubicnl(drive, 0);", MiMOLookAndFeel::crayonPeach, false, { 320.0f, 180.0f },
              { { "drive", "Drive", 0.0f, 36.0f, 12.0f, 12.0f, " dB", true },
                { "tone",  "Tono",  400.0f, 16000.0f, 6500.0f, 6500.0f, " Hz", true } } },
            { "b1", "Mix & Vol", "process = _ * vol;", MiMOLookAndFeel::crayonSage, false, { 620.0f, 180.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 100.0f, 100.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(sat);
    }

    {
        CatalogItem delay;
        delay.id = "fx-eco-nastro";
        delay.category = "Spazio";
        delay.name = "Eco a Nastro Ping-Pong";
        delay.engine = "Faust";
        delay.hint = "Delay analogico con smorzamento";
        delay.colour = MiMOLookAndFeel::crayonMint;
        delay.isEffect = true;
        delay.templateBlocks = {
            { "b0", "Delay a Nastro", "process = +~(de.fdelay(48000, time) * fb);", MiMOLookAndFeel::crayonMint, false, { 320.0f, 170.0f },
              { { "time", "Tempo", 20.0f, 1200.0f, 380.0f, 380.0f, " ms", true },
                { "fb",   "Feedback", 0.0f, 95.0f, 45.0f, 45.0f, " %", true } } },
            { "b1", "Mix & Vol", "process = fi.lowpass(2, tone);", MiMOLookAndFeel::crayonOchre, false, { 620.0f, 170.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 35.0f, 35.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(delay);
    }

    {
        CatalogItem rev;
        rev.id = "fx-riverbero-fdn";
        rev.category = "Spazio";
        rev.name = "Riverbero Lush FDN";
        rev.engine = "Faust";
        rev.hint = "Ambiente diffuso denso";
        rev.colour = MiMOLookAndFeel::crayonLilac;
        rev.isEffect = true;
        rev.templateBlocks = {
            { "b0", "Ambiente", "process = re.mono_freeverb(room, 0.5, damp, 23);", MiMOLookAndFeel::crayonLilac, false, { 320.0f, 170.0f },
              { { "room", "Stanza", 0.0f, 100.0f, 75.0f, 75.0f, " %", true },
                { "damp", "Smorzam.", 0.0f, 100.0f, 40.0f, 40.0f, " %", true } } },
            { "b1", "Mix & Vol", "process = _ <: *(1-mix), *(mix) :> _;", MiMOLookAndFeel::crayonSage, false, { 620.0f, 180.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 35.0f, 35.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(rev);
    }

    {
        CatalogItem moog;
        moog.id = "fx-moog-ladder";
        moog.category = "Filtri";
        moog.name = "Moog Ladder 24dB";
        moog.engine = "Faust";
        moog.hint = "Filtro analogico a scala con transistor drive";
        moog.colour = MiMOLookAndFeel::crayonOchre;
        moog.isEffect = true;
        moog.templateBlocks = {
            { "b0", "Ladder", "process = moog_ladder(cutoff, res, drive);", MiMOLookAndFeel::crayonOchre, false, { 320.0f, 170.0f },
              { { "cutoff", "Cutoff", 20.0f, 18000.0f, 1200.0f, 1200.0f, " Hz", true },
                { "res",    "Risonanza", 0.0f, 100.0f, 45.0f, 45.0f, " %", true } } },
            { "b1", "Drive & Vol", "process = _ * vol;", MiMOLookAndFeel::crayonPeach, false, { 620.0f, 170.0f },
              { { "drive", "Drive", 0.0f, 24.0f, 6.0f, 6.0f, " dB", true },
                { "mix",   "Mix",   0.0f, 100.0f, 100.0f, 100.0f, " %", true },
                { "vol",   "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(moog);
    }

    {
        CatalogItem chorus;
        chorus.id = "fx-chorus-bbd";
        chorus.category = "Modulazione";
        chorus.name = "Chorus BBD Analogico";
        chorus.engine = "Faust";
        chorus.hint = "Stereo ensemble a ritardo variabile vintage";
        chorus.colour = MiMOLookAndFeel::crayonSage;
        chorus.isEffect = true;
        chorus.templateBlocks = {
            { "b0", "BBD Ensemble", "process = bbd_chorus(rate, depth);", MiMOLookAndFeel::crayonSage, false, { 320.0f, 170.0f },
              { { "rate",  "Velocita", 0.1f, 8.0f, 1.25f, 1.25f, " Hz", true },
                { "depth", "Profond.", 0.0f, 100.0f, 65.0f, 65.0f, " %", true } } },
            { "b1", "Mix & Vol", "process = _ * vol;", MiMOLookAndFeel::crayonMint, false, { 620.0f, 170.0f },
              { { "mix", "Mix", 0.0f, 100.0f, 50.0f, 50.0f, " %", true },
                { "vol", "Volume", 0.0f, 150.0f, 100.0f, 100.0f, " %", true } } }
        };
        effectCatalog.push_back(chorus);
    }
}

void PresetCatalog::initDefaultStudioGraph(std::vector<CanvasNode>& nodes,
                                          std::vector<CanvasCable>& cables,
                                          const std::vector<CatalogItem>& effectCatalog)
{
    nodes.clear();
    cables.clear();

    // 1. Base Centrale Rock Pi 4 (Fissa al centro del foglio)
    CanvasNode rockPi;
    rockPi.id = "rockpi";
    rockPi.type = NodeType::RockPiCentral;
    rockPi.label = "MiMO Core";
    rockPi.subtitle = "Rock Pi 4 Plus - Hub I2C + Audio";
    rockPi.pos = { -175.0f, -135.0f };
    rockPi.size = { 350.0f, 218.0f };
    rockPi.crayonColour = MiMOLookAndFeel::crayonOchre;
    nodes.push_back(rockPi);

    // 2. Modulo Periferico P1: Modulo Batteria (Posizionato in basso sotto al connettore P1)
    CanvasNode drum;
    drum.id = "hw-drum";
    drum.type = NodeType::HardwareModule;
    drum.label = "Modulo Batteria";
    drum.subtitle = "Sensori Piezo - I2C Ch1";
    drum.portBadge = "P1";
    drum.pos = { -280.0f, 175.0f };
    drum.size = { 255.0f, 215.0f };
    {
        NodeOutputGroup gTrig;
        gTrig.id = "drum_trig";
        gTrig.name = "Impulsi Trigger (Colpo)";
        gTrig.isExpanded = true;
        gTrig.items = {
            { "trigger:out", "Trigger Colpo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        NodeOutputGroup gMod;
        gMod.id = "drum_mod";
        gMod.name = "Modulazione & Dinamica";
        gMod.isExpanded = true;
        gMod.items = {
            { "mod:vel",   "Velocity",  MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" },
            { "mod:press", "Pressione", MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };
        drum.outputGroups = { gTrig, gMod };
        drum.size.y = CanvasGeometry::getNodeCalculatedHeight(drum);
    }
    nodes.push_back(drum);

    // 3. Modulo Periferico P2: Modulo Occhio (Posizionato in basso sotto al connettore P2)
    CanvasNode eye;
    eye.id = "hw-eye";
    eye.type = NodeType::HardwareModule;
    eye.isIntegratedCamera = true;
    eye.label = "Modulo Occhio";
    eye.subtitle = "Tracking Mano 3D - 60 FPS";
    eye.portBadge = "P2";
    eye.pos = { 35.0f, 175.0f };
    eye.size = { 255.0f, 380.0f };
    eye.crayonColour = MiMOLookAndFeel::crayonSage;
    {
        NodeOutputGroup grpDxRoot;
        grpDxRoot.id = "eye_dx";
        grpDxRoot.name = "Mano Destra (DX)";
        grpDxRoot.parentGroup = "";
        grpDxRoot.isExpanded = true;

        NodeOutputGroup grpDxPalm;
        grpDxPalm.id = "eye_dx_palm";
        grpDxPalm.name = "Palmo 3D";
        grpDxPalm.parentGroup = "eye_dx";
        grpDxPalm.isExpanded = true;
        grpDxPalm.items = {
            { "mod:dx_x",    "Posizione Orizz. X",  MiMOLookAndFeel::crayonCyan,   false, "(verso manopola)" },
            { "mod:dx_y",    "Altezza Vert. Y",   MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_z",    "Distanza / Prof. Z",     MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" },
            { "mod:dx_fist", "Apertura / Pugno",   MiMOLookAndFeel::crayonSage,   false, "(verso manopola)" },
            { "mod:dx_tilt", "Inclinazione Polso",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        NodeOutputGroup grpDxFingers;
        grpDxFingers.id = "eye_dx_fingers";
        grpDxFingers.name = "Dita & Falangi";
        grpDxFingers.parentGroup = "eye_dx";
        grpDxFingers.isExpanded = false;
        grpDxFingers.items = {
            { "mod:dx_thumb", "Chiusura Pollice",  MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_idx",   "Chiusura Indice",   MiMOLookAndFeel::crayonCyan,   false, "(verso manopola)" },
            { "mod:dx_mid",   "Chiusura Medio",    MiMOLookAndFeel::crayonSage,   false, "(verso manopola)" },
            { "mod:dx_rng",   "Chiusura Anulare",  MiMOLookAndFeel::crayonOchre,  false, "(verso manopola)" },
            { "mod:dx_pky",   "Chiusura Mignolo",  MiMOLookAndFeel::crayonCherry, false, "(verso manopola)" }
        };

        NodeOutputGroup grpDxPinches;
        grpDxPinches.id = "eye_dx_pinches";
        grpDxPinches.name = "Trigger Pinch";
        grpDxPinches.parentGroup = "eye_dx";
        grpDxPinches.isExpanded = false;
        grpDxPinches.items = {
            { "trigger:dx_pinch_idx", "Pinch Indice",  juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_mid", "Pinch Medio",   juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_rng", "Pinch Anulare", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:dx_pinch_pky", "Pinch Mignolo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        NodeOutputGroup grpSxRoot;
        grpSxRoot.id = "eye_sx";
        grpSxRoot.name = "Mano Sinistra (SX)";
        grpSxRoot.parentGroup = "";
        grpSxRoot.isExpanded = false;

        NodeOutputGroup grpSxPalm;
        grpSxPalm.id = "eye_sx_palm";
        grpSxPalm.name = "Palmo 3D";
        grpSxPalm.parentGroup = "eye_sx";
        grpSxPalm.isExpanded = true;
        grpSxPalm.items = {
            { "mod:sx_x",    "Posizione Orizz. X",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_y",    "Altezza Vert. Y",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_z",    "Distanza / Prof. Z",     MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_fist", "Apertura / Pugno",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_tilt", "Inclinazione Polso",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        NodeOutputGroup grpSxFingers;
        grpSxFingers.id = "eye_sx_fingers";
        grpSxFingers.name = "Dita & Falangi";
        grpSxFingers.parentGroup = "eye_sx";
        grpSxFingers.isExpanded = false;
        grpSxFingers.items = {
            { "mod:sx_thumb", "Chiusura Pollice",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_idx",   "Chiusura Indice",   MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_mid",   "Chiusura Medio",    MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_rng",   "Chiusura Anulare",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" },
            { "mod:sx_pky",   "Chiusura Mignolo",  MiMOLookAndFeel::crayonPeach,  false, "(verso manopola)" }
        };

        NodeOutputGroup grpSxPinches;
        grpSxPinches.id = "eye_sx_pinches";
        grpSxPinches.name = "Trigger Pinch";
        grpSxPinches.parentGroup = "eye_sx";
        grpSxPinches.isExpanded = false;
        grpSxPinches.items = {
            { "trigger:sx_pinch_idx", "Pinch Indice",  juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_mid", "Pinch Medio",   juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_rng", "Pinch Anulare", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" },
            { "trigger:sx_pinch_pky", "Pinch Mignolo", juce::Colour(0xFFFF7A18), true, "(verso TRIG strumento)" }
        };

        eye.outputGroups = {
            grpDxRoot, grpDxPalm, grpDxFingers, grpDxPinches,
            grpSxRoot, grpSxPalm, grpSxFingers, grpSxPinches
        };
        eye.size.y = CanvasGeometry::getNodeCalculatedHeight(eye);
    }
    nodes.push_back(eye);

    // 4. Effetto Iniziale Nested: Filtro Naif
    CanvasNode filtro;
    if (!effectCatalog.empty())
        filtro = CanvasGraphHelper::createFxNodeFromCatalog(effectCatalog[0], { 205.0f, -95.0f }, 1);
    nodes.push_back(filtro);

    // 5. Nodo Master Output
    CanvasNode master;
    master.id = "master";
    master.type = NodeType::MasterOut;
    master.label = "MASTER OUT";
    master.subtitle = "ASIO - 64 campioni";
    master.pos = { 525.0f, -80.0f };
    master.size = { 165.0f, 155.0f };
    master.crayonColour = MiMOLookAndFeel::crayonSage;
    master.directParams = {
        { "gain", "Master Vol", -48.0f, 6.0f, 0.0f, 0.0f, " dB", true }
    };
    nodes.push_back(master);

    // Cavi audio iniziali a penna stilografica
    cables.push_back({ "c-drum-pi", "hw-drum", "audio:out", "rockpi",   "audio:p1", false, MiMOLookAndFeel::fountainInk, {} });
    cables.push_back({ "c-eye-pi",  "hw-eye",  "audio:out", "rockpi",   "audio:p2", false, MiMOLookAndFeel::fountainInk, {} });
    cables.push_back({ "c-pi-fx",   "rockpi",  "audio:out", filtro.id,  "audio:in", false, MiMOLookAndFeel::fountainInk, {} });
    cables.push_back({ "c-fx-mst",  filtro.id, "audio:out", "master",   "audio:in", false, MiMOLookAndFeel::fountainInk, {} });

    for (auto& n : nodes)
    {
        n.targetPos = n.pos;
        n.isAnimatingPos = false;
    }
}

void PresetCatalog::initDefaultLivePresets(std::vector<PolaroidItem>& photoAlbum,
                                          PolaroidItem sceneSlots[5],
                                          const std::vector<CanvasNode>& nodes,
                                          const std::vector<CanvasCable>& cables)
{
    photoAlbum.clear();

    // Preset 1: Batte-Funk 70 & Tape (Hybrid)
    {
        PolaroidItem p;
        p.id = "pol-vintage-beat";
        p.autoName = "Batte-Funk 70 & Tape";
        p.title = "Batte-Funk 70 & Tape";
        p.timestamp = "14:20";
        p.engineType = "hybrid";
        p.bpm = 112;
        p.rotationDeg = -1.2f;
        p.badges = {
            { "Percussioni 808 (Faust)", MiMOLookAndFeel::crayonCherry },
            { "Tape Delay (Faust)", MiMOLookAndFeel::crayonCyan },
            { "Vel -> Drive", MiMOLookAndFeel::crayonOchre }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 2: Theremin Nuvola 3D (Faust)
    {
        PolaroidItem p;
        p.id = "pol-theremin-nuvola";
        p.autoName = "Theremin Nuvola 3D";
        p.title = "Theremin Nuvola 3D";
        p.timestamp = "14:24";
        p.engineType = "faust";
        p.bpm = 96;
        p.rotationDeg = 1.4f;
        p.badges = {
            { "Occhio AI 60 FPS", MiMOLookAndFeel::crayonCyan },
            { "Theremin Faust", MiMOLookAndFeel::crayonMint },
            { "Reverb 3D", MiMOLookAndFeel::crayonLilac }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 3: Piano Feltro Ambient (Faust)
    {
        PolaroidItem p;
        p.id = "pol-piano-feltro";
        p.autoName = "Piano Feltro Ambient";
        p.title = "Piano Feltro Ambient";
        p.timestamp = "14:28";
        p.engineType = "faust";
        p.bpm = 84;
        p.rotationDeg = -0.8f;
        p.badges = {
            { "Piano Feltro (Faust)", MiMOLookAndFeel::crayonLilac },
            { "Riverbero Carta", MiMOLookAndFeel::crayonCherry },
            { "Dist -> Damp", MiMOLookAndFeel::crayonSage }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 4: Percussioni Lofi & Fuzz (Faust)
    {
        PolaroidItem p;
        p.id = "pol-toy-drums";
        p.autoName = "Percussioni Lofi & Fuzz";
        p.title = "Percussioni Lofi & Fuzz";
        p.timestamp = "14:32";
        p.engineType = "faust";
        p.bpm = 126;
        p.rotationDeg = 1.1f;
        p.badges = {
            { "Percussioni Lofi (Faust)", MiMOLookAndFeel::crayonPeach },
            { "Filtro Naif SVF", MiMOLookAndFeel::crayonCyan }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 5: Pad Gessetto Granulare (Faust)
    {
        PolaroidItem p;
        p.id = "pol-pad-gessetto";
        p.autoName = "Pad Gessetto Granulare";
        p.title = "Pad Gessetto Granulare";
        p.timestamp = "14:35";
        p.engineType = "faust";
        p.bpm = 108;
        p.rotationDeg = -1.5f;
        p.badges = {
            { "Pad Gessetto (Faust)", MiMOLookAndFeel::crayonMint },
            { "Delay Spaziale", MiMOLookAndFeel::crayonCyan }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Preset 6: Acid Bass 303 & Sub (Faust)
    {
        PolaroidItem p;
        p.id = "pol-acid-bass";
        p.autoName = "Acid Bass 303 & Sub";
        p.title = "Acid Bass 303 & Sub";
        p.timestamp = "14:40";
        p.engineType = "faust";
        p.bpm = 128;
        p.rotationDeg = 0.9f;
        p.badges = {
            { "TB-303 Acid (Faust)", MiMOLookAndFeel::crayonCherry },
            { "Drive Analog", MiMOLookAndFeel::crayonPeach }
        };
        p.snapshot = { nodes, cables };
        p.isValid = true;
        photoAlbum.push_back(p);
    }

    // Pre-assegnazione dei primi 3 slot della Dock Hardware
    sceneSlots[0] = photoAlbum[0];
    sceneSlots[1] = photoAlbum[1];
    sceneSlots[2] = photoAlbum[2];
    sceneSlots[3].isValid = false;
    sceneSlots[4].isValid = false;
}

} // namespace mimo
