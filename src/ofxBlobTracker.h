#pragma once

#include "ofMain.h"
#include "ofxOpenCv.h"

#define DISTANCE_THRESHOLD 100
#define DISTANCE_THRESHOLD_SQUARED (DISTANCE_THRESHOLD*DISTANCE_THRESHOLD)

#define AREA_THRESHOLD 0.1
#define WIDTH_THRESHOLD 0.25
#define HEIGHT_THRESHOLD 0.5
#define SHAPE_THRESHOLD 0.25
#define SHAPE_THRESHOLD_SQUARED (SHAPE_THRESHOLD*SHAPE_THRESHOLD)

#define LIFE_THRESHOLD 3.0f
#define AFTERLIFE 1.0f
#define AFTERLIFE_PLUS 4.0f

#define FRAGMENTED_SCALE 1.1
#define UNION_SCALE 0.9

typedef struct ofxBlob : public ofxCvBlob {
	ofxBlob(ofxCvBlob& _blob, int _id=0){
		area=_blob.area;
        length=_blob.length;
        boundingRect=_blob.boundingRect;
        centroid=_blob.centroid;
        hole=_blob.hole;
		pts=_blob.pts;
        nPts=_blob.nPts;
		id=_id;
		life=0.0f;
		velocity.set(0,0);
		active=true;
		updated=false;
		deletion=false;
		counted=false;
	};

	int id;
	float life;
	ofVec2f velocity;
	bool active,updated,deletion;
	bool counted;
	vector<int> fragmented;
};

typedef struct ofxNewBlob : public ofxCvBlob{
	ofxNewBlob(ofxCvBlob& _blob, bool _taken=false){
		area=_blob.area;
        length=_blob.length;
        boundingRect=_blob.boundingRect;
        centroid=_blob.centroid;
        hole=_blob.hole;
		pts=_blob.pts;
        nPts=_blob.nPts;
		taken=_taken;
	};
	bool taken;
	vector<int> united;
};

typedef struct ofxBlobMatch{
	int blobIndex;
	int newBlobIndex;
};

