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
    
	threshold = 50;
    
	background.setLearningTime(10000);
	background.setLearningRate(0.0001); //default value
	background.setThresholdValue(threshold);
	
	frame.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	thresholded.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	mask.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);

	imgBackground.allocate(CAMERA_WIDTH,CAMERA_HEIGHT,OF_IMAGE_COLOR);

	ofAddListener(blobTracker.blobAdded, this, &testApp::blobAdded);
    ofAddListener(blobTracker.blobMoved, this, &testApp::blobMoved);
    ofAddListener(blobTracker.blobDeleted, this, &testApp::blobDeleted);

	lastBlobCount=0;
	lastPeopleDetection=0;

	peopleCount=0;

	limitTop.set(CAMERA_WIDTH/2,0);
	limitBottom.set(CAMERA_WIDTH/2,CAMERA_HEIGHT);

	preLimit.vertex.push_back(ofPoint(CAMERA_WIDTH,0));
	preLimit.vertex.push_back(ofPoint(CAMERA_WIDTH,CAMERA_HEIGHT));

	postLimit.vertex.push_back(ofPoint(0,0));
	postLimit.vertex.push_back(ofPoint(0,CAMERA_HEIGHT));

	preLimitDensity=0;
	lastPreLimitOccupation=0;
	preLimitMask.allocate(CAMERA_WIDTH,CAMERA_HEIGHT,OF_IMAGE_GRAYSCALE);
	postLimitDensity=0;
	lastPostLimitOccupation=0;
	postLimitMask.allocate(CAMERA_WIDTH,CAMERA_HEIGHT,OF_IMAGE_GRAYSCALE);

	loadLimits();

	makeLimitMask();

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
		blobTracker.update(thresholded, (CAMERA_WIDTH*CAMERA_HEIGHT)/50, (CAMERA_WIDTH*CAMERA_HEIGHT)/3, 10, false);	// dont find holes
		if(blobTracker.size()!=lastBlobCount){
			cout<<ofGetTimestampString()<<": "<<blobTracker.size()<<endl;
			lastBlobCount=blobTracker.size();
		}

		if(blobTracker.size()){
			ofPoint limitMidpointNormalized(limitMidpoint.x/CAMERA_WIDTH,limitMidpoint.y/CAMERA_HEIGHT);
			bool peopleDetection = false;
			for(int i=0; i<blobTracker.size(); i++){
				if(blobTracker[i].boundingRect.inside(limitMidpointNormalized)){
					peopleDetection=true;
					break;
				}
			}
			if(persistenceCalculation(peopleDetection,lastPeopleDetection,PERSISTENCE_THRESHOLD))
				peopleCount++;
		}
		int width=thresholded.getWidth();
		int height=thresholded.getHeight();
		unsigned char * framePixels = thresholded.getPixels();
		unsigned char * preLimitMaskPixels = preLimitMask.getPixels();
		long preLimitOccupation=0;
		for(int y=preLimit.minY;y<preLimit.maxY;y++){
			for(int x=preLimit.minX;x<preLimit.maxX;x++){
				if(preLimitMaskPixels[x+y*width])
					preLimitOccupation+=framePixels[x+y*width];
			}
		}
		preLimitOccupation/=255;
		preLimitDensity=preLimitOccupation/(float)preLimit.pixelCount;

		unsigned char * postLimitMaskPixels = postLimitMask.getPixels();
		long postLimitOccupation=0;
		for(int y=postLimit.minY;y<postLimit.maxY;y++){
			for(int x=postLimit.minX;x<postLimit.maxX;x++){
				if(postLimitMaskPixels[x+y*width])
					postLimitOccupation+=framePixels[x+y*width];
			}
		}
		postLimitOccupation/=255;
		postLimitDensity=postLimitOccupation/(float)postLimit.pixelCount;

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
void testApp::blobAdded(ofxBlob &_blob){
    ofLog(OF_LOG_NOTICE, "Blob ID " + ofToString(_blob.id) + " added" );
}

//--------------------------------------------------------------
void testApp::blobMoved(ofxBlob &_blob){
    ofLog(OF_LOG_NOTICE, "Blob ID " + ofToString(_blob.id) + " moved" );
}

