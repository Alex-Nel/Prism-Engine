#define PRISM_EDITOR
#include "Prism.hpp"




class RotationScript : public Prism::Behavior
{
public:
    float speed = 2.5f;
    

    RotationScript()
    {
        this->script_class_name = "RotationScript";
        ExposeVariable("Speed", &speed);
    }

    void OnUpdate()
    {
        this->entity.GetTransform()->RotateEuler(Prism::Vector3{0.0f, speed * Prism::Time::DeltaTime(), 0.0f});
        this->entity.GetTransform()->RotateEuler(Prism::Vector3{0.0f, 0.0f, speed * Prism::Time::DeltaTime()});
    }

    void OnTriggerEnter(Prism::Entity other)
    {
        Debug_Log("Trigger has been entered");
    }
    void OnTriggerStay(Prism::Entity other)
    {
        Debug_Log("Trigger is actively occupied");
    }
    void OnTriggerExit(Prism::Entity other)
    {
        Debug_Log("Trigger has been exited");
    }
};





static bool is_simulating = false;
static Prism::Entity selected_entity;
Prism::Scene editor_scene;
static float current_fps = 0;
static uint32_t window_width = 1600;
static uint32_t window_height = 900;



void DrawEditorUI(void* userdata)
{
    window_width = Prism::Platform::GetWindowWidth();
    window_height = Prism::Platform::GetWindowHeight();


    // Top Menu Bar / Toolbar
    if (Prism::UI::BeginWindow("Toolbar", 0, 0, window_width, 50, Prism::WindowFlags::Background | Prism::WindowFlags::NoScrollbar))
    {
        std::string fpsCounter = "Current FPS: " + std::to_string(current_fps);
        Prism::UI::LayoutRowStatic(30, 100, 4);
        if (Prism::UI::Button(is_simulating ? "Stop" : "Play")) {
            is_simulating = !is_simulating;
            Prism::Engine::SetSimulationMode(is_simulating);
        }
        if (Prism::UI::Button("Save Scene")) {
            editor_scene.Save("SampleScene");
            Debug_Log("Save Scene clicked");
        }
        if (Prism::UI::Button("Load Scene")) {
            editor_scene.Load("SampleScene");
            Debug_Log("Load Scene clicked");
        }
        Prism::UI::Label(fpsCounter);
    }
    Prism::UI::EndWindow();


    static Prism::Entity selected_entity;

    // Scene Hierarchy Panel
    if (Prism::UI::BeginWindow("Hierarchy", "Scene Hierarchy", 0, 50, 250, window_height-50, Prism::WindowFlags::Bordered | Prism::WindowFlags::Title))
    {
        Prism::UI::LayoutRowDynamic(25, 1);

        std::vector<Prism::Entity> entities = editor_scene.GetAllEntities();
        for (Prism::Entity& e : entities)
        {
            if (e.CompareTag("EditorOnly"))
                continue;

            bool is_selected = (selected_entity.id == e.id && selected_entity.IsValid());
            if (Prism::UI::Selectable(e.GetName(), &is_selected))
            {
                if (is_selected) selected_entity = e;
                else selected_entity = Prism::Entity(); // deselect
            }
        }
    }
    Prism::UI::EndWindow();



    // Inspector Panel
    if (Prism::UI::BeginWindow("Inspector", "Properties", window_width-400, 50, 400, window_height-50, Prism::WindowFlags::Bordered | Prism::WindowFlags::Title))
    {
        if (selected_entity.IsValid())
        {
            Prism::UI::LayoutRowDynamic(30, 1);
            Prism::UI::Label(selected_entity.GetName());
            Prism::UI::LayoutRowDynamic(2, 1);
            Prism::UI::Separator(Prism::Color::White());

            Prism::Transform* t = selected_entity.GetTransform();
            if (t != nullptr)
            {
                Prism::UI::LayoutRowDynamic(20, 1);
                Prism::UI::Label("Transform");

                Prism::Vector3 pos = t->GetLocalPosition();
                Prism::UI::LayoutRowDynamic(20, 3);
                Prism::UI::PropertyFloat("X", -1000.0f, &pos.x, 1000.0f, 0.1f, 0.1f);
                Prism::UI::PropertyFloat("Y", -1000.0f, &pos.y, 1000.0f, 0.1f, 0.1f);
                Prism::UI::PropertyFloat("Z", -1000.0f, &pos.z, 1000.0f, 0.1f, 0.1f);
                t->SetLocalPosition(pos);
            }
        }
    }
    Prism::UI::EndWindow();
}





