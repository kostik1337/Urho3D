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
#include "../Urho2D/TileMapDefs2D.h"

namespace Urho3D
{

class CollisionShape2D;
class NavigationMesh;
class TileMapLayer2D;
class TmxFile2D;

/// Tile map component.
class URHO3D_API TileMap2D : public Component
{
    URHO3D_OBJECT(TileMap2D, Component);

public:
    /// Construct.
    TileMap2D(Context* context);
    /// Destruct.
    ~TileMap2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Visualize the component as debug geometry.
    virtual void DrawDebugGeometry(DebugRenderer* debug, bool depthTest);

    /// Set tmx file.
    void SetTmxFile(TmxFile2D* tmxFile);
    /// Add debug geometry to the debug renderer.
    void DrawDebugGeometry();

    /// Return tmx file.
    TmxFile2D* GetTmxFile() const;

    /// Return information.
    const TileMapInfo2D& GetInfo() const { return info_; }

    /// Return number of layers.
    unsigned GetNumLayers() const { return layers_.Size(); }

    /// Return tile map layer at index.
    TileMapLayer2D* GetLayer(unsigned index) const;
    /// Return tile map layer by name.
    TileMapLayer2D* GetLayer(const String& name) const;
    /// Convert tile index to position.
    Vector2 TileIndexToPosition(int x, int y) const;
    /// Convert position to tile index, if out of map return false.
    bool PositionToTileIndex(int& x, int& y, const Vector2& position) const;

    /// Set tile map file attribute.
    void SetTmxFileAttr(const ResourceRef& value);
    /// Return tile map file attribute.
    ResourceRef GetTmxFileAttr() const;

    /// Create rigid bodies and collision shapes from TMX file objects.
    void CreatePhysicsFromObjects();
    /// Create Box2D constraints from TMX file objects.
    void CreateConstraintsFromObjects();
    /// Create rigid bodies and collision shapes for a given TMX file object.
    void CreatePhysicsFromObject(TileMapObject2D* tileMapObject, Vector2 positionOffset = Vector2::ZERO, Node* node = 0);
    /// Create navigation mesh if "Physics" layer contains an object of type "NavMesh".
    void CreateNavMesh(TileMapObject2D* navObject);
    /// Create CollisionPolygon2D for object.
    void CreatePolygonShape(Vector<CollisionShape2D*>& shapes, TileMapObject2D* tileMapObject, Vector2 positionOffset = Vector2::ZERO, Node* node = 0, bool isTile = false);
    /// Create CollisionChain2D for object.
    CollisionShape2D* CreatePolyLineShape(TileMapObject2D* tileMapObject, Vector2 positionOffset = Vector2::ZERO, Node* node = 0);
    /// Convert an ellipse to points.
    void ConvertEllipseToPoints(Vector<Vector2>& points, TileMapObject2D* tileMapObject, bool isTile = false);
    /// Decompose a polygon object into convex polygons. Return true on success.
    bool DecomposePolygon(Vector<Vector<Vector2> >& polygons, Vector<Vector2> points);
    /// Create procedural 3D shape to be used for navMesh generation.
    Node* CreateProceduralModel(Vector<float> polypoints, bool dummy = false, Node* = 0);
    /// Store objects' vertices in Urho3D format, to be used for procedural 3D shapes. Return model center position.
    Vector3 StoreVertices(unsigned& numVertices, PODVector<float>& vertexData, PODVector<unsigned short>& indexData, BoundingBox& bbox, Vector<float> polypoints, bool dummy = false);
    /// Convert edges (from a polyline) to triangles by duplicating the second vertice of each pair of points.
    void EdgesToTriangles(Vector<float>& points);
    /// Triangulate
    bool Triangulate(Vector<float>& vertices, Vector<Vector2> points);
    /// Create a procedural 3D model from polyline points.
    Node* ModelFromPolyline(Vector<Vector2> points, Node* node = 0);
    /// Get navigation mesh.
    NavigationMesh* GetNavMesh() const;
    /// Remove an obstacle from the navigation mesh.
    void RemoveObstacle(Node* hitNode);
    /// Add an obstacle to the navigation mesh. Optionaly add a child node that can be used to display a Drawable2D.
    void AddObstacle(const Vector2& pos, Vector<Vector2> points, Node* node = 0);
    /// Add an obstacle to the navigation mesh. Optionaly add a child node that can be used to display a Drawable2D.
    void AddObstacle(const Vector2& pos, TileMapObject2D* obj, Node* node = 0);
    /// Fully rebuild navigation mesh.
    bool RebuildNavMesh();
    ///
    Vector<SharedPtr<TileMapObject2D> > GetTileCollisionShapes(int gid) const;
    /// Detach constraints (parent them to scene) and optionaly remove tile map (if you don't intend to use it).
    void DetachConstraints(bool removeTileMap = false);

private:
    /// Tmx file.
    SharedPtr<TmxFile2D> tmxFile_;
    /// Tile map information.
    TileMapInfo2D info_;
    /// Root node for tile map layer.
    SharedPtr<Node> rootNode_;
    /// Tile map layers.
    Vector<WeakPtr<TileMapLayer2D> > layers_;
    ///
    Quaternion mapRotation_;
};

}
