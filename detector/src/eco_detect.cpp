#include <stdio.h>
// #include <vector>
// #include <string>
#include <opencv2/highgui.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <base/Time.hpp>
#include <base/Thread.hpp>
#include <base/Logger.hpp>

#include "count.hpp"
#include "eco_detect.hpp"

void __clean_trackers__(EcoTrackers &);

void eco_detect(args_t *args, extra_args_t extra_args, str frame_file, EcoTrackers *trackers_ptr, int iframe)
{

    lock_func_t lock_func = extra_args.lock_func;
    detect_func_t detect_func = extra_args.detect_func;
    track_func_t track_func = extra_args.track_func;


    network *net = args->network;
    float thresh = args->thresh;
    float hier = args->hier;
    int *map = args->map;
    int relative = args->relative;
    bool debug = args->debug;

    if (debug && (iframe % 100 == 0))
    {
        LOG_DEBUG << "eco_detect: " << iframe;
    }

    Mat frame = cv::imread(frame_file);
    // VideoCapture vc;
    // vc.open(url);

    EcoTrackers &trackers = *trackers_ptr;

    // int iframe(0);
    int traffic[4];
    ::memset(traffic, 0, sizeof traffic);
    // for (;;)
    {
        // wait for a new frame from camera and store it into 'frame'
        // vc >> frame;

        // check if we succeeded
        if (frame.empty())
        {
            //perror("ERROR! blank frame grabbed\n");
            return;
        }
        // show live and wait for a key with timeout long enough to show images
        // cv::imshow("Live", frame);
        if (debug && cv::waitKey(5) >= 0)
        {
            return;
        }

        // cv::resize(frame, frame, cv::Size(640.0f, (int)(640.0f / (float)frame.cols * (float)frame.rows)), 0, 0, CV_INTER_LINEAR); //kcf tracing need high reslution

        float x1 = args->x1 * frame.cols;
        float y1 = args->y1 * frame.rows;
        float x2 = args->x2 * frame.cols;
        float y2 = args->y2 * frame.rows;

        if (debug)
        {
            cv::rectangle(frame, cv::Rect2d(x1, y1, x2 - x1, y2 - y1), Scalar(255, 0, 0), 2, 1);
        }

        for (EcoTrackers::iterator it = trackers.begin(); it != trackers.end(); it++)
        {
            EcoTracker &tracker = *it;

            tracker.last = tracker.roi;
            // update the tracking result
            tracker.trackable = tracker.eco->update(frame, tracker.roi);

            if (debug)
            {
                // draw the tracked object
                cv::rectangle(frame, tracker.roi, tracker.disabled ? Scalar(100, 100, 100) : Scalar(0, 255, 0), 2, 1);
            }

            if (tracker.trackable)
            {
                count<EcoTracker>(tracker, x1, y1, x2, y2, traffic);
            }
        }

        if (iframe % 15 == 0)
        {
            // trackers.clear();
            (*lock_func)();
            image img = mat_to_image(frame);
            int nboxes;
            network_predict_image(net, img);
            detection *dets = get_network_boxes(net, img.w, img.h, thresh, hier, map, relative, &nboxes);
            if (!(*detect_func)(dets, nboxes))
            {
                extra_args.queue->stop();
                return;
            }

            for (int i = 0; i < nboxes; ++i)
            {
                bool nothing(true);
                float prob(0.0f);
                // for (int j(0); j < meta->classes; ++j)
                for (int j(0); j < 1; ++j) // detect people only
                {
                    if (dets[i].prob[j] > .6f)
                    {
                        nothing = false;
                        prob = dets[i].prob[j];
                        break;
                    }
                }
                if (nothing)
                    continue;

                box &bbox = dets[i].bbox;

                if (debug)
                {
                    cv::circle(frame, Point((int)bbox.x, (int)bbox.y), 10.0, Scalar(0, 0, 255), 1, 8);
                    cv::putText(frame,
                                boost::lexical_cast<std::string>(prob),
                                Point((int)bbox.x + 20, (int)bbox.y), // Coordinates
                                cv::FONT_HERSHEY_COMPLEX_SMALL,       // Font
                                1.0,                                  // Scale. 2.0 = 2x bigger
                                Scalar(255, 255, 255));               // BGR Color
                }

                double w = bbox.w, h = bbox.h;
                double x = bbox.x; // - (bbox.w * .5), x_ = x + w;
                double y = bbox.y; // - (bbox.h * .5), y_ = y + h;
                if (x > x1 && x < x2 && y > y1 && y < y2)
                {
                    eco_track(trackers, bbox, frame);
                }
            }
            __clean_trackers__(trackers);

            free_image(img);
            free_detections(dets, nboxes);
        }
        else if (debug && (iframe % 15 == 1))
        {
            // codechiev::base::Time::SleepMillis(500);
        }
        else if (debug)
        {
            // codechiev::base::Time::SleepMillis(50);
        }

        if (iframe % 300 == 0)
        {

            (*track_func)(traffic[0], traffic[1], traffic[2], traffic[3]);
            ::memset(traffic, 0, sizeof traffic);
        }

        if (debug)
        {
            cv::imshow("Live", frame);
        }

        iframe++;
    }

    (*track_func)(traffic[0], traffic[1], traffic[2], traffic[3]);
}

void __clean_trackers__(EcoTrackers &trackers)
{
    for (EcoTrackers::iterator it = trackers.begin(); it != trackers.end();)
    {
        EcoTracker &tracker = *it;

        if (tracker.lost > 0)
        {
            it = trackers.erase(it);
        }
        else
        {
            tracker.lost += 1; // deletable in next round
            it++;
        }
    }
}