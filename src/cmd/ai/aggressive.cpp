#include "aggressive.h"
#include "event_xml.h"
#include "script.h"
#include <list>
#include <vector>
#include "vs_globals.h"
#include "config_xml.h"
#include "xml_support.h"
#include "cmd/unit.h"
#include "communication.h"
#include "cmd/script/flightgroup.h"
using namespace Orders;

const EnumMap::Pair element_names[] = {
  EnumMap::Pair ("AggressiveAI" , AggressiveAI::AGGAI),
  EnumMap::Pair ("UNKNOWN", AggressiveAI::UNKNOWN),
  EnumMap::Pair ("Distance", AggressiveAI::DISTANCE),
  EnumMap::Pair ("MeterDistance", AggressiveAI::METERDISTANCE),
  EnumMap::Pair ("Threat", AggressiveAI::THREAT),
  EnumMap::Pair ("FShield", AggressiveAI::FSHIELD),
  EnumMap::Pair ("LShield",AggressiveAI:: LSHIELD),
  EnumMap::Pair ("RShield", AggressiveAI::RSHIELD),
  EnumMap::Pair ("BShield", AggressiveAI::BSHIELD),
  EnumMap::Pair ("Hull", AggressiveAI::HULL),
  EnumMap::Pair ("Facing", AggressiveAI::FACING),
  EnumMap::Pair ("Movement", AggressiveAI::MOVEMENT),
  EnumMap::Pair ("Rand", AggressiveAI::RANDOMIZ)
};
const EnumMap AggressiveAIel_map(element_names, 12);
static float aggressivity=2.01;
AggressiveAI::AggressiveAI (const char * filename, const char * interruptname, Unit * target):FireAt(.2,2), logic (AggressiveAIel_map), interrupts (AggressiveAIel_map) {
  if (aggressivity==2.01)
    aggressivity = XMLSupport::parse_float (vs_config->getVariable ("unit","aggressivity","2"));
  if (target !=NULL) {
    AttachOrder (target);
  }
  AIEvents::LoadAI (filename,logic);
  AIEvents::LoadAI (interruptname,interrupts);
}

bool AggressiveAI::ExecuteLogicItem (const AIEvents::AIEvresult &item) {
  
  if (item.script.length()!=0) {
    Order * tmp = new AIScript (item.script.c_str());	
    //    parent->EnqueueAI (tmp);
    EnqueueOrder (tmp);
    return true;
  }else {
    return false;
  }
}


bool AggressiveAI::ProcessLogicItem (const AIEvents::AIEvresult &item) {
  float value;
  switch (abs(item.type)) {
  case DISTANCE:
    value = distance;
    break;
  case METERDISTANCE:
    {
      Unit * targ = parent->Target();
      if (targ) {
	value = (parent->Position()-targ->Position()).Magnitude()-parent->rSize()-targ->rSize();
      }else {
	value = 10000; 
      }
    }
    break;
  case THREAT:
    value = parent->GetComputerData().threatlevel;
    break;
  case FSHIELD:
    value = parent->FShieldData();
    break;
  case BSHIELD:
    value = parent->BShieldData();
    break;
  case HULL:
    value = parent->GetHull();
    break;
  case LSHIELD:
    value = parent->LShieldData();
    break;
  case RSHIELD:
    value = parent->RShieldData();
    break;
  case FACING:
    //    return parent->getAIState()->queryType (Order::FACING)==NULL;
    return queryType (Order::FACING)==NULL;
  case MOVEMENT:
    //    return parent->getAIState()->queryType (Order::MOVEMENT)==NULL;
    return queryType (Order::MOVEMENT)==NULL;
  case RANDOMIZ:
    value= ((float)rand())/RAND_MAX;
  default:
    return false;
  }
  return item.Eval(value);
}

bool AggressiveAI::ProcessLogic (AIEvents::ElemAttrMap & logi, bool inter) {
  //go through the logic. 
  bool retval=false;
  //  Unit * tmp = parent->Target();
  //  distance = tmp? (tmp->Position()-parent->Position()).Magnitude()-parent->rSize()-tmp->rSize() : FLT_MAX;
  std::vector <std::list <AIEvents::AIEvresult> >::iterator i = logi.result.begin();
  for (;i!=logi.result.end();i++) {
    std::list <AIEvents::AIEvresult>::iterator j;
    bool trueit=true;
    for (j= i->begin();j!=i->end();j++) {
      if (!ProcessLogicItem(*j)) {
	trueit=false;
	break;
      }
    }
    if (trueit&&j==i->end()) {
      //do it
      if (inter) {

	//parent->getAIState()->eraseType (Order::FACING);
	//parent->getAIState()->eraseType (Order::MOVEMENT);
	eraseType (Order::FACING);
	eraseType (Order::MOVEMENT);


      }
      j = i->begin();
      while (j!=i->end()) {
	if (ExecuteLogicItem (*j)) {
	  AIEvents::AIEvresult tmp = *j;
	  i->erase(j);
	  retval=true;
	  i->push_back (tmp);
	  break; 
	}else {
	  j++;
	}
      }

    }
  }
  return retval;
}

