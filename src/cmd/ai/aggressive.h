#include "fire.h"

namespace Orders {
class AggressiveAI: public FireAt {
  enum INTERCUR {INTERR,INTNORMAL,INTRECOVER} curinter;
protected:
  AIEvents::ElemAttrMap logic;
  AIEvents::ElemAttrMap interrupts;
  bool ProcessLogicItem (const AIEvents::AIEvresult & item);
  bool ExecuteLogicItem (const AIEvents::AIEvresult & item);
  bool ProcessLogic(AIEvents::ElemAttrMap &logic, bool inter=false);//returns if found anything
  string last_directive;
  void ReCommandWing(Flightgroup* fg);
  bool ProcessCurrentFgDirective(Flightgroup *fg);
public:
  enum types {AGGAI, MOVEMENT, FACING, UNKNOWN, DISTANCE, METERDISTANCE, THREAT, FSHIELD, LSHIELD, RSHIELD, BSHIELD, FARMOR, BARMOR, LARMOR, RARMOR, HULL, RANDOMIZ};
  AggressiveAI (const char *file, const char * interruptename, Unit * target=NULL);
  void Execute ();
  virtual string getOrderDescription() { return "aggressive"; };
};
}
