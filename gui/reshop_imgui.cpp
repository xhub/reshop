

// Based on https://gist.github.com/ocornut/7e9b3ec566a333d725d4


// FIXME power save: https://github.com/ocornut/imgui/wiki/Implementing-Power-Save,-aka-Idling-outside-of-ImGui

// Creating a node graph editor for Dear ImGui
// Quick sample, not production code! 
// This is quick demo I crafted in a few hours in 2015 showcasing how to use Dear ImGui to create custom stuff,
// which ended up feeding a thread full of better experiments.
// See https://github.com/ocornut/imgui/issues/306 for details

// Fast forward to 2023, see e.g. https://github.com/ocornut/imgui/wiki/Useful-Extensions#node-editors

// Changelog
// - v0.05 (2023-03): fixed for renamed api: AddBezierCurve()->AddBezierCubic().
// - v0.04 (2020-03): minor tweaks
// - v0.03 (2018-03): fixed grid offset issue, inverted sign of 'scrolling'
// - v0.01 (2015-08): initial version

#include <cmath> // fmodf
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_opengl3_loader.h"
#include <GLFW/glfw3.h>
#include <iostream>


// START of my changes

#include "imguial_term.hpp"
#include "rhp_defines.h"
#include "rhp_socket_server.h"
#include "rhp_gui_data.h"


#include "JetBrainsMonoNL-Regular.h"

#include "data/IconsFontAwesome4.h"
#include "data/FontAwesome4.inl"


#include "rhpgui_ipc_poll.h"

// NB: You can use math functions/operators on ImVec2 if you #define IMGUI_DEFINE_MATH_OPERATORS and #include "imgui_internal.h"
// Here we only declare simple +/- operators so others don't leak into the demo code.
static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return {lhs.x + rhs.x, lhs.y + rhs.y}; }
static inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return {lhs.x - rhs.x, lhs.y - rhs.y}; }
static inline ImVec2 operator*(const float c, const ImVec2& vec) { return {c*vec.x, c*vec.y}; }
static inline ImVec2 operator/(const ImVec2& vec, float c) { return {vec.x/c, vec.y/c}; }

// Function to evaluate a cubic Bézier curve at a given t (0 <= t <= 1)
static ImVec2 EvaluateBezier(const ImVec2& p0, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, float t) {
   float u = 1.0F - t;
   float tt = t * t;
   float uu = u * u;
   float uuu = uu * u;
   float ttt = tt * t;

   ImVec2 point = 
      uuu * p0 +               // (1-t)^3 * P0
      3 * uu * t * p1 +        // 3(1-t)^2 * t * P1
      3 * u * tt * p2 +        // 3(1-t) * t^2 * P2
      ttt * p3;                // t^3 * P3
   return point;
}

#define COLOR1 ImColor(255, 100, 100);
#define COLOR2 ImColor(200, 100, 200);
#define COLOR3 ImColor(0, 200, 100);

#define MPCOLOR           IM_COL32(255, 75, 75, 255)
#define MPCOLOR_SELECTED  IM_COL32(245, 60, 60, 255)

#define COLOR_ARCVF_BG    IM_COL32(45, 60, 160, 155)
constexpr ImVec4 COLOR_ARCVF_TEXT = ImVec4(1.0F, 1.0F, 1.0F, 1.0F); // just black

constexpr float NODE_SLOT_RADIUS = 4.0F;
constexpr ImVec2 NODE_WINDOW_PADDING(8.0F, 8.0F);

static void DrawArcVFText(ImVec2 text_center, const char* text, const ImU32 bg_color, const ImVec4& text_color) {
   // Get the current draw list
   ImDrawList* draw_list = ImGui::GetWindowDrawList();

   // Calculate the size of the text
   ImVec2 text_size = ImGui::CalcTextSize(text)/2.F;

   // Define the background rectangle
   ImVec2 bg_min = ImVec2(text_center.x - text_size.x, text_center.y - text_size.y) - NODE_WINDOW_PADDING;
   ImVec2 bg_max = ImVec2(text_center.x + text_size.x, text_center.y + text_size.y) + NODE_WINDOW_PADDING;

   ImGui::SetCursorScreenPos(bg_min + NODE_WINDOW_PADDING);
   // Draw the background rectangle
   draw_list->AddRectFilled(bg_min, bg_max, ImGui::GetColorU32(bg_color));

   // Draw the text
   ImGui::TextColored(text_color, "%s", text);

}


