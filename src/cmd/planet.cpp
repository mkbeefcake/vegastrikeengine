#include <math.h>
#include "vegastrike.h"
#include "planet.h"
#include "gfxlib.h"
#include "gfx/sphere.h"
#include "collection.h"
#include "gfx/bsp.h"
#include "ai/order.h"
#include "gfxlib_struct.h"
#include "vs_globals.h"
#include "config_xml.h"
#include <assert.h>
#include "cont_terrain.h"
#include "atmosphere.h"
#include "gfx/planetary_transform.h"
#include "collide/rapcol.h"
PlanetaryOrbit:: PlanetaryOrbit(Unit *p, double velocity, double initpos, const Vector &x_axis, const Vector &y_axis, const Vector & centre, Unit * targetunit) : Order(MOVEMENT), parent(p), velocity(velocity), theta(initpos), x_size(x_axis), y_size(y_axis) { 
  parent->SetResolveForces(false);
    double delta = x_size.Magnitude() - y_size.Magnitude();
    if(delta == 0) {
      focus = Vector(0,0,0);
    }
    else if(delta>0) {
      focus = x_size*(delta/x_size.Magnitude());
    } else {
      focus = y_size*(-delta/y_size.Magnitude());
    }
    if (targetunit) {
      type = (MOVEMENT | TARGET);
      UnitCollection tmpcoll;
      tmpcoll.prepend (targetunit);
      AttachOrder (&tmpcoll);
    } else {
      type = (MOVEMENT | LOCATION);
      AttachOrder (centre);
    }
}
PlanetaryOrbit::~PlanetaryOrbit () {
  parent->SetResolveForces (true);
}
void PlanetaryOrbit::Execute() {
  if (done) 
    return;
  Vector x_offset = cos(theta) * x_size;
  Vector y_offset = sin(theta) * y_size;
  Vector origin (targetlocation);
  if (type&TARGET) {
    if (targets) {
      UnitCollection::UnitIterator * tmp = targets->createIterator();
      if (tmp->current()) {
	origin = tmp->current()->Position();
	delete tmp;
      }else {
	delete tmp;
	done = true;
	return;
      }
    }
  }
  double radius =  sqrt((x_offset - focus).MagnitudeSquared() + (y_offset - focus).MagnitudeSquared());
  theta+=velocity/(radius?radius:1) *SIMULATION_ATOM;
  parent->Velocity = (origin - focus + x_offset+y_offset-parent->curr_physical_state.position)/SIMULATION_ATOM;
}


void Planet::endElement() {  
}
Planet * Planet::GetTopPlanet (int level) {
  if (level>2) {
    UnitCollection::UnitIterator * satiterator = satellites.createIterator();
	  assert(satiterator->current()!=NULL);
	  if (satiterator->current()->isUnit()==PLANETPTR) {
	    return ((Planet *)satiterator->current())->GetTopPlanet (level-1);
	  } else {
	    fprintf (stderr,"Planets are unable to orbit around units");
	    return NULL;
	  }

  } else {
    return this;
  }
  
}
void Planet::AddSatellite (Unit * orbiter) {
	satellites.prepend (orbiter);
	orbiter->SetOwner (this);
}
void Planet::beginElement(Vector x,Vector y,float vely,float pos,float gravity,float radius,char * filename,char * alpha,vector<char *> dest,int level,  const GFXMaterial & ourmat, const vector <GFXLightLocal>& ligh, bool isunit, int faction){
  UnitCollection::UnitIterator * satiterator =NULL;
  if (level>2) {
    UnitCollection::UnitIterator * satiterator = satellites.createIterator();
	  assert(satiterator->current()!=NULL);
	  if (satiterator->current()->isUnit()==PLANETPTR) {
		((Planet *)satiterator->current())->beginElement(x,y,vely,pos,gravity,radius,filename,alpha,dest,level-1,ourmat,ligh, isunit, faction);
	  } else {
	    fprintf (stderr,"Planets are unable to orbit around units");
	  }
  } else {
    if (isunit==true) {
      satellites.prepend(new Unit (filename, true, false, faction));
      satiterator = satellites.createIterator();
      satiterator->current()->SetAI (new PlanetaryOrbit (satiterator->current(),vely,pos,x,y, Vector (0,0,0), this)) ;
      satiterator->current()->SetOwner (this);
    }else {
      satellites.prepend(new Planet(x,y,vely,pos,gravity,radius,filename,alpha,dest, Vector (0,0,0), this, ourmat, ligh, faction));
    }
  }
  delete satiterator;
}

