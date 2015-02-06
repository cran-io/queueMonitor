#include "testApp.h"

#define APP_FPS 5
#define PERSISTENCE_THRESHOLD 10

#define CASE1_POSTLIMIT_DENSITY_THRESHOLD 0.1
#define CASE1_PRELIMIT_DENSITY_THRESHOLD 0.4

#define CASE2_POSTLIMIT_DENSITY_THRESHOLD 0.2
#define CASE2_PRELIMIT_DENSITY_THRESHOLD 0.2

#define QUEUE_THRESHOLD 25

#define LED0 7
#define LED1 27

#define REPORT_TIMEOUT 2.0

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

ofPoint deletionZoneP1,deletionZoneP2;

//--------------------------------------------------------------
void testApp::setup(){
    
	doDebug = ofFile::doesFileExist("DEBUG");

    if(doDebug){
		ofSetLogLevel(OF_LOG_VERBOSE);
		ofSetLogLevel("ofThread", OF_LOG_ERROR);
	}
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

	video.setExposureMode(OMX_ExposureControlAuto);
	video.setWhiteBalance(OMX_WhiteBalControlAuto);

	if(wiringPiSetup() == -1){
        cout<<"Error on wiringPi setup"<<endl;
    }
    else{
        cout<<"Success in wiringPi setup"<<endl;
        pinMode(LED0,OUTPUT);
        pinMode(LED1,OUTPUT);
        digitalWrite(LED0,LOW);
		digitalWrite(LED1,LOW);
        ofSleepMillis(500);
    }
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
	video.setVolume(0.0f);
    video.play();
#endif
    
	threshold = 15;
    
	waitForCamera=10.0; //seconds
	waitForBackground=5.0; //seconds

	background.setLearningTime(100);
	background.setLearningRate(0.0001);
	background.setThresholdValue(threshold);
	
	cameraFrame.allocate(CAMERA_WIDTH,CAMERA_HEIGHT);
	processingFrame.allocate(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	thresholded.allocate(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	//mask.allocate(PROCESSING_WIDTH,PROCESSING_HEIGHT);

	imgBackground.allocate(PROCESSING_WIDTH,PROCESSING_HEIGHT,OF_IMAGE_COLOR);

	blobsTracker.normalizePercentage = 0.5;
	blobsTracker.clearDeletionZones();
	deletionZoneP1.set(PROCESSING_WIDTH-PROCESSING_WIDTH*0.15,0);
	deletionZoneP2.set(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	blobsTracker.addDeletionZone(ofRectangle(deletionZoneP1,deletionZoneP2));

	limitTop.set(PROCESSING_WIDTH/2,0);
	limitBottom.set(PROCESSING_WIDTH/2,PROCESSING_HEIGHT);

	preLimit.vertex.push_back(ofPoint(PROCESSING_WIDTH,0));
	preLimit.vertex.push_back(ofPoint(PROCESSING_WIDTH,PROCESSING_HEIGHT));

	postLimit.vertex.push_back(ofPoint(0,0));
	postLimit.vertex.push_back(ofPoint(0,PROCESSING_HEIGHT));

	preLimitMask.allocate(PROCESSING_WIDTH,PROCESSING_HEIGHT,OF_IMAGE_GRAYSCALE);
	preLimitDensity=0;
	postLimitMask.allocate(PROCESSING_WIDTH,PROCESSING_HEIGHT,OF_IMAGE_GRAYSCALE);
	postLimitDensity=0;

	case1PreLimitOccupation=0;
	case1PostLimitOccupation=0;

	case2PreLimitOccupation=0;
	case2PostLimitOccupation=0;

	loadLimits();

	makeLimitMask();

	queueCounter=0;
	queueFull=false;

#ifndef CRANIO_RPI
	calibrationMode=false;
#endif
    
	ofEnableAlphaBlending();
    ofSetVerticalSync(true);
	ofSetFrameRate(APP_FPS);
	
	reportTimeout=REPORT_TIMEOUT;
	time=ofGetElapsedTimef();
}

//--------------------------------------------------------------
void testApp::update(){
	float t = ofGetElapsedTimef();
	float dt = t - time;
	time = t;

#ifndef CRANIO_RPI
	if(calibrationMode)
		return;

	video.update();
#endif
	if(video.isFrameNew()){
		if(CAMERA_WIDTH!=PROCESSING_WIDTH || CAMERA_HEIGHT!=PROCESSING_HEIGHT){
			cameraFrame.setFromPixels(video.getPixels(),video.getWidth(),video.getHeight());
			processingFrame.scaleIntoMe(cameraFrame);
		}
		else{
			processingFrame.setFromPixels(video.getPixels(),video.getWidth(),video.getHeight());
		}
		background.update(ofxCv::toCv(processingFrame), ofxCv::toCv(thresholded));
		
		thresholded.erode(5);
		thresholded.dilate(10);
		thresholded.blur(21);

		//mask=thresholded;
		//processingFrame&=mask;

		if(doDebug){
			processingFrame.updateTexture();
			thresholded.updateTexture();
			ofxCv::toOf(background.getBackground(),imgBackground);
			imgBackground.update();
		}

		// find contours which are between the size of 1/50 and 1/3 the w*h pixels.
		// also, find holes is set to false so we will not get interior contours.
		contourFinder.findContours(thresholded, (preLimit.pixelCount+postLimit.pixelCount)/10, postLimit.pixelCount+preLimit.pixelCount, 10, false);	// dont find holes
		blobsTracker.update(contourFinder.blobs);

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

		persistenceCalculation(preLimitDensity>CASE1_PRELIMIT_DENSITY_THRESHOLD,case1PreLimitOccupation,PERSISTENCE_THRESHOLD);
		persistenceCalculation(postLimitDensity>CASE1_POSTLIMIT_DENSITY_THRESHOLD,case1PostLimitOccupation,PERSISTENCE_THRESHOLD);

		persistenceCalculation(preLimitDensity>CASE2_PRELIMIT_DENSITY_THRESHOLD,case2PreLimitOccupation,PERSISTENCE_THRESHOLD);
		persistenceCalculation(postLimitDensity>CASE2_POSTLIMIT_DENSITY_THRESHOLD,case2PostLimitOccupation,PERSISTENCE_THRESHOLD);

		if((case1PreLimitOccupation>=PERSISTENCE_THRESHOLD && case1PostLimitOccupation>=PERSISTENCE_THRESHOLD) || (case2PreLimitOccupation>=PERSISTENCE_THRESHOLD && case2PostLimitOccupation>=PERSISTENCE_THRESHOLD)){
			if(queueCounter<(QUEUE_THRESHOLD*4))
				queueCounter++;
		}
		else{
			if(queueCounter>0)
				queueCounter--;
		}

		queueFull=(queueCounter>QUEUE_THRESHOLD);

#ifdef CRANIO_RPI
		digitalWrite(LED0,queueFull?HIGH:LOW);
		digitalWrite(LED1,queueFull?HIGH:LOW);
#endif
	}

	if(waitForCamera){
		waitForCamera-=dt;
		if(waitForCamera<=0){
			background.reset();
			background.setLearningTime(50);
			background.setLearningRate(0.05);
#ifdef CRANIO_RPI
			//Fix camera settings for backgound subtraction
			video.setSharpness(video.getSharpness());
			video.setContrast(video.getContrast());
			video.setBrightness(video.getBrigthness());
			video.setSaturation(video.getSaturation());

			digitalWrite(LED0,LOW);
#endif
			cout<<"Finished waiting for camera."<<endl;
			waitForCamera=0;
		}
		else{
#ifdef CRANIO_RPI
			digitalWrite(LED0,(((int)waitForCamera)%2)?HIGH:LOW);
#endif
		}
	}
	else if(waitForBackground){
		waitForBackground-=dt;
		if(waitForBackground<=0){
			blobsTracker.reset();

			background.setLearningTime(100);
			background.setLearningRate(0.0001);
#ifdef CRANIO_RPI
			digitalWrite(LED1,LOW);
#endif
			cout<<"Finished capturing background."<<endl;
			if(doDebug){
				string filenameTime = ofGetTimestampString();
				ofSaveScreen("screens/"+filenameTime +".png");
				ofSaveImage(imgBackground.getPixelsRef(),"screens/"+filenameTime+"_background.png");
			}
			waitForBackground=0;
		}
		else{
#ifdef CRANIO_RPI
			digitalWrite(LED1,(((int)(waitForBackground*3.0f))%2)?HIGH:LOW);
#endif
		}
	}

	if(reportTimeout){
		reportTimeout-=dt;
		if(reportTimeout<=0){
			cout<<ofGetTimestampString()<<" - "<<ofToString(ofGetFrameRate(),2)<<" FPS - "<<blobsTracker.count<<" PEOPLE."<<endl;
			reportTimeout=REPORT_TIMEOUT;
		}
	}
}

//--------------------------------------------------------------
void testApp::draw(){
	if(queueFull)
		ofBackground(255,0,0);
	else
		ofBackground(255);
	if(doDebug){
		ofPushMatrix();
		ofScale(DRAW_SCALE,DRAW_SCALE);
		ofSetColor(255);
#ifdef CRANIO_RPI
		video.draw();
#else
		if(!calibrationMode)
			video.draw(0,0);
#endif
		ofScale(CAMERA_WIDTH/PROCESSING_WIDTH,CAMERA_HEIGHT/PROCESSING_HEIGHT);
		// draw the incoming, the grayscale, the bg and the thresholded difference
		imgBackground.draw(PROCESSING_WIDTH,0);
		
		//thresholded.draw(670,10,320,240);
		thresholded.draw(0,PROCESSING_HEIGHT);
		contourFinder.draw(0,PROCESSING_HEIGHT);
		blobsTracker.draw(0,PROCESSING_HEIGHT);
		//processingFrame.draw(PROCESSING_WIDTH,PROCESSING_HEIGHT);

		drawAllZones();

		ofPopMatrix();

		stringstream info;
		info << "APP FPS: " << ofToString(ofGetFrameRate(),2) << "\n";
		info << "Camera Resolution: " << video.getWidth() << "x" << video.getHeight()	<< " @ "<< CAMERA_FPS <<"FPS"<< "\n";
		info << "BLOBS: " << contourFinder.nBlobs << "\n";
		info << "PEOPLE COUNT: " << blobsTracker.count << "\n";
		info << "DENSITY: Post: " << ofToString(postLimitDensity,2) << ", Pre: " << ofToString(preLimitDensity,2) << "\n";
		info << "QUEUE COUNTER: " << queueCounter << "\n";
		info << "\n";
		info << "Press space to capture background" << "\n";
		info << "Threshold " << threshold << " (press: UP/DOWN)" << "\n";
		info << "Press p to Toggle pixel processing" << "\n";
		info << "Press r to Toggle pixel reloading" << "\n";
		info << "Press g to Toggle info" << "\n";
		ofDrawBitmapStringHighlight(info.str(), CAMERA_WIDTH*DRAW_SCALE+10, CAMERA_HEIGHT*DRAW_SCALE+20, ofColor::black, ofColor::yellow);
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
	ofTranslate(PROCESSING_WIDTH,0);
	ofFill();
	drawZones();
	ofPopMatrix();

	ofPushMatrix();
	ofTranslate(0,PROCESSING_HEIGHT);
	ofNoFill();
	drawZones();
	ofPopMatrix();
}

//--------------------------------------------------------------
void testApp::drawZones(){
	ofPushStyle();
	ofSetColor(200,200,200,100);
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
	ofSetColor(0,255,0,100);
	ofCircle(limitTop,10);
	ofCircle(limitBottom,10);
	ofCircle(preLimit.vertex[0],10);
	ofCircle(preLimit.vertex[1],10);
	ofCircle(postLimit.vertex[0],10);
	ofCircle(postLimit.vertex[1],10);
	ofCircle(limitMidpoint,5);
	ofSetColor(255,0,0,100);
	ofRect(ofRectangle(deletionZoneP1,deletionZoneP2));
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
	if (key == 'd'){
		doDebug = !doDebug;
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
	ofPoint mouse((x/DRAW_SCALE-CAMERA_WIDTH)/(CAMERA_WIDTH/PROCESSING_WIDTH),(y/DRAW_SCALE)/(CAMERA_WIDTH/PROCESSING_WIDTH));
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
	ofPoint mouse((x/DRAW_SCALE-CAMERA_WIDTH)/(CAMERA_WIDTH/PROCESSING_WIDTH),(y/DRAW_SCALE)/(CAMERA_WIDTH/PROCESSING_WIDTH));
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
#ifndef CRANIO_RPI
	if( dragInfo.files.size() > 0 ){
		calibrationMode=true;
		imgBackground.loadImage(dragInfo.files[0]);
	}
#endif
}

//--------------------------------------------------------------
void testApp::exit(){
	saveLimits();

#ifdef CRANIO_RPI
	digitalWrite(LED0,LOW);
	digitalWrite(LED1,LOW);
#endif
}

//--------------------------------------------------------------
void testApp::loadLimits(){
	if(ofFile::doesFileExist("0.limits")){
		ofBuffer buf = ofBufferFromFile("0.limits");
        string line = buf.getNextLine();
		vector<string> p = ofSplitString(line, ",");
        limitTop.set(ofPoint(ofToFloat(p[0])*PROCESSING_WIDTH,ofToFloat(p[1])*PROCESSING_HEIGHT));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        limitBottom.set(ofPoint(ofToFloat(p[0])*PROCESSING_WIDTH,ofToFloat(p[1])*PROCESSING_HEIGHT));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[0].set(ofPoint(ofToFloat(p[0])*PROCESSING_WIDTH,ofToFloat(p[1])*PROCESSING_HEIGHT));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[1].set(ofPoint(ofToFloat(p[0])*PROCESSING_WIDTH,ofToFloat(p[1])*PROCESSING_HEIGHT));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        postLimit.vertex[0].set(ofPoint(ofToFloat(p[0])*PROCESSING_WIDTH,ofToFloat(p[1])*PROCESSING_HEIGHT));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        postLimit.vertex[1].set(ofPoint(ofToFloat(p[0])*PROCESSING_WIDTH,ofToFloat(p[1])*PROCESSING_HEIGHT));
	}
}

//--------------------------------------------------------------
void testApp::saveLimits(){
	ofVec2f dimensions(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	ofBuffer buf;
    buf.append(ofToString(limitTop/=dimensions)+"\n");
	buf.append(ofToString(limitBottom/=dimensions)+"\n");
	buf.append(ofToString(preLimit.vertex[0]/=dimensions)+"\n");
	buf.append(ofToString(preLimit.vertex[1]/=dimensions)+"\n");
	buf.append(ofToString(postLimit.vertex[0]/=dimensions)+"\n");
	buf.append(ofToString(postLimit.vertex[1]/=dimensions)+"\n");
    ofBufferToFile("0.limits",buf);
}
