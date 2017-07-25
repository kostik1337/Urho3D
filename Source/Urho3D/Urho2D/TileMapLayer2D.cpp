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
#include "../Resource/ResourceCache.h"
#include "../Scene/Node.h"
#include "../Urho2D/StaticSprite2D.h"
#include "../Urho2D/TileMap2D.h"
#include "../Urho2D/TileMapLayer2D.h"
#include "../Urho2D/TmxFile2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

TileMapLayer2D::TileMapLayer2D(Context* context) :
    Component(context),
    tmxLayer_(0),
    drawOrder_(0),
    visible_(true)
{
}

TileMapLayer2D::~TileMapLayer2D()
{
}

void TileMapLayer2D::RegisterObject(Context* context)
{
    context->RegisterFactory<TileMapLayer2D>();
}

// Transform vector from tiled isometric space to Urho3d node-local space
static Vector2 TransformIsometricVector(Vector2 vec)
{
    return Vector2(vec.x_ - vec.y_, - (vec.x_ + vec.y_) / 2);
}

// Transform vector from node-local space to global space
Vector2 TransformNode2D(Matrix3x4 transform, Vector2 local)
{
    Vector3 transformed = transform * Vector4(local.x_, local.y_, 0.f, 1.f);
    return Vector2(transformed.x_, transformed.y_);
}