const float densityOfRock = .01; // 1 cm of durasteel equiv per cubic meter

Planet::Planet()  : Unit(),  atmosphere (NULL), terrain (NULL), radius(0.0f), satellites() {
  inside=false;
  Init();
  terraintrans = NULL;
  SetAI(new Order()); // no behavior
}

Planet::Planet(Vector x,Vector y,float vely, float pos,float gravity,float radius,char * textname,char * alpha,vector <char *> dest, const Vector & orbitcent, Unit * parent, const GFXMaterial & ourmat, const std::vector <GFXLightLocal> &ligh, int faction) : Unit(), atmosphere(NULL), terrain(NULL), radius(0.0f),  satellites() {
  inside =false;
  for (unsigned int i=0;i<ligh.size();i++) {
    int l;
    GFXCreateLight (l,ligh[i].ligh,!ligh[i].islocal);
    lights.push_back (l);
  }
  curr_physical_state.position = prev_physical_state.position=cumulative_transformation.position=orbitcent+x;
  destination=dest;
  Init();
  this->faction = faction;
  killed=false;
  name = "Planet - ";
  name += textname;
  this->radius=radius;
  this->gravity=gravity;
  hull = (4./3)*M_PI*radius*radius*radius*densityOfRock;
  SetAI(new PlanetaryOrbit(this, vely, pos, x, y, orbitcent, parent)); // behavior
  terraintrans=NULL;
  meshdata = new Mesh*[2];
  static int stacks=XMLSupport::parse_int(vs_config->getVariable ("graphics","planet_detail","24"));
  meshdata[0] = new SphereMesh(radius, stacks, stacks, textname, alpha,false,alpha!=NULL?SRCALPHA:ONE,alpha!=NULL?INVSRCALPHA:ZERO);
  meshdata[0]->setEnvMap(GFXFALSE);
  meshdata[0]->SetMaterial (ourmat);
  nummesh = 1;

  calculate_extent();

  /*stupid Sphere BSP when intersection should do
  string tmpname ("sphere");
  char temp [64];
  sprintf (temp, "%f", radius);
  tmpname +=temp;
  tmpname +=".bsp";
  
  FILE * fp = fopen (tmpname.c_str(), "rb");
  if (!fp) {
  */
#ifdef RAPIDCOLLIDEPLANET
  meshdata[1]= new SphereMesh (radius,8,8,textname, alpha);
  std::vector <bsp_polygon> spherepolys;
  meshdata[1]->GetPolys (spherepolys);
  colTree = new csRapidCollider (spherepolys);
  //BuildBSPTree (tmpname.c_str(),true,meshdata[1]);
  delete meshdata[1];
#else
  colTree= NULL;
#endif
    /*
      } else {
      fclose (fp);
      }
      bspTree = new BSPTree (tmpname.c_str());
  */
  meshdata[1]=NULL;
}
extern bool shouldfog;
void Planet::Draw(const Transformation & quat, const Matrix m) {
  //Do lighting fx
  // if cam inside don't draw?
  //  if(!inside) {
  Unit::Draw(quat,m);
  //  }

  if (inside&&terrain) {
    _Universe->AccessCamera()->UpdatePlanetGFX();
    //    Camera * cc = _Universe->AccessCamera();
    //    VectorAndPositionToMatrix (tmp,cc->P,cc->Q,cc->R,cc->GetPosition()+cc->R*100);
    terrain->SetTransformation (_Universe->AccessCamera()->GetPlanetGFX());
    terrain->AdjustTerrain(_Universe->activeStarSystem());
    terrain->Draw();
#ifdef PLANETARYTRANSFORM

    terraintrans->GrabPerpendicularOrigin (_Universe->AccessCamera()->GetPosition(),tmp);
    terrain->SetTransformation (tmp);
    terrain->AdjustTerrain(_Universe->activeStarSystem());
    terrain->Draw();
    if (atmosphere) {
      Vector tup (tmp[4],tmp[5],tmp[6]);
      Vector p = (_Universe->AccessCamera()->GetPosition());
      Vector blah = p-Vector (tmp[12],tmp[13],tmp[14]);
      blah = p - (blah.Dot (tup))*tup;
      tmp[12]=blah.i;
      tmp[13]=blah.j;
      tmp[14]=blah.k;      
      atmosphere->SetMatricesAndDraw (_Universe->AccessCamera()->GetPosition(),tmp);
    }
#endif
  }
    
  GFXLoadIdentity (MODEL);
  for (unsigned int i=0;i<lights.size();i++) {
    GFXSetLight (lights[i], POSITION,GFXColor (cumulative_transformation.position));
  }
  Vector t (_Universe->AccessCamera()->GetPosition()-Position());
  static int counter=0;
  if (counter ++>100)
  if (t.Magnitude()<corner_max.i) {
    if (!inside) {
      _Universe->AccessCamera()->SetPlanetaryTransform (terraintrans);
      inside =true;
      if (terrain)
	terrain->EnableUpdate();
#ifdef PLANETARYTRANSFORM
      TerrainUp = t;
      Normalize(TerrainUp);
      TerrainH = TerrainUp.Cross (Vector (-TerrainUp.i+.25, TerrainUp.j-.24,-TerrainUp.k+.24));
      Normalize (TerrainH);
#endif
    }

    //    shouldfog=true;
  } else {
    if (inside) {
      _Universe->AccessCamera()->SetPlanetaryTransform (NULL);
      //if ((terrain&&t.Dot (TerrainH)>corner_max.i)||(!terrain&t.Dot(t)>corner_max.i*corner_max.i)) {
      inside=false;
      ///somehow warp unit to reasonable place outisde of planet
      if (terrain) {
#ifdef PLANETARYTRANSFORM
	terrain->DisableUpdate();
#endif
      }
    }
  }
  
}





