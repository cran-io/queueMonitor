#include "testApp.h"

#define APP_FPS 5
#define PERSISTENCE_THRESHOLD 10

#define CASE1_POSTLIMIT_DENSITY_THRESHOLD 0.1
#define CASE1_PRELIMIT_DENSITY_THRESHOLD 0.4

#define CASE2_POSTLIMIT_DENSITY_THRESHOLD 0.2
#define CASE2_PRELIMIT_DENSITY_THRESHOLD 0.2

#define QUEUE_THRESHOLD 25

#define LED0 27
#define LED1 7

#define REPORT_TIMEOUT 5.0

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
	omxCameraSettings.enablePixels = true;
	
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

	ofAddListener(blobsTracker.blobAdded, this, &testApp::blobAdded);
    ofAddListener(blobsTracker.blobUpdated, this, &testApp::blobUpdated);
    ofAddListener(blobsTracker.blobDeleted, this, &testApp::blobDeleted);

	blobsTracker.normalizePercentage = 0.5;
	blobsTracker.clearDeletionZones();
	deletionZoneP1.set(PROCESSING_WIDTH-PROCESSING_WIDTH*0.15,0);
	deletionZoneP2.set(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	blobsTracker.addDeletionZone(ofRectangle(deletionZoneP1,deletionZoneP2));

	preLimit.vertex[0].set(0.5,1.0);
	preLimit.vertex[1].set(0.5,0.0);
	preLimit.vertex[2].set(1.0,0.0);
	preLimit.vertex[3].set(1.0,1.0);

	postLimit.vertex[0].set(preLimit.vertex[0]);
	postLimit.vertex[1].set(preLimit.vertex[1]);
	postLimit.vertex[2].set(0.0,0.0);
	postLimit.vertex[3].set(0.0,1.0);

	loadLimits();

	preLimit.updateMask(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	postLimit.updateMask(PROCESSING_WIDTH,PROCESSING_HEIGHT);

	preLimitDensity=0;
	postLimitDensity=0;

	case1PreLimitOccupation=0;
	case1PostLimitOccupation=0;

	case2PreLimitOccupation=0;
	case2PostLimitOccupation=0;

	peopleCount=0;
	peopleCountLeaving=0;
	peopleCountEntering=0;

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
		unsigned char * preLimitMaskPixels = preLimit.mask.getPixels();
		long preLimitOccupation=0;
		for(int y=preLimit.minY*height;y<(preLimit.maxY*height);y++){
			for(int x=preLimit.minX*width;x<(preLimit.maxX*width);x++){
				if(preLimitMaskPixels[x+y*width])
					preLimitOccupation+=framePixels[x+y*width];
			}
		}
		preLimitOccupation/=255;
		preLimitDensity=preLimitOccupation/(float)preLimit.pixelCount;

		unsigned char * postLimitMaskPixels = postLimit.mask.getPixels();
		long postLimitOccupation=0;
		for(int y=postLimit.minY*height;y<postLimit.maxY*height;y++){
			for(int x=postLimit.minX*width;x<postLimit.maxX*width;x++){
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
			video.setBrightness(video.getBrightness());
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
			peopleCount=0;
			peopleCountEntering=0;
			peopleCountLeaving=0;
			peopleCurrent.clear();
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
			digitalWrite(LED1,(((int)(waitForBackground*4.0f))%2)?HIGH:LOW);
#endif
		}
	}
	else{
#ifdef CRANIO_RPI
		digitalWrite(LED0,((int)(time/3.0f)%2)?LOW:HIGH);
		digitalWrite(LED1,queueFull?HIGH:LOW);
#endif
		if(reportTimeout){
			reportTimeout-=dt;
			if(reportTimeout<=0){
				cout<<ofGetTimestampString()<<" - "<<ofToString(ofGetFrameRate(),2)<<" FPS - "<<blobsTracker.count<<" PEOPLE."<<endl;
				reportTimeout=REPORT_TIMEOUT;
			}
		}
	}
}

//--------------------------------------------------------------
void testApp::blobAdded(ofxBlob &_blob){
    ofLog(OF_LOG_NOTICE, "Blob ID " + ofToString(_blob.id) + " added" );
	int x=_blob.centroid.x;
	int y=_blob.centroid.y;
	if(preLimit.mask.getPixels()[x+y*PROCESSING_WIDTH] || postLimit.mask.getPixels()[x+y*PROCESSING_WIDTH]){
		peopleCount++;
		peopleCurrent.push_back(_blob.id);
	}
}

//--------------------------------------------------------------
void testApp::blobUpdated(ofxBlob &_blob){
    //ofLog(OF_LOG_NOTICE, "Blob ID " + ofToString(_blob.id) + " updated" );
}

//--------------------------------------------------------------
void testApp::blobDeleted(ofxBlob &_blob){
    ofLog(OF_LOG_NOTICE, "Blob ID " + ofToString(_blob.id) + " deleted" );
	for(vector<int>::iterator it=peopleCurrent.begin();it!=peopleCurrent.end();it++){
		if(_blob.id==(*it)){
			ofPoint centroid=_blob.centroid;
			ofRectangle preLimitRect(ofPoint(preLimit.minX*PROCESSING_WIDTH,preLimit.minY*PROCESSING_HEIGHT),ofPoint(preLimit.maxX*PROCESSING_WIDTH,preLimit.maxY*PROCESSING_HEIGHT));
			ofRectangle postLimitRect(ofPoint(postLimit.minX*PROCESSING_WIDTH,postLimit.minY*PROCESSING_HEIGHT),ofPoint(postLimit.maxX*PROCESSING_WIDTH,postLimit.maxY*PROCESSING_HEIGHT));
			if(centroid.distanceSquared(preLimitRect.getCenter())<centroid.distanceSquared(postLimitRect.getCenter()))
				peopleCountEntering++;
			else
				peopleCountLeaving++;
			peopleCurrent.erase(it);
			return;
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

		ofPushMatrix();
		ofScale((float)PROCESSING_WIDTH/CAMERA_WIDTH,(float)PROCESSING_HEIGHT/CAMERA_HEIGHT);
		ofSetColor(255);
#ifdef CRANIO_RPI
		video.draw();
#else
		if(!calibrationMode)
			video.draw(0,0);
#endif
		ofPopMatrix();

		imgBackground.draw(PROCESSING_WIDTH,0);
		thresholded.draw(0,PROCESSING_HEIGHT);
		contourFinder.draw(0,PROCESSING_HEIGHT);
		blobsTracker.draw(0,PROCESSING_HEIGHT);
		drawAllZones();

		ofPopMatrix();

		stringstream info;
		info << "APP FPS: " << ofToString(ofGetFrameRate(),2) << "\n";
		info << "Camera Resolution: " << video.getWidth() << "x" << video.getHeight()	<< " @ "<< CAMERA_FPS <<"FPS"<< "\n";
		info << "CURRENT BLOBS: " << contourFinder.nBlobs << " CURRENT PEOPLE:" << peopleCurrent.size() << "\n";
		info << "PEOPLE BRUTO: " << blobsTracker.count << " PEOPLE NETO: " << peopleCount << "\n";
		info << "ENTERING: " << peopleCountEntering << " LEAVING: " << peopleCountLeaving << "\n";
		info << "DENSITY: Post: " << ofToString(postLimitDensity,2) << ", Pre: " << ofToString(preLimitDensity,2) << "\n";
		info << "QUEUE COUNTER: " << queueCounter << "\n";
		info << "\n";
		info << "Press space to capture background" << "\n";
		info << "Threshold " << threshold << " (press: UP/DOWN)" << "\n";
		info << "Press p to Toggle pixel processing" << "\n";
		info << "Press r to Toggle pixel reloading" << "\n";
		info << "Press g to Toggle info" << "\n";
		ofDrawBitmapStringHighlight(info.str(), PROCESSING_WIDTH*DRAW_SCALE+10, PROCESSING_HEIGHT*DRAW_SCALE+20, ofColor::black, ofColor::yellow);
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
	
	ofPopStyle();
}

//--------------------------------------------------------------
void testApp::drawZones(){
	ofPushMatrix();
	ofScale(PROCESSING_WIDTH,PROCESSING_HEIGHT);
	ofPushStyle();
	ofSetColor(200,200,200,100);
	preLimit.drawArea();
	ofSetColor(0,200,200,100);
	postLimit.drawArea();
	ofSetColor(0,255,0,100);
	preLimit.drawCorners();
	postLimit.drawCorners();
	ofPopMatrix();
	ofSetColor(255,0,0,100);
	ofRect(ofRectangle(deletionZoneP1,deletionZoneP2));
	ofPopStyle();
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	if (key == 'd'){
		doDebug = !doDebug;
	}	    
    if (key == ' ') {
        background.reset();
	}
	if(key == 'r'){
		peopleCount=0;
		peopleCountEntering=0;
		peopleCountLeaving=0;
		peopleCurrent.clear();
		blobsTracker.reset();
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
	ofPoint mouse((x/DRAW_SCALE-PROCESSING_WIDTH)/((float)PROCESSING_WIDTH),(y/DRAW_SCALE)/((float)PROCESSING_HEIGHT));
	if(dragginPoint==0){
		preLimit.vertex[0].set(mouse);
		postLimit.vertex[0].set(preLimit.vertex[0]);
	}
	else if(dragginPoint==1){
		preLimit.vertex[1].set(mouse);
		postLimit.vertex[1].set(preLimit.vertex[1]);
	}
	else if(dragginPoint==2)
		preLimit.vertex[2].set(mouse);
	else if(dragginPoint==3)
		preLimit.vertex[3].set(mouse);
	else if(dragginPoint==4)
		postLimit.vertex[2].set(mouse);
	else if(dragginPoint==5)
		postLimit.vertex[3].set(mouse);
}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){
	ofPoint mouse((x/DRAW_SCALE-PROCESSING_WIDTH)/((float)PROCESSING_WIDTH),(y/DRAW_SCALE)/((float)PROCESSING_HEIGHT));
	if(preLimit.vertex[0].distanceSquared(mouse)<0.01f)
		dragginPoint=0;
	else if(preLimit.vertex[1].distanceSquared(mouse)<0.01f)
		dragginPoint=1;
	else if(preLimit.vertex[2].distanceSquared(mouse)<0.01f)
		dragginPoint=2;
	else if(preLimit.vertex[3].distanceSquared(mouse)<0.01f)
		dragginPoint=3;
	else if(postLimit.vertex[2].distanceSquared(mouse)<0.01f)
		dragginPoint=4;
	else if(postLimit.vertex[3].distanceSquared(mouse)<0.01f)
		dragginPoint=5;
	else
		dragginPoint=-1;
}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
	if(dragginPoint>=0){
		preLimit.updateMask(PROCESSING_WIDTH,PROCESSING_HEIGHT);
		postLimit.updateMask(PROCESSING_WIDTH,PROCESSING_HEIGHT);
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
        preLimit.vertex[0].set(ofPoint(ofToFloat(p[0]),ofToFloat(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[1].set(ofPoint(ofToFloat(p[0]),ofToFloat(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[2].set(ofPoint(ofToFloat(p[0]),ofToFloat(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        preLimit.vertex[3].set(ofPoint(ofToFloat(p[0]),ofToFloat(p[1])));

		postLimit.vertex[0].set(preLimit.vertex[0]);
		postLimit.vertex[1].set(preLimit.vertex[1]);

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        postLimit.vertex[2].set(ofPoint(ofToFloat(p[0]),ofToFloat(p[1])));

		line = buf.getNextLine();
		p = ofSplitString(line, ",");
        postLimit.vertex[3].set(ofPoint(ofToFloat(p[0]),ofToFloat(p[1])));
	}
}

//--------------------------------------------------------------
void testApp::saveLimits(){
	ofBuffer buf;
    buf.append(ofToString(preLimit.vertex[0])+"\n");
	buf.append(ofToString(preLimit.vertex[1])+"\n");
	buf.append(ofToString(preLimit.vertex[2])+"\n");
	buf.append(ofToString(preLimit.vertex[3])+"\n");
	buf.append(ofToString(postLimit.vertex[2])+"\n");
	buf.append(ofToString(postLimit.vertex[3])+"\n");
    ofBufferToFile("0.limits",buf);
}
