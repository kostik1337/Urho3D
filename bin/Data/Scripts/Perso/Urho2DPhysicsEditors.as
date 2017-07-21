// Urho2D Physics Body Editor example.
// This sample demonstrates:
//     - Creating a 2D scene from a Physics Body Editor json file
//     - Using a sprite sheet in combination with the json file
//     - Handling keyboard to move and zoom 2D camera
//     - Interacting with the rigid bodies (deletion)
//     - Displaying debug geometry for physics

#include "Scripts/Utilities/Sample.as"


void Start()
{
	SampleStart();
	CreateScene();
	CreateInstructions();
//	SampleInitMouseMode(MM_FREE);
	input.mouseVisible = true;
	SubscribeToEvents();
}

void CreateScene()
{
	// SCENE
	scene_ = Scene();
	scene_.CreateComponent("Octree");
	scene_.CreateComponent("DebugRenderer");
	PhysicsWorld2D@ physicsWorld = scene_.CreateComponent("PhysicsWorld2D");

	// CAMERA and VIEWPORT
	cameraNode = scene_.CreateChild("Camera");
	cameraNode.position = Vector3(5.12f, 3.0f, 0.0f);
	Camera@ camera = cameraNode.CreateComponent("Camera");
	camera.orthographic = true;
	camera.orthoSize = graphics.height * PIXEL_SIZE;
	camera.zoom = 1.1f * Min(graphics.width / 1280.0f, graphics.height / 800.0f); // Set zoom according to user's resolution to ensure full visibility (initial zoom (1.0) is set for full visibility at 1280x800 resolution)
	renderer.viewports[0] = Viewport(scene_, camera);

	// PhysicsEditor (plist)
	//PhysicsData2D@ data3 = scene_.CreateComponent("PhysicsData2D");
	//data3.physicsLoader = cache.GetResource("PhysicsLoader2D", "Assets/Urho2D/PhysicsEditorPlist/Cocos2d/Shapes.plist");
	//Array<Sprite2D@> sprites1 = {cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorPlist/Cocos2d/ground.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorPlist/Cocos2d/banana.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorPlist/Cocos2d/cherries.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorPlist/Cocos2d/crate.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorPlist/Cocos2d/orange.png")};
	//Array<Node@>@ nodes3 = data3.CreatePhysicalSprites(sprites1, true);
	//for (uint i = 0; i < nodes3.length; ++i)
	//{
		//nodes3[i].position = Vector3(Random(-7.0f, 7.0f), Random(10.0f, 20.0f), 0.0f);
		//if (i ==0)
		//{
			//nodes3[i].position = Vector3(0.0f, 0.0f, 0.0f);
			//nodes3[i].SetScale(4.0f); // Ground
		//}
	//}

	// PhysicsEditor (xml)
//	PhysicsLoader2D@ loader1 = cache.GetResource("PhysicsLoader2D", "Assets/Urho2D/PhysicsEditorXml/shapes.xml");
//	PhysicsData2D@ data1 = scene_.CreateComponent("PhysicsData2D");
//	data1.physicsLoader = loader1;
//	Array<Sprite2D@> sprites2 = {cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorXml/drink.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorXml/hamburger.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorXml/hotdog.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorXml/icecream.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorXml/icecream2.png"), cache.GetResource("Sprite2D", "Assets/Urho2D/PhysicsEditorXml/icecream3.png")};
//	Array<Node@>@ nodes1 = data1.CreatePhysicalSprites(sprites2);
//	for (uint i = 0; i < nodes1.length; ++i)
//	{
//		Node@ n = nodes1[i];
//		n.position = Vector3(Random(-7.0f, 7.0f), Random(10.0f, 20.0f), 0.0f);
//		n.SetScale(1.5f);
//	}

	// In this sample we will use a sprite sheet instead of the individual images set in the json file to demonstrate this feature.
	// This requires that json and xml atlas names match.
	SpriteSheet2D@ spriteSheet = cache.GetResource("SpriteSheet2D", "Assets/Urho2D/Tilesets/Staggered/S1_Atlas.xml");
	Array<String> spriteNames = spriteSheet.names;

	// Get the Physics Body Editor data (from json file)
	PhysicsData2D@ data = scene_.CreateComponent("PhysicsData2D");
	data.physicsLoader = cache.GetResource("PhysicsLoader2D", "Assets/Urho2D/Tilesets/Staggered/S1.json");

	// Load a simple tile map for setting a basic scene
	Node@ tileMapNode = scene_.CreateChild("TileMapScene");
	TileMap2D@ tileMap = tileMapNode.CreateComponent("TileMap2D");
	tileMap.tmxFile = cache.GetResource("TmxFile2D", "Assets/Urho2D/Tilesets/Staggered/Constraints.tmx");

	// Add physics to the scene from the json file (in real-life we would use Tiled objects instead)
	Node@ walls = data.CreatePhysicalSprite("Walls");
	walls.position = Vector3(0.0f, -0.02f, 0.0f);
	walls.SetScale(10.25f); // Scale collision shape to match tile map size.

	// Create 40 different sprites from the sprite sheet, with rigid body and collision shapes
	for (uint i = 0; i < data.numDefs - 1; ++i)
	{
		String spriteName = spriteNames[i];
		Node@ n = data.CreatePhysicalSprite(spriteName, spriteSheet.GetSprite(spriteName));
		if (n !is null)
		{
			n.position = Vector3(Random(2.0f, 8.5f), Random(8.0f, 50.0f), 0.0f);
			n.SetScale(1.1f);
			Node@ n2 = n.Clone(); // Duplicate node
			n2.position = Vector3(Random(2.0f, 8.5f), Random(50.0f, 100.0f), 0.0f);
		}
	}
}

