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

#include "../Urho2D/AnimatedSprite2D.h"
#include "../Urho2D/AnimationSet2D.h"
#include "../Core/Context.h"
#include "../Engine/Engine.h"
#include "../IO/FileSystem.h"
#include "../IO/Log.h"
#include "../Urho3D/Graphics/Renderer.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/SpriterData2D.h"
#include "../Graphics/Texture2D.h"
#include "../Urho2D/TmxFile2D.h"
#include "../Resource/XMLFile.h"
#include "../Graphics/Zone.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const float PIXEL_SIZE;

TmxLayer2D::TmxLayer2D(TmxFile2D* tmxFile, TileMapLayerType2D type) :
    tmxFile_(tmxFile),
    type_(type)
{

}

TmxLayer2D::~TmxLayer2D()
{
}

bool TmxLayer2D::HasProperty(const String& name) const
{
    if (!propertySet_)
        return false;
    return propertySet_->HasProperty(name);
}

const String& TmxLayer2D::GetProperty(const String& name) const
{
    if (!propertySet_)
        return String::EMPTY;
    return propertySet_->GetProperty(name);
}

void TmxLayer2D::LoadInfo(const XMLElement& element)
{
    name_ = element.GetAttribute("name");
    width_ = element.GetInt("width");
    height_ = element.GetInt("height");
    visible_ = element.HasAttribute("visible") ? element.GetInt("visible") != 0 : true;
    opacity_ = element.HasAttribute("opacity") ? element.GetFloat("opacity") : 1.0f;
    offset_ = Vector2(element.HasAttribute("offsetx") ? element.GetFloat("offsetx") * PIXEL_SIZE : 0.0f, element.HasAttribute("offsety") ? -element.GetFloat("offsety") * PIXEL_SIZE : 0.0f);
}

void TmxLayer2D::LoadPropertySet(const XMLElement& element)
{
    propertySet_ = new PropertySet2D();
    propertySet_->Load(element);
}

TmxTileLayer2D::TmxTileLayer2D(TmxFile2D* tmxFile) :
    TmxLayer2D(tmxFile, LT_TILE_LAYER)
{
}

enum LayerEncoding {
    XML,
    CSV,
    Base64,
};

void TmxTileLayer2D::SaveTileForGid(int x, int y, unsigned gid) {
    Vector3 flipAxis;
    tmxFile_->GetActualGid(gid, flipAxis);

    if (gid > 0)
    {
        SharedPtr<Tile2D> tile(new Tile2D());
        tile->gid_ = (int)gid;
        tile->sprite_ = tmxFile_->GetTileSprite(gid);
        tile->anim_ = tmxFile_->GetTileAnim(gid);
        tile->collisionShapes_ = tmxFile_->GetTileCollisionShapes(gid);
        tile->flipAxis_ = flipAxis;
        tile->propertySet_ = tmxFile_->GetTilePropertySet(gid);
        tiles_[y * width_ + x] = tile;
    }
}

