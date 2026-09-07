/* sc_tunnel.c -- TUNNEL (bar 8).
 *
 * The oldest effect in the book. The 1990s version was a lookup table: for
 * every screen pixel, precompute an angle and a depth, then scroll a texture
 * through them. It could not bend, could not light itself and could not have
 * anything inside it.
 *
 * This one is a raymarched surface instead. The tunnel is the zero level of a
 * distance function whose axis wanders and whose radius ripples, so the camera
 * genuinely banks through it, the walls genuinely catch the light, and the
 * rings are geometry rather than a texture scroll.
 */
#include "../demo.h"

#define TUN_SPEED 7.4f

/* Where the axis of the tunnel sits at a given depth. Two incommensurate
 * frequencies mean the path never visibly repeats. */
/* The curvature has to stay well under the bore radius per unit length. Bend
 * the axis any harder than this and the wall closes across the line of sight:
 * you stop flying down a corridor and start staring at the inside of a pipe. */
/* Two terms, not four. This runs once per march step, and the ray marcher is
 * the whole cost of the shot. */
static inline v2 tun_axis(f32 z)
{
    return V2(sinf(z * 0.062f) * 1.45f, cosf(z * 0.051f) * 1.15f);
}

/* Signed distance to the wall, positive inside. */
static inline f32 tun_map(v3 p, f32 t)
{
    v2  a = tun_axis(p.z);
    v2  q = V2(p.x - a.x, p.y - a.y);
    f32 r = v2_len(q);

    /* The six-fold fluting needs sin(6*theta + phase), and theta only ever
     * appears multiplied by six. Rather than pay for an atan2 to recover the
     * angle and a sine to throw it away again, raise the unit vector to the
     * sixth power: Re and Im of (cx + i*cy)^6 are cos(6*theta) and
     * sin(6*theta) directly, for nine multiplies and no transcendentals. */
    f32 inv = 1.0f / DM_MAX(r, 1e-5f);
    f32 cx = q.x * inv, cy = q.y * inv;
    f32 x2 = cx * cx - cy * cy, y2 = 2.0f * cx * cy;
    f32 x4 = x2 * x2 - y2 * y2, y4 = 2.0f * x2 * y2;
    f32 c6 = x4 * x2 - y4 * y2;
    f32 s6 = x4 * y2 + y4 * x2;

    f32 ph = p.z * 0.34f;
    f32 flute = s6 * cosf(ph) + c6 * sinf(ph);        /* sin(6*theta + ph) */

    f32 R = 3.15f
          + 0.26f * flute                              /* fluting down the bore */
          + 0.12f * sinf(p.z * 1.65f - t * 1.9f);      /* travelling ripple     */
    return R - r;
}

static v3 tun_normal(v3 p, f32 t)
{
    const f32 e = 0.010f;
    /* Inward-facing: the wall's gradient points outward, we want the surface
     * normal pointing back at the camera. */
    f32 dx = tun_map(V3(p.x + e, p.y, p.z), t) - tun_map(V3(p.x - e, p.y, p.z), t);
    f32 dy = tun_map(V3(p.x, p.y + e, p.z), t) - tun_map(V3(p.x, p.y - e, p.z), t);
    f32 dz = tun_map(V3(p.x, p.y, p.z + e), t) - tun_map(V3(p.x, p.y, p.z - e), t);
    return v3_norm(V3(dx, dy, dz));
}