//--------------------------------------------------------------
void testApp::blobDeleted(ofxBlob &_blob){
    ofLog(OF_LOG_NOTICE, "Blob ID " + ofToString(_blob.id) + " deleted" );
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
		blobTracker.draw(0,CAMERA_HEIGHT);

		frame.draw(CAMERA_WIDTH,CAMERA_HEIGHT);
	}

	drawAllZones();

    ofPopMatrix();

	stringstream info;
	info << "APP FPS: " << ofGetFrameRate() << "\n";
	info << "Camera Resolution: " << video.getWidth() << "x" << video.getHeight()	<< " @ "<< CAMERA_FPS <<"FPS"<< "\n";
    info << "BLOBS: " << blobTracker.size() << "\n";
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
void testApp::drawAllZones(){
	ofPushStyle();
	ofPushMatrix();
	ofTranslate(0,0);
	ofNoFill();
	drawZones();
	ofPopMatrix();

	ofPushMatrix();
	ofTranslate(CAMERA_WIDTH,0);
	ofFill();
	drawZones();
	ofPopMatrix();

	ofPushMatrix();
	ofTranslate(0,CAMERA_HEIGHT);
	ofNoFill();
	drawZones();
	ofPopMatrix();

	ofPushMatrix();
	ofTranslate(CAMERA_WIDTH,CAMERA_HEIGHT);
	ofNoFill();
	drawZones();
	ofPopMatrix();
	ofPopStyle();
}

//--------------------------------------------------------------
void testApp::drawZones(){
	ofPushStyle();
	ofSetColor(200,0,200,100);
	ofSetPolyMode(OF_POLY_WINDING_ODD);	// this is the normal mode
	ofBeginShape();
		ofVertex(limitBottom);
		ofVertex(limitTop);
		ofVertex(preLimit.vertex[0]);
		ofVertex(preLimit.vertex[1]);
	ofEndShape(true);
	ofSetColor(0,200,200,100);
	ofSetPolyMode(OF_POLY_WINDING_ODD);	// this is the normal mode
	ofBeginShape();
		ofVertex(limitBottom);
		ofVertex(limitTop);
		ofVertex(postLimit.vertex[0]);
		ofVertex(postLimit.vertex[1]);
	ofEndShape(true);
	ofSetColor(255,0,0,100);
	ofCircle(limitTop,10);
	ofCircle(limitBottom,10);
	ofCircle(preLimit.vertex[0],10);
	ofCircle(preLimit.vertex[1],10);
	ofCircle(postLimit.vertex[0],10);
	ofCircle(postLimit.vertex[1],10);
	ofCircle(limitMidpoint,5);
	ofPopStyle();
}

