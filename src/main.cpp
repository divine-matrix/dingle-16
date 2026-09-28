// NOTE: Standard C++ library headers MUST come first
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

// NOTE: C and OS headers
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// NOTE: Project and ImGui headers
#include "assembler.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h"
#include "tinyfiledialogs.h"
#include <GLFW/glfw3.h>

// NOTE: Memory editor included after standard types are fully loaded
// #include "imgui_memory_editor.h"

using namespace std;

string gstatus = "idle";
int currentId = 0;
ImVec2 nextWindowPosition = ImVec2(10, 10);

static bool showSaveAsDialog = false;

vector<string> split(const string &input, char delimiter) {
  vector<string> result;
  stringstream ss(input);
  string token;

  while (getline(ss, token, delimiter)) {
    result.push_back(token);
  }

  return result;
}

struct ErrorData {
  string name = "";
  string debugInfo = "";
  int code = 1;
};

enum Error {
  FileOpenError = 100,
  FileSaveError = 101,
  UnknownCommand = 200,
  UnsupportedFeature = 201,
  FileNotOpenError = 110,
};

ErrorData getErrorData(Error error) {
  ErrorData data;
  switch (error) {
  case FileOpenError:
    data.name = "File Open Error";
    data.debugInfo = "An error occured while opening a file.";
    data.code = 100;
    break;
  case FileSaveError:
    data.name = "File Save Error";
    data.debugInfo = "An error occured while saving a file to the disk.";
    data.code = 101;
    break;
  case UnknownCommand:
    data.name = "Unknown Command Error";
    data.debugInfo =
        "Malformed or unknown command was entered into the Command Console.";
    data.code = 200;
    break;
  case UnsupportedFeature:
    data.name = "Unsupported Feature";
    data.debugInfo = "That feature has not been added yet, keep a lookout for "
                     "future updates on the YouTube channel or the Github.";
    data.code = 201;
    break;
  case FileNotOpenError:
    data.name = "File Not Open Error";
    data.debugInfo = "Open a file to use this function";
    data.code = 110;
    break;
  }
  return data;
}

class ErrorWindow {
public:
  Error currentError;
  int windowID = 0;
  bool is_open = true;
  ImVec2 position;

  ErrorWindow(Error cError) {
    currentError = cError;
    position = nextWindowPosition;
    nextWindowPosition.x += 30;
    nextWindowPosition.y += 30;
  }

  void draw() {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.4f, 0.2f, 0.2f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,
                          ImVec4(0.5f, 0.1f, 0.1f, 1.0f));

    ImGui::SetNextWindowPos(position);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoMove;

    std::string windowTitle = "Error: " + getErrorData(currentError).name +
                              "###ErrorWin_" + std::to_string(windowID);
    ImGui::Begin(windowTitle.c_str(), nullptr, flags);

    ImGui::Text("Error: %s", getErrorData(currentError).name.c_str());
    ImGui::Text("Details: %s", getErrorData(currentError).debugInfo.c_str());
    ImGui::Text("Error Code: %i", getErrorData(currentError).code);

    if (ImGui::Button("Close Window")) {
      is_open = false;
    }

    ImGui::End();
    ImGui::PopStyleColor(3);
  }
};

vector<ErrorWindow> errorWindows;

void showError(Error err) {
  ErrorWindow newWindow(err);
  newWindow.windowID = currentId++;
  errorWindows.push_back(newWindow);
}

// NOTE: Renamed from File to AppFile to prevent namespace shadowing with system
// headers
class AppFile {
public:
  string fileContents;
  string filePath;
  string fileName;
  int fileStatus; // NOTE: 0 is saved, 1 is not saved, 2 is no path.

  AppFile(string path, string contents) {
    fileContents = contents;
    filePath = path;
    fileName = split(path, '/').back();
    fileStatus = 0;
  }

