#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"

#define CRANIO_LIVE

#define CRANIO_RPI
#ifdef CRANIO_RPI
#include "ofxRPiCameraVideoGrabber.h"
#endif

#define CAMERA_WIDTH 320
#define CAMERA_HEIGHT 240
#define CAMERA_FPS 30

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);
#ifdef CRANIO_LIVE
#ifdef CRANIO_RPI
        ofxRPiCameraVideoGrabber video;
#else
        ofVideoGrabber video;
#endif
#else
        ofVideoPlayer video;
#endif
    
    bool doDrawInfo;
    ofTexture videoTexture;
    bool doPixels;
    bool doReloadPixels;
    
    ofxCvColorImage			colorImg;
    
    ofxCvGrayscaleImage 	grayImage;
    ofxCvGrayscaleImage 	grayBg;
    ofxCvGrayscaleImage 	grayDiff;
    
    ofxCvContourFinder 	contourFinder;
    
    int 				threshold;
    bool				bLearnBakground;
		
};
