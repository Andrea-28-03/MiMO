#include "MiMOEyeServer.h"
#include <cstring>

namespace mimo
{

// =============================================================================
// Algoritmo Standard RFC 3174 SHA-1 (per Handshake WebSocket RFC 6455)
// =============================================================================
namespace Sha1
{
    static inline void processBlock(uint32_t state[5], const uint8_t block[64])
    {
        uint32_t w[80];
        for (int i = 0; i < 16; ++i)
        {
            w[i] = (static_cast<uint32_t>(block[i * 4])     << 24)
                 | (static_cast<uint32_t>(block[i * 4 + 1]) << 16)
                 | (static_cast<uint32_t>(block[i * 4 + 2]) << 8)
                 | (static_cast<uint32_t>(block[i * 4 + 3]));
        }
        for (int i = 16; i < 80; ++i)
        {
            uint32_t val = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
            w[i] = (val << 1) | (val >> 31);
        }

        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

        for (int i = 0; i < 80; ++i)
        {
            uint32_t f = 0, k = 0;
            if (i < 20)      { f = (b & c) | ((~b) & d);          k = 0x5A827999; }
            else if (i < 40) { f = b ^ c ^ d;                     k = 0x6ED9EBA1; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d);   k = 0x8F1BBCDC; }
            else             { f = b ^ c ^ d;                     k = 0xCA62C1D6; }

            uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
            e = d;
            d = c;
            c = (b << 30) | (b >> 2);
            b = a;
            a = temp;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

    static inline std::array<uint8_t, 20> hash(const uint8_t* data, size_t len)
    {
        uint32_t state[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
        size_t offset = 0;
        while (len - offset >= 64)
        {
            processBlock(state, data + offset);
            offset += 64;
        }

        uint8_t pad[128] = { 0 };
        size_t rem = len - offset;
        if (rem > 0)
            std::memcpy(pad, data + offset, rem);
        pad[rem] = 0x80;
        size_t padLen = (rem < 56) ? 64 : 128;
        uint64_t bitLen = static_cast<uint64_t>(len) * 8;
        for (int i = 0; i < 8; ++i)
            pad[padLen - 1 - i] = static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF);

        processBlock(state, pad);
        if (padLen == 128)
            processBlock(state, pad + 64);

        std::array<uint8_t, 20> digest;
        for (int i = 0; i < 5; ++i)
        {
            digest[i * 4]     = static_cast<uint8_t>((state[i] >> 24) & 0xFF);
            digest[i * 4 + 1] = static_cast<uint8_t>((state[i] >> 16) & 0xFF);
            digest[i * 4 + 2] = static_cast<uint8_t>((state[i] >> 8)  & 0xFF);
            digest[i * 4 + 3] = static_cast<uint8_t>((state[i])       & 0xFF);
        }
        return digest;
    }

    static inline juce::String computeWebSocketAccept(const juce::String& clientKey)
    {
        const juce::String magicGuid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
        const juce::String combined = clientKey.trim() + magicGuid;
        const auto digest = hash(reinterpret_cast<const uint8_t*>(combined.toRawUTF8()), static_cast<size_t>(combined.getNumBytesAsUTF8()));
        return juce::Base64::toBase64(digest.data(), digest.size());
    }
} // namespace Sha1

// =============================================================================
// Worker Sessione Streaming WebSocket (riceve frame a 60 FPS in background)
// =============================================================================
class MiMOEyeServer::WebSocketSessionWorker : public juce::Thread
{
public:
    WebSocketSessionWorker(std::unique_ptr<juce::StreamingSocket> socketToUse, MiMOEyeServer& parentServer)
        : juce::Thread("MiMOWebSocketSession"),
          socket(std::move(socketToUse)),
          server(parentServer)
    {
        startThread();
    }

    ~WebSocketSessionWorker() override
    {
        signalThreadShouldExit();
        if (socket != nullptr)
            socket->close();
        stopThread(1000);
    }

    bool isConnected() const
    {
        return isRunning && socket != nullptr && socket->isConnected();
    }

    bool readExact(void* dest, size_t numBytes)
    {
        uint8_t* ptr = static_cast<uint8_t*>(dest);
        size_t total = 0;
        while (total < numBytes && !threadShouldExit() && socket != nullptr && socket->isConnected())
        {
            const int remaining = static_cast<int>(numBytes - total);
            const int r = socket->read(ptr + total, remaining, true);
            if (r <= 0)
                return false;
            total += static_cast<size_t>(r);
        }
        return total == numBytes;
    }

private:
    void run() override
    {
        isRunning = true;
        std::vector<uint8_t> buffer;
        buffer.reserve(32768);

        while (!threadShouldExit() && socket != nullptr && socket->isConnected())
        {
            const int ready = socket->waitUntilReady(true, 250);
            if (ready < 0)
                break;
            if (ready == 0)
                continue;

            // Leggi header del frame WebSocket (2 byte completi)
            uint8_t header[2];
            if (!readExact(header, 2))
                break;

            const uint8_t b0 = header[0];
            const uint8_t b1 = header[1];
            const uint8_t opcode = b0 & 0x0F;
            const bool isMasked = (b1 & 0x80) != 0;
            uint64_t payloadLen = b1 & 0x7F;

            // Opcode di chiusura
            if (opcode == 0x08)
                break;

            // Opcode di Ping -> invia Pong
            if (opcode == 0x09)
            {
                uint8_t pong[2] = { 0x8A, 0x00 };
                socket->write(pong, 2);
                continue;
            }

            // Calcola lunghezza estesa se presente
            if (payloadLen == 126)
            {
                uint8_t extLen[2];
                if (!readExact(extLen, 2))
                    break;
                payloadLen = (static_cast<uint64_t>(extLen[0]) << 8) | extLen[1];
            }
            else if (payloadLen == 127)
            {
                uint8_t extLen[8];
                if (!readExact(extLen, 8))
                    break;
                payloadLen = 0;
                for (int i = 0; i < 8; ++i)
                    payloadLen = (payloadLen << 8) | extLen[i];
            }

            // Chiave di mascheramento (4 byte dai client)
            uint8_t maskKey[4] = { 0 };
            if (isMasked)
            {
                if (!readExact(maskKey, 4))
                    break;
            }

            if (payloadLen > 1024 * 1024) // Limite di sicurezza (1MB)
                break;

            // Leggi il payload completo (gestisce frammentazione TCP su fotogrammi JPEG 25KB)
            buffer.resize(static_cast<size_t>(payloadLen));
            if (payloadLen > 0)
            {
                if (!readExact(buffer.data(), static_cast<size_t>(payloadLen)))
                    break;
            }

            // Smistamento / unmasking dei dati
            if (isMasked)
            {
                for (size_t i = 0; i < payloadLen; ++i)
                    buffer[i] ^= maskKey[i % 4];
            }

            // Se è un frame di testo (opcode 0x01) o continuazione, interpreta come JSON
            if (opcode == 0x01 || opcode == 0x00)
            {
                juce::String jsonStr = juce::String::fromUTF8(reinterpret_cast<const char*>(buffer.data()), static_cast<int>(payloadLen));
                server.parseIncomingHandJson(jsonStr);
            }
        }

        isRunning = false;
    }

    std::unique_ptr<juce::StreamingSocket> socket;
    MiMOEyeServer& server;
    std::atomic<bool> isRunning { false };
};

// =============================================================================
// Worker per Tunnel HTTPS Automatico (localhost.run via OpenSSH di sistema)
// =============================================================================
class MiMOEyeServer::HttpsTunnelWorker : public juce::Thread
{
public:
    HttpsTunnelWorker(MiMOEyeServer& ownerServer, int targetLocalPort)
        : juce::Thread("MiMOEyeHttpsTunnel"),
          server(ownerServer),
          localPort(targetLocalPort)
    {
        startThread();
    }

    ~HttpsTunnelWorker() override
    {
        signalThreadShouldExit();
        child.kill();
        stopThread(1200);
    }

private:
    void run() override
    {
        juce::StringArray cmd;
        cmd.add("ssh");
        cmd.add("-T");
        cmd.add("-o");
        cmd.add("StrictHostKeyChecking=no");
       #if JUCE_WINDOWS
        cmd.add("-o");
        cmd.add("UserKnownHostsFile=NUL");
       #else
        cmd.add("-o");
        cmd.add("UserKnownHostsFile=/dev/null");
       #endif
        cmd.add("-o");
        cmd.add("ServerAliveInterval=15");
        cmd.add("-o");
        cmd.add("ServerAliveCountMax=3");
        cmd.add("-R");
        cmd.add("80:127.0.0.1:" + juce::String(localPort));
        cmd.add("nokey@localhost.run");

        // FONDAMENTALE: catturare sia wantStdOut che wantStdErr! ssh scrive su STDERR!
        if (!child.start(cmd, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            return;

        juce::String accumulated;
        char buf[1024];

        while (!threadShouldExit() && child.isRunning())
        {
            const int n = child.readProcessOutput(buf, sizeof(buf) - 1);
            if (n > 0)
            {
                buf[n] = '\0';
                accumulated += juce::String::fromUTF8(buf, n);

                // Cerca un URL https://<id>.lhr.life
                int idx = accumulated.indexOf("https://");
                while (idx >= 0)
                {
                    int endIdx = idx + 8;
                    while (endIdx < accumulated.length())
                    {
                        const juce::juce_wchar ch = accumulated[endIdx];
                        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                            (ch >= '0' && ch <= '9') || ch == '.' || ch == '-')
                        {
                            ++endIdx;
                        }
                        else
                        {
                            break;
                        }
                    }

                    const juce::String candidate = accumulated.substring(idx, endIdx);
                    if (candidate.endsWithIgnoreCase(".lhr.life") && candidate.length() > 18)
                    {
                        server.setPublicHttpsUrl(candidate + "/eye");
                        break;
                    }

                    idx = accumulated.indexOf(endIdx, "https://");
                }

                if (accumulated.length() > 16384)
                    accumulated = accumulated.substring(accumulated.length() - 4096);
            }
            else
            {
                wait(40);
            }
        }
    }

    MiMOEyeServer& server;
    int localPort = 8088;
    juce::ChildProcess child;
};

// =============================================================================
// Helper Ricerca File Asset MediaPipe
// =============================================================================
static juce::File findMediaPipeAssetFile(const juce::String& fileName)
{
    const juce::Array<juce::File> searchDirs = {
        juce::File("C:/Users/Utente/AppData/Local/MiMOHub/Resources/mediapipe"),
        juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("Resources/mediapipe"),
        juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/mimo_hub_juce/Resources/mediapipe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("Resources/mediapipe"),
        juce::File::getCurrentWorkingDirectory().getChildFile("mimo_hub_juce/Resources/mediapipe"),
        juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("Resources/mediapipe"),
        juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getParentDirectory().getChildFile("Resources/mediapipe"),
        juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("mimo_mp_cache")
    };

    for (const auto& dir : searchDirs)
    {
        auto f = dir.getChildFile(fileName);
        if (f.existsAsFile())
            return f;
    }
    return {};
}

// =============================================================================
// Worker MediaPipe HandLandmarker Locale sul PC (Headless Engine)
// Analizza sul PC sia i fotogrammi della Webcam PC sia quelli dello Smartphone
// =============================================================================
class MiMOEyeServer::LocalMediaPipeWorker : public juce::Thread
{
public:
    LocalMediaPipeWorker(int targetLocalPort)
        : juce::Thread("MiMOLocalMediaPipeWorker"),
          localPort(targetLocalPort)
    {
        startThread();
    }

    ~LocalMediaPipeWorker() override
    {
        signalThreadShouldExit();
        child.kill();
       #if JUCE_WINDOWS
        juce::ChildProcess p;
        p.start("taskkill /F /FI \"WINDOWTITLE eq *mediapipe*\"");
       #endif
        stopThread(1200);
    }

private:
    void run() override
    {
        wait(250);
        if (threadShouldExit())
            return;

        // 1. Priorita Massima: Worker Python Nativo (Google MediaPipe 1.2+ CPU XNNPACK SIMD)
        const juce::Array<juce::File> pythonCandidates = {
            juce::File("C:/Users/Utente/AppData/Local/MiMOHub/eye_env/Scripts/python.exe"),
            juce::File("C:/Users/Utente/AppData/Local/Programs/Python/Python311/python.exe"),
            juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("eye_env/Scripts/python.exe"),
            juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/mimo_hub_juce/eye_env/Scripts/python.exe"),
            juce::File("H:/Il mio Drive/5. Programmazione MIMO/MiMO_Project/mimo_hub_juce/eye_env/Scripts/pythonw.exe"),
            juce::File::getCurrentWorkingDirectory().getChildFile("eye_env/Scripts/python.exe"),
            juce::File::getCurrentWorkingDirectory().getChildFile("eye_env/Scripts/pythonw.exe"),
            juce::File::getCurrentWorkingDirectory().getChildFile("mimo_hub_juce/eye_env/Scripts/python.exe"),
            juce::File::getCurrentWorkingDirectory().getChildFile("mimo_hub_juce/eye_env/Scripts/pythonw.exe"),
            juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("eye_env/Scripts/python.exe"),
            juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory().getChildFile("eye_env/Scripts/pythonw.exe"),
            juce::File("C:/Users/Utente/AppData/Local/Programs/Python/Python311/python.exe")
        };

        juce::File pyExe;
        for (const auto& f : pythonCandidates)
        {
            if (f.existsAsFile())
            {
                pyExe = f;
                break;
            }
        }

        const auto workerScript = findMediaPipeAssetFile("mediapipe_worker.py");

        if (pyExe.existsAsFile() && workerScript.existsAsFile())
        {
            juce::StringArray pyCmd;
            pyCmd.add(pyExe.getFullPathName());
            pyCmd.add(workerScript.getFullPathName());
            pyCmd.add(juce::String(localPort));

            child.start(pyCmd, 0);

            while (!threadShouldExit())
            {
                if (!child.isRunning())
                {
                    wait(2000);
                    if (!threadShouldExit())
                        child.start(pyCmd, 0);
                }
                wait(500);
            }

            child.kill();
            return;
        }

        // 2. Fallback Secondario: Headless Browser Engine (Edge / Chrome)
        juce::String browserExe;
        const char* candidates[] = {
            "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
            "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
            "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
            "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe"
        };
        for (const char* c : candidates)
        {
            if (juce::File(c).existsAsFile())
            {
                browserExe = c;
                break;
            }
        }

        if (browserExe.isEmpty())
            return;

        auto profileDir = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("mimo_mp_worker_profile_" + juce::String(localPort));
        profileDir.createDirectory();

        juce::StringArray cmd;
        cmd.add(browserExe);
        cmd.add("--headless=new");
        cmd.add("--remote-debugging-port=0");
        cmd.add("--disable-gpu-sandbox");
        cmd.add("--no-first-run");
        cmd.add("--no-default-browser-check");
        cmd.add("--disable-background-timer-throttling");
        cmd.add("--disable-backgrounding-occluded-windows");
        cmd.add("--disable-renderer-backgrounding");
        cmd.add("--disable-features=CalculateNativeWinOcclusion");
        cmd.add("--user-data-dir=" + profileDir.getFullPathName());
        cmd.add("http://127.0.0.1:" + juce::String(localPort) + "/pc_worker");

        child.start(cmd);

        while (!threadShouldExit())
        {
            if (!child.isRunning())
            {
                wait(3000);
                if (!threadShouldExit())
                    child.start(cmd);
            }
            wait(500);
        }

        child.kill();
    }

