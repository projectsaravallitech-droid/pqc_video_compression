#include <opencv2/opencv.hpp>
#include <iostream>

using namespace cv;
using namespace std;

int main() {
    VideoCapture cap(0);

    if (!cap.isOpened()) {
        cout << "Camera not detected" << endl;
        return -1;
    }

    Mat frame;
    while (true) {
        cap >> frame;
        if (frame.empty())
            break;

        imshow("USB Camera", frame);

        if (waitKey(1) == 27) // ESC
            break;
    }

    cap.release();
    destroyAllWindows();
    return 0;
}
