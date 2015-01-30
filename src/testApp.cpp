#include "testApp.h"

#define PERSISTENCE_THRESHOLD 10
#define PRELIMIT_DENSITY_THRESHOLD 0.4

#define POSTLIMIT_DENSITY_THRESHOLD 0.2
#define PRELIMIT_DENSITY_THRESHOLD_ALT 0.2

bool persistenceCalculation(bool condition,int& last, int threshold){
	if(condition){
		if(last<threshold){
			last++;
			if(last>=threshold)
				return true;
		}
	}
	else{
		if(last){
			last-=2;
			if(last<=0){
				last=0;
			}
		}
	}
	return false;
}

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
	lastPeopleDetection=0;

	peopleCount=0;

	preLimitDensity=0;
	lastPreLimitOccupation=0;
	postLimitDensity=0;
	lastPostLimitOccupation=0;

	queueFull=false;
    
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

		if(contourFinder.nBlobs){
			vector<ofxCvBlob>  blobs = contourFinder.blobs;
			ofPoint center(CAMERA_WIDTH/2,CAMERA_HEIGHT/2);
			bool peopleDetection = false;
			for(int i=0; i<blobs.size(); i++){
				if(blobs[i].boundingRect.inside(center)){
					peopleDetection=true;
					break;
				}
			}
			if(persistenceCalculation(peopleDetection,lastPeopleDetection,PERSISTENCE_THRESHOLD))
				peopleCount++;
		}
		int width=thresholded.getWidth();
		int height=thresholded.getHeight();
		unsigned char * pixels = thresholded.getPixels();
		long preLimitOccupation=0;
		for(int y=0;y<height;y++){
			for(int x=width/2;x<width;x++){
				preLimitOccupation+=pixels[x+y*width];
			}
		}
		preLimitOccupation/=255;
		preLimitDensity=preLimitOccupation/(height*width*0.5);

		long postLimitOccupation=0;
		for(int y=0;y<height;y++){
			for(int x=0;x<width/2;x++){
				postLimitOccupation+=pixels[x+y*width];
			}
		}
		postLimitOccupation/=255;
		postLimitDensity=postLimitOccupation/(height*width*0.5);

		persistenceCalculation(preLimitDensity>PRELIMIT_DENSITY_THRESHOLD,lastPreLimitOccupation,PERSISTENCE_THRESHOLD);
		
		persistenceCalculation(postLimitDensity>POSTLIMIT_DENSITY_THRESHOLD,lastPostLimitOccupation,PERSISTENCE_THRESHOLD);

		if(lastPreLimitOccupation>=PERSISTENCE_THRESHOLD && lastPeopleDetection>=PERSISTENCE_THRESHOLD){
			queueFull=true;
		}
		else if(lastPostLimitOccupation>=PERSISTENCE_THRESHOLD && preLimitDensity>=PRELIMIT_DENSITY_THRESHOLD_ALT){
			queueFull=true;
		}
		else{
			queueFull=false;
		}
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	if(queueFull)
		ofBackground(255,0,0);
	else
		ofBackground(125);
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
		ofSetColor(0,100,255);
		ofLine(CAMERA_WIDTH/2,0,CAMERA_WIDTH/2,CAMERA_HEIGHT);
		ofLine(CAMERA_WIDTH+CAMERA_WIDTH/2,0,CAMERA_WIDTH+CAMERA_WIDTH/2,CAMERA_HEIGHT);
		ofLine(CAMERA_WIDTH/2,CAMERA_HEIGHT,CAMERA_WIDTH/2,CAMERA_HEIGHT+CAMERA_HEIGHT);
		ofLine(CAMERA_WIDTH+CAMERA_WIDTH/2,CAMERA_HEIGHT,CAMERA_WIDTH+CAMERA_WIDTH/2,CAMERA_HEIGHT+CAMERA_HEIGHT);
		ofSetColor(255,0,0);
		ofCircle(CAMERA_WIDTH/2,CAMERA_HEIGHT/2,10);
		ofCircle(CAMERA_WIDTH+CAMERA_WIDTH/2,CAMERA_HEIGHT/2,10);
		ofCircle(CAMERA_WIDTH/2,CAMERA_HEIGHT+CAMERA_HEIGHT/2,10);
		ofCircle(CAMERA_WIDTH+CAMERA_WIDTH/2,CAMERA_HEIGHT+CAMERA_HEIGHT/2,10);
		ofPopStyle();

	}
    ofPopMatrix();

	stringstream info;
	info << "APP FPS: " << ofGetFrameRate() << "\n";
	info << "Camera Resolution: " << video.getWidth() << "x" << video.getHeight()	<< " @ "<< CAMERA_FPS <<"FPS"<< "\n";
    info << "BLOBS: " << contourFinder.nBlobs << "\n";
	info << "PEOPLE COUNT: " << peopleCount << "\n";
	info << "DENSITY: Post: " << postLimitDensity << ", Pre: " << preLimitDensity << "\n";
	info << "QUEUE FULL: " << queueFull << "\n";
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
