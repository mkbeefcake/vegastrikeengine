#include <map>
#include <string>
#include <vector>
#include "unit_generic.h"
#include "unit_const_cache.h"
std::map <StringIntKey,Unit*> UnitConstCache::unit_cache;
std::map <std::string, Mesh*> WeaponMeshCache::unit_cache;
#if 0
typedef std::map <keyval, Unit *> UnitCacheType;
static UnitCacheType unit_cache;
const Unit * getCachedConstUnit (std::string name, int faction) {
  keyval kv (name,faction);
  UnitCacheType::const_iterator i = unit_cache.find (kv);
  if (i!=unit_cache.end())
    return (*i).second;
  return NULL;
}
const Unit * setCachedConstUnit (std::string name, int faction, Unit * un) {
  unit_cache.insert (pair<keyval,Unit *> (keyval(name,faction),un));
  return un;
}
void purgeCache( ) {
    UnitCacheType::iterator i=unit_cache.begin();
    for (;i!=unit_cache.end();++i) {
      (*i).second->Kill();
    }
    unit_cache.clear();
}
#endif