void Planet::reactToCollision(Unit * un, const Vector & biglocation, const Vector & bignormal, const Vector & smalllocation, const Vector & smallnormal, float dist) {
  if (terrain&&un->isUnit()!=PLANETPTR) {
    un->SetPlanetOrbitData (terraintrans);
    Matrix top;
    Identity(top);
    Vector posRelToTerrain = terraintrans->InvTransform(un->LocalPosition());
    top[12]=un->Position().i- posRelToTerrain.i;
    top[13]=un->Position().j- posRelToTerrain.j;
    top[14]=un->Position().k- posRelToTerrain.k;

#ifdef PLANETARYTRANSFORM

    terraintrans->GrabPerpendicularOrigin(un->Position(),top);
    static int tmp=0;
    /*    if (tmp) {
      terrain->SetTransformation (top);
      terrain->AdjustTerrain (_Universe->activeStarSystem());
      terrain->Collide (un);
      }else {*/

      //    }
#endif
    terrain->Collide (un,top);      
  }
  if (!destination.empty()&&un->JumpDrive()>=0) {
    un->DeactivateJumpDrive();
    _Universe->activeStarSystem()->JumpTo (un, std::string(destination[un->JumpDrive()%destination.size()]));
  }
#if 0
  //screws with earth having an atmosphere... blahrgh
  if (!terrain&&destination.empty()) {//no place to go and acts like a ship
    Unit::reactToCollision (un,biglocation,bignormal,smalllocation,smallnormal,dist);
  }
#endif
  //nothing happens...you fail to do anythign :-)
  //maybe air reisstance here? or swithc dynamics to atmos mode
}