class ofxBlobTracker{
public:
	ofxBlobTracker(){
		time=0;
		normalizePercentage=1;
		id=0;
		count=0;
	}
	// by what percentage the blob movement will be normalized
	float normalizePercentage;
	//increasing id
	int id;
	//people count
	int count;
	// the final resulting blobs
	vector<ofxBlob> blobs;
	// main update function
	void update(vector<ofxCvBlob>& _newBlobs){
		float t = ofGetElapsedTimef();
		float dt = t - time;
		time = t;

		resetBlobs();

		vector<ofxNewBlob> newBlobs;
		for(int i=0;i<_newBlobs.size();i++)
			newBlobs.push_back(ofxNewBlob(_newBlobs[i],false));

		map<float,ofxBlobMatch> grafo;
		for(int blobIndex=0;blobIndex<blobs.size();blobIndex++){
			for(int newBlobIndex=0;newBlobIndex<newBlobs.size();newBlobIndex++){
				float distance = blobs[blobIndex].centroid.distanceSquared(newBlobs[newBlobIndex].centroid);
				if(distance<DISTANCE_THRESHOLD_SQUARED){
					ofxBlobMatch match; match.blobIndex=blobIndex; match.newBlobIndex=newBlobIndex; 
					grafo.insert(pair<float,ofxBlobMatch>(distance,match));
				}
			}
		}

		for(map<float,ofxBlobMatch>::iterator it = grafo.begin(); it!=grafo.end(); it++){
			int blobIndex = (it->second).blobIndex;
			int newBlobIndex = (it->second).newBlobIndex;
			if(!blobs[blobIndex].updated && !newBlobs[newBlobIndex].taken){
				float areaDiff = abs(blobs[blobIndex].area-newBlobs[newBlobIndex].area)/blobs[blobIndex].area;
				float widthDiff = abs(blobs[blobIndex].boundingRect.width-newBlobs[newBlobIndex].boundingRect.width)/blobs[blobIndex].boundingRect.width;
				float heightDiff = abs(blobs[blobIndex].boundingRect.height-newBlobs[newBlobIndex].boundingRect.height)/blobs[blobIndex].boundingRect.height;
				float shapeDiff = ofVec2f(blobs[blobIndex].boundingRect.width,blobs[blobIndex].boundingRect.height).distanceSquared(ofVec2f(newBlobs[newBlobIndex].boundingRect.width,newBlobs[newBlobIndex].boundingRect.height))/ofVec2f(blobs[blobIndex].boundingRect.width,blobs[blobIndex].boundingRect.height).lengthSquared();

				if(true){//areaDiff<AREA_THRESHOLD && widthDiff<WIDTH_THRESHOLD && heightDiff<HEIGHT_THRESHOLD && shapeDiff<SHAPE_THRESHOLD_SQUARED){
					updateBlob(dt,blobs[blobIndex],newBlobs[newBlobIndex]);
					blobs[blobIndex].updated=true;
					newBlobs[newBlobIndex].taken=true;
				}
				else{
					cout<<"Morphology difference."<<endl;
					if(areaDiff<AREA_THRESHOLD)
						cout<<"Area"<<endl;
					if(widthDiff<WIDTH_THRESHOLD)
						cout<<"Width"<<endl;
					if(heightDiff<HEIGHT_THRESHOLD)
						cout<<"Height"<<endl;
					if(shapeDiff<SHAPE_THRESHOLD_SQUARED)
						cout<<"Shape"<<endl;
				}
			}
		}

		for(int newBlobIndex=0;newBlobIndex<newBlobs.size();newBlobIndex++){
			if(!newBlobs[newBlobIndex].taken){
				//LOOK FOR UNION OF BLOBS
				for(int blobIndex=0;blobIndex<blobs.size();blobIndex++){
					if(!blobs[blobIndex].updated){
						if(isContained(newBlobs[newBlobIndex].boundingRect,blobs[blobIndex].boundingRect)){
							newBlobs[newBlobIndex].united.push_back(blobIndex);
						}
					}
				}

				if(newBlobs[newBlobIndex].united.size()){
					if(newBlobs[newBlobIndex].united.size()==1){
						updateBlob(dt,blobs[newBlobs[newBlobIndex].united[0]],newBlobs[newBlobIndex]);
						blobs[newBlobs[newBlobIndex].united[0]].updated=true;
						newBlobs[newBlobIndex].taken=true;
					}
					else if(newBlobs[newBlobIndex].united.size()>1){
						cout<<"New blob "<<newBlobIndex<<" is union from:";
						for(int i=0;i<newBlobs[newBlobIndex].united.size();i++){
							blobs[newBlobs[newBlobIndex].united[i]].updated=true;
							cout<<" "<<blobs[newBlobs[newBlobIndex].united[i]].id;
						}
						cout<<"."<<endl;
						newBlobs[newBlobIndex].taken=true;
					}
				}
				else{
					//LOOK FOR FRAGMENTATIONS OF BLOBS
					for(int blobIndex=0;blobIndex<blobs.size();blobIndex++){
						if(!blobs[blobIndex].updated){
							if(isContained(blobs[blobIndex].boundingRect,newBlobs[newBlobIndex].boundingRect)){
								blobs[blobIndex].fragmented.push_back(newBlobIndex);
							}
						}
					}
				}
			}
		}

		for(int blobIndex=0;blobIndex<blobs.size();blobIndex++){
			if(blobs[blobIndex].fragmented.size()){
				if(blobs[blobIndex].fragmented.size()==1){
					updateBlob(dt,blobs[blobIndex],newBlobs[blobs[blobIndex].fragmented[0]]);
					blobs[blobIndex].updated=true;
					newBlobs[blobs[blobIndex].fragmented[0]].taken=true;
				}
				else if(blobs[blobIndex].fragmented.size()>1){
					if(blobs[blobIndex].life>LIFE_THRESHOLD){
						cout<<"Fragmentation from: "<<blobs[blobIndex].id;
						updateBlob(dt,blobs[blobIndex],newBlobs[blobs[blobIndex].fragmented[0]]);
						blobs[blobIndex].updated=true;
						newBlobs[blobs[blobIndex].fragmented[0]].taken=true;
						for(int i=1;i<blobs[blobIndex].fragmented.size();i++){
							blobs.push_back(ofxBlob(newBlobs[blobs[blobIndex].fragmented[i]],id++));
							blobs.back().life=blobs[blobIndex].life;
							blobs.back().updated=true;
							newBlobs[blobs[blobIndex].fragmented[i]].taken=true;
							cout<<" "<<id;
						}
						cout<<"."<<endl;
					}
				}
			}
		}

		for(int blobIndex=0;blobIndex<blobs.size();blobIndex++){
			if(!blobs[blobIndex].updated){
				if(blobs[blobIndex].life<LIFE_THRESHOLD){
					blobs[blobIndex].deletion=true;
				}
				else if(blobs[blobIndex].active){
					blobs[blobIndex].life=min(blobs[blobIndex].life,LIFE_THRESHOLD+AFTERLIFE);
					if(!isInsideDeletionLimits(blobs[blobIndex].centroid))
						blobs[blobIndex].life+=AFTERLIFE_PLUS;
					blobs[blobIndex].active=false;
				}
				blobs[blobIndex].life-=dt;
			}
		}

		for(int newBlobIndex=0;newBlobIndex<newBlobs.size();newBlobIndex++){
			if(!newBlobs[newBlobIndex].taken){
				blobs.push_back(ofxBlob(newBlobs[newBlobIndex],id++));
			}
		}

		for (vector<ofxBlob>::iterator it=blobs.begin(); it!=blobs.end();/*it++*/){
			if(it->deletion)
				it = blobs.erase(it);
			else 
				++it;
		}
	}

	void updateBlob(float dt,ofxBlob& blob, ofxNewBlob& newBlob){
		blob.area=newBlob.area;
        blob.length=newBlob.length;
        blob.boundingRect=newBlob.boundingRect;
        blob.centroid=newBlob.centroid;
        blob.hole=newBlob.hole;
		blob.pts=newBlob.pts;
        blob.nPts=newBlob.nPts;
		blob.life+=dt;
		blob.active=true;

		if(!blob.counted && blob.life>LIFE_THRESHOLD){
			count++;
			blob.counted=true;
		}
	}

	void resetBlobs(){
		for(int i=0;i<blobs.size();i++){
			blobs[i].updated=false;
			blobs[i].fragmented.clear();
		}
	}

	void addDeletionZone(ofRectangle rect){
		deletionZones.push_back(rect);
	}
	
	void clearDeletionZones(){
		deletionZones.clear();
	}

	bool isInsideDeletionLimits(ofPoint pos){
		for(int i=0;i<deletionZones.size();i++){
			if(deletionZones[i].inside(pos))
				return true;
		}
		return false;
	}

	bool isContained(ofRectangle fragmented, ofRectangle united){
		fragmented.scaleFromCenter(FRAGMENTED_SCALE);
		united.scaleFromCenter(UNION_SCALE);
		return fragmented.inside(united);
	}

	void draw(int x, int y){
		ofPushMatrix();
		ofTranslate(x,y);
		ofPushStyle();
		ofSetColor(255);
		for(int blobIndex=0;blobIndex<blobs.size();blobIndex++){
			ofColor back;
			if(blobs[blobIndex].life<LIFE_THRESHOLD)
				back.set(255,255,0);
			else
				back.set(0,255,255);
			ofDrawBitmapStringHighlight(ofToString(blobs[blobIndex].id),blobs[blobIndex].centroid,back,ofColor::black);
		}
		ofPopStyle();
		ofPopMatrix();
	}

private:
	float time;

	vector<ofRectangle> deletionZones;
};