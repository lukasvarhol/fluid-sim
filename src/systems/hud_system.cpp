#include "hud_system.h"
#include "objects3d/object_builder.h"

void DrawHUD(Particles& particles, SimulationControl& simulationControl,
             EditorState& editorState, float dt) {

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  if (simulationControl.isShowHUD) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(320, 600), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar  |
      ImGuiWindowFlags_NoResize    |
      ImGuiWindowFlags_NoBackground|
      ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("##hud", nullptr, flags);

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("[ESC] HUD  [Space] Pause  [Ctrl+R] Reset");
    ImGui::Separator();

    // Status toast — fades out after statusTimer reaches 0
    if (editorState.statusTimer > 0.0f) {
      editorState.statusTimer -= dt;
      if (editorState.statusTimer < 0.0f) editorState.statusTimer = 0.0f;
      ImGui::TextColored({1.0f, 1.0f, 0.2f, 1.0f},
			 "%s", editorState.statusMsg.c_str());
      ImGui::Separator();
    }

    // -----------------------------------------------------------------------
    // Objects Mode toggle (HUD-only, no keyboard shortcut)
    // -----------------------------------------------------------------------
    {
      bool prevEnabled = editorState.objectsModeEnabled;
      ImGui::Checkbox("Enable Objects Mode", &editorState.objectsModeEnabled);
      ImGui::TextDisabled("Object editing keys work only when Objects Mode is enabled.");
      if (prevEnabled && !editorState.objectsModeEnabled && editorState.previewActive)
	cancelPreview(editorState.grid, editorState);
    }
    ImGui::Separator();

    // -----------------------------------------------------------------------
    // Cell Editor
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Cell Editor", ImGuiTreeNodeFlags_DefaultOpen)) {
      GridState&   g  = editorState.grid;
      CellFeature& cf = g.GetCell(g.selX, g.selY, g.selZ);

      static const char* featureNames[] = {
	"Empty", "Ramp", "Straight Channel", "L Channel" 
      };
      static const char* orientNames[] = {
	"North", "East", "South", "West"
      };

      ImGui::Text("Cell:   (%d, %d, %d)", g.selX, g.selY, g.selZ);
      ImGui::Text("Placed: %s", featureNames[(int)cf.feature]);

      if (editorState.previewActive) {
	ImGui::Separator();
	ImGui::TextColored({1.0f, 1.0f, 0.3f, 1.0f},
			   "Preview: %s facing %s",
			   featureNames[(int)editorState.previewCell.feature],
			   orientNames[(int)editorState.previewCell.facing]);
	ImGui::TextDisabled("[Enter]=Place  [Esc]=Cancel");
      } else {
	ImGui::Text("Facing: %s", orientNames[(int)cf.facing]);
      }
    }

    // -----------------------------------------------------------------------
    // Build Controls
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Build Controls")) {
      ImGui::TextDisabled("Objects Mode must be ON for editing keys");
      ImGui::Separator();
      ImGui::TextDisabled("F / Shift+F      cycle feature");
      ImGui::TextDisabled("Q                rotate CW");
      ImGui::TextDisabled("E                rotate CCW");
      ImGui::TextDisabled("V / Shift+V      cycle variant (Ramp)");
      ImGui::TextDisabled("Enter            place");
      ImGui::TextDisabled("Esc              cancel preview / toggle HUD");
      ImGui::TextDisabled("Del / Backspace  clear cell");
      ImGui::Separator();
      ImGui::TextDisabled("Arrows           move X/Z");
      ImGui::TextDisabled("T / G            move Y up/down");
      ImGui::TextDisabled("PgUp / PgDn      move Y up/down");
      ImGui::Separator();
      ImGui::TextDisabled("Ctrl+R           reset simulation");
      ImGui::TextDisabled("Space            pause / unpause");
      ImGui::TextDisabled("Middle drag      orbit camera");
      ImGui::TextDisabled("Scroll           zoom");
    }

    // -----------------------------------------------------------------------
    // View Options
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("View Options")) {
      ImGui::Checkbox("Show Grid",              &editorState.showGrid);
      ImGui::Checkbox("Show Selected Cell",     &editorState.showSelectedCell);
      ImGui::Checkbox("Show Occupied Outlines", &editorState.showOccupiedOutlines);
    }

    // -----------------------------------------------------------------------
    // Scene Controls
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Scene Controls")) {
      if (ImGui::Button("Load Tutorial Machine"))
	loadDefaultScene(editorState);
      ImGui::SameLine();
      if (ImGui::Button("Clear"))
	clearScene(editorState);
      ImGui::Checkbox("Reset Scene on R", &editorState.resetObjectsOnR);
      //ImGui::Text("Collision objects: %d", (int)editorState.objects.size()); //todo: fix this
    }

    // -----------------------------------------------------------------------
    // Appearance
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Appearance")) {
      if (simulationControl.isPaused) {
	bool changed = false;
	int nPending = particles.numParticles;
	changed |= ImGui::SliderInt("Particles", &nPending, 10, 60000);
	changed |= ImGui::SliderFloat("Spacing",  &initSpacing,  0.01f, 0.2f);
	changed |= ImGui::SliderFloat("Offset X", &initOffsetX, -0.5f, 0.5f);
	changed |= ImGui::SliderFloat("Offset Y", &initOffsetY, -0.5f, 0.5f);
	changed |= ImGui::SliderFloat("Offset Z", &initOffsetZ, -0.5f, 0.5f);
	if (changed)
	  particles.ResizeParticles(nPending, smoothingRadius,
				    initSpacing, initOffsetX, initOffsetY, initOffsetZ);
      } else {
	int nDisplay = particles.numParticles;
	ImGui::BeginDisabled();
	ImGui::SliderInt("Particles", &nDisplay, 1000, 60000);
	ImGui::SliderFloat("Pos X",   &initOffsetX, -1.0f, 1.0f);
	ImGui::SliderFloat("Pos Y",   &initOffsetY, -1.0f, 1.0f);
	ImGui::SliderFloat("Spacing", &initSpacing,  0.001f, 0.1f);
	ImGui::EndDisabled();
      }
      ImGui::SliderFloat("Particle Size", &radiusLogical, 1.0f, 50.0f);
    }

    // -----------------------------------------------------------------------
    // Physics
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Physics")) {
      ImGui::SliderFloat("Smoothing Radius",   &smoothingRadius,   0.01f, 1.0f);
      ImGui::SliderFloat("Relaxation Factor",  &relaxation,     1000.0f, 50000.0f);
      ImGui::SliderFloat("Gravity",            &gravity,          -10.0f, 10.0f);
      ImGui::SliderFloat("Scorr Coefficient",  &scorrCoefficient, 0.000001f, 0.00005f);
      ImGui::SliderFloat("Viscosity",          &xsphC,             0.01f, 1.0f);
      ImGui::SliderFloat("Vorticity",          &vorticityEpsilon,  0.0f, 20000.0f);
      ImGui::SliderInt("Solver Iterations",    &numIterations,     1, 20);
    }

    // -----------------------------------------------------------------------
    // Mouse
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Mouse")) {
      ImGui::SliderFloat("Push Strength", &pushStrength, -100.0f, 0.0f);
      ImGui::SliderFloat("Pull Strength", &pullStrength,   0.0f, 100.0f);
      ImGui::SliderFloat("Push Radius",   &pushRadius,     0.0f, 1.0f);
      ImGui::SliderFloat("Pull Radius",   &pullRadius,     0.0f, 1.0f);
    }

    // -----------------------------------------------------------------------
    // Trickler
    // -----------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Trickler")) {
      bool prevMode = tricklerMode;
      ImGui::Checkbox("Trickler Mode", &tricklerMode);
      if (tricklerMode && !prevMode) {
	particles.ResetTrickler();
      } else if (!tricklerMode && prevMode) {
	particles.activeParticles = particles.numParticles;
      }
      if (tricklerMode) {
	ImGui::SliderFloat("Spawn Rate (p/s)", &tricklerSpawnRate, 0.1f, 1000.0f);
	ImGui::SliderFloat("Spread", &tricklerSpread, 0.0f, 0.2f);
	ImGui::SliderFloat("Origin X", &tricklerOriginX, -1.0f, 1.0f);
	ImGui::SliderFloat("Origin Y", &tricklerOriginY, -1.0f, 1.0f);
	ImGui::SliderFloat("Origin Z", &tricklerOriginZ, -1.0f, 1.0f);
	ImGui::Text("Active: %d / %d", particles.activeParticles, particles.numParticles);
      }
    }

    ImGui::End();
  }

}