// Dummy data structure provided for the example.
// Note that we storing links as indices (not ID) to make example code shorter.
static void ShowExampleAppCustomNodeGraph(bool* opened)
{
   ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
   if (!ImGui::Begin("Example: Custom Node Graph", opened))
   {
      ImGui::End();
      return;
   }

   // Dummy
   struct EmpDagNode
   {
      int     ID;
      char    Name[32];
      ImVec2  CornerLL, Size;
      int     nParents, nChildren;

      EmpDagNode(int id, const char* name, const ImVec2& pos, int inputs_count, int outputs_count) { ID = id; strcpy(Name, name); CornerLL = pos; nParents = inputs_count; nChildren = outputs_count; }

      ImVec2 GetInputSlotPos(int slot_no) const { return ImVec2(CornerLL.x, CornerLL.y + Size.y * ((float)slot_no + 1) / ((float)nParents + 1)); }
      ImVec2 GetOutputSlotPos(int slot_no) const { return ImVec2(CornerLL.x + Size.x, CornerLL.y + Size.y * ((float)slot_no + 1) / ((float)nChildren + 1)); }
   };
   struct EmpDagSimpleArc
   {
      unsigned     parentIdx, parentSlot, childIdx, childSlot;

      EmpDagSimpleArc(unsigned input_idx, unsigned input_slot, unsigned output_idx, unsigned output_slot) { parentIdx = input_idx; parentSlot = input_slot; childIdx = output_idx; childSlot = output_slot; }
   };
   struct EmpDagVArc
   {
      unsigned     parentIdx, parentSlot, childIdx, childSlot;

      EmpDagVArc(unsigned input_idx, unsigned input_slot, unsigned output_idx, unsigned output_slot) { parentIdx = input_idx; parentSlot = input_slot; childIdx = output_idx; childSlot = output_slot; }
   };

   // State
   static ImVector<EmpDagNode> nodes;
   static ImVector<EmpDagVArc> links;
   static ImVector<EmpDagSimpleArc> linksCtlr;
   static ImVector<EmpDagSimpleArc> linksNash;
   static ImVec2 scrolling = ImVec2(0.0F, 0.0F);
   static bool inited = false;
   static bool show_grid = true;
   static int node_selected = -1;

   // Initialization
   ImGuiIO& io = ImGui::GetIO();
   if (!inited)
   {
      //      nodes.push_back(EmpDagNode(0, "MainTex", ImVec2(40, 50), 0.5f, 1, 1));
      //      nodes.push_back(EmpDagNode(1, "BumpMap", ImVec2(40, 150), 0.42f, 1, 1));
      //      nodes.push_back(EmpDagNode(2, "Combine", ImVec2(270, 80), 1.0f, 2, 2));
      //      links.push_back(EmpDagSimpleArc(0, 0, 2, 0));
      //      links.push_back(EmpDagSimpleArc(1, 0, 2, 1));
      //      inited = true;
   }

   // Draw a list of nodes on the left side
   bool open_context_menu = false;
   int node_hovered_in_list = -1;
   int node_hovered_in_scene = -1;
   ImGui::BeginChild("node_list", ImVec2(100, 0));
   ImGui::Text("Nodes");
   ImGui::Separator();
   for (int node_idx = 0; node_idx < nodes.Size; node_idx++)
   {
      EmpDagNode* node = &nodes[node_idx];
      ImGui::PushID(node->ID);
      if (ImGui::Selectable(node->Name, node->ID == node_selected)) {
         node_selected = node->ID;
      }
      if (ImGui::IsItemHovered())
      {
         node_hovered_in_list = node->ID;
         open_context_menu |= ImGui::IsMouseClicked(1);
      }
      ImGui::PopID();
   }
   ImGui::EndChild();

   ImGui::SameLine();
   ImGui::BeginGroup();

   // Create our child canvas
   ImGui::Text("Hold middle mouse button to scroll (%.2f,%.2f)", scrolling.x, scrolling.y);
   ImGui::SameLine(ImGui::GetWindowWidth() - 100);
   ImGui::Checkbox("Show grid", &show_grid);
   ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(1, 1));
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
   ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(60, 60, 70, 200));
   ImGui::BeginChild("scrolling_region", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove);
   ImGui::PopStyleVar(); // WindowPadding
   ImGui::PushItemWidth(120.0F);

   const ImVec2 offset = ImGui::GetCursorScreenPos() + scrolling;
   ImDrawList* drawList = ImGui::GetWindowDrawList();

   // Display grid
   if (show_grid)
   {
      ImU32 GRID_COLOR = IM_COL32(200, 200, 200, 40);
      float GRID_SZ = 64.0F;
      ImVec2 win_pos = ImGui::GetCursorScreenPos();
      ImVec2 canvas_sz = ImGui::GetWindowSize();
      for (float x = fmodf(scrolling.x, GRID_SZ); x < canvas_sz.x; x += GRID_SZ)
         drawList->AddLine(ImVec2(x, 0.0F) + win_pos, ImVec2(x, canvas_sz.y) + win_pos, GRID_COLOR);
      for (float y = fmodf(scrolling.y, GRID_SZ); y < canvas_sz.y; y += GRID_SZ)
         drawList->AddLine(ImVec2(0.0F, y) + win_pos, ImVec2(canvas_sz.x, y) + win_pos, GRID_COLOR);
   }

   // Display links
   drawList->ChannelsSplit(2);
   drawList->ChannelsSetCurrent(0); // Background
   for (int link_idx = 0; link_idx < links.Size; link_idx++)
   {
      EmpDagSimpleArc* link = &linksCtlr[link_idx];
      EmpDagNode* parent = &nodes[link->parentIdx];
      EmpDagNode* child = &nodes[link->childIdx];
      ImVec2 p1 = offset + parent->GetOutputSlotPos(link->parentSlot);
      ImVec2 p1a = p1 + ImVec2(+50, 0);
      ImVec2 p2 = offset + child->GetInputSlotPos(link->childSlot);
      ImVec2 p2a = p2 + ImVec2(-50, 0);
      drawList->AddBezierCubic(p1, p1a, p2a, p2, IM_COL32(200, 200, 100, 255), 3.0f);

      // Calculate middle point at t = 0.5
      ImVec2 midpoint = EvaluateBezier(p1, p1a, p2a, p2, 0.5f);

      // Render label
      DrawArcVFText(midpoint, "Midpoint", COLOR_ARCVF_BG, COLOR_ARCVF_TEXT);
   }

   // Display nodes
   for (int node_idx = 0; node_idx < nodes.Size; node_idx++)
   {
      EmpDagNode* node = &nodes[node_idx];
      ImGui::PushID(node->ID);
      ImVec2 node_rect_min = offset + node->CornerLL;

      // Display node contents first
      drawList->ChannelsSetCurrent(1); // Foreground
      bool old_any_active = ImGui::IsAnyItemActive();
      ImGui::SetCursorScreenPos(node_rect_min + NODE_WINDOW_PADDING);
      ImGui::BeginGroup(); // Lock horizontal position
      ImGui::Text("%s", node->Name);
      ImGui::EndGroup();

      // Save the size of what we have emitted and whether any of the widgets are being used
      bool node_widgets_active = (!old_any_active && ImGui::IsAnyItemActive());
      node->Size = ImGui::GetItemRectSize() + NODE_WINDOW_PADDING + NODE_WINDOW_PADDING;
      ImVec2 node_rect_max = node_rect_min + node->Size;

      // Display node box
      drawList->ChannelsSetCurrent(0); // Background
      ImGui::SetCursorScreenPos(node_rect_min);
      ImGui::InvisibleButton("node", node->Size);
      if (ImGui::IsItemHovered())
      {
         node_hovered_in_scene = node->ID;
         open_context_menu |= ImGui::IsMouseClicked(1);
      }
      bool node_moving_active = ImGui::IsItemActive();
      if (node_widgets_active || node_moving_active)
         node_selected = node->ID;
      // TODO: if control is pressed, then move the whole subtree by io.MouseDelta
      if (node_moving_active && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
         node->CornerLL = node->CornerLL + io.MouseDelta;

      ImU32 node_bg_color = (node_hovered_in_list == node->ID || node_hovered_in_scene == node->ID || (node_hovered_in_list == -1 && node_selected == node->ID)) ? MPCOLOR : MPCOLOR_SELECTED;
      drawList->AddRectFilled(node_rect_min, node_rect_max, node_bg_color, 4.0f);
      drawList->AddRect(node_rect_min, node_rect_max, IM_COL32(100, 100, 100, 255), 4.0f);
      for (int slot_idx = 0; slot_idx < node->nParents; slot_idx++)
         drawList->AddCircleFilled(offset + node->GetInputSlotPos(slot_idx), NODE_SLOT_RADIUS, IM_COL32(150, 150, 150, 150));
      for (int slot_idx = 0; slot_idx < node->nChildren; slot_idx++)
         drawList->AddCircleFilled(offset + node->GetOutputSlotPos(slot_idx), NODE_SLOT_RADIUS, IM_COL32(150, 150, 150, 150));

      ImGui::PopID();
   }
   drawList->ChannelsMerge();

   // Open context menu
   if (ImGui::IsMouseReleased(ImGuiMouseButton_Right))
      if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) || !ImGui::IsAnyItemHovered())
      {
         node_selected = node_hovered_in_list = node_hovered_in_scene = -1;
         open_context_menu = true;
      }
   if (open_context_menu)
   {
      ImGui::OpenPopup("context_menu");
      if (node_hovered_in_list != -1)
         node_selected = node_hovered_in_list;
      if (node_hovered_in_scene != -1)
         node_selected = node_hovered_in_scene;
   }

   // Draw context menu
   ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
   if (ImGui::BeginPopup("context_menu"))
   {
      EmpDagNode* node = node_selected != -1 ? &nodes[node_selected] : NULL;
      // FIXME: ImVec2 scene_pos = ImGui::GetMousePosOnOpeningCurrentPopup() - offset;
      if (node != nullptr)
      {
         ImGui::Text("Node '%s'", node->Name);
         ImGui::Separator();
         if (ImGui::MenuItem("Rename..", nullptr, false, false)) {}
         if (ImGui::MenuItem("Delete", nullptr, false, false)) {}
         if (ImGui::MenuItem("Copy", nullptr, false, false)) {}
      }
      else
   {
         //            if (ImGui::MenuItem("Add")) { nodes.push_back(Node(nodes.Size, "New node", scene_pos, 0.5f, ImColor(100, 100, 200), 2, 2)); }
         //            if (ImGui::MenuItem("Paste", NULL, false, false)) {}
      }
      ImGui::EndPopup();
   }
   ImGui::PopStyleVar();

   // Scrolling
   if (ImGui::IsWindowHovered() && !ImGui::IsAnyItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f)) {
      scrolling = scrolling + io.MouseDelta;
   }

   ImGui::PopItemWidth();
   ImGui::EndChild();
   ImGui::PopStyleColor();
   ImGui::PopStyleVar();
   ImGui::EndGroup();

   ImGui::End();
}

