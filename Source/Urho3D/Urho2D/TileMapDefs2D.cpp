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

#include "../Resource/XMLElement.h"
#include "../Resource/JSONFile.h"
#include "../Urho2D/TileMapDefs2D.h"

#include "../IO/Log.h" /// A SUPPRIMER ======================================

#include "../DebugNew.h"

namespace Urho3D
{
extern URHO3D_API const float PIXEL_SIZE;

float TileMapInfo2D::GetMapWidth() const
{
    float mapWidth = width_ * tileWidth_;

    if (orientation_ == O_STAGGERED)
    {
        if (staggerX_)
            mapWidth = (mapWidth + tileWidth_) * 0.5f;
        else
            mapWidth += tileWidth_ * 0.5f;
    }
    else if (orientation_ == O_HEXAGONAL)
    {
        float sideLength = staggerX_ ? hexSideLength_ : 0.0f;
        float sideOffset = (tileWidth_ - sideLength) * 0.5f;
        float columnWidth = sideOffset + sideLength;

        if (staggerX_)
            mapWidth = width_ * columnWidth + sideOffset;
        else
            mapWidth = width_ * (tileWidth_ + sideLength) + columnWidth;
    }
    return mapWidth;
}

float TileMapInfo2D::GetMapHeight() const
{
    float mapHeight = height_ * tileHeight_;

    if (orientation_ == O_STAGGERED)
    {
        if (staggerX_)
            mapHeight += tileHeight_ * 0.5f;
        else
            mapHeight = (mapHeight + tileHeight_) * 0.5f;
    }
    else if (orientation_ == O_HEXAGONAL)
    {
        float sideLength = staggerX_ ? 0.0f : hexSideLength_;
        float sideOffset = (tileHeight_ - sideLength) * 0.5f;
        float rowHeight = sideOffset + sideLength;

        if (staggerX_)
            mapHeight = height_ * (tileHeight_ + sideLength) + rowHeight;
        else
            mapHeight = height_ * rowHeight + sideOffset;
    }

    return mapHeight;
}

Vector2 TileMapInfo2D::ConvertPosition(const Vector2& position, bool isTile) const
{
    switch (orientation_)
    {
    case O_ISOMETRIC:
        {
            // Do not convert tile collision shape(s) as diamonds, keep them straight
            if (isTile)
                return Vector2(position.x_ * PIXEL_SIZE, GetMapHeight() - position.y_ * PIXEL_SIZE);

            Vector2 index = position * PIXEL_SIZE / tileHeight_;
            return Vector2((width_ + index.x_ - index.y_) * tileWidth_ * 0.5f, (height_ * 2.0f - index.x_ - index.y_) * tileHeight_ * 0.5f);
        }

    case O_STAGGERED:
    case O_HEXAGONAL:
    case O_ORTHOGONAL:
    default:
        return Vector2(position.x_ * PIXEL_SIZE, GetMapHeight() - position.y_ * PIXEL_SIZE);
    }

    return Vector2::ZERO;
}

Vector2 TileMapInfo2D::TileIndexToPosition(int x, int y) const
{
    switch (orientation_)
    {
    case O_ISOMETRIC:
        return Vector2((width_ + x - y - 1) * tileWidth_ * 0.5f, (height_ * 2 - x - y - 2) * tileHeight_ * 0.5f);

    case O_STAGGERED:
    case O_HEXAGONAL:
        if (staggerX_)
        {
            if (x % 2 == 0)
                return Vector2(x * (tileWidth_ + hexSideLength_) * 0.5f, (float)(height_ - 1 - y) * tileHeight_ + (staggerEven_ ? 0.0f :  tileHeight_ * 0.5f));
            else
                return Vector2(x * (tileWidth_ + hexSideLength_) * 0.5f, (float)(height_ - 1 - y) * tileHeight_ + (staggerEven_ ? tileHeight_ * 0.5f : 0.0f));
        }
        else
        {
            if (y % 2 == 0)
                return Vector2(x * tileWidth_ + (staggerEven_ ? tileWidth_ * 0.5f : 0.0f), (float)(height_ - 1 - y) * (tileHeight_ + hexSideLength_) * 0.5f);
            else
                return Vector2(x * tileWidth_ + (staggerEven_ ? 0.0f : tileWidth_ * 0.5f), (float)(height_ - 1 - y) * (tileHeight_ + hexSideLength_) * 0.5f);
        }

    case O_ORTHOGONAL:
    default:
        return Vector2(x * tileWidth_, (height_ - 1 - y) * tileHeight_);
    }

    return Vector2::ZERO;
}

bool TileMapInfo2D::PositionToTileIndex(int& x, int& y, const Vector2& position) const
{
    switch (orientation_)
    {
    case O_ISOMETRIC:
    {
        float ox = position.x_ / tileWidth_ - height_ * 0.5f;
        float oy = position.y_ / tileHeight_;

        x = (int)(width_ - oy + ox);
        y = (int)(height_ - oy - ox);
    }
        break;

    case O_STAGGERED:
    {
        float sideOffsetX = tileWidth_ * 0.5f;
        float sideOffsetY = tileHeight_ * 0.5f;

        float posX = position.x_;
        float posY = GetMapHeight() - position.y_;

        if (staggerX_)
            posX -= staggerEven_ ? sideOffsetX : 0.0f;
        else
            posY -= staggerEven_ ? sideOffsetY : 0.0f;

        IntVector2 referencePoint = IntVector2((int)floorf(posX / tileWidth_), (int)floorf(posY / tileHeight_));
        Vector2 rel = Vector2(posX - sideOffsetX - (float)(referencePoint.x_ * tileWidth_), posY - sideOffsetY - (float)(referencePoint.y_ * tileHeight_)); // Position in range x[0.0f; tile width] and y[0.0f, tile height] ~ Base = bottom-left

        int staggerAxisIndex = staggerX_ ? referencePoint.x_ : referencePoint.y_;
        staggerAxisIndex *= 2;
        if (staggerEven_)
            ++staggerAxisIndex;
        if (staggerX_) referencePoint.x_ = staggerAxisIndex;
        else referencePoint.y_ = staggerAxisIndex;

        float y_pos = rel.x_ * ((float)tileHeight_ / (float)tileWidth_); // Y position on the diamond

        // Check whether the cursor is in any of the corners (neighboring tiles)
        int rX = referencePoint.x_;
        int rY = referencePoint.y_;
        if (-y_pos - sideOffsetY > rel.y_) // TopLeft
            referencePoint = staggerX_ ? IntVector2(rX - 1, (rX & 1) ^ (int)staggerEven_ ? rY : rY - 1) : IntVector2((rY & 1) ^ (int)staggerEven_ ? rX : rX - 1, rY - 1);
        if (y_pos - sideOffsetY > rel.y_) // TopRight
            referencePoint = staggerX_ ? IntVector2(rX + 1, (rX & 1) ^ (int)staggerEven_ ? rY : rY -1) : IntVector2((rY & 1) ^ (int)staggerEven_ ? rX + 1 : rX, rY - 1);
        if (y_pos + sideOffsetY < rel.y_) // BottomLeft
            referencePoint = IntVector2(rX - 1, staggerX_ ? ((rX & 1) ^ (int)staggerEven_ ? rY + 1 : rY) : ((rY & 1) ^ (int)staggerEven_ ? rX : rX - 1, rY + 1));
        if (-y_pos + sideOffsetY < rel.y_) // BottomRight
            referencePoint = staggerX_ ? IntVector2(rX + 1, (rX & 1) ^ (int)staggerEven_ ? rY + 1 : rY) : IntVector2((rY & 1) ^ (int)staggerEven_ ? rX + 1 : rX, rY + 1);

        // Return tile index
        x = referencePoint.x_;
        y = referencePoint.y_;
    }
        break;

    case O_HEXAGONAL:
    {
        float sideLengthX = 0;
        float sideLengthY = 0;

        if (staggerX_)
            sideLengthX = hexSideLength_;
        else
            sideLengthY = hexSideLength_;

        float sideOffsetX = (tileWidth_ - sideLengthX) * 0.5f;
        float sideOffsetY = (tileHeight_ - sideLengthY) * 0.5f;

        float columnWidth = sideOffsetX + sideLengthX;
        float rowHeight = sideOffsetY + sideLengthY;

        float posX = position.x_;
        float posY = GetMapHeight() - position.y_;

        if (staggerX_)
            posX -= staggerEven_ ? tileWidth_ : sideOffsetX;
        else
            posY -= staggerEven_ ? tileHeight_ : sideOffsetY;

        IntVector2 referencePoint = IntVector2((int)floorf(posX / (columnWidth * 2)), (int)floorf(posY / (rowHeight * 2)));
        Vector2 rel = Vector2(posX - (float)(referencePoint.x_ * columnWidth * 2), posY - (float)(referencePoint.y_ * rowHeight * 2));

        int staggerAxisIndex = staggerX_ ? referencePoint.x_ : referencePoint.y_;
        staggerAxisIndex *= 2;
        if (staggerEven_)
            ++staggerAxisIndex;
        if (staggerX_) referencePoint.x_ = staggerAxisIndex;
        else referencePoint.y_ = staggerAxisIndex;

        // Determine the nearest hexagon tile by the distance to the center
        Vector<Vector2> centers;

        if (staggerX_)
        {
            const float left = sideLengthX * 0.5f;
            const float centerX = left + columnWidth;
            const float centerY = tileHeight_ * 0.5f;

            centers.Push(Vector2(left, centerY));
            centers.Push(Vector2(centerX, centerY - rowHeight));
            centers.Push(Vector2(centerX, centerY + rowHeight));
            centers.Push(Vector2(centerX + columnWidth, centerY));
        }
        else
        {
            const float top = sideLengthY * 0.5f;
            const float centerX = tileWidth_ * 0.5f;
            const float centerY = top + rowHeight;

            centers.Push(Vector2(centerX, top));
            centers.Push(Vector2(centerX - columnWidth, centerY));
            centers.Push(Vector2(centerX + columnWidth, centerY));
            centers.Push(Vector2(centerX, centerY + rowHeight));
        }

        int nearest = 0;
        float minDist = M_INFINITY;

        for (int i = 0; i < 4; ++i)
        {
            const Vector2& center = centers[i];
            const float dc = (center - rel).LengthSquared();
            if (dc < minDist)
            {
                minDist = dc;
                nearest = i;
            }
        }

        Vector<IntVector2> offsets;
        if (staggerX_)
        {
            offsets.Push(IntVector2(0, 0));
            offsets.Push(IntVector2(1, -1));
            offsets.Push(IntVector2(1, 0));
            offsets.Push(IntVector2(2, 0));
        }
        else
        {
            offsets.Push(IntVector2(0, 0));
            offsets.Push(IntVector2(-1, 1));
            offsets.Push(IntVector2(0, 1));
            offsets.Push(IntVector2(0, 2));
        }

        // Return tile index
        referencePoint = referencePoint + offsets[nearest];
        x = referencePoint.x_;
        y = referencePoint.y_;
    }
        break;

    case O_ORTHOGONAL:
    default:
        x = (int)(position.x_ / tileWidth_);
        y = height_ - 1 - int(position.y_ / tileHeight_);
        break;
    }

    return x >= 0 && x < width_ && y >= 0 && y < height_;
}

PropertySet2D::PropertySet2D()
{
}

PropertySet2D::~PropertySet2D()
{
}

void PropertySet2D::Load(const XMLElement& element)
{
    assert(element.GetName() == "properties");
    for (XMLElement propertyElem = element.GetChild("property"); propertyElem; propertyElem = propertyElem.GetNext("property"))
        nameToValueMapping_[propertyElem.GetAttribute("name")] = propertyElem.GetAttribute("value");
}

bool PropertySet2D::HasProperty(const String& name) const
{
    return nameToValueMapping_.Find(name) != nameToValueMapping_.End();
}

const String& PropertySet2D::GetProperty(const String& name) const
{
    HashMap<String, String>::ConstIterator i = nameToValueMapping_.Find(name);
    if (i == nameToValueMapping_.End())
        return String::EMPTY;

    return i->second_;
}

Tile2D::Tile2D() :
    gid_(0)
{
}

unsigned Tile2D::GetNumProperties() const
{
    if (!propertySet_)
        return 0;
    return propertySet_->GetNumProperties();
}

bool Tile2D::HasProperty(const String& name) const
{
    if (!propertySet_)
        return false;
    return propertySet_->HasProperty(name);
}

const String& Tile2D::GetProperty(const String& name) const
{
    if (!propertySet_)
        return String::EMPTY;

    return propertySet_->GetProperty(name);
}

TileMapObject2D::TileMapObject2D()
{
}

const Vector2& TileMapObject2D::GetPoint(unsigned index) const
{
    if (index >= points_.Size())
        return Vector2::ZERO;

    return points_[index];
}

unsigned TileMapObject2D::GetNumProperties() const
{
    if (!propertySet_)
        return 0;
    return propertySet_->GetNumProperties();
}

bool TileMapObject2D::HasProperty(const String& name) const
{
    if (!propertySet_)
        return false;
    return propertySet_->HasProperty(name);
}

const String& TileMapObject2D::GetProperty(const String& name) const
{
    if (!propertySet_)
        return String::EMPTY;
    return propertySet_->GetProperty(name);
}

Vector2 TileMapObject2D::RotatedPosition(const Vector2& position, float rotation)
{
    if (rotation == 0.0f || position.IsNaN())
        return position;

    Vector3 rotated = Quaternion(0.0f, 0.0f, rotation) * Vector3(position);
    return Vector2(rotated.x_, rotated.y_);
}

}
