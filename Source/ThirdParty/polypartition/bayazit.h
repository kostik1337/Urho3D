/*
    Convex decomposition algorithm originally created by Mark Bayazit aka darkzerox (http://mnbayazit.com)
    For more information about this algorithm, see http://mnbayazit.com/406/bayazit
    Modified by Yogesh (http://yogeshkulkarni.com)
*/

#pragma once

#include <algorithm>
#include <limits>
#include <vector>
#include <cstdio>

const float  EPSILON = 0.0001f;

namespace Bayazit
{

class Point
{
public:
	Point(float x = 0.0f, float y = 0.0f) : x(x), y(y) { }
	float x, y;
};

/// Add 2 points.
Point operator +(const Point& a, const Point& b) { return Point(a.x + b.x, a.y + b.y); }
/// Divide a point by a scalar.
Point operator /(const Point& a, float x) { return Point(a.x / x, a.y / x); }
/// Test for equality with another point without epsilon.
bool operator ==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
/// Test for inequality with another point without epsilon.
bool operator !=(const Point& a, const Point& b) { return a.x != b.x || a.y != b.y; }

/// Polygon, as a vector of points.
typedef std::vector<Point> Polygon;


///==========
class Decomposer
///==========
{

public:
	/// Construct.
	Decomposer(unsigned max = 8, bool debug = false) : maxPolygonVertices_(max), debug_(debug) { }

	/// Destruct.
	~Decomposer() { }

	/// Decompose a polygon into convex polygons.
	std::vector<Polygon> Decompose(const Polygon& vertices) { return ConvexPartition(vertices); }

private:

/// Retrieve an element at position in the polygon.
Point At(int i, const Polygon& vertices)
{
	int s = (int)vertices.size();
	return vertices[i < 0 ? s - (-i % s) : i % s];
}

/// Check if a point in the polygon is a reflex point. Precondition: ccw.
bool Reflex(int i, const Polygon& vertices) { return Right(i, vertices); }

/// If i is on the left of i-1 and i+1 line in the polygon vertices. Checks Area -ve.
bool Left(const Point& a, const Point& b, const Point& c) { return Area(a, b, c) > 0; }

/// If i is on the left or ON of i-1 and i+1 line in the polygon vertices. Checks Area -ve and 0.
bool LeftOn(const Point& a, const Point& b, const Point& c) { return Area(a, b, c) >= 0; }

/// If i is on the right of i-1 and i+1 line in the polygon vertices. Checks Area +ve.
bool Right(int i, const Polygon& vertices) { return Right(At(i - 1, vertices), At(i, vertices), At(i + 1, vertices)); }

/// If i is on the right of i-1 and i+1 line in the polygon vertices. Checks Area +ve.
bool Right(const Point& a, const Point& b, const Point& c) { return Area(a, b, c) < 0; }

/// If i is on the right or ON of i-1 and i+1 line in the polygon vertices. Checks Area +ve and 0.
bool RightOn(const Point& a, const Point& b, const Point& c) { return Area(a, b, c) <= 0; }

/// Position of c on [ab] edge (0 = collinear points).
float Area(const Point& a, const Point& b, const Point& c) { return a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y); } ///{ return (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y); }

/// Squared distance between 2 points.
float SquareDist(const Point& a, const Point& b)
{
	float dx = b.x - a.x;
	float dy = b.y - a.y;
	return dx * dx + dy * dy;
}

/// Check if two vertices in the polygon can see each other without any obstruction.
bool CanSee(int i, int j, const Polygon& vertices)
{
	if (Reflex(i, vertices))
	{
		if (LeftOn(At(i, vertices), At(i - 1, vertices), At(j, vertices)) && RightOn(At(i, vertices), At(i + 1, vertices), At(j, vertices)))
			return false;
	}
	else
	{
		if (RightOn(At(i, vertices), At(i + 1, vertices), At(j, vertices)) || LeftOn(At(i, vertices), At(i - 1, vertices), At(j, vertices)))
			return false;
	}

	if (Reflex(j, vertices))
	{
		if (LeftOn(At(j, vertices), At(j - 1, vertices), At(i, vertices)) && RightOn(At(j, vertices), At(j + 1, vertices), At(i, vertices)))
			return false;
	}
	else
	{
		if (RightOn(At(j, vertices), At(j + 1, vertices), At(i, vertices)) || LeftOn(At(j, vertices), At(j - 1, vertices), At(i, vertices)))
			return false;
	}

	for (int k = 0; k < (int)vertices.size(); ++k)
	{
		// YOGESH : changed from Line-Line intersection to Segment-Segment Intersection
		Point intPoint;
		Point p1 = At(i, vertices);
		Point p2 = At(j, vertices);
		Point q1 = At(k, vertices);
		Point q2 = At(k + 1, vertices);

		// Ignore incident edges
		if (p1 == q1 || p1 == q2 || p2 == q1 || p2 == q2)
			continue;

		bool result = LineIntersect(p1, p2, q1, q2, true, true, intPoint); // Segment intersection

		if (debug_) printf("Line from point %i to %i is tested against [%i,%i to see if they intersect]", i, j, k, k + 1);

		if (intPoint != Point())
		{
			if ((intPoint != (At(k, vertices))) || (intPoint != (At(k + 1, vertices)))) // intPoint is not any of the j line then false, else continue. Intersection has to be interior to qualify s 'false' from here
				return false;
		}
	}
	return true;
}

/// Copy the polygon from vertex i to vertex j.
void Copy(int i, int j, const Polygon& vertices, Polygon& to)
{
	to.clear();

	while (j < i)
		j += vertices.size();

	for (; i <= j; ++i)
		to.push_back(At(i, vertices));
}

/// Intersection point between 2 lines.
Point LineIntersect(const Point& p1, const Point& p2, const Point& q1, const Point& q2)
{
	Point i;
	float a1 = p2.y - p1.y;
	float b1 = p1.x - p2.x;
	float c1 = a1 * p1.x + b1 * p1.y;
	float a2 = q2.y - q1.y;
	float b2 = q1.x - q2.x;
	float c2 = a2 * q1.x + b2 * q1.y;
	float det = a1 * b2 - a2 * b1;

	if (!FloatEquals(det, 0))
	{
		// Lines are not parallel
		i.x = (b2 * c1 - b1 * c2) / det;
		i.y = (a1 * c2 - a2 * c1) / det;
	}
	return i;
}

/// Check if 2 lines intersect.
bool LineIntersect(const Point& point1, const Point& point2, const Point& point3, const Point& point4, bool firstIsSegment, bool secondIsSegment, Point& point)
{
	point.x = 0.0f;
	point.y = 0.0f;

	// These are reused later. Each lettered sub-calculation is used twice, except for b and d, which are used 3 times
	float a = point4.y - point3.y;
	float b = point2.x - point1.x;
	float c = point4.x - point3.x;
	float d = point2.y - point1.y;

	// Denominator to solution of linear system
	float denom = (a * b) - (c * d);

	// If denominator is 0, then lines are parallel
	if (!(denom >= -EPSILON && denom <= EPSILON))
	{
		float e = point1.y - point3.y;
		float f = point1.x - point3.x;
		float oneOverDenom = 1.0f / denom;

		// Numerator of first equation
		float ua = (c * e) - (a * f);
		ua *= oneOverDenom;

		// Check if intersection point of the two lines is on line segment 1
		if (!firstIsSegment || ua >= 0.0f && ua <= 1.0f)
		{
			// Numerator of second equation
			float ub = (b * e) - (d * f);
			ub *= oneOverDenom;

			// Check if intersection point of the two lines is on line segment 2
			// means the line segments intersect, since we know it is on
			// segment 1 as well.
			if (!secondIsSegment || ub >= 0.0f && ub <= 1.0f)
			{
				// Check if they are coincident (no collision in this case)
				if (ua != 0.0f || ub != 0.0f)
				{
					// There is an intersection
					point.x = point1.x + ua * b;
					point.y = point1.y + ua * d;
					return true;
				}
			}
		}
	}
	return false;
}

/// Check equality between 2 float values with epsilon.
bool FloatEquals(float value1, float value2) { return fabs(value1 - value2) <= EPSILON; }

/// Check if polygon winding is counter clock wise.
bool IsCounterClockWise(Polygon poly) { return poly.size() < 3 ? true : GetSignedArea(poly) > 0.0f; }

/// Get polygon signed area.
float GetSignedArea(Polygon points)
{
	if (points.size() < 3)
		return 0;

	float sum = 0;
	for (unsigned i = 0; i < points.size(); ++i)
	{
		Point p1 = points[i];
		Point p2 = i != points.size() - 1 ? points[i + 1] : points[0];
		sum += (p1.x * p2.y) - (p1.y * p2.x);
	}
	return 0.5f * sum;
}

/// Check triangle collinearity.
bool IsCollinear(Point a, Point b, Point c, float tolerance = 0.0f) { return FloatInRange(Area(a, b, c), -tolerance, tolerance); }

/// Check float is within range.
bool FloatInRange(float value, float min, float max) { return (value >= min && value <= max); }

/// Remove all collinear points on the polygon according to collinearity tolerance and return a simplified polygon.
Polygon CollinearSimplify(Polygon& vertices, float collinearityTolerance = 0.0f)
{
	// We can't simplify polygons under 3 vertices
	if (vertices.size() < 3)
		return vertices;

	Polygon simplified;
	for (unsigned i = 0; i < vertices.size(); ++i)
	{
		int prevId = i - 1;
		if (prevId < 0)
			prevId = vertices.size() - 1;
		int nextId = i + 1;
		if (nextId >= vertices.size())
			nextId = 0;
		Point prev = vertices[prevId];
		Point current = vertices[i];
		Point next = vertices[nextId];

		// If they are collinear, continue
		if (IsCollinear(prev, current, next, collinearityTolerance))
			continue;
		simplified.push_back(current);
	}
	return simplified;
}

/// Decompose a polygon into convex ones, while honoring maximum vertices per polygon limit.
std::vector<Polygon> ConvexPartition(Polygon vertices)
{
	std::vector<Polygon> list;

	// YOGESH : Convex Partition can not happen if there are less than 3, ie 2,1 vertices
	if (vertices.size() < 3)
		return list;

	// We force it to CCW as it is a precondition in this algorithm.
	if (!IsCounterClockWise(vertices))
		reverse(vertices.begin(), vertices.end());

	int lowerIndex = 0, upperIndex = 0;
	Polygon lowerPoly, upperPoly;
	Point lowerInt, upperInt; // Intersection points
	for (int i = 0; i < vertices.size(); ++i)
	{
		if (Reflex(i, vertices))
		{
			float upperDist;
			float lowerDist = upperDist = std::numeric_limits<float>().max();
			for (int j = 0; j < (int)vertices.size(); ++j)
			{
				// If line intersects with an edge
				float d;
				Point p;
				if (Left(At(i - 1, vertices), At(i, vertices), At(j, vertices)) && RightOn(At(i - 1, vertices), At(i, vertices), At(j - 1, vertices)))
				{
					// Find the point of intersection
					p = LineIntersect(At(i - 1, vertices), At(i, vertices), At(j, vertices), At(j - 1, vertices));

					if (Right(At(i + 1, vertices), At(i, vertices), p))
					{
						// Make sure it's inside the poly
						d = SquareDist(At(i, vertices), p);
						if (d < lowerDist)
						{
							// Keep only the closest intersection
							lowerDist = d;
							lowerInt = p;
							lowerIndex = j;
						}
					}
				}

				if (Left(At(i + 1, vertices), At(i, vertices), At(j + 1, vertices)) && RightOn(At(i + 1, vertices), At(i, vertices), At(j, vertices)))
				{
					p = LineIntersect(At(i + 1, vertices), At(i, vertices), At(j, vertices), At(j + 1, vertices));

					if (Left(At(i - 1, vertices), At(i, vertices), p))
					{
						d = SquareDist(At(i, vertices), p);
						if (d < upperDist)
						{
							upperDist = d;
							upperIndex = j;
							upperInt = p;
						}
					}
				}
			}

			// If there are no vertices to connect to, choose a point in the middle
			if (lowerIndex == (upperIndex + 1) % (int)vertices.size())
			{
				Point p = ((lowerInt + upperInt) / 2);

				Copy(i, upperIndex, vertices, lowerPoly);
				lowerPoly.push_back(p);
				Copy(lowerIndex, i, vertices, upperPoly);
				upperPoly.push_back(p);
			}
			else
			{
				double highestScore = 0, bestIndex = lowerIndex;
				while (upperIndex < lowerIndex)
					upperIndex += (int)vertices.size();

				for (int j = lowerIndex; j <= upperIndex; ++j)
				{
					if (CanSee(i, j, vertices))
					{
						double score = 1.0f / (SquareDist(At(i, vertices), At(j, vertices)) + 1);
						if (Reflex(j, vertices))
							score += (RightOn(At(j - 1, vertices), At(j, vertices), At(i, vertices)) && LeftOn(At(j + 1, vertices), At(j, vertices), At(i, vertices))) ? 3 : 2;
						else
							score += 1;

						if (score > highestScore)
						{
							bestIndex = j;
							highestScore = score;
						}
					}
				}
				Copy(i, (int)bestIndex, vertices, lowerPoly);
				Copy((int)bestIndex, i, vertices, upperPoly);
			}

			// Solve smallest poly first
			if (lowerPoly.size() < upperPoly.size())
			{
				std::vector<Polygon> lower = ConvexPartition(lowerPoly);
				for (unsigned p = 0; p < lower.size(); ++p)
					list.push_back(lower[p]);

				std::vector<Polygon> upper = ConvexPartition(upperPoly);
				for (unsigned p = 0; p < upper.size(); ++p)
					list.push_back(upper[p]);
			}
			else
			{
				std::vector<Polygon> upper = ConvexPartition(upperPoly);
				for (unsigned p = 0; p < upper.size(); ++p)
					list.push_back(upper[p]);

				std::vector<Polygon> lower = ConvexPartition(lowerPoly);
				for (unsigned p = 0; p < lower.size(); ++p)
					list.push_back(lower[p]);
			}

			return list;
		}
	}

	// Polygon is already convex
	if (vertices.size() > maxPolygonVertices_)
	{
		Copy(0, vertices.size() / 2, vertices, lowerPoly);
		Copy(vertices.size() / 2, 0, vertices, upperPoly);

		std::vector<Polygon> lower = ConvexPartition(lowerPoly);
		for (unsigned i = 0; i < lower.size(); ++i)
			list.push_back(lower[i]);

		std::vector<Polygon> upper = ConvexPartition(upperPoly);
		for (unsigned i = 0; i < upper.size(); ++i)
			list.push_back(upper[i]);
	}
	else
		list.push_back(vertices);

	// The polygons are not guaranteed to be with collinear points. We remove them to be sure
	for (unsigned i = 0; i < list.size(); ++i)
		list[i] = CollinearSimplify(list[i], 0.0f);

	/// Remove empty vertice collections
	std::vector<Polygon>::iterator pt;
	for (pt = list.end() - 1; pt >= list.begin(); --pt)
	{
		if ((*pt).empty())
			list.erase(pt, pt);
	}

	return list;
}

/// Max vertices per polygon. Primarily used to honor Box2D limit (b2_maxPolygonVertices = 8).
unsigned maxPolygonVertices_;

/// Print debug info flag.
bool debug_;

};

}