void Planet::EnableLights () {
  for (unsigned int i=0;i<lights.size();i++) {
    GFXEnableLight (lights[i]);
  }  
}
void Planet::DisableLights () {
  for (unsigned int i=0;i<lights.size();i++) {
    GFXDisableLight (lights[i]);
  }
}
Planet::~Planet() { 
  if (terrain) {
    delete terrain;
  }
  if (atmosphere){
     delete atmosphere;
  }
	unsigned int i;
	if (bspTree)
	  delete bspTree;
	for (i=0;i<this->destination.size();i++) {
		delete [] destination[i];
	}
	if (terraintrans) {
	  float * tmp = (float *) malloc (sizeof(float)*16);
	  memcpy (tmp,cumulative_transformation_matrix,sizeof(float)*16);
	  terraintrans->SetTransformation (tmp);
	  //FIXME
	  //We're losing memory here...but alas alas... planets don't die that often
	}
}

PlanetaryTransform *Planet::setTerrain (ContinuousTerrain * t) {
  terrain = t;
  terrain->DisableDraw();
  float x,z;
  t->GetTotalSize (x,z);
  terraintrans = new PlanetaryTransform (.8*corner_max.i,x*2,z,4);
  terraintrans->SetTransformation (cumulative_transformation_matrix);
  return terraintrans;
}
void Planet::setAtmosphere (Atmosphere *t) {
  atmosphere = t;
}


void Planet::Kill() {
	UnitCollection::UnitIterator * iter;
	Unit *tmp;
	for (iter = satellites.createIterator();
	     (tmp = iter->current())!=NULL;
	     iter->advance()) {
	  tmp->SetAI (new Order);
	}
	delete iter;
	for (unsigned int i=0;i<this->lights.size();i++) {
	  GFXDeleteLight (lights[i]);
	}

	Unit::Kill();
}

void Planet::gravitate(UnitCollection *uc) {
  /*
  float *t = cumulative_transformation_matrix;

  
  if(gravity!=0.0&&uc) {
    Iterator *iterator = uc->createIterator();
    Unit *unit;
    Vector vec(0,0,0);
    
    while((unit = iterator->current())!=NULL) {
      if(unit->type()!=PLANETPTR) {
	Vector r = (unit->Position() - (vec.Transform(t)));
	//      cerr << "Unit (" << unit << "): " << endl;
	//      cerr << "Gravity source: " << vec.Transform(t) << "\nUnit position: " << unit->Position() << "\nDelta: " << r << endl;
	float _r_ = r.Magnitude();
	r = r * (1.0/_r_);
	r =  r * -(gravity/(_r_*_r_));
	//      cerr << "Distance: " << _r_ << "\nGravity force vector: " << r << endl;
      
	if(_r_ > radius) {
	  unit->Accelerate(r);
	}
      }
      iterator->advance();
    }
    delete iterator;
    }*/

  // fake gravity
  /***FIXME 091401 why do we need to traverse satellites??????
  UnitCollection::UnitIterator * iter;
  for (iter = satellites.createIterator();
       iter->current()!=NULL;
       iter->advance()) {
	if (iter->current()->isUnit()==PLANETPTR) 
	    ((Planet *)iter->current())->gravitate(uc);//FIXME 071201
	else { //FIXME...[causes flickering for crashing orbiting units
	
	  //((Unit *)iter->current())->ResolveForces (identity_transformation,identity_matrix,false); 
	  ((Unit *)iter->current())->UpdateCollideQueue();
	}
  }
  delete iter;
  **/
  UpdateCollideQueue();
}
