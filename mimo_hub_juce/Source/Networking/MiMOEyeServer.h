#pragma once

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_osc/juce_osc.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>

namespace mimo
{

struct HandLandmark
{
    float x = 0.5f;
    float y = 0.5f;
    float z = 0.0f;
};

struct HandTrackingState
{
    // Mano Primaria (DX o prima mano rilevata)
    bool hasHand = false;
    juce::String handedness = "Right"; // "Right" o "Left"
    std::array<HandLandmark, 21> landmarks {};
    float palmX = 0.5f;
    float palmY = 0.5f;
    float palmZ = 0.5f;
    float fist = 0.0f;     // 0.0 = mano aperta, 1.0 = pugno chiuso
    float handTilt = 0.5f; // 0.0 = inclinata a sinistra, 0.5 = verticale, 1.0 = inclinata a destra

    // Flessione individuale delle 5 dita (0.0 = esteso, 1.0 = piegato):
    // [0]=Pollice, [1]=Indice, [2]=Medio, [3]=Anulare, [4]=Mignolo
    std::array<float, 5> fingerFlex { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    // 4 Pinch indipendenti: [0]=Indice, [1]=Medio, [2]=Anulare, [3]=Mignolo
    std::array<float, 4> pinches { 0.0f, 0.0f, 0.0f, 0.0f };       // 0.0 = aperto, 1.0 = pinch stretto
    std::array<bool, 4>  pinchActive { false, false, false, false };
    std::array<bool, 4>  pinchTrigger { false, false, false, false }; // Onset trigger
    std::array<float, 4> pinchVelocity { 0.8f, 0.8f, 0.8f, 0.8f };

    // Mano Secondaria (SX quando presenti entrambe le mani contemporaneamente)
    bool hasSecondaryHand = false;
    juce::String secondaryHandedness = "Left";
    std::array<HandLandmark, 21> secondaryLandmarks {};
    float secondaryPalmX = 0.5f;
    float secondaryPalmY = 0.5f;
    float secondaryPalmZ = 0.5f;
    float secondaryFist = 0.0f;
    float secondaryHandTilt = 0.5f;
    std::array<float, 5> secondaryFingerFlex { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> secondaryPinches { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<bool, 4>  secondaryPinchActive { false, false, false, false };
    std::array<bool, 4>  secondaryPinchTrigger { false, false, false, false };
    std::array<float, 4> secondaryPinchVelocity { 0.8f, 0.8f, 0.8f, 0.8f };

    float fps = 0.0f;
    uint32_t frameCount = 0;
    juce::int64 lastTimestamp = 0;
};

/**
 * Server Micro-HTTP + WebSocket + Ricevitore OSC per MiMO Eye.
 * Gestisce l'elaborazione Zero-Install MediaPipe Hands a 21 landmark eseguita
 * sulla GPU dello smartphone (o browser) e trasmette le coordinate in tempo reale a MiMO Hub.
 */
class MiMOEyeServer : public juce::Thread,
                      public juce::OSCReceiver,
                      public juce::OSCReceiver::ListenerWithOSCAddress<juce::OSCReceiver::MessageLoopCallback>
{
public:
    explicit MiMOEyeServer(int httpPort = 8088, int oscPort = 8089);
    ~MiMOEyeServer() override;

    // Controllo ciclo di vita
    void startServer();
    void stopServer();

    // Stato di connessione
    bool isClientConnected() const;
    int getClientFps() const;
    juce::String getClientTypeDescription() const; // "Smartphone MediaPipe (60 FPS)", "Webcam PC", "In attesa"

    // Lettura istantanea thread-safe dello stato mano
    HandTrackingState getLatestState();

    // Consuma i trigger di pinch (azzerandoli dopo la lettura per non ripeterli)
    struct PinchEvent
    {
        int fingerIndex = -1; // 0=Indice, 1=Medio, 2=Anulare, 3=Mignolo
        float velocity = 0.8f;
        bool isSecondary = false; // false = Mano Primaria, true = Mano Secondaria
    };
    std::vector<PinchEvent> consumePinchTriggers();

    // Rilevamento IP locale Wi-Fi e Tunnel HTTPS per generazione QR Code
    static juce::String getLocalWifiIpAddress();
    juce::String getEyeUrl() const;
    juce::String getLocalEyeUrl() const;
    bool hasHttpsTunnel() const;
    void setPublicHttpsUrl(const juce::String& url);
    int getHttpPort() const { return httpPort; }

    // Invio fotogramma video (da Webcam PC o da Smartphone) al motore MediaPipe locale sul PC
    void pushVideoFrameForAnalysis(const juce::Image& frame, bool fromSmartphone);
    void updateFromLocalWebcam(const HandTrackingState& webcamState);

    // Impostazioni di dettaglio analisi mano sul PC
    // handFilter: 0 = Entrambe, 1 = Solo Mano Destra, 2 = Solo Mano Sinistra
    void setHandFilter(int filterMode) { handFilterMode.store(filterMode); }
    int getHandFilter() const { return handFilterMode.load(); }
    void setPinchTriggerEnabled(bool enabled) { pinchTriggerEnabled.store(enabled); }
    bool isSmartphoneStreaming() const;

    // Controllo esclusivo sorgente video: se true usa lo smartphone, se false la webcam PC
    void setSmartphoneExclusive(bool usePhone);
    bool isSmartphoneExclusive() const;

    // Callback per trigger audio immediato e ricezione frame video dallo smartphone
    std::function<void(int fingerIdx, float velocity, bool isSecondary)> onPinchTrigger;
    std::function<void(const juce::Image&)> onSmartphoneFrame;

    // Metodo di test manuale (per simulare o testare pinch da mouse/PC)
    void injectTestPinch(int fingerIdx, float velocity = 0.85f);
    void injectTestHandMove(float normX, float normY, float normZ);

    // Rileva se il motore MediaPipe locale o esterno sta inviando dati attivi
    bool isMediaPipeActive() const;

    // Ricezione pacchetti OSC (/mimo/eye/...)
    void oscMessageReceived(const juce::OSCMessage& message) override;

private:
    void run() override;

    // Gestione connessione HTTP / WebSocket
    void handleIncomingConnection(std::unique_ptr<juce::StreamingSocket> client);
    void handleHttpRequest(juce::StreamingSocket& client, const juce::String& requestHeader);
    void handleWebSocketSession(juce::StreamingSocket& client, const juce::String& wsKey);
    void parseIncomingHandJson(const juce::String& jsonText);

    // HTML5 minimale per Smartphone (solo Videocamera) e Worker MediaPipe locale su PC
    static juce::String getEmbeddedWebAppHtml(int httpPort);
    static juce::String getLocalPcMediaPipeWorkerHtml();

    int httpPort = 8088;
    int oscPort = 8089;

    std::unique_ptr<juce::StreamingSocket> serverSocket;
    juce::ThreadPool httpPool { 4 };

    juce::CriticalSection stateLock;
    HandTrackingState currentState;
    std::vector<PinchEvent> pendingPinchEvents;
    std::array<bool, 4> prevPinchActiveState { false, false, false, false };
    std::array<bool, 4> prevSecondaryPinchActiveState { false, false, false, false };

    juce::CriticalSection frameLock;
    juce::MemoryBlock latestFrameJpeg;
    std::atomic<uint32_t> latestFrameSeq { 0 };
    std::atomic<juce::int64> lastSmartphoneFrameMs { 0 };
    std::atomic<juce::int64> lastMediaPipeDataMs { 0 };
    std::atomic<bool> smartphoneExclusive { false };

    std::atomic<int> handFilterMode { 0 }; // 0 = Entrambe, 1 = Destra, 2 = Sinistra
    std::atomic<bool> pinchTriggerEnabled { true };

    std::atomic<bool> clientConnected { false };
    std::atomic<int> clientFps { 0 };
    std::atomic<int> connectionType { 0 }; // 0 = In attesa, 1 = Smartphone Cam, 2 = OSC, 3 = Sim, 4 = Webcam PC MediaPipe

    mutable juce::CriticalSection urlLock;
    juce::String publicHttpsUrl;

    // Worker per sessione attiva di streaming WebSocket
    class WebSocketSessionWorker;
    std::unique_ptr<WebSocketSessionWorker> activeSession;

    // Worker per Tunnel HTTPS automatico (per bypassare isolamento Wi-Fi e abilitare fotocamera su iOS/Android)
    class HttpsTunnelWorker;
    std::unique_ptr<HttpsTunnelWorker> tunnelWorker;

    // Worker MediaPipe HandLandmarker locale sul PC (analizza i frame della Webcam PC e dello Smartphone)
    class LocalMediaPipeWorker;
    std::unique_ptr<LocalMediaPipeWorker> mpWorker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiMOEyeServer)
};

} // namespace mimo