int main()
{
	// Initialize the engine with an Editor specific title
    if (!Prism::Engine::Init("Prism Editor", window_width, window_height, 75)) {
        return -1;
    }


    // Set the Editor UI to draw during window resize/drag events
    Prism::Engine::SetModalCallback(DrawEditorUI, nullptr);


    // Set up a basic scene (in the future, this would load from a file)
    editor_scene = Prism::Scene::Create();

    // Enable SSAO
    Prism::RendererSettings s = Prism::Engine::GetRendererSettings();
    s.enable_ssao = !s.enable_ssao;
    Prism::Engine::SetRendererSettings(s);

    // Create an Editor Camera
    Prism::Entity editor_camera = editor_scene.CreateEntity("Editor Camera");
    editor_camera.SetTag("EditorOnly");
    editor_camera.AddCamera(90.0f);
    editor_camera.GetTransform()->SetLocalPosition(Prism::Vector3{0.0f, 0.0f, 5.0f});

    // Create an Editor Default Light
    Prism::Entity editor_light = editor_scene.CreateEntity("Editor Light");
    editor_light.SetTag("EditorOnly");
    editor_light.AddLight(Prism::LightType::Directional, Prism::Color::White());
    editor_light.GetTransform()->SetLocalRotationEuler(Prism::Vector3{-45.0f, 45.0f, 0.0f});
    editor_light.GetLight()->SetIntensity(1.0f);
    editor_light.GetLight()->SetAmbientStrength(0.1f);
    editor_light.GetLight()->SetCastsShadows(true);


    // Set up environment map
    Prism::EnvironmentMap env_map = Prism::AssetManager::LoadEnvironmentMapFromSkybox("SampleSkybox", "assets/textures/SampleLeft.png", "assets/textures/SampleRight.png", "assets/textures/SampleUp.png", "assets/textures/SampleDown.png", "assets/textures/SampleFront.png", "assets/textures/SampleBack.png");
    editor_scene.SetEnvironmentMap(env_map);





    //
    // ----- Test Objects -----
    //

    // ----- Rotating Cube -----
    Prism::Mesh cube = Prism::AssetManager::GetBuiltinCube();
    Prism::Material default_mat = Prism::AssetManager::CreateMaterial(Prism::AssetManager::CreateSolidColorTexture("Red", Prism::Color::Red()));
    Prism::Entity box3 = editor_scene.CreateEntity("Box3");
    box3.GetTransform()->local_position = Prism::Vector3{0, 0, 0};
    box3.AddMeshRenderer(cube, default_mat);
    RotationScript* rot = box3.AddScript<RotationScript>();
    Prism::ScriptRegistry::Register<RotationScript>("RotationScript");


    // ----- Static Model -----
    Prism::Model dragonStan = Prism::AssetManager::LoadModel("Dragon Stan", "assets/SampleObjects/StanfordDragon.stl");
    Prism::Entity staticModel = editor_scene.CreateEntity("Static Model");
    staticModel.GetTransform()->SetLocalPosition(Prism::Vector3{2, 0, 0});
    staticModel.GetTransform()->SetLocalScale(Prism::Vector3{0.02f, 0.02f, 0.02f});
    staticModel.GetTransform()->SetLocalRotationEuler(Prism::Vector3{-90, 0, 0});
    staticModel.AddModel(dragonStan);





    // Start with simulation paused (Edit Mode)
    Prism::Engine::SetSimulationMode(is_simulating);
    

    // Setup basic input
    Prism::Input::BindKeyPressed(Prism::KEYCODE_P, []() {
        is_simulating = !is_simulating;
        Prism::Engine::SetSimulationMode(is_simulating);
        Debug_Log("Simulation Mode: %s", is_simulating ? "ON" : "OFF");
    });


    // Editor camera state
    float cam_pitch = 0.0f;
    float cam_yaw = 0.0f;


    while (Prism::Engine::IsRunning())
    {
        current_fps = 1.0f / Prism::Time::DeltaTime();
        Prism::Input::DispatchCallbacks();

        Prism::Engine::Update(editor_scene);
        window_width = Prism::Platform::GetWindowWidth();
        window_height = Prism::Platform::GetWindowHeight();



        // --- Editor Light Logic ---
        bool has_real_light = false;
        for (Prism::Entity& e : editor_scene.GetAllEntities())
        {
            if (e.id != editor_light.id && e.HasComponent<Prism::LightComponent>())
            {
                if (e.GetLight()->type == Prism::LightType::Directional)
                {
                    has_real_light = true;
                    break;
                }
            }
        }
        editor_light.GetLight()->SetActive(!has_real_light);



        // --- Editor Camera Controls ---
        if (Prism::Input::IsMouseButtonDown(Prism::MOUSE_BUTTON_RIGHT))
        {
            if (!Prism::Engine::IsMouseCaptured())
                Prism::Engine::CaptureMouse();

            float dt = Prism::Time::DeltaTime();
            float move_speed = 5.0f * dt;
            float look_speed = 0.002f;

            // Look
            cam_yaw -= Prism::Input::GetMouseDeltaX() * look_speed;
            cam_pitch -= Prism::Input::GetMouseDeltaY() * look_speed;
            if (cam_pitch > 89.0f) cam_pitch = 89.0f;
            if (cam_pitch < -89.0f) cam_pitch = -89.0f;

            Prism::Transform* cam_t = editor_camera.GetTransform();
            cam_t->SetLocalRotationEuler(Prism::Vector3{cam_pitch, cam_yaw, 0.0f});

            // Move
            Prism::Vector3 forward = cam_t->GetForwardVector();
            Prism::Vector3 right = cam_t->GetRightVector();
            Prism::Vector3 up = Prism::Vector3{0.0f, 1.0f, 0.0f};

            if (Prism::Input::IsKeyDown(Prism::KEYCODE_LEFTSHIFT)) move_speed = 10.0f * dt;
            else move_speed = 5.0f * dt;

            if (Prism::Input::IsKeyDown(Prism::KEYCODE_W)) cam_t->Translate(forward * move_speed);
            if (Prism::Input::IsKeyDown(Prism::KEYCODE_S)) cam_t->Translate(forward * -move_speed);
            if (Prism::Input::IsKeyDown(Prism::KEYCODE_A)) cam_t->Translate(right * -move_speed);
            if (Prism::Input::IsKeyDown(Prism::KEYCODE_D)) cam_t->Translate(right * move_speed);
            if (Prism::Input::IsKeyDown(Prism::KEYCODE_E)) cam_t->Translate(up * move_speed);
            if (Prism::Input::IsKeyDown(Prism::KEYCODE_Q)) cam_t->Translate(up * -move_speed);
        }
        else
        {
            if (Prism::Engine::IsMouseCaptured())
                Prism::Engine::ReleaseMouse();
        }


        // --- Editor UI Rendering ---
        DrawEditorUI(nullptr);


        // 3. Render the 3D scene and the UI over it
        Prism::Engine::Render(editor_scene);
    }


    // Clean up
    Prism::Engine::Shutdown();
    return 0;
}