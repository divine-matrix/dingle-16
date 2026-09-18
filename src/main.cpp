#include "assembler.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

using namespace std;

class File {
public:
  string fileContents;
  string fileName;
};

class FileViewer {
public:
  vector<File> fileList = {};
  int selectedFile = 0;
  void draw() {
    ImGui::Begin("File Viewer");

    if (fileList.size() == 0) {

      // 1. Define the size and padding
      ImVec2 box_size = ImVec2(800, 600.0f);
      float padding = 10.0f;

      // 2. Get screen positions for the frame
      ImVec2 p_min = ImGui::GetCursorScreenPos();
      p_min.x += padding;
      p_min.y += padding;
      ImVec2 p_max = ImVec2(p_min.x + box_size.x, p_min.y + box_size.y);

      // 3. Advance the ImGui layout cursor
      ImGui::Dummy(
          ImVec2(box_size.x + (padding * 2.0f), box_size.y + (padding * 2.0f)));

      // 4. Draw the empty frame border
      ImU32 frame_color = ImGui::GetColorU32(ImGuiCol_Border);
      ImGui::GetWindowDrawList()->AddRect(p_min, p_max, frame_color, 4.0f);

      // 5. Center and draw the text
      const char *text = "No files currently open.";
      ImVec2 text_size = ImGui::CalcTextSize(text);

      // Calculate the center of the box, then subtract half the text size
      ImVec2 text_pos;
      text_pos.x = p_min.x + (box_size.x - text_size.x) * 0.5f;
      text_pos.y = p_min.y + (box_size.y - text_size.y) * 0.5f;

      // Render the text using the draw list
      ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
      ImGui::GetWindowDrawList()->AddText(text_pos, text_color, text);
    } else {
      if (ImGui::BeginTabBar("FileViewerTabBar")) {
      }
      ImGui::EndTabBar();
    }
  }
};

// DELETE: Variables for demo window

bool showUseFileLoadToLoadFileText = false;

// Variables for File Viewer

vector<FileViewer> fileViewersCurrentOpen;
static bool showFileViewer = true;

// Variables for File -> Open Recent

static bool showOpenRecentWindow = false;
static char openRecentSearchBarBuffer[128] = "Search here";

// Track the actual index of the selected file within the global array
static int openRecentSelectedFile = -1;
static string openRecentRecentFiles[10] = {
    "poo.dingle",       "fibonacci.dingle", "mario.dingle",     "poo.dingle",
    "fibonacci.dingle", "poo.dingle",       "fibonacci.dingle", "poo.dingle",
    "fibonacci.dingle", "poop.dingle"};

void drawOpenRecentWindow() {}

bool fuzzyMatch(const string &query, const string &target) {
  if (query.empty() || query == "Search here")
    return true;

  size_t queryIdx = 0;
  size_t targetIdx = 0;

  while (queryIdx < query.size() && targetIdx < target.size()) {
    if (tolower(static_cast<unsigned char>(query[queryIdx])) ==
        tolower(static_cast<unsigned char>(target[targetIdx]))) {
      queryIdx++;
    }
    targetIdx++;
  }

  return queryIdx == query.size();
}


int main() {
  if (!glfwInit())
    return 1;

  // --- FIX FOR MACOS OPENGL CRASH ---
  // Request OpenGL 3.3 Core Profile
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Required on macOS
  // ----------------------------------

  // Create window with graphics context
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "dingle-16 assembler v1", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // Enable vsync

  // Setup Dear ImGui context
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();

  // Setup Platform/Renderer backends
  ImGui_ImplGlfw_InitForOpenGL(window, true);

  // --- FIX FOR MACOS SHADER VERSION ---
  // Change "#version 130" to "#version 150" (OpenGL 3.3 Core shader string)
  ImGui_ImplOpenGL3_Init("#version 150");
  // ------------------------------------
  // Main loop
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGuiIO &io = ImGui::GetIO();

    // Example using GLFW
    int w = 1280, h = 720;
    glfwGetFramebufferSize(window, &w, &h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    ImGui::NewFrame();

    // Must go above ImGui::Begin
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
        }
        if (ImGui::MenuItem("Open")) {
        }
        if (ImGui::MenuItem("Open Recent")) {
          showOpenRecentWindow = true;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::Begin("DINGLE-16 ASSEMBLER");

    ImGui::Text("No file currently running");
    if (ImGui::Button("Load File")) {
      showUseFileLoadToLoadFileText = true;
    }
    if (showUseFileLoadToLoadFileText) {
      ImGui::Text("Use File -> Load to load a file for assembling.");
    }
    ImGui::End();

    // Open Recent window

    if (showOpenRecentWindow) {
      ImGui::Begin("Open Recent", &showOpenRecentWindow);

      // Simple click handling to clear default hint text
      if (ImGui::InputText("##SearchBar", openRecentSearchBarBuffer,
                           IM_ARRAYSIZE(openRecentSearchBarBuffer))) {
        // Input processed dynamically via fuzzyMatch
      }
      if (ImGui::IsItemActivated() &&
          strcmp(openRecentSearchBarBuffer, "Search here") == 0) {
        openRecentSearchBarBuffer[0] = '\0';
      }

      ImGui::Separator();

      if (ImGui::BeginListBox("Recent Files", ImVec2(-FLT_MIN, 150))) {
        string currentQuery(openRecentSearchBarBuffer);

        for (int i = 0; i < IM_ARRAYSIZE(openRecentRecentFiles); i++) {
          // Filter items dynamically on the fly
          if (!fuzzyMatch(currentQuery, openRecentRecentFiles[i])) {
            continue;
          }

          ImGui::PushID(i);

          const bool isSelected = (openRecentSelectedFile == i);
          if (ImGui::Selectable(openRecentRecentFiles[i].c_str(), isSelected)) {
            openRecentSelectedFile = i;
          }

          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }

          ImGui::PopID();
        }
        ImGui::EndListBox();
      }

      if (ImGui::Button("Open")) {
      }

      ImGui::End();
    }

    ImGui::Render();
    int display_w, display_h;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();

  return 0;
}
