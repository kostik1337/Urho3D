// Urho2D tile map example.
// This sample demonstrates:
//     - Creating a 2D scene with tile map
//     - Displaying the scene using the Renderer subsystem
//     - Handling keyboard to move and zoom 2D camera
//     - Interacting with the tile map

#include "Scripts/Utilities/Sample.as"

Node@ pickedNode;

//--------------
void Start()
//--------------
{
	SampleStart();
	CreateScene();
//	CreateInstructions();
	SampleInitMouseMode(MM_FREE);
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
	physicsWorld.drawJoint = true; // Display the joints
///	physicsWorld.updateEnabled = false;

	// CAMERA
	cameraNode = Node();
	Camera@ camera = cameraNode.CreateComponent("Camera");
	camera.orthographic = true;
	camera.orthoSize = graphics.height * PIXEL_SIZE;
	camera.zoom = 1.0f * Min(graphics.width / 1280.0f, graphics.height / 800.0f); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.0) is set for full visibility at 1280x800 resolution)
	renderer.viewports[0] = Viewport(scene_, camera);

	// TILE MAP
	Node@ tileMapNode = scene_.CreateChild("TileMap");
	TileMap2D@ tileMap = tileMapNode.CreateComponent("TileMap2D");
	tileMap.tmxFile = cache.GetResource("TmxFile2D", "Assets/Urho2D/Tilesets/Staggered/Collision2.tmx");

//tileMap.GetLayer(0).visible = false;
//tileMap.GetLayer(1).visible = false;

Node@ zero = scene_.CreateChild("Zero");
zero.Roll(180.0f);

	// Set camera's position
	TileMapInfo2D@ info = tileMap.info;
	cameraNode.position = Vector3(info.mapWidth * 0.5f, info.mapHeight * 0.5f, -100.0f);

	// Mouse cursor helper tile
	Node@ tileCursor = scene_.CreateChild("TileCursor");
	tileCursor.scale2D = Vector2(info.tileWidth / PIXEL_SIZE, info.tileHeight / PIXEL_SIZE) / Vector2(32, 32);

	StaticSprite2D@ cursorSprite = tileCursor.CreateComponent("StaticSprite2D");
	Sprite2D@ sprite =  cache.GetResource("Sprite2D", "Assets/Urho2D/Tilesets/Ghost.png");
	if (info.orientation == O_ORTHOGONAL)
		sprite.rectangle = IntRect(0, 0, 32, 32);
	else if (info.orientation == O_ISOMETRIC || info.orientation == O_STAGGERED)
		sprite.rectangle = IntRect(32, 0, 64, 32);
	else if (info.orientation == O_HEXAGONAL)
		sprite.rectangle = IntRect(64, 0, 96, 32);

	cursorSprite.useHotSpot = true;
	cursorSprite.hotSpot = Vector2(0.0f, 0.0f); // Move sprite to tile center (Tiled behavior)
	cursorSprite.hotSpot = Vector2(0.0f, -0.5f); // In this tileset, tiles have some 'depth', increase offset for visual matching

	cursorSprite.layer = (tileMap.numLayers) * 10 + 1; // Draw on top of the map
	cursorSprite.color = Color(1.0f, 1.0f, 1.0f, 0.25f);
	cursorSprite.sprite = sprite;
}


//-------------------------------
void CreateInstructions()
//-------------------------------
{
	// Construct new Text object, set string to display and font to use
	Text@ instructionText = ui.root.CreateChild("Text");
	instructionText.text = "Use directional keys and mouse to move, Use PageUp PageDown to zoom.\n LMB to remove a tile, RMB to swap grass and water";
	instructionText.SetFont(cache.GetResource("Font", "Fonts/Anonymous Pro.ttf"), 15);

	// Position the text relative to the screen center
	instructionText.SetAlignment(HA_CENTER, VA_CENTER);
	instructionText.SetPosition(0, ui.root.height / 4);
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

	// Zoom in
	if (input.keyDown[KEY_PAGEUP])
		camera.zoom = camera.zoom * 1.01f;
	// Zoom out
	if (input.keyDown[KEY_PAGEDOWN])
		camera.zoom = camera.zoom * 0.99f;
}


//-------------------------------
void SubscribeToEvents()
//-------------------------------
{
	SubscribeToEvent("Update", "HandleUpdate");
	SubscribeToEvent("PostRenderUpdate", "HandlePostRenderUpdate");
	SubscribeToEvent("MouseButtonDown", "HandleMouseButtonDown");
	UnsubscribeFromEvent("SceneUpdate"); // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
}


