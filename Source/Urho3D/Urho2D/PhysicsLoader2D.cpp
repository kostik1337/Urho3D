//
// Copyright (c) 2008-2016 the Urho3D project.
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

#include "../Urho2D/CollisionChain2D.h"
#include "../Urho2D/CollisionCircle2D.h"
#include "../Urho2D/CollisionPolygon2D.h"
#include "../Core/Context.h"
#include "../IO/FileSystem.h"
#include "../Resource/JSONFile.h"
#include "../IO/Log.h"
#include "../Resource/PListFile.h"
#include "../Resource/ResourceCache.h"
#include "../Urho2D/RigidBody2D.h"
#include "../Scene/Scene.h"
#include "../Urho2D/Sprite2D.h"
#include "../Urho2D/StaticSprite2D.h"
#include "../Resource/XMLFile.h"

#include "../Urho2D/PhysicsLoader2D.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const float PIXEL_SIZE;
extern const char* URHO2D_CATEGORY;


PhysicsLoader2D::PhysicsLoader2D(Context* context) :
	Resource(context)
{
}

PhysicsLoader2D::~PhysicsLoader2D()
{
}

void PhysicsLoader2D::RegisterObject(Context* context)
{
	context->RegisterFactory<PhysicsLoader2D>();
}

bool PhysicsLoader2D::BeginLoad(Deserializer& source)
{
	if (GetName().Empty())
		SetName(source.GetName());

	String extension = GetExtension(source.GetName());
    if (extension == ".plist")
        return BeginLoadFromPListFile(source);

	if (extension == ".xml")
		return BeginLoadFromXMLFile(source);

	if (extension == ".json")
		return BeginLoadFromJSONFile(source);

	URHO3D_LOGERROR("Unsupported file type");
	return false;
}

bool PhysicsLoader2D::EndLoad()
{
    if (loadPListFile_)
        return EndLoadFromPListFile();

	if (loadXMLFile_)
		return EndLoadFromXMLFile();

	if (loadJSONFile_)
		return EndLoadFromJSONFile();

	return false;
}

bool PhysicsLoader2D::BeginLoadFromPListFile(Deserializer& source)
{
    loadPListFile_ = new PListFile(context_);
    if (!loadPListFile_->Load(source))
    {
        URHO3D_LOGERROR("Could not load PhysicsEditor file");
        loadPListFile_.Reset();
        return false;
    }

    SetMemoryUse(source.GetSize());
    return true;
}

bool PhysicsLoader2D::EndLoadFromPListFile()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();

    const PListValueMap& root = loadPListFile_->GetRoot();
    const PListValueMap& metadata = root["metadata"]->GetValueMap();

