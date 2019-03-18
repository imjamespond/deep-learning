#include <stdio.h>
#include <vector>
#include <string>
#include <opencv2/highgui.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <codechiev/base/Time.hpp>
#include <codechiev/base/Thread.hpp>

#include "detect.hpp"

using cv::Mat;
using cv::Point;
using cv::Rect2d;
using cv::Scalar;
using cv::Tracker;
using cv::VideoCapture;

typedef std::vector<DarknetTracker> Trackers;
typedef std::string str;
typedef boost::shared_ptr<count_args> count_args_ptr;
typedef boost::unordered_map<str, codechiev::base::thread_ptr> thread_map;

bool __check_overlapped__( Trackers &, box &, Mat &, int &);
void __clean_trackers__(Trackers &);
void __detect__(str, str, on_detect_func, on_track_func, count_args_ptr);


void detect(on_detect_func onDetect, on_track_func onTrack, count_args *args)
{

    // metadata *meta = args->metadata;
    str name(args->name);
    str url(args->url); 


    count_args_ptr pArgs(new count_args(*args));

    codechiev::base::thread_func func = boost::bind(&__detect__, name, url, onDetect, onTrack, pArgs);
    // codechiev::base::thread_ptr thread(new codechiev::base::Thread(name, func));
    // thread->start();
    // thread->join();
    func();
}

void __detect__(str name, str url, on_detect_func onDetect, on_track_func onTrack, count_args_ptr args)
{
    network *net = args->network;
    float thresh = args->thresh;
    float hier = args->hier;
    int *map = args->map;
    int relative = args->relative;

    Mat frame;
    VideoCapture vc;
    vc.open(url);

    Trackers trackers;

    int iframe(0);
    int count(0);
    int _count[4];
    ::memset(_count, 0, sizeof _count);
    for (;;)
    {
        // wait for a new frame from camera and store it into 'frame'
        vc >> frame;

        // check if we succeeded
        if (frame.empty())
        {
            //perror("ERROR! blank frame grabbed\n");
            break;
        }
        // show live and wait for a key with timeout long enough to show images
        // cv::imshow("Live", frame);
        if (cv::waitKey(5) >= 0)
        {
            break;
        }

        cv::resize(frame, frame, cv::Size(frame.cols * 0.75, frame.rows * 0.75), 0, 0, CV_INTER_LINEAR);

        float x1 = args->x1 * frame.cols;
        float y1 = args->y1 * frame.rows;
        float x2 = args->x2 * frame.cols;
        float y2 = args->y2 * frame.rows;

        for (Trackers::iterator it = trackers.begin(); it != trackers.end(); it++)
        {
            DarknetTracker &tracker = *it;

            tracker.last.x = tracker.roi.x;
            tracker.last.y = tracker.roi.y;
            // update the tracking result
            tracker.trackable = tracker.ptr->update(frame, tracker.roi);
            // draw the tracked object
            cv::rectangle(frame, tracker.roi, Scalar(255, 0, 0), 2, 1);
            cv::putText(frame,
                        tracker.id,
                        Point(tracker.roi.x, tracker.roi.y), // Coordinates
                        cv::FONT_HERSHEY_COMPLEX_SMALL,      // Font
                        1.0,                                 // Scale. 2.0 = 2x bigger
                        Scalar(255, 255, 255));              // BGR Color
            
            if(tracker.trackable)
            { 
                countTest(tracker, x1, y1, x2, y2, _count);
            }
        }

       
        if (iframe % 15 == 0)
        {
            // trackers.clear();

            image img = mat_to_image(frame);
            int nboxes;
            network_predict_image(net, img);
            detection *dets = get_network_boxes(net, img.w, img.h, thresh, hier, map, relative, &nboxes);
            if(!(*onDetect)(dets, nboxes))
                return;

            for (int i = 0; i < nboxes; ++i)
            {
                bool nothing(true);
                float prob(0.0f);
                // for (int j(0); j < meta->classes; ++j)
                for (int j(0); j < 1; ++j) // detect people only
                {
                    if (dets[i].prob[j] > .3f)
                    {
                        nothing = false;
                        prob = dets[i].prob[j];
                        break;
                    }
                }
                if (nothing)
                    continue;

                box &bbox = dets[i].bbox;
                cv::circle(frame, Point((int)bbox.x, (int)bbox.y), 10.0, Scalar(0, 0, 255), 1, 8);
                cv::putText(frame,
                            boost::lexical_cast<std::string>(prob),
                            Point((int)bbox.x + 20, (int)bbox.y), // Coordinates
                            cv::FONT_HERSHEY_COMPLEX_SMALL,      // Font
                            1.0,                                 // Scale. 2.0 = 2x bigger
                            Scalar(255, 255, 255));              // BGR Color


                double w = bbox.w, h = bbox.h;
                double x = bbox.x - (bbox.w * .5), x_ = x + w;
                double y = bbox.y - (bbox.h * .5), y_ = y + h;
                if (x>x1 && x<x2 && y>y1 && y<y2 )
                {
                    __check_overlapped__(trackers, bbox, frame, count);
                }
                
            }
            __clean_trackers__(trackers);

            free_image(img);
            free_detections(dets, nboxes);
        }
        else if (iframe % 15 == 1) 
        {
            // codechiev::base::Time::SleepMillis(1000);
        }

        if (iframe % 45 == 0)
        {

            (*onTrack)(_count[0],_count[1],_count[2],_count[3]);
            ::memset(_count, 0, sizeof _count);
        }

        cv::imshow("Live", frame);
        iframe++;
    }
}

