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

// SliceForMax8Vertices function ported from Physics Body Editor Copyright (c) Aurelien Ribbon

#include "../Precompiled.h"

#include "../Urho2D/CollisionBox2D.h"
#include "../Urho2D/CollisionChain2D.h"
#include "../Urho2D/CollisionCircle2D.h"
#include "../Urho2D/CollisionPolygon2D.h"
#include "../Urho2D/CollisionShape2D.h"
#include "../Urho2D/ConstraintDistance2D.h"
#include "../Urho2D/ConstraintFriction2D.h"
#include "../Urho2D/ConstraintGear2D.h"
#include "../Urho2D/ConstraintMotor2D.h"
#include "../Urho2D/ConstraintMouse2D.h"
#include "../Urho2D/ConstraintPrismatic2D.h"
#include "../Urho2D/ConstraintPulley2D.h"
#include "../Urho2D/ConstraintRevolute2D.h"
#include "../Urho2D/ConstraintRope2D.h"
#include "../Urho2D/ConstraintWeld2D.h"
#include "../Urho2D/ConstraintWheel2D.h"
#include "../Core/Context.h"
#include "../Graphics/DebugRenderer.h"
#include "../Navigation/DynamicNavigationMesh.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/IndexBuffer.h"
#include "../IO/Log.h"
#include "../Graphics/Material.h"
#include "../Graphics/Model.h"
#include "../Navigation/Navigable.h"
#include "../Navigation/NavigationMesh.h"
#include "../Scene/Node.h"
#include "../Urho2D/PhysicsWorld2D.h"
#include "../Resource/ResourceCache.h"
#include "../Urho2D/RigidBody2D.h"
#include "../Scene/Scene.h"
#include "../Graphics/StaticModel.h"
#include "../Urho2D/StaticSprite2D.h"
#include "../Core/StringUtils.h"
#include "../Urho2D/TileMap2D.h"
#include "../Urho2D/TileMapLayer2D.h"
#include "../Urho2D/TmxFile2D.h"
#include "../Graphics/VertexBuffer.h"

#include <polypartition/bayazit.h>

#define MPE_POLY2TRI_IMPLEMENTATION
#include <polypartition/MPE_fastpoly2tri.h>

#include "../DebugNew.h"

namespace Urho3D
{

extern const float PIXEL_SIZE;
extern const char* URHO2D_CATEGORY;

typedef Vector<Vector2> Points;

TileMap2D::TileMap2D(Context* context) :
    Component(context),
    mapRotation_(Quaternion(-90.0f, 0.0f, 0.0f)) ///(Quaternion(-90.0f, 0.0f, 0.0f))
{
}

TileMap2D::~TileMap2D()
{
}

void TileMap2D::RegisterObject(Context* context)
{
    context->RegisterFactory<TileMap2D>(URHO2D_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Tmx File", GetTmxFileAttr, SetTmxFileAttr, ResourceRef, ResourceRef(TmxFile2D::GetTypeStatic()),
        AM_DEFAULT);
}

void TileMap2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    const Color& color = Color::RED;
    float mapW = info_.GetMapWidth();
    float mapH = info_.GetMapHeight();

    switch (info_.orientation_)
    {
    case O_ORTHOGONAL:
    case O_STAGGERED:
    case O_HEXAGONAL:
        debug->AddLine(Vector2(0.0f, 0.0f), Vector2(mapW, 0.0f), color);
        debug->AddLine(Vector2(mapW, 0.0f), Vector2(mapW, mapH), color);
        debug->AddLine(Vector2(mapW, mapH), Vector2(0.0f, mapH), color);
        debug->AddLine(Vector2(0.0f, mapH), Vector2(0.0f, 0.0f), color);
        break;

    case O_ISOMETRIC:
        debug->AddLine(Vector2(0.0f, mapH * 0.5f), Vector2(mapW * 0.5f, 0.0f), color);
        debug->AddLine(Vector2(mapW * 0.5f, 0.0f), Vector2(mapW, mapH * 0.5f), color);
        debug->AddLine(Vector2(mapW, mapH * 0.5f), Vector2(mapW * 0.5f, mapH), color);
        debug->AddLine(Vector2(mapW * 0.5f, mapH), Vector2(0.0f, mapH * 0.5f), color);
        break;
    }

    for (unsigned i = 0; i < layers_.Size(); ++i)
        layers_[i]->DrawDebugGeometry(debug, depthTest);
}

void TileMap2D::DrawDebugGeometry()
{
    Scene* scene = GetScene();
    if (!scene)
        return;

    DebugRenderer* debug = scene->GetComponent<DebugRenderer>();
    if (!debug)
        return;

    DrawDebugGeometry(debug, false);
}

void TileMap2D::SetTmxFile(TmxFile2D* tmxFile)
{
    if (tmxFile == tmxFile_)
        return;

    if (rootNode_)
        rootNode_->RemoveAllChildren();

    layers_.Clear();

    tmxFile_ = tmxFile;
    if (!tmxFile_)
        return;

    info_ = tmxFile_->GetInfo();

    if (!rootNode_)
    {
        rootNode_ = GetNode()->CreateTemporaryChild("_root_", LOCAL);
    }

    unsigned numLayers = tmxFile_->GetNumLayers();
    layers_.Resize(numLayers);

    // Create navigation mesh if "Physics" layer contains an object of type "NavMesh"
    for (unsigned i = 0; i < numLayers; ++i)
    {
        const TmxLayer2D* tmxLayer = tmxFile_->GetLayer(i);
        if (tmxLayer->GetName() == "Physics" && tmxLayer->GetType() == LT_OBJECT_GROUP)
        {
            for (unsigned i = 0; i < ((TmxObjectGroup2D*)tmxLayer)->GetNumObjects(); ++i)
            {
                if (((TmxObjectGroup2D*)tmxLayer)->GetObject(i)->GetType() == "NavMesh")
                {
                    CreateNavMesh(((TmxObjectGroup2D*)tmxLayer)->GetObject(i));
                    break;
                }
            }
        }
    }

    // Create layers
    for (unsigned i = 0; i < numLayers; ++i)
    {
        const TmxLayer2D* tmxLayer = tmxFile_->GetLayer(i);

        Node* layerNode(rootNode_->CreateTemporaryChild(tmxLayer->GetName(), LOCAL));

        TileMapLayer2D* layer = layerNode->CreateComponent<TileMapLayer2D>();
        layer->Initialize(this, tmxLayer);
        layer->SetDrawOrder(i * 10);

        layers_[i] = layer;
    }

    // Create rigid bodies, collision shapes and constraints for objects belonging to "Physics" and "Constraints" layers
    CreatePhysicsFromObjects();
    CreateConstraintsFromObjects();

    // Build navMesh
    NavigationMesh* navMesh = GetNavMesh();
    if (navMesh)
        navMesh->Build();
}

TileMapLayer2D* TileMap2D::GetLayer(unsigned index) const
{
    if (index >= layers_.Size())
        return 0;

    return layers_[index];
}

TileMapLayer2D* TileMap2D::GetLayer(const String& name) const
{
    for (Vector<WeakPtr<TileMapLayer2D> >::ConstIterator i = layers_.Begin(); i != layers_.End(); ++i)
    {
        if ((*i)->GetName() == name)
            return *i;
    }

    return 0;
}

Vector2 TileMap2D::TileIndexToPosition(int x, int y) const
{
    return info_.TileIndexToPosition(x, y);
}

bool TileMap2D::PositionToTileIndex(int& x, int& y, const Vector2& position) const
{
    return info_.PositionToTileIndex(x, y, position);
}

void TileMap2D::SetTmxFileAttr(const ResourceRef& value)
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    SetTmxFile(cache->GetResource<TmxFile2D>(value.name_));
}

ResourceRef TileMap2D::GetTmxFileAttr() const
{
    return GetResourceRef(tmxFile_, TmxFile2D::GetTypeStatic());
}

Vector<SharedPtr<TileMapObject2D> > TileMap2D::GetTileCollisionShapes(int gid) const
{
    Vector<SharedPtr<TileMapObject2D> > shapes;
    return tmxFile_ ? tmxFile_->GetTileCollisionShapes(gid) : shapes;
}

void TileMap2D::CreateNavMesh(TileMapObject2D* navObject)
{
    if (!navObject)
        return;

    // NavMesh root node
    Node* n = rootNode_->CreateChild("NavMesh");

    n->SetRotation(mapRotation_);
    n->CreateComponent<Navigable>();

///    NavigationMesh* navMesh;
    bool dynamic;
    if (navObject && navObject->HasProperty("Dynamic"))
        dynamic = ToBool(navObject->GetProperty("Dynamic"));
///    navMesh = dynamic ? n->CreateComponent<DynamicNavigationMesh>() : n->CreateComponent<NavigationMesh>();
NavigationMesh* navMesh = n->CreateComponent<NavigationMesh>();

    // Set navMesh properties according to tmx file
    if (navObject->GetNumProperties() > 0)
    {
        if (navObject->HasProperty("MeshName")) navMesh->SetMeshName(navObject->GetProperty("MeshName"));
        if (navObject->HasProperty("TileSize")) navMesh->SetTileSize(ToInt(navObject->GetProperty("TileSize")));
        if (navObject->HasProperty("CellSize")) navMesh->SetCellSize(ToFloat(navObject->GetProperty("CellSize")));
        if (navObject->HasProperty("CellHeight")) navMesh->SetCellHeight(ToFloat(navObject->GetProperty("CellHeight")));
        if (navObject->HasProperty("AgentHeight")) navMesh->SetAgentHeight(ToFloat(navObject->GetProperty("AgentHeight")));
        if (navObject->HasProperty("AgentRadius")) navMesh->SetAgentRadius(ToFloat(navObject->GetProperty("AgentRadius")));
        if (navObject->HasProperty("AgentMaxClimb")) navMesh->SetAgentMaxClimb(ToFloat(navObject->GetProperty("AgentMaxClimb")));
        if (navObject->HasProperty("AgentMaxSlope")) navMesh->SetAgentMaxSlope(ToFloat(navObject->GetProperty("AgentMaxSlope")));
        if (navObject->HasProperty("RegionMinSize")) navMesh->SetRegionMinSize(ToFloat(navObject->GetProperty("RegionMinSize")));
        if (navObject->HasProperty("RegionMergeSize")) navMesh->SetRegionMergeSize(ToFloat(navObject->GetProperty("RegionMergeSize")));
        if (navObject->HasProperty("EdgeMaxLength")) navMesh->SetEdgeMaxLength(ToFloat(navObject->GetProperty("EdgeMaxLength")));
        if (navObject->HasProperty("EdgeMaxError")) navMesh->SetEdgeMaxError(ToFloat(navObject->GetProperty("EdgeMaxError")));
        if (navObject->HasProperty("DetailSampleDistance")) navMesh->SetDetailSampleDistance(ToFloat(navObject->GetProperty("DetailSampleDistance")));
        if (navObject->HasProperty("DetailSampleMaxError")) navMesh->SetDetailSampleMaxError(ToFloat(navObject->GetProperty("DetailSampleMaxError")));
        if (navObject->HasProperty("Padding")) navMesh->SetPadding(ToVector3(navObject->GetProperty("Padding")));
        if (navObject->HasProperty("Watershed")) navMesh->SetPartitionType(ToBool(navObject->GetProperty("Watershed")) ? NAVMESH_PARTITION_WATERSHED : NAVMESH_PARTITION_MONOTONE);
        if (navObject->HasProperty("DrawOffMeshConnections")) navMesh->SetDrawOffMeshConnections(ToBool(navObject->GetProperty("DrawOffMeshConnections")));
        if (navObject->HasProperty("DrawNavAreas")) navMesh->SetDrawNavAreas(ToBool(navObject->GetProperty("DrawNavAreas")));
    }

    // FLOOR
    Node* ground;

    unsigned numPoints = navObject->GetNumPoints();
    if (numPoints > 1 && (navObject->GetPoint(0) == navObject->GetPoint(numPoints - 1)))
        numPoints -= 1; // Remove closing point

    // For now we only support 1 unic polyline shape to build the ground, as it is the most versatile
    if (numPoints > 2 && navObject->GetObjectType() == OT_POLYLINE)
    {
        Points points;
        for (unsigned i = 0; i < numPoints; ++i)
            points.Push(navObject->GetPoint(i));

        Vector<float> vertices;
        Triangulate(vertices, points);
        ground = CreateProceduralModel(vertices, false);
    }

    // If no polyline shape supplied, build ground from tilemap size
    if (!ground)
    {
        float scaleX = info_.GetMapWidth();
        float scaleZ = info_.GetMapHeight();
        float vertices[] = {0.0f, 0.0f, 0.0f, scaleZ, scaleX, scaleZ, 0.0f, 0.0f, scaleX, scaleZ, scaleX, 0.0f}; // 2 triangles from 3 vertices: (x, z, x, z, x, z)...
        Vector<float> polypoints;
        for (unsigned i = 0; i < 12; ++i)
            polypoints.Push(vertices[i]);
        ground = CreateProceduralModel(polypoints, false);
    }

    // Do not render the ground
    ground->SetName("NavGround");
    StaticModel* model = ground->GetComponent<StaticModel>();
    if (model)
        model->SetMaterial(GetSubsystem<ResourceCache>()->GetResource<Material>("Assets/Momo_Ogre/Materials/Physics.xml"));
}

void TileMap2D::EdgesToTriangles(Vector<float>& points)
{
    Vector<float> polypoints;
    for (unsigned i = 0; i < points.Size(); ++i)
    {
        polypoints.Push(points[i]);

        // Duplicate 2nd vertice, to form a virtual triangle with no surface
        if ((i + 1) % 4 == 0)
        {
            polypoints.Push(points[i - 1]);
            polypoints.Push(points[i]);
        }
    }
    points = polypoints;
}

