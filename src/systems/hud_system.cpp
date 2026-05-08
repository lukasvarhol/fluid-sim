#include "hud_system.h"

void DrawHUD(Particles &particles, SimulationControl& simulationControl) {

  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  if (simulationControl.isShowHUD) {
    ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_Once);
    ImGui::SetNextWindowSize(ImVec2(300, 300), ImGuiCond_FirstUseEver);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoBackground |
      ImGuiWindowFlags_AlwaysAutoResize;
    ImGui::Begin("##hud", nullptr, flags);

    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    if (ImGui::CollapsingHeader("Appearance")) {
      if (simulationControl.isPaused) {
	bool changed = false;
	int nPending = particles.numParticles;
	changed |= ImGui::SliderInt("Particles", &nPending, 10, 60000);
	changed |= ImGui::SliderFloat("Spacing", &initSpacing,  0.01f, 0.2f);
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
	ImGui::SliderFloat("Pos X", &initOffsetX, -1.0f, 1.0f);
	ImGui::SliderFloat("Pos Y", &initOffsetY, -1.0f, 1.0f);
	ImGui::SliderFloat("Spacing", &initSpacing, 0.001f, 0.1f);
	ImGui::EndDisabled();
      }	  
      ImGui::SliderFloat("Particle Size", &radiusLogical, 1.0f, 50.0f);
    }

    if (ImGui::CollapsingHeader("Physics")) {
      ImGui::SliderFloat("Smoothing Radius", &smoothingRadius, 0.01f, 1.0f);
      ImGui::SliderFloat("Relaxation Factor", &relaxation, 1000.0f, 50000.0f);
      ImGui::SliderFloat("Gravity", &gravity, -10.0f, 10.0f);
      ImGui::SliderFloat("Scorr Coefficient", &scorrCoefficient, 0.000001f, 0.00005f);
      ImGui::SliderFloat("Viscosity", &xsphC, 0.01f, 1.0f);
      ImGui::SliderFloat("Vorticity", &vorticityEpsilon, 0.0f, 20000.0f);
      ImGui::SliderInt("Solvert Iterations", &numIterations, 1, 20);
    }
            
    if (ImGui::CollapsingHeader("Mouse")) {
      ImGui::SliderFloat("Push Strength", &pushStrength, -100.0f, 0.0f);
      ImGui::SliderFloat("Pull Strength", &pullStrength, 0.0, 100.0f);
      ImGui::SliderFloat("Push Radius", &pushRadius, 0.0f, 1.0f);
      ImGui::SliderFloat("Pull Radius", &pullRadius, 0.0f, 1.0f); 
    }
    ImGui::End();
  }

}