bool __check_overlapped__(Trackers &trackers, box &bbox, Mat &frame, int &count)
{
    double w1 = bbox.w, h1 = bbox.h;
    double x1 = bbox.x - (bbox.w * .5), x1_ = x1 + w1;
    double y1 = bbox.y - (bbox.h * .5), y1_ = y1 + h1;

    double x = 0.0f, x_ = 0.0f, w = 0.0f;
    double y = 0.0f, y_ = 0.0f, h = 0.0f;
    
    bool overlapped(false);
    for (Trackers::iterator it = trackers.begin(); it != trackers.end(); it++)
    {
        DarknetTracker &tracker = *it;

        if (tracker.trackable)
        {
            double w2 = tracker.roi.width, h2 = tracker.roi.height;
            double x2 = (tracker.roi.x), x2_ = (tracker.roi.x + tracker.roi.width);
            x = x2 > x1 ? x2 : x1;
            x_ = x2_ > x1_ ? x1_ : x2_;
            w = x_ - x;
            double y2 = (tracker.roi.y), y2_ = (tracker.roi.y + tracker.roi.height);
            y = y2 > y1 ? y2 : y1;
            y_ = y2_ > y1_ ? y1_ : y2_;
            h = y_ - y;

            if (w/w1>.7 && w/w2>.7 && h/h1>.6 && h/h2>.6)
            {
                //tracker and detection of darknet overlapped
                // printf("%f, %f\n", w, h);
                overlapped = true;
                tracker.lostCount = 0;

                tracker.roi.x = x1;
                tracker.roi.y = y1;
                tracker.last.x = x1;
                tracker.last.y = y1;
                tracker.roi.width = w1;
                tracker.roi.height = h1;

                // create a tracker object
                tracker.ptr = cv::TrackerKCF::create();
                // initialize the tracker
                tracker.ptr->init(frame, tracker.roi);
                // update the tracking result
                tracker.ptr->update(frame, tracker.roi);

                break;
            }
        }
    }

    if (!overlapped)
    {
        DarknetTracker tracker;
        tracker.id = boost::lexical_cast<std::string>(++count);
        tracker.lostCount = 0; 
        tracker.disabled = false;

        tracker.roi.x = x1;
        tracker.roi.y = y1;
        tracker.last.x = x1;
        tracker.last.y = y1;
        tracker.roi.width = w1;
        tracker.roi.height = h1;

        // create a tracker object
        tracker.ptr = cv::TrackerKCF::create();
        // initialize the tracker
        tracker.ptr->init(frame, tracker.roi);
        // update the tracking result
        tracker.trackable = tracker.ptr->update(frame, tracker.roi);

        trackers.push_back(tracker);

    }
  
}

void __clean_trackers__(Trackers &trackers)
{
    for (Trackers::iterator it = trackers.begin(); it != trackers.end(); )
    {
        DarknetTracker &tracker = *it;

        if (tracker.lostCount > 3)
        {
            it = trackers.erase(it);
        }
        else
        {
            tracker.lostCount += 1; // deletable in next round
            it++;
        }
        
    }
}
