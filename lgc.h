/*
** $Id: lgc.h,v 2.75 2013/09/11 14:47:08 roberto Exp roberto $
** Garbage Collector
** See Copyright Notice in lua.h
*/

#ifndef lgc_h
#define lgc_h


#include "lobject.h"
#include "lstate.h"

/*
** Collectable objects may have one of three colors: white, which
** means the object is not marked; gray, which means the
** object is marked, but its references may be not marked; and
** black, which means that the object and all its references are marked.
** The main invariant of the garbage collector, while marking objects,
** is that a black object can never point to a white one. Moreover,
** any gray object must be in a "gray list" (gray, grayagain, weak,
** allweak, ephemeron) so that it can be visited again before finishing
** the collection cycle. These lists have no meaning when the invariant
** is not being enforced (e.g., sweep phase).
*/



/* how much to allocate before next GC step */
#if !defined(GCSTEPSIZE)
/* ~100 small strings */
#define GCSTEPSIZE	(cast_int(100 * sizeof(TString)))
#endif


/*
** Possible states of the Garbage Collector
*/
#define GCSpropagate	0
#define GCSatomic	1
#define GCSswplocalgc	2
#define GCSswpallgc	4
#define GCSswpthreads	3
#define GCSswplocalfin	5
#define GCSswpfinobj	6
#define GCSswptobefnz	7
#define GCSswpend	8
#define GCSpause	9


#define issweepphase(g)  \
	(GCSswplocalgc <= (g)->gcstate && (g)->gcstate <= GCSswpend)


/*
** macro to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep
** phase may break the invariant, as objects turned white may point to
** still-black objects. The invariant is restored when sweep ends and
** all objects are white again.
*/

#define keepinvariant(g)	((g)->gcstate <= GCSatomic)


/*
** some useful bit tricks
*/
#define resetbits(x,m)		((x) &= cast(lu_byte, ~(m)))
#define setbits(x,m)		((x) |= (m))
#define testbits(x,m)		((x) & (m))
#define bitmask(b)		(1<<(b))
#define bit2mask(b1,b2)		(bitmask(b1) | bitmask(b2))
#define l_setbit(x,b)		setbits(x, bitmask(b))
#define resetbit(x,b)		resetbits(x, bitmask(b))
#define testbit(x,b)		testbits(x, bitmask(b))


/* Layout for bit use in `marked' field: */
#define WHITE0BIT	0  /* object is white (type 0) */
#define WHITE1BIT	1  /* object is white (type 1) */
#define BLACKBIT	2  /* object is black */
#define FINALIZEDBIT	3  /* object has been marked for finalization */
#define NOLOCALBIT	4  /* object is not local */
#define LOCALMARK	5  /* object is 'locally marked' or out of local list */
/* bit 7 is currently used by tests (luaL_checkmemory) */

#define WHITEBITS	bit2mask(WHITE0BIT, WHITE1BIT)


#define iswhite(x)      testbits((x)->gch.marked, WHITEBITS)
#define isblack(x)      testbit((x)->gch.marked, BLACKBIT)
#define isgray(x)  /* neither white nor black */  \
	(!testbits((x)->gch.marked, WHITEBITS | bitmask(BLACKBIT)))
#define islocal(x)	(!testbit((x)->gch.marked, NOLOCALBIT))

#define tofinalize(x)	testbit((x)->gch.marked, FINALIZEDBIT)

#define otherwhite(g)	((g)->currentwhite ^ WHITEBITS)
#define isdeadm(ow,m)	(!(((m) ^ WHITEBITS) & (ow)))
#define isdead(g,v)	isdeadm(otherwhite(g), (v)->gch.marked)

#define changewhite(x)	((x)->gch.marked ^= WHITEBITS)
#define gray2black(x)	l_setbit((x)->gch.marked, BLACKBIT)

#define nolocal(x)	l_setbit((x)->gch.marked, NOLOCALBIT)
#define valnolocal(v)	{ if (iscollectable(v)) nolocal(gcvalue(v)); }

#define luaC_white(g)	cast(lu_byte, (g)->currentwhite & WHITEBITS)


#define luaC_condGC(L,c) \
	{if (G(L)->GCdebt > 0) {c;}; condchangemem(L);}
#define luaC_checkGC(L)		luaC_condGC(L, luaC_step(L);)


#define luaC_barrier(L,p,v) {  \
	if (iscollectable(v) && \
          (nolocal(gcvalue(v)), isblack(obj2gco(p)) && iswhite(gcvalue(v))))  \
	luaC_barrier_(L,obj2gco(p),gcvalue(v)); }

#define luaC_barrierback(L,p,v) {  \
	if (iscollectable(v) && \
          (nolocal(gcvalue(v)), isblack(obj2gco(p)) && iswhite(gcvalue(v))))  \
	luaC_barrierback_(L,obj2gco(p)); }

#define luaC_objbarrier(L,p,o) {  \
	if (nolocal(obj2gco(o)), isblack(obj2gco(p)) && iswhite(obj2gco(o))) \
		luaC_barrier_(L,obj2gco(p),obj2gco(o)); }

#define luaC_upvalbarrier(L,uv) \
  { if (iscollectable((uv)->v) && !upisopen(uv)) \
         luaC_upvalbarrier_(L,uv); }

LUAI_FUNC void luaC_fix (lua_State *L, GCObject *o);
LUAI_FUNC void luaC_freeallobjects (lua_State *L);
LUAI_FUNC void luaC_step (lua_State *L);
LUAI_FUNC void luaC_forcestep (lua_State *L);
LUAI_FUNC void luaC_runtilstate (lua_State *L, int statesmask);
LUAI_FUNC void luaC_fullgc (lua_State *L, int isemergency);
LUAI_FUNC GCObject *luaC_newobj (lua_State *L, int tt, size_t sz);
LUAI_FUNC void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v);
LUAI_FUNC void luaC_barrierback_ (lua_State *L, GCObject *o);
LUAI_FUNC void luaC_barrierproto_ (lua_State *L, Proto *p, Closure *c);
LUAI_FUNC void luaC_upvalbarrier_ (lua_State *L, UpVal *uv);
LUAI_FUNC void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt);
LUAI_FUNC void luaC_upvdeccount (lua_State *L, UpVal *uv);


#endif
