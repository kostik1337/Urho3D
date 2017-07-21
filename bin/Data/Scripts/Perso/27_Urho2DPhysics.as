// Urho2D physics sample.
// This sample demonstrates:
//     - Creating both static and moving 2D physics objects to a scene
//     - Interacting with rigid bodies

#include "Scripts/Utilities/Sample.as"


//--------------
void Start()
//--------------
{
	SampleStart();
	CreateScene();
	CreateInstructions();
//	SampleInitMouseMode(MM_FREE);
	SubscribeToEvents();
	input.mouseVisible = true;
}


//------------------------
void CreateScene()
//------------------------
{
	// SCENE
	scene_ = Scene();
	scene_.CreateComponent("Octree");
	scene_.CreateComponent("PhysicsWorld2D");
	scene_.CreateComponent("DebugRenderer");

	// CAMERA
	cameraNode = scene_.CreateChild("Camera");
	Camera@ camera = cameraNode.CreateComponent("Camera");
	camera.orthographic = true;
	camera.orthoSize = graphics.height * PIXEL_SIZE;
	camera.zoom = 1.9f * Min(graphics.width / 1280.0f, graphics.height / 800.0f); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.2) is set for full visibility at 1280x800 resolution)
	renderer.viewports[0] = Viewport(scene_, camera);

	// SPRITES
	Sprite2D@ boxSprite = cache.GetResource("Sprite2D", "Urho2D/Box.png");
	Sprite2D@ ballSprite = cache.GetResource("Sprite2D", "Urho2D/Ball.png");

	// GROUND
	Node@ groundNode = scene_.CreateChild("Ground");
	groundNode.position = Vector3(0.0f, -1.8f, 0.0f); // Top=0 / Bottom=1
	groundNode.scale = Vector3(50.0f, 1.0f, 0.0f);
	RigidBody2D@ groundBody = groundNode.CreateComponent("RigidBody2D");
	StaticSprite2D@ groundSprite = groundNode.CreateComponent("StaticSprite2D");
	groundSprite.sprite = boxSprite;
	CollisionBox2D@ groundShape = groundNode.CreateComponent("CollisionBox2D");
	groundShape.size = Vector2(0.32f, 0.32f);
	groundShape.friction = 0.5f;

	// WALLS
	Node@ leftWall = groundNode.Clone();
	leftWall.scale = Vector3(0.5f, 10.0f, 0.0f);
	leftWall.position = Vector3(-3.0f, -0.1f, 0.0f);

	Node@ rightWall = leftWall.Clone();
	rightWall.position = Vector3(3.0f, -0.1f, 0.0f);

	// BALLS & BOXES
	for (uint i = 0; i < 150; ++i)
	{
		Node@ node  = scene_.CreateChild("BallOrCube");
		node.position = Vector3(Random(-0.1f, 0.1f), 5.0f + i * 0.4f, 0.0f);
		StaticSprite2D@ staticSprite = node.CreateComponent("StaticSprite2D");
		RigidBody2D@ body = node.CreateComponent("RigidBody2D");
		body.bodyType = BT_DYNAMIC; // Other types are BT_STATIC & BT_KINEMATIC

		CollisionShape2D@ shape;

		if (i % 2 == 0) // Alternate between ball and box
		{
			staticSprite.sprite = boxSprite;
			shape = node.CreateComponent("CollisionBox2D"); // Create box shape
			cast<CollisionBox2D>(shape).size = Vector2(0.32f, 0.32f);
		}
		else
		{
			staticSprite.sprite = ballSprite;
			shape = node.CreateComponent("CollisionCircle2D"); // Create circle shape
			cast<CollisionCircle2D>(shape).radius = 0.16f;
		}
		shape.density = 1.0f;
		shape.friction = 0.1f;
		shape.restitution = 0.5f;
	}
}


