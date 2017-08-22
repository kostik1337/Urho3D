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

#ifdef GetObject
#undef GetObject
#endif

namespace Urho3D
{

class DebugRenderer;
class Node;
class StaticSprite2D;
class TileMap2D;
class TmxImageLayer2D;
class TmxLayer2D;
class TmxObjectGroup2D;
class TmxTileLayer2D;

/// Tile map component.
class URHO3D_API TileMapLayer2D : public Component
{
    URHO3D_OBJECT(TileMapLayer2D, Component);

public:
    /// Construct.
    TileMapLayer2D(Context* context);
    /// Destruct.
    ~TileMapLayer2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Add debug geometry to the debug renderer.
    virtual void DrawDebugGeometry(DebugRenderer* debug, bool depthTest);

    /// Initialize with tile map and tmx layer.
    void Initialize(TileMap2D* tileMap, const TmxLayer2D* tmxLayer);
    /// Set draw order
    void SetDrawOrder(int drawOrder);
    /// Set visible.
    void SetVisible(bool visible);
    /// Set opacity.
    void SetOpacity(float opacity);
    /// Set offset.
    void SetOffset(const Vector2& offset) { offset_ = offset; }

    /// Convert position to tile index, if out of map return false.
    bool PositionToTileIndex(int& x, int& y, const Vector2& position) const { return tileMap_->GetInfo().PositionToTileIndex(x, y, position - offset_); }

    /// Return tile map.
    TileMap2D* GetTileMap() const;

    /// Return tmx layer.
    const TmxLayer2D* GetTmxLayer() const { return tmxLayer_; }

    /// Return draw order.
    int GetDrawOrder() const { return drawOrder_; }

    /// Return visible.
    bool IsVisible() const { return visible_; }

    /// Return opacity.
    bool GetOpacity() const { return opacity_; }

    /// Return offset.
    const Vector2& GetOffset() const { return offset_; }

    /// Return has property
    bool HasProperty(const String& name) const;
    /// Return property.
    const String& GetProperty(const String& name) const;
    /// Return layer type.
    TileMapLayerType2D GetLayerType() const;

    /// Return width (for tile layer only).
    int GetWidth() const;
    /// Return height (for tile layer only).
    int GetHeight() const;
    /// Return tile node (for tile layer only).
    Node* GetTileNode(int x, int y) const;
    /// Return tile (for tile layer only).
    Tile2D* GetTile(int x, int y) const;

    /// Return number of tile map objects (for object group only).
    unsigned GetNumObjects() const;
    /// Return tile map object by index (for object group only).
    TileMapObject2D* GetObject(unsigned index) const;
    /// Return tile map object by name (for object group only).
    TileMapObject2D* GetObject(const String& name) const;
    /// Return object node (for object group only).
    Node* GetObjectNode(unsigned index) const;

    /// Return image node (for image layer only).
    Node* GetImageNode() const;

    /// Return name.
    const String& GetName() const { return name_; }

    /// Get tile sprite order in layer according to RenderOrder2D.
    int TileRenderOrder(int x, int y);

private:
    /// Set tile layer.
    void SetTileLayer(const TmxTileLayer2D* tileLayer);
    /// Set object group.
    void SetObjectGroup(const TmxObjectGroup2D* objectGroup);
    /// Set image layer.
    void SetImageLayer(const TmxImageLayer2D* imageLayer);
    /// Flip sprite (tile and tile object).
    void FlipSprite(StaticSprite2D* sprite, Vector3 flipAxis, Vector2 hotSpot);

    /// Tile map.
    WeakPtr<TileMap2D> tileMap_;
    /// Tmx layer.
    const TmxLayer2D* tmxLayer_;
    /// Tile layer.
    const TmxTileLayer2D* tileLayer_;
    /// Object group.
    const TmxObjectGroup2D* objectGroup_;
    /// Image layer.
    const TmxImageLayer2D* imageLayer_;
    /// Draw order.
    int drawOrder_;
    /// Visible.
    bool visible_;
    /// Opacity.
    float opacity_;
    /// Offset.
    Vector2 offset_;
    /// Objects nodes.
    Vector<SharedPtr<Node> > nodes_;
    /// Layer name.
    String name_;
};

}
