// Urho2D navigation example.
// This sample demonstrates:
//     - Creating a complete 2D scene with tile map, Box2D rigid bodies and collision shapes, and navigation mesh
//     - Performing path queries to the navigation mesh (pathfinding)
//     - Rebuilding the navigation mesh partially when adding or removing objects
//     - Visualizing custom debug geometry
//     - Raycasting drawable components
//     - Rendering 3D content in the 2D scene by adjusting camera position accordingly (enabled in debug mode)
// Note that for now 2D navigation feature is only available from a tile map

#include "Scripts/Utilities/Sample.as"

Node@ impNode;
Node@ n; // NavMesh


//--------------
void Start()
//--------------
{
	SampleStart();
	CreateScene();
	CreateUI();
	input.mouseVisible = true;
	SubscribeToEvents();
}


//------------------------
void CreateScene()
//------------------------
{
	// SCENE
	scene_ = Scene();
	scene_.CreateComponent("Octree");
	scene_.CreateComponent("DebugRenderer");
	PhysicsWorld2D@ physicsWorld = scene_.CreateComponent("PhysicsWorld2D");
	physicsWorld.gravity = Vector2(0.0f, 0.0f); // Neutralize gravity as the characters will always be grounded
	physicsWorld.drawJoint = true; // Display the joints

	// LIGHT (for 3D models)
	Node@ lightNode = scene_.CreateChild("DirectionalLight");
	lightNode.direction = Vector3(0, -0.7, 0.7);
	Light@ light = lightNode.CreateComponent("Light");
	light.lightType = LIGHT_DIRECTIONAL;
	light.color = Color(0.4, 1.0, 0.4);
	light.specularIntensity = 1.5;

	// TILE MAP
	Node@ tileMapNode = scene_.CreateChild("TileMap");
	TileMap2D@ tileMap = tileMapNode.CreateComponent("TileMap2D");
	tileMap.tmxFile = cache.GetResource("TmxFile2D", "Assets/Urho2D/Tilesets/atrium3.tmx");
	n = tileMap.navMesh.node;
	if (n is null)
	{
		log.Error("Aborted: no NavMesh found in this tmx file");
		return;
	}

	// CHARACTER
	impNode = SpawnImp(Vector2(13.0f, 13.0f), false);
	AnimatedSprite2D@ animatedSprite = impNode.GetComponent("AnimatedSprite2D");
	animatedSprite.color = Color(0.0f, 1.0f, 1.0f, 1.0f);

	// BALLS
	TileMapInfo2D@ info = tileMap.info;
	for (uint i = 0; i < 40; ++i)
	{
		Node@ ball  = scene_.CreateChild("ball");
		ball.position = Vector3(Random(0.0f, info.mapWidth), Random(0.0f, info.mapHeight), 0.0f);
		StaticSprite2D@ sprite = ball.CreateComponent("StaticSprite2D");
		sprite.sprite = cache.GetResource("Sprite2D", "Urho2D/Ball.png");
		sprite.layer = 2;
		RigidBody2D@ body = ball.CreateComponent("RigidBody2D");
		body.bodyType = BT_DYNAMIC;
		CollisionCircle2D@ shape = ball.CreateComponent("CollisionCircle2D");
		shape.radius = 0.16f;
		shape.friction = 0.9f;
		shape.density = 1.0f;
		shape.restitution = 0.5f;
	}

	// CAMERA
	cameraNode = Node();
	Camera@ camera = cameraNode.CreateComponent("Camera");
	camera.orthographic = true;
	camera.orthoSize = graphics.height * PIXEL_SIZE;
	camera.zoom = 1.0f * Min(graphics.width / 1280.0f, graphics.height / 800.0f); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.0) is set for full visibility at 1280x800 resolution)
	camera.viewMask = 127; // Skip layer 128 where 3D navMesh geometry lies
	renderer.viewports[0] = Viewport(scene_, camera);
	cameraNode.position = Vector3(impNode.position2D, -10.0f);
}