static char const* logger_actions[] = {
      ICON_FA_FILES_O " Copy",
      ICON_FA_FILE_O " Clear",
      NULL
   };

#include "tlsdef.h"
tlsvar static ImGuiAl::Log *logObj = nullptr;

//void loggermsg(u8 level, const char* msg)
//{
//   logObj->puts(level, msg);
//}

void loggerfmt(u8 level, const char* fmt, ...) {
   va_list args;
   va_start(args, fmt);

   switch (static_cast<ImGuiAl::Log::Level>(level)) {
   case ImGuiAl::Log::Level::Debug:   logObj->debugVA(fmt, args); break;
   case ImGuiAl::Log::Level::Info:    logObj->infoVA(fmt, args); break;
   case ImGuiAl::Log::Level::Warning: logObj->warningVA(fmt, args); break;
   case ImGuiAl::Log::Level::Error:   logObj->errorVA(fmt, args); break;
   default:
      logObj->errorVA(fmt, args);
   }

   va_end(args);
}

static void logger_init(ImGuiAl::Log &logger)
{
   logObj = &logger;

   logger.setActions(logger_actions);

   logger.setLabel(ImGuiAl::Log::Level::Debug, ICON_FA_BUG " Debug");
   logger.setLabel(ImGuiAl::Log::Level::Info, ICON_FA_INFO " Info");
   logger.setLabel(ImGuiAl::Log::Level::Warning, ICON_FA_EXCLAMATION_TRIANGLE " Warn");
   logger.setLabel(ImGuiAl::Log::Level::Error, ICON_FA_BOMB " Error");
   logger.setCumulativeLabel(ICON_FA_SORT_AMOUNT_DESC " Cumulative");
   logger.setFilterHeaderLabel(ICON_FA_FILTER " Filters");
   logger.setFilterLabel(ICON_FA_SEARCH " Filter (inc,-exc)");

   logger.setLevel(ImGuiAl::Log::Level::Info);
}


