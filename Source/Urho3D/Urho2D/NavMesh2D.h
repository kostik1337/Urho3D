//
// Copyright (c) 2008-2017 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#pragma once

#include "../Scene/Component.h"

#include <polypath/polypath.h>

namespace Urho3D
{

/// 2D navigation mesh component.
class URHO3D_API NavMesh2D : public Component
{
    URHO3D_OBJECT(NavMesh2D, Component);

public:
    /// Construct.
    NavMesh2D(Context* context);
    /// Destruct.
    ~NavMesh2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Add debug geometry to the debug renderer.
    virtual void DrawDebugGeometry(DebugRenderer* debug, bool depthTest);

    /// Create polygon or polyline shape from vertices. Return shape ID, starting from 1.
    int CreateShape(Vector<Vector2> vertices);

    /// Build navigation mesh.
    void Build();

    /// Find path. Return 0 if successful, 1 if no source link and 2 if no dest link (when dest is unreachable : inside a shape and/or due to agent radius).
    int FindPath(const Vector2& start_pos, const Vector2& end_pos);

    /// Set agent radius.
    void SetAgentRadius(float radius) { s_AgentRadius = radius; }
    /// Return agent radius.
    float GetAgentRadius() const { return s_AgentRadius; }

    /// Return number of shapes.
    int GetNumShapes() const { return s_PPF.GetNumShapes(); }

private:
    /// Convert Vector2 to Vec2.
    polypath::Vec2 ToVec2(const Vector2& point) { return polypath::Vec2(point.x_, point.y_); }
    /// Convert Vec2 to Vector2.
    Vector2 ToVector2(const polypath::Vec2& point) { return Vector2(point.x, point.y); }

    /// Map definition.
    polypath::MapDef s_PPF;
    /// Path.
    Vector<Vector2> path_; //static std::vector<polypath::Vec2> s_Path;
    /// Agent radius.
    float s_AgentRadius;
};

}
