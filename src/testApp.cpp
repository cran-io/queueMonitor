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
    
	background.setLearningTime(1000);
	background.setThresholdValue(threshold);
	
	frame.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	thresholded.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	mask.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);

	imgBackground.allocate(CAMERA_WIDTH,CAMERA_HEIGHT,OF_IMAGE_COLOR);

	descriptor.setSVMDetector(ofxCv::HOGDescriptor::getDefaultPeopleDetector());
	//descriptor.checkDetectorSize();

	skip=SKIP_FRAMES;
    
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
		
		thresholded.erode(4);
		thresholded.dilate(10);
		thresholded.blur(20);

		mask=thresholded;
		frame&=mask;
		
		imgThresholded.setFromPixels(thresholded.getPixelsRef());

		if(doDebug){
			frame.updateTexture();
			thresholded.updateTexture();
			ofxCv::toOf(background.getBackground(),imgBackground);
			imgBackground.update();
		}

		// find contours which are between the size of 20 pixels and 1/3 the w*h pixels.
		// also, find holes is set to true so we will get interior contours as well....
		contourFinder.findContours(thresholded, 20, (CAMERA_WIDTH*CAMERA_HEIGHT)/3, 10, false);	// dont find holes
		
		if(skip<=0){
			vector<ofxCv::Rect> found;
			descriptor.detectMultiScale(ofxCv::toCv(frame), found, 0, ofxCv::Size(8,8), ofxCv::Size(32,32),1.05,2);

			findings.clear();
			for (int i=0; i<found.size(); i++){
				ofxCv::Rect r = found[i];
				int j=0;
				for (j=0; j<found.size(); j++)
					if (j!=i && (r & found[j])==r)
						break;
				if (j==found.size())
					findings.push_back(r);
			}
			skip=SKIP_FRAMES;
		}
		else
			skip--;
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

		ofPushStyle();
		ofNoFill();
		ofPushMatrix();
		ofTranslate(CAMERA_WIDTH,CAMERA_HEIGHT);
		for (int i=0; i<findings.size(); i++){
			ofxCv::Rect r = findings[i];
			ofRect(r.x,r.y,r.width,r.height);
		}
		ofPopMatrix();
		ofPopStyle();
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