bool TmxTileLayer2D::Load(const XMLElement& element, const TileMapInfo2D& info)
{
    LoadInfo(element);

    XMLElement dataElem = element.GetChild("data");
    if (!dataElem)
    {
        URHO3D_LOGERROR("Could not find data in layer");
        return false;
    }

    LayerEncoding encoding;
    if (dataElem.HasAttribute("compression"))
    {
        URHO3D_LOGERROR("Compression not supported now");
        return false;
    }

    if (dataElem.HasAttribute("encoding"))
    {
        String encodingAttribute = dataElem.GetAttribute("encoding");
        if (encodingAttribute == "xml")
            encoding = XML;
        else if (encodingAttribute == "csv")
            encoding = CSV;
        else if (encodingAttribute == "base64")
            encoding = Base64;
        else
        {
            URHO3D_LOGERROR("Invalid encoding: " + encodingAttribute);
            return false;
        }
    }
    else
        encoding = XML;

    tiles_.Resize((unsigned)(width_ * height_));
    if (encoding == XML)
    {
        XMLElement tileElem = dataElem.GetChild("tile");

        for (int y = 0; y < height_; ++y)
        {
            for (int x = 0; x < width_; ++x)
            {
                if (!tileElem)
                    return false;

                unsigned gid = tileElem.GetInt("gid");
                SaveTileForGid(x, y, gid);

                tileElem = tileElem.GetNext("tile");
            }
        }
    }
    else if (encoding == CSV)
    {
        String dataValue = dataElem.GetValue();
        Vector<String> gidVector = dataValue.Split(',');
        int currentIndex = 0;
        for (int y = 0; y < height_; ++y)
        {
            for (int x = 0; x < width_; ++x)
            {
                gidVector[currentIndex].Replace("\n", "");
                unsigned gid = ToUInt(gidVector[currentIndex]);
                SaveTileForGid(x, y, gid);
                ++currentIndex;
            }
        }
    }
    else if (encoding == Base64)
    {
        String dataValue = dataElem.GetValue();
        int startPosition = 0;
        while (!IsAlpha(dataValue[startPosition]) && !IsDigit(dataValue[startPosition])
              && dataValue[startPosition] != '+' && dataValue[startPosition] != '/') ++startPosition;
        dataValue = dataValue.Substring(startPosition);
        PODVector<unsigned char> buffer = DecodeBase64(dataValue);
        int currentIndex = 0;
        for (int y = 0; y < height_; ++y)
        {
            for (int x = 0; x < width_; ++x)
            {
                // buffer contains 32-bit integers in little-endian format
                unsigned gid = (buffer[currentIndex+3] << 24) | (buffer[currentIndex+2] << 16)
                             | (buffer[currentIndex+1] << 8) | buffer[currentIndex];
                SaveTileForGid(x, y, gid);
                currentIndex += 4;
            }
        }
    }

    if (element.HasChild("properties"))
        LoadPropertySet(element.GetChild("properties"));

    return true;
}

Tile2D* TmxTileLayer2D::GetTile(int x, int y) const
{
    if (x < 0 || x >= width_ || y < 0 || y >= height_)
        return 0;

    return tiles_[y * width_ + x];
}

TmxObjectGroup2D::TmxObjectGroup2D(TmxFile2D* tmxFile) :
    TmxLayer2D(tmxFile, LT_OBJECT_GROUP)
{
}

bool TmxObjectGroup2D::Load(const XMLElement& element, const TileMapInfo2D& info)
{
    LoadInfo(element);

    for (XMLElement objectElem = element.GetChild("object"); objectElem; objectElem = objectElem.GetNext("object"))
    {
        SharedPtr<TileMapObject2D> object(new TileMapObject2D());
        StoreObject(objectElem, object, info);
    }

    drawTopDown_ = !element.HasAttribute("draworder");

    if (element.HasChild("properties"))
        LoadPropertySet(element.GetChild("properties"));

    return true;
}

