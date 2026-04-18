#include "header.h"

// Instantiate Global variables 
atomic<bool> streamToClient(false);
string appStatus = "Welcome! Step 1: Click 'Generate/Rec keys'!";
string cryptText1 = "Decryption: Awaiting PQC handshake...";
string cryptText2 = "Stream Decode: Waiting for encrypted packets...";
string fpsText = "FPS: 0.0";
vector<string> perfLines = {
    "Handshake: waiting...",
    "KEM: waiting...",
    "Avg decrypt: waiting...",
    "Avg decode: waiting...",
    "Avg show: waiting...",
    "1s frames: waiting...",
    "1s assemble: waiting...",
    "1s delay: waiting..."
};
uint32_t RxCount = 0;

int main() {
    RunClient();
    return 0;
}
