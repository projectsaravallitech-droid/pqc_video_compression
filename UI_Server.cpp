#include "header.h"

bool isBtn(int x, int y, Rect r) { return x >= r.x && x <= r.x + r.width && y >= r.y && y <= r.y + r.height; }

Rect r_mode(20, 20, 200, 100);
Rect r_btns(240, 20, 740, 100);
Rect r_video(20, 140, 730, 540);
Rect r_perf(770, 140, 210, 540);

Rect b_start( 250, 30,  150, 40);
Rect b_gen(   420, 30,  200, 40);
Rect b_linkK( 640, 30,  320, 40);
Rect b_rec(   250, 80,  150, 40);
Rect b_stream(420, 80,  200, 40);
Rect b_linkF( 640, 80,  320, 40);

void drawUI(Mat& ui, bool serverMode, Mat& videoFrame) {
    ui = Mat(700, 1000, CV_8UC3, Scalar(15, 15, 15)); 
    rectangle(ui, r_mode, Scalar(200,200,200), 1);
    rectangle(ui, r_btns, Scalar(200,200,200), 1);
    rectangle(ui, r_video, Scalar(200,200,200), 1);
    rectangle(ui, r_perf, Scalar(200,200,200), 1);

    putText(ui, "Server UI", Point(r_mode.x + 20, r_mode.y + 55), FONT_HERSHEY_SIMPLEX, 0.7, Scalar(255,255,255), 1);
    rectangle(ui, b_start, Scalar(200,200,200), 1);
    putText(ui, isStreaming ? "Stop Camera" : "Start Camera", Point(b_start.x + 10, b_start.y + 25), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,255,255), 1);
    rectangle(ui, b_gen, Scalar(200,200,200), 1);
    putText(ui, "Generate/Rec keys", Point(b_gen.x + 10, b_gen.y + 25), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,255,255), 1);
    
    // Cryptography Data visualization Box 1
    rectangle(ui, b_linkK, Scalar(200,200,200), 1);
    putText(ui, cryptText1, Point(b_linkK.x + 10, b_linkK.y + 25), FONT_HERSHEY_SIMPLEX, 0.45, Scalar(0,255,255), 1);

    // Cryptography Data visualization Box 2
    rectangle(ui, b_rec, Scalar(200,200,200), 1);
    putText(ui, isRecording ? "Stop Rec" : "Record", Point(b_rec.x + 10, b_rec.y + 25), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,255,255), 1);
    rectangle(ui, b_stream, Scalar(200,200,200), 1);
    putText(ui, "Start Streaming", Point(b_stream.x + 10, b_stream.y + 25), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,255,255), 1);
    
    rectangle(ui, b_linkF, Scalar(200,200,200), 1);
    putText(ui, cryptText2, Point(b_linkF.x + 10, b_linkF.y + 25), FONT_HERSHEY_SIMPLEX, 0.45, Scalar(0,255,255), 1);

    if (!videoFrame.empty()) {
        Mat resized;
        resize(videoFrame, resized, Size(r_video.width, r_video.height));
        resized.copyTo(ui(r_video));
    }
    int baseline = 0;
    Size fpsSize = getTextSize(fpsText, FONT_HERSHEY_SIMPLEX, 0.6, 2, &baseline);
    Point fpsOrigin(r_video.x + r_video.width - fpsSize.width - 20, r_video.y + 28);
    rectangle(ui,
              Rect(fpsOrigin.x - 8, fpsOrigin.y - fpsSize.height - 6, fpsSize.width + 16, fpsSize.height + 12),
              Scalar(20, 20, 20),
              FILLED);
    putText(ui, fpsText, fpsOrigin, FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0,255,0), 2);
    putText(ui, "Status: " + appStatus + (TxCount > 0 ? " | Frames Tx: " + to_string(TxCount) : ""), Point(r_video.x + 10, r_video.y + r_video.height - 15), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(0,255,0), 2);

    putText(ui, "Performance", Point(r_perf.x + 18, r_perf.y + 28), FONT_HERSHEY_SIMPLEX, 0.6, Scalar(255,255,255), 1);
    int perfY = r_perf.y + 62;
    for (const string& line : perfLines) {
        putText(ui, line, Point(r_perf.x + 10, perfY), FONT_HERSHEY_SIMPLEX, 0.34, Scalar(120,255,120), 1);
        perfY += 34;
    }
}

void onMouse(int event, int x, int y, int, void*) {
    if (event == EVENT_LBUTTONDOWN) {
        if (isBtn(x, y, b_start)) {
            isStreaming = !isStreaming;
            if (isStreaming) {
                appStatus = "Camera activated! Next: Click 'Generate/Rec keys' to start PQC.";
            } else {
                appStatus = "Camera stopped.";
            }
        }
        else if (isBtn(x, y, b_gen)) {
            requestPQC = true;
            appStatus = "PQC initializing! Negotiating keys securely with Client over UDP...";
        }
        else if (isBtn(x, y, b_rec)) {
            isRecording = !isRecording;
        }
        else if (isBtn(x, y, b_stream)) {
            streamToClient = true;
            appStatus = "Streaming active! Blasting securely over network.";
        }
    }
}