void TmxObjectGroup2D::StoreObject(XMLElement objectElem, SharedPtr<TileMapObject2D> object, const TileMapInfo2D& info, bool isTile)
{
        if (objectElem.HasAttribute("name"))
            object->name_ = objectElem.GetAttribute("name");
        if (objectElem.HasAttribute("type"))
            object->type_ = objectElem.GetAttribute("type");

        if (objectElem.HasAttribute("gid"))
            object->objectType_ = OT_TILE;
        else if (objectElem.HasChild("polygon"))
            object->objectType_ = OT_POLYGON;
        else if (objectElem.HasChild("polyline"))
            object->objectType_ = OT_POLYLINE;
        else if (objectElem.HasChild("ellipse"))
            object->objectType_ = OT_ELLIPSE;
        else
            object->objectType_ = OT_RECTANGLE;

        Vector2 position(objectElem.GetFloat("x") + offset_.x_ / PIXEL_SIZE, objectElem.GetFloat("y") + offset_.y_ / PIXEL_SIZE);
        const Vector2 size(objectElem.GetFloat("width"), objectElem.GetFloat("height"));
        const float rotation(-objectElem.GetFloat("rotation"));
        TileMapObjectType2D type = object->objectType_;

        object->rotation_ = rotation;

        switch (type)
        {
        case OT_RECTANGLE:
        case OT_ELLIPSE:
        {
            bool isSphere = size.x_ == (size.y_ * (info.orientation_ == O_ISOMETRIC && isTile ? (info.tileHeight_ / info.tileWidth_) : 1.0f));

            if (type == OT_RECTANGLE && info.orientation_ == O_ISOMETRIC && !isTile) // Rectangle becomes a diamond, so we convert it to polyline
            {
                object->objectType_ = OT_POLYLINE;

                Vector<Vector2> points;
                points.Push(Vector2::ZERO);
                points.Push(Vector2(size.x_, 0.0f));
                points.Push(Vector2(size.x_, size.y_));
                points.Push(Vector2(0.0f, size.y_));
                points.Push(Vector2::ZERO);

                object->points_.Resize(points.Size());

                for (unsigned i = 0; i < points.Size(); ++i)
                    object->points_[i] = info.ConvertPosition(position + object->RotatedPosition(points[i], rotation), isTile);
            }
            else if (type == OT_ELLIPSE && !isSphere) // If ellipse is not a sphere, convert to poly line (8 vertices, in case we use it as a collision shape)
            {
                object->objectType_ = OT_POLYLINE;

                Vector2 halfSize = size * 0.5f;

                for (unsigned i = 0; i <= 360; i += 45)
                {
                    Vector2 point = Vector2(halfSize.x_ * Cos((float)i), halfSize.y_ * Sin((float)i)) + halfSize;
                    object->points_.Push(info.ConvertPosition(position + object->RotatedPosition(point, -rotation), isTile));
                }
            }
            else if (type == OT_ELLIPSE && isSphere) // Apply rotation to rotated sphere
                object->position_ = info.ConvertPosition(object->RotatedPosition(position, -rotation), isTile); /// Check rotation sign
            else
                object->position_ = info.ConvertPosition(position, isTile);
            object->size_ = size * PIXEL_SIZE;
        }
            break;

        case OT_TILE:
        {
            unsigned gid = objectElem.GetUInt("gid");
            Vector3 flipAxis;
            tmxFile_->GetActualGid(gid, flipAxis);

            object->position_ = info.ConvertPosition(position);
            // In isometric orientation, Tiled position is center-bottom instead of left-bottom
            if (info.orientation_ == O_ISOMETRIC)
                object->position_.x_ -= size.x_ * PIXEL_SIZE * 0.5f;

            object->gid_ = (int)gid;
            object->sprite_ = tmxFile_->GetTileSprite(gid);
            object->anim_ = tmxFile_->GetTileAnim(gid);
            object->collisionShapes_ = tmxFile_->GetTileCollisionShapes(gid);
            object->flipAxis_ = flipAxis;

            if (objectElem.HasAttribute("width") || objectElem.HasAttribute("height"))
                object->size_ = Vector2(size.x_ * PIXEL_SIZE, size.y_ * PIXEL_SIZE);
            if (object->sprite_)
            {
                IntVector2 spriteSize = object->sprite_->GetRectangle().Size();
                if (size.x_ != spriteSize.x_ || size.y_ != spriteSize.y_)
                {
                    Vector2 newSize(size.x_ / spriteSize.x_, size.y_ / spriteSize.y_);
                    object->size_ = newSize;
                }
                else
                    object->size_ = Vector2::ONE;
            }
        }
            break;

        case OT_POLYGON:
        case OT_POLYLINE:
            {
                const char* name = object->objectType_ == OT_POLYGON ? "polygon" : "polyline";
                XMLElement polygonElem = objectElem.GetChild(name);
                Vector<String> points = polygonElem.GetAttribute("points").Split(' ');

                if (points.Size() <= 1)
                    return;

                object->points_.Resize(points.Size());

                for (unsigned i = 0; i < points.Size(); ++i)
                {
                    points[i].Replace(',', ' ');
                    object->points_[i] = info.ConvertPosition(position + object->RotatedPosition(ToVector2(points[i]), rotation), isTile);
                }
            }
            break;

        default: break;
        }

        if (objectElem.HasChild("properties"))
        {
            object->propertySet_ = new PropertySet2D();
            object->propertySet_->Load(objectElem.GetChild("properties"));
        }

        objects_.Push(object);
}

TileMapObject2D* TmxObjectGroup2D::GetObject(unsigned index) const
{
    if (index >= objects_.Size())
        return 0;
    return objects_[index];
}


TmxImageLayer2D::TmxImageLayer2D(TmxFile2D* tmxFile) :
    TmxLayer2D(tmxFile, LT_IMAGE_LAYER)
{
}