//--------------------------------------------------------------------------------------
void HandleUpdate(StringHash eventType, VariantMap& eventData)
//--------------------------------------------------------------------------------------
{
	// Move the camera, scale movement with time step
	MoveCamera(eventData["TimeStep"].GetFloat());

	// Toggle debug geometry with space
	if (input.keyPress[KEY_SPACE]) drawDebug = !drawDebug;

	// Load/Save the scene
	if (input.keyPress[KEY_F5])
	{
		File saveFile(fileSystem.programDir + "Data/Scenes/TileMapDemo.xml", FILE_WRITE);
		scene_.SaveXML(saveFile);
	}
	if (input.keyPress[KEY_F7])
	{
		File loadFile(fileSystem.programDir + "Data/Scenes/TileMapDemo.xml", FILE_READ);
		scene_.LoadXML(loadFile);
	}

	// Draw tile cursor helper
	Node@ tileCursor = scene_.GetChild("TileCursor", true);
	if (tileCursor !is null)
	{
		Node@ tileMapNode = scene_.GetChild("TileMap", true);
		if (tileMapNode is null)
			return;
		TileMap2D@ map = tileMapNode.GetComponent("TileMap2D");
		TileMapLayer2D@ layer = map.GetLayer(0);

		int x, y;
		if (layer.PositionToTileIndex(x, y, GetMousePositionXY()))
			tileCursor.position = Vector3(map.TileIndexToPosition(x, y));
	}
}


//-----------------------------------------------------------------------------------------------------
void HandleMouseButtonDown(StringHash eventType, VariantMap& eventData)
//-----------------------------------------------------------------------------------------------------
{
	RigidBody2D@ rigidBody = scene_.physicsWorld2D.GetRigidBody(input.mousePosition.x, input.mousePosition.y, M_MAX_UNSIGNED); // Raycast for RigidBody2Ds to pick
	if (rigidBody !is null)
	{
		pickedNode = rigidBody.node;
///		log.Info(pickedNode.name);
		CollisionPolygon2D@ shape = pickedNode.GetComponent("CollisionPolygon2D");
		StaticSprite2D@ staticSprite = pickedNode.GetComponent("StaticSprite2D");
		if (staticSprite !is null)
			staticSprite.color = Color(1.0f, 0.0f, 0.0f, 1.0f); // Temporary modify color of the picked sprite
	}
	SubscribeToEvent("MouseMove", "HandleMouseMove");
	SubscribeToEvent("MouseButtonUp", "HandleMouseButtonUp");

	Node@ tileMapNode = scene_.GetChild("TileMap", true);
	if (tileMapNode is null)
		return;
	TileMap2D@ map = tileMapNode.GetComponent("TileMap2D");

	Vector2 pos = GetMousePositionXY();

	int x, y;
	for (uint i = 0; i < map.numLayers; ++i)
	{
		TileMapLayer2D@ layer = map.GetLayer(i);
		if (layer.PositionToTileIndex(x, y, pos))
		{
			Node@ n = layer.GetTileNode(x, y);

			// If nothing on this layer, try next one
			if (n is null)
				continue;

			// Remove tile node
			n.Remove(); //n.Roll(15.0f);			//sprite.sprite = null; // 'Remove' sprite /// sprite.blendMode = BLEND_INVDESTALPHA to simply hide the sprite
		}
	}
}

void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData)
{
	if (pickedNode !is null)
	{
		StaticSprite2D@ staticSprite = pickedNode.GetComponent("StaticSprite2D");
		if (staticSprite !is null)
			staticSprite.color = Color(1.0f, 1.0f, 1.0f, 1.0f); // Restore picked sprite color
		pickedNode = null;
	}
	UnsubscribeFromEvent("MouseMove");
	UnsubscribeFromEvent("MouseButtonUp");
}

void HandleMouseMove(StringHash eventType, VariantMap& eventData)
{
	if (pickedNode !is null)
	{
		ConstraintMouse2D@ constraintMouse = pickedNode.GetComponent("ConstraintMouse2D");
		constraintMouse.target = GetMousePositionXY();
	}
}

//-------------------------------------
Vector2 GetMousePositionXY()
//-------------------------------------
{
	Vector3 worldPoint = renderer.viewports[0].ScreenToWorldPoint(input.mousePosition.x, input.mousePosition.y, 0.0f);
	return Vector2(worldPoint.x, worldPoint.y);
}


//----------------------------------------------------------------------------------------------------
void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
//----------------------------------------------------------------------------------------------------
{
	if (drawDebug)
	{
		DebugRenderer@ debug = scene_.debugRenderer;

		scene_.physicsWorld2D.DrawDebugGeometry();

		// Draw nodes
		renderer.DrawDebugGeometry(true);

debug.AddNode(scene_.GetChild("Zero", true), 1.0, false);

		Node@ tileMapNode = scene_.GetChild("TileMap", true);
		if (tileMapNode is null)
			return;
		TileMap2D@ tileMap = tileMapNode.GetComponent("TileMap2D");
		tileMap.DrawDebugGeometry(debug, true);

//		tileMap.GetLayer(0).DrawDebugGeometry(debug, true);

if (pickedNode !is null)
	debug.AddNode(pickedNode, 1.0, false);

		TileMapInfo2D@ info = tileMap.info;

		for (int x = 0; x < info.width; ++x)
		{
			for (int y = 0; y < info.height; ++y)
			{
				Node@ n = tileMap.GetLayer(0).GetTileNode(x, y);
//				debug.AddNode(n, 1.0, false);
//				for (uint j = 0; j < n.numComponents; ++j)
//					n.components[j].DrawDebugGeometry(debug, false);
			}
		}
	}
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
