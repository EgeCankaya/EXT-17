#include "Geodesy.h"

#include <cmath>

namespace ext17::assertion {
namespace {

constexpr double kPi = 3.14159265358979323846;

double radians(double degrees) { return degrees * (kPi / 180.0); }

// Squared distance from a point to a segment, in the polygon's own plane. Degrees, not metres —
// the caller converts once at the end, so the comparison of candidate edges never leaves the
// plane the polygon is defined in.
double pointToSegmentSquared(double px, double py,
                             double ax, double ay,
                             double bx, double by) {
    const double dx = bx - ax;
    const double dy = by - ay;
    const double lengthSquared = dx * dx + dy * dy;
    double t = 0.0;
    if (lengthSquared > 0.0) {
        t = ((px - ax) * dx + (py - ay) * dy) / lengthSquared;
        if (t < 0.0) { t = 0.0; }
        if (t > 1.0) { t = 1.0; }
    }
    const double cx = ax + t * dx;
    const double cy = ay + t * dy;
    return (px - cx) * (px - cx) + (py - cy) * (py - cy);
}

// Is the point exactly on this segment? Exact rather than approximate: a vertex or an edge point
// is INSIDE by rule, and a rule decided by floating-point slop is not a rule. The tolerance is
// the smallest that survives the multiplications above rather than a comfort margin.
bool onSegment(double px, double py, double ax, double ay, double bx, double by) {
    const double cross = (bx - ax) * (py - ay) - (by - ay) * (px - ax);
    if (std::fabs(cross) > 1e-12) { return false; }
    const double minX = ax < bx ? ax : bx;
    const double maxX = ax < bx ? bx : ax;
    const double minY = ay < by ? ay : by;
    const double maxY = ay < by ? by : ay;
    return px >= minX - 1e-12 && px <= maxX + 1e-12 &&
           py >= minY - 1e-12 && py <= maxY + 1e-12;
}

} // namespace

Ecef toEcef(const Geodetic& g) {
    const double lat = radians(g[0]);
    const double lon = radians(g[1]);
    const double alt = g[2];

    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double sinLon = std::sin(lon);
    const double cosLon = std::cos(lon);

    const double n = kWgs84SemiMajorAxisM /
                     std::sqrt(1.0 - kWgs84EccentricitySquared * sinLat * sinLat);

    return Ecef{
        (n + alt) * cosLat * cosLon,
        (n + alt) * cosLat * sinLon,
        (n * (1.0 - kWgs84EccentricitySquared) + alt) * sinLat,
    };
}

double distanceM(const Geodetic& a, const Geodetic& b) {
    const Ecef p = toEcef(a);
    const Ecef q = toEcef(b);
    const double dx = p[0] - q[0];
    const double dy = p[1] - q[1];
    const double dz = p[2] - q[2];
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool insideCircle(const Geodetic& point, const Geodetic& centre, double radiusM) {
    // `<=`, per the boundary rule in the header. A point exactly on the edge is inside.
    return distanceM(point, centre) <= radiusM;
}

bool insidePolygon(const std::array<double, 2>& point,
                   const std::vector<std::array<double, 2>>& vertices) {
    const std::size_t n = vertices.size();
    if (n < 3) { return false; }

    const double px = point[0];
    const double py = point[1];

    // On an edge or a vertex is INSIDE, by rule. Checked first and explicitly, because a
    // ray-cast answers such a point according to which side the ray happened to graze.
    for (std::size_t i = 0; i < n; ++i) {
        const std::array<double, 2>& a = vertices[i];
        const std::array<double, 2>& b = vertices[(i + 1) % n];
        if (onSegment(px, py, a[0], a[1], b[0], b[1])) { return true; }
    }

    bool inside = false;
    for (std::size_t i = 0; i < n; ++i) {
        const std::array<double, 2>& a = vertices[i];
        const std::array<double, 2>& b = vertices[(i + 1) % n];
        const bool straddles = (a[1] > py) != (b[1] > py);
        if (!straddles) { continue; }
        const double x = (b[0] - a[0]) * (py - a[1]) / (b[1] - a[1]) + a[0];
        if (px < x) { inside = !inside; }
    }
    return inside;
}

double distanceToPolygonEdgeM(const std::array<double, 2>& point,
                              const std::vector<std::array<double, 2>>& vertices) {
    const std::size_t n = vertices.size();
    if (n < 3) { return 0.0; }

    double bestSquared = -1.0;
    std::array<double, 2> nearest{point[0], point[1]};
    for (std::size_t i = 0; i < n; ++i) {
        const std::array<double, 2>& a = vertices[i];
        const std::array<double, 2>& b = vertices[(i + 1) % n];
        const double dx = b[0] - a[0];
        const double dy = b[1] - a[1];
        const double lengthSquared = dx * dx + dy * dy;
        double t = 0.0;
        if (lengthSquared > 0.0) {
            t = ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / lengthSquared;
            if (t < 0.0) { t = 0.0; }
            if (t > 1.0) { t = 1.0; }
        }
        const double cx = a[0] + t * dx;
        const double cy = a[1] + t * dy;
        const double d = pointToSegmentSquared(point[0], point[1], a[0], a[1], b[0], b[1]);
        if (bestSquared < 0.0 || d < bestSquared) {
            bestSquared = d;
            nearest[0] = cx;
            nearest[1] = cy;
        }
    }

    // The candidate edges are chosen in the plane; the answer is returned in metres, by
    // measuring the winning point on the ellipsoid at a common altitude. Mixing the two — a
    // plane comparison and a metric answer — is deliberate: the polygon IS a plane figure, and
    // only the reported magnitude needs to be in the units a bound is expressed in.
    const Geodetic from{point[0], point[1], 0.0};
    const Geodetic to{nearest[0], nearest[1], 0.0};
    return distanceM(from, to);
}

} // namespace ext17::assertion
