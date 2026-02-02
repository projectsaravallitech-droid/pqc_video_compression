#include <opencv2/opencv.hpp>
#include <iostream>

int main()
{
    cv::VideoCapture cap(0);    // Opens the deafult laptop webcam 

    if (!cap.isOpened())
    {
        std::cerr << "Error: Could not open webcam" << std::endl;
        return -1;
    }

    cv::Mat frame;  // cv::Mat is matrix data structure for images 

    std::cout << "Press 'q' to quit" << std::endl;

    while (true)
    {
        cap >> frame;   // Capture frame

        if (frame.empty())
        {
            std::cerr << "Error: Empty frame received" << std::endl;
            break;
        }

        cv::imshow("My Webcam", frame);

        // Exit on 'q'
        if (cv::waitKey(1) == 'q')
            break;
    }

    cap.release();
    cv::destroyAllWindows();

    return 0;
}
