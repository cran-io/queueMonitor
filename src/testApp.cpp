#include "testApp.h"

//--------------------------------------------------------------
void testApp::setup(){
    
    ofSetLogLevel(OF_LOG_VERBOSE);
	ofSetLogLevel("ofThread", OF_LOG_ERROR);
    doDrawInfo	= true;
    doPixels = true;
	doReloadPixels = true;

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
    video.setDeviceID(0);
    video.setDesiredFrameRate(CAMERA_FPS);
    video.setUseTexture(true);
    video.initGrabber(CAMERA_WIDTH, CAMERA_HEIGHT);
#endif
#else
    video.loadMovie("test.mov");
    video.play();
#endif
    
    if(doPixels){
        videoTexture.allocate(CAMERA_WIDTH, CAMERA_HEIGHT, GL_RGBA);
    }
    
    colorImg.allocate(CAMERA_WIDTH, CAMERA_HEIGHT);
	grayImage.allocate(CAMERA_WIDTH, CAMERA_HEIGHT);
	grayBg.allocate(CAMERA_WIDTH, CAMERA_HEIGHT);
	grayDiff.allocate(CAMERA_WIDTH, CAMERA_HEIGHT);
    
	bLearnBakground = true;
	threshold = 80;
    
    
    ofSetVerticalSync(true);
}

//--------------------------------------------------------------
void testApp::update(){
#ifndef CRANIO_RPI
    video.update();
#endif
    
    if (!doPixels){
		return;
	}

	if(video.isFrameNew()){
		if(doReloadPixels)
            videoTexture.loadData(video.getPixels(), video.getWidth(), video.getHeight(), GL_RGBA);
        
        colorImg.setFromPixels(video.getPixels(), video.getWidth(), video.getHeight());
        
        grayImage = colorImg;
		if (bLearnBakground == true){
			grayBg = grayImage;		// the = sign copys the pixels from grayImage into grayBg (operator overloading)
			bLearnBakground = false;
		}
        
		// take the abs value of the difference between background and incoming and then threshold:
		grayDiff.absDiff(grayBg, grayImage);
		grayDiff.threshold(threshold);
        
		// find contours which are between the size of 20 pixels and 1/3 the w*h pixels.
		// also, find holes is set to true so we will get interior contours as well....
		contourFinder.findContours(grayDiff, 20, (CAMERA_WIDTH*CAMERA_HEIGHT)/3, 10, true);	// find holes
	}
}

//--------------------------------------------------------------
void testApp::draw(){
#ifdef CRANIO_RPI
    video.draw();
#else
    video.draw(0,0);
#endif
	if(doPixels && doReloadPixels)
	{
		video.draw(0, 0, CAMERA_WIDTH/2, CAMERA_HEIGHT/2);
	}
    
    // draw the incoming, the grayscale, the bg and the thresholded difference
	//colorImg.draw(20,20);
    grayBg.draw(10,400,320,240);
	grayImage.draw(340,400,320,240);
	grayDiff.draw(670,400,320,240);
    contourFinder.draw(670,400,320,240);
    
	stringstream info;
	info << "APP FPS: " << ofGetFrameRate() << "\n";
	info << "Camera Resolution: " << video.getWidth() << "x" << video.getHeight()	<< " @ "<< CAMERA_FPS <<"FPS"<< "\n";
    info << "BLOBS: " << contourFinder.nBlobs << "\n";
	info << "PIXELS ENABLED: " << doPixels << "\n";
	info << "PIXELS RELOADING ENABLED: " << doReloadPixels << "\n";	
	info << "\n";
    info << "Press space to capture background" << "\n";
    info << "Threshold " << threshold << " (press: +/-)" << "\n";
	info << "Press p to Toggle pixel processing" << "\n";
	info << "Press r to Toggle pixel reloading" << "\n";
	info << "Press g to Toggle info" << "\n";
	if (doDrawInfo)
	{
		ofDrawBitmapStringHighlight(info.str(), 1000, 420, ofColor::black, ofColor::yellow);
	}
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
    
    if (key == 'g')
	{
		doDrawInfo = !doDrawInfo;
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
	if (key == 'r') {
		doReloadPixels = !doReloadPixels;
	}
    
    if (key == ' ') {
        bLearnBakground = true;
	}
	if (key == '+') {
        threshold ++;
        if (threshold > 255)
            threshold = 255;
	}
	if (key == '-') {
        threshold --;
        if (threshold < 0)
            threshold = 0;
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