void error_init(const char *fmt, ...)
{
   bool stop_gui = false;
   va_list va;
   GLFWwindow *window = glfwGetCurrentContext();
   if (!window) {
      fprintf(stderr, "Fatal error: no active windows.\n");
      fprintf(stderr, "The original failure was:\n");
      va_start(va, fmt);
      vfprintf(stderr, fmt, va);
      va_end(va);
      exit(EXIT_FAILURE);
   }

   while (!glfwWindowShouldClose(window) && !stop_gui) {
      glfwPollEvents();

      if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
         ImGui_ImplGlfw_Sleep(10);
         continue;
      }

   // Start ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();
      
      if (!ImGui::IsPopupOpen("Fatal Error")) { ImGui::OpenPopup("Fatal Error"); }

      if (ImGui::BeginPopupModal("Fatal Error"))
      {
         ImGui::Text("ReSHOP GUI could not initialize. Error msg follows:\n");
         va_start(va, fmt);
         ImGui::TextV(fmt, va);
         va_end(va);
         if (ImGui::Button("Close Program")) {
            ImGui::CloseCurrentPopup();
            stop_gui = true;
         }
         ImGui::EndPopup();
      }

      // Render ImGui
      ImGui::Render();

      int display_w, display_h;
      glfwGetFramebufferSize(window, &display_w, &display_h);
      glViewport(0, 0, display_w, display_h);
      glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      // Swap buffers
      glfwSwapBuffers(window);
   }

   // Cleanup ImGui
   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplGlfw_Shutdown();
   ImGui::DestroyContext();

   glfwDestroyWindow(window);
   glfwTerminate();

   exit(EXIT_FAILURE);
}