//-------------------------------
void CreateInstructions()
//-------------------------------
{
	// Construct new Text object, set string to display and font to use
	Text@ instructionText = ui.root.CreateChild("Text");
	instructionText.text = "Use directional keys to move, Use PageUp PageDown to zoom.\n Click to remove items, including walls and ground";
	instructionText.SetFont(cache.GetResource("Font", "Fonts/Anonymous Pro.ttf"), 15);
	instructionText.textAlignment = HA_CENTER;

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

	const float MOVE_SPEED = 4.0f; // Movement speed as world units per second

	// Read directional keys and move the camera scene node to the corresponding direction if they are pressed
	if (input.keyDown[KEY_UP]) cameraNode.Translate(Vector3(0.0f, 1.0f, 0.0f) * MOVE_SPEED * timeStep);
	if (input.keyDown[KEY_DOWN]) cameraNode.Translate(Vector3(0.0f, -1.0f, 0.0f) * MOVE_SPEED * timeStep);
	if (input.keyDown[KEY_LEFT]) cameraNode.Translate(Vector3(-1.0f, 0.0f, 0.0f) * MOVE_SPEED * timeStep);
	if (input.keyDown[KEY_RIGHT]) cameraNode.Translate(Vector3(1.0f, 0.0f, 0.0f) * MOVE_SPEED * timeStep);

	// Zoom with mouse wheel
	if (input.mouseMoveWheel != 0)
	{
		Camera@ camera = cameraNode.GetComponent("Camera");
		camera.zoom = Clamp(camera.zoom + input.mouseMoveWheel * 0.1f, 0.1f, 10.0f);
	}

	// Zoom in
	if (input.keyDown[KEY_PAGEUP])
	{
		Camera@ camera = cameraNode.GetComponent("Camera");
		camera.zoom = camera.zoom * 1.01f;
	}

	// Zoom out
	if (input.keyDown[KEY_PAGEDOWN])
	{
		Camera@ camera = cameraNode.GetComponent("Camera");
		camera.zoom = camera.zoom * 0.99f;
	}

	// Load/Save the scene
	if (input.keyPress[KEY_F5])
	{
		File saveFile(fileSystem.programDir + "Data/Scenes/2DPhysics.xml", FILE_WRITE);
		scene_.SaveXML(saveFile);
	}
	if (input.keyPress[KEY_F7])
	{
		File loadFile(fileSystem.programDir + "Data/Scenes/2DPhysics.xml", FILE_READ);
		scene_.LoadXML(loadFile);
	}
}


//-------------------------------
void SubscribeToEvents()
//-------------------------------
{
	SubscribeToEvent("Update", "HandleUpdate");
	SubscribeToEvent("PostRenderUpdate", "HandlePostRenderUpdate");

	if (GetPlatform() == "Android" || GetPlatform() == "iOS" || input.touchEmulation)
		SubscribeToEvent("TouchBegin", "HandleTouch");
	else
		SubscribeToEvent("MouseButtonDown", "HandleMouseClick");
}


//--------------------------------------------------------------------------------------
void HandleUpdate(StringHash eventType, VariantMap& eventData)
//--------------------------------------------------------------------------------------
{
	MoveCamera(eventData["TimeStep"].GetFloat()); // Move camera
	if (input.keyPress[KEY_SPACE]) drawDebug = !drawDebug; // Toggle debug geometry with space
}


//----------------------------------------------------------------------------------------------------
void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
//----------------------------------------------------------------------------------------------------
{
	if (drawDebug) scene_.physicsWorld2D.DrawDebugGeometry();
}


//-------------------------------------------------------------------------------------------
void HandleMouseClick(StringHash eventType, VariantMap& eventData)
//-------------------------------------------------------------------------------------------
{
	RemoveBodyAtPos(input.mousePosition.x, input.mousePosition.y);
}


//------------------------------------------------------------------------------------
void HandleTouch(StringHash eventType, VariantMap& eventData)
//------------------------------------------------------------------------------------
{
	RemoveBodyAtPos(eventData["X"].GetInt(), eventData["Y"].GetInt());
}


//---------------------------------------------
void RemoveBodyAtPos(int x, int y)
//---------------------------------------------
{
	RigidBody2D@ body = scene_.physicsWorld2D.GetRigidBody(x, y); // We could skip walls and ground by using collision layers and passing a collision mask as third argument
	if (body !is null)
		body.node.Remove();
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