//-------------------------------------------------------------------
Node@ SpawnImp(Vector2 pos, bool random = true)
//-------------------------------------------------------------------
{
	Node@ imp = scene_.CreateChild("Imp");
	imp.position = Vector3(pos, 0.0f);
	imp.SetScale(0.15f);
	AnimatedSprite2D@ animatedSprite = imp.CreateComponent("AnimatedSprite2D");
	animatedSprite.animationSet = cache.GetResource("AnimationSet2D", "Urho2D/imp/imp.scml");
	animatedSprite.SetAnimation("idle"); // Play "idle" anim

	// For layering purpose, put character on layer 10 (tileMap layer #1 * 10) and set hot spot to bottom-center
	// Order in layer will be updated to ensure accurate occlusions
	animatedSprite.layer = 10;
	animatedSprite.useHotSpot = true;
	animatedSprite.hotSpot = Vector2(0.5f, 0.0f);

	// Create rigid body and collision shape
	RigidBody2D@ body = imp.CreateComponent("RigidBody2D");
	body.bodyType = BT_KINEMATIC; // Body is not moved by physics simulation
	body.allowSleep = false;
	CollisionCircle2D@ shape = imp.CreateComponent("CollisionCircle2D");
	shape.radius = 1.1f; // Set shape size
	shape.friction = 0.0f; // Set friction
	shape.restitution = 0.1f; // Bounce

	// Create the Crowd logic object, which takes care of steering the imp
	Crowd@ crowd = cast<Crowd>(imp.CreateScriptObject(scriptFile, "Crowd"));
	crowd.navMesh = n.GetComponent("NavigationMesh");
	crowd.SetTarget(pos); // Fake initial target used to trigger the update loop
	crowd.random = random;

	return imp;
}


//-------------------------------
void SubscribeToEvents()
//-------------------------------
{
	SubscribeToEvent("Update", "HandleUpdate");
	SubscribeToEvent("PostRenderUpdate", "HandlePostRenderUpdate");
	SubscribeToEvent("NavigationAreaRebuilt", "HandleNavigationAreaRebuilt");
	UnsubscribeFromEvent("SceneUpdate"); // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
}


//-------------------------------------------
void MoveCamera(float timeStep)
//-------------------------------------------
{
	// Do not move if the UI has a focused element (the console)
	if (ui.focusElement !is null)
		return;

	// Movement speed as world units per second
	const float MOVE_SPEED = 4.0f;

	// Read directional keys and move the camera scene node to the corresponding direction if they are pressed
	if (input.keyDown[KEY_UP]) cameraNode.Translate(Vector3(0.0f, 1.0f, 0.0f) * MOVE_SPEED * timeStep);
	if (input.keyDown[KEY_DOWN]) cameraNode.Translate(Vector3(0.0f, -1.0f, 0.0f) * MOVE_SPEED * timeStep);
	if (input.keyDown[KEY_LEFT]) cameraNode.Translate(Vector3(-1.0f, 0.0f, 0.0f) * MOVE_SPEED * timeStep);
	if (input.keyDown[KEY_RIGHT]) cameraNode.Translate(Vector3(1.0f, 0.0f, 0.0f) * MOVE_SPEED * timeStep);

	// Zoom with mouse wheel
	Camera@ camera = cameraNode.GetComponent("Camera");
	if (input.mouseMoveWheel != 0)
		camera.zoom = Clamp(camera.zoom + input.mouseMoveWheel * 0.1f, 0.1f, 10.0f);

	// Toggle debug geometry with space
	if (input.keyPress[KEY_SPACE])
	{
		drawDebug = !drawDebug;
		// In debug mode we display navMesh' triangulated shapes, which appear in black as they don't have a material assigned.
		// This demonstrates how we can render 3D objects in a 2D scene, by setting camera position below 0.
		camera.viewMask = drawDebug ? DEFAULT_VIEWMASK : 127;
	}
}


//---------------------------------------------------------------------------------------------------------
void HandleNavigationAreaRebuilt(StringHash eventType, VariantMap& eventData)
//---------------------------------------------------------------------------------------------------------
{
	// Repath each wanderer
	Node@[]@ imps = scene_.GetChildrenWithScript("Crowd");
	for (uint i = 0; i < imps.length; ++i)
	{
		Node@ imp = imps[i];
		Crowd@ crowd = cast<Crowd>(imp.GetScriptObject("Crowd"));
		if (crowd.path.length > 0)
		{
			Vector3 targetPos = crowd.path[crowd.path.length - 1];
			crowd.SetTarget(Vector2(targetPos.x, targetPos.y));
		}
	}
}


//--------------------------------------------------------------------------------------
void HandleUpdate(StringHash eventType, VariantMap& eventData)
//--------------------------------------------------------------------------------------
{
	// Move the camera, scale movement with time step
	MoveCamera(eventData["TimeStep"].GetFloat());

	// Set destination or Spawn a new imp with left mouse button
	if (input.mouseButtonPress[MOUSEB_LEFT])
	{
		Vector3 targetPos = renderer.viewports[0].ScreenToWorldPoint(input.mousePosition.x, input.mousePosition.y, 0.0f);
		Vector2 target2D = Vector2(targetPos.x, targetPos.y);

		if (input.qualifierDown[QUAL_SHIFT])
		{
			Crowd@ crowd = cast<Crowd>(impNode.GetScriptObject("Crowd"));
			crowd.SetTarget(target2D);
		}
		else
			SpawnImp(target2D);
	}

	// Add or remove objects with middle mouse button, then rebuild navigation mesh partially
	if (input.mouseButtonPress[MOUSEB_MIDDLE] || input.keyPress[KEY_O])
		AddOrRemoveObject();

	// Load/Save the scene
	if (input.keyPress[KEY_F5])
	{
		File saveFile(fileSystem.programDir + "Data/Scenes/TileMapNavigationDemo.xml", FILE_WRITE);
		scene_.SaveXML(saveFile);
	}
	if (input.keyPress[KEY_F7])
	{
		File loadFile(fileSystem.programDir + "Data/Scenes/TileMapNavigationDemo.xml", FILE_READ);
		scene_.LoadXML(loadFile);
		impNode = scene_.GetChild("Jack", true);
	}
}