void CreateInstructions()
{
	// Construct new Text object, set string to display and font to use
	Text@ instructionText = ui.root.CreateChild("Text");
	instructionText.text = "Use directional keys and mouse to move, PageUp PageDown to zoom.\nLMB or touch to remove a sprite, Space to toggle debug geometry.";
	instructionText.SetFont(cache.GetResource("Font", "Fonts/Anonymous Pro.ttf"), 15);

	// Position the text relative to the screen center
	instructionText.SetAlignment(HA_CENTER, VA_TOP);
	instructionText.SetPosition(0, 40);
}

void MoveCamera(float timeStep)
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

void SubscribeToEvents()
{
	SubscribeToEvent("Update", "HandleUpdate");
	SubscribeToEvent("PostRenderUpdate", "HandlePostRenderUpdate");

	if (GetPlatform() == "Android" || GetPlatform() == "iOS" || input.touchEmulation)
		SubscribeToEvent("TouchBegin", "HandleTouch");
	else
		SubscribeToEvent("MouseButtonDown", "HandleMouseClick");

	UnsubscribeFromEvent("SceneUpdate"); // Unsubscribe the SceneUpdate event from base class to prevent camera pitch and yaw in 2D sample
}

void HandleUpdate(StringHash eventType, VariantMap& eventData)
{
	// Move the camera, scale movement with time step
	MoveCamera(eventData["TimeStep"].GetFloat());

	// Toggle debug geometry with space
	if (input.keyPress[KEY_SPACE])
		drawDebug = !drawDebug;

	// Load/Save the scene
	if (input.keyPress[KEY_F5])
	{
		File saveFile(fileSystem.programDir + "Data/Scenes/PhysicsEditorsDemo.xml", FILE_WRITE);
		scene_.SaveXML(saveFile);
	}
	if (input.keyPress[KEY_F7])
	{
		File loadFile(fileSystem.programDir + "Data/Scenes/PhysicsEditorsDemo.xml", FILE_READ);
		scene_.LoadXML(loadFile);
	}
}

void HandlePostRenderUpdate(StringHash eventType, VariantMap& eventData)
{
	if (drawDebug)
		scene_.physicsWorld2D.DrawDebugGeometry();
}

void HandleMouseClick(StringHash eventType, VariantMap& eventData)
{
	RemoveBodyAtPos(input.mousePosition.x, input.mousePosition.y);
}

void HandleTouch(StringHash eventType, VariantMap& eventData)
{
	RemoveBodyAtPos(eventData["X"].GetInt(), eventData["Y"].GetInt());
}

void RemoveBodyAtPos(int x, int y)
{
	RigidBody2D@ body = scene_.physicsWorld2D.GetRigidBody(x, y);
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