void TileMapLayer2D::DrawDebugGeometry(DebugRenderer* debug, bool depthTest)
{
    if (!debug)
        return;

    if (objectGroup_)
    {
        const Matrix3x4 transform = GetTileMap()->GetNode()->GetTransform();
        for (unsigned i = 0; i < objectGroup_->GetNumObjects(); ++i)
        {
            TileMapObject2D* object = objectGroup_->GetObject(i);
            const Color& color = Color::YELLOW;

            switch (object->GetObjectType())
            {
            case OT_RECTANGLE:
                {
<<<<<<< HEAD
                    switch (GetTileMap()->GetInfo().orientation_)
                    {
                    case O_ORTHOGONAL:
                    case O_HEXAGONAL:
                    case O_STAGGERED:
                        {
                            const Vector2& lb = object->GetPosition();
                            const Vector2& rt = lb + object->GetSize();
                            debug->AddLine(TransformNode2D(transform, Vector2(lb.x_, lb.y_)), TransformNode2D(transform, Vector2(rt.x_, lb.y_)), color, depthTest);
                            debug->AddLine(TransformNode2D(transform, Vector2(rt.x_, lb.y_)), TransformNode2D(transform, Vector2(rt.x_, rt.y_)), color, depthTest);
                            debug->AddLine(TransformNode2D(transform, Vector2(rt.x_, rt.y_)), TransformNode2D(transform, Vector2(lb.x_, rt.y_)), color, depthTest);
                            debug->AddLine(TransformNode2D(transform, Vector2(lb.x_, rt.y_)), TransformNode2D(transform, Vector2(lb.x_, lb.y_)), color, depthTest);
                            break;
                        }
                    case O_ISOMETRIC:
                        {
                            const Vector2& size = object->GetSize();
                            const Vector2 lt = object->GetPosition();
                            const Vector2 lb = lt - TransformIsometricVector(Vector2(0, size.y_));
                            const Vector2 rb = lb + TransformIsometricVector(Vector2(size.x_, 0));
                            const Vector2 rt = lb + TransformIsometricVector(Vector2(size.x_, size.y_));
                            debug->AddLine(TransformNode2D(transform, lt), TransformNode2D(transform, lb), color, depthTest);
                            debug->AddLine(TransformNode2D(transform, lb), TransformNode2D(transform, rb), color, depthTest);
                            debug->AddLine(TransformNode2D(transform, rb), TransformNode2D(transform, rt), color, depthTest);
                            debug->AddLine(TransformNode2D(transform, rt), TransformNode2D(transform, lt), color, depthTest);
                            break;
                        }
=======
                    const TileMapInfo2D& info = tileMap_->GetInfo();
                    const Vector2& size = object->GetSize();
                    float rotation = object->GetRotation();

                    if (rotation == 0.0f)
                    {
                        const Vector2& lb = object->GetPosition();
                        const Vector2& rt = lb + Vector2(size.x_, -size.y_); // Top-left pivot

                        debug->AddLine(Vector2(lb.x_, lb.y_), Vector2(rt.x_, lb.y_), color, depthTest);
                        debug->AddLine(Vector2(rt.x_, lb.y_), Vector2(rt.x_, rt.y_), color, depthTest);
                        debug->AddLine(Vector2(rt.x_, rt.y_), Vector2(lb.x_, rt.y_), color, depthTest);
                        debug->AddLine(Vector2(lb.x_, rt.y_), Vector2(lb.x_, lb.y_), color, depthTest);
                    }

                    else // Convert rectangle to points to allow rotation
                    {
                        Vector<Vector2> points;
                        points.Push(Vector2::ZERO);
                        points.Push(Vector2(size.x_, 0.0f));
                        points.Push(Vector2(size.x_, -size.y_));
                        points.Push(Vector2(0.0f, -size.y_));
                        points.Push(Vector2::ZERO);

                        for (unsigned i = 0; i < points.Size(); ++i)
                            points[i] = object->GetPosition() + object->RotatedPosition(points[i], rotation);

                        for (unsigned j = 0; j < points.Size() - 1; ++j)
                            debug->AddLine(points[j], points[j + 1], color, depthTest);
>>>>>>> new-tmx-features
                    }
                }
                break;

            case OT_ELLIPSE:
                {
<<<<<<< HEAD
                    switch (GetTileMap()->GetInfo().orientation_)
                    {
                    case O_ORTHOGONAL:
                    case O_HEXAGONAL:
                    case O_STAGGERED:
                        {
                            const Vector2 halfSize = object->GetSize() * 0.5f;
                            const Vector2 center = object->GetPosition() + halfSize;
                            for (unsigned i = 0; i < 360; i += 30)
                            {
                                unsigned j = i + 30;
                                float x1 = halfSize.x_ * Cos((float)i);
                                float y1 = halfSize.y_ * Sin((float)i);
                                float x2 = halfSize.x_ * Cos((float)j);
                                float y2 = halfSize.y_ * Sin((float)j);
                                debug->AddLine(TransformNode2D(transform, center + Vector2(x1, y1)),
                                               TransformNode2D(transform, center + Vector2(x2, y2)), color, depthTest);
                            }
                            break;
                        }
                    case O_ISOMETRIC:
                        {
                            const Vector2 halfSize = object->GetSize() * 0.5f;
                            const Vector2 center = object->GetPosition() + TransformIsometricVector(Vector2(halfSize.x_, -halfSize.y_));
                            for (unsigned i = 0; i < 360; i += 30)
                            {
                                unsigned j = i + 30;
                                float x1 = halfSize.x_ * Cos((float)i);
                                float y1 = halfSize.y_ * Sin((float)i);
                                float x2 = halfSize.x_ * Cos((float)j);
                                float y2 = halfSize.y_ * Sin((float)j);
                                debug->AddLine(TransformNode2D(transform, center + TransformIsometricVector(Vector2(x1, y1))),
                                               TransformNode2D(transform, center + TransformIsometricVector(Vector2(x2, y2))), color, depthTest);
                            }
                            break;
                        }
=======
                    const TileMapInfo2D& info = tileMap_->GetInfo();
                    const Vector2 halfSize = object->GetSize() * 0.5f;
                    float rotation = object->GetRotation();
                    float ratio = (info.tileWidth_ / info.tileHeight_) * 0.5f; // For isometric only

                    Vector2 pivot = object->GetPosition();

                    for (unsigned i = 0; i < 360; i += 30)
                    {
                        unsigned j = i + 30;
                        float x1 = halfSize.x_ * Cos((float)i);
                        float y1 = halfSize.y_ * Sin((float)i);
                        float x2 = halfSize.x_ * Cos((float)j);
                        float y2 = halfSize.y_ * Sin((float)j);
                        Vector2 point1 = Vector2(x1, - y1) + Vector2(halfSize.x_, -halfSize.y_);
                        Vector2 point2 = Vector2(x2, - y2) + Vector2(halfSize.x_, -halfSize.y_);

                        if (info.orientation_ == O_ISOMETRIC)
                        {
                            point1 = Vector2((point1.x_ + point1.y_) * ratio, (point1.y_ - point1.x_) * 0.5f);
                            point2 = Vector2((point2.x_ + point2.y_) * ratio, (point2.y_ - point2.x_) * 0.5f);
                        }

                        debug->AddLine(pivot + point1, pivot + point2, color, depthTest);
>>>>>>> new-tmx-features
                    }
                }
                break;

            case OT_POLYGON:
            case OT_POLYLINE:
                {
                    for (unsigned j = 0; j < object->GetNumPoints() - 1; ++j)
                        debug->AddLine(TransformNode2D(transform, object->GetPoint(j)),
                                       TransformNode2D(transform, object->GetPoint(j + 1)), color, depthTest);

                    if (object->GetObjectType() == OT_POLYGON)
<<<<<<< HEAD
                        debug->AddLine(TransformNode2D(transform, object->GetPoint(0)),
                                       TransformNode2D(transform, object->GetPoint(object->GetNumPoints() - 1)), color, depthTest);
=======
                        debug->AddLine(object->GetPoint(0), object->GetPoint(object->GetNumPoints() - 1), color, depthTest);
                    // Also draw a circle at origin to indicate direction
                    else
                        debug->AddCircle(object->GetPoint(0), Vector3::FORWARD, 0.05f, color, 64, depthTest); // Also draw a circle at origin to indicate direction
>>>>>>> new-tmx-features
                }
                break;

            default: break;
            }
        }
    }
}

void TileMapLayer2D::Initialize(TileMap2D* tileMap, const TmxLayer2D* tmxLayer)
{
    if (tileMap == tileMap_ && tmxLayer == tmxLayer_)
        return;

    if (tmxLayer_)
    {
        for (unsigned i = 0; i < nodes_.Size(); ++i)
        {
            if (nodes_[i])
                nodes_[i]->Remove();
        }

        nodes_.Clear();
    }

    tileLayer_ = 0;
    objectGroup_ = 0;
    imageLayer_ = 0;

    tileMap_ = tileMap;
    tmxLayer_ = tmxLayer;

    if (!tmxLayer_)
        return;

    name_ = tmxLayer_->GetName();

    switch (tmxLayer_->GetType())
    {
    case LT_TILE_LAYER:
        SetTileLayer((const TmxTileLayer2D*)tmxLayer_);
        break;

    case LT_OBJECT_GROUP:
        SetObjectGroup((const TmxObjectGroup2D*)tmxLayer_);
        break;

    case LT_IMAGE_LAYER:
        SetImageLayer((const TmxImageLayer2D*)tmxLayer_);
        break;

    default:
        break;
    }

    SetVisible(tmxLayer_->IsVisible());
}

void TileMapLayer2D::SetDrawOrder(int drawOrder)
{
    if (drawOrder == drawOrder_)
        return;

    drawOrder_ = drawOrder;

    for (unsigned i = 0; i < nodes_.Size(); ++i)
    {
        if (!nodes_[i])
            continue;

        StaticSprite2D* staticSprite = nodes_[i]->GetComponent<StaticSprite2D>();
        if (staticSprite)
            staticSprite->SetLayer(drawOrder_);
    }
}

void TileMapLayer2D::SetVisible(bool visible)
{
    if (visible == visible_)
        return;

    visible_ = visible;

    for (unsigned i = 0; i < nodes_.Size(); ++i)
    {
        if (nodes_[i])
            nodes_[i]->SetEnabled(visible_);
    }
}

TileMap2D* TileMapLayer2D::GetTileMap() const
{
    return tileMap_;
}

bool TileMapLayer2D::HasProperty(const String& name) const
{
    if (!tmxLayer_)
        return false;

    return tmxLayer_->HasProperty(name);
}

const String& TileMapLayer2D::GetProperty(const String& name) const
{
    if (!tmxLayer_)
        return String::EMPTY;
    return tmxLayer_->GetProperty(name);
}

TileMapLayerType2D TileMapLayer2D::GetLayerType() const
{
    return tmxLayer_ ? tmxLayer_->GetType() : LT_INVALID;
}

int TileMapLayer2D::GetWidth() const
{
    return tmxLayer_ ? tmxLayer_->GetWidth() : 0;
}

int TileMapLayer2D::GetHeight() const
{
    return tmxLayer_ ? tmxLayer_->GetHeight() : 0;
}

Tile2D* TileMapLayer2D::GetTile(int x, int y) const
{
    if (!tileLayer_)
        return 0;

    return tileLayer_->GetTile(x, y);
}

Node* TileMapLayer2D::GetTileNode(int x, int y) const
{
    if (!tileLayer_)
        return 0;

    if (x < 0 || x >= tileLayer_->GetWidth() || y < 0 || y >= tileLayer_->GetHeight())
        return 0;

    return nodes_[y * tileLayer_->GetWidth() + x];
}

unsigned TileMapLayer2D::GetNumObjects() const
{
    if (!objectGroup_)
        return 0;

    return objectGroup_->GetNumObjects();
}

TileMapObject2D* TileMapLayer2D::GetObject(unsigned index) const
{
    if (!objectGroup_)
        return 0;

    return objectGroup_->GetObject(index);
}

Node* TileMapLayer2D::GetObjectNode(unsigned index) const
{
    if (!objectGroup_)
        return 0;

    if (index >= nodes_.Size())
        return 0;

    return nodes_[index];
}

Node* TileMapLayer2D::GetImageNode() const
{
    if (!imageLayer_ || nodes_.Empty())
        return 0;

    return nodes_[0];
}

void TileMapLayer2D::SetTileLayer(const TmxTileLayer2D* tileLayer)
{
    tileLayer_ = tileLayer;

    int width = tileLayer->GetWidth();
    int height = tileLayer->GetHeight();
    nodes_.Resize((unsigned)(width * height));

    const TileMapInfo2D& info = tileMap_->GetInfo();
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const Tile2D* tile = tileLayer->GetTile(x, y);
            if (!tile)
                continue;

            SharedPtr<Node> tileNode(GetNode()->CreateTemporaryChild("Tile"));
            tileNode->SetPosition(info.TileIndexToPosition(x, y) + tileLayer->GetOffset());

            // Create collision shape component
            Vector<SharedPtr<TileMapObject2D> > tileShapes = tile->GetCollisionShapes();
            for (Vector<SharedPtr<TileMapObject2D> >::ConstIterator i = tileShapes.Begin(); i != tileShapes.End(); ++i)
                tileMap_->CreatePhysicsFromObject(*i, tileNode->GetPosition2D());

            Vector3 flipAxis = tile->GetFlipAxis();

            // Create static or animated sprite
            if (tile->GetAnim().Empty())
            {
                StaticSprite2D* staticSprite = tileNode->CreateComponent<StaticSprite2D>();
                Sprite2D* sprite = tile->GetSprite();
                staticSprite->SetSprite(sprite);
                staticSprite->SetLayer(drawOrder_);
                staticSprite->SetOrderInLayer(TileRenderOrder(x, y));

                // Flip sprite
                if (flipAxis != Vector3::ZERO)
                {
                    IntVector2 spriteSize = sprite->GetRectangle().Size();
                    tileNode->SetPosition(tileNode->GetPosition() + Vector3(spriteSize.x_, spriteSize.y_) * PIXEL_SIZE * 0.5f);
                    FlipSprite(staticSprite, flipAxis, Vector2(0.5f, 0.5f));
                }
            }
            else // Animated tile
            {
                AnimatedSprite2D* animatedSprite = tileNode->CreateComponent<AnimatedSprite2D>();
                animatedSprite->SetLayer(drawOrder_);
                animatedSprite->SetOrderInLayer(TileRenderOrder(x, y));

                AnimationSet2D* anim = GetSubsystem<ResourceCache>()->GetExistingResource<AnimationSet2D>(tile->GetAnim());
                animatedSprite->SetAnimationSet(anim);
                animatedSprite->SetAnimation(anim->GetAnimation(0));

                // Offset position
                IntVector2 spriteSize = tile->GetSprite()->GetRectangle().Size();
                tileNode->SetPosition(tileNode->GetPosition() + Vector3(spriteSize.x_, spriteSize.y_) * PIXEL_SIZE * 0.5f);

                // Flip sprite
                if (flipAxis != Vector3::ZERO)
                    animatedSprite->SetFlip((int)flipAxis.x_, (int)flipAxis.y_);
            }

            nodes_[y * width + x] = tileNode;
        }
    }
}