//----------------------------------------------------------------------------------------------------
void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
//----------------------------------------------------------------------------------------------------
{
	if (!drawDebug)
		return;

	DebugRenderer@ debug = scene_.debugRenderer;

	scene_.physicsWorld2D.DrawDebugGeometry();

	// Draw nodes
	renderer.DrawDebugGeometry(true);

	Node@ tileMapNode = scene_.GetChild("TileMap", true);
	TileMap2D@ tileMap = tileMapNode.GetComponent("TileMap2D");
	tileMap.DrawDebugGeometry(debug, true);

	// Draw navMesh debug geometry
	NavigationMesh@ navMesh = tileMap.navMesh;
	navMesh.DrawDebugGeometry(debug, true);

	Crowd@ crowd = cast<Crowd>(impNode.GetScriptObject("Crowd"));
	Vector3[] path = crowd.path;
	uint length = path.length;
	if (length > 0)
	{
		// Visualize the current calculated path
		Vector3 endPos = path[length - 1];
		debug.AddBoundingBox(BoundingBox(endPos - Vector3(0.1f, 0.1f, 0.1f), endPos + Vector3(0.1f, 0.1f, 0.1f)), Color(1.0f, 1.0f, 1.0f));

		// Draw the path
		debug.AddLine(impNode.position, path[0], Color(1.0f, 1.0f, 1.0f));

		if (path.length > 1)
		{
			for (uint i = 0; i < path.length - 1; ++i)
				debug.AddLine(path[i], path[i + 1], Color(1.0f, 1.0f, 1.0f));
		}
	}
}


//----------------------------------
void AddOrRemoveObject()
//----------------------------------
{
	Node@ tileMapNode = scene_.GetChild("TileMap");
	TileMap2D@ tileMap = tileMapNode.GetComponent("TileMap2D");

	// Raycast and check if we hit an object located on last view mask
	Vector2 hitPos;
	Drawable@ hitDrawable;

	if (Raycast(250.0f, hitPos, hitDrawable))
		tileMap.RemoveObstacle(hitDrawable.node);
	else
	{
		/// Create a spheric obstacle and an imp as a child. Note that we could get the vertices from any tmx object.
		//Array<Vector2> points = {Vector2(0.16, -0.24), Vector2(0.226274, -0.16), Vector2(0.16, -0.08), Vector2(0, -0.0468626), Vector2(-0.16, -0.08), Vector2(-0.226274, -0.16), Vector2(-0.16, -0.24), Vector2(0, -0.273137), Vector2(0.16, -0.24)};
		//Node@ clone = impNode.Clone();
		//tileMap.AddObstacle(hitPos, points, clone);

		/// Using a tile object
		TileMapLayer2D@ layer = tileMap.GetLayer("Physics");
		TileMapObject2D@ obj = layer.GetObject("Statue");

		Node@ clone = scene_.CreateChild("NewObstacle");
		tileMap.AddObstacle(hitPos, obj, clone);
	}
}


//------------------------------------------------------------------------------------------------
bool Raycast(float maxDistance, Vector2& hitPos, Drawable@& hitDrawable)
//------------------------------------------------------------------------------------------------
{
	hitDrawable = null;

	IntVector2 pos = input.mousePosition;
	// Check the cursor is visible and there is no UI element in front of the cursor
	if (!input.mouseVisible || ui.GetElementAt(pos, true) !is null)
		return false;

	Camera@ camera = cameraNode.GetComponent("Camera");
	Ray cameraRay = camera.GetScreenRay(float(pos.x) / graphics.width, float(pos.y) / graphics.height);
	// Pick only geometry objects, not eg. zones or lights, only get the first (closest) hit
	// Note the convenience accessor to scene's Octree component
	RayQueryResult result = scene_.octree.RaycastSingle(cameraRay, RAY_TRIANGLE, maxDistance, DRAWABLE_GEOMETRY);
	if (result.drawable !is null && result.drawable.viewMask == 128)
	{
		hitPos = Vector2(result.position.x, result.position.y);
		hitDrawable = result.drawable;
		return true;
	}

	Vector3 mousePos = renderer.viewports[0].ScreenToWorldPoint(pos.x, pos.y, 0.0f);
	hitPos = Vector2(mousePos.x, mousePos.y);
	return false;
}