//	const PListValueVector& catNames = root["category_names"]->GetValueVector();
//	for (unsigned n = 0; n < fixtures.Size(); ++n)
//	{
//		= catNames[n].GetString();
//	}

    const PListValueMap& bodies = root["bodies"]->GetValueMap();
    for (PListValueMap::ConstIterator i = bodies.Begin(); i != bodies.End(); ++i)
    {
		// Name and body type
		String name = i->first_.Split('.')[0];
		StringHash nameHash(name);
		PhysicsInfo2D& info = physicsData_[nameHash];
		info.scale_ = root["scale_factor"]->GetFloat();
		info.name_ = name;
		info.nameHash_ = nameHash;

        const PListValueMap& settings = i->second_.GetValueMap();

		// Origin
        info.origin_ = settings["anchorpoint"]->GetVector2();
        info.leftBottom_ = true; // Shape pivot is left-bottom so we can't offset coords without knowing sprite's size

		info.bodyType_ = settings["is_static"]->GetBool() ? BT_STATIC : BT_DYNAMIC;
		info.gravityScale_ = (int)settings["affected_by_gravity"]->GetBool();
		info.fixedRotation_ = !settings["allows_rotation"]->GetBool();

		// Fixtures
		const PListValueVector& fixtures = settings["fixtures"]->GetValueVector();
		for (unsigned f = 0; f < fixtures.Size(); ++f)
		{
			const PListValueMap& fixture = fixtures[f].GetValueMap();

			FixtureData2D fixtureData;
			info.mass_ = fixture["mass"]->GetFloat();
//			fixtureData. = fixture["elasticity"]->GetFloat();
			fixtureData.friction_ = fixture["friction"]->GetFloat();
//			fixtureData. = fixture["surface_velocity"]->GetVector2();
//			fixtureData. = fixture["corner_radius"]->GetFloat();
			fixtureData.categoryBits_ = fixture["collision_categories"]->GetInt();
			fixtureData.maskBits_ = fixture["collision_mask"]->GetInt();
//			fixtureData. = fixture["collision_type"]->GetInt();
			fixtureData.groupIndex_ = fixture["collision_group"]->GetInt();
			fixtureData.trigger_ = fixture["is_sensor"]->GetBool();

			String type = fixture["fixture_type"]->GetString();

			if (type == "POLYGON" || type == "POLYLINE")
			{
				fixtureData.solid_ = type == "POLYGON";
				const PListValueVector& polygons = fixture["polygons"]->GetValueVector();
				for (unsigned p = 0; p < polygons.Size(); ++p)
				{
					// Vertices
					Vector<Vector2> points;
					const PListValueVector& vertices = polygons[p].GetValueVector();
					for (unsigned v = 0; v < vertices.Size(); ++v)
					{
						Vector2 vertex = vertices[v].GetVector2();
						points.Push(Vector2(vertex.x_, vertex.y_) * PIXEL_SIZE);
					}
					fixtureData.vertices_.Push(points);
				}
			}
			else if (type == "CIRCLE")
			{
				CircleData2D circleData;

				// Get position and radius
				const PListValueMap& circle = fixture["circle"]->GetValueMap();
				Vector2 position = circle["position"]->GetVector2();
				circleData.center_ = Vector2(position.x_ * PIXEL_SIZE, position.y_ * PIXEL_SIZE);
				circleData.radius_ = circle["radius"]->GetFloat() * PIXEL_SIZE;

				fixtureData.circles_.Push(circleData);
			}
			else
				URHO3D_LOGERROR("Unsupported fixture type: " + type);

			info.fixtures_.Push(fixtureData);
		}
    }

    loadPListFile_.Reset();
    return true;
}

bool PhysicsLoader2D::BeginLoadFromXMLFile(Deserializer& source)
{
	loadXMLFile_ = new XMLFile(context_);
	if (!loadXMLFile_->Load(source) || !loadXMLFile_->GetRoot("bodydef"))
	{
		URHO3D_LOGERROR("Could not load PhysicsEditor file");
		loadXMLFile_.Reset();
		return false;
	}

	SetMemoryUse(source.GetSize());
	return true;
}

