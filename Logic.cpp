#include "header.h"

namespace {
using Clock = std::chrono::steady_clock;

struct FramePerfStats {
    double handshakeMs = 0.0;
    double handshakeEncapMs = 0.0;
    bool handshakeDone = false;

    double totalDecryptMs = 0.0;
    double totalDecodeMs = 0.0;
    double totalDisplayPrepMs = 0.0;
    double totalAssemblyMs = 0.0;
    double totalArrivalLatencyMs = 0.0;
    uint64_t decryptedFrames = 0;

    double secDecryptMs = 0.0;
    double secDecodeMs = 0.0;
    double secDisplayPrepMs = 0.0;
    double secAssemblyMs = 0.0;
    double secArrivalLatencyMs = 0.0;
    uint32_t secFrames = 0;
    uint32_t secPackets = 0;
    Clock::time_point lastReport = Clock::now();
};

struct FpsCounter {
    uint32_t frames = 0;
    double fps = 0.0;
    Clock::time_point windowStart = Clock::now();
};

double ToMs(const Clock::duration& d) {
    return std::chrono::duration<double, std::milli>(d).count();
}

uint64_t ReadUint64BE(const char* data) {
    uint64_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value = (value << 8) | (uint8_t)data[i];
    }
    return value;
}

string FormatMs(const char* label, double value) {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s%.3f ms", label, value);
    return string(buffer);
}

void PrintPerfReport(FramePerfStats& stats, uint32_t chunksReceived, uint16_t totalChunks) {
    const auto now = Clock::now();
    if (now - stats.lastReport < std::chrono::seconds(1)) return;

    const double avgDecrypt = stats.decryptedFrames ? stats.totalDecryptMs / stats.decryptedFrames : 0.0;
    const double avgDecode = stats.decryptedFrames ? stats.totalDecodeMs / stats.decryptedFrames : 0.0;
    const double avgDisplay = stats.decryptedFrames ? stats.totalDisplayPrepMs / stats.decryptedFrames : 0.0;
    const double avgAssembly = stats.decryptedFrames ? stats.totalAssemblyMs / stats.decryptedFrames : 0.0;
    const double avgArrival = stats.decryptedFrames ? stats.totalArrivalLatencyMs / stats.decryptedFrames : 0.0;
    const double secFrames = stats.secFrames ? static_cast<double>(stats.secFrames) : 1.0;

    perfLines = {
        "Handshake: " + FormatMs("", stats.handshakeMs),
        "KEM: " + FormatMs("", stats.handshakeEncapMs),
        "Avg decrypt: " + FormatMs("", avgDecrypt),
        "Avg decode: " + FormatMs("", avgDecode),
        "Avg show: " + FormatMs("", avgDisplay),
        "1s frames: " + to_string(stats.secFrames) + " | pkt: " + to_string(stats.secPackets),
        "1s assemble: " + FormatMs("", stats.secAssemblyMs / secFrames),
        "1s arrival: " + FormatMs("", stats.secArrivalLatencyMs / secFrames),
        "1s delay: " + FormatMs("", (stats.secAssemblyMs + stats.secDecodeMs + stats.secDisplayPrepMs) / secFrames),
        "1s partial: " + to_string(chunksReceived) + "/" + to_string(totalChunks)
    };

    cryptText2 = "Avg arrival(clock): " + FormatMs("", avgArrival) + " | Avg assembly: " + FormatMs("", avgAssembly);
    cout << "[Client Perf] Handshake total: " << FormatMs("", stats.handshakeMs)
         << " | KEM encapsulate: " << FormatMs("", stats.handshakeEncapMs) << '\n';
    cout << "[Client Perf] Avg decrypt: " << FormatMs("", avgDecrypt)
         << " | Avg decode: " << FormatMs("", avgDecode)
         << " | Avg show: " << FormatMs("", avgDisplay) << '\n';
    cout << "[Client Perf] 1s report -> frames: " << stats.secFrames
         << " | pkt: " << stats.secPackets
         << " | recv/assemble: " << FormatMs("", stats.secAssemblyMs / secFrames)
         << " | arrival(clock): " << FormatMs("", stats.secArrivalLatencyMs / secFrames)
         << " | frame delay: " << FormatMs("", (stats.secAssemblyMs + stats.secDecodeMs + stats.secDisplayPrepMs) / secFrames)
         << " | partial: " << chunksReceived << "/" << totalChunks << '\n'
         << flush;

    stats.secDecryptMs = 0.0;
    stats.secDecodeMs = 0.0;
    stats.secDisplayPrepMs = 0.0;
    stats.secAssemblyMs = 0.0;
    stats.secArrivalLatencyMs = 0.0;
    stats.secFrames = 0;
    stats.secPackets = 0;
    stats.lastReport = now;
}

