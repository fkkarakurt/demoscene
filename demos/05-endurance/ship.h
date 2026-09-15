/* ship.h -- Endurance.
 *
 * Built at Framnaes, Sandefjord, and launched in December 1912 as Polaris: a
 * three-masted barquentine 43.9 metres long and 7.62 across, 4.80 metres from
 * keel to deck, oak and pine and a skin of greenheart against the ice. She was
 * painted black in the 1914 refit, and kept the star on her stern.
 *
 * The ship's own frame: x along the keel with the bow toward +x, y up from the
 * underside of the keel, z across with +z to port. Every number in ship.c
 * is in metres.
 */
#ifndef ENDURANCE_SHIP_H
#define ENDURANCE_SHIP_H

#include "demo.h"

#define SHIP_LOA      43.9f
#define SHIP_BEAM      7.62f
#define SHIP_DEPTH     4.80f     /* moulded: keel to main deck                */
#define SHIP_DRAUGHT   3.30f     /* where the sea, or the ice, meets her      */

enum {
    SHIP_HULL = 1,      /* the black topsides and bottom                     */
    SHIP_DECK,          /* planking                                          */
    SHIP_RAIL,          /* bulwark capping and the taffrail                  */
    SHIP_HOUSE,         /* deckhouses and the fo'c'sle front                 */
    SHIP_SPAR,          /* the lower masts                                   */
    SHIP_SPAR_PALE,     /* yards, topmasts, booms, gaffs, bowsprit           */
    SHIP_FUNNEL,
    SHIP_BOAT,
    SHIP_GILT,          /* the name and the star                             */
    SHIP_IRON,          /* davits, capstan, the wheel's boss                 */
    SHIP_GLASS,         /* windows and skylights                             */
    SHIP_WHEEL,         /* the wheel: teak                                   */
};

/* The rig comes apart in sections, each of which can be turned about the
 * point where it breaks and moved, so a mast can fall in one shot and lie
 * across the deck in another. A section rides on its parent: when the
 * foremast goes, its topmast and yards go with it. */
typedef enum {
    RIG_FORE_LOWER, RIG_FORE_TOP, RIG_FORE_TGALLANT,
    RIG_MAIN_LOWER, RIG_MAIN_TOP,
    RIG_MIZ_LOWER,  RIG_MIZ_TOP,
    RIG_BOWSPRIT,
    RIG_FUNNEL,
    RIG_COUNT
} en_rig_section;

/* What state she is in. Everything a scene can change about the ship. */
typedef struct {
    f32 rime;           /* 0..1: frost on the rigging and the spars          */
    f32 lights;         /* 0..1: the two lamps on poles, and the windows     */
    f32 stern_light;    /* 0..1: the electric light at the stern, Oct 1915   */
    f32 wheelhouse;     /* 1 while it stood; 0 once it was taken onto the ice */
    int wreck;          /* the ship on the seabed in 2022                    */
    int no_boats;       /* once the boats had been taken onto the ice        */

    /* Per section: turned by `angle` about `axis` through its break point,
     * then moved by `shift`, all in the parent's frame. */
    f32 angle[RIG_COUNT];
    v3  axis[RIG_COUNT];
    v3  shift[RIG_COUNT];
    u8  gone[RIG_COUNT];

    /* Filled in by en_ship_state_prepare: each section's placement in the
     * ship's frame, point = rot * p + off. */
    m3  rot[RIG_COUNT];
    v3  off[RIG_COUNT];
    v3  bound_lo, bound_hi;
} en_ship_state;

/* A ship standing as she was built, every section in place. */
en_ship_state en_ship_state_default(void);
void          en_ship_state_prepare(en_ship_state *S);

/* Where she is: the frame of the ship in the world. */
typedef struct {
    v3  pos;            /* world position of the ship's origin               */
    f32 yaw;            /* heading: rotation about world y                   */
    f32 heel;           /* roll about the keel, positive down to port        */
    f32 trim;           /* pitch, positive bow up                            */
    m3  rot;            /* built from the three angles by en_ship_pose_make  */
} en_ship_pose;

void en_ship_pose_make(en_ship_pose *P);
v3   en_ship_to_local(const en_ship_pose *P, v3 world);
v3   en_ship_dir_to_local(const en_ship_pose *P, v3 dir);
v3   en_ship_to_world(const en_ship_pose *P, v3 local);
v3   en_ship_dir_to_world(const en_ship_pose *P, v3 dir);

/* Built once: the name on the stern and the rig tables. */
void en_ship_init(void);

/* Distance to the ship, in its own frame. `part` receives what was hit. */
f32 en_ship_sdf(v3 p, const en_ship_state *S, int *part);
v3  en_ship_normal(v3 p, const en_ship_state *S);

/* A ray in the ship's frame, marched with every component it cannot reach
 * left out. 1 on a hit. */
int en_ship_march(const en_ship_state *S, v3 ro, v3 dir, f32 t_max, f32 cone,
                  f32 *t_out, int *part_out);

/* How much of a light along `dir` from p gets past the ship: 1 all, 0 none.
 * `soft` is the light's angular size, which sets how soft the edge is. */
f32 en_ship_shadow(const en_ship_state *S, v3 p, v3 dir, f32 soft);

/* A box, in the ship's frame, that contains every surface of her. */
void en_ship_bounds(const en_ship_state *S, v3 *lo, v3 *hi);

/* Half the breadth of the hull at a length and a height, 0 past the ends. */
f32 en_hull_half_breadth(f32 x, f32 y);

/* The horizontal distance from a point to the hull's outline at height y, in
 * the ship's frame, positive outside: what the ice needs to know. */
f32 en_hull_waterline_distance(f32 x, f32 z, f32 y);

/* Surface colour and roughness at a hit, and light it gives off itself. */
v3 en_ship_albedo(v3 p, v3 n, int part, const en_ship_state *S, f32 *rough);
v3 en_ship_emission(v3 p, v3 n, int part, const en_ship_state *S);

/* The rigging, as lines in the ship's frame. Returns how many were written. */
int en_ship_rigging(const en_ship_state *S, en_line *out, int cap);

/* The ship's own lamps, in her frame. */
typedef struct {
    v3  pos, dir;
    f32 cos_outer, cos_inner;
    v3  power;
} en_ship_lamp;

int en_ship_lamps(const en_ship_state *S, en_ship_lamp *out, int cap);

#endif /* ENDURANCE_SHIP_H */