bool PhysicsLoader2D::EndLoadFromXMLFile()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	XMLElement root = loadXMLFile_->GetRoot("bodydef");
	XMLElement bodies = root.GetChild("bodies");
	XMLElement body = bodies.GetChild("body");
	while (body)
	{
		// Name and body type
		String name = body.GetAttribute("name");
		StringHash nameHash(name);
		PhysicsInfo2D& info = physicsData_[nameHash];
		info.name_ = name;
		info.nameHash_ = nameHash;
		info.bodyType_ = body.GetBool("dynamic") ? BT_DYNAMIC : BT_STATIC;

		// Collision shape settings
		XMLElement fixture = body.GetChild("fixture");
		while (fixture)
		{
			FixtureData2D fixtureData;
			fixtureData.density_ = fixture.GetFloat("density");
			fixtureData.friction_ = fixture.GetFloat("friction");
			fixtureData.restitution_ = fixture.GetFloat("restitution");
			fixtureData.categoryBits_ = fixture.GetInt("filter_categoryBits");
			fixtureData.groupIndex_ = fixture.GetInt("filter_groupIndex");
			fixtureData.maskBits_ = fixture.GetUInt("filter_maskBits");
			fixtureData.trigger_ = fixture.GetBool("isSensor");

			// Collision shape (POLYGON or CIRCLE)
			String type = fixture.GetAttribute("type");

			if (type == "POLYGON" || type == "POLYLINE")
			{
				fixtureData.solid_ = type == "POLYGON";
				XMLElement polygon = fixture.GetChild("polygon");
				while (polygon)
				{
					Vector<Vector2> vertices;
					XMLElement vertex = polygon.GetChild("vertex");
					while (vertex)
					{
						vertices.Push(Vector2(vertex.GetFloat("x"), 0.5f - vertex.GetFloat("y")) * PIXEL_SIZE);
						vertex = vertex.GetNext("vertex");
					}
					fixtureData.vertices_.Push(vertices);
					polygon = polygon.GetNext("polygon");
				}
			}
			else if (type == "CIRCLE")
			{
				XMLElement circle = fixture.GetChild("circle");
				while (circle)
				{
					CircleData2D circleData;

					// Get position and radius
					XMLElement position = circle.GetChild("position");
					circleData.center_ = Vector2(position.GetFloat("x") * PIXEL_SIZE, 0.5f - position.GetFloat("y") * PIXEL_SIZE);
					circleData.radius_ = circle.GetFloat("radius") * PIXEL_SIZE;

					fixtureData.circles_.Push(circleData);
					circle = circle.GetNext("circle");
				}
			}
			else
				URHO3D_LOGERROR("Unsupported fixture type: " + type);

			info.fixtures_.Push(fixtureData);
			fixture = fixture.GetNext("fixture");
		}
		body = body.GetNext("body");
	}

	loadXMLFile_.Reset();
	return true;
}

bool PhysicsLoader2D::BeginLoadFromJSONFile(Deserializer& source)
{
	loadJSONFile_ = new JSONFile(context_);
	if (!loadJSONFile_->Load(source) || loadJSONFile_->GetRoot().IsNull())
	{
		URHO3D_LOGERROR("Could not load Physics Body Editor file");
		loadJSONFile_.Reset();
		return false;
	}

	SetMemoryUse(source.GetSize());
	return true;
}

bool PhysicsLoader2D::EndLoadFromJSONFile()
{
	ResourceCache* cache = GetSubsystem<ResourceCache>();
	JSONValue root = loadJSONFile_->GetRoot();
	JSONArray rigidBodies = root.Get("rigidBodies").GetArray();

	for (unsigned i = 0; i < rigidBodies.Size(); ++i)
	{
		JSONValue body = rigidBodies[i];

		// Name and body type
		String name = body.Get("name").GetString();
		StringHash nameHash(name);
		PhysicsInfo2D& info = physicsData_[nameHash];
		info.name_ = name;
		info.nameHash_ = nameHash;

		// Sprite
		float scale = 1.0f;
		Vector2 center;
		String imagePath = GetParentPath(GetName()) + body.Get("imagePath").GetString();
		if (cache->Exists(imagePath))
		{
			Sprite2D* sprite = cache->GetResource<Sprite2D>(imagePath);
			if (sprite)
			{
				info.sprite_ = sprite;
				IntVector2 spriteSize = sprite->GetRectangle().Size();
				scale = (float)spriteSize.x_ / 100.0f; // Scale is normalized to width = 100
				center = Vector2(0.5f, ((float)spriteSize.y_ / (float)spriteSize.x_) * 0.5f);
			}
		}
		// When sprite is not set or not found, trigger scaling later at sprite creation
		if (center == Vector2::ZERO)
			info.pbeNoSprite_ = true;

		// Origin
		JSONValue origin = body.Get("origin");
		info.origin_ = Vector2(origin.Get("x").GetFloat(), origin.Get("y").GetFloat());

		FixtureData2D fixtureData;

		// Polygons and polylines (although the format doesn't natively support poly lines, we can manually substitute 'polygons' by 'polylines' in the file
		JSONArray polygons = body.Get("polygons").GetArray();
		if (polygons.Size() == 0)
		{
			polygons = body.Get("shapes").GetArray();
			if (polygons.Size() != 0)
			{
				fixtureData.solid_ = false;
				info.bodyType_ = BT_STATIC;
				info.mass_ = 0.0f;
				info.fixedRotation_ = true;
				info.gravityScale_ = 0.0f;
			}
		}

		for (unsigned p = 0; p < polygons.Size(); ++p)
		{
			Vector<Vector2> vertices;
			JSONArray polygon = polygons[p].GetArray();

			// Poly line
			if (polygon.Size() == 0 && polygons[p].Get("type").GetString() == "POLYGON")
			{
				polygon = polygons[p].Get("vertices").GetArray();
				JSONValue point = polygon[0];
				polygon.Push(point); // Close the loop
			}

			for (unsigned v = 0; v < polygon.Size(); ++v)
			{
				JSONValue point = polygon[v];
				vertices.Push((Vector2(point.Get("x").GetFloat(), point.Get("y").GetFloat()) - center) * scale);
			}
			fixtureData.vertices_.Push(vertices);
		}

		// Circles
		JSONValue circles = body.Get("circles");
		for (unsigned c = 0; c < circles.Size(); ++c)
		{
			CircleData2D circleData;

			// Get center and radius
			JSONValue circle = circles[c];
			circleData.center_ = (Vector2(circle.Get("cx").GetFloat(), circle.Get("cy").GetFloat()) - center) * scale;
			circleData.radius_ = circle.Get("r").GetFloat() * scale;
			fixtureData.circles_.Push(circleData);
		}

		info.fixtures_.Push(fixtureData);
	}

	loadJSONFile_.Reset();
    return true;
}


