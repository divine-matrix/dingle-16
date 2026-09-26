
#include "assembler.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"    // Adjusted path notation for vcpkg
#include "imgui_impl_opengl3.h" // Adjusted path notation for vcpkg
#include "imgui_stdlib.h"
#include "tinyfiledialogs.h" // Make sure this file is placed in src/
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <vector>

using namespace std;

string gstatus = "idle";
int currentId = 0;
ImVec2 nextWindowPosition = ImVec2(10, 10);

static bool showSaveAsDialog;

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
  case FileSaveError:
    data.name = "File Save Error";
    data.debugInfo = "An error occured while saving a file to the disk.";
    data.code = 101;
  case UnknownCommand:
    data.name = "Unknown Command Error";
    data.debugInfo =
        "Malformed or unknown command was entered into the Command Console.";
    data.code = 200;
  case UnsupportedFeature:
    data.name = "Unsupported Feature";
    data.debugInfo = "That feature has not been added yet, keep a lookout for "
                     "future updates on the YouTube channel or the Github.";
    data.code = 201;
  case FileNotOpenError:
    data.name = "File Not Open Error";
    data.debugInfo = "Open a file to use this function";
    data.code = 110;
  }
  return data;
}

class ErrorWindow {
public:
  Error currentError;
  int windowID = 0; // Assigned dynamically upon creation
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

    // NOTE: Appending ### ensures a unique hash ID for the window layout state
    // while presenting a clean, user-friendly visible window title string.
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
  newWindow.windowID =
      currentId++; //  NOTE: Explicit unique ID tracking assigned instantly
  errorWindows.push_back(newWindow);
}

class File {
public:
  string fileContents;
  string filePath;
  string fileName;
  int fileStatus; // NOTE: 0 is saved, 1 is not saved, 2 is no path.
  File(string path, string contents) {
    fileContents = contents;
    filePath = path;
    fileName = split(path, '/').back();
  }
  File() {
    fileContents = "";
    fileStatus = 2;
  }

  void save() { saveAtPath(filePath); }

  void saveAtPath(string path) {
    ofstream saveFile(path);
    if (!saveFile.is_open()) {
      showError(FileSaveError);
    } else {
      saveFile << fileContents << endl;
    }
    saveFile.close();
  }
};

class FileViewer {
private:
  string status;

public:
  vector<File> fileList = {};
  int selectedFile = 0;

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

      selectedFile = fileList.size();
      fileList.push_back(File(filePath, content));
    }
  }

  // NOTE: Added viewerIndex parameters to prevent cross-window ID pollution
  void draw(int viewerIndex) {
    ImGui::PushID(viewerIndex); // NOTE: Scopes everything rendered inside this
                                // window instance

    // NOTE: Prevent window stack collisions if multiple File Viewer panels
    // coexist
    std::string panelTitle =
        "File Viewer###FileViewerPanel_" + std::to_string(viewerIndex);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar;

    if (showSaveAsDialog) {
      flags |= ImGuiWindowFlags_NoInputs;
    }

    ImGui::Begin(panelTitle.c_str(), nullptr, ImGuiWindowFlags_MenuBar);

    if (ImGui::IsWindowFocused() && fileList.size() != 0) {
      status = "editing";
    }

    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
          fileList.push_back(File());
        }
        if (ImGui::MenuItem("Save")) {
          // NOTE: Guard against saving when no file is loaded to prevent
          // out-of-bounds access
          if (fileList.empty()) {
            showError(FileNotOpenError);
          } else {
            if (fileList[selectedFile].fileStatus == 2) {
              showSaveAsDialog = true;
            } else {
            }
            fileList[selectedFile].save();
            status = "Saved " + fileList[selectedFile].fileName + " to disk.";
          }
        }
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }

    if (fileList.size() == 0) {
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
      // NOTE: Tab bars must have localized unique scopes to preserve click
      // states

      std::string tabbarId = "FileViewerTabBar_" + std::to_string(viewerIndex);
      if (ImGui::BeginTabBar(tabbarId.c_str())) {
        for (size_t i = 0; i < fileList.size(); i++) {
          ImGui::PushID(
              (int)i); // NOTE: Protects iterative button clicks within tabs
          if (ImGui::TabItemButton(fileList[i].fileName.c_str())) {
            selectedFile = (int)i;
          }
          ImGui::PopID();
        }

        // NOTE: Replaced uniform label string with dynamic variant targeting
        // active index state
        std::string activeEditorId =
            "##fileEditor_" + std::to_string(selectedFile);
        ImGui::InputTextMultiline(activeEditorId.c_str(),
                                  &fileList[selectedFile].fileContents,
                                  ImVec2(-FLT_MIN, -FLT_MIN));
      }
      ImGui::EndTabBar();
    }
    ImGui::Text(status.c_str());
    ImGui::End();
    ImGui::PopID(); // NOTE: Safely closes pushed viewerIndex scope
  }
};

// Variables for File Viewer
bool showUseFileLoadToLoadFileText = false;
vector<FileViewer> fileViewersCurrentOpen = {FileViewer()};
static bool showFileViewer = true;

// Variables for command console
static bool showCommandConsole = false;

// Variables for File -> Open Recent
static bool showOpenRecentWindow = false;
static std::string openRecentSearchBarBuffer =
    "Search here"; // Converted to string for stdlib support
static int openRecentSelectedFile = -1;
static string openRecentRecentFiles[10] = {
    "poo.dingle",       "fibonacci.dingle", "mario.dingle",     "poo.dingle",
    "fibonacci.dingle", "poo.dingle",       "fibonacci.dingle", "poo.dingle",
    "fibonacci.dingle", "poop.dingle"};

// Varaiables for File -> Save As
static string saveAsPath;

void addFileToRecentList(const string &filePath) {
  if (filePath.empty())
    return;

  for (int i = 0; i < 10; i++) {
    if (openRecentRecentFiles[i] == filePath) {
      for (int j = i; j > 0; j--) {
        openRecentRecentFiles[j] = openRecentRecentFiles[j - 1];
      }
      openRecentRecentFiles[0] = filePath;
      return;
    }
  }

  for (int i = 9; i > 0; i--) {
    openRecentRecentFiles[i] = openRecentRecentFiles[i - 1];
  }
  openRecentRecentFiles[0] = filePath;
}

void drawCommandBar() {
  if (showCommandConsole) {
    ImGui::Begin("Command Console", &showCommandConsole);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue;
    static char inputBuffer[128] = ""; // Fixed array instantiation bug

    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsAnyItemActive() && !ImGui::IsMouseClicked(0)) {
      ImGui::SetKeyboardFocusHere();
    }

    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##ConsoleInput", inputBuffer, sizeof(inputBuffer),
                         flags)) {
      std::string fullInput(inputBuffer);
      if (!fullInput.empty()) {
        std::stringstream ss(fullInput);
        std::string command;
        std::string argument;

        ss >> command;
        std::getline(ss >> std::ws, argument);

        if (command == "error") {
          if (!argument.empty()) {
            showError(static_cast<Error>(
                std::stoi(argument))); // FIX: Use centralized wrapper
          } else {
            showError(UnknownCommand);
          }
        } else {
          std::cout << "Unknown command: " << command << std::endl;
        }
        inputBuffer[0] = '\0';
      }
    }
    ImGui::End();
  }
}

void drawSaveAsDialog(File &currentFile) {
  ImGui::Begin("Save as", &showSaveAsDialog, ImGuiWindowFlags_NoCollapse);

  ImGui::Text(saveAsPath.c_str());
  if (ImGui::Button("Browse...")) {
    const char *filterPatterns[] = {"*"};
    const char *selectedPath =
        tinyfd_openFileDialog("Browse",       // Title
                              ".",            // Default path
                              1,              // Number of filter patterns
                              filterPatterns, // Filter array
                              "All Files",    // Description
                              0               // Allow multiple (0 = false)
        );

    saveAsPath = selectedPath;
  }
  if (ImGui::Button("Save as")) {
    if (saveAsPath.empty()) {
      showError(UnsupportedFeature);
    } else {
      currentFile.saveAtPath(saveAsPath);
      currentFile.fileStatus = 0;
      saveAsPath = "";
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
  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 150");
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGuiIO &io = ImGui::GetIO();
    int w = 1280, h = 720;
    glfwGetFramebufferSize(window, &w, &h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    ImGui::NewFrame();

    std::erase_if(errorWindows,
                  [](const ErrorWindow &window) { return !window.is_open; });

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New")) {
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
      } else if (ImGui::BeginMenu("Debug")) {
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
        fileViewersCurrentOpen[i].draw(
            (int)i); // FIX: Passing loop index parameter here
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
