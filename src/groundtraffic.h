/*
 * GroundTraffic
 *
 * (c) Jonathan Harris 2013
 *
 * Licensed under GNU LGPL v2.1.
 */

#ifndef	_GROUNDTRAFFIC_H_
#define	_GROUNDTRAFFIC_H_

#ifdef _MSC_VER
#  define _USE_MATH_DEFINES
#  define _CRT_SECURE_NO_DEPRECATE
#  define inline __forceinline
#endif

#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _MSC_VER
#  define PATH_MAX MAX_PATH
#  define snprintf _snprintf
#  define strcasecmp(s1, s2) _stricmp(s1, s2)
#  define strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
#endif

#if IBM
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <dirent.h>
#  include <libgen.h>
#endif

#if APL
#  include <OpenGL/gl.h>
#  include <OpenGL/glu.h>
#else
#  include <GL/gl.h>
#  include <GL/glu.h>
#endif

#define XPLM210	/* Requires X-Plane 10.0 or later */
#define XPLM200
#include "XPLMDataAccess.h"
#include "XPLMDisplay.h"
#include "XPLMGraphics.h"
#include "XPLMPlanes.h"
#include "XPLMPlugin.h"
#include "XPLMProcessing.h"
#include "XPLMScenery.h"
#include "XPLMUtilities.h"

/* Version of assert that suppresses "variable ... set but not used" if the variable only exists for the purpose of the asserted expression */
#ifdef NDEBUG
#  undef assert
#  define assert(expr)	((void)(expr))
#elif !IBM
#  include <signal.h>
#  undef assert
#  define assert(expr)	{ if (!(expr)) raise(SIGTRAP); };
#endif

/* constants */
#define MAX_NAME 256		/* Arbitrary limit on object name lengths */
#define TILE_RANGE 1		/* How many tiles away from plane's tile to consider getting out of bed for */
#define ACTIVE_POLL 16		/* Poll to see if we've come into range every n frames */
#define ACTIVE_DISTANCE 6000.f	/* Distance [m] from tower location at which to actually get out of bed */
#define ACTIVE_HYSTERESIS (ACTIVE_DISTANCE*0.05f)
#define DRAW_DISTANCE 3500.f	/* Distance [m] from object to draw it. Divided by LOD value. */
#define DEFAULT_LOD 2.25f	/* Equivalent to "medium" world detail distance */
#define PROBE_INTERVAL 4.f	/* How often to probe ahead for altitude [s] */
#define TURN_TIME 2.f		/* Time [s] to execute a turn at a waypoint */
#define AT_INTERVAL 60.f	/* How often [s] to poll for At times */
#define WHEN_INTERVAL 1.f	/* How often [s] to poll for When DataRef values */
#define COLLISION_INTERVAL 4.f	/* How long [s] to poll for crossing route path to become free */
#define COLLISION_TIMEOUT ((int) 60/COLLISION_INTERVAL)	/* How many times to poll before giving up to break deadlock */
#define RESET_TIME 15.f		/* If we're deactivated for longer than this then reset route timings */
#define MAX_VAR 10		/* How many var datarefs */

/* Published DataRefs */
#define REF_BASE		"marginal/groundtraffic/"
#define REF_VAR			REF_BASE "var"
#define REF_DISTANCE		REF_BASE "distance"
#define REF_SPEED		REF_BASE "speed"
#define REF_STEER		REF_BASE "steer"
#define REF_NODE_LAST		REF_BASE "waypoint/last"
#define REF_NODE_LAST_DISTANCE	REF_BASE "waypoint/last/distance"
#define REF_NODE_NEXT		REF_BASE "waypoint/next"
#define REF_NODE_NEXT_DISTANCE	REF_BASE "waypoint/next/distance"
typedef enum
{
    distance=0, speed, steer, node_last, node_last_distance, node_next, node_next_distance,
    dataref_count
} dataref_t;

/* Geolocation */
typedef struct
{
    float lat, lon, alt;	/* drawing routines use float, so no point storing higher precision */
} loc_t;

#define INVALID_ALT DBL_MAX
typedef struct
{
    double lat, lon, alt;	/* but XPLMWorldToLocal uses double, so prevent type conversions */
} dloc_t;

/* OpenGL coordinate */
typedef struct
{
    float x, y, z;
} point_t;

typedef struct
{
    double x, y, z;
} dpoint_t;