void UpdateDisplayedFps(FpsCounter& counter) {
    const auto now = Clock::now();
    counter.frames++;
    const double elapsedSec = std::chrono::duration<double>(now - counter.windowStart).count();
    if (elapsedSec >= 1.0) {
        counter.fps = counter.frames / elapsedSec;
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "FPS: %.1f", counter.fps);
        fpsText = buffer;
        counter.frames = 0;
        counter.windowStart = now;
    }
}
}

void RunClient() {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    int rcvbuf = 1024 * 1024; // 1 MB buffer
    setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&rcvbuf, sizeof(rcvbuf));
    
    u_long mode = 1; ioctlsocket(client_socket, FIONBIO, &mode); 

    sockaddr_in client_bind_addr = {};
    client_bind_addr.sin_family = AF_INET;
    client_bind_addr.sin_addr.s_addr = INADDR_ANY;
    client_bind_addr.sin_port = htons(CLIENT_PORT);
    if (::bind(client_socket, (sockaddr*)&client_bind_addr, sizeof(client_bind_addr)) == SOCKET_ERROR) {
        appStatus = "Fatal: Socket BIND ERROR (Port 8889).";
    }

    const char* winName = "Video Stream Client";
    namedWindow(winName, WINDOW_AUTOSIZE);
    setMouseCallback(winName, onMouse, nullptr);

    KeyTransfer pqc;
    vector<uint8_t> shared_key;
    uint32_t lastFrameId = 0;
    bool hasKeys = false;
    Mat uiFrame;
    Mat displayFrame(480, 640, CV_8UC3, Scalar(15,15,15));
    
    static uint32_t currentFrameId = 0;
    static vector<vector<uint8_t>> frameChunks;
    static uint16_t chunksReceived = 0;
    static uint64_t serverSendTimestampUs = 0;
    static Clock::time_point frameAssemblyStart = Clock::now();
    FramePerfStats perf;
    FpsCounter fpsCounter;
    Clock::time_point handshakeStart = Clock::now();
    bool handshakeStarted = false;

    while (true) {
        char recv_buf[4096]; 
        sockaddr_in sender_addr = {};
        int s_len = sizeof(sender_addr);
        int len;
        
        while ((len = recvfrom(client_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&sender_addr, &s_len)) > 0) {
            int current_s_len = s_len;
            s_len = sizeof(sender_addr); 
            
            if (recv_buf[0] == 1) { 
                handshakeStart = Clock::now();
                handshakeStarted = true;
                vector<uint8_t> pub_key(recv_buf + 1, recv_buf + len);
                vector<uint8_t> ciphertext;
                const auto encapsStart = Clock::now();
                if (pqc.senderEncapsulate(pub_key, ciphertext, shared_key)) {
                    const auto encapsEnd = Clock::now();
                    vector<uint8_t> pkt(1, 2);
                    pkt.insert(pkt.end(), ciphertext.begin(), ciphertext.end());
                    sendto(client_socket, (char*)pkt.data(), pkt.size(), 0, (sockaddr*)&sender_addr, current_s_len);
                    hasKeys = true;
                    appStatus = "Handshake Complete! Click 'Start Streaming' here.";
                    perf.handshakeEncapMs = ToMs(encapsEnd - encapsStart);
                    perf.handshakeMs = handshakeStarted ? ToMs(Clock::now() - handshakeStart) : 0.0;
                    perf.handshakeDone = true;
                    cout << "[Client Perf] Handshake complete | total: " << FormatMs("", perf.handshakeMs)
                         << " | KEM encapsulate: " << FormatMs("", perf.handshakeEncapMs) << '\n'
                         << flush;

                    char hexK[128]; 
                    if (shared_key.size() >= 8) snprintf(hexK, sizeof(hexK), "Key: PQC_C:%02X%02X%02X%02X%02X%02X%02X%02X...", shared_key[0], shared_key[1], shared_key[2], shared_key[3], shared_key[4], shared_key[5], shared_key[6], shared_key[7]);
                    cryptText1 = hexK;
                }
            } else if (recv_buf[0] == 3 && hasKeys && streamToClient) {
                if (len < 17) {
                    continue;
                }
                uint32_t frameId = ((uint32_t)(uint8_t)recv_buf[1] << 24) | ((uint32_t)(uint8_t)recv_buf[2] << 16) | 
                                   ((uint32_t)(uint8_t)recv_buf[3] << 8) | ((uint32_t)(uint8_t)recv_buf[4]);
                                   
                uint16_t totalChunks = ((uint16_t)(uint8_t)recv_buf[5] << 8) | (uint8_t)recv_buf[6];
                uint16_t chunkIdx = ((uint16_t)(uint8_t)recv_buf[7] << 8) | (uint8_t)recv_buf[8];
                uint64_t packetSendTimestampUs = len >= 17 ? ReadUint64BE(recv_buf + 9) : 0;
                const auto packetReceiveTime = Clock::now();
                perf.secPackets++;

                if (frameId > currentFrameId || frameId < 10) {
                    currentFrameId = frameId;
                    frameChunks.assign(totalChunks, vector<uint8_t>());
                    chunksReceived = 0;
                    serverSendTimestampUs = packetSendTimestampUs;
                    frameAssemblyStart = packetReceiveTime;
                }
                
                if (frameId == currentFrameId && chunkIdx < totalChunks) {
                    if (frameChunks[chunkIdx].empty()) { 
                        frameChunks[chunkIdx] = vector<uint8_t>((uint8_t*)recv_buf + 17, (uint8_t*)recv_buf + len);
                        chunksReceived++;
                        
                        if (chunksReceived == totalChunks) { 
                            const auto assemblyDone = Clock::now();
                            vector<uchar> buf;
                            for (const auto& c : frameChunks) buf.insert(buf.end(), c.begin(), c.end());

                            char hexIn[64] = "";
                            size_t mid = buf.size() / 2;
                            if(mid + 3 < buf.size()) snprintf(hexIn, sizeof(hexIn), "Enc: %02X%02X%02X%02X", buf[mid], buf[mid+1], buf[mid+2], buf[mid+3]);
                            
                            const auto decryptStart = Clock::now();
                            for (size_t i = 0; i < buf.size(); i++) buf[i] ^= shared_key[i % shared_key.size()];
                            const auto decryptEnd = Clock::now();
                            
                            char hexOut[64] = "";
                            if(mid + 3 < buf.size()) snprintf(hexOut, sizeof(hexOut), "Dec: %02X%02X%02X%02X", buf[mid], buf[mid+1], buf[mid+2], buf[mid+3]);
                            
                            cryptText2 = string(hexIn) + " -> " + string(hexOut);

                            const auto decodeStart = Clock::now();
                            Mat decoded = imdecode(buf, IMREAD_COLOR);
                            const auto decodeEnd = Clock::now();
                            if (!decoded.empty()) {
                                const auto showPrepStart = Clock::now();
                                displayFrame = decoded;
                                const auto showPrepEnd = Clock::now();
                                RxCount++;
                                UpdateDisplayedFps(fpsCounter);

                                const double assemblyMs = ToMs(assemblyDone - frameAssemblyStart);
                                const double decryptMs = ToMs(decryptEnd - decryptStart);
                                const double decodeMs = ToMs(decodeEnd - decodeStart);
                                const double displayPrepMs = ToMs(showPrepEnd - showPrepStart);
                                const double arrivalLatencyMs =
                                    serverSendTimestampUs == 0 ? 0.0 :
                                    (static_cast<double>(std::chrono::duration_cast<std::chrono::microseconds>(
                                        std::chrono::system_clock::now().time_since_epoch()).count() - serverSendTimestampUs) / 1000.0);

                                perf.totalAssemblyMs += assemblyMs;
                                perf.totalDecryptMs += decryptMs;
                                perf.totalDecodeMs += decodeMs;
                                perf.totalDisplayPrepMs += displayPrepMs;
                                perf.totalArrivalLatencyMs += arrivalLatencyMs;
                                perf.decryptedFrames++;

                                perf.secAssemblyMs += assemblyMs;
                                perf.secDecryptMs += decryptMs;
                                perf.secDecodeMs += decodeMs;
                                perf.secDisplayPrepMs += displayPrepMs;
                                perf.secArrivalLatencyMs += arrivalLatencyMs;
                                perf.secFrames++;
                            }
                        }
                    }
                }
                PrintPerfReport(perf, chunksReceived, totalChunks);
            }
        }

        PrintPerfReport(perf, chunksReceived, static_cast<uint16_t>(frameChunks.size()));
        drawUI(uiFrame, false, displayFrame);
        imshow(winName, uiFrame);
        if (waitKey(30) == 27 || getWindowProperty(winName, WND_PROP_VISIBLE) < 1) break;
    }
    closesocket(client_socket); WSACleanup();
}
