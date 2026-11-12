#define main existing_fixture_main
#include "test.cpp"
#undef main

static void appendHostPose(std::string& packet, int32_t space, int32_t base, float x, float qw = 1) {
  int32_t count;
  constexpr size_t countOffset = 4 + 12 * sizeof(float) + 3 * sizeof(int32_t);
  std::memcpy(&count, packet.data() + countOffset, sizeof(count));
  ++count;
  std::memcpy(packet.data() + countOffset, &count, sizeof(count));
  packet.append(reinterpret_cast<const char*>(&space), sizeof(space));
  packet.append(reinterpret_cast<const char*>(&base), sizeof(base));
  const float pose[14] = {x, 0, 0, 0, 0, 0, qw, 3};
  packet.append(reinterpret_cast<const char*>(pose), sizeof(pose));
}

static std::string hostPacket(float rightGripQw = 1) {
  auto host = bin(0.0702f, 0, 5, 0.25f, false);
  appendHostPose(host, 108, 106, 0.25f);
  appendHostPose(host, 108, 107, 0.35f);
  appendHostPose(host, 107, 106, 0.45f);
  appendHostPose(host, 106, 107, 0.55f);
  appendHostPose(host, 203, 106, 0.65f, rightGripQw);
  appendHostPose(host, 402, 106, 0.75f);
  return host;
}

static std::string playDown() {
  auto down = txt(10);
  size_t countPos = down.find(" 3 ", down.find("FFFFFFFFFFTFFFFFFFT"));
  assert(countPos != std::string::npos);
  down[countPos + 1] = '9';
  down += " 108 106 0 0 0 0 0 0 1 3 0 0 0 0 0 0";
  down += " 108 107 0 0 0 0 0 0 1 3 0 0 0 0 0 0";
  down += " 107 106 0 0 0 0 0 0 1 3 0 0 0 0 0 0";
  down += " 106 107 0 0 0 0 0 0 1 3 0 0 0 0 0 0";
  down += " 203 106 0.2 0 0 0 0 0 1 3 0 0 0 0 0 0";
  down += " 402 106 0.3 0 0 0 0 0 1 3 0 0 0 0 0 0";
  return down;
}

static void receive(WinXrApiUDP& udp, const std::string& packet) {
  udp.StorePacket(packet.data(), static_cast<int>(packet.size()));
}

static void position(const WxrHostState& state, int space, int base, float expected) {
  const auto* pose = find(state, space, base);
  assert(pose && std::fabs(pose->pose[0] - expected) < 0.0001f);
}

static void checkDiagnosticRoles() {
  using Role = WinXrApiUDP::RightPoseRole;
  assert(WinXrApiUDP::ResolveRightPoseRole(2, "/user/hand/right/input/aim/pose") == Role::Aim);
  assert(WinXrApiUDP::ResolveRightPoseRole(2, "/user/hand/right/input/grip/pose") == Role::Grip);
  for (const auto* path : {"/user/hand/right/input/palm_ext/pose",
                           "/user/hand/right/input/grip_surface/pose",
                           "/user/hand/right/input/a/click",
                           "/user/hand/right/input/aim/pose_extra",
                           "/user/hand/left/input/aim/pose"}) {
    assert(WinXrApiUDP::ResolveRightPoseRole(2, path) == Role::Other);
  }
  assert(WinXrApiUDP::ResolveRightPoseRole(1, "/user/hand/right/input/grip/pose") == Role::Other);
  puts("Diagnostic scope recognizes exact resolved right aim and grip only.");
}

// Platform types used by the extracted runtime binding functions.
#include <algorithm>
#include <mutex>
#include <unordered_map>
using XrAction = int;
using XrPath = int;
using XrSpace = int;
using XrInstance = int;
using XrResult = int;
#define XRAPI_PTR
constexpr int XR_NULL_PATH = 0, XR_SUCCESS = 0, XR_ERROR_VALIDATION_FAILURE = -1;
struct XrPosef {
  struct { float x, y, z, w; } orientation;
  struct { float x, y, z; } position;
};
struct XrActionSpaceCreateInfo { XrAction action; XrPath subactionPath; XrPosef poseInActionSpace; };
struct XrActionSuggestedBinding { XrAction action; XrPath binding; };
struct XrInteractionProfileSuggestedBinding {
  XrPath interactionProfile;
  int countSuggestedBindings;
  const XrActionSuggestedBinding* suggestedBindings;
};
static bool textPriorityRightPoses = true;
static void Logf(const char*, ...) {}
static void Log(const char*) {}
static void XrPosef_Multiply(XrPosef*, const XrPosef*, const XrPosef*) {}

