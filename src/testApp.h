#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"
#include "ofxCv.h"
#include "ofxBlobTracker.h"
#include "ofxArea.h"

#ifdef TARGET_LINUX
	#define CRANIO_RPI
#endif

#ifdef CRANIO_RPI
#include "ofxRPiCameraVideoGrabber.h"
#include "wiringPi.h"
#endif

#define CRANIO_LIVE

#define DRAW_SCALE 1.0

#define CAMERA_FPS 30
#define CAMERA_WIDTH 320
#define CAMERA_HEIGHT 240

#define PROCESSING_WIDTH 320
#define PROCESSING_HEIGHT 240

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

	bool doDebug;
    
    ofxBlobTracker		blobsTracker;
	ofxCvContourFinder  contourFinder;

	int peopleCount,peopleCountEntering,peopleCountLeaving;
	vector<int> peopleCurrent;

	void blobAdded(ofxBlob &_blob);
    void blobUpdated(ofxBlob &_blob);
    void blobDeleted(ofxBlob &_blob);
    
    int 				threshold;

	ofxCv::RunningBackground	background;
	ofxCvColorImage				cameraFrame;
	ofxCvColorImage				processingFrame;
	ofxCvGrayscaleImage			thresholded;
	//ofxCvColorImage				mask;
	ofImage imgBackground;

	ofxArea preLimit;
	ofxArea postLimit;

	int dragginPoint;

	float preLimitDensity;
	float postLimitDensity;
	
	int case1PreLimitOccupation;
	int case1PostLimitOccupation;

	int case2PreLimitOccupation;
	int case2PostLimitOccupation;

	void loadLimits();
	void saveLimits();

	int queueCounter;
	bool queueFull;

	float waitForCamera;
	float waitForBackground;

	float reportTimeout;

	ofSoundPlayer alarm;
	float alarmTimeout;

#ifndef CRANIO_RPI
	bool calibrationMode;
#endif
};