static void glfw_error_callback(int error, const char* description)
{
   fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void ShowLicenses(bool *p_open)
{
   if (!ImGui::Begin("ReSHOP GUI license", p_open, ImGuiWindowFlags_AlwaysAutoResize))
   {
      ImGui::End();
      return;
   }
   ImGui::Text("Font Licenses");
   ImGui::BulletText("The fonts used in this GUI, JetBrainsMonoNL and Font Awesome 4, are released under the OFL 1.1 license available at");
   ImGui::SameLine(0, 0);
   ImGui::TextLinkOpenURL("https://openfontlicense.org/");

   ImGui::BulletText("JetBrainsMonoNL is a product of JetBrains: ");
   ImGui::SameLine(0, 0);
   ImGui::TextLinkOpenURL("https://www.jetbrains.com/lp/mono/");
   ImGui::BulletText("Font Awesome 4 is a product of Dave Gandy: ");
   ImGui::SameLine(0, 0);
   ImGui::TextLinkOpenURL("https://fontawesome.com/v4");

    ImGui::End();
}

int main(int argc, char *argv[])
{
   int rc = EXIT_SUCCESS;

   static struct {
      bool showLicenses;
   } winData;

   // START Initialize GLFW
   glfwSetErrorCallback(glfw_error_callback);
   if (!glfwInit()) {
      std::cerr << "Failed to initialize GLFW" << std::endl;
      return -1;
   }

   // Set GLFW context version (OpenGL 3.3 Core Profile)
   glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
   glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
   glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

   GLFWmonitor *monitor = glfwGetPrimaryMonitor();
   float xscale, yscale;
   glfwGetMonitorContentScale(monitor, &xscale, &yscale);
   float highDPIscaleFactor = (xscale > 1 || yscale > 1) ? xscale : 1.F;
#ifdef _WIN32
   if (highDPIscaleFactor > 1.F) {
      glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
   }
#elif __APPLE__
   glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);
   (void)highDPIscaleFactor;
#else
   (void)highDPIscaleFactor;
#endif

   int xdummy, ydummy, mon_w, mon_h;
   glfwGetMonitorWorkarea(monitor, &xdummy, &ydummy, &mon_w, &mon_h);

   // Create a window
   //int display_w0 = 1400, display_h0 = 900;
   constexpr float coeff = .7F;
   int display_w0 = (int)((float) mon_w*coeff), display_h0 = (int)((float)mon_h*coeff);
   GLFWwindow* window = glfwCreateWindow(display_w0, display_h0, "ReSHOP GUI", nullptr, nullptr);
   if (!window) {
      std::cerr << "Failed to create GLFW window" << std::endl;
      glfwTerminate();
      return -1;
   }

   glfwMakeContextCurrent(window);
   glfwSwapInterval(1); // Enable vsync
   // END Initialize GLFW

   // START Initialize Dear ImGui
   IMGUI_CHECKVERSION();
   ImGui::CreateContext();
   ImGuiIO& io = ImGui::GetIO();
   io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
   io.IniFilename = NULL; // FIXME: ini file belongs to the GAMS config or data directory
 
   ImFont* const JetBrainsMonoNL_Regular = io.Fonts->AddFontFromMemoryCompressedBase85TTF(
      JetBrainsMonoNL_Regular_compressed_data_base85,
      14.0F
   );

   if (!JetBrainsMonoNL_Regular) {
      std::cerr << "Error adding JetBrainsMonoNL Regular font" << std::endl;
      return EXIT_FAILURE;
   }

   // Add icons from Font Awesome
   ImFontConfig config;
   config.MergeMode = true;
   config.PixelSnapH = true;

   const ImWchar rangesFA[] = {ICON_MIN_FA, ICON_MAX_FA, 0};

   ImFont* const fontAwesome = io.Fonts->AddFontFromMemoryCompressedTTF(
      FontAwesome4_compressed_data,
      FontAwesome4_compressed_size,
      14.0F, &config, rangesFA
   );

   if (!fontAwesome) {
      fprintf(stderr, "Error adding Font Awesome 4 font");
      return EXIT_FAILURE;
   }


   // Set Dear ImGui style
   ImGui::StyleColorsDark();

   // Initialize Dear ImGui GLFW and OpenGL backends
   ImGui_ImplGlfw_InitForOpenGL(window, true);
   ImGui_ImplOpenGL3_Init("#version 330");
   // END nitialize Dear ImGui

   size_t bufsz = 10240*10240;
   void *buf = malloc(bufsz);

   if (!buf) {
      fprintf(stderr, "ResHOP GUI: failed to allocate buffer of size %zu", bufsz);
      return EXIT_FAILURE;
   }

   ImGuiAl::Log logWin(buf, bufsz);
   logger_init(logWin);

   if (argc != 2 && argc != 3) {
      error_init("ReSHOP_gui expects exactly one or two command line argument(s), got %d\n", argc > 0 ? argc-1 : 0);
   }

   int pid_parent = -1;
   if (argc == 3) {
      char *endptr;
      pid_parent = strtol(argv[2], &endptr, 10);

      /* Check for various possible errors. */

      if (errno == ERANGE) {
         error_init("ReSHOP_gui expects the 2nd argument '%s' to be a valid PID: '%s'\n", argv[1]);
      }

      if (endptr == argv[2]) {
         error_init("ReSHOP_gui expects the 2nd argument '%s' to be a valid PID: '%s'\n", argv[1]);
      }
   }

   GuiData guidat;
   guidat.ipc.name = argv[1];
   guidat.ipc.server_fd = server_init_socket(guidat.ipc.name, pid_parent);
   if (guidat.ipc.server_fd >= 0) {
      logWin.info("ReSHOP GUI starts to listen on %s", argv[1]);
   }
   guidat.models = NULL;

   ipc_poll_gui_init(&guidat);

   bool show_demo_window = true;
   bool is_log_collapsed = false; // Track Log collapse state

   float logHcur = 0;

   // Main loop
   while (!glfwWindowShouldClose(window)) {
      // Poll and handle events (inputs, window resize, etc.)
      // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
      // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
      // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
      // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
      glfwPollEvents();

      int ipc_rc = ipc_pool_process(&guidat);
      if (ipc_rc) {
         loggerfmt(ErrorGui, "[IPC] ERRROR: polling failed with rc = %d\n", ipc_rc);
      }

      if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
         ImGui_ImplGlfw_Sleep(10);
         continue;
      }

      int display_w, display_h;
      glfwGetFramebufferSize(window, &display_w, &display_h);

      // Start ImGui frame
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      if (ImGui::BeginMainMenuBar())
      {
         if (ImGui::BeginMenu("About"))
         {
            ImGui::MenuItem("Licenses", NULL, &winData.showLicenses);
            ImGui::EndMenu();
         }

         ImGui::EndMainMenuBar();
      }

      // FIXME: List all windows here, including Log
      if (winData.showLicenses) { ShowLicenses(&winData.showLicenses); }

      ImVec2 mainWindowSize = io.DisplaySize;

      // Calculate the position and size for the bottom window
      float logH = mainWindowSize.y * 0.25f;
      float logW = mainWindowSize.x;
      ImVec2 logPos(0, mainWindowSize.y - logH);

      // Set position and size for the child window
      ImGui::SetNextWindowPos(logPos, ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(logW, logH), ImGuiCond_Once);

      if (display_w0 != display_w) {
         if (logHcur == 0) {
            logHcur = logH;
         }
         ImGui::SetNextWindowSize(ImVec2(mainWindowSize.x, logHcur));
      }

      // use ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove ?
      // TODO we have a bit of screen tearing because of the use of
      // SetWindowPos. Using SetNextWindowPos is better but
      // we need to understand when the windows collapse status has changed
      ImGui::Begin("Log");
      if (ImGui::IsWindowCollapsed()) {
         ImGui::SetWindowPos(ImVec2(0, mainWindowSize.y - ImGui::GetFrameHeight())); // Adjust to bottom
         is_log_collapsed = true;
      } else if (is_log_collapsed) {
         ImGui::SetWindowPos((ImVec2(0, mainWindowSize.y - logHcur)));
         is_log_collapsed = false;
      } else {
         logHcur = ImGui::GetWindowHeight();
      }

      int log_button = logWin.draw();

      switch (log_button) {
         case 1: {
            std::string buffer;

            logWin.iterate([&buffer](ImGuiAl::Log::Info const& header, char const* const line) -> bool {
               switch (static_cast<ImGuiAl::Log::Level>(header.metaData)) {
                  case ImGuiAl::Log::Level::Debug:   buffer += "[DEBUG] "; break;
                  case ImGuiAl::Log::Level::Info:    buffer += "[INFO ] "; break;
                  case ImGuiAl::Log::Level::Warning: buffer += "[WARN ] "; break;
                  case ImGuiAl::Log::Level::Error:   buffer += "[ERROR] "; break;
               }

               buffer += line;
               buffer += '\n';

               return true;
            });

            ImGui::SetClipboardText(buffer.c_str());
            break;
         }

         case 2: {
            logWin.clear();
            break;
         }
      }

      ImGui::End();


      // 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
#ifndef GAMS_BUILD
      if (show_demo_window) {
         ImGui::ShowDemoWindow(&show_demo_window);
      }
#endif

      // Call the function to show the tree
      bool dummy;
      ShowExampleAppCustomNodeGraph(&dummy);


      // Render ImGui
      ImGui::Render();
      glViewport(0, 0, display_w, display_h);
      glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
      glClear(GL_COLOR_BUFFER_BIT);
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

      // Swap buffers
      glfwSwapBuffers(window);
   }

   guidata_fini(&guidat);
   free(buf);

   // Cleanup ImGui
   ImGui_ImplOpenGL3_Shutdown();
   ImGui_ImplGlfw_Shutdown();
   ImGui::DestroyContext();

   // Cleanup reshop data

   glfwDestroyWindow(window);
   glfwTerminate();

   return rc;
}