void TileMapLayer2D::SetObjectGroup(const TmxObjectGroup2D* objectGroup)
{
    objectGroup_ = objectGroup;

    TmxFile2D* tmxFile = objectGroup->GetTmxFile();
    nodes_.Resize(objectGroup->GetNumObjects());

    for (unsigned i = 0; i < objectGroup->GetNumObjects(); ++i)
    {
        const TileMapObject2D* object = objectGroup->GetObject(i);
        TileMapObjectType2D type = object->GetObjectType();

        // Create dummy node for all objects
        SharedPtr<Node> objectNode(GetNode()->CreateTemporaryChild(object->GetName()));
        objectNode->SetPosition(object->GetPosition()); // Offset is already applied to objects' position
        objectNode->SetScale2D(object->GetSize());

        // If object is tile, create static sprite component
        if (type == OT_TILE && object->GetTileGid() && object->GetTileSprite())
        {
            Vector3 flipAxis = object->GetFlipAxis();

            if (object->GetTileAnim().Empty())
            {
                StaticSprite2D* staticSprite = objectNode->CreateComponent<StaticSprite2D>();
                staticSprite->SetSprite(object->GetTileSprite());
                staticSprite->SetLayer(drawOrder_);
                staticSprite->SetOrderInLayer(objectGroup->DrawTopDown() ? (int)((10.0f - object->GetPosition().y_) * 100) : i);

                // Flip sprite
                if (flipAxis != Vector3::ZERO)
                    FlipSprite(staticSprite, flipAxis, Vector2(flipAxis.x_, flipAxis.y_));
            }
            else // Animated tile
            {
                AnimatedSprite2D* animatedSprite = objectNode->CreateComponent<AnimatedSprite2D>();
                animatedSprite->SetLayer(drawOrder_);
                animatedSprite->SetOrderInLayer(objectGroup->DrawTopDown() ? (int)((10.0f - object->GetPosition().y_) * 100) : i);

                AnimationSet2D* anim = GetSubsystem<ResourceCache>()->GetResource<AnimationSet2D>(object->GetTileAnim());
                animatedSprite->SetAnimationSet(anim);
                animatedSprite->SetAnimation(anim->GetAnimation(0));

                // Flip sprite
                if (flipAxis != Vector3::ZERO)
                {
                    animatedSprite->SetUseHotSpot(true);
                    animatedSprite->SetHotSpot(Vector2(flipAxis.x_, flipAxis.y_));
                    animatedSprite->SetFlip((int)flipAxis.x_, (int)flipAxis.y_);
                }
            }
        }

        // Apply custom rotation to tiles (for polygons and poly lines, rotation has already been applied to points
        // in TmxObjectGroup2D::Load() and applying rotation here to rectangles and ellipses is of no use)
        if (object->GetRotation() != 0 && type == OT_TILE)
            objectNode->Roll(object->GetRotation());

        nodes_[i] = objectNode;
    }
}

void TileMapLayer2D::SetImageLayer(const TmxImageLayer2D* imageLayer)
{
    imageLayer_ = imageLayer;

    if (!imageLayer->GetSprite())
        return;

    SharedPtr<Node> imageNode(GetNode()->CreateTemporaryChild("Tile"));
    imageNode->SetPosition(imageLayer->GetPosition());

    StaticSprite2D* staticSprite = imageNode->CreateComponent<StaticSprite2D>();
    staticSprite->SetSprite(imageLayer->GetSprite());
    staticSprite->SetOrderInLayer(0);

    nodes_.Push(imageNode);
}

}