bool TmxImageLayer2D::Load(const XMLElement& element, const TileMapInfo2D& info)
{
    LoadInfo(element);

    XMLElement imageElem = element.GetChild("image");
    if (!imageElem)
        return false;

    position_ = Vector2(0.0f, info.GetMapHeight());
    source_ = imageElem.GetAttribute("source");
    String textureFilePath = GetParentPath(tmxFile_->GetName()) + source_;
    ResourceCache* cache = tmxFile_->GetSubsystem<ResourceCache>();
    SharedPtr<Texture2D> texture(cache->GetResource<Texture2D>(textureFilePath));
    if (!texture)
    {
        URHO3D_LOGERROR("Could not load texture " + textureFilePath);
        return false;
    }

    sprite_ = new Sprite2D(tmxFile_->GetContext());
    sprite_->SetTexture(texture);
    sprite_->SetRectangle(IntRect(0, 0, texture->GetWidth(), texture->GetHeight()));
    // Set image hot spot at left top
    sprite_->SetHotSpot(Vector2(0.0f, 1.0f));

    if (element.HasChild("properties"))
        LoadPropertySet(element.GetChild("properties"));

    return true;
}

TmxFile2D::TmxFile2D(Context* context) :
    Resource(context)
{
}

TmxFile2D::~TmxFile2D()
{
    for (unsigned i = 0; i < layers_.Size(); ++i)
        delete layers_[i];
}

void TmxFile2D::RegisterObject(Context* context)
{
    context->RegisterFactory<TmxFile2D>();
}

bool TmxFile2D::BeginLoad(Deserializer& source)
{
    if (GetName().Empty())
        SetName(source.GetName());

    loadXMLFile_ = new XMLFile(context_);
    if (!loadXMLFile_->Load(source))
    {
        URHO3D_LOGERROR("Load XML failed " + source.GetName());
        loadXMLFile_.Reset();
        return false;
    }

    XMLElement rootElem = loadXMLFile_->GetRoot("map");
    if (!rootElem)
    {
        URHO3D_LOGERROR("Invalid tmx file " + source.GetName());
        loadXMLFile_.Reset();
        return false;
    }

    // If we're async loading, request the texture now. Finish during EndLoad().
    if (GetAsyncLoadState() == ASYNC_LOADING)
    {
        for (XMLElement tileSetElem = rootElem.GetChild("tileset"); tileSetElem; tileSetElem = tileSetElem.GetNext("tileset"))
        {
            // Tile set defined in TSX file
            if (tileSetElem.HasAttribute("source"))
            {
                String source = tileSetElem.GetAttribute("source");
                SharedPtr<XMLFile> tsxXMLFile = LoadTSXFile(source);
                if (!tsxXMLFile)
                    return false;

                tsxXMLFiles_[source] = tsxXMLFile;

                String textureFilePath =
                    GetParentPath(GetName()) + tsxXMLFile->GetRoot("tileset").GetChild("image").GetAttribute("source");
                GetSubsystem<ResourceCache>()->BackgroundLoadResource<Texture2D>(textureFilePath, true, this);
            }
            else
            {
                String textureFilePath = GetParentPath(GetName()) + tileSetElem.GetChild("image").GetAttribute("source");
                GetSubsystem<ResourceCache>()->BackgroundLoadResource<Texture2D>(textureFilePath, true, this);
            }
        }

        for (XMLElement imageLayerElem = rootElem.GetChild("imagelayer"); imageLayerElem;
             imageLayerElem = imageLayerElem.GetNext("imagelayer"))
        {
            String textureFilePath = GetParentPath(GetName()) + imageLayerElem.GetChild("image").GetAttribute("source");
            GetSubsystem<ResourceCache>()->BackgroundLoadResource<Texture2D>(textureFilePath, true, this);
        }
    }

    return true;
}

