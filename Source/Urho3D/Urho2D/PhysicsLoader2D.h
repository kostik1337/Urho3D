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

#include "../Core/Context.h"
#include "../Resource/Resource.h"
#include "../Urho2D/RigidBody2D.h"

namespace Urho3D
{

class JSONFile;
class PListFile;
class Sprite2D;
class XMLElement;
class XMLFile;


/// Circle Data.
struct CircleData2D
{
    /// Circle radius.
    float radius_;
    /// Circle center (position).
    Vector2 center_;
};

/// Fixture data.
struct URHO3D_API FixtureData2D
{
    /// Construct.
    FixtureData2D() :
        density_(1.0f),
        friction_(0.2f),
        restitution_(0.0f),
        categoryBits_(1),
        groupIndex_(0),
        maskBits_(65535),
        trigger_(false),
        solid_(true)
    {
    }

    /// Vertices (polygons). One Vector for each polygon.
    Vector<Vector<Vector2> > vertices_;
    /// Cercle info (center and radius).
    Vector<CircleData2D> circles_;
    /// Polyline info (points).
    Vector<Vector2> points_;
    /// Shape density.
    float density_;
    /// Shape friction.
    float friction_;
    /// Shape restitution.
    float restitution_;
    /// Shape category bits.
    int categoryBits_;
    /// Shape group index.
    int groupIndex_;
    /// Shape mask bits.
    int maskBits_;
    /// Shape trigger flag.
    bool trigger_;
    /// Flag to discriminate between solid (polygon) and hollow (polyline) shapes.
    bool solid_;
};

/// Physics information.
struct URHO3D_API PhysicsInfo2D
{
    /// Construct.
    PhysicsInfo2D() :
        bodyType_(BT_DYNAMIC),
        mass_(1.0f),
        gravityScale_(1.0f),
        fixedRotation_(false),
        scale_(1.0f),
        leftBottom_(false),
        pbeNoSprite_(false)
    {
    }

    /// Rigid body type.
    BodyType2D bodyType_;
    /// Rigid body mass.
    float mass_;
    /// Rigid body gravity scale.
    float gravityScale_;
    /// Toggle rotation for rigid body.
    bool fixedRotation_;
    /// Name
    String name_;
    /// Name hash.
    StringHash nameHash_;
    /// Node scale.
    float scale_;
    /// Fixtures.
    Vector<FixtureData2D> fixtures_;
    /// Sprite.
    SharedPtr<Sprite2D> sprite_;
    /// Origin.
    Vector2 origin_;
    /// Left-bottom pivot flag used to trigger shape centering according to sprite's size.
    bool leftBottom_;
    /// Flag used to trigger collision shape scaling and centering according to sprite size when sprite is not found or not set in Physics Body Editor file.
    bool pbeNoSprite_;
};


/// Physics Body Editor (json) or PhysicsEditor (xml or plist) file.
class URHO3D_API PhysicsLoader2D : public Resource
{
    URHO3D_OBJECT(PhysicsLoader2D, Resource);

public:
    /// Construct.
    PhysicsLoader2D(Context* context);
    /// Destruct.
    virtual ~PhysicsLoader2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Load resource from stream. May be called from a worker thread. Return true if successful.
    virtual bool BeginLoad(Deserializer& source);
    /// Finish resource loading. Always called from the main thread. Return true if successful.
    virtual bool EndLoad();

    /// Return number of definitions in the file.
    unsigned GetNumDefs() const { return physicsData_.Size(); }

    /// Return physics definitions.
    const HashMap<StringHash, PhysicsInfo2D>& GetPhysicsDefs() const { return physicsData_; }

private:
    /// Begin load from PList file.
    bool BeginLoadFromPListFile(Deserializer& source);
    /// End load from PList file.
    bool EndLoadFromPListFile();
    /// Begin load from XML file.
    bool BeginLoadFromXMLFile(Deserializer& source);
    /// End load from XML file.
    bool EndLoadFromXMLFile();
    /// Begin load from JSON file.
    bool BeginLoadFromJSONFile(Deserializer& source);
    /// End load from JSON file.
    bool EndLoadFromJSONFile();

    /// PList file used while loading.
    SharedPtr<PListFile> loadPListFile_;
    /// XML file used while loading.
    SharedPtr<XMLFile> loadXMLFile_;
    /// JSON file used while loading.
    SharedPtr<JSONFile> loadJSONFile_;

    /// Physics data (one per object in the source file).
    HashMap<StringHash, PhysicsInfo2D> physicsData_;
};

/// %Physics definitions component.
class URHO3D_API PhysicsData2D : public Component
{
    URHO3D_OBJECT(PhysicsData2D, Component);

public:
    /// Construct.
    PhysicsData2D(Context* context);
    /// Destruct.
    virtual ~PhysicsData2D();
    /// Register object factory.
    static void RegisterObject(Context* context);

    /// Set resource file to use.
    void SetPhysicsLoader(PhysicsLoader2D* loader);
    /// Return physics definitions from resource file.
    const HashMap<StringHash, PhysicsInfo2D>& GetPhysicsDefs() const { return physicsData_; }
    /// Create nodes, sprites, rigid bodies and collision shapes for every objects in the resource file. Optionaly set sprites to use.
    const Vector<Node*> CreatePhysicalSprites(Vector<Sprite2D*> sprites = Vector<Sprite2D*>(), bool savePrefabs = false);
    /// Create node, sprite, rigid body and collision shape for a given object in the resource file. Optionaly set sprite to use.
    Node* CreatePhysicalSprite(const String& name, Sprite2D* sprite = 0, bool savePrefab = false);

    /// Return number of objects in the resource file.
    unsigned GetNumDefs() const { return physicsData_.Size(); }

    /// Return physics data by name.
    PhysicsInfo2D* GetPhysicsData(const String& name);
    /// Return physics data by name hash.
    PhysicsInfo2D* GetPhysicsData(StringHash nameHash);

private:
    /// Physics data (one per object).
    HashMap<StringHash, PhysicsInfo2D> physicsData_;
    /// Store loader path in case loader gets cleared from the resource cache.
    String loaderPath_;
};

}