Vector3 TileMap2D::StoreVertices(unsigned& numVertices, PODVector<float>& vertexData, PODVector<unsigned short>& indexData, BoundingBox& bbox, Vector<float> polypoints, bool dummy)
{
    float firstX = polypoints[0];
    float minX = firstX;
    float maxX = firstX;
    float firstZ = polypoints[1];
    float minZ = firstZ;
    float maxZ = firstZ;

    // Bounding box
    for (unsigned i = 0; i < polypoints.Size() - 1; i += 2)
    {
        float x = polypoints[i];
        float z = polypoints[i + 1];

        if (x < minX) minX = x;
        else if (x > maxX) maxX = x;
        if (z < minZ) minZ = z;
        else if (z > maxZ) maxZ = z;
    }
    Vector3 size = Vector3(maxX - minX, 0.0f, maxZ - minZ);
    bbox = BoundingBox(Vector3(-size.x_ * 0.5f, 0.0f, -size.z_ * 0.5f), Vector3(size.x_ * 0.5f, dummy ? 10.0f : 0.0f, size.z_ * 0.5f));

    // Mesh center (node position)
    Vector3 center = Vector3(maxX, 0.0f, maxZ) - size * 0.5f;

    // Store vertices and normals
    for (unsigned i = 0; i < polypoints.Size() - 1; i += 2)
    {
        float x = polypoints[i] - center.x_;
        float z = polypoints[i + 1] - center.z_;

        // Vertices
        vertexData.Push(x);
        vertexData.Push(dummy ? 1.0f : 0.0f); // y
        vertexData.Push(z);

        // Normals
        for (unsigned n = 0; n < 3; ++n)
            vertexData.Push(0.0f);
    }

        /// TODO: REMOVE DUPLICATE VERTICES

    // Dummy grounded face (one vertice below floor level)
    if (dummy)
    {
        for (unsigned i = 0; i < 18; ++i)
            vertexData.Push(0.0f);
        vertexData[vertexData.Size() - 5] = -1.0f; // Grounded y vertice
    }

    // Number of vertices
    numVertices = vertexData.Size() / 6;

    // Indices (faces)
    for (uint16 i = 0; i < numVertices; ++i)
        indexData.Push(i);

    return center;
}


Node* TileMap2D::CreateProceduralModel(Vector<float> polypoints, bool dummy, Node* node)
{
    if (polypoints.Size() < 2)
        return 0;

    // Store vertices in Urho format
    unsigned numVertices = 0;
    PODVector<float> vertexData;
    PODVector<unsigned short> indexData;
    BoundingBox bbox;

    Vector3 center = StoreVertices(numVertices, vertexData, indexData, bbox, polypoints, dummy);

    // Vertex elements
    PODVector<VertexElement> elements;
    elements.Push(VertexElement(TYPE_VECTOR3, SEM_POSITION));
    ///    if (!dummy) // Do not render
        elements.Push(VertexElement(TYPE_VECTOR3, SEM_NORMAL));

    // Vertices
    SharedPtr<VertexBuffer> vb(new VertexBuffer(context_));
    vb->SetShadowed(true);
    vb->SetSize(numVertices, elements);
    vb->SetData(&vertexData[0]);

    // Indices
    SharedPtr<IndexBuffer> ib(new IndexBuffer(context_));
    ib->SetShadowed(true);
    ib->SetSize(numVertices, false);
    ib->SetData(&indexData[0]);

    // Geometry
    SharedPtr<Geometry> geom(new Geometry(context_));
    geom->SetVertexBuffer(0, vb);
    geom->SetIndexBuffer(ib);
    geom->SetDrawRange(TRIANGLE_LIST, 0, numVertices);

    // Create model
    SharedPtr<Model> model(new Model(context_));
    model->SetNumGeometries(1);
    model->SetGeometry(0, 0, geom);
    model->SetBoundingBox(bbox);

    // Create node
    if (!node)
        node = GetScene()->CreateChild("ProceduralObject");
    StaticModel* object = node->CreateComponent<StaticModel>();
    object->SetViewMask(128); // Enable last layer only to ease removal
    object->SetModel(model);

    // In case this method is used externally
    if (!rootNode_)
        return node;

    // Parent node to navMesh node
    Node* navNode = rootNode_->GetChild("NavMesh", true);
    if (!navNode)
        return node;

    node->SetParent(navNode);
    node->SetTransform(center, Quaternion());

    return node;
}

void TileMap2D::CreatePhysicsFromObjects()
{
    // Get "Physics" layer
    TileMapLayer2D* tileMapLayer = GetLayer("Physics");
    if (!tileMapLayer)
        return;

    // Create rigid bodies and collision shapes
    for (unsigned i = 0; i < tileMapLayer->GetNumObjects(); ++i)
    {
        TileMapObject2D* tileMapObject = tileMapLayer->GetObject(i);
        if (tileMapObject->GetType() == "NavMesh")
            continue;
        CreatePhysicsFromObject(tileMapObject, Vector2::ZERO, tileMapObject->GetObjectType() == OT_TILE ? tileMapLayer->GetObjectNode(i) : 0);
    }
}

