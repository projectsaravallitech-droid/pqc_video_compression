#include "header.h"

namespace {
uint64_t GetUnixTimeMicros() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

using Clock = std::chrono::steady_clock;

struct ServerPerfStats {
    double currentEncryptMs = 0.0;
    double totalEncryptMs = 0.0;
    double secEncryptMs = 0.0;
    uint64_t encryptedFrames = 0;
    uint32_t secFrames = 0;
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

string FormatMs(double value) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.3f ms", value);
    return string(buffer);
}

void UpdateServerPerfUi(ServerPerfStats& stats) {
    const auto now = Clock::now();
    if (now - stats.lastReport < std::chrono::seconds(1)) {
        return;
    }

    const double avgEncryptMs = stats.encryptedFrames ? stats.totalEncryptMs / stats.encryptedFrames : 0.0;
    const double secAvgEncryptMs = stats.secFrames ? stats.secEncryptMs / stats.secFrames : 0.0;

    perfLines = {
        "Enc current: " + FormatMs(stats.currentEncryptMs),
        "Enc average: " + FormatMs(avgEncryptMs),
        "Enc 1s avg: " + FormatMs(secAvgEncryptMs),
        "Frames in 1s: " + to_string(stats.secFrames)
    };

    stats.secEncryptMs = 0.0;
    stats.secFrames = 0;
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

void RunServer() {
    WSADATA wsa; WSAStartup(MAKEWORD(2,2), &wsa);
    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    
    int sndbuf = 1024 * 1024; 
    setsockopt(server_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&sndbuf, sizeof(sndbuf));

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(SERVER_PORT);
    
    if (::bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        appStatus = "Fatal: Socket BIND ERROR (Port 8888). Close other servers!";
    }
    u_long mode = 1; ioctlsocket(server_socket, FIONBIO, &mode); 

    const char* winName = "Video Stream Server";
    namedWindow(winName, WINDOW_AUTOSIZE);
    setMouseCallback(winName, onMouse, nullptr);

    VideoCapture cap; VideoWriter writer;
    KeyTransfer pqc;
    vector<uint8_t> shared_key, secret_key;
    
    sockaddr_in client_addr = {};
    client_addr.sin_family = AF_INET;
    inet_pton(AF_INET, LOCAL_IP, &client_addr.sin_addr);
    client_addr.sin_port = htons(CLIENT_PORT);
    int client_len = sizeof(client_addr);
    
    bool handshakeComplete = false;
    uint32_t frameId = 0;
    int retryCount = 0;
    Mat uiFrame, camFrame;
    ServerPerfStats perf;
    FpsCounter fpsCounter;

    while (true) {
        if (requestPQC && !handshakeComplete) {
            if (retryCount++ % 30 == 0) { 
                vector<uint8_t> pub_key;
                if (pqc.generateReceiverKeyPair(pub_key, secret_key)) {
                    vector<uint8_t> pkt(1, 1);
                    pkt.insert(pkt.end(), pub_key.begin(), pub_key.end());
                    sendto(server_socket, (char*)pkt.data(), pkt.size(), 0, (sockaddr*)&client_addr, sizeof(client_addr));
                }
            }
        }
    
        char recv_buf[65000];
        sockaddr_in sender_addr = {};
        int sender_len = sizeof(sender_addr);
        int len;
        
        while ((len = recvfrom(server_socket, recv_buf, sizeof(recv_buf), 0, (sockaddr*)&sender_addr, &sender_len)) > 0) {
            sender_len = sizeof(sender_addr);
            
            if (recv_buf[0] == 2 && requestPQC) { 
                vector<uint8_t> ciphertext(recv_buf + 1, recv_buf + len);
                if (pqc.receiverDecapsulate(secret_key, ciphertext, shared_key)) {
                    handshakeComplete = true;
                    requestPQC = false;
                    appStatus = "Handshake successful! You can start streaming.";
                    client_addr = sender_addr; 
                    
                    // Display Key
                    char hexK[128]; 
                    if (shared_key.size() >= 8) snprintf(hexK, sizeof(hexK), "Key: PQC_S:%02X%02X%02X%02X%02X%02X%02X%02X...", shared_key[0], shared_key[1], shared_key[2], shared_key[3], shared_key[4], shared_key[5], shared_key[6], shared_key[7]);
                    cryptText1 = hexK;
                }
            }
        }

        if (isStreaming && !cap.isOpened()) { cap.open(0, CAP_DSHOW); if(!cap.isOpened()) cap.open(0); }
        if (!isStreaming && cap.isOpened()) cap.release();
        if (isRecording && cap.isOpened() && !writer.isOpened()) writer.open("recording.avi", VideoWriter::fourcc('M','J','P','G'), 20.0, Size(640, 480), true);
        if (!isRecording && writer.isOpened()) writer.release();

        if (cap.isOpened()) {
            cap.read(camFrame);
            if (!camFrame.empty()) {
                UpdateDisplayedFps(fpsCounter);
                if (writer.isOpened()) writer.write(camFrame);
            }
        } else camFrame = Mat();

        drawUI(uiFrame, true, camFrame);
        imshow(winName, uiFrame);

        // Fix: OpenCV window closure safely exits
        if (waitKey(30) == 27 || getWindowProperty(winName, WND_PROP_VISIBLE) < 1) break;

        if (streamToClient && handshakeComplete && !shared_key.empty() && !camFrame.empty()) {
            Mat smaller; resize(camFrame, smaller, Size(640, 480));
            vector<uchar> buf;
            vector<int> params = {IMWRITE_JPEG_QUALITY, 45};
            imencode(".jpg", smaller, buf, params);
            
            if (buf.size() < 64000) {
                // Formatting payload
                char hexIn[64] = "";
                size_t mid = buf.size() / 2;
                if(mid + 3 < buf.size()) snprintf(hexIn, sizeof(hexIn), "Raw: %02X%02X%02X%02X", buf[mid], buf[mid+1], buf[mid+2], buf[mid+3]);
                
                // Cryptographically mask identically
                const auto encryptStart = Clock::now();
                for (size_t i = 0; i < buf.size(); i++) buf[i] ^= shared_key[i % shared_key.size()]; 
                const auto encryptEnd = Clock::now();
                perf.currentEncryptMs = ToMs(encryptEnd - encryptStart);
                perf.totalEncryptMs += perf.currentEncryptMs;
                perf.secEncryptMs += perf.currentEncryptMs;
                perf.encryptedFrames++;
                perf.secFrames++;
                
                char hexOut[64] = "";
                if(mid + 3 < buf.size()) snprintf(hexOut, sizeof(hexOut), "Enc: %02X%02X%02X%02X", buf[mid], buf[mid+1], buf[mid+2], buf[mid+3]);
                cryptText2 = string(hexIn) + " -> " + string(hexOut);

                // MTU Fragmentation
                uint16_t chunkSize = 1200;
                uint16_t totalChunks = (uint16_t)((buf.size() + chunkSize - 1) / chunkSize);
                uint64_t sendTimestampUs = GetUnixTimeMicros();
                
                for (uint16_t c = 0; c < totalChunks; c++) {
                    uint16_t offset = c * chunkSize;
                    uint16_t length = min((size_t)chunkSize, buf.size() - offset);
                    
                    vector<uint8_t> pkt(17 + length);
                    pkt[0] = 3; 
                    pkt[1] = (uint8_t)((frameId>>24)&0xFF); pkt[2] = (uint8_t)((frameId>>16)&0xFF); 
                    pkt[3] = (uint8_t)((frameId>>8)&0xFF); pkt[4] = (uint8_t)(frameId&0xFF);
                    pkt[5] = (uint8_t)((totalChunks>>8)&0xFF); pkt[6] = (uint8_t)(totalChunks&0xFF);
                    pkt[7] = (uint8_t)((c>>8)&0xFF); pkt[8] = (uint8_t)(c&0xFF);
                    for (int i = 0; i < 8; ++i) {
                        pkt[9 + i] = (uint8_t)((sendTimestampUs >> (56 - (i * 8))) & 0xFF);
                    }
                    
                    memcpy(&pkt[17], &buf[offset], length);
                    sendto(server_socket, (char*)pkt.data(), pkt.size(), 0, (sockaddr*)&client_addr, client_len);
                }
                frameId++; TxCount++;
            }
        }

        UpdateServerPerfUi(perf);
    }
    if (cap.isOpened()) cap.release();
    closesocket(server_socket); WSACleanup();
}