//-------------------
void CreateUI()
//-------------------
{
	// Construct new Text object, set string to display and font to use
	Text@ instructionText = ui.root.CreateChild("Text");
	instructionText.text =
		"Use WASD keys to move, Mouse Wheel or PageUp / PageDown to zoom\n"
		"LMB to set destination, MMB or O key to add or remove obstacles\n"
		"Space to toggle debug geometry";
	instructionText.SetFont(cache.GetResource("Font", "Fonts/Anonymous Pro.ttf"), 15);
	// The text has multiple rows. Center them in relation to each other
	instructionText.textAlignment = HA_CENTER;

	// Position the text relative to the screen center
	instructionText.SetAlignment(HA_CENTER, VA_CENTER);
	instructionText.SetPosition(0, ui.root.height / 4);
}

// Create XML patch instructions for screen joystick layout specific to this sample app
String patchInstructions =
		"<patch>" +
		"    <remove sel=\"/element/element[./attribute[@name='Name' and @value='Button0']]/attribute[@name='Is Visible']\" />" +
		"    <replace sel=\"/element/element[./attribute[@name='Name' and @value='Button0']]/element[./attribute[@name='Name' and @value='Label']]/attribute[@name='Text']/@value\">Zoom In</replace>" +
		"    <add sel=\"/element/element[./attribute[@name='Name' and @value='Button0']]\">" +
		"        <element type=\"Text\">" +
		"            <attribute name=\"Name\" value=\"KeyBinding\" />" +
		"            <attribute name=\"Text\" value=\"PAGEUP\" />" +
		"        </element>" +
		"    </add>" +
		"    <remove sel=\"/element/element[./attribute[@name='Name' and @value='Button1']]/attribute[@name='Is Visible']\" />" +
		"    <replace sel=\"/element/element[./attribute[@name='Name' and @value='Button1']]/element[./attribute[@name='Name' and @value='Label']]/attribute[@name='Text']/@value\">Zoom Out</replace>" +
		"    <add sel=\"/element/element[./attribute[@name='Name' and @value='Button1']]\">" +
		"        <element type=\"Text\">" +
		"            <attribute name=\"Name\" value=\"KeyBinding\" />" +
		"            <attribute name=\"Text\" value=\"PAGEDOWN\" />" +
		"        </element>" +
		"    </add>" +
		"</patch>";


//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// Crowd script object class
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

class Crowd : ScriptObject
{
	Vector3[] path;
	NavigationMesh@ navMesh;
	float speed = 1.0f;
	bool random = true;


//-------------------------------------------
void SetTarget(Vector2 targetPos)
//-------------------------------------------
{
	if (navMesh is null)
		return;

	// Calculate path from current to target position
	path = navMesh.FindPath(node.position, Vector3(targetPos, 0.0f));

	// Play run anim
	AnimatedSprite2D@ sprite = node.GetComponent("AnimatedSprite2D");
	if (sprite !is null)
		sprite.SetAnimation("run");
}

//----------------------------------------
void PostUpdate(float timeStep)
//----------------------------------------
{
	if (path.length == 0)
		return;

	Vector3 nextWaypoint = path[0]; // NB: path[0] is the next waypoint in order

	// Move to next waypoint. Check for not overshooting the target
	Vector3 pos = node.position;
	Vector3 dir = nextWaypoint - node.position;
	dir.z = 0.0f; // For layering purpose
	float distance = dir.length;
	float move = Min(speed * timeStep, distance);
	node.Translate(dir.Normalized() * move);

	// Flip character toward next waypoint
	AnimatedSprite2D@ sprite = node.GetComponent("AnimatedSprite2D");
	if (sprite !is null && ((dir.x > 0.0f && !sprite.flipX) || (dir.x < 0.0f && sprite.flipX)))
		sprite.flipX = !sprite.flipX;

	// Remove waypoint when reached. Play idle anim when destination is reached.
	if (distance < 0.01f)
	{
		path.Erase(0);
		if (path.length == 0)
		{
			if (random) // Set new random destination
			{
				Vector3 target = navMesh.GetRandomPoint();
				SetTarget(Vector2(target.x, target.y));
			}
			else
				sprite.SetAnimation("idle");
		}
	}

	// Update character z-order to ensure accurate occlusion
	Node@ tileMapNode = scene_.GetChild("TileMap", true);
	TileMap2D@ map = tileMapNode.GetComponent("TileMap2D");
	TileMapLayer2D@ layer = map.GetLayer(1);
	int x, y;

	if (layer.PositionToTileIndex(x, y, Vector2(pos.x, pos.y)))
	{
		AnimatedSprite2D@ animatedSprite = node.GetComponent("AnimatedSprite2D");
		animatedSprite.orderInLayer = layer.TileRenderOrder(x, y);
	}
}

}
