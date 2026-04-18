#include "header.h"

// Instantiate globals 
atomic<bool> isStreaming(false);
atomic<bool> isRecording(false);
atomic<bool> requestPQC(false);
atomic<bool> streamToClient(false);
string appStatus = "Welcome to Server! Step 1: Click 'Start Camera'.";
string cryptText1 = "Encryption: Awaiting PQC handshake...";
string cryptText2 = "Stream Output: Waiting for connection...";
string fpsText = "FPS: 0.0";
vector<string> perfLines = {
    "Enc current: waiting...",
    "Enc average: waiting...",
    "Enc 1s avg: waiting...",
    "Frames in 1s: 0"
};
uint32_t TxCount = 0;
uint32_t RxCount = 0;

int main() {
    RunServer();
    return 0;
}