bool TmxFile2D::EndLoad()
{
    if (!loadXMLFile_)
        return false;

    XMLElement rootElem = loadXMLFile_->GetRoot("map");
    String version = rootElem.GetAttribute("version");
    if (version != "1.0")
    {
        URHO3D_LOGERROR("Invalid version");
        return false;
    }

    String orientation = rootElem.GetAttribute("orientation");
    if (orientation == "orthogonal")
        info_.orientation_ = O_ORTHOGONAL;
    else if (orientation == "isometric")
        info_.orientation_ = O_ISOMETRIC;
    else if (orientation == "staggered")
        info_.orientation_ = O_STAGGERED;
    else if (orientation == "hexagonal")
        info_.orientation_ = O_HEXAGONAL;
    else
    {
        URHO3D_LOGERROR("Unsupported orientation type " + orientation);
        return false;
    }

   if (orientation == "staggered" || orientation == "hexagonal")
   {
        if (rootElem.HasAttribute("staggeraxis"))
            info_.staggerX_ = rootElem.GetAttribute("staggeraxis") == "x";
        if (rootElem.HasAttribute("staggerindex"));
            info_.staggerEven_ = rootElem.GetAttribute("staggerindex") == "even";
        info_.hexSideLength_ = rootElem.HasAttribute("hexsidelength") ? rootElem.GetFloat("hexsidelength") * PIXEL_SIZE : 0.0f;
    }

    // Render order (order in layer)
    String renderOrder = rootElem.GetAttribute("renderorder");
    if (renderOrder == "right-down" || orientation != "orthogonal")
        info_.renderOrder_ = RO_RIGHT_DOWN;
    else if (renderOrder == "right-up")
        info_.renderOrder_ = RO_RIGHT_UP;
    else if (renderOrder == "left-down")
        info_.renderOrder_ = RO_LEFT_DOWN;
    else if (renderOrder == "left-up")
        info_.renderOrder_ = RO_LEFT_UP;

    // Map/tiles width and height
    info_.width_ = rootElem.GetInt("width");
    info_.height_ = rootElem.GetInt("height");
    info_.tileWidth_ = rootElem.GetFloat("tilewidth") * PIXEL_SIZE;
    float tileHeight = rootElem.GetFloat("tileheight");

    // Even tile height is expected in staggered orientation
    if (orientation == "staggered" && (int)tileHeight % 2 != 0)
        tileHeight -= 1.0f;
    info_.tileHeight_ = tileHeight * PIXEL_SIZE;

    // A 'diamond' map is expected in isometric orientation
    if (orientation == "isometric")
    {
        info_.width_ = Max(info_.width_, info_.height_);
        info_.height_ = info_.width_;
    }

    // Set background color for the scene
    Color color = Color::GRAY;
    if (rootElem.HasAttribute("backgroundcolor"))
    {
        String backgroundColor = rootElem.GetAttribute("backgroundcolor").Substring(1);
        unsigned col = ToUInt(backgroundColor, 16);
        color = Color(col >> 16 & 0xFF, col >> 8 & 0xFF, col & 0xFF, backgroundColor.Length() > 6 ? col>>24 : 255.0f) * (1 / 255.0f);
    }
    GetSubsystem<Renderer>()->GetDefaultZone()->SetFogColor(color);

    for (unsigned i = 0; i < layers_.Size(); ++i)
        delete layers_[i];
    layers_.Clear();

    for (XMLElement childElement = rootElem.GetChild(); childElement; childElement = childElement.GetNext())
    {
        bool ret = true;
        String name = childElement.GetName();
        if (name == "tileset")
            ret = LoadTileSet(childElement);
        else if (name == "layer")
        {
            TmxTileLayer2D* tileLayer = new TmxTileLayer2D(this);
            ret = tileLayer->Load(childElement, info_);

            layers_.Push(tileLayer);
        }
        else if (name == "objectgroup")
        {
            TmxObjectGroup2D* objectGroup = new TmxObjectGroup2D(this);
            ret = objectGroup->Load(childElement, info_);

            layers_.Push(objectGroup);

        }
        else if (name == "imagelayer")
        {
            TmxImageLayer2D* imageLayer = new TmxImageLayer2D(this);
            ret = imageLayer->Load(childElement, info_);

            layers_.Push(imageLayer);
        }

        if (!ret)
        {
            loadXMLFile_.Reset();
            tsxXMLFiles_.Clear();
            return false;
        }
    }

    loadXMLFile_.Reset();
    tsxXMLFiles_.Clear();
    return true;
}

bool TmxFile2D::SetInfo(Orientation2D orientation, int width, int height, float tileWidth, float tileHeight)
{
    if (layers_.Size() > 0)
        return false;
    info_.orientation_ = orientation;
    info_.width_ = width;
    info_.height_ = height;
    info_.tileWidth_ = tileWidth * PIXEL_SIZE;
    info_.tileHeight_ = tileHeight * PIXEL_SIZE;
    return true;
}

void TmxFile2D::AddLayer(unsigned index, TmxLayer2D *layer)
{
    if (index > layers_.Size())
        layers_.Push(layer);
    else // index <= layers_.size()
        layers_.Insert(index, layer);
}

void TmxFile2D::AddLayer(TmxLayer2D *layer)
{
    layers_.Push(layer);
}

Sprite2D* TmxFile2D::GetTileSprite(int gid) const
{
    HashMap<int, SharedPtr<Sprite2D> >::ConstIterator i = gidToSpriteMapping_.Find(gid);
    if (i == gidToSpriteMapping_.End())
        return 0;

    return i->second_;
}

String TmxFile2D::GetTileAnim(int gid) const
{
    HashMap<int, String>::ConstIterator i = gidToAnimMapping_.Find(gid);
    if (i == gidToAnimMapping_.End())
        return String::EMPTY;

    return i->second_;
}

Vector<SharedPtr<TileMapObject2D> > TmxFile2D::GetTileCollisionShapes(int gid) const
{
    Vector<SharedPtr<TileMapObject2D> > tileShapes;
    HashMap<int, Vector<SharedPtr<TileMapObject2D> > >::ConstIterator i = gidToCollisionShapeMapping_.Find(gid);
    if (i == gidToCollisionShapeMapping_.End())
        return tileShapes;

    return i->second_;
}

PropertySet2D* TmxFile2D::GetTilePropertySet(int gid) const
{
    HashMap<int, SharedPtr<PropertySet2D> >::ConstIterator i = gidToPropertySetMapping_.Find(gid);
    if (i == gidToPropertySetMapping_.End())
        return 0;
    return i->second_;
}

const TmxLayer2D* TmxFile2D::GetLayer(unsigned index) const
{
    if (index >= layers_.Size())
        return 0;

    return layers_[index];
}

SharedPtr<XMLFile> TmxFile2D::LoadTSXFile(const String& source)
{
    String tsxFilePath = GetParentPath(GetName()) + source;
    SharedPtr<File> tsxFile = GetSubsystem<ResourceCache>()->GetFile(tsxFilePath);
    SharedPtr<XMLFile> tsxXMLFile(new XMLFile(context_));
    if (!tsxFile || !tsxXMLFile->Load(*tsxFile))
    {
        URHO3D_LOGERROR("Load TSX file failed " + tsxFilePath);
        return SharedPtr<XMLFile>();
    }

    return tsxXMLFile;
}

bool TmxFile2D::LoadTileSet(const XMLElement& element)
{
    int firstgid = element.GetInt("firstgid");

    XMLElement tileSetElem;
    if (element.HasAttribute("source"))
    {
        String source = element.GetAttribute("source");
        HashMap<String, SharedPtr<XMLFile> >::Iterator i = tsxXMLFiles_.Find(source);
        if (i == tsxXMLFiles_.End())
        {
            SharedPtr<XMLFile> tsxXMLFile = LoadTSXFile(source);
            if (!tsxXMLFile)
                return false;

            // Add to mapping to avoid release
            tsxXMLFiles_[source] = tsxXMLFile;

            tileSetElem = tsxXMLFile->GetRoot("tileset");
        }
        else
            tileSetElem = i->second_->GetRoot("tileset");
    }
    else
        tileSetElem = element;

    XMLElement imageElem = tileSetElem.GetChild("image");

    // Note that a tileset is not mandatory, as we can use individual images, with various texture sizes
    if (!imageElem.IsNull())
    {
        String textureFilePath = GetParentPath(GetName()) + imageElem.GetAttribute("source");
        ResourceCache* cache = GetSubsystem<ResourceCache>();
        SharedPtr<Texture2D> texture(cache->GetResource<Texture2D>(textureFilePath));
        tileSetTextures_.Push(texture);

        int tileWidth = tileSetElem.GetInt("tilewidth");
        int tileHeight = tileSetElem.GetInt("tileheight");
        int spacing = tileSetElem.GetInt("spacing");
        int margin = tileSetElem.GetInt("margin");
        int imageWidth = imageElem.GetInt("width");
        int imageHeight = imageElem.GetInt("height");

        // Set hot spot at left bottom
        Vector2 hotSpot(0.0f, 0.0f);
        if (tileSetElem.HasChild("tileoffset"))
        {
            XMLElement offsetElem = tileSetElem.GetChild("tileoffset");
            hotSpot.x_ += offsetElem.GetFloat("x") / (float)tileWidth;
            hotSpot.y_ += offsetElem.GetFloat("y") / (float)tileHeight;
        }

        int gid = firstgid;
        for (int y = margin; y + tileHeight <= imageHeight - margin; y += tileHeight + spacing)
        {
            for (int x = margin; x + tileWidth <= imageWidth - margin; x += tileWidth + spacing)
            {
                SharedPtr<Sprite2D> sprite(new Sprite2D(context_));
                sprite->SetTexture(texture);
                sprite->SetRectangle(IntRect(x, y, x + tileWidth, y + tileHeight));
                sprite->SetHotSpot(hotSpot);

                gidToSpriteMapping_[gid++] = sprite;
            }
        }
    }

    // Tile properties, animation, collision shape and image
    for (XMLElement tileElem = tileSetElem.GetChild("tile"); tileElem; tileElem = tileElem.GetNext("tile"))
    {
        // Tile image
        for (XMLElement imageElem = tileElem.GetChild("image"); imageElem; imageElem = imageElem.GetNext("image"))
        {
            String textureFilePath = GetParentPath(GetName()) + imageElem.GetAttribute("source");
            ResourceCache* cache = GetSubsystem<ResourceCache>();
            SharedPtr<Texture2D> texture(cache->GetResource<Texture2D>(textureFilePath));
            if (!texture)
                return false;

            SharedPtr<Sprite2D> sprite(new Sprite2D(context_));
            sprite->SetTexture(texture);
            sprite->SetRectangle(IntRect(0, 0, imageElem.GetInt("width"), imageElem.GetInt("height")));
            sprite->SetHotSpot(Vector2::ZERO); // Set hot spot at left bottom

            gidToSpriteMapping_[firstgid + tileElem.GetInt("id")] = sprite;
        }

        // Tile collision shape(s)
        for (XMLElement collisionElem = tileElem.GetChild("objectgroup"); collisionElem; collisionElem = collisionElem.GetNext("objectgroup"))
        {
            Vector<SharedPtr<TileMapObject2D> > objects;
            for (XMLElement objectElem = collisionElem.GetChild("object"); objectElem; objectElem = objectElem.GetNext("object"))
            {
                SharedPtr<TileMapObject2D> object(new TileMapObject2D());
                IntVector2 spriteSize = GetTileSprite(firstgid + tileElem.GetInt("id"))->GetRectangle().Size();

                // Convert Tiled local position (left top) to Urho3D local position (left bottom)
                objectElem.SetAttribute("y", String(info_.GetMapHeight() / PIXEL_SIZE - (spriteSize.y_ - objectElem.GetFloat("y"))));

                TmxObjectGroup2D* objectGroup = new TmxObjectGroup2D(this);
                objectGroup->StoreObject(objectElem, object, info_, true);
                objects.Push(object);
            }
            gidToCollisionShapeMapping_[firstgid + tileElem.GetInt("id")] = objects;
        }

        // Tile animation
        for (XMLElement animElem = tileElem.GetChild("animation"); animElem; animElem = animElem.GetNext("animation"))
        {
            Vector<IntVector2> frames;

            for (XMLElement frameElem = animElem.GetChild("frame"); frameElem; frameElem = frameElem.GetNext("frame"))
                frames.Push(IntVector2(firstgid + frameElem.GetInt("tileid"), frameElem.GetInt("duration")));

            String animName = "TileAnimationGid_" + String(firstgid + tileElem.GetInt("id"));
            gidToAnimMapping_[firstgid + tileElem.GetInt("id")] = animName;
            CreateProceduralAnimation(animName, frames);
        }

        // Custom properties
        if (tileElem.HasChild("properties"))
        {
            SharedPtr<PropertySet2D> propertySet(new PropertySet2D());
            propertySet->Load(tileElem.GetChild("properties"));
            gidToPropertySetMapping_[firstgid + tileElem.GetInt("id")] = propertySet;
        }
    }

    return true;
}

void TmxFile2D::GetActualGid(unsigned& gid, Vector3& flipAxis)
{
    // Bits on the far end of the 32-bit global tile ID (gid) are used for tile flags
    const unsigned FLIPPED_HORIZONTALLY_FLAG = 0x80000000;
    const unsigned FLIPPED_VERTICALLY_FLAG = 0x40000000;
    const unsigned FLIPPED_DIAGONALLY_FLAG = 0x20000000;

    // Read out the flags
    bool flipped_horizontally = (gid & FLIPPED_HORIZONTALLY_FLAG);
    bool flipped_vertically = (gid & FLIPPED_VERTICALLY_FLAG);
    bool flipped_diagonally = (gid & FLIPPED_DIAGONALLY_FLAG);

    // Store flips
    if (flipped_horizontally) flipAxis.x_ = 1.0f;
    if (flipped_vertically) flipAxis.y_ = 1.0f;
    if (flipped_diagonally) flipAxis.z_ = 1.0f;

    // Clear the flags
    gid &= ~(FLIPPED_HORIZONTALLY_FLAG | FLIPPED_VERTICALLY_FLAG | FLIPPED_DIAGONALLY_FLAG);
}