void TileMap2D::CreateConstraintsFromObjects()
{
    // Get "Constraints" layer
    TileMapLayer2D* tileMapLayer = GetLayer("Constraints");
    if (!tileMapLayer)
        return;

    // Create rigid body and collision shape(s) for each object, except links
    for (unsigned i = 0; i < tileMapLayer->GetNumObjects(); ++i)
    {
        Vector2 positionOffset;
        TileMapObject2D* tileMapObject = tileMapLayer->GetObject(i);
        if (tileMapObject->GetType() == "CollisionShape2D")
        {
            // For tile objects, Sprite2D hotspot has been set to left-bottom. For constraints we need to have it centered
            if (tileMapObject->GetObjectType() == OT_TILE)
            {
                Node* objectNode = tileMapLayer->GetObjectNode(i);
                if (objectNode)
                {
                    StaticSprite2D* sprite = objectNode->GetComponent<StaticSprite2D>();
                    if (sprite)
                        sprite->GetUseHotSpot() ? sprite->SetHotSpot(Vector2(0.5f, 0.5f)) : sprite->SetUseHotSpot(true);

                    IntVector2 spriteSize = tileMapObject->GetTileSprite()->GetRectangle().Size(); // Convert size back to pixels
                    Vector2 size = Vector2(spriteSize.x_, spriteSize.y_) * PIXEL_SIZE * 0.5f;
                    Vector2 offset = tileMapObject->GetSize() * size; // Center
                    positionOffset -= size; // Move shape to center
                    objectNode->SetWorldPosition(objectNode->GetWorldPosition2D() + offset); // Move node to center
                }
            }
            CreatePhysicsFromObject(tileMapObject, positionOffset, tileMapObject->GetObjectType() == OT_TILE ? tileMapLayer->GetObjectNode(i) : 0);
        }
    }

    // Create constraints
    for (unsigned i = 0; i < tileMapLayer->GetNumObjects(); ++i)
    {
        TileMapObject2D* tileMapObject = tileMapLayer->GetObject(i);
        bool hasProperties = tileMapObject->GetNumProperties();
        String constraintType = tileMapObject->GetType();
        if (constraintType.Empty() || constraintType == "CollisionShape2D")
            continue;

        unsigned numPoints = tileMapObject->GetNumPoints();
        if (numPoints < 2 || numPoints > 5)
            continue;

        Vector2 ownerAnchor = tileMapObject->GetPoint(0);  // First point
        Vector2 otherAnchor = tileMapObject->GetPoint(numPoints - 1); // Last point

        // Get the 2 rigid bodies to constrain together
        PhysicsWorld2D* physicsWorld = GetScene()->GetComponent<PhysicsWorld2D>();
        RigidBody2D* ownerBody = physicsWorld->GetRigidBody(ownerAnchor);
        RigidBody2D* otherBody = physicsWorld->GetRigidBody(otherAnchor);

        if (!ownerBody || !otherBody)
        {
            URHO3D_LOGINFO("Cannot find owner and/or other bodie(s) for building " + constraintType + " " + tileMapObject->GetName());
            continue;
        }

        // If rigid bodies are overlapping, deepen selection
        if (ownerBody == otherBody)
        {
            PODVector<RigidBody2D*> bodies;
            physicsWorld->GetRigidBodies(bodies, Rect(otherAnchor - Vector2(0.01f, 0.01f), otherAnchor + Vector2(0.01f, 0.01f)));
            if (bodies.Size() > 1)
                otherBody = bodies[1];
            else
            {
                bodies.Clear();
                physicsWorld->GetRigidBodies(bodies, Rect(ownerAnchor - Vector2(0.01f, 0.01f), ownerAnchor + Vector2(0.01f, 0.01f)));
                if (bodies.Size() > 1)
                    ownerBody = bodies[1];
            }
        }

        // If rigid bodies are still identical, we need to improve detection
        if (ownerBody == otherBody)
            URHO3D_LOGINFO("Failed to create constraint: OwnerBody and OtherBody are identical");

        // Get nodes
        Node* ownerNode = ownerBody->GetNode();
        Node* otherNode = otherBody->GetNode();

        // If anchor points are almost centered, do not consider them as distinct anchors
        Vector2 ownerCenter = ownerNode->GetPosition2D();
        bool ownerCentered = Rect(ownerCenter - Vector2(0.01f, 0.01f), ownerCenter + Vector2(0.01f, 0.01f)).IsInside(ownerAnchor) == INSIDE;

        Vector2 otherCenter = otherNode->GetPosition2D();
        bool otherCentered = Rect(otherCenter - Vector2(0.01f, 0.01f), otherCenter + Vector2(0.01f, 0.01f)).IsInside(otherAnchor) == INSIDE;

        // Create constraints between bodyA and bodyB
        Constraint2D* cs;

        if (constraintType == "ConstraintDistance2D")
        {
            // Note: use of intermediate points allows to set anchor points outside of the shapes
            if (numPoints >1 && numPoints < 5)
            {
                cs = ownerNode->CreateComponent<ConstraintDistance2D>();
                ConstraintDistance2D* constraint  = reinterpret_cast<ConstraintDistance2D*>(cs);
                constraint->SetOtherBody(otherBody);
                if (numPoints == 2)
                {
                    constraint->SetOwnerBodyAnchor(ownerAnchor); // First point
                    constraint->SetOtherBodyAnchor(otherAnchor); // Last point
                }
                else if (numPoints == 3)
                {
                    constraint->SetOwnerBodyAnchor(tileMapObject->GetPoint(1)); // Middle point
                    constraint->SetOtherBodyAnchor(otherAnchor); // Last point
                }
                else if (numPoints == 4)
                {
                    constraint->SetOwnerBodyAnchor(tileMapObject->GetPoint(1)); // Second point
                    constraint->SetOtherBodyAnchor(tileMapObject->GetPoint(2)); // Third point
                }

                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("FrequencyHz")) constraint->SetFrequencyHz(ToFloat(tileMapObject->GetProperty("FrequencyHz")));
                if (tileMapObject->HasProperty("DampingRatio")) constraint->SetDampingRatio(ToFloat(tileMapObject->GetProperty("DampingRatio")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'distance' constraint for object " + tileMapObject->GetName() + " : 2-4 points required");
        }

        else if (constraintType == "ConstraintFriction2D")
        {
            if (numPoints == 2 || numPoints == 3)
            {
                cs = ownerNode->CreateComponent<ConstraintFriction2D>();
                ConstraintFriction2D* constraint  = reinterpret_cast<ConstraintFriction2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetAnchor(tileMapObject->GetPoint(1)); // 2nd point
                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("MaxForce")) constraint->SetMaxForce(ToFloat(tileMapObject->GetProperty("MaxForce")));
                if (tileMapObject->HasProperty("MaxTorque")) constraint->SetMaxTorque(ToFloat(tileMapObject->GetProperty("MaxTorque")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'friction' constraint for object " + tileMapObject->GetName() + " : 2-3 points required");
        }

        else if (constraintType == "ConstraintGear2D")
        {
            if (numPoints == 2)
            {
                // Ensure that we can access the involved constraints
                unsigned ownerConstraintID = ownerNode->GetVar(StringHash("GearID")).GetUInt();
                unsigned otherConstraintID = otherNode->GetVar(StringHash("GearID")).GetUInt();
                if (ownerConstraintID == 0 || otherConstraintID == 0)
                {
                    URHO3D_LOGINFO("Cannot create 'gear' constraint: participating nodes must be flagged as 'Geared' in Tiled");
                    continue;
                }

                Constraint2D* ownerConstraint = static_cast<Constraint2D*>(GetScene()->GetComponent(ownerConstraintID));
                Constraint2D* otherConstraint = static_cast<Constraint2D*>(GetScene()->GetComponent(otherConstraintID));
                if (!ownerConstraint || !otherConstraint)
                {
                    URHO3D_LOGINFO("Cannot create 'gear' constraint: cannot find participating constraints");
                    continue;
                }

                // Create constraint
                cs = ownerNode->CreateComponent<ConstraintGear2D>();
                ConstraintGear2D* constraint  = reinterpret_cast<ConstraintGear2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetOwnerConstraint(ownerConstraint);
                constraint->SetOtherConstraint(otherConstraint);
                if (tileMapObject->HasProperty("Ratio")) constraint->SetRatio(ToFloat(tileMapObject->GetProperty("Ratio")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'gear' constraint for object " + tileMapObject->GetName() + " : 2 points required");
        }

        else if (constraintType == "ConstraintMotor2D")
        {
            if (numPoints == 2)
            {
                cs = ownerNode->CreateComponent<ConstraintMotor2D>();
                ConstraintMotor2D* constraint  = reinterpret_cast<ConstraintMotor2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetLinearOffset(otherNode->GetPosition2D() - ownerNode->GetPosition2D()); // Offset from owner center, which defines the other rest position

                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("AngularOffset")) constraint->SetAngularOffset(ToFloat(tileMapObject->GetProperty("AngularOffset")));
                if (tileMapObject->HasProperty("MaxForce")) constraint->SetMaxForce(ToFloat(tileMapObject->GetProperty("MaxForce")));
                if (tileMapObject->HasProperty("MaxTorque")) constraint->SetMaxTorque(ToFloat(tileMapObject->GetProperty("MaxTorque")));
                if (tileMapObject->HasProperty("CorrectionFactor")) constraint->SetCorrectionFactor(ToFloat(tileMapObject->GetProperty("CorrectionFactor")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'motor' constraint for object " + tileMapObject->GetName() + " : 2 points required");
        }

        else if (constraintType == "ConstraintPrismatic2D")
        {
            if (numPoints == 5)
            {
                cs = ownerNode->CreateComponent<ConstraintPrismatic2D>();
                ConstraintPrismatic2D* constraint  = reinterpret_cast<ConstraintPrismatic2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetAnchor(tileMapObject->GetPoint(1)); // Second point
                Vector2 axis = (tileMapObject->GetPoint(3) - tileMapObject->GetPoint(2)).Normalized(); // Normalized direction between points 3 and 4
                constraint->SetAxis(axis);
                constraint->SetLowerTranslation(-(tileMapObject->GetPoint(2) - tileMapObject->GetPoint(1)).Length());
                constraint->SetUpperTranslation((tileMapObject->GetPoint(3) - tileMapObject->GetPoint(1)).Length());
                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("EnableLimit")) constraint->SetEnableLimit(ToBool(tileMapObject->GetProperty("EnableLimit")));
                if (tileMapObject->HasProperty("EnableMotor")) constraint->SetEnableMotor(ToBool(tileMapObject->GetProperty("EnableMotor")));
                if (tileMapObject->HasProperty("MaxMotorForce")) constraint->SetMaxMotorForce(ToFloat(tileMapObject->GetProperty("MaxMotorForce")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'prismatic' constraint for object " + tileMapObject->GetName() + " : 5 points required");
        }

        else if (constraintType == "ConstraintPulley2D")
        {
            // Note: it is assumed that owner and other anchors are inside their respective shapes. We could allow 2 more points to overcome this limitation
            if (numPoints == 4)
            {
                cs = ownerNode->CreateComponent<ConstraintPulley2D>();
                ConstraintPulley2D* constraint  = reinterpret_cast<ConstraintPulley2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetOwnerBodyAnchor(ownerAnchor); // First point
                constraint->SetOtherBodyAnchor(otherAnchor); // Last point
                constraint->SetOwnerBodyGroundAnchor(tileMapObject->GetPoint(1)); // Second point
                constraint->SetOtherBodyGroundAnchor(tileMapObject->GetPoint(2)); // Third point
                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("Ratio")) constraint->SetRatio(ToFloat(tileMapObject->GetProperty("Ratio")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'pulley' constraint for object " + tileMapObject->GetName() + " : 4 points required");
        }

        else if (constraintType == "ConstraintRevolute2D")
        {
            // Note: use of an intermediate point allows to set the anchor outside of the owner shape. Constraint is designed in rest pose, angles are in radians
            if (numPoints == 2 || numPoints == 3)
            {
                cs = ownerNode->CreateComponent<ConstraintRevolute2D>();
                ConstraintRevolute2D* constraint  = reinterpret_cast<ConstraintRevolute2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetAnchor(tileMapObject->GetPoint(numPoints == 2 ? 0 : 1)); // First or second point
                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("EnableLimit")) constraint->SetEnableLimit(ToBool(tileMapObject->GetProperty("EnableLimit")));
                if (tileMapObject->HasProperty("LowerAngle")) constraint->SetLowerAngle(ToFloat(tileMapObject->GetProperty("LowerAngle")));
                if (tileMapObject->HasProperty("UpperAngle")) constraint->SetUpperAngle(ToFloat(tileMapObject->GetProperty("UpperAngle")));
                if (tileMapObject->HasProperty("EnableMotor")) constraint->SetEnableMotor(ToBool(tileMapObject->GetProperty("EnableMotor")));
                if (tileMapObject->HasProperty("MotorSpeed")) constraint->SetMotorSpeed(ToFloat(tileMapObject->GetProperty("MotorSpeed")));
                if (tileMapObject->HasProperty("MaxMotorTorque")) constraint->SetMaxMotorTorque(ToFloat(tileMapObject->GetProperty("MaxMotorTorque")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'revolute' constraint for object " + tileMapObject->GetName() + " : 2-3 points required");
        }

        else if (constraintType == "ConstraintRope2D")
        {
            // Note: anchors are offsets from node center. The rope is rigid from center to anchor
            // Use of intermediate points allows to set anchors outside of the shapes

            if (numPoints > 1 && numPoints < 5)
            {
                cs = ownerNode->CreateComponent<ConstraintRope2D>();
                ConstraintRope2D* constraint  = reinterpret_cast<ConstraintRope2D*>(cs);
                constraint->SetOtherBody(otherBody);
                float length;

                if (numPoints == 2)
                {
                    constraint->SetOwnerBodyAnchor(ownerAnchor - ownerNode->GetWorldPosition2D());
                    constraint->SetOtherBodyAnchor(otherAnchor - otherNode->GetWorldPosition2D());
                    length = (ownerAnchor - otherAnchor).Length();
                }
                if (numPoints == 3)
                {
                    constraint->SetOwnerBodyAnchor(tileMapObject->GetPoint(1) - ownerNode->GetWorldPosition2D());
                    constraint->SetOtherBodyAnchor(otherAnchor - otherNode->GetWorldPosition2D());
                    length = (tileMapObject->GetPoint(1) - otherAnchor).Length();
                }
                else if (numPoints == 4)
                {
                    constraint->SetOwnerBodyAnchor(tileMapObject->GetPoint(1) - ownerNode->GetWorldPosition2D());
                    constraint->SetOtherBodyAnchor(tileMapObject->GetPoint(2) - otherNode->GetWorldPosition2D());
                    length = (ownerAnchor - otherAnchor).Length();
                }
                constraint->SetMaxLength(length);
            }
            else
                URHO3D_LOGINFO("Failed to create 'rope' constraint for object " + tileMapObject->GetName() + " : 2-4 points required");
        }

        else if (constraintType == "ConstraintWeld2D")
        {
            // Note: use of an intermediate point allows to set the other anchor outside of its shape
            if (numPoints == 2 || numPoints == 3)
            {
                cs = ownerNode->CreateComponent<ConstraintWeld2D>();
                ConstraintWeld2D* constraint  = reinterpret_cast<ConstraintWeld2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetAnchor(tileMapObject->GetPoint(1)); // Second point (which can be the last one)

                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("FrequencyHz")) constraint->SetFrequencyHz(ToFloat(tileMapObject->GetProperty("FrequencyHz")));
                if (tileMapObject->HasProperty("DampingRatio")) constraint->SetDampingRatio(ToFloat(tileMapObject->GetProperty("DampingRatio")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'weld' constraint for object " + tileMapObject->GetName() + " : 2-3 points required");
        }

        else if (constraintType == "ConstraintWheel2D")
        {
            // Note: use of an intermediate point allows to set the other anchor outside of its shape
            if (numPoints == 2 || numPoints == 3)
            {
                cs = ownerNode->CreateComponent<ConstraintWheel2D>();
                ConstraintWheel2D* constraint  = reinterpret_cast<ConstraintWheel2D*>(cs);
                constraint->SetOtherBody(otherBody);
                constraint->SetAnchor(tileMapObject->GetPoint(1)); // Second point (which can be the last one)
                Vector2 axis = tileMapObject->HasProperty("Axis") ? ToVector2(tileMapObject->GetProperty("Axis")) : (otherAnchor - ownerAnchor).Normalized(); // Normalized direction between other and owner center
                constraint->SetAxis(axis);

                if (!hasProperties)
                    continue;
                if (tileMapObject->HasProperty("EnableMotor")) constraint->SetEnableMotor(ToBool(tileMapObject->GetProperty("EnableMotor")));
                if (tileMapObject->HasProperty("MaxMotorTorque")) constraint->SetMaxMotorTorque(ToFloat(tileMapObject->GetProperty("MaxMotorTorque"))); /// Other rotation (higher values disable rotation)
                if (tileMapObject->HasProperty("MotorSpeed")) constraint->SetMotorSpeed(ToFloat(tileMapObject->GetProperty("MotorSpeed")));
                if (tileMapObject->HasProperty("FrequencyHz")) constraint->SetFrequencyHz(ToFloat(tileMapObject->GetProperty("FrequencyHz")));
                if (tileMapObject->HasProperty("DampingRatio")) constraint->SetDampingRatio(ToFloat(tileMapObject->GetProperty("DampingRatio")));
            }
            else
                URHO3D_LOGINFO("Failed to create 'wheel' constraint for object " + tileMapObject->GetName() + " : 2-3 points required");
        }

        if (tileMapObject->HasProperty("CollideConnected")) cs->SetCollideConnected(ToBool(tileMapObject->GetProperty("CollideConnected")));

        // Store constraint ID if it's part of a gear constraint (flagged as 'Geared' in Tiled)
        if (ToBool(tileMapObject->GetProperty("Geared")))
            otherNode->SetVar(StringHash("GearID"), cs->GetID());
    }
}

void TileMap2D::CreatePhysicsFromObject(TileMapObject2D* tileMapObject, Vector2 positionOffset, Node* node)
{
    /// If we have a navMesh, create one node per object /// TODO: add a property to allow this option for physics either
    Node* navNode = rootNode_->GetChild("NavMesh", true);
    if (navNode)
        node = navNode->CreateChild("NavObstacle");

    if (!node)
        node = rootNode_;

    // Create rigid body if it doesn't exist
    RigidBody2D* body = node->GetComponent<RigidBody2D>();
    if (!body)
        body = node->CreateComponent<RigidBody2D>();

    // Set rigid body properties
    if (tileMapObject->GetNumProperties() > 0)
    {
        if (tileMapObject->HasProperty("BodyType"))
            body->SetBodyType(tileMapObject->GetProperty("BodyType") == "Dynamic" ? BT_DYNAMIC : BT_KINEMATIC);
        if (tileMapObject->HasProperty("Mass")) body->SetMass(ToFloat(tileMapObject->GetProperty("Mass")));
        if (tileMapObject->HasProperty("Inertia")) body->SetInertia(ToFloat(tileMapObject->GetProperty("Inertia")));
        if (tileMapObject->HasProperty("MassCenter")) body->SetMassCenter(ToVector2(tileMapObject->GetProperty("MassCenter")));
        if (tileMapObject->HasProperty("UseFixtureMass")) body->SetUseFixtureMass(ToBool(tileMapObject->GetProperty("UseFixtureMass")));
        if (tileMapObject->HasProperty("LinearDamping")) body->SetLinearDamping(ToFloat(tileMapObject->GetProperty("LinearDamping")));
        if (tileMapObject->HasProperty("AngularDamping")) body->SetAngularDamping(ToFloat(tileMapObject->GetProperty("AngularDamping")));
        if (tileMapObject->HasProperty("AllowSleep")) body->SetAllowSleep(ToBool(tileMapObject->GetProperty("AllowSleep")));
        if (tileMapObject->HasProperty("FixedRotation")) body->SetFixedRotation(ToBool(tileMapObject->GetProperty("FixedRotation")));
        if (tileMapObject->HasProperty("Bullet")) body->SetBullet(ToBool(tileMapObject->GetProperty("Bullet")));
        if (tileMapObject->HasProperty("GravityScale")) body->SetGravityScale(ToFloat(tileMapObject->GetProperty("GravityScale")));
        if (tileMapObject->HasProperty("Awake")) body->SetAwake(ToBool(tileMapObject->GetProperty("Awake")));
        if (tileMapObject->HasProperty("LinearVelocity")) body->SetLinearVelocity(ToVector2(tileMapObject->GetProperty("LinearVelocity")));
        if (tileMapObject->HasProperty("AngularVelocity")) body->SetAngularVelocity(ToFloat(tileMapObject->GetProperty("AngularVelocity")));
    }

    Vector2 size = tileMapObject->GetSize();

    // Tile object can hold any collision shape type and can hold compound shapes
    Vector<TileMapObject2D*> objects;

    bool isTile = false;

    if (tileMapObject->GetObjectType() == OT_TILE)
    {
        // Convert size back to pixels
        IntVector2 spriteSize = tileMapObject->GetTileSprite()->GetRectangle().Size();
        size = Vector2(spriteSize.x_ * PIXEL_SIZE, spriteSize.y_ * PIXEL_SIZE);

        isTile = true;

        Vector<SharedPtr<TileMapObject2D> > tileShapes = tileMapObject->GetTileCollisionShapes();
        if (!tileShapes.Empty())
        {
            for (Vector<SharedPtr<TileMapObject2D> >::ConstIterator i = tileShapes.Begin(); i != tileShapes.End(); ++i)
                objects.Push(*i);
        }
    }
    if (objects.Empty())
        objects.Push(tileMapObject);

    // Create collision shape for each object
    for (unsigned i = 0; i < objects.Size(); ++i)
    {
        TileMapObject2D* subObject = objects[i];
        Vector<CollisionShape2D*> shapes;
        TileMapObjectType2D type = subObject->GetObjectType();

        switch (type)
        {
            case OT_RECTANGLE:
            case OT_TILE:
            {
                CollisionShape2D* shape = node->CreateComponent<CollisionBox2D>();
                reinterpret_cast<CollisionBox2D*>(shape)->SetSize(size);

                // Rotate shape according to custom rotation
                float rotation = subObject->GetRotation();
                if (rotation != 0.0f)
                    reinterpret_cast<CollisionBox2D*>(shape)->SetAngle(rotation);

                // Apply rotated position
                Vector3 pos = node == rootNode_ ? subObject->GetPosition() + positionOffset : positionOffset; // Position in map world or local position

                Vector2 center;
                if (type == OT_RECTANGLE)
                    center = Vector2(size.x_, -size.y_) * 0.5f; // Pivot for rectangle object is left-top
                else // Tile object
                    center = Vector2(size.x_, size.y_) * 0.5f; // Tile object pivot is left-bottom (in isometric orientation it is middle-bottom but we've already fixed this when storing position)
                pos += Quaternion(0.0f, 0.0f, rotation) * Vector3(center); // Rotated world position center
                reinterpret_cast<CollisionBox2D*>(shape)->SetCenter(Vector2(pos.x_, pos.y_));

                shapes.Push(shape);
            }
            break;

            case OT_ELLIPSE: // Note: spherical only, as other elliptic shapes have been converted to polyline
            {
                Vector2 objectSize = subObject->GetSize(); // Sub-object size for tile object

                CollisionShape2D* shape = node->CreateComponent<CollisionCircle2D>();
                CollisionCircle2D* circle = reinterpret_cast<CollisionCircle2D*>(shape);
                if (node == rootNode_)
                    circle->SetCenter(subObject->GetPosition() + positionOffset + Vector2(objectSize.x_, -objectSize.y_) * 0.5f - Vector2(info_.orientation_ == O_ISOMETRIC ? info_.tileWidth_ * 0.25f : 0.0f, 0.0f));
                else
                    circle->SetCenter(Vector2::ZERO);
                circle->SetRadius(objectSize.x_ * 0.5f);
                shapes.Push(shape);

                /// NavMesh spheres
                if (rootNode_->GetChild("NavMesh", true))
                {
                    Points points;
                    ConvertEllipseToPoints(points, tileMapObject);
                    points.Pop(); // Remove closing point
                    Vector<float> vertices;
                    Triangulate(vertices, points);
                    CreateProceduralModel(vertices, true, node);
                }
            }
            break;

            case OT_POLYGON:
                CreatePolygonShape(shapes, subObject, positionOffset, node, isTile);
                break;

            case OT_POLYLINE:
                isTile ? CreatePolygonShape(shapes, subObject, positionOffset, node, isTile) : shapes.Push(CreatePolyLineShape(subObject, positionOffset, node));
                break;

            default: break;
        }

        if (tileMapObject->GetNumProperties() == 0)
            continue;

        // Set collision shape(s) properties. Note: only the main object (tilemapObject) holds shape settings, there are no per shape settings in a compound setup
        for (unsigned s = 0; s < shapes.Size(); ++s)
        {
            CollisionShape2D* shape = shapes[s];
            if (!shape)
                continue;

            if (tileMapObject->HasProperty("Trigger")) shape->SetTrigger(ToBool(tileMapObject->GetProperty("Trigger")));
            if (tileMapObject->HasProperty("CategoryBits")) shape->SetCategoryBits(ToInt(tileMapObject->GetProperty("CategoryBits")));
            if (tileMapObject->HasProperty("MaskBits")) shape->SetMaskBits(ToInt(tileMapObject->GetProperty("MaskBits"))); /// 65535
            if (tileMapObject->HasProperty("GroupIndex")) shape->SetGroupIndex(ToInt(tileMapObject->GetProperty("GroupIndex")));
            if (tileMapObject->HasProperty("Density")) shape->SetDensity(ToFloat(tileMapObject->GetProperty("Density")));
            if (tileMapObject->HasProperty("Friction")) shape->SetFriction(ToFloat(tileMapObject->GetProperty("Friction")));
            if (tileMapObject->HasProperty("Restitution")) shape->SetRestitution(ToFloat(tileMapObject->GetProperty("Restitution")));
        }
    }
}

void TileMap2D::CreatePolygonShape(Vector<CollisionShape2D*>& shapes, TileMapObject2D* tileMapObject, Vector2 positionOffset, Node* node, bool isTile)
{
    TileMapObjectType2D type = tileMapObject->GetObjectType();
    Points points;

    // Convert rectangle (hollow) to polygon (solid)
    if (type == OT_RECTANGLE)
    {
        float rotation = tileMapObject->GetRotation();
        Vector2 size = tileMapObject->GetSize();
        float ratio = (info_.tileWidth_ / info_.tileHeight_) * 0.5f;
        points.Push(Vector2::ZERO);
        points.Push(Vector2(size.x_ * ratio, -size.x_ * 0.5f));
        points.Push(Vector2((size.x_ - size.y_) * ratio, (-size.y_ - size.x_) * 0.5f));
        points.Push(Vector2(-size.y_ * ratio, -size.y_ * 0.5f));
        points.Push(Vector2::ZERO);

        for (unsigned i = 0; i < points.Size(); ++i)
            points[i] = tileMapObject->GetPosition() + tileMapObject->RotatedPosition(points[i], rotation);
    }
    else
    {
        for (unsigned i = 0; i < tileMapObject->GetNumPoints(); ++i)
            points.Push(tileMapObject->GetPoint(i));
        if (type == OT_POLYLINE)
            points.Pop(); // Remove closing point
    }

    /// Create navMesh 3D triangulated shape
    bool navMesh = rootNode_->GetChild("NavMesh", true);
    if (navMesh)
    {
        Vector<float> vertices;
        Points offPoints;
        for (unsigned i = 0; i < points.Size(); ++i)
            offPoints.Push(points[i] + positionOffset);
        Triangulate(vertices, offPoints);
        CreateProceduralModel(vertices, true, node);
    }

    // Decompose Tiled polygon into convex polygons
    Vector<Points> polygons;
    if (tileMapObject->GetNumPoints() > 3)
    {
        if (!DecomposePolygon(polygons, points))
            URHO3D_LOGINFO("Failed to decompose polygon " + tileMapObject->GetName());
    }

    // Fail-safe if decomposition failed (self-intersecting) or triangle (no need to decompose)
    if (polygons.Size() == 0)
        polygons.Push(points);

    // When using a navMesh, node position becomes centered, so we'll need to clear this offset ///\todo: apply this when allowing to create per object physics
    Vector3 newPos = mapRotation_ * node->GetPosition();

    // Create one collision shape for each polygon
    for (unsigned p = 0; p < polygons.Size(); ++p)
    {
        points = polygons[p];
        CollisionShape2D* shape = node->CreateComponent<CollisionPolygon2D>();
        CollisionPolygon2D* polygon = reinterpret_cast<CollisionPolygon2D*>(shape);
        polygon->SetVertexCount(points.Size());
        for (unsigned i = 0; i < polygon->GetVertexCount(); ++i)
            polygon->SetVertex(i, points[i] + positionOffset - (navMesh ? Vector2(newPos.x_, newPos.y_) : Vector2::ZERO));
        shapes.Push(shape);
    }
}

CollisionShape2D* TileMap2D::CreatePolyLineShape(TileMapObject2D* tileMapObject, Vector2 positionOffset, Node* node)
{
    TileMapObjectType2D type = tileMapObject->GetObjectType();
    CollisionShape2D* shape = node->CreateComponent<CollisionChain2D>();
    CollisionChain2D* chain = reinterpret_cast<CollisionChain2D*>(shape);

    Points points;
    Points vertices;
    bool navMesh = rootNode_->GetChild("NavMesh", true);

    if (type == OT_ELLIPSE)
        ConvertEllipseToPoints(points, tileMapObject);
    else
    {
        for (unsigned i = 0; i < tileMapObject->GetNumPoints(); ++i)
            points.Push(tileMapObject->GetPoint(i));
    }

    /// Create navMesh 3D triangulated shape
    if (navMesh)
    {
        ModelFromPolyline(points, node);

        // When using a navMesh, node position becomes centered, so we need to clear this offset ///\todo: apply this when allowing to create per object physics
        Vector3 newPos = mapRotation_ * node->GetPosition();
        positionOffset -= Vector2(newPos.x_, newPos.y_);
    }

    chain->SetVertexCount(points.Size());
    for (unsigned i = 0; i < points.Size(); ++i)
    {
        Vector2 point = points[i] + positionOffset;
        chain->SetVertex(i, point);
    }

    return shape;
}

Node* TileMap2D::ModelFromPolyline(Points points, Node* node)
{
    if (points.Empty() || !rootNode_->GetChild("NavMesh", true))
        return 0;

    Vector<float> vertices;
    unsigned numPoints = points.Size();

    // If closed shape, treat as a polygon
    if (numPoints > 3 && points[0] == points[numPoints - 1])
    {
        points.Pop(); // Remove closing point
        Triangulate(vertices, points);
    }
    else
    {
        for (unsigned i = 0; i < numPoints; ++i)
        {
            Vector2 point = points[i];
            vertices.Push(point.x_);
            vertices.Push(point.y_);

            // Duplicate intermediate vertices
            if (i > 0 && i < numPoints - 1)
            {
                vertices.Push(point.x_);
                vertices.Push(point.y_);
            }
        }
        EdgesToTriangles(vertices);
    }
    return CreateProceduralModel(vertices, true, node);
}

void TileMap2D::ConvertEllipseToPoints(Points& points, TileMapObject2D* tileMapObject, bool isTile)
{
    float rotation = tileMapObject->GetRotation();
    Vector2 halfSize = tileMapObject->GetSize() * 0.5f;

    for (unsigned i = 0; i <= 360; i += 45) // 8 vertices so that we end up with only one polygon
    {
        Vector2 point = Vector2(halfSize.x_ * Cos((float)i), halfSize.y_ * Sin((float)i)) + Vector2(halfSize.x_, -halfSize.y_);
        if (info_.orientation_ == O_ISOMETRIC && !isTile) // Note: we don't convert tile objects from rectangle to diamond
        {
            float ratio = (info_.tileWidth_ / info_.tileHeight_) * 0.5f;
            point = Vector2((point.x_ + point.y_) * ratio, (point.y_ - point.x_) * 0.5f);
        }

        // Apply rotation and store
        point = tileMapObject->RotatedPosition(point, rotation);
        points.Push(tileMapObject->GetPosition() + point);
    }
}

bool TileMap2D::DecomposePolygon(Vector<Points>& polygons, Points points)
{
    // Convert polygon points to Bayazit format
    Bayazit::Polygon poly;
    for (unsigned i = 0; i < points.Size(); ++i)
        poly.push_back(Bayazit::Point(points[i].x_, points[i].y_));

    // Decompose polygon into convex polygons
    Bayazit::Decomposer dec;
    std::vector<Bayazit::Polygon> polys = dec.Decompose(poly);

    // Convert convex polygons to Urho3D format
    for (unsigned i = 0; i < polys.size(); ++i)
    {
        Bayazit::Polygon p = polys[i];
        Points points;
        for (unsigned j = 0; j < p.size(); ++j)
            points.Push(Vector2(p[j].x, p[j].y));

        polygons.Push(points);
    }
    return polygons.Size() > 0;
}

bool TileMap2D::Triangulate(Vector<float>& vertices, Points points)
{
    // At least 2 points are required to form a triangle
    if (points.Size() < 4)
        return true;

    // Remove duplicate points
    Points clean;
    for (unsigned i = 0; i < points.Size(); ++i)
    {
        if (clean.Contains(points[i]))
            continue;
        clean.Push(points[i]);
    }
    points = clean;

    // The maximum number of points we expect to need (used to calculate required working memory)
    uint32_t MaxPointCount = 3000;

    // Request how much memory (in bytes) we should allocate for the library
    size_t MemoryRequired = MPE_PolyMemoryRequired(MaxPointCount);

    // Allocate a void* memory block of size MemoryRequired. IMPORTANT: The memory must be zero initialized
    void* Memory = calloc(MemoryRequired, 1);

    // Initialize the poly context by passing the memory pointer, and max number of points from before
    MPEPolyContext PolyContext;

    if (MPE_PolyInitContext(&PolyContext, Memory, MaxPointCount))
    {
        // Populate the points of the polyline for the shape to triangulate (one point at a time)
        for(unsigned i = 0; i < points.Size(); ++i)
        {
            MPEPolyPoint* Point = MPE_PolyPushPoint(&PolyContext);
            Point->X = points[i].x_;
            Point->Y = points[i].y_;
        }

        // Add the polyline for the edge. This will consume all points added so far.
        MPE_PolyAddEdge(&PolyContext);
    }

    // Triangulate the shape
    MPE_PolyTriangulate(&PolyContext);

    // Store resulting triangles vertices
    for (unsigned TriangleIndex = 0; TriangleIndex < PolyContext.TriangleCount; ++TriangleIndex)
    {
        MPEPolyTriangle* Triangle = PolyContext.Triangles[TriangleIndex];

        unsigned v[] = { 0, 2, 1 }; // Swap 2nd and last vertices
        for (unsigned p = 0; p < 3; ++p)
        {
            MPEPolyPoint* point = Triangle->Points[v[p]];
            vertices.Push(point->X);
            vertices.Push(point->Y);
        }
    }

    // Clear memory
    free(PolyContext.Allocator.Memory);

    return vertices.Size() > 3; // 2 vertices expected to form a triangle
}

NavigationMesh* TileMap2D::GetNavMesh() const
{
    Node* node = rootNode_->GetChild("NavMesh", true);
    if (!node)
        return 0;

    return node->GetDerivedComponent<NavigationMesh>(true);
}

void TileMap2D::AddObstacle(const Vector2& pos, Points points, Node* child)
{
    // Get navMesh
    NavigationMesh* navMesh = GetNavMesh();
    if (!navMesh)
        return;

    // Create procedural mesh from supplied vertices
    Node* node = ModelFromPolyline(points);
    if (!node)
        return;

    node->SetPosition(Quaternion(90.0f, 0.0f, 0.0f) * pos);

    // Rebuild modified part of the navMesh
    navMesh->Build(node->GetComponent<StaticModel>()->GetWorldBoundingBox());

    // Parent optional 2D drawable node to procedural node
    if (child)
    {
        child->SetParent(node);
        child->SetPosition(Vector3::ZERO);
    }
}

void TileMap2D::AddObstacle(const Vector2& pos, TileMapObject2D* obj, Node* child)
{
    // Get navMesh
    NavigationMesh* navMesh = GetNavMesh();
    if (!navMesh)
        return;

    // Create procedural mesh and physics from supplied object
    CreatePhysicsFromObject(obj, pos);

    // Get new node  parented to the navMesh root node
    Node* navNode = navMesh->GetNode();
    Node* n = navNode->GetChild(navNode->GetNumChildren() - 1);

    // Add 2D drawable if available
    Sprite2D* sprite = obj->GetTileSprite();
    if (sprite)
    {
        StaticSprite2D* staticSprite = child->CreateComponent<StaticSprite2D>();
        staticSprite->SetSprite(sprite);
    }

    child->SetParent(n);
    child->SetPosition(Vector3::ZERO);

    // Rebuild modified part of the navMesh
    navMesh->Build(n->GetComponent<StaticModel>()->GetWorldBoundingBox());
}

void TileMap2D::RemoveObstacle(Node* hitNode)
{
    // Get navMesh
    NavigationMesh* navMesh = GetNavMesh();

    if (!hitNode || !navMesh)
        return;

    // The part of the navMesh we must update, which is the world bounding box of the associated 3D drawable component
    Drawable* drawable = hitNode->GetDerivedComponent<Drawable>(true);
    if (!drawable)
        return;

    // Store bounding box before removing the node
    BoundingBox bbox = drawable->GetWorldBoundingBox();

    // Remove the node
    hitNode->Remove();

    // Rebuild part of the navigation mesh
    navMesh->Build(bbox);
}

bool TileMap2D::RebuildNavMesh()
{
    NavigationMesh* navMesh = GetNavMesh();
    return navMesh ? navMesh->Build() : false;
}

void TileMap2D::DetachConstraints(bool removeTileMap)
{
    Node* constraintsLayer = GetLayer("Constraints")->GetNode();
    PODVector<Node*> nodes;
    constraintsLayer->GetChildrenWithComponent<RigidBody2D>(nodes, true);
    for (unsigned i = 0; i < nodes.Size(); ++i)
        nodes[i]->SetParent(GetScene());

    if (removeTileMap)
        Remove();
}

}
