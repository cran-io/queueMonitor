#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxCv.h"
#include "ofxBlobTracker.h"

#ifdef TARGET_LINUX
	#define CRANIO_RPI
#endif

#ifdef CRANIO_RPI
#include "ofxRPiCameraVideoGrabber.h"
#include "wiringPi.h"
#endif

#define CRANIO_LIVE

#define CAMERA_WIDTH 640
#define CAMERA_HEIGHT 360
#define CAMERA_FPS 30

#define PROCESSING_WIDTH 320
#define PROCESSING_HEIGHT 180

#define DRAW_SCALE 0.5

typedef struct Area{
	vector<ofPoint> vertex;
	int minX,minY,maxX,maxY;
	long pixelCount;
};

class testApp : public ofBaseApp{

	public:
		void setup();
		void update();
		void draw();
		void drawAllZones();
		void drawZones();

		void keyPressed(int key);
		void keyReleased(int key);
		void mouseMoved(int x, int y );
		void mouseDragged(int x, int y, int button);
		void mousePressed(int x, int y, int button);
		void mouseReleased(int x, int y, int button);
		void windowResized(int w, int h);
		void dragEvent(ofDragInfo dragInfo);
		void gotMessage(ofMessage msg);

		void exit();
#ifdef CRANIO_LIVE
#ifdef CRANIO_RPI
        ofxRPiCameraVideoGrabber video;
#else
        ofVideoGrabber video;
#endif
#else
        ofVideoPlayer video;
#endif

	float time;
    
    bool doDrawInfo;
    bool doPixels;
	bool doDebug;
    
    ofxBlobTracker		blobsTracker;
	ofxCvContourFinder  contourFinder;
    
    int 				threshold;

	ofxCv::RunningBackground	background;
	ofxCvColorImage				cameraFrame;
	ofxCvColorImage				processingFrame;
	ofxCvGrayscaleImage			thresholded;
	ofxCvColorImage				mask;
	ofImage imgThresholded;
	ofImage imgBackground;

	Area preLimit;
	Area postLimit;
	ofPoint limitTop,limitBottom,limitMidpoint;

	bool draggin;
	int dragginPoint;

	ofPixels preLimitMask;
	float preLimitDensity;
	ofPixels postLimitMask;
	float postLimitDensity;
	
	int case1PreLimitOccupation;
	int case1PostLimitOccupation;

	int case2PreLimitOccupation;
	int case2PostLimitOccupation;

	void loadLimits();
	void saveLimits();
	void makeLimitMask();

	int queueCounter;
	bool queueFull;

	float waitForCamera;
	float waitForBackground;
};