    int localPort = 8088;
    juce::ChildProcess child;
};

// =============================================================================
// Implementazione Principale MiMOEyeServer
// =============================================================================
MiMOEyeServer::MiMOEyeServer(int httpPortToUse, int oscPortToUse)
    : juce::Thread("MiMOEyeServerThread"),
      httpPort(httpPortToUse),
      oscPort(oscPortToUse)
{
    currentState.hasHand = false;
}

MiMOEyeServer::~MiMOEyeServer()
{
    stopServer();
}

void MiMOEyeServer::startServer()
{
    stopServer();

    connect(oscPort);
    juce::OSCReceiver::addListener(this, "/mimo/eye/hand");
    juce::OSCReceiver::addListener(this, "/mimo/eye/pinch");

    startThread();
}

void MiMOEyeServer::stopServer()
{
    signalThreadShouldExit();

    mpWorker.reset();
    tunnelWorker.reset();
    activeSession.reset();

    if (serverSocket != nullptr)
        serverSocket->close();

    httpPool.removeAllJobs(true, 1500);
    disconnect();
    stopThread(1500);
    serverSocket.reset();
    clientConnected.store(false);
}

bool MiMOEyeServer::isSmartphoneStreaming() const
{
    const juce::int64 lastMs = lastSmartphoneFrameMs.load();
    if (lastMs <= 0)
        return false;
    return (juce::Time::currentTimeMillis() - lastMs) < 3500;
}

bool MiMOEyeServer::isClientConnected() const
{
    return isSmartphoneStreaming() || clientConnected.load();
}

int MiMOEyeServer::getClientFps() const
{
    return clientFps.load();
}

juce::String MiMOEyeServer::getClientTypeDescription() const
{
    if (isSmartphoneStreaming())
        return "Videocamera Smartphone Connessa";

    if (!clientConnected.load())
        return "In attesa";

    const int t = connectionType.load();
    if (t == 1)
        return "Videocamera Smartphone";
    if (t == 2)
        return "Dispositivo OSC";
    if (t == 3)
        return "Simulatore Locale";
    if (t == 4)
        return "MediaPipe PC (21 Landmark)";
    return "Connesso";
}

HandTrackingState MiMOEyeServer::getLatestState()
{
    juce::ScopedLock sl(stateLock);
    return currentState;
}

std::vector<MiMOEyeServer::PinchEvent> MiMOEyeServer::consumePinchTriggers()
{
    juce::ScopedLock sl(stateLock);
    auto events = pendingPinchEvents;
    pendingPinchEvents.clear();
    for (auto& t : currentState.pinchTrigger)
        t = false;
    for (auto& t : currentState.secondaryPinchTrigger)
        t = false;
    return events;
}

juce::String MiMOEyeServer::getLocalWifiIpAddress()
{
    auto addresses = juce::IPAddress::getAllAddresses();

    for (const auto& ip : addresses)
    {
        auto s = ip.toString();
        if (s.startsWith("192.168.") && !s.startsWith("192.168.56."))
            return s;
    }

    for (const auto& ip : addresses)
    {
        auto s = ip.toString();
        if (s.startsWith("10.") || s.startsWith("172.") || s.startsWith("192.168."))
            return s;
    }

    for (const auto& ip : addresses)
    {
        auto s = ip.toString();
        if (s != "127.0.0.1" && s != "0.0.0.0" && !s.contains(":"))
            return s;
    }

    return "127.0.0.1";
}

juce::String MiMOEyeServer::getLocalEyeUrl() const
{
    return "http://" + getLocalWifiIpAddress() + ":" + juce::String(httpPort) + "/eye";
}

bool MiMOEyeServer::hasHttpsTunnel() const
{
    const juce::ScopedLock sl(urlLock);
    return publicHttpsUrl.isNotEmpty();
}

void MiMOEyeServer::setPublicHttpsUrl(const juce::String& url)
{
    const juce::ScopedLock sl(urlLock);
    publicHttpsUrl = url;
}

juce::String MiMOEyeServer::getEyeUrl() const
{
    {
        const juce::ScopedLock sl(urlLock);
        if (publicHttpsUrl.isNotEmpty())
            return publicHttpsUrl;
    }
    return getLocalEyeUrl();
}

void MiMOEyeServer::setSmartphoneExclusive(bool usePhone)
{
    smartphoneExclusive.store(usePhone);
}

bool MiMOEyeServer::isSmartphoneExclusive() const
{
    return smartphoneExclusive.load();
}

void MiMOEyeServer::pushVideoFrameForAnalysis(const juce::Image& frame, bool fromSmartphone)
{
    if (!frame.isValid())
        return;

    if (fromSmartphone)
    {
        lastSmartphoneFrameMs.store(juce::Time::currentTimeMillis());
        clientConnected.store(true);
        connectionType.store(1);

        // Se lo smartphone NON e la sorgente attiva scelta dall'utente, registra la presenza ma NON propagare il frame!
        if (!smartphoneExclusive.load())
            return;

        if (onSmartphoneFrame)
            onSmartphoneFrame(frame);
    }
    else
    {
        // Se lo smartphone e la sorgente attiva, ignora i frame della webcam PC per evitare qualsiasi sfarfallio!
        if (smartphoneExclusive.load())
            return;
    }

    juce::MemoryOutputStream mos;
    juce::JPEGImageFormat jpegFmt;
    jpegFmt.setQuality(0.75f);
    if (jpegFmt.writeImageToStream(frame, mos))
    {
        const juce::ScopedLock sl(frameLock);
        latestFrameJpeg.replaceAll(mos.getData(), mos.getDataSize());
        latestFrameSeq.fetch_add(1);
    }
}

bool MiMOEyeServer::isMediaPipeActive() const
{
    return (juce::Time::currentTimeMillis() - lastMediaPipeDataMs.load()) < 2500;
}

void MiMOEyeServer::updateFromLocalWebcam(const HandTrackingState& webcamState)
{
    if (isMediaPipeActive())
        return;

    juce::ScopedLock sl(stateLock);
    currentState = webcamState;
    currentState.lastTimestamp = juce::Time::currentTimeMillis();

    for (int i = 0; i < 4; ++i)
    {
        if (webcamState.pinchTrigger[static_cast<size_t>(i)])
        {
            pendingPinchEvents.push_back({ i, webcamState.pinchVelocity[static_cast<size_t>(i)], false });
            if (onPinchTrigger && pinchTriggerEnabled.load())
                onPinchTrigger(i, webcamState.pinchVelocity[static_cast<size_t>(i)], false);
        }
    }
}

void MiMOEyeServer::injectTestPinch(int fingerIdx, float velocity)
{
    if (fingerIdx < 0 || fingerIdx >= 4)
        return;

    {
        juce::ScopedLock sl(stateLock);
        currentState.hasHand = true;
        currentState.pinches[static_cast<size_t>(fingerIdx)] = 1.0f;
        currentState.pinchActive[static_cast<size_t>(fingerIdx)] = true;
        currentState.pinchTrigger[static_cast<size_t>(fingerIdx)] = true;
        currentState.pinchVelocity[static_cast<size_t>(fingerIdx)] = velocity;
        pendingPinchEvents.push_back({ fingerIdx, velocity, false });
        currentState.lastTimestamp = juce::Time::currentTimeMillis();
    }

    if (onPinchTrigger && pinchTriggerEnabled.load())
        onPinchTrigger(fingerIdx, velocity, false);
}

void MiMOEyeServer::injectTestHandMove(float normX, float normY, float normZ)
{
    juce::ScopedLock sl(stateLock);
    currentState.hasHand = true;
    currentState.palmX = juce::jlimit(0.0f, 1.0f, normX);
    currentState.palmY = juce::jlimit(0.0f, 1.0f, normY);
    currentState.palmZ = juce::jlimit(0.0f, 1.0f, normZ);
    currentState.lastTimestamp = juce::Time::currentTimeMillis();
}

void MiMOEyeServer::oscMessageReceived(const juce::OSCMessage& message)
{
    const auto address = message.getAddressPattern().toString();
    if (address == "/mimo/eye/hand")
    {
        if (message.size() >= 3)
        {
            float x = message[0].isFloat32() ? message[0].getFloat32() : 0.5f;
            float y = message[1].isFloat32() ? message[1].getFloat32() : 0.5f;
            float z = message[2].isFloat32() ? message[2].getFloat32() : 0.5f;
            {
                juce::ScopedLock sl(stateLock);
                currentState.hasHand = true;
                currentState.palmX = x;
                currentState.palmY = y;
                currentState.palmZ = z;
                clientConnected.store(true);
                connectionType.store(2);
                currentState.lastTimestamp = juce::Time::currentTimeMillis();
            }
        }
    }
    else if (address == "/mimo/eye/pinch")
    {
        if (message.size() >= 2)
        {
            int finger = message[0].isInt32() ? message[0].getInt32() : 0;
            float vel = message[1].isFloat32() ? message[1].getFloat32() : 0.8f;
            injectTestPinch(finger, vel);
        }
    }
}

void MiMOEyeServer::run()
{
    serverSocket = std::make_unique<juce::StreamingSocket>();
    if (!serverSocket->createListener(httpPort, ""))
    {
        httpPort = 8080;
        if (!serverSocket->createListener(httpPort, ""))
            return;
    }

    // 1. Avvia il Tunnel HTTPS automatico per lo Smartphone
    tunnelWorker = std::make_unique<HttpsTunnelWorker>(*this, httpPort);

    // 2. Avvia il motore Google MediaPipe HandLandmarker locale sul PC
    mpWorker = std::make_unique<LocalMediaPipeWorker>(httpPort);

    while (!threadShouldExit())
    {
        auto* rawClient = serverSocket->waitForNextConnection();
        if (rawClient == nullptr)
        {
            wait(10);
            continue;
        }

        httpPool.addJob([this, rawClient]()
        {
            handleIncomingConnection(std::unique_ptr<juce::StreamingSocket>(rawClient));
        });
    }
}

void MiMOEyeServer::handleIncomingConnection(std::unique_ptr<juce::StreamingSocket> client)
{
    if (client == nullptr)
        return;

    if (client->waitUntilReady(true, 2500) != 1)
    {
        client->close();
        return;
    }

    juce::MemoryBlock reqData;
    char buffer[8192];
    int headerEndPos = -1;
    int contentLength = 0;

    for (int attempt = 0; attempt < 25 && !threadShouldExit(); ++attempt)
    {
        if (attempt > 0 && client->waitUntilReady(true, 300) != 1)
            break;

        const int n = client->read(buffer, sizeof(buffer), false);
        if (n <= 0)
            break;

        reqData.append(buffer, static_cast<size_t>(n));
        const juce::String partial = juce::String::fromUTF8(static_cast<const char*>(reqData.getData()),
                                                            static_cast<int>(reqData.getSize()));
        headerEndPos = partial.indexOf("\r\n\r\n");
        if (headerEndPos >= 0)
        {
            auto lines = juce::StringArray::fromLines(partial.substring(0, headerEndPos));
            for (const auto& l : lines)
            {
                if (l.startsWithIgnoreCase("Content-Length:"))
                {
                    contentLength = l.fromFirstOccurrenceOf(":", false, false).trim().getIntValue();
                    break;
                }
            }
            break;
        }
    }

    if (reqData.getSize() == 0)
    {
        client->close();
        return;
    }

    if (headerEndPos >= 0 && contentLength > 0 && contentLength < 512 * 1024)
    {
        const juce::String currentStr = juce::String::fromUTF8(static_cast<const char*>(reqData.getData()),
                                                               static_cast<int>(reqData.getSize()));
        const int bodyStartByte = currentStr.substring(0, headerEndPos + 4).getNumBytesAsUTF8();
        while (static_cast<int>(reqData.getSize()) - bodyStartByte < contentLength && !threadShouldExit())
        {
            if (client->waitUntilReady(true, 450) != 1)
                break;
            const int n = client->read(buffer, sizeof(buffer), false);
            if (n <= 0)
                break;
            reqData.append(buffer, static_cast<size_t>(n));
        }
    }

    const juce::String request = juce::String::fromUTF8(static_cast<const char*>(reqData.getData()),
                                                        static_cast<int>(reqData.getSize()));

    if (request.containsIgnoreCase("Upgrade: websocket"))
    {
        juce::String wsKey;
        auto lines = juce::StringArray::fromLines(request);
        for (const auto& l : lines)
        {
            if (l.startsWithIgnoreCase("Sec-WebSocket-Key:"))
            {
                wsKey = l.fromFirstOccurrenceOf(":", false, false).trim();
                break;
            }
        }

        if (wsKey.isNotEmpty())
        {
            handleWebSocketSession(*client, wsKey);
            activeSession = std::make_unique<WebSocketSessionWorker>(std::move(client), *this);
            return;
        }
    }

    handleHttpRequest(*client, request);
}

static void writeAllToSocket(juce::StreamingSocket& client, const char* data, int totalBytes)
{
    int written = 0;
    while (written < totalBytes)
    {
        if (client.waitUntilReady(false, 1500) != 1)
            break;
        const int w = client.write(data + written, totalBytes - written);
        if (w <= 0)
            break;
        written += w;
    }
}


void MiMOEyeServer::handleHttpRequest(juce::StreamingSocket& client, const juce::String& request)
{
    const juce::String firstLine = request.upToFirstOccurrenceOf("\r\n", false, false);

    if (firstLine.startsWithIgnoreCase("OPTIONS "))
    {
        const char corsResp[] =
            "HTTP/1.1 204 No Content\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
            "Access-Control-Allow-Headers: Content-Type\r\n"
            "Connection: close\r\n\r\n";
        writeAllToSocket(client, corsResp, static_cast<int>(sizeof(corsResp) - 1));
    }
    else if (firstLine.containsIgnoreCase("GET /pc_worker"))
    {
        const juce::String html = getLocalPcMediaPipeWorkerHtml();
        juce::String response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/html; charset=utf-8\r\n"
                 << "Cache-Control: no-store\r\n"
                 << "Content-Length: " << html.getNumBytesAsUTF8() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << html;
        writeAllToSocket(client, response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
    }
    else if (firstLine.containsIgnoreCase("GET /mp/"))
    {
        juce::String pathPart = firstLine.fromFirstOccurrenceOf("GET /mp/", false, true)
                                         .upToFirstOccurrenceOf(" ", false, false)
                                         .upToFirstOccurrenceOf("?", false, false);
        auto assetFile = findMediaPipeAssetFile(pathPart);
        juce::MemoryBlock fileData;
        if (assetFile.existsAsFile() && assetFile.loadFileAsData(fileData))
        {
            juce::String mime = "application/octet-stream";
            if (pathPart.endsWithIgnoreCase(".wasm"))
                mime = "application/wasm";
            else if (pathPart.endsWithIgnoreCase(".js") || pathPart.endsWithIgnoreCase(".mjs"))
                mime = "application/javascript; charset=utf-8";

            juce::String hdr;
            hdr << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: " << mime << "\r\n"
                << "Content-Length: " << static_cast<int>(fileData.getSize()) << "\r\n"
                << "Access-Control-Allow-Origin: *\r\n"
                << "Connection: close\r\n\r\n";
            writeAllToSocket(client, hdr.toRawUTF8(), static_cast<int>(hdr.getNumBytesAsUTF8()));
            writeAllToSocket(client, static_cast<const char*>(fileData.getData()), static_cast<int>(fileData.getSize()));
        }
        else
        {
            const char nf[] = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
            writeAllToSocket(client, nf, static_cast<int>(sizeof(nf) - 1));
        }
    }
    else if (firstLine.containsIgnoreCase("GET /frame.jpg"))
    {
        juce::MemoryBlock jpegCopy;
        uint32_t seq = 0;
        {
            const juce::ScopedLock sl(frameLock);
            jpegCopy = latestFrameJpeg;
            seq = latestFrameSeq.load();
        }

        if (jpegCopy.getSize() > 0)
        {
            juce::String hdr;
            hdr << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: image/jpeg\r\n"
                << "Cache-Control: no-store\r\n"
                << "X-Frame-Seq: " << static_cast<int>(seq) << "\r\n"
                << "Access-Control-Allow-Origin: *\r\n"
                << "Access-Control-Expose-Headers: X-Frame-Seq\r\n"
                << "Content-Length: " << static_cast<int>(jpegCopy.getSize()) << "\r\n"
                << "Connection: close\r\n\r\n";
            writeAllToSocket(client, hdr.toRawUTF8(), static_cast<int>(hdr.getNumBytesAsUTF8()));
            writeAllToSocket(client, static_cast<const char*>(jpegCopy.getData()), static_cast<int>(jpegCopy.getSize()));
        }
        else
        {
            const char emptyResp[] = "HTTP/1.1 204 No Content\r\nAccess-Control-Allow-Origin: *\r\nConnection: close\r\n\r\n";
            writeAllToSocket(client, emptyResp, static_cast<int>(sizeof(emptyResp) - 1));
        }
    }
    else if (firstLine.containsIgnoreCase("GET /api/tunnel") || firstLine.containsIgnoreCase("GET /api/status"))
    {
        const juce::ScopedLock sl(urlLock);
        juce::DynamicObject::Ptr json = new juce::DynamicObject();
        json->setProperty("has_tunnel", publicHttpsUrl.isNotEmpty());
        json->setProperty("https_url", publicHttpsUrl);
        json->setProperty("local_url", getLocalEyeUrl());
        const juce::String body = juce::JSON::toString(juce::var(json));

        juce::String response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: application/json; charset=utf-8\r\n"
                 << "Access-Control-Allow-Origin: *\r\n"
                 << "Cache-Control: no-cache\r\n"
                 << "Content-Length: " << body.getNumBytesAsUTF8() << "\r\n"
                 << "Connection: close\r\n\r\n"
                 << body;
        writeAllToSocket(client, response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
    }
    else if (firstLine.containsIgnoreCase("GET /eye") || firstLine.containsIgnoreCase("GET / ") || firstLine.containsIgnoreCase("GET /index.html"))
    {
        const juce::String html = getEmbeddedWebAppHtml(httpPort);
        juce::String response;
        response << "HTTP/1.1 200 OK\r\n"
                 << "Content-Type: text/html; charset=utf-8\r\n"
                 << "Cache-Control: no-store, no-cache, must-revalidate\r\n"
                 << "Content-Length: " << html.getNumBytesAsUTF8() << "\r\n"
                 << "Connection: close\r\n"
                 << "Access-Control-Allow-Origin: *\r\n\r\n"
                 << html;

        writeAllToSocket(client, response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
        juce::Thread::sleep(8);
    }
    else if (firstLine.startsWithIgnoreCase("POST /data"))
    {
        const auto body = request.fromFirstOccurrenceOf("\r\n\r\n", false, false);
        parseIncomingHandJson(body);

        const char resp[] =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 2\r\n"
            "Connection: close\r\n"
            "Access-Control-Allow-Origin: *\r\n\r\nOK";
        writeAllToSocket(client, resp, static_cast<int>(sizeof(resp) - 1));
    }
    else
    {
        const char notFound[] =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Length: 9\r\n"
            "Connection: close\r\n\r\nNot Found";
        writeAllToSocket(client, notFound, static_cast<int>(sizeof(notFound) - 1));
    }

    client.close();
}

void MiMOEyeServer::handleWebSocketSession(juce::StreamingSocket& client, const juce::String& wsKey)
{
    const juce::String acceptVal = Sha1::computeWebSocketAccept(wsKey);

    juce::String response;
    response << "HTTP/1.1 101 Switching Protocols\r\n"
             << "Upgrade: websocket\r\n"
             << "Connection: Upgrade\r\n"
             << "Sec-WebSocket-Accept: " << acceptVal << "\r\n\r\n";

    writeAllToSocket(client, response.toRawUTF8(), static_cast<int>(response.getNumBytesAsUTF8()));
}

void MiMOEyeServer::parseIncomingHandJson(const juce::String& jsonText)
{
    auto varParsed = juce::JSON::parse(jsonText);
    if (!varParsed.isObject())
        return;

    auto* obj = varParsed.getDynamicObject();
    if (obj == nullptr)
        return;

    const juce::String msgType = obj->getProperty("type").toString();

    // 1. Ricezione Fotogramma Video dallo Smartphone (Smartphone come pura Videocamera!)
    if (msgType == "smartphone_frame")
    {
        const bool isActive = static_cast<bool>(obj->getProperty("active"));
        if (!isActive)
        {
            lastSmartphoneFrameMs.store(0);
            clientConnected.store(false);
            return;
        }

        const juce::String jpegDataUrl = obj->getProperty("jpeg").toString();
        const juce::String b64 = jpegDataUrl.contains(",")
                               ? jpegDataUrl.fromFirstOccurrenceOf(",", false, false)
                               : jpegDataUrl;
        if (b64.isNotEmpty())
        {
            juce::MemoryOutputStream decoded;
            if (juce::Base64::convertFromBase64(decoded, b64))
            {
                juce::JPEGImageFormat jpegFmt;
                juce::MemoryInputStream mis(decoded.getData(), decoded.getDataSize(), false);
                juce::Image img = jpegFmt.decodeImage(mis);
                if (img.isValid())
                    pushVideoFrameForAnalysis(img, true);
            }
        }
        return;
    }

    // 2. Ricezione Risultati Analisi MediaPipe HandLandmarker (dal worker PC o esterno)
    std::vector<PinchEvent> newPinchEvents;
    lastMediaPipeDataMs.store(juce::Time::currentTimeMillis());
    connectionType.store(4);

    {
        juce::ScopedLock sl(stateLock);

        if (obj->hasProperty("fps"))
        {
            currentState.fps = static_cast<float>(obj->getProperty("fps"));
            clientFps.store(static_cast<int>(currentState.fps + 0.5f));
        }

        // Supporto per array multi-mano ("hands"): Mano Destra (DX) e Mano Sinistra (SX)
        const juce::DynamicObject* chosenHand = nullptr;   // DX (Right)
        const juce::DynamicObject* chosenHand2 = nullptr;  // SX (Left)
        if (obj->hasProperty("hands") && obj->getProperty("hands").isArray())
        {
            auto* handsArr = obj->getProperty("hands").getArray();
            for (const auto& hVar : *handsArr)
            {
                if (auto* hObj = hVar.getDynamicObject())
                {
                    const juce::String hSide = hObj->getProperty("handedness").toString();
                    if (hSide.equalsIgnoreCase("Right"))
                    {
                        if (chosenHand == nullptr) chosenHand = hObj;
                    }
                    else if (hSide.equalsIgnoreCase("Left"))
                    {
                        if (chosenHand2 == nullptr) chosenHand2 = hObj;
                    }
                }
            }
            // Se nessuna associazione per nome ma c'è una mano rilevata
            if (chosenHand == nullptr && chosenHand2 == nullptr && !handsArr->isEmpty())
            {
                if (auto* firstH = (*handsArr)[0].getDynamicObject())
                {
                    const juce::String hSide = firstH->getProperty("handedness").toString();
                    if (hSide.equalsIgnoreCase("Left"))
                        chosenHand2 = firstH;
                    else
                        chosenHand = firstH;
                }
            }
        }
        else if (obj->hasProperty("landmarks"))
        {
            const juce::String hSide = obj->getProperty("handedness").toString();
            if (hSide.equalsIgnoreCase("Left"))
                chosenHand2 = obj;
            else
                chosenHand = obj;
        }

        if (chosenHand == nullptr && chosenHand2 == nullptr)
        {
            currentState.hasHand = false;
            currentState.hasSecondaryHand = false;
            currentState.lastTimestamp = juce::Time::currentTimeMillis();
            for (int i = 0; i < 4; ++i)
            {
                currentState.pinchActive[static_cast<size_t>(i)] = false;
                currentState.pinchTrigger[static_cast<size_t>(i)] = false;
                prevPinchActiveState[static_cast<size_t>(i)] = false;
                currentState.secondaryPinchActive[static_cast<size_t>(i)] = false;
                currentState.secondaryPinchTrigger[static_cast<size_t>(i)] = false;
                prevSecondaryPinchActiveState[static_cast<size_t>(i)] = false;
            }
            return;
        }

        // =====================================================================
        // MANO PRIMARIA (DX)
        // =====================================================================
        if (chosenHand != nullptr)
        {
            currentState.hasHand = true;
            if (chosenHand->hasProperty("handedness"))
                currentState.handedness = chosenHand->getProperty("handedness").toString();
            else
                currentState.handedness = "Right";

        if (chosenHand->hasProperty("x"))
            currentState.palmX = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand->getProperty("x")));
        if (chosenHand->hasProperty("y"))
            currentState.palmY = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand->getProperty("y")));
        if (chosenHand->hasProperty("z"))
            currentState.palmZ = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand->getProperty("z")));
        if (chosenHand->hasProperty("fist"))
            currentState.fist = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand->getProperty("fist")));
        if (chosenHand->hasProperty("tilt"))
            currentState.handTilt = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand->getProperty("tilt")));

        // Flessione individuale delle 5 dita (3 falangi per dito)
        auto flexVar = chosenHand->getProperty("fingerFlex");
        if (flexVar.isArray())
        {
            auto* fArr = flexVar.getArray();
            for (int i = 0; i < std::min(5, fArr->size()); ++i)
                currentState.fingerFlex[static_cast<size_t>(i)] = juce::jlimit(0.0f, 1.0f, static_cast<float>((*fArr)[i]));
        }

        // Lettura 21 Landmarks
        auto landmarksVar = chosenHand->getProperty("landmarks");
        if (landmarksVar.isArray())
        {
            auto* arr = landmarksVar.getArray();
            const int count = std::min(21, arr->size());
            for (int i = 0; i < count; ++i)
            {
                const auto& ptVar = (*arr)[i];
                if (ptVar.isArray() && ptVar.getArray()->size() >= 2)
                {
                    currentState.landmarks[static_cast<size_t>(i)].x = static_cast<float>((*ptVar.getArray())[0]);
                    currentState.landmarks[static_cast<size_t>(i)].y = static_cast<float>((*ptVar.getArray())[1]);
                    if (ptVar.getArray()->size() >= 3)
                        currentState.landmarks[static_cast<size_t>(i)].z = static_cast<float>((*ptVar.getArray())[2]);
                }
            }
        }

        // Lettura distanze pinch e rilevamento onset trigger con isteresi
        auto pinchesVar = chosenHand->getProperty("pinches");
        if (pinchesVar.isArray())
        {
            auto* pArr = pinchesVar.getArray();
            for (int i = 0; i < std::min(4, pArr->size()); ++i)
            {
                const float pVal = juce::jlimit(0.0f, 1.0f, static_cast<float>((*pArr)[i]));
                currentState.pinches[static_cast<size_t>(i)] = pVal;

                const bool wasAct = prevPinchActiveState[static_cast<size_t>(i)];
                const bool nowAct = wasAct ? (pVal > 0.48f) : (pVal > 0.68f);
                currentState.pinchActive[static_cast<size_t>(i)] = nowAct;

                if (nowAct && !wasAct && pinchTriggerEnabled.load())
                {
                    currentState.pinchTrigger[static_cast<size_t>(i)] = true;
                    currentState.pinchVelocity[static_cast<size_t>(i)] = 0.88f;
                    pendingPinchEvents.push_back({ i, 0.88f, false });
                    newPinchEvents.push_back({ i, 0.88f, false });
                }
                prevPinchActiveState[static_cast<size_t>(i)] = nowAct;
            }
        }

        }
        else
        {
            currentState.hasHand = false;
        }

        // =====================================================================
        // MANO SECONDARIA (SX)
        // =====================================================================
        if (chosenHand2 != nullptr)
        {
            currentState.hasSecondaryHand = true;
            if (chosenHand2->hasProperty("handedness"))
                currentState.secondaryHandedness = chosenHand2->getProperty("handedness").toString();
            else
                currentState.secondaryHandedness = "Left";

            if (chosenHand2->hasProperty("x"))
                currentState.secondaryPalmX = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand2->getProperty("x")));
            if (chosenHand2->hasProperty("y"))
                currentState.secondaryPalmY = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand2->getProperty("y")));
            if (chosenHand2->hasProperty("z"))
                currentState.secondaryPalmZ = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand2->getProperty("z")));
            if (chosenHand2->hasProperty("fist"))
                currentState.secondaryFist = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand2->getProperty("fist")));
            if (chosenHand2->hasProperty("tilt"))
                currentState.secondaryHandTilt = juce::jlimit(0.0f, 1.0f, static_cast<float>(chosenHand2->getProperty("tilt")));

            auto flex2Var = chosenHand2->getProperty("fingerFlex");
            if (flex2Var.isArray())
            {
                auto* fArr = flex2Var.getArray();
                for (int i = 0; i < std::min(5, fArr->size()); ++i)
                    currentState.secondaryFingerFlex[static_cast<size_t>(i)] = juce::jlimit(0.0f, 1.0f, static_cast<float>((*fArr)[i]));
            }

            auto lm2Var = chosenHand2->getProperty("landmarks");
            if (lm2Var.isArray())
            {
                auto* arr = lm2Var.getArray();
                const int count = std::min(21, arr->size());
                for (int i = 0; i < count; ++i)
                {
                    const auto& ptVar = (*arr)[i];
                    if (ptVar.isArray() && ptVar.getArray()->size() >= 2)
                    {
                        currentState.secondaryLandmarks[static_cast<size_t>(i)].x = static_cast<float>((*ptVar.getArray())[0]);
                        currentState.secondaryLandmarks[static_cast<size_t>(i)].y = static_cast<float>((*ptVar.getArray())[1]);
                        if (ptVar.getArray()->size() >= 3)
                            currentState.secondaryLandmarks[static_cast<size_t>(i)].z = static_cast<float>((*ptVar.getArray())[2]);
                    }
                }
            }

            auto pinches2Var = chosenHand2->getProperty("pinches");
            if (pinches2Var.isArray())
            {
                auto* pArr = pinches2Var.getArray();
                for (int i = 0; i < std::min(4, pArr->size()); ++i)
                {
                    const float pVal = juce::jlimit(0.0f, 1.0f, static_cast<float>((*pArr)[i]));
                    currentState.secondaryPinches[static_cast<size_t>(i)] = pVal;

                    const bool wasAct = prevSecondaryPinchActiveState[static_cast<size_t>(i)];
                    const bool nowAct = wasAct ? (pVal > 0.48f) : (pVal > 0.68f);
                    currentState.secondaryPinchActive[static_cast<size_t>(i)] = nowAct;

                    if (nowAct && !wasAct && pinchTriggerEnabled.load())
                    {
                        currentState.secondaryPinchTrigger[static_cast<size_t>(i)] = true;
                        currentState.secondaryPinchVelocity[static_cast<size_t>(i)] = 0.88f;
                        pendingPinchEvents.push_back({ i, 0.88f, true });
                        newPinchEvents.push_back({ i, 0.88f, true });
                    }
                    prevSecondaryPinchActiveState[static_cast<size_t>(i)] = nowAct;
                }
            }

            if (!currentState.hasHand)
            {
                currentState.hasHand = true;
                currentState.handedness = "Left";
            }
        }
        else
        {
            currentState.hasSecondaryHand = false;
            for (int i = 0; i < 4; ++i)
            {
                currentState.secondaryPinchActive[static_cast<size_t>(i)] = false;
                currentState.secondaryPinchTrigger[static_cast<size_t>(i)] = false;
                prevSecondaryPinchActiveState[static_cast<size_t>(i)] = false;
            }
        }

        currentState.frameCount++;
        currentState.lastTimestamp = juce::Time::currentTimeMillis();
    }

    if (onPinchTrigger && pinchTriggerEnabled.load())
    {
        for (const auto& ev : newPinchEvents)
            onPinchTrigger(ev.fingerIndex, ev.velocity, ev.isSecondary);
    }
}

