#include "testApp.h"

#define SKIP_FRAMES 10
//--------------------------------------------------------------
void testApp::setup(){
    
    ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetLogLevel("ofThread", OF_LOG_ERROR);
    doDrawInfo	= true;
	doPixels = true;
	doDebug = true;
#ifdef CRANIO_LIVE
#ifdef CRANIO_RPI
	OMXCameraSettings omxCameraSettings;
    omxCameraSettings.width = CAMERA_WIDTH;
	omxCameraSettings.height = CAMERA_HEIGHT;
	omxCameraSettings.framerate = CAMERA_FPS;
	omxCameraSettings.isUsingTexture = true;
	    
	if (doPixels){
		omxCameraSettings.enablePixels = true;
	}
	video.setup(omxCameraSettings);
#else
    video.setVerbose(true);
    video.setDeviceID(1);
    video.setDesiredFrameRate(CAMERA_FPS);
    video.setUseTexture(true);
    video.initGrabber(CAMERA_WIDTH, CAMERA_HEIGHT);
#endif
#else
    video.loadMovie("test.mov");
	video.setLoopState(OF_LOOP_NORMAL);
    video.play();
#endif
    
	threshold = 100;
    
	background.setLearningTime(10000);
	background.setLearningRate(0.0001); //default value
	background.setThresholdValue(threshold);
	
	frame.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	thresholded.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	mask.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);

	imgBackground.allocate(CAMERA_WIDTH,CAMERA_HEIGHT,OF_IMAGE_COLOR);

	lastBlobCount=0;
    
	ofEnableAlphaBlending();
    ofSetVerticalSync(true);
}

//--------------------------------------------------------------
void testApp::update(){
#ifndef CRANIO_RPI
    video.update();
#endif

	if(!doPixels){
		return;
	}

	if(video.isFrameNew()){
		frame.setFromPixels(video.getPixels(),video.getWidth(),video.getHeight());
		background.update(ofxCv::toCv(frame), ofxCv::toCv(thresholded));
		
		thresholded.erode(5);
		thresholded.dilate(10);
		thresholded.blur(21);

		mask=thresholded;
		frame&=mask;
		
		imgThresholded.setFromPixels(thresholded.getPixelsRef());

		if(doDebug){
			frame.updateTexture();
			thresholded.updateTexture();
			ofxCv::toOf(background.getBackground(),imgBackground);
			imgBackground.update();
		}

		// find contours which are between the size of 1/50 and 1/3 the w*h pixels.
		// also, find holes is set to false so we will not get interior contours.
		contourFinder.findContours(thresholded, (CAMERA_WIDTH*CAMERA_HEIGHT)/50, (CAMERA_WIDTH*CAMERA_HEIGHT)/3, 10, false);	// dont find holes
		if(contourFinder.nBlobs!=lastBlobCount){
			cout<<ofGetTimestampString()<<": "<<contourFinder.nBlobs<<endl;
			lastBlobCount=contourFinder.nBlobs;
		}
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	ofPushMatrix();
	ofScale(DRAW_SCALE,DRAW_SCALE);
#ifdef CRANIO_RPI
    video.draw();
#else
    video.draw(0,0);
#endif
	if(doPixels && doDebug){
		// draw the incoming, the grayscale, the bg and the thresholded difference
		imgBackground.draw(CAMERA_WIDTH,0);
		
		//thresholded.draw(670,10,320,240);
		imgThresholded.draw(0,CAMERA_HEIGHT);
		contourFinder.draw(0,CAMERA_HEIGHT);

		frame.draw(CAMERA_WIDTH,CAMERA_HEIGHT);
	}
    ofPopMatrix();

	stringstream info;
	info << "APP FPS: " << ofGetFrameRate() << "\n";
	info << "Camera Resolution: " << video.getWidth() << "x" << video.getHeight()	<< " @ "<< CAMERA_FPS <<"FPS"<< "\n";
    info << "BLOBS: " << contourFinder.nBlobs << "\n";
	info << "PIXELS ENABLED: " << doPixels << "\n";	
	info << "\n";
    info << "Press space to capture background" << "\n";
    info << "Threshold " << threshold << " (press: UP/DOWN)" << "\n";
	info << "Press p to Toggle pixel processing" << "\n";
	info << "Press r to Toggle pixel reloading" << "\n";
	info << "Press g to Toggle info" << "\n";
	if (doDrawInfo){
		ofDrawBitmapStringHighlight(info.str(), 0, 2*CAMERA_HEIGHT*DRAW_SCALE+20, ofColor::black, ofColor::yellow);
	}
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
    
    if (key == 'g'){
		doDrawInfo = !doDrawInfo;
	}
	if (key == 'd'){
		doDebug = !doDebug;
	}	
	if (key == 'p')
	{
		doPixels = !doPixels;
#ifdef CRANIO_RPI
		if (!doPixels)
		{
			video.disablePixels();
		}else
		{
			video.enablePixels();
		}
#endif
	}
    
    if (key == ' ') {
        background.reset();
	}
	if (key == OF_KEY_UP) {
        threshold ++;
        if (threshold > 255)
            threshold = 255;
		background.setThresholdValue(threshold);
	}
	if (key == OF_KEY_DOWN) {
        threshold --;
        if (threshold < 0)
            threshold = 0;
		background.setThresholdValue(threshold);
	}

}

//--------------------------------------------------------------
void testApp::keyReleased(int key){

}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 

}
