// EXT-17 — geodesy for the condition evaluator: the one computation every proximity and area
// verdict rests on.
//
// **This file exists because `contract/` does not carry it.** The vendored
// `condition-file-schema.md` documents `within_m` as a threshold "in metres" and stops there.
// The sections that state *how* a distance is computed and *how* a boundary is decided are in
// EXT-08's own documentation and were not excerpted into `contract/` — see `docs/m6-oq5.md` §5
// and escalation **E-5**. A consumer working from the digest alone could reasonably compute a
// great-circle distance, or a two-dimensional horizontal separation, and produce verdicts that
// disagree with the producer's on the same capture while parsing the same file. That is silent
// divergence across the project boundary, and it is exactly what the `contract/` discipline
// exists to prevent.
//
// So EXT-17 decides it here, on the record, and states the constants so that CR-AS-2's third
// acceptance criterion is satisfiable with a calculator: *"A verdict's numbers are reproducible:
// recomputing them by hand from the samples it names gives the same values."*
//
// ## The decision, and what constrains it
//
// **Positions are converted to earth-centred, earth-fixed coordinates on WGS-84, and a distance
// is the straight-line Euclidean distance between them, in metres.**
//
// Two things in `contract/` constrain the choice and between them settle it:
//
//   - **§15 forbids converting units.** A capture's `positionGeodetic` is
//     `[latitude°, longitude°, altitude m]` and the capture applies no conversion, so neither
//     may a consumer. ECEF is a change of *frame*, not of unit: metres in, metres out, and the
//     angles are consumed rather than rewritten.
//   - **The third component is data.** A metric that discards altitude discards a value the
//     format carries deliberately, and two entities stacked six kilometres apart vertically
//     are not close. A horizontal-only separation would report them as touching.
//
// A great-circle metric is rejected for the same reason — it answers the surface question, and
// the surface question is not the one a proximity condition asks.
//
// **Altitudes carry the platform's own caveat and it happens to help here.** This install ships
// no geoid grid (F-14, deliberately not fixed), so altitudes are ellipsoidal rather than
// orthometric — which is the datum ECEF wants. Nothing needs correcting.
//
// ## Boundary semantics, because a threshold ambiguous at the threshold is untestable
//
//   - A point exactly at `within_m`, or exactly on a circle's edge, is **inside**: the
//     comparison is `<=`.
//   - A point exactly on a polygon's edge or vertex is **inside**.
//   - Polygons are plane figures in latitude/longitude. That is accurate at scenario scale and
//     is not defended across the antimeridian or a pole; a condition file declaring one is
//     refused by the loader rather than answered wrongly.
//
// In practice the proximity boundary is not reachable — a computed double lands a fraction of a
// millimetre off a nominal threshold — so `<=` matters for **reproducibility**, that the same
// input always gives the same answer, and not because anything will land on it.
//
// ## Never throws (constraint C3), and no locale
//
// Nothing here formats or parses a number. Every value arrives as a double the JSON parser
// already produced through the C locale explicitly, and leaves as a double. CR-DET-2's locale
// hazard has no foothold on this path and `tools/n8ro-judge/build.cmd` searches to keep it so.
#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace ext17::assertion {

// WGS-84, spelled out rather than referenced, so a verdict can be recomputed by hand.
//   a  — semi-major axis, metres
//   f  — flattening
//   e2 — first eccentricity squared, f * (2 - f)
constexpr double kWgs84SemiMajorAxisM = 6378137.0;
constexpr double kWgs84Flattening = 1.0 / 298.257223563;
constexpr double kWgs84EccentricitySquared =
    kWgs84Flattening * (2.0 - kWgs84Flattening);

// A geodetic position in the platform's own order and units: [latitude°, longitude°, altitude m].
// The order is §15's, and it is never reordered on the way through.
using Geodetic = std::array<double, 3>;

// Earth-centred, earth-fixed, metres.
using Ecef = std::array<double, 3>;

// Geodetic to ECEF on WGS-84.
//
//   N = a / sqrt(1 - e2 * sin(lat)^2)
//   x = (N + alt) * cos(lat) * cos(lon)
//   y = (N + alt) * cos(lat) * sin(lon)
//   z = (N * (1 - e2) + alt) * sin(lat)
Ecef toEcef(const Geodetic& g);

// Straight-line distance between two geodetic positions, in metres.
double distanceM(const Geodetic& a, const Geodetic& b);

// Is a point inside a circle centred on `centre` with radius `radiusM`? The comparison is `<=`,
// so a point exactly on the edge is inside.
bool insideCircle(const Geodetic& point, const Geodetic& centre, double radiusM);

// Is a point inside a polygon of `[latitude°, longitude°]` vertices, treated as a plane figure?
// A point on an edge or a vertex is inside, which is checked explicitly rather than left to
// whichever side the ray-cast happens to fall on — that is the difference between a rule and an
// accident.
bool insidePolygon(const std::array<double, 2>& point,
                   const std::vector<std::array<double, 2>>& vertices);

// The shortest distance in metres from a point to a polygon's boundary, at the polygon's own
// altitude-free plane. Used only to size the continuity bound of a not-met area verdict, never
// to decide inside-ness — the predicate above does that.
double distanceToPolygonEdgeM(const std::array<double, 2>& point,
                              const std::vector<std::array<double, 2>>& vertices);

} // namespace ext17::assertion