// =============================================================================
// Worker HTML/JS eseguito in background sul PC (Google MediaPipe HandLandmarker)
// =============================================================================
juce::String MiMOEyeServer::getLocalPcMediaPipeWorkerHtml()
{
    juce::String s;
    s << R"HTML(<!DOCTYPE html>
<html>
<head><meta charset="utf-8"><title>MiMO PC MediaPipe Engine</title></head>
<body>
<script type="module">
import { HandLandmarker, FilesetResolver } from "/mp/vision_bundle.mjs";

function dist3D(a, b) {
    const dx = a.x - b.x;
    const dy = a.y - b.y;
    const dz = (a.z || 0) - (b.z || 0);
    return Math.sqrt(dx * dx + dy * dy + dz * dz);
}

(async () => {
    let hl = null;
    try {
        const vision = await FilesetResolver.forVisionTasks("/mp");
        hl = await HandLandmarker.createFromOptions(vision, {
            baseOptions: {
                modelAssetPath: "/mp/hand_landmarker.task",
                delegate: "CPU"
            },
            runningMode: "IMAGE",
            numHands: 2,
            minHandDetectionConfidence: 0.35,
            minHandPresenceConfidence: 0.35,
            minTrackingConfidence: 0.35
        });
    } catch (e) {
        console.error("Errore init HandLandmarker:", e);
        return;
    }

    let lastSeq = -1;
    let frameCount = 0;
    let lastFpsTime = performance.now();
    let currentFps = 30;

    while (true) {
        try {
            const resp = await fetch("/frame.jpg", { cache: "no-store" });
            if (resp.status === 200) {
                const seqHeader = resp.headers.get("X-Frame-Seq");
                const seq = (seqHeader !== null && seqHeader !== "") ? parseInt(seqHeader, 10) : (lastSeq + 1);
                if (seq !== lastSeq) {
                    lastSeq = seq;
                    const blob = await resp.blob();
                    const bmp = await createImageBitmap(blob);
                    const res = hl.detect(bmp);
                    bmp.close();

                    frameCount++;
                    const now = performance.now();
                    if (now - lastFpsTime >= 1000) {
                        currentFps = Math.round((frameCount * 1000) / (now - lastFpsTime));
                        frameCount = 0;
                        lastFpsTime = now;
                    }

                    const handsOut = [];
                    if (res.landmarks && res.landmarks.length > 0) {
                        for (let h = 0; h < res.landmarks.length; h++) {
                            const lm = res.landmarks[h];
                            let side = "Right";
                            if (res.handednesses && res.handednesses[h] && res.handednesses[h][0]) {
                                // L'immagine e gia specchiata (mirror selfie): MediaPipe inverte Left/Right sulle immagini specchiate
                                const rawLabel = res.handednesses[h][0].categoryName;
                                side = (rawLabel === "Left") ? "Right" : "Left";
                            }

                            const wrist = lm[0];
                            const midMcp = lm[9];
                            const palmScale = Math.max(0.04, dist3D(wrist, midMcp));
                            const palmX = Math.max(0, Math.min(1, (wrist.x + midMcp.x) * 0.5));
                            const palmY = Math.max(0, Math.min(1, 1.0 - (wrist.y + midMcp.y) * 0.5));
                            const palmZ = Math.max(0, Math.min(1, (palmScale - 0.08) / 0.22));

                            // Angolo rotazione palmo (0.0 = sinistra, 0.5 = verticale, 1.0 = destra)
                            const ang = Math.atan2(midMcp.x - wrist.x, wrist.y - midMcp.y);
                            const tilt = Math.max(0, Math.min(1, 0.5 + (ang / 1.4)));

                            // Flessione individuale delle 5 dita (3 falangi ciascuna): [Pollice, Indice, Medio, Anulare, Mignolo]
                            const tipIds = [4, 8, 12, 16, 20];
                            const mcpIds = [2, 5, 9, 13, 17];
                            const fingerFlex = [];
                            for (let f = 0; f < 5; f++) {
                                const dTip = dist3D(wrist, lm[tipIds[f]]);
                                const dMcp = Math.max(0.03, dist3D(wrist, lm[mcpIds[f]]));
                                const ratio = dTip / dMcp;
                                const flex = (f === 0)
                                    ? Math.max(0, Math.min(1, (1.55 - ratio) / 0.55))
                                    : Math.max(0, Math.min(1, (1.88 - ratio) / 0.90));
                                fingerFlex.push(flex);
                            }

                            const fist = (fingerFlex[1] + fingerFlex[2] + fingerFlex[3] + fingerFlex[4]) * 0.25;

                            // 4 Pinch tra punta pollice [4] e punte [8, 12, 16, 20]
                            const thumbTip = lm[4];
                            const pinches = [8, 12, 16, 20].map(tIdx => {
                                const d = dist3D(thumbTip, lm[tIdx]) / palmScale;
                                return Math.max(0, Math.min(1, (0.45 - d) / 0.24));
                            });

                            handsOut.push({
                                handedness: side,
                                x: palmX,
                                y: palmY,
                                z: palmZ,
                                fist: fist,
                                tilt: tilt,
                                fingerFlex: fingerFlex,
                                pinches: pinches,
                                landmarks: lm.map(p => [p.x, p.y, p.z || 0])
                            });
                        }
                    }

                    await fetch("/data", {
                        method: "POST",
                        headers: { "Content-Type": "application/json" },
                        body: JSON.stringify({ fps: currentFps, hands: handsOut })
                    });
                } else {
                    await new Promise(r => setTimeout(r, 16));
                }
            } else {
                await new Promise(r => setTimeout(r, 40));
            }
        } catch (err) {
            await new Promise(r => setTimeout(r, 80));
        }
    }
})();
</script>
</body>
</html>)HTML";
    return s;
}