//--------------------------------------------------------------
void testApp::makeLimitMask(){
	limitMidpoint=limitTop+(limitBottom-limitTop)*0.5;

	preLimit.minX=min(limitTop.x,limitBottom.x);
	preLimit.maxX=max(preLimit.vertex[0].x,preLimit.vertex[1].x);
	preLimit.minY=min(limitTop.y,preLimit.vertex[0].y);
	preLimit.maxY=max(limitBottom.y,preLimit.vertex[1].y);

	postLimit.maxX=max(limitTop.x,limitBottom.x);
	postLimit.minX=min(postLimit.vertex[0].x,postLimit.vertex[1].x);
	postLimit.minY=min(limitTop.y,postLimit.vertex[0].y);
	postLimit.maxY=max(limitBottom.y,postLimit.vertex[1].y);

	ofFbo tempFbo;
	tempFbo.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);

	tempFbo.begin();
	ofClear(0);
	ofSetColor(255);
	ofSetPolyMode(OF_POLY_WINDING_ODD);	// this is the normal mode
	ofBeginShape();
		ofVertex(limitBottom);
		ofVertex(limitTop);
		ofVertex(preLimit.vertex[0]);
		ofVertex(preLimit.vertex[1]);
	ofEndShape(true);
	tempFbo.end();

	tempFbo.readToPixels(preLimitMask);
	preLimitMask.setImageType(OF_IMAGE_GRAYSCALE);

	preLimit.pixelCount=0;
	for(int y=preLimit.minY;y<preLimit.maxY;y++){
		for(int x=preLimit.minX;x<preLimit.maxX;x++){
			if(preLimitMask.getPixels()[x+y*preLimitMask.getWidth()])
				preLimit.pixelCount++;
		}
	}
	
	tempFbo.begin();
	ofClear(0);
	ofSetColor(255);
	ofSetPolyMode(OF_POLY_WINDING_ODD);	// this is the normal mode
	ofBeginShape();
		ofVertex(limitBottom);
		ofVertex(limitTop);
		ofVertex(postLimit.vertex[0]);
		ofVertex(postLimit.vertex[1]);
	ofEndShape(true);
	tempFbo.end();

	tempFbo.readToPixels(postLimitMask);
	postLimitMask.setImageType(OF_IMAGE_GRAYSCALE);

	postLimit.pixelCount=0;
	for(int y=postLimit.minY;y<postLimit.maxY;y++){
		for(int x=postLimit.minX;x<postLimit.maxX;x++){
			if(postLimitMask.getPixels()[x+y*postLimitMask.getWidth()])
				postLimit.pixelCount++;
		}
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
	ofPoint mouse(x/DRAW_SCALE-CAMERA_WIDTH,y/DRAW_SCALE);
	if(dragginPoint==0)
		limitBottom.set(mouse);
	else if(dragginPoint==1)
		limitTop.set(mouse);
	else if(dragginPoint==2)
		preLimit.vertex[0].set(mouse);
	else if(dragginPoint==3)
		preLimit.vertex[1].set(mouse);
	else if(dragginPoint==4)
		postLimit.vertex[0].set(mouse);
	else if(dragginPoint==5)
		postLimit.vertex[1].set(mouse);
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
	ofPoint mouse(x/DRAW_SCALE-CAMERA_WIDTH,y/DRAW_SCALE);
	//cout<<limitBottom.distanceSquared(mouse)<<endl;
	if(limitBottom.distanceSquared(mouse)<100)
		dragginPoint=0;
	else if(limitTop.distanceSquared(mouse)<100)
		dragginPoint=1;
	else if(preLimit.vertex[0].distanceSquared(mouse)<100)
		dragginPoint=2;
	else if(preLimit.vertex[1].distanceSquared(mouse)<100)
		dragginPoint=3;
	else if(postLimit.vertex[0].distanceSquared(mouse)<100)
		dragginPoint=4;
	else if(postLimit.vertex[1].distanceSquared(mouse)<100)
		dragginPoint=5;
	else
		dragginPoint=-1;
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
	if(dragginPoint>=0){
		makeLimitMask();
		dragginPoint=-1;
	}
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

//--------------------------------------------------------------
void testApp::exit(){
	saveLimits();
}

//--------------------------------------------------------------
void testApp::loadLimits(){
	if(ofFile::doesFileExist("0.limits")){
		ofBuffer buf = ofBufferFromFile("0.limits");
        string line = buf.getNextLine();
		vector<string> p = ofSplitString(line, ",");
        limitTop.set(ofPoint(ofToInt(p[0]),ofToInt(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        limitBottom.set(ofPoint(ofToInt(p[0]),ofToInt(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[0].set(ofPoint(ofToInt(p[0]),ofToInt(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[1].set(ofPoint(ofToInt(p[0]),ofToInt(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        postLimit.vertex[0].set(ofPoint(ofToInt(p[0]),ofToInt(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        postLimit.vertex[1].set(ofPoint(ofToInt(p[0]),ofToInt(p[1])));
	}
}

//--------------------------------------------------------------
void testApp::saveLimits(){
	ofBuffer buf;
    buf.append(ofToString(limitTop)+"\n");
	buf.append(ofToString(limitBottom)+"\n");
	buf.append(ofToString(preLimit.vertex[0])+"\n");
	buf.append(ofToString(preLimit.vertex[1])+"\n");
	buf.append(ofToString(postLimit.vertex[0])+"\n");
	buf.append(ofToString(postLimit.vertex[1])+"\n");
    ofBufferToFile("0.limits",buf);
}