static v3 tunnel_px(v2 uv, const yk_ctx *c, u32 seed)
{
    (void)seed;
    f32 t    = (f32)c->t;
    f32 camz = t * TUN_SPEED;

    v2 ca = tun_axis(camz);
    v3 ro = V3(ca.x, ca.y, camz);

    /* Aim a little way down the bore rather than straight ahead: that is what
     * makes the camera lean into the bends instead of clipping them. */
    v2 aa = tun_axis(camz + 7.0f);
    v3 at = V3(aa.x, aa.y, camz + 7.0f);

    f32 roll = sinf(t * 0.33f) * 0.42f + sinf(t * 0.21f) * 0.18f;
    m3 cam = m3_look_at(ro, at, roll);
    v3 rd  = m3_mul_v3(cam, v3_norm(V3(uv.x, uv.y, 1.55f)));

    f32 dist = 0.05f;
    int hit = 0;
    v3  p = ro;
    for (int i = 0; i < 72; i++) {
        p = v3_add(ro, v3_scl(rd, dist));
        f32 d = tun_map(p, t);
        if (d < 0.0025f) { hit = 1; break; }
        /* Under-relaxed because the radial distance overestimates the true
         * distance wherever the bore is turning. With the gentler axis the
         * overestimate is small, so this can be far less conservative than
         * it had to be when the tunnel was bending harder. */
        dist += d * 0.80f;
        if (dist > 70.0f) break;
    }

    v3 col = V3(0.0f, 0.0f, 0.0f);

    if (hit) {
        v3 n = tun_normal(p, t);
        v2 a = tun_axis(p.z);
        f32 ang = atan2f(p.y - a.y, p.x - a.x);

        /* Base colour walks the house palette along the bore, so the tunnel
         * changes hue as you fly rather than being one flat corridor. */
        /* Oscillate inside a narrow band of the palette instead of walking the
         * whole of it: a full sweep passes through olive and mustard, which
         * fight everything else on screen. The corridor stays cold so the
         * neon has something to be warm against. */
        v3 base = dm_pal_house(0.56f + 0.095f * sinf(p.z * 0.018f + t * 0.06f));
        base = v3_scl(base, 0.20f);

        /* Panel grid: thin dark seams between plates. */
        f32 gz = dm_fract(p.z * 0.5f);
        f32 ga = dm_fract(ang * (6.0f / DM_TAU) * 4.0f);
        f32 seam = dm_smoothstep(0.0f, 0.06f, gz) * dm_smoothstep(1.0f, 0.94f, gz)
                 * dm_smoothstep(0.0f, 0.08f, ga) * dm_smoothstep(1.0f, 0.92f, ga);
        base = v3_scl(base, 0.35f + 0.65f * seam);

        /* Surface break-up, so the light has something to grip. */
        f32 grain = dm_fbm3(v3_scl(p, 2.6f), 4, 2.1f, 0.5f, 31u);
        base = v3_scl(base, 0.45f + 0.95f * dm_sat(grain * 0.5f + 0.5f));

        /* A lamp riding just behind the camera. Distance falloff alone gives
         * the corridor its depth -- no fog needed for the near field. */
        v3 lp = v3_add(ro, v3_scl(V3(0, 0, 1), 1.2f));
        v3 ld = v3_sub(lp, p);
        f32 ll = v3_len(ld);
        ld = v3_scl(ld, 1.0f / DM_MAX(ll, 1e-4f));
        f32 diff = dm_sat(v3_dot(n, ld)) * 11.0f / (ll * ll + 3.0f);

        /* A tight specular from the same lamp. It is the only thing that
         * describes the fluting on the near wall, which diffuse alone leaves
         * as a flat coloured curtain across the edges of the frame. */
        f32 spec = powf(dm_sat(v3_dot(v3_reflect(rd, n), ld)), 28.0f)
                 * 9.0f / (ll * ll + 3.0f);

        f32 fres = powf(1.0f - dm_sat(v3_dot(n, v3_neg(rd))), 4.0f);

        col = v3_add(v3_scl(base, diff), v3_scl(dm_rgb8(120, 190, 255), fres * 0.18f));
        col = v3_add(col, v3_scl(dm_rgb8(180, 215, 255), spec * 0.35f));

        /* Emissive rings, one every four units, pulsing on the beat. These are
         * the thing the eye actually locks onto, and they are why the shot
         * reads as musical rather than merely moving. */
        f32 ring = powf(dm_sat(sinf(p.z * (DM_PI / 2.0f) - t * 1.2f)), 90.0f);
        f32 beat = 0.45f + 1.30f * c->beat_hit;
        col = v3_add(col, v3_scl(dm_pal_house(0.14f + t * 0.012f), ring * beat * 1.7f));

        /* A second, slower set in a contrasting hue every sixteen units. */
        f32 ring2 = powf(dm_sat(sinf(p.z * (DM_PI / 8.0f) - t * 0.3f)), 200.0f);
        col = v3_add(col, v3_scl(dm_rgb8(255, 120, 200), ring2 * (0.5f + 0.8f * c->bar_hit) * 1.4f));

        col = yk_fog(col, v3_scl(dm_rgb8(20, 26, 58), 0.05f), dist, 0.045f);
    }

    /* The vanishing point: whatever the ray missed, it is looking down the
     * throat of the tunnel, so put the light there. */
    f32 r2 = v2_dot(uv, uv);
    col = v3_add(col, v3_scl(dm_pal_house(0.30f),
                             expf(-r2 * 3.0f) * (0.10f + 0.22f * c->bar_hit)));

    return v3_scl(col, yk_vignette(uv, 0.40f));
}

void scene_tunnel(dm_fb *fb, const yk_ctx *c)
{
    yk_shade(fb, c, tunnel_px, yk_aa);
}