bool AggressiveAI::ProcessCurrentFgDirective(Flightgroup * fg) {
  bool retval=false;
  if (fg !=NULL) {
    Unit * leader = fg->leader.GetUnit();
    if (fg->directive!=last_directive) {
      eraseType (Order::FACING);
      eraseType (Order::MOVEMENT);
      parent->Target(NULL);
    }
    if (fg->directive==string("a")) {
      Unit * targ = fg->leader.GetUnit();
      targ = targ!=NULL?targ->Target():NULL;
      if (targ) {
	Vector vec;
	if (parent->InRange (targ,vec))
	  parent->Target (targ);
      }
    }else if (fg->directive==string("f")) {
      retval=true;
      if (leader!=NULL) {
	if (fg->directive!=last_directive) {
	  float left= parent->getFgSubnumber()%2?1:-1;
	  static float esc_percent= XMLSupport::parse_float(vs_config->getVariable ("AI",
										    "Targetting",
										    "EscortDistance",
										    "1.4"));
	  float dist=esc_percent*(1+parent->getFgSubnumber()/2)*left*(parent->rSize()+leader->rSize());
	  Order * ord = new Orders::FormUp(Vector(dist,0,-fabs(dist)));
	  ord->SetParent (parent);
	  ReplaceOrder (ord);
	  ord = new Orders::FaceDirection(dist*2);
	  ord->SetParent (parent);
	  ReplaceOrder (ord);
	}
      }
      for (unsigned int i=0;i<suborders.size();i++) {
	suborders[i]->AttachSelfOrder (leader);
      }
    }else if (fg->directive==string("h")) {
      if (fg->directive!=last_directive&&leader) {
	Unit * th;
	if ((th=leader->Threat())) {
	  Vector vec;
	  if (parent->InRange(th,vec)) {
	    parent->Target(th);
	  }
	}
      }
    } 
    last_directive=fg->directive;
  } 
  return retval;
}
void AggressiveAI::ReCommandWing(Flightgroup * fg) {
  static float time_to_recommand_wing = XMLSupport::parse_float(vs_config->getVariable ("AI",
											"Targetting",
											"TargetCommandierTime",
											"100"));
  if (fg!=NULL) {
    Unit* lead;
    if (NULL!=(lead=fg->leader.GetUnit())) {
      if (lead->getFgSubnumber()>=parent->getFgSubnumber()) {
	if (float(rand())/RAND_MAX<SIMULATION_ATOM/time_to_recommand_wing) {
	  fg->leader.SetUnit(parent);
	  fg->directive = (parent->FShieldData()<.2||parent->RShieldData()<.2)?string("h"):string("b");
	}
      }
    }
  }
}
void AggressiveAI::Execute () {  
  Flightgroup * fg=parent->getFlightgroup();
  ReCommandWing(fg);
  FireAt::Execute();
  if (!ProcessCurrentFgDirective (fg)) {

  if (
#if 1
      curinter==INTRECOVER||//this makes it so only interrupts may not be interrupted
#endif
      curinter==INTNORMAL) {
    if ((curinter = (ProcessLogic (interrupts, true)?INTERR:curinter))==INTERR) {
      logic.curtime=interrupts.maxtime;//set it to the time allotted
    }
  }
  //  if (parent->getAIState()->queryType (Order::FACING)==NULL&&parent->getAIState()->queryType (Order::MOVEMENT)==NULL) { 
  if (queryType (Order::FACING)==NULL&&queryType (Order::MOVEMENT)==NULL) { 
     ProcessLogic(logic);
     curinter=(curinter==INTERR)?INTRECOVER:INTNORMAL;
  } else {
    if ((--logic.curtime)==0) {
      curinter=(curinter==INTERR)?INTRECOVER:INTNORMAL;
      //parent->getAIState()->eraseType (Order::FACING);
      //parent->getAIState()->eraseType (Order::MOVEMENT);
      eraseType (Order::FACING);
      eraseType (Order::MOVEMENT);
      ProcessLogic (logic);
      logic.curtime = logic.maxtime;      
    }
  }
  }
#ifdef AGGDEBUG
  fprintf (stderr,"endagg");
  fflush (stderr);
#endif    
}  