namespace rt {
static bool g_spacesChanged;
#include "runtime-binding-fixture.inc"
static void checkRuntimeBindingResolution() {
  using Role = WinXrApiUDP::RightPoseRole;
  using Source = WinXrApiUDP::PoseSource;
  constexpr XrAction poseAction = 17;
  constexpr XrPath active = 10, alternate = 11;
  enum : XrPath { Right = 1, Left, RightAim, LeftAim, RightGrip, LeftGrip, RightPalm, RightSurface, RightButton };
  rt::g_pathStrings = {{Right, "/user/hand/right"}, {Left, "/user/hand/left"},
      {RightAim, "/user/hand/right/input/aim/pose"}, {LeftAim, "/user/hand/left/input/aim/pose"},
      {RightGrip, "/user/hand/right/input/grip/pose"}, {LeftGrip, "/user/hand/left/input/grip/pose"},
      {RightPalm, "/user/hand/right/input/palm_ext/pose"}, {RightSurface, "/user/hand/right/input/grip_surface/pose"},
      {RightButton, "/user/hand/right/input/a/click"}};
  rt::g_actionNames[poseAction] = "shared_pose";
  rt::g_activeProfile = active;
  rt::g_diagnosticProfileAttached = true;
  auto suggest = [&](XrPath profile, std::initializer_list<XrPath> paths) {
    std::vector<XrActionSuggestedBinding> suggestions;
    for (auto path : paths) suggestions.push_back({poseAction, path});
    XrInteractionProfileSuggestedBinding info{profile, int(suggestions.size()), suggestions.data()};
    assert(xrSuggestInteractionProfileBindings_runtime(0, &info) == XR_SUCCESS);
  };
  XrSpace space = 203;
  XrActionSpaceCreateInfo info{poseAction, Right, {{0, 0, 0, 1}, {0.25f, 0, 0}}};
  auto registered = [&]() {
    assert(rt::AddController(&space, &info));
    assert(rt::g_controllerSpaces.at(space) == 2);
    return rt::g_diagnosticRightPoseSpaces.at(space);
  };
  auto checkOrder = [&](XrPath first, XrPath last, Role expected) {
    rt::g_actionBindings.clear();
    suggest(active, {first, last});
    auto resolved = registered();
    assert(rt::g_actionPaths.at(poseAction) == last);
    assert(rt::ResolveDiagnosticRightPoseRole(resolved.info) == expected);
    assert(resolved.binaryRole == expected);
  };
  checkOrder(RightAim, LeftAim, Role::Aim);
  checkOrder(LeftAim, RightAim, Role::Aim);
  checkOrder(RightGrip, LeftGrip, Role::Grip);
  checkOrder(LeftGrip, RightGrip, Role::Grip);
  puts("Production suggestions and registration resolve shared right aim/grip in both hand orders.");

  rt::g_actionBindings.clear();
  suggest(active, {RightAim, LeftAim});
  suggest(alternate, {RightPalm});
  auto resolved = registered();
  assert(resolved.info.poseInActionSpace.position.x == 0.25f);
  assert(rt::g_controllerGrips.at(space));
  assert(resolved.binaryRole == Role::Other);
  const Role role = rt::ResolveDiagnosticRightPoseRole(resolved.info);
  assert(role == Role::Aim);
  WinXrApiUDP udp;
  udp.RegisterActionSpace(space);
  receive(udp, hostPacket());
  WxrHostPose pose;
  assert(udp.GetPreferredRightPose(space, 106, pose) == Source::Host);
  assert(udp.GetPreferredRightPose(space, 106, pose, role == resolved.binaryRole) == Source::Missing);
  receive(udp, playDown());
  assert(udp.GetPreferredRightPose(space, 106, pose, role == resolved.binaryRole) == Source::Text);
  rt::g_actionBindings.clear();
  suggest(active, {RightAim, LeftGrip});
  resolved = registered();
  assert(rt::ResolveDiagnosticRightPoseRole(resolved.info) == Role::Aim);
  assert(resolved.binaryRole == Role::Grip);
  g_fakeTick += 500;
  assert(udp.GetPreferredRightPose(space, 106, pose, role == resolved.binaryRole) == Source::Missing);
  puts("Attached aim wins over alternate palm, retaining legacy grip routing but rejecting its binary pose.");

  for (auto competing : {RightGrip, RightPalm, RightSurface, RightButton}) {
    rt::g_actionBindings.clear();
    suggest(active, {RightAim, competing});
    assert(rt::ResolveDiagnosticRightPoseRole(registered().info) == Role::Other);
    rt::g_actionBindings.clear();
    suggest(active, {competing, RightAim});
    assert(rt::ResolveDiagnosticRightPoseRole(registered().info) == Role::Other);
  }
  puts("Conflicting right-hand suggestions exclude the space in either order.");

  rt::g_actionBindings.clear();
  suggest(active, {LeftAim});
  suggest(alternate, {RightAim});
  assert(rt::ResolveDiagnosticRightPoseRole(registered().info) == Role::Other);
  rt::g_actionBindings.clear();
  suggest(alternate, {LeftAim, RightAim});
  resolved = registered();
  assert(rt::ResolveDiagnosticRightPoseRole(resolved.info) == Role::Aim);
  resolved.info.subactionPath = XR_NULL_PATH;
  assert(rt::ResolveDiagnosticRightPoseRole(resolved.info) == Role::Other);
  suggest(12, {RightGrip});
  assert(rt::ResolveDiagnosticRightPoseRole(registered().info) == Role::Other);
  rt::g_actionBindings.clear();
  suggest(active, {RightAim});
  rt::g_diagnosticProfileAttached = false;
  assert(rt::ResolveDiagnosticRightPoseRole(registered().info) == Role::Other);
  puts("Profile fallback requires no active binding and a determinate right role after attach.");
}

static void checkPreferredRightPoses() {
  using Source = WinXrApiUDP::PoseSource;
  WinXrApiUDP udp;
  udp.RegisterActionSpace(202);
  udp.RegisterActionSpace(203);
  receive(udp, hostPacket());

  auto idle = txt(21, 0, false);
  const auto count = idle.find(" 3 ", idle.find("FFFFFFFFFFFFFFFFFFF"));
  assert(count != std::string::npos);
  idle[count + 1] = '4';
  idle += " 203 106 0.2 0 0 0 0 0 1 3 1 0 0 0 0 0";
  receive(udp, idle);

  WxrHostState state;
  assert(udp.GetState(state));
  position(state, 0, 106, 0.25f);
  position(state, 202, 106, 9.0f);
  position(state, 203, 106, 0.65f);
  assert(!state.buttons[10] && !state.buttons[18] && state.floats[6] == 0);
  WxrHostPose pose;
  assert(udp.GetPreferredRightPose(202, 106, pose) == Source::Text && pose.pose[0] == 0.15f);
  auto source = udp.GetPreferredRightPose(203, 106, pose);
  assert(source == Source::Text && pose.pose[0] == 0.2f);
  assert(WinXrApiUDP::PreferredPoseLocationFlags(source) == 0xf);
  assert(pose.velocity[0] == 3 && pose.velocity[1] == 1);
  const auto flags = static_cast<unsigned>(pose.velocity[0]);
  assert(WinXrApiUDP::PreferredPoseVelocityFlags(source, flags, true) == 2);
  assert(WinXrApiUDP::PreferredPoseVelocityFlags(source, flags, false) == 3);
  assert(WinXrApiUDP::PreferredPoseVelocityFlags(Source::Host, flags, true) == 3);
  puts("A translated text pose invalidates raw linear velocity only.");
  assert(udp.GetPreferredRightPose(203, 107, pose) == Source::Missing);
  assert(udp.GetPreferredRightPose(402, 106, pose) == Source::Missing);
  assert(WinXrApiUDP::PreferredPoseLocationFlags(Source::Missing) == 0);
  puts("Pose-only CLIENT0 selects both registered pairs. Merge controls, head and default poses retain their existing values.");

  g_fakeTick += 499;
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Text);
  g_fakeTick += 1;
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Host && pose.pose[0] == 0.65f);
  puts("Diagnostic pose expires exactly at 500 ms without another packet.");

  g_fakeTick += 600;
  receive(udp, idle);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Text);
  auto wrongClient = idle;
  wrongClient.replace(0, 7, "CLIENT1");
  receive(udp, wrongClient);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Host);
  receive(udp, idle.substr(0, idle.size() - 2));
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Host);
  puts("Fresh CLIENT0 wins despite stale host. Another client or truncated pose falls back to binary.");

  auto invalidOrientation = idle;
  const auto rightPose = invalidOrientation.find("203 106 0.2 0 0 0 0 0 1");
  assert(rightPose != std::string::npos);
  invalidOrientation[rightPose + std::strlen("203 106 0.2 0 0 0 0 0 ")] = '2';
  receive(udp, invalidOrientation);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Host && pose.pose[6] == 1);

  receive(udp, hostPacket(2));
  const auto missing = udp.GetPreferredRightPose(203, 106, pose);
  assert(missing == Source::Missing && WinXrApiUDP::PreferredPoseLocationFlags(missing) == 0);

  auto rounded = idle;
  rounded.replace(rightPose, std::strlen("203 106 0.2 0 0 0 0 0 1"),
                  "203 106 0.2 0 0 0 0.7071 0 0.7071");
  receive(udp, rounded);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Text);
  assert(std::fabs(std::hypot(pose.pose[4], pose.pose[6]) - 1.0f) < 0.000001f);
  puts("Invalid text orientation falls back, invalid binary is missing, rounded text is normalized.");

  receive(udp, idle);
  udp.ForgetSpace(203);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Missing);
  udp.RegisterActionSpace(203);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Missing);
  receive(udp, idle);
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Text);
  udp.ClearSpaces();
  assert(udp.GetPreferredRightPose(203, 106, pose) == Source::Missing);
  puts("Destroy, re-register and clear retire diagnostic samples.");
}