AnimationSet2D* TmxFile2D::CreateProceduralAnimation(String animName, Vector<IntVector2> frames)
{
    if (frames.Empty())
        return 0;

    // SpriterData (we can skip header, folders and files as we are using a spritesheet)
    Spriter::SpriterData* spriterData = new Spriter::SpriterData();

    // Entity
    PODVector<Spriter::Entity*> entities;

    Spriter::Entity* entity = new Spriter::Entity();
    entity->id_ = 0;
    entity->name_ = animName;

    // Animation
    PODVector<Spriter::Animation*> animations;

    Spriter::Animation* animation = new Spriter::Animation();
    animation->id_ = 0;
    animation->name_ = "TileAnim";
    float animLength = 0.0f;
    for (int i = 0; i < frames.Size(); ++i)
        animLength += (float)frames[i].y_ / 1000.0f; // Frames are in ms
    animation->length_ = animLength;
    animation->looping_ = true;

    // Mainline
    PODVector<Spriter::MainlineKey*> mainlineKeys;

    float time = 0.0f;
    for (int i = 0; i < frames.Size(); ++i)
    {
        // Key
        Spriter::MainlineKey* mainlineKey = new Spriter::MainlineKey();
        mainlineKey->id_ = i;
        mainlineKey->time_ = time;
        time += (float)frames[i].y_ / 1000.0f;  // Frames are in ms

        // Ref
        PODVector<Spriter::Ref*> objectRefs;

        Spriter::Ref* objectRef = new Spriter::Ref();
        objectRef->id_ = 0;
        objectRef->parent_ = -1;
        objectRef->timeline_ = 0;
        objectRef->key_ = i;
        objectRefs.Push(objectRef);

        mainlineKey->objectRefs_ = objectRefs;
        mainlineKeys.Push(mainlineKey);
    }

    animation->mainlineKeys_ = mainlineKeys;

    // Timeline
    PODVector<Spriter::Timeline*> timelines;

    Spriter::Timeline* timeline = new Spriter::Timeline();
    timeline->id_ = 0;
    timeline->name_ = "TileAnim";
    timeline->objectType_ = Spriter::SPRITE;

    // Keys
    PODVector<Spriter::SpatialTimelineKey*> keys;

    for (int i = 0; i < frames.Size(); ++i)
    {
        Spriter::SpriteTimelineKey* key = new Spriter::SpriteTimelineKey(timeline);
        key->folderId_ = 0;
        key->fileId_ = i;
        key->useDefaultPivot_ = true; // Equivalent to key->pivotX_ = 0.5f and key->pivotY_ = 0.5f

        keys.Push((Spriter::SpatialTimelineKey*)(key));
    }

    timeline->keys_ = keys;
    timelines.Push(timeline);

    animation->timelines_ = timelines;

    animations.Push(animation);

    entity->animations_ = animations;

    // Add entity
    entities.Push(entity);
    spriterData->entities_ = entities;

    // Create the AnimationSet2D resource
    AnimationSet2D* anim(new AnimationSet2D(context_));
    anim->SetName(animName);
    anim->SetSpriterData(spriterData);

    // Set sprites
    HashMap<int, SharedPtr<Sprite2D> > sprites;
    for (int i = 0; i < frames.Size(); ++i)
    {
        IntVector2 frame = frames[i];
        int key = (0 << 16) + i;
        Sprite2D* sprite = GetTileSprite(frame.x_);
        sprite->SetHotSpot(Vector2(0.5f, 0.5f));
        sprites[key] = sprite;

        if (!anim->GetSprite())
            anim->SetSprite(sprite);
    }

    anim->SetSpriterFileSprites(sprites);

    // Add anim to the ResourceCache
    GetSubsystem<ResourceCache>()->AddManualResource(anim);

    return anim;
}

}