  AppFile() {
    fileContents = "";
    filePath = "";
    fileName = "Untitled";
    fileStatus = 2;
  }

  void save() {
    if (!filePath.empty()) {
      saveAtPath(filePath);
    }
  }

  void saveAtPath(string path) {
    ofstream saveFile(path);
    if (!saveFile.is_open()) {
      showError(FileSaveError);
    } else {
      saveFile << fileContents;
      filePath = path;
      fileName = split(path, '/').back();
      fileStatus = 0;
    }
    saveFile.close();
  }
};

class FileViewer {
private:
  string status;

public:
  vector<AppFile> fileList = {};
  int selectedFile = 0;
  bool requestSelect = false;

  FileViewer() {}

  void loadFile(string filePath) {
    if (filePath.empty())
      return;

    ifstream targetFile(filePath);
    if (!targetFile.is_open()) {
      cout << "Failed to open file: " << filePath << endl;
      showError(FileOpenError);
    } else {
      string content((std::istreambuf_iterator<char>(targetFile)),
                     std::istreambuf_iterator<char>());

      selectedFile = (int)fileList.size();
      requestSelect = true;
      fileList.push_back(AppFile(filePath, content));
    }
  }

  void draw(int viewerIndex) {
    ImGui::PushID(viewerIndex);

    std::string panelTitle =
        "File Viewer###FileViewerPanel_" + std::to_string(viewerIndex);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;

    if (showSaveAsDialog) {
      flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin(panelTitle.c_str(), nullptr, flags);

    if (ImGui::IsWindowFocused() && !fileList.empty()) {
      status = "editing";
    }

    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
          fileList.push_back(AppFile());
          selectedFile = (int)fileList.size() - 1;
          requestSelect = true;
        }
        if (ImGui::MenuItem("Save")) {
          if (fileList.empty()) {
            showError(FileNotOpenError);
          } else if (fileList[selectedFile].fileStatus == 2 ||
                     fileList[selectedFile].filePath.empty()) {
            showSaveAsDialog = true;
          } else {
            fileList[selectedFile].save();
            status = "Saved " + fileList[selectedFile].fileName + " to disk.";
          }
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    if (fileList.empty()) {
      ImVec2 box_size = ImVec2(800, 600.0f);
      float padding = 10.0f;

      ImVec2 p_min = ImGui::GetCursorScreenPos();
      p_min.x += padding;
      p_min.y += padding;
      ImVec2 p_max = ImVec2(p_min.x + box_size.x, p_min.y + box_size.y);

      ImGui::Dummy(
          ImVec2(box_size.x + (padding * 2.0f), box_size.y + (padding * 2.0f)));

      ImU32 frame_color = ImGui::GetColorU32(ImGuiCol_Border);
      ImGui::GetWindowDrawList()->AddRect(p_min, p_max, frame_color, 4.0f);

      const char *text = "No files currently open.";
      ImVec2 text_size = ImGui::CalcTextSize(text);

      ImVec2 text_pos;
      text_pos.x = p_min.x + (box_size.x - text_size.x) * 0.5f;
      text_pos.y = p_min.y + (box_size.y - text_size.y) * 0.5f;

      ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
      ImGui::GetWindowDrawList()->AddText(text_pos, text_color, text);
    } else {
      std::string tabbarId = "FileViewerTabBar_" + std::to_string(viewerIndex);
      if (ImGui::BeginTabBar(tabbarId.c_str())) {
        for (size_t i = 0; i < fileList.size(); i++) {
          ImGui::PushID((int)i);
          ImGuiTabItemFlags tabFlags = ImGuiTabItemFlags_None;
          if (requestSelect && (int)i == selectedFile)
            tabFlags |= ImGuiTabItemFlags_SetSelected;
          if (fileList[i].fileStatus == 1)
            tabFlags |= ImGuiTabItemFlags_UnsavedDocument;

          if (ImGui::BeginTabItem(fileList[i].fileName.c_str(), nullptr,
                                  tabFlags)) {
            selectedFile = (int)i;
            if (ImGui::InputTextMultiline("##fileEditor",
                                          &fileList[i].fileContents,
                                          ImVec2(-FLT_MIN, -FLT_MIN))) {
              if (fileList[i].fileStatus == 0)
                fileList[i].fileStatus = 1;
            }
            ImGui::EndTabItem();
          }
          ImGui::PopID();
        }
        requestSelect = false;
        ImGui::EndTabBar(); // only call when BeginTabBar returned true
      }
    }
    ImGui::Text("%s", status.c_str());
    ImGui::End();
    ImGui::PopID();
  }
};

// ---- Globals that main() relies on (were missing) ----
vector<FileViewer> fileViewersCurrentOpen(1);
bool showFileViewer = true;
bool showOpenRecentWindow = false;
bool showCommandConsole = false;
string openRecentSearchBarBuffer = "Search here";
array<string, 10> openRecentRecentFiles;
int openRecentSelectedFile = -1;

void addFileToRecentList(const string &path) {
  if (path.empty())
    return;
  // Remove existing occurrence, then push to the front
  size_t last = openRecentRecentFiles.size() - 1;
  for (size_t i = 0; i < openRecentRecentFiles.size(); i++) {
    if (openRecentRecentFiles[i] == path) {
      last = i;
      break;
    }
  }
  for (size_t i = last; i > 0; i--)
    openRecentRecentFiles[i] = openRecentRecentFiles[i - 1];
  openRecentRecentFiles[0] = path;
}

static string commandBuffer;
static vector<string> commandLog;

void drawCommandBar() {
  ImGui::Begin("Command Console", &showCommandConsole);
  for (const string &line : commandLog)
    ImGui::TextUnformatted(line.c_str());
  ImGui::Separator();
  if (ImGui::InputText("##cmd", &commandBuffer,
                       ImGuiInputTextFlags_EnterReturnsTrue)) {
    vector<string> parts = split(commandBuffer, ' ');
    if (!parts.empty()) {
      commandLog.push_back("> " + commandBuffer);
      if (parts[0] == "help") {
        commandLog.push_back("Commands: help, status, clear");
      } else if (parts[0] == "status") {
        commandLog.push_back("Status: " + gstatus);
      } else if (parts[0] == "clear") {
        commandLog.clear();
      } else {
        showError(UnknownCommand);
      }
    }
    commandBuffer.clear();
    ImGui::SetKeyboardFocusHere(-1);
  }
  ImGui::End();
}

static string saveAsPath;

void drawSaveAsDialog(AppFile &currentFile) {
  if (!showSaveAsDialog)
    return;

  ImGui::Begin("Save as", &showSaveAsDialog, ImGuiWindowFlags_NoCollapse);

  ImGui::Text("%s", saveAsPath.c_str());
  if (ImGui::Button("Browse...")) {
    const char *filterPatterns[] = {"*.dingle", "*.txt", "*"};
    const char *selectedPath = tinyfd_saveFileDialog(
        "Save As", "untitled.dingle", 3, filterPatterns, "Dingle Files");

    if (selectedPath != nullptr) {
      saveAsPath = selectedPath;
    }
  }

  if (ImGui::Button("Save")) {
    if (saveAsPath.empty()) {
      showError(FileSaveError);
    } else {
      currentFile.saveAtPath(saveAsPath);
      saveAsPath = "";
      showSaveAsDialog = false;
    }
  }

  ImGui::End();
}

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

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window =
      glfwCreateWindow(1280, 720, "dingle-16 assembler v1", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");

  ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                 ImGuiDockNodeFlags_PassthruCentralNode);
    errorWindows.erase(
        std::remove_if(errorWindows.begin(), errorWindows.end(),
                       [](const ErrorWindow &w) { return !w.is_open; }),
        errorWindows.end());

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
          fileViewersCurrentOpen[0].fileList.push_back(AppFile());
          fileViewersCurrentOpen[0].selectedFile =
              (int)fileViewersCurrentOpen[0].fileList.size() - 1;
          fileViewersCurrentOpen[0].requestSelect = true;
        }
        if (ImGui::MenuItem("Open...")) {

          const char *filterPatterns[] = {"*.dingle", "*.txt", "*"};
          const char *selectedPath =
              tinyfd_openFileDialog("Open Dingle File", // Title
                                    ".",                // Default path
                                    3,              // Number of filter patterns
                                    filterPatterns, // Filter array
                                    "Dingle Files", // Description
                                    0 // Allow multiple (0 = false)
              );

          // Check if the user actually picked a file (didn't click cancel)
          if (selectedPath != nullptr) {
            std::string pathStr = selectedPath;
            fileViewersCurrentOpen[0].loadFile(pathStr);
            addFileToRecentList(pathStr);
          }
        }
        if (ImGui::MenuItem("Open Recent")) {
          showOpenRecentWindow = true;
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Debug")) {
        if (ImGui::MenuItem("Command Console")) {
          showCommandConsole = !showCommandConsole;
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    ImGui::Begin("DINGLE-16 ASSEMBLER");
    ImGui::Text("No file currently running");
    if (ImGui::Button("Load File")) {

      const char *filterPatterns[] = {"*"};
      const char *selectedPath =
          tinyfd_openFileDialog("Load File Context", // Title
                                ".",                 // Default path
                                1,              // Number of filter patterns
                                filterPatterns, // Filter array
                                "All Files",    // Description
                                0               // Allow multiple (0 = false)
          );

      if (selectedPath != nullptr) {
        std::string pathStr = selectedPath;
        fileViewersCurrentOpen[0].loadFile(pathStr);
        addFileToRecentList(pathStr);
      }
    }
    ImGui::End();

    if (showOpenRecentWindow) {
      ImGui::Begin("Open Recent", &showOpenRecentWindow);

      ImGui::InputText("##SearchBar", &openRecentSearchBarBuffer);

      if (ImGui::IsItemActivated() &&
          openRecentSearchBarBuffer == "Search here") {
        openRecentSearchBarBuffer.clear();
      }

      ImGui::Separator();

      if (ImGui::BeginListBox("Recent Files", ImVec2(-FLT_MIN, 150))) {
        for (int i = 0; i < 10; i++) {
          if (!fuzzyMatch(openRecentSearchBarBuffer,
                          openRecentRecentFiles[i]) ||
              openRecentRecentFiles[i].empty()) {
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
      if (ImGui::Button("Open") && openRecentSelectedFile >= 0 &&
          openRecentSelectedFile < 10) {
        string recentPath = openRecentRecentFiles[openRecentSelectedFile];
        if (!recentPath.empty()) {
          fileViewersCurrentOpen[0].loadFile(recentPath);
          addFileToRecentList(recentPath);
          showOpenRecentWindow = false;
        }
      }
      ImGui::End();
    }

    if (showFileViewer) {
      for (size_t i = 0; i < fileViewersCurrentOpen.size(); i++) {
        fileViewersCurrentOpen[i].draw((int)i);
      }

      // NOTE: Call Save As dialog if active and a file exists
      if (showSaveAsDialog && !fileViewersCurrentOpen.empty() &&
          !fileViewersCurrentOpen[0].fileList.empty()) {
        int activeIdx = fileViewersCurrentOpen[0].selectedFile;
        drawSaveAsDialog(fileViewersCurrentOpen[0].fileList[activeIdx]);
      }
    }
    if (showCommandConsole) {
      drawCommandBar();
    }

    // FIX: Render tracking utilizing references so changes propagate back
    // safely
    for (size_t i = 0; i < errorWindows.size(); i++) {
      errorWindows[i].draw();
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
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
