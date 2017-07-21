// Urho2D tile map example.
// This sample demonstrates:
//     - Creating a 2D scene with tile map
//     - Displaying the scene using the Renderer subsystem
//     - Handling keyboard to move and zoom 2D camera
//     - Creating sprites and Box2D constraints from tile map objects
//     - Optionally removing the tile map if you're only interested in the constraints

#include "Scripts/Utilities/Sample.as"

Node@ pickedNode;
bool removeMap = true;

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
	drawDebug = true; // Enable debug geometry by default
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
	tileMap.tmxFile = cache.GetResource("TmxFile2D", "Urho2D/Constraints.tmx");

	// Display Text3D flags
	CreateFlags();

	// Set camera's position
	TileMapInfo2D@ info = tileMap.info;
	cameraNode.position = Vector3(info.mapWidth * 0.5f, info.mapHeight * 0.5f, 0.0f);

	// Detach constraints and remove tile map
	if (removeMap)
		tileMap.DetachConstraints(true);
}


//-----------------------
void CreateFlags()
//-----------------------
{
	CreateFlag("ConstraintDistance2D", 0.03f, 7.2f);
	CreateFlag("ConstraintFriction2D", 6.45f, 4.8f);
	CreateFlag("ConstraintGear2D", 0.03f, 2.35f);
	CreateFlag("ConstraintWheel2Ds compound", 3.25f, 2.35f);
	CreateFlag("ConstraintMotor2D", 9.65f, 2.35f);
	CreateFlag("ConstraintMouse2D", 6.45f, 2.35f);
	CreateFlag("ConstraintPrismatic2D", 9.65f, 7.2f);
	CreateFlag("ConstraintPulley2D", 6.45f, 7.2f);
	CreateFlag("ConstraintRevolute2D", 3.25f, 7.2f);
	CreateFlag("ConstraintRope2D", 0.03f, 4.8f);
	CreateFlag("ConstraintWeld2D", 3.25f, 4.8f);
	CreateFlag("ConstraintWheel2D",  9.65f, 4.8f);
}


//--------------------------------------------------------------------
void CreateFlag(const String&in text, float x, float y)
//--------------------------------------------------------------------
{
    Node@ flagNode = scene_.CreateChild("Flag");
    flagNode.position = Vector3(x, y, 0.0f);
    Text3D@ flag3D = flagNode.CreateComponent("Text3D"); // We use Text3D in order to make the text affected by zoom (so that it sticks to 2D)
    flag3D.text = text;
    flag3D.SetFont(cache.GetResource("Font", "Fonts/Anonymous Pro.ttf"), 15);
}


//-------------------------------
void CreateInstructions()
//-------------------------------
{
	// Construct new Text object, set string to display and font to use
	Text@ instructionText = ui.root.CreateChild("Text");
	instructionText.text = "Use WASD keys to move, Use PageUp PageDown to zoom.\n LMB and mouse to grab and move objects\n Space to toggle debug geometry and joints, F5 to save scene";
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
		File saveFile(fileSystem.programDir + "Data/Scenes/TmcConstraintsDemo.xml", FILE_WRITE);
		scene_.SaveXML(saveFile);
	}
	if (input.keyPress[KEY_F7])
	{
		File loadFile(fileSystem.programDir + "Data/Scenes/TmcConstraintsDemo.xml", FILE_READ);
		scene_.LoadXML(loadFile);
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
		CollisionPolygon2D@ shape = pickedNode.GetComponent("CollisionPolygon2D");
		StaticSprite2D@ staticSprite = pickedNode.GetComponent("StaticSprite2D");
		if (staticSprite !is null)
			staticSprite.color = Color(1.0f, 0.0f, 0.0f, 1.0f); // Temporary modify color of the picked sprite

		// Create a ConstraintMouse2D - Temporaryly apply this constraint to the pickedNode to allow grasping and moving with the mouse
		ConstraintMouse2D@ constraintMouse = pickedNode.CreateComponent("ConstraintMouse2D");
		constraintMouse.target = GetMousePositionXY();
		constraintMouse.maxForce = 1000 * rigidBody.mass;
		constraintMouse.collideConnected = true;
		constraintMouse.otherBody = rigidBody;
		constraintMouse.dampingRatio = 0.0f;
	}
	SubscribeToEvent("MouseMove", "HandleMouseMove");
	SubscribeToEvent("MouseButtonUp", "HandleMouseButtonUp");
}

void HandleMouseButtonUp(StringHash eventType, VariantMap& eventData)
{
	if (pickedNode !is null)
	{
		StaticSprite2D@ staticSprite = pickedNode.GetComponent("StaticSprite2D");
		if (staticSprite !is null)
			staticSprite.color = Color(1.0f, 1.0f, 1.0f, 1.0f); // Restore picked sprite color

		pickedNode.RemoveComponent("ConstraintMouse2D"); // Remove temporary constraint
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

		// Draw physics
		scene_.physicsWorld2D.DrawDebugGeometry();

		// Draw picked node transforms
		if (pickedNode !is null)
			debug.AddNode(pickedNode, 1.0, false);
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
