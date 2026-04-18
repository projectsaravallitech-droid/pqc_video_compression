#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <iostream>
#include <vector>
#include <string>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <opencv2/opencv.hpp>
#include <oqs/oqs.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_PORT 8888
#define CLIENT_PORT 8889
#define LOCAL_IP "192.168.1.8" //IP adsress

using namespace cv;
using namespace std;

// Globals
extern atomic<bool> streamToClient;
extern string appStatus;
extern string cryptText1;
extern string cryptText2;
extern string fpsText;
extern vector<string> perfLines;
extern uint32_t RxCount;

// Common KEM crypto logic
class KeyTransfer {
public:
    KeyTransfer();
    ~KeyTransfer();
    bool generateReceiverKeyPair(std::vector<uint8_t>& p_key, std::vector<uint8_t>& s_key);
    bool senderEncapsulate(const std::vector<uint8_t>& r_pub, std::vector<uint8_t>& c_text, std::vector<uint8_t>& s_sec);
    bool receiverDecapsulate(const std::vector<uint8_t>& s_key, const std::vector<uint8_t>& c_text, std::vector<uint8_t>& s_sec);
private:
    OQS_KEM* kem;
};

// UI declarations
void drawUI(Mat& ui, bool serverMode, Mat& videoFrame);
void onMouse(int event, int x, int y, int, void*);

// Logic declarations
void RunClient();
