#include "vegastrike.h"
#include "gfx/mesh.h"
#include "cmd_unit.h"
#include "gfx/bounding_box.h"
extern Vector mouseline;
extern vector<Vector> perplines;
Vector MouseCoordinate (int mouseX, int mouseY);
float Unit::getMinDis (const Vector &pnt) {
  float minsofar=1e+10;
  float tmpvar;
  int i;
  Vector TargetPoint (cumulative_transformation_matrix[0],cumulative_transformation_matrix[1],cumulative_transformation_matrix[2]);

#ifdef VARIABLE_LENGTH_PQR
  float SizeScaleFactor = sqrtf(TargetPoint.Dot(TargetPoint)); //the scale factor of the current UNIT
#endif
  for (i=0;i<nummesh;i++) {

    TargetPoint = Transform(cumulative_transformation_matrix,meshdata[i]->Position())-pnt;
    tmpvar = sqrtf (TargetPoint.Dot (TargetPoint))-meshdata[i]->rSize()
#ifdef VARIABLE_LENGTH_PQR
	*SizeScaleFactor
#endif
      ;
    if (tmpvar<minsofar) {
      minsofar = tmpvar;
    }
  }
  for (i=0;i<numsubunit;i++) {
    tmpvar = subunits[i]->getMinDis (pnt);
    if (tmpvar<minsofar) {
      minsofar=tmpvar;
    }			       
  }
  return minsofar;
}

float Unit::querySphere (const Vector &st, const Vector &dir, float err) {
  int i;
  float retval=0;
  float adjretval=0;
  float * tmpo = cumulative_transformation_matrix;

  Vector TargetPoint (tmpo[0],tmpo[1],tmpo[2]);
  for (i=0;i<nummesh;i++) {
    TargetPoint = Transform (tmpo,meshdata[i]->Position());
    Vector origPoint = TargetPoint;

    perplines.push_back(TargetPoint);
    //find distance away from the line now :-)
    //find scale factor of end on start to get line.
    Vector tst = TargetPoint-st;
    //Vector tst = TargetPoint;
    float k = tst.Dot (dir);
    TargetPoint = tst - k*(dir);
    /*
    cerr << origPoint << "-" << st << " = " << tst << " projected length " << k << " along direction " << dir << endl;
    cerr << "projected line " << st << " - " << st + k*dir << endl;
    cerr << "length of orthogonal projection " << TargetPoint.Magnitude() << ", " << "radius " << meshdata[i]->rSize() << endl;
    */
    perplines.push_back(origPoint-TargetPoint);
    
    ///      fprintf (stderr, "i%f,j%f,k%f end %f,%f,%f>, k %f distance %f, rSize %f\n", st.i,st.j,st.k,end.i,end.j,end.k,k,TargetPoint.Dot(TargetPoint), meshdata[i]->rSize());    
    
    if (TargetPoint.Dot (TargetPoint)< 
	err*err+
	meshdata[i]->rSize()*meshdata[i]->rSize()+2*err*meshdata[i]->rSize()
	)
      {
	if (retval==0) {
	  retval = k;
	  adjretval=k;
	  if (adjretval<0) {
		adjretval+=meshdata[i]->rSize();
		if (adjretval>0)
				adjretval=.001;
		}
	}else {
		if (retval>0&&k<retval&&k>-meshdata[i]->rSize()){
			retval = k;
			adjretval=k;
			if (adjretval<0) {
				adjretval+=meshdata[i]->rSize();
				if (adjretval>0)
					adjretval=.001;
			}
		}
		if (retval<0&&k+meshdata[i]->rSize()>retval) {
			retval = k;
			adjretval=k+meshdata[i]->rSize();
			if (adjretval>0)
				adjretval=.001;//THRESHOLD;
		}
	}
    }
  }
  for (i=0;i<numsubunit;i++) {
    float tmp = (subunits[i]->querySphere (st,dir,err));
    if (tmp==0) continue;
    if (retval==0) {
      retval = tmp;
    }else{
		if (adjretval>0&&tmp<adjretval) {
			retval = tmp;
			adjretval=tmp;
		}
		if (adjretval<0&&tmp>adjretval) {
		    retval = tmp;
			adjretval=tmp;
		}
    }
  }
  return adjretval;
}



bool Unit::queryBoundingBox (const Vector &pnt, float err) {
  int i;
  BoundingBox * bbox=NULL;
  for (i=0;i<nummesh;i++) {
    bbox = meshdata[i]->getBoundingBox();
    bbox->Transform (cumulative_transformation_matrix);
    if (bbox->Within(pnt,err)) {
      delete bbox;
      return true;
    }
    delete bbox;
  }
  for (i=0;i<numsubunit;i++) {
    if (subunits[i]->queryBoundingBox (pnt,err)) 
      return true;
  }
  return false;
}

int Unit::queryBoundingBox (const Vector &origin, const Vector &direction, float err) {
  int i;
  int retval=0;
  BoundingBox * bbox=NULL;
  for (i=0;i<nummesh;i++) {
    bbox = meshdata[i]->getBoundingBox();
    bbox->Transform (cumulative_transformation_matrix);
    switch (bbox->Intersect(origin,direction,err)) {
    case 1:delete bbox;
      return 1;
    case -1:delete bbox;
      retval =-1;
      break;
    case 0: delete bbox;
      break;
    }
  }
  for (i=0;i<numsubunit;i++) {
    switch (subunits[i]->queryBoundingBox (origin,direction,err)) {
    case 1: return 1;
    case -1: retval= -1;
      break;
    case 0: break;
    }
  }
  return retval;
}


bool Unit::querySphere (int mouseX, int mouseY, float err, Camera * activeCam) {
  int i;
  Matrix vw;
  _Universe->AccessCamera()->GetView (vw);
  Vector mousePoint;
#ifdef VARIABLE_LENGTH_PQR
  Vector TargetPoint (cumulative_transformation_matrix[0],cumulative_transformation_matrix[1],cumulative_transformation_matrix[2]);
  float SizeScaleFactor = sqrtf(TargetPoint.Dot(TargetPoint));
#else
  Vector TargetPoint;
#endif

  Vector CamP,CamQ,CamR;
  for (i=0;i<nummesh;i++) {
    //cerr << "pretransform position: " << meshdata[i]->Position() << endl;
    TargetPoint = Transform(cumulative_transformation_matrix,meshdata[i]->Position());
    
    mousePoint = Transform (vw,TargetPoint);
    if (mousePoint.k>0) { //z coordinate reversed  -  is in front of camera
      continue;
    }
    mousePoint = MouseCoordinate (mouseX,mouseY);
    
    activeCam->GetPQR(CamP,CamQ,CamR);
    mousePoint = Transform (CamP,CamQ,CamR,mousePoint);	
    activeCam->GetPosition(CamP);    
    mousePoint +=CamP; 
    
    
    
    TargetPoint =TargetPoint-mousePoint;
    if (TargetPoint.Dot (TargetPoint)< 
	err*err+
	meshdata[i]->rSize()*meshdata[i]->rSize()
#ifdef VARIABLE_LENGTH_PQR
	*SizeScaleFactor*SizeScaleFactor
#endif
	+
#ifdef VARIABLE_LENGTH_PQR
	SizeScaleFactor*
#endif
	2*err*meshdata[i]->rSize()
	)
      return true;
  }
  for (i=0;i<numsubunit;i++) {
    if (subunits[i]->querySphere (mouseX,mouseY,err,activeCam))
      return true;
  }
  return false;
}
