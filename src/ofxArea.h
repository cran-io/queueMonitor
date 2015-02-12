#pragma once

#define AREA_CORNERS 4

class ofxArea{
public:
	ofPoint vertex[AREA_CORNERS];
	float minX,minY,maxX,maxY;
	ofPixels mask;
	long pixelCount;
	void updateMask(float maskWidth, float maskHeight){
		minX=1.0f; maxX=0.0f; minY=1.0f; maxY=0.0f;
		for(int i=0;i<AREA_CORNERS;i++){
			minX=min(minX,vertex[i].x);
			minY=min(minY,vertex[i].y);
			maxX=max(maxX,vertex[i].x);
			maxY=max(maxY,vertex[i].y);
		}
		minX=max(minX,0.0f); minY=max(minY,0.0f); 
		maxX=min(maxX,1.0f); maxY=min(maxY,1.0f);
		
		ofFbo tempFbo;
		tempFbo.allocate(maskWidth,maskHeight);
		tempFbo.begin();
		ofClear(0);
		ofSetColor(255);
		ofFill();
		ofSetPolyMode(OF_POLY_WINDING_ODD);
		ofScale(maskWidth,maskHeight);
		drawArea();
		tempFbo.end();

		tempFbo.readToPixels(mask);
		mask.setImageType(OF_IMAGE_GRAYSCALE);

		pixelCount=0;
		for(int y=minY*maskHeight;y<(maxY*maskHeight);y++){
			for(int x=minX*maskWidth;x<(maxX*maskWidth);x++){
				if(mask.getPixels()[x+y*mask.getWidth()])
					pixelCount++;
			}
		}
	}
	void drawArea(){
		ofBeginShape();
		for(int i=0;i<AREA_CORNERS;i++){
			ofVertex(vertex[i]);
		}
		ofEndShape(true);
	}
	void drawCorners(){
		for(int i=0;i<AREA_CORNERS;i++){
			ofCircle(vertex[i],0.01f);
		}
	}
};