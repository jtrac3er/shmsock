
// debug prints hat neu auch noch einen optionalen string
// __VA_OPT__ erzeugt das Symbol nur wenn varargs mindestens ein element hat

#define xstr(a) str(a)
#define str(a) #a

#ifdef DEBUG_PRINTS
#define debug_printf(str, ...) printf("[eapp] \'" xstr(DEBUG_PRINTS) "\' : " str __VA_OPT__(,) __VA_ARGS__)
#else 
#define debug_printf(...)
#endif