PhysicsData2D::PhysicsData2D(Context* context) :
    Component(context)
{
}

PhysicsData2D::~PhysicsData2D()
{
}

void PhysicsData2D::RegisterObject(Context* context)
{
    context->RegisterFactory<PhysicsData2D>(URHO2D_CATEGORY);
    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
}

void PhysicsData2D::SetPhysicsLoader(PhysicsLoader2D* loader)
{
    if (!loader)
        return;

	loaderPath_ = GetPath("Data/" + loader->GetName());
    physicsData_ = loader->GetPhysicsDefs();
}

const Vector<Node*> PhysicsData2D::CreatePhysicalSprites(Vector<Sprite2D*> sprites, bool savePrefabs)
{
	Vector<Node*> nodes;

	if (physicsData_.Empty())
		return nodes;

	unsigned s = 0;
	for (HashMap<StringHash, PhysicsInfo2D>::Iterator i = physicsData_.Begin(); i != physicsData_.End(); ++i)
    {
		PhysicsInfo2D& info = i->second_;
		Node* node = CreatePhysicalSprite(String(info.name_), sprites.Empty() || sprites.Size() <= s ? 0 : sprites[s], savePrefabs);
		nodes.Push(node);
		++s;
	}
	return nodes;
}

Node* PhysicsData2D::CreatePhysicalSprite(const String& name, Sprite2D* newSprite, bool savePrefab)
{
	if (physicsData_.Empty())
		return (Node*)0;

	PhysicsInfo2D* info = GetPhysicsData(StringHash(name));
	if (!info)
		return 0;

	Vector2 offset;
	Vector2 shapeCenter;
	float shapeScale = 1.0f;

	// Create node
	Node* node = GetScene()->CreateChild(name);
	node->SetScale(info->scale_);

	// Create rigid body
	RigidBody2D* body = node->CreateComponent<RigidBody2D>();
	body->SetBodyType(info->bodyType_);
	body->SetMass(info->mass_);
	body->SetFixedRotation(info->fixedRotation_);
	body->SetGravityScale(info->gravityScale_);

	// Create sprite
	if (newSprite || info->sprite_)
	{
		Sprite2D* sprite = newSprite ? newSprite : info->sprite_;
		if (sprite)
		{
			StaticSprite2D* staticSprite = node->CreateComponent<StaticSprite2D>();
			staticSprite->SetSprite(sprite);
			IntVector2 spriteSize = sprite->GetRectangle().Size();

			// Offset collision shapes with left-bottom coordinates by half sprite size (because we use default centered hot spot)
			if (info->leftBottom_)
				offset = Vector2((float)spriteSize.x_, (float)spriteSize.y_) * (PIXEL_SIZE * 0.5f);
			if (info->pbeNoSprite_)
			{
				shapeScale = (float)spriteSize.x_ / 100.0f;
				shapeCenter = Vector2(0.5f, ((float)spriteSize.y_ / (float)spriteSize.x_) * 0.5f);
			}
		}
	}

	Vector<FixtureData2D> fixtures = info->fixtures_;
	for (unsigned f = 0; f < fixtures.Size(); ++f)
	{
		FixtureData2D fixtureData = fixtures[f];

		// Polygons and polylines
		for (unsigned p = 0; p < fixtureData.vertices_.Size(); ++p)
		{
			CollisionShape2D* shape;
			Vector<Vector2> vertices = fixtureData.vertices_[p];
			int numVertices = vertices.Size();

			if (fixtureData.solid_) // Polygon
			{
				shape = node->CreateComponent<CollisionPolygon2D>();
				reinterpret_cast<CollisionPolygon2D*>(shape)->SetVertexCount(numVertices);
				for (unsigned v = 0; v < numVertices; ++v)
					reinterpret_cast<CollisionPolygon2D*>(shape)->SetVertex(v, (vertices[v] - shapeCenter) * shapeScale - offset);
			}
			else // Polyline
			{
				shape = node->CreateComponent<CollisionChain2D>();
				reinterpret_cast<CollisionChain2D*>(shape)->SetVertexCount(numVertices);
				for (unsigned v = 0; v < numVertices; ++v)
					reinterpret_cast<CollisionChain2D*>(shape)->SetVertex(v, (vertices[v] - shapeCenter) * shapeScale - offset);
			}

			// Shape tuning
			shape->SetDensity(fixtureData.density_);
			shape->SetFriction(fixtureData.friction_);
			shape->SetRestitution(fixtureData.restitution_);
			shape->SetCategoryBits(fixtureData.categoryBits_);
			shape->SetGroupIndex(fixtureData.groupIndex_);
			shape->SetMaskBits(fixtureData.maskBits_);
			shape->SetTrigger(fixtureData.trigger_);
		}

		// Circles
		Vector<CircleData2D> circles = fixtureData.circles_;
		for (unsigned c = 0; c < circles.Size(); ++c)
		{
			CollisionCircle2D* shape = node->CreateComponent<CollisionCircle2D>();

			CircleData2D circleData = circles[c];
			Vector2 center = (circleData.center_ - shapeCenter) * shapeScale - offset;
			shape->SetCenter(center.x_, center.y_);
			shape->SetRadius(circleData.radius_ * shapeScale);

			// Shape tuning
			shape->SetDensity(fixtureData.density_);
			shape->SetFriction(fixtureData.friction_);
			shape->SetRestitution(fixtureData.restitution_);
			shape->SetCategoryBits(fixtureData.categoryBits_);
			shape->SetGroupIndex(fixtureData.groupIndex_);
			shape->SetMaskBits(fixtureData.maskBits_);
			shape->SetTrigger(fixtureData.trigger_);
		}
	}

	// Save node as prefab
	if (savePrefab)
	{
		File prefab(context_, loaderPath_ + name + ".bin", FILE_WRITE);
		node->Save(prefab);
	}

	return node;
}

PhysicsInfo2D* PhysicsData2D::GetPhysicsData(const String& name)
{
    return GetPhysicsData(StringHash(name));
}

PhysicsInfo2D* PhysicsData2D::GetPhysicsData(StringHash nameHash)
{
    HashMap<StringHash, PhysicsInfo2D>::Iterator i = physicsData_.Find(nameHash);
    return i != physicsData_.End() ? &i->second_ : (PhysicsInfo2D*)0;
}

}