#include <iostream>
#include <opencv2/opencv.hpp>

using namespace cv;
using namespace std;

int main(){
    
    VideoCapture cap(0);
    Mat img;

    while(true){
        cap.read(img);

        imshow("Webcam", img);
        waitKey(1);
    }

    cap.release();
    destroyAllWindows();

    return 0;
}