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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Graphics/DebugRenderer.h"
#include "../IO/Log.h"
#include "../Scene/Node.h"

#include "../Urho2D/NavMesh2D.h"

#include <polypath/polypath.h>

#include "../DebugNew.h"

namespace Urho3D
{

NavMesh2D::NavMesh2D(Context* context) :
    Component(context),
    s_AgentRadius(0.0f),
    s_PPF(polypath::MapDef())
{
}

NavMesh2D::~NavMesh2D()
{
}

void NavMesh2D::RegisterObject(Context* context)
{
    context->RegisterFactory<NavMesh2D>();
}

void NavMesh2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (!debug)
        return;

    // Draw shapes ~ Note: with agent radius (offset) applied
    std::vector<std::vector<polypath::Vec2> > shapes;
    s_PPF.getInstanceShapeVertices(s_AgentRadius, shapes);
    std::vector<std::vector<polypath::Vec2> >::const_iterator shape;
    std::vector<polypath::Vec2>::const_iterator vertice;
    for (shape = shapes.begin(); shape != shapes.end(); ++shape)
    {
        if ((*shape).empty())
            continue;

        for (vertice = (*shape).begin(); vertice != (*shape).end() - 1; ++vertice)
            debug->AddLine(Vector3(vertice->x, vertice->y, 0.0), Vector3((vertice + 1)->x, (vertice + 1)->y, 0.0), Color(1.0f, 1.0f, 1.0f));
        debug->AddCircle(Vector3((*shape).begin()->x, (*shape).begin()->y, 0.0), Vector3::FORWARD, 0.05f, Color(1.0f, 1.0f, 1.0f), 64, depthTest); // Also draw a circle at origin to indicate direction
    }

    // Draw path
    if (path_.Empty())
        return;
    for (unsigned i = 0; i < path_.Size() - 1; ++i)
        debug->AddLine(Vector3(path_[i]), Vector3(path_[i + 1]), Color(1.0f, 1.0f, 1.0f));
}

int NavMesh2D::CreateShape(Vector<Vector2> vertices)
{
    if (vertices.Size() < 2)
        return 0;

    std::vector<polypath::Vec2> shape;
    for (unsigned i = 0; i < vertices.Size(); ++i)
        shape.push_back(ToVec2(vertices[i]));

    return(s_PPF.addShape(shape));
}

//void NavMesh2D::DeleteShape(int shapeID)
//{
//    s_PPF.removeShape(shapeID + 1); // First shape ID is 1
//    s_PPF.rebuildInstances();
//}

void NavMesh2D::Build()
{
    s_PPF.initInstance(s_AgentRadius);
}

int NavMesh2D::FindPath(const Vector2& start_pos, const Vector2& end_pos)
{
    path_.Clear();
    std::vector<polypath::Vec2> s_Path;
    int result = s_PPF.computePath(s_AgentRadius, ToVec2(start_pos), ToVec2(end_pos), &s_Path, NULL);
    for (unsigned i = 0; i < s_Path.size(); ++i)
        path_.Push(ToVector2(s_Path[i]));

    return result;
}

}