int main() {
  WinXrApiUDP udp;
  WxrHostState state;
  udp.RegisterActionSpace(202);
  udp.RegisterActionSpace(203);
  receive(udp, hostPacket());
  receive(udp, playDown());
  assert(udp.GetState(state));
  position(state, 0, 106, 0.25f);
  position(state, 108, 106, 0.25f);
  position(state, 108, 107, 0.35f);
  position(state, 107, 106, 0.45f);
  position(state, 106, 107, 0.55f);
  position(state, 402, 106, 0.75f);
  position(state, 202, 106, 0.15f);
  position(state, 203, 106, 0.2f);
  assert(state.floats[6] == 1 && state.buttons[10] && state.buttons[18]);
  puts("PLAY DOWN preserves physical HMD and VIEW/LOCAL/STAGE reference pairs. Registered aim and grip override, unregistered high handle keeps host pose.");
  receive(udp, txt(11, 0, false));
  assert(udp.GetState(state));
  position(state, 202, 106, 9);
  assert(state.floats[6] == 0 && !state.buttons[10] && !state.buttons[18]);
  puts("PLAY UP releases injected controls and restores host aim.");

  receive(udp, playDown());
  udp.ForgetSpace(202);
  assert(udp.GetState(state));
  assert(!find(state, 202, 106));
  receive(udp, playDown());
  assert(udp.GetState(state));
  assert(!find(state, 202, 106));
  udp.RegisterActionSpace(202);
  assert(udp.GetState(state));
  assert(!find(state, 202, 106));
  receive(udp, playDown());
  assert(udp.GetState(state));
  position(state, 202, 106, 0.15f);
  puts("Destroyed aim stops using cached and new injected samples. Re-registration admits a new sample only.");

  g_fakeTick += 1001;
  receive(udp, playDown());
  assert(udp.GetState(state));
  position(state, 108, 106, 0.25f);
  position(state, 202, 106, 0.15f);
  puts("Stale host fallback retains physical reference data and continues registered action injection.");
  udp.ClearSpaces();
  receive(udp, playDown());
  assert(udp.GetState(state));
  assert(!find(state, 202, 106) && !find(state, 203, 106) && !find(state, 108, 106));
  assert(find(state, 0, 106) && state.floats[6] == 1);
  puts("Session clear retires old pairs. Injector-only startup retains synthetic head and controls.");
  checkDiagnosticRoles();
  checkRuntimeBindingResolution();
  checkPreferredRightPoses();
}