/* Days in same order as tm_wday in struct tm, such that 2**tm_wday==DAY_X */
#define DAY_SUN 1
#define DAY_MON 2
#define DAY_TUE 4
#define DAY_WED 8
#define DAY_THU 16
#define DAY_FRI 32
#define DAY_SAT 64
#define DAY_ALL (DAY_SUN|DAY_MON|DAY_TUE|DAY_WED|DAY_THU|DAY_FRI|DAY_SAT)
#define MAX_ATTIMES 24		/* Number of times allowed in an At command */
#define INVALID_AT -1


/* User-defined DataRef */
typedef enum { rising, falling } slope_t;
typedef enum { linear, sine } curve_t;
typedef struct userref_t
{
    char *name;			/* NULL for standard var[n] datarefs */
    XPLMDataRef ref;
    float duration;
    float start1, start2;
    slope_t slope;
    curve_t curve;
    struct userref_t *next;
} userref_t;


/* DataRef referenced in When or And command */
#define xplmType_Mine -1
typedef struct extref_t
{
    char *name;
    XPLMDataRef ref;		/* ID, or pointer if type == xplmType_Mine */
    XPLMDataTypeID type;
    struct extref_t *next;
} extref_t;


/* When & And command */
#define xplmType_Mine -1
typedef struct whenref_t
{
    extref_t *extref;
    int idx;
    float from, to;
    struct whenref_t *next;	/* Next whenref at a waypoint */
} whenref_t;


/* Route path - locations or commands */
struct collision_t;
typedef struct
{
    loc_t waypoint;		/* World */
    point_t p;			/* Local OpenGL co-ordinates */
    point_t p1, p3;		/* Bezier points for turn */
    int pausetime;
    short attime[MAX_ATTIMES];	/* minutes past midnight */
    unsigned char atdays;
    struct {
        int reverse : 1;	/* Reverse whole route */
        int backup : 1;		/* Just reverse to next node */
        int set1 : 1;		/* set command */
        int set2 : 1;		/* pause ... set command */
        slope_t slope : 1;
        curve_t curve : 1;
    } flags;
    struct collision_t *collisions;	/* Collisions with other routes */
    userref_t *userref;
    float userduration;
    whenref_t *whenrefs;
    int drawX, drawY;		/* For labeling nodes */
} path_t;

typedef struct
{
    GLfloat r, g, b;
} glColor3f_t;

typedef struct
{
    char name[MAX_NAME];
    float heading;		/* rotation applied before drawing */
    float offset;		/* offset applied after rotation before drawing. [m] */
    float lag;			/* time lag. [m] in train defn, [s] in route */
} objdef_t;

/* A route from routes.txt */
typedef struct route_t
{
    objdef_t object;
    XPLMObjectRef objref;
    path_t *path;
    int pathlen;
    struct
    {
        int frozen : 1;		/* Child whose parent is waiting */
        int paused : 1;		/* Waiting for pause duration */
        int waiting : 1;	/* Waiting for At time */
        int dataref : 1;	/* Waiting for DataRef value */
        int collision : 1;	/* Waiting for collision to resolve */
        int forwardsb : 1;	/* Waypoint before backing up */
        int backingup : 1;
        int forwardsa : 1;	/* Waypoint after backing up */
        int hasdataref: 1;	/* Does the object on this route have DataRef callbacks? */
    } state;
    int direction;		/* Traversing path 1=forwards, -1=reverse */
    int last_node, next_node;	/* The last and next waypoints visited on the path */
    float last_time, next_time;	/* Time we left last_node, expected time to hit the next node */
    float freeze_time;		/* For children: Time when parent started pause */
    float speed;		/* [m/s] */
    float last_distance;	/* Cumulative distance travelled from first to last_node [m] */
    float next_distance;	/* Distance from last_node to next_node [m] */
    float distance;		/* Cumulative distance travelled from first node [m] */
    float next_heading;		/* Heading from last_node to next_node [m] */
    float steer;		/* Approximate steer angle (degrees) while turning */
    glColor3f_t drawcolor;
    XPLMDrawInfo_t *drawinfo;	/* Where to draw - current OpenGL co-ordinates */
    float next_probe;		/* Time we should probe altitude again */
    float last_y, next_y;	/* OpenGL co-ordinates at last probe point */
    int deadlocked;		/* Counter used to break collision deadlock */
    userref_t (*varrefs)[MAX_VAR];	/* Per-route var dataref */
    struct route_t *parent;	/* Points to head of a train */
    struct route_t *next;
} route_t;