// =============================================================================
// Web App HTML5 Minimale per Smartphone (Solo Videocamera Remota -> Analisi su PC)
// Ottimizzata specificamente per Samsung Galaxy S22 Ultra e tutti gli smartphone Android/iOS
// =============================================================================
juce::String MiMOEyeServer::getEmbeddedWebAppHtml(int httpPortNumber)
{
    juce::ignoreUnused(httpPortNumber);
    juce::String s;
    s << R"HTML(<!DOCTYPE html>
<html lang="it">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, viewport-fit=cover">
    <title>MiMO Eye Camera</title>
    <style>
        :root {
            --bg-dark: #080c14;
            --card-bg: #121824;
            --card-border: #232d40;
            --cyan-pastel: #67e8f9;
            --sage-pastel: #86efac;
            --peach-pastel: #fca5a5;
            --ochre-pastel: #fde047;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
            user-select: none;
            -webkit-user-select: none;
            -webkit-tap-highlight-color: transparent;
        }
        html, body {
            width: 100vw;
            height: 100dvh;
            min-height: -webkit-fill-available;
            max-height: 100dvh;
            overflow: hidden;
            position: fixed;
            inset: 0;
            background-color: var(--bg-dark);
            color: var(--text-main);
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif;
            display: flex;
            flex-direction: column;
            justify-content: space-between;
            padding-top: max(10px, env(safe-area-inset-top));
            padding-bottom: max(14px, env(safe-area-inset-bottom));
            padding-left: max(14px, env(safe-area-inset-left));
            padding-right: max(14px, env(safe-area-inset-right));
        }
        header {
            display: flex;
            align-items: center;
            justify-content: space-between;
            padding: 12px 16px;
            background: rgba(18, 24, 36, 0.94);
            backdrop-filter: blur(14px);
            -webkit-backdrop-filter: blur(14px);
            border-radius: 18px;
            border: 1px solid var(--card-border);
            z-index: 20;
            margin-bottom: 10px;
        }
        .brand {
            display: flex;
            align-items: center;
            gap: 8px;
            font-weight: 800;
            font-size: 1.05rem;
            letter-spacing: -0.3px;
            color: var(--text-main);
        }
        .brand-dot {
            width: 9px;
            height: 9px;
            border-radius: 50%;
            background: var(--cyan-pastel);
            box-shadow: 0 0 10px var(--cyan-pastel);
        }
        .status-pill {
            display: flex;
            align-items: center;
            gap: 7px;
            padding: 6px 14px;
            border-radius: 999px;
            font-size: 0.78rem;
            font-weight: 700;
            background: rgba(253, 224, 71, 0.14);
            color: var(--ochre-pastel);
            border: 1px solid rgba(253, 224, 71, 0.35);
            transition: all 0.25s ease;
        }
        .status-pill.connected {
            background: rgba(134, 239, 172, 0.16);
            color: var(--sage-pastel);
            border-color: rgba(134, 239, 172, 0.45);
        }
        .status-pill.paused {
            background: rgba(252, 165, 165, 0.16);
            color: var(--peach-pastel);
            border-color: rgba(252, 165, 165, 0.45);
        }
        .view-container {
            position: relative;
            flex: 1;
            width: 100%;
            display: flex;
            align-items: center;
            justify-content: center;
            overflow: hidden;
            border-radius: 20px;
            background: #04060a;
            border: 1px solid var(--card-border);
        }
        #webcam {
            width: 100%;
            height: 100%;
            object-fit: cover;
            border-radius: 20px;
            transition: opacity 0.25s ease;
        }
        #webcam.mirror {
            transform: scaleX(-1);
        }
        #webcam.hidden {
            opacity: 0;
            pointer-events: none;
        }
        /* Mirino geometrico minimale al centro della camera */
        .reticle {
            position: absolute;
            width: 120px;
            height: 120px;
            border: 1.5px dashed rgba(103, 232, 249, 0.35);
            border-radius: 24px;
            pointer-events: none;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .reticle::after {
            content: "";
            width: 6px;
            height: 6px;
            border-radius: 50%;
            background: var(--cyan-pastel);
            opacity: 0.6;
        }
        /* Overlay iniziale di richiesta permessi & stato disattivato */
        .overlay-card {
            position: absolute;
            inset: 0;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-content: center;
            gap: 14px;
            padding: 24px;
            text-align: center;
            background: rgba(8, 12, 20, 0.94);
            backdrop-filter: blur(16px);
            -webkit-backdrop-filter: blur(16px);
            border-radius: 20px;
            z-index: 15;
            transition: opacity 0.25s ease;
        }
        .overlay-card.hidden {
            display: none;
        }
        .overlay-icon {
            font-size: 2.8rem;
            margin-bottom: 2px;
        }
        .overlay-title {
            font-size: 1.25rem;
            font-weight: 800;
            color: var(--text-main);
            letter-spacing: -0.2px;
        }
        .overlay-desc {
            font-size: 0.88rem;
            color: var(--text-muted);
            max-width: 300px;
            line-height: 1.45;
        }
        .btn-grant {
            margin-top: 10px;
            padding: 16px 28px;
            border-radius: 16px;
            border: 1.5px solid var(--cyan-pastel);
            background: rgba(103, 232, 249, 0.18);
            color: var(--cyan-pastel);
            font-size: 1.05rem;
            font-weight: 800;
            cursor: pointer;
            display: flex;
            align-items: center;
            gap: 10px;
            box-shadow: 0 4px 20px rgba(103, 232, 249, 0.15);
            transition: all 0.15s ease;
        }
        .btn-grant:active {
            transform: scale(0.97);
            background: rgba(103, 232, 249, 0.28);
        }
        .https-alert {
            margin-top: 10px;
            padding: 10px 14px;
            background: rgba(253, 224, 71, 0.12);
            border: 1px solid rgba(253, 224, 71, 0.35);
            color: var(--ochre-pastel);
            border-radius: 12px;
            font-size: 0.80rem;
            line-height: 1.4;
            max-width: 310px;
        }
        .https-btn {
            display: inline-block;
            margin-top: 8px;
            padding: 7px 14px;
            border-radius: 8px;
            background: var(--ochre-pastel);
            color: #000;
            font-weight: 700;
            text-decoration: none;
            font-size: 0.80rem;
        }
        footer {
            margin-top: 10px;
            display: flex;
            gap: 12px;
            z-index: 20;
        }
        .btn-control {
            flex: 1;
            padding: 16px 20px;
            border-radius: 16px;
            border: 1.5px solid rgba(252, 165, 165, 0.45);
            background: rgba(252, 165, 165, 0.16);
            color: var(--peach-pastel);
            font-size: 0.96rem;
            font-weight: 700;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            transition: all 0.15s ease;
        }
        .btn-control:active {
            transform: scale(0.98);
        }
        .btn-control.activate-mode {
            border-color: rgba(134, 239, 172, 0.45);
            background: rgba(134, 239, 172, 0.18);
            color: var(--sage-pastel);
        }
        .btn-rotate {
            padding: 16px 20px;
            border-radius: 16px;
            border: 1.5px solid var(--card-border);
            background: rgba(18, 24, 36, 0.94);
            color: var(--text-main);
            font-size: 0.96rem;
            font-weight: 700;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 6px;
            transition: all 0.15s ease;
        }
        .btn-rotate:active {
            transform: scale(0.96);
        }
    </style>
</head>
<body>
    <header>
        <div class="brand">
            <span class="brand-dot"></span>
            <span>MiMO Eye</span>
        </div>
        <div class="status-pill" id="statusPill">
            <span>●</span>
            <span id="statusText">In connessione...</span>
        </div>
    </header>

    <div class="view-container">
        <video id="webcam" class="mirror hidden" playsinline autoplay muted></video>
        <div class="reticle" id="reticle" style="display: none;"></div>

        <!-- Schermata di avvio / Richiesta autorizzazione fotocamera Android -->
        <div class="overlay-card" id="startOverlay">
            <div class="overlay-icon">📸</div>
            <div class="overlay-title" id="overlayTitle">Attiva Videocamera</div>
            <div class="overlay-desc" id="overlayDesc">
                Tocca il pulsante qui sotto per consentire l'accesso alla fotocamera. Il video viene trasmesso a MiMO Hub sul PC dove avviene il calcolo delle dita.
            </div>
            <button class="btn-grant" id="grantBtn">
                <span>Consenti & Attiva</span>
            </button>
            <div class="https-alert" id="httpsNotice" style="display: none;">
                <div>⚠️ Su Android la fotocamera richiede una connessione protetta HTTPS.</div>
                <a class="https-btn" id="httpsRedirectBtn" href="#">Passa a HTTPS</a>
            </div>
        </div>
    </div>

    <footer>
        <button class="btn-control" id="toggleCamBtn" style="display: none;">Disattiva Videocamera</button>
        <button class="btn-rotate" id="flipCamBtn" style="display: none;" title="Ruota fotocamera">Gira ↺</button>
    </footer>

    <script>
        const video = document.getElementById('webcam');
        const reticle = document.getElementById('reticle');
        const statusPill = document.getElementById('statusPill');
        const statusText = document.getElementById('statusText');
        const startOverlay = document.getElementById('startOverlay');
        const overlayTitle = document.getElementById('overlayTitle');
        const overlayDesc = document.getElementById('overlayDesc');
        const grantBtn = document.getElementById('grantBtn');
        const httpsNotice = document.getElementById('httpsNotice');
        const httpsRedirectBtn = document.getElementById('httpsRedirectBtn');
        const toggleCamBtn = document.getElementById('toggleCamBtn');
        const flipCamBtn = document.getElementById('flipCamBtn');

        const captureCanvas = document.createElement('canvas');
        captureCanvas.width = 320;
        captureCanvas.height = 240;
        const captureCtx = captureCanvas.getContext('2d');

        let cameraActive = false;
        let isFacingUser = true;
        let mediaStream = null;
        let ws = null;
        let inFlightPost = false;
        let pcConnected = false;
        let frameCount = 0;
        let lastFpsCheck = Date.now();
        let currentFps = 0;

        // 1. Verifica Contesto di Sicurezza HTTPS per Android (Samsung Galaxy S22 Ultra, Chrome, Samsung Internet)
        function checkSecureContextAndTunnel() {
            const isSecure = (location.protocol === 'https:' || location.hostname === 'localhost' || location.hostname === '127.0.0.1');
            if (!isSecure) {
                const pollTunnel = () => {
                    fetch('/api/tunnel')
                        .then(r => r.json())
                        .then(data => {
                            if (data && data.has_tunnel && data.https_url) {
                                httpsRedirectBtn.href = data.https_url;
                                httpsNotice.style.display = 'block';
                                // Reindirizzamento istantaneo verso HTTPS certificato
                                window.location.replace(data.https_url);
                            } else {
                                httpsNotice.style.display = 'block';
                                httpsNotice.innerHTML = '<div>⚠️ La fotocamera richiede HTTPS sui dispositivi Android. Inquadra il QR Code sul PC con tunnel attivo o tocca qui sotto appena disponibile.</div>';
                                setTimeout(pollTunnel, 1200);
                            }
                        })
                        .catch(() => {
                            setTimeout(pollTunnel, 1800);
                        });
                };
                pollTunnel();
            }
        }
        checkSecureContextAndTunnel();

        // 2. Connessione WebSocket e Fallback HTTP
        function connectWebSocket() {
            try {
                const proto = (location.protocol === 'https:') ? 'wss:' : 'ws:';
                ws = new WebSocket(proto + '//' + location.host + '/ws');
                ws.onopen = () => {
                    pcConnected = true;
                    updateUiState();
                };
                ws.onclose = () => {
                    pcConnected = false;
                    updateUiState();
                    setTimeout(connectWebSocket, 2000);
                };
                ws.onerror = () => {
                    try { ws.close(); } catch (e) {}
                };
            } catch (e) {}
        }
        connectWebSocket();

        function updateUiState() {
            if (!cameraActive) {
                statusPill.className = 'status-pill paused';
                statusText.textContent = 'Videocamera Disattivata';
                toggleCamBtn.style.display = 'none';
                flipCamBtn.style.display = 'none';
                startOverlay.classList.remove('hidden');
                reticle.style.display = 'none';
                video.classList.add('hidden');
            } else {
                startOverlay.classList.add('hidden');
                reticle.style.display = 'flex';
                video.classList.remove('hidden');
                toggleCamBtn.style.display = 'flex';
                flipCamBtn.style.display = 'flex';
                toggleCamBtn.className = 'btn-control';
                toggleCamBtn.textContent = 'Disattiva Videocamera';

                if (pcConnected) {
                    statusPill.className = 'status-pill connected';
                    statusText.textContent = 'Connesso a MiMO Hub • ' + (currentFps > 0 ? currentFps : 30) + ' FPS';
                } else {
                    statusPill.className = 'status-pill';
                    statusText.textContent = 'In attesa del PC...';
                }
            }
        }

        function sendPayload(payload) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                // Drop frame solo se il socket di rete e' congestionato (>64KB in coda, previene lag progressivo)
                if (ws.bufferedAmount > 65536) {
                    return;
                }
                ws.send(JSON.stringify(payload));
                if (!pcConnected) {
                    pcConnected = true;
                    updateUiState();
                }
                return;
            }
            if (!inFlightPost) {
                inFlightPost = true;
                fetch('/data', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify(payload),
                    keepalive: true
                }).then(r => {
                    if (r.ok && !pcConnected) {
                        pcConnected = true;
                        updateUiState();
                    }
                    inFlightPost = false;
                }).catch(() => {
                    inFlightPost = false;
                });
            }
        }

        // Wrapper universale con supporto sia navigator.mediaDevices che legacy getUserMedia
        function requestUniversalMedia(constraints) {
            if (navigator.mediaDevices && navigator.mediaDevices.getUserMedia) {
                return navigator.mediaDevices.getUserMedia(constraints);
            }
            const legacyGUM = navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia || navigator.msGetUserMedia;
            if (legacyGUM) {
                return new Promise((resolve, reject) => {
                    legacyGUM.call(navigator, constraints, resolve, reject);
                });
            }
            return Promise.reject(new Error("SECURE_CONTEXT_REQUIRED"));
        }

        // 3. Richiesta Permessi e Avvio Fotocamera (Multi-Stage Graceful Fallback)
        async function startCamera() {
            try {
                if (mediaStream) {
                    mediaStream.getTracks().forEach(t => t.stop());
                    mediaStream = null;
                }

                video.classList.toggle('mirror', isFacingUser);

                let stream = null;
                const facing = isFacingUser ? 'user' : 'environment';

                // Tentativo 1: vincolo standard per smartphone Samsung
                try {
                    stream = await requestUniversalMedia({
                        video: { facingMode: facing },
                        audio: false
                    });
                } catch (err1) {
                    // Tentativo 2: qualsiasi fotocamera video senza vincoli di orientamento
                    try {
                        stream = await requestUniversalMedia({
                            video: true,
                            audio: false
                        });
                    } catch (err2) {
                        // Tentativo 3: vincoli di risoluzione ideale
                        stream = await requestUniversalMedia({
                            video: { width: { ideal: 640 }, height: { ideal: 480 } },
                            audio: false
                        });
                    }
                }

                mediaStream = stream;
                video.srcObject = mediaStream;
                await video.play();

                cameraActive = true;
                updateUiState();
                setTimeout(captureFrameTick, 50);
            } catch (err) {
                cameraActive = false;
                updateUiState();
                startOverlay.classList.remove('hidden');

                if (err.message === "SECURE_CONTEXT_REQUIRED" || !window.isSecureContext) {
                    overlayTitle.textContent = "Connessione HTTPS Richiesta";
                    overlayDesc.textContent = "Android blocca l'accesso alla fotocamera sui siti HTTP locali. Apri l'indirizzo protetto HTTPS (lucchetto verde) generato dal QR Code sul PC.";
                    httpsNotice.style.display = 'block';
                    alert("Su Android la fotocamera richiede una connessione HTTPS sicura. Inquadra il QR Code aggiornato con il lucchetto HTTPS sul PC.");
                } else if (err.name === "NotAllowedError" || err.name === "PermissionDeniedError") {
                    overlayTitle.textContent = "Permesso Negato";
                    overlayDesc.textContent = "L'autorizzazione fotocamera è stata bloccata. Tocca l'icona del lucchetto nella barra indirizzi del browser e imposta 'Fotocamera -> Consenti'.";
                    alert("Permesso Negato dal telefono:\nApri le impostazioni del browser o tocca il lucchetto nella barra degli indirizzi e attiva l'accesso alla fotocamera.");
                } else {
                    overlayTitle.textContent = "Errore Fotocamera";
                    overlayDesc.textContent = "Dettaglio: " + (err.name || "") + " " + (err.message || "");
                    alert("Errore Fotocamera: " + (err.name || "Sconosciuto") + "\n" + (err.message || ""));
                }
            }
        }

        function stopCamera() {
            cameraActive = false;
            if (mediaStream) {
                mediaStream.getTracks().forEach(t => t.stop());
                mediaStream = null;
            }
            video.srcObject = null;
            updateUiState();
            sendPayload({ type: 'smartphone_frame', active: false });
        }

        // Il gesto dell'utente sul pulsante garantisce la richiesta permessi di sistema senza blocco
        grantBtn.addEventListener('click', async () => {
            const isSecure = (location.protocol === 'https:' || location.hostname === 'localhost' || location.hostname === '127.0.0.1');
            if (!isSecure) {
                try {
                    const r = await fetch('/api/tunnel');
                    const d = await r.json();
                    if (d && d.has_tunnel && d.https_url) {
                        window.location.replace(d.https_url);
                        return;
                    }
                } catch (e) {}
            }
            startCamera();
        });

        toggleCamBtn.addEventListener('click', () => {
            if (cameraActive) {
                stopCamera();
            } else {
                startCamera();
            }
        });

        flipCamBtn.addEventListener('click', () => {
            isFacingUser = !isFacingUser;
            if (cameraActive) {
                startCamera();
            }
        });

        // 4. Streaming fluido a bassissima latenza (zero bufferbloat, scarta fotogrammi vecchi)
        let isEncodingFrame = false;
        function captureFrameTick() {
            if (!cameraActive || !mediaStream || video.videoWidth === 0) {
                setTimeout(captureFrameTick, 100);
                return;
            }

            // Se la trasmissione di rete e' congestionata (>64KB in coda) o stiamo gia codificando, scarta il frame (mantiene latenza fissa ~35ms!)
            if ((ws && ws.readyState === WebSocket.OPEN && ws.bufferedAmount > 65536) ||
                (inFlightPost && (!ws || ws.readyState !== WebSocket.OPEN)) ||
                isEncodingFrame) {
                setTimeout(captureFrameTick, 25);
                return;
            }

            isEncodingFrame = true;
            try {
                captureCtx.save();
                if (isFacingUser) {
                    captureCtx.translate(captureCanvas.width, 0);
                    captureCtx.scale(-1, 1);
                }
                captureCtx.drawImage(video, 0, 0, captureCanvas.width, captureCanvas.height);
                captureCtx.restore();

                const jpegDataUrl = captureCanvas.toDataURL('image/jpeg', 0.45);
                sendPayload({
                    type: 'smartphone_frame',
                    active: true,
                    jpeg: jpegDataUrl
                });

                frameCount++;
                const now = Date.now();
                if (now - lastFpsCheck >= 1000) {
                    currentFps = Math.round((frameCount * 1000) / (now - lastFpsCheck));
                    frameCount = 0;
                    lastFpsCheck = now;
                    if (cameraActive && pcConnected) {
                        statusText.textContent = 'Connesso a MiMO Hub • ' + currentFps + ' FPS';
                    }
                }
            } catch (e) {}
            isEncodingFrame = false;

            setTimeout(captureFrameTick, 33); // ~30 FPS fluidi e sincronizzati
        }

        updateUiState();
        captureFrameTick();
    </script>
</body>
</html>)HTML";

    return s;
}

} // namespace mimo