/* A train of interconnected objects */
#define MAX_TRAIN 16
typedef struct train_t
{
    char name[MAX_NAME];
    objdef_t objects[MAX_TRAIN];
    struct train_t *next;
} train_t;


/* Collision between routes */
typedef struct collision_t
{
    route_t *route;	/* Other route */
    int node;		/* Other node (assuming forwards direction) */
    struct collision_t *next;
} collision_t;


/* airport info from routes.txt */
typedef struct
{
    enum { noconfig=0, inactive, active } state;
    char ICAO[5];
    dloc_t tower;
    dpoint_t p;			/* Remember OpenGL location of tower to detect scenery shift */
    int drawroutes;
    route_t *routes;
    route_t *firstroute;
    train_t *trains;
    userref_t *userrefs;
    extref_t *extrefs;
    XPLMDrawInfo_t *drawinfo;	/* consolidated XPLMDrawInfo_t array for all routes/objects so they can be batched */
} airport_t;


/* prototypes */
int activate(airport_t *airport);
void deactivate(airport_t *airport);
void proberoutes(airport_t *airport);
void maproutes(airport_t *airport);
float userrefcallback(XPLMDataRef inRefcon);

int xplog(char *msg);
int readconfig(char *pkgpath, airport_t *airport);
void clearconfig(airport_t *airport);

void labelcallback(XPLMWindowID inWindowID, void *inRefcon);
int drawcallback(XPLMDrawingPhase inPhase, int inIsBefore, void *inRefcon);


/* Globals */
extern char *pkgpath;
extern XPLMDataRef ref_plane_lat, ref_plane_lon, ref_view_x, ref_view_y, ref_view_z, ref_rentype, ref_night, ref_monotonic, ref_doy, ref_tod, ref_LOD;
extern XPLMDataRef ref_datarefs[dataref_count], ref_varref;
extern XPLMProbeRef ref_probe;
extern float draw_distance;
extern airport_t airport;
extern route_t *drawroute;	/* Global so can be accessed in dataref callback */
extern int year;		/* Current year (in GMT tz) */

extern float last_frame;	/* Global so can be reset while disabled */


/* inlines */
static inline int indrawrange(float xdist, float ydist, float zdist, float range)
{
    assert (airport.tower.alt!=INVALID_ALT);	/* If altitude is invalid then arguments to this function will be too */
    return (xdist*xdist + ydist*ydist + zdist*zdist <= range*range);
}

static inline float R2D(float r)
{
    return r * ((float) (180*M_1_PI));
}

static inline float D2R(float d)
{
    return d * ((float) (M_PI/180));
}

/* Operations on point_t */

static inline float angleto(point_t *from, point_t *to)
{
    return atan2f(to->x-from->x, to->z-from->z);
}

/* 2D is point inside polygon? */
static inline int inside(point_t *p, point_t *poly, int npoints)
{
    /* http://paulbourke.net/geometry/polygonmesh/ "Determining if a point lies on the interior of a polygon" */
    int i, j, c=0;
    for (i=0, j=npoints-1; i<npoints; j=i++)
        if ((((poly[i].z <= p->z) && (p->z < poly[j].z)) || ((poly[j].z <= p->z) && (p->z < poly[i].z))) &&
            (p->x < (poly[j].x - poly[i].x) * (p->z - poly[i].z) / (poly[j].z - poly[i].z) + poly[i].x))
            c = !c;
    return c;
}

/* 2D does line p0->p1 intersect p2->p3 */
static inline int intersect(point_t *p0, point_t *p1, point_t *p2, point_t *p3)
{
    /* http://stackoverflow.com/a/1968345 */
    float s, t, d, s1_x, s1_z, s2_x, s2_z;

    s1_x = p1->x - p0->x;  s1_z = p1->z - p0->z;
    s2_x = p3->x - p2->x;  s2_z = p3->z - p2->z;
    d = -s2_x * s1_z + s1_x * s2_z;
    s = (-s1_z * (p0->x - p2->x) + s1_x * (p0->z - p2->z)) / d;
    t = ( s2_x * (p0->z - p2->z) - s2_z * (p0->x - p2->x)) / d;

    return s >= 0 && s <= 1 && t >= 0 && t <= 1;
}

#endif /* _GROUNDTRAFFIC_H_ */
