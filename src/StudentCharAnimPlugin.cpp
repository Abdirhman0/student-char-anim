// Student Character Animation Plugin
// Refactored and cleaned for final submission
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS
#include "StudentCharAnimPlugin.h"

#include <model/AnimationModel.h>
#include <model/ModelFactoryRegistry.h>
#include <plugin/IModelPluginService.h>
#include <plugin/PluginContext.h>
#include <plugin/IPluginServices.h>

#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace student::charanim {

enum class AnimState { STAND, WALK, RUN, JUMP };

// ==========================================
// 1. GLOBALS & CONSTANTS
// ==========================================

// Thread-safe state
static std::mutex g_stateMutex;
static AnimState g_currentState = AnimState::STAND;
static double g_stateStartTime = 0.0;

// Current angles for smooth interpolation
static std::unordered_map<std::string, std::vector<double>> g_currentAngles;
static double g_lastTime = 0.0;

namespace {

// --- Speeds (rad/s) ---
constexpr double WALK_CYCLE_SPEED   = 3.5;
constexpr double RUN_CYCLE_SPEED    = 8.3;
constexpr double IDLE_SWAY_SPEED    = 0.5;
constexpr double IDLE_BREATHE_SPEED = 1.5;

// --- Walk Amplitudes ---
constexpr double WALK_LEG_SWING  = 0.40;
constexpr double WALK_KNEE_BEND  = 0.35;
constexpr double WALK_BODY_SWAY  = 0.05;
constexpr double WALK_ARM_SWING  = 0.20;
constexpr double WALK_ARM_SPREAD = 1.5;
constexpr double WALK_ELBOW_BEND = 0.20;

// --- Run Amplitudes ---
constexpr double RUN_LEG_SWING   = 0.88;
constexpr double RUN_KNEE_BEND   = 0.55;
constexpr double RUN_BODY_SWAY   = 0.08;
constexpr double RUN_ARM_SWING   = 0.40;
constexpr double RUN_ARM_SPREAD  = 1.5;
constexpr double RUN_ELBOW_BEND  = 0.20;

// --- Jump Settings ---
constexpr double JUMP_DURATION   = 0.8;
constexpr double JUMP_KNEE_BEND  = -0.65;
constexpr double JUMP_HIP_SPREAD = 1.0;
constexpr double JUMP_ARM_LIFT   = 1.25;

// --- Idle Settings ---
constexpr double IDLE_SWAY_AMP      = 0.16; // Emphasized weight shift for visibility
constexpr double IDLE_BREATHE_AMP   = 0.03;
constexpr double IDLE_ARM_SPREAD    = 1.35;

// --- System Settings ---
constexpr double AUTO_STAND_TIMEOUT = 10.0;
constexpr double INTERP_SPEED_SNAP  = 80.0; // Fast decay lerp to prevent jitter

// ==========================================
// 2. HELPERS
// ==========================================

[[nodiscard]] bool hasJoint(const std::unordered_set<std::string>& availableJointIds, const char* jointId) {
    if (!jointId || *jointId == '\0') return false;
    if (availableJointIds.empty()) return true;
    return availableJointIds.find(jointId) != availableJointIds.end();
}

void smoothInterpolate(std::unordered_map<std::string, std::vector<double>>& current,
                       const std::unordered_map<std::string, std::vector<double>>& target,
                       double lerpSpeed, double dt)
{
    // Apply exponential decay interpolation to prevent robotic joint snapping
    double alpha = 1.0 - std::exp(-lerpSpeed * dt);
    for (auto& pair : current)
    {
        const std::string& joint = pair.first;
        if (target.find(joint) != target.end())
        {
            const auto& tgt = target.at(joint);
            std::vector<double>& cur = pair.second;
            cur[0] += (tgt[0] - cur[0]) * alpha;
            cur[1] += (tgt[1] - cur[1]) * alpha;
            cur[2] += (tgt[2] - cur[2]) * alpha;
        }
    }
}

// ==========================================
// 3. INPUT LOGIC
// ==========================================

void checkInput(double t)
{
    // Map numeric keys to animation states for debugging/testing
    bool key1 = (GetAsyncKeyState('1') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD1) & 0x8000) != 0;
    bool key2 = (GetAsyncKeyState('2') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD2) & 0x8000) != 0;
    bool key3 = (GetAsyncKeyState('3') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD3) & 0x8000) != 0;
    bool key4 = (GetAsyncKeyState('4') & 0x8000) != 0 || (GetAsyncKeyState(VK_NUMPAD4) & 0x8000) != 0;
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    static bool lastKey1 = false;
    static bool lastKey2 = false;
    static bool lastShift = false;

    std::lock_guard<std::mutex> lock(g_stateMutex);

    // Release SHIFT immediate stop
    if (lastShift && !shift)
    {
        if (g_currentState == AnimState::WALK || g_currentState == AnimState::RUN)
        {
            g_currentState = AnimState::STAND;
        }
    }

    if (key1)
    {
        if (shift)
        {
            g_currentState = AnimState::WALK;
        }
        else if (!lastKey1)
        {
            g_currentState = AnimState::WALK;
            g_stateStartTime = t;
        }
    }
    else if (key2)
    {
        if (shift)
        {
            g_currentState = AnimState::RUN;
        }
        else if (!lastKey2)
        {
            g_currentState = AnimState::RUN;
            g_stateStartTime = t;
        }
    }
    else if (key3 && g_currentState != AnimState::JUMP)
    {
        g_currentState = AnimState::JUMP;
        g_stateStartTime = t;
    }
    else if (key4)
    {
        g_currentState = AnimState::STAND;
        g_stateStartTime = 0.0;
    }

    // Automatically stop walking/running after timeout if shift is not held
    if (!shift && g_currentState == AnimState::WALK && (t - g_stateStartTime) > AUTO_STAND_TIMEOUT)
    {
        g_currentState = AnimState::STAND;
    }
    
    if (!shift && g_currentState == AnimState::RUN && (t - g_stateStartTime) > AUTO_STAND_TIMEOUT)
    {
        g_currentState = AnimState::STAND;
    }

    lastKey1 = key1;
    lastKey2 = key2;
    lastShift = shift;
}

// ==========================================
// 4. POSE GENERATORS
// ==========================================

void applyStandPose(std::unordered_map<std::string, std::vector<double>>& targets)
{
    // Apply pendulum-like weight shift to make idle pose look alive
    double sway = std::sin(g_lastTime * IDLE_SWAY_SPEED) * IDLE_SWAY_AMP;
    
    // Slightly faster, subtle breathing
    double breathe = std::sin(g_lastTime * IDLE_BREATHE_SPEED) * IDLE_BREATHE_AMP;

    // 1. Hips (X-axis sway)
    targets["leftHip"]  = {sway, 0.0, 0.0};
    targets["rightHip"] = {sway, 0.0, 0.0};

    // 2. Shoulders (Y-axis breathing + sway counterbalance)
    // Counterbalance torso to prevent tipping
    targets["leftShoulder"]  = {0.0,  IDLE_ARM_SPREAD + breathe + (sway * 0.2), 0.0};
    targets["rightShoulder"] = {0.0, -IDLE_ARM_SPREAD - breathe + (sway * 0.2), 0.0};

    // 3. Elbows (Y-axis relaxed breathing)
    targets["leftElbow"]  = {0.0, 0.08 + breathe, -0.05};
    targets["rightElbow"] = {0.0, 0.08 + breathe, -0.05};

    // 4. Knees (Static relaxed bend)
    // Lock knees with a slight bend to avoid IK snapping and unnatural stiffness.
    targets["leftKnee"]  = {0.0, 0.0, -0.05};
    targets["rightKnee"] = {0.0, 0.0, -0.05};

    // 5. Ankles (Static flat)
    targets["leftAnkle"]  = {0.0, 0.0, 0.0};
    targets["rightAnkle"] = {0.0, 0.0, 0.0};
}

void applyWalkPose(double t, std::unordered_map<std::string, std::vector<double>>& targets)
{
    double L = std::sin(t * WALK_CYCLE_SPEED) * WALK_LEG_SWING;
    double A = std::sin(t * WALK_CYCLE_SPEED) * WALK_ARM_SWING;
    double body = std::sin(t * WALK_CYCLE_SPEED) * WALK_BODY_SWAY;

    targets["leftHip"]  = {body, 0.0,  L};
    targets["rightHip"] = {body, 0.0, -L};

    double kneeL = (L > 0.0) ? -std::abs(L) * WALK_KNEE_BEND : 0.0;
    double kneeR = (L < 0.0) ? -std::abs(L) * WALK_KNEE_BEND : 0.0;

    targets["leftKnee"]  = {0.0, 0.0, kneeL};
    targets["rightKnee"] = {0.0, 0.0, kneeR};

    targets["leftAnkle"]  = {0.0, 0.0, std::abs(L) * 0.10};
    targets["rightAnkle"] = {0.0, 0.0, std::abs(L) * 0.10};

    // Arm swing phase is inverted from leg phase to mimic natural human counterbalance
    targets["leftShoulder"]  = {0.0, WALK_ARM_SPREAD - A, 0.15};
    targets["rightShoulder"] = {0.0, WALK_ARM_SPREAD + A, 0.15};

    targets["leftElbow"]  = {0.0, WALK_ELBOW_BEND, -0.10};
    targets["rightElbow"] = {0.0, WALK_ELBOW_BEND, -0.10};
}

void applyRunPose(double t, std::unordered_map<std::string, std::vector<double>>& targets)
{
    double L = std::sin(t * RUN_CYCLE_SPEED) * RUN_LEG_SWING;
    double A = std::sin(t * RUN_CYCLE_SPEED) * RUN_ARM_SWING;
    double body = std::sin(t * RUN_CYCLE_SPEED) * RUN_BODY_SWAY;

    targets["leftHip"]  = {body, 0.0,  L};
    targets["rightHip"] = {body, 0.0, -L};

    double kneeL = (L > 0.0) ? -std::abs(L) * RUN_KNEE_BEND : 0.0;
    double kneeR = (L < 0.0) ? -std::abs(L) * RUN_KNEE_BEND : 0.0;

    targets["leftKnee"]  = {0.0, 0.0, kneeL};
    targets["rightKnee"] = {0.0, 0.0, kneeR};

    targets["leftAnkle"]  = {0.0, 0.0, std::abs(L) * 0.20};
    targets["rightAnkle"] = {0.0, 0.0, std::abs(L) * 0.20};

    // Arm swing phase is inverted from leg phase to mimic natural human counterbalance
    targets["leftShoulder"]  = {0.0, RUN_ARM_SPREAD - A, 0.15};
    targets["rightShoulder"] = {0.0, RUN_ARM_SPREAD + A, 0.15};

    targets["leftElbow"]  = {0.0, RUN_ELBOW_BEND, -0.10};
    targets["rightElbow"] = {0.0, RUN_ELBOW_BEND, -0.10};
}

bool applyJumpPose(double t, double stateStartTime, std::unordered_map<std::string, std::vector<double>>& targets)
{
    double elapsed = t - stateStartTime;
    if (elapsed > JUMP_DURATION) {
        applyStandPose(targets);
        return true; // Jump is finished
    }
    
    // Legs - wider stance
    targets["leftHip"]  = {0.0, 0.0,  JUMP_HIP_SPREAD};
    targets["rightHip"] = {0.0, 0.0, -JUMP_HIP_SPREAD};

    // Knees - deeper crouch but safer
    targets["leftKnee"]  = {0.0, 0.0, JUMP_KNEE_BEND};
    targets["rightKnee"] = {0.0, 0.0, JUMP_KNEE_BEND};

    // Ankles
    targets["leftAnkle"]  = {0.0, 0.0, 0.15};
    targets["rightAnkle"] = {0.0, 0.0, 0.15};

    // Arms
    targets["leftShoulder"]  = {0.10,  JUMP_ARM_LIFT, 0.0};
    targets["rightShoulder"] = {0.10, -JUMP_ARM_LIFT, 0.0};

    // Elbows
    targets["leftElbow"]  = {0.0, 0.20, -0.10};
    targets["rightElbow"] = {0.0, 0.20, -0.10};

    return false;
}

// ==========================================
// 5. MAIN EVALUATION LOOP
// ==========================================

[[nodiscard]] bool evaluateStudentAnimation(const arkheon::astsim::AnimationModelInput& input, arkheon::astsim::AnimationModelOutput& output) {
    std::unordered_set<std::string> availableJointIds;
    availableJointIds.reserve(input.entity.joints.size());
    for (const auto& joint : input.entity.joints) { availableJointIds.insert(joint.jointId); }

    const double t = input.simulationTimeSeconds;
    checkInput(t);
    
    AnimState state;
    double stateStartTime;
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        state = g_currentState;
        stateStartTime = g_stateStartTime;
    }

    static AnimState lastEvaluatedState = AnimState::STAND;

    std::unordered_map<std::string, std::vector<double>> targetAngles;

    bool jumpFinished = false;
    switch (state)
    {
        case AnimState::WALK:
            applyWalkPose(t, targetAngles);
            break;
        case AnimState::RUN:
            applyRunPose(t, targetAngles);
            break;
        case AnimState::JUMP:
            jumpFinished = applyJumpPose(t, stateStartTime, targetAngles);
            break;
        case AnimState::STAND:
        default:
            applyStandPose(targetAngles);
            break;
    }

    if (jumpFinished) {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        if (g_currentState == AnimState::JUMP) {
            g_currentState = AnimState::STAND;
        }
    }

    double dt = t - g_lastTime;
    if (dt < 0.0 || dt > 1.0) dt = 0.016;
    g_lastTime = t;

    // Ensure all target keys exist in current map
    for (const auto& pair : targetAngles) {
        if (g_currentAngles.find(pair.first) == g_currentAngles.end()) {
            g_currentAngles[pair.first] = pair.second;
        }
    }

    if (state != lastEvaluatedState) {
        // Snap instantly to the new state's pose (0 frames delay)
        g_currentAngles = targetAngles;
    } else {
        // Apply exponential decay interpolation (lerp) to prevent robotic joint snapping during intra-state frame updates.
        smoothInterpolate(g_currentAngles, targetAngles, INTERP_SPEED_SNAP, dt);
    }

    lastEvaluatedState = state;

    // FALSE allows spine to breathe naturally
    output.clearExistingJointOverrides = false;
    output.jointOverrides.clear();

    for (const auto& pair : g_currentAngles) {
        // Output all of our calculated angles
        if (hasJoint(availableJointIds, pair.first.c_str())) {
            output.jointOverrides.push_back({pair.first.c_str(), pair.second[0], pair.second[1], pair.second[2]});
        }
    }

    return !output.jointOverrides.empty();
}

} // namespace

int StudentCharAnimPlugin::getInterfaceVersion() const { return 1; }

arkheon::astlib::PluginMetadata StudentCharAnimPlugin::getMetadata() const {
    arkheon::astlib::PluginMetadata metadata;
    metadata.setPluginId("student-char-anim");
    metadata.setVersion("1.0.0");
    metadata.setAuthor("Student");
    return metadata;
}

void StudentCharAnimPlugin::initialize(arkheon::astlib::PluginContext& context) {
    initialized_         = true;
    shutdown_            = false;
    animationRegistered_ = false;
    modelFactoryRegistry_ = nullptr;

    if (context.services) {
        auto* rawService = context.services->getService(arkheon::astsim::IModelPluginService::kPluginServiceId);
        auto* service = static_cast<arkheon::astsim::IModelPluginService*>(rawService);
        modelFactoryRegistry_ = service ? &service->modelFactoryRegistry() : nullptr;
    }
    if (!modelFactoryRegistry_) return;

    auto* prototypeBase = modelFactoryRegistry_->getRegisteredPrototype(modelType_);
    auto* prototypeAnim = dynamic_cast<arkheon::astsim::IAnimationModel*>(prototypeBase);

    if (!prototypeAnim) {
        return;
    }

    const char* clipNames[] = { "T-Pose", "Idle", "Idle Neutral", "Walk", "Student Walk", "Push", "Jump", "Stand", "Run" };
    for (auto name : clipNames) prototypeAnim->registerAnimation(name, evaluateStudentAnimation);

    animationRegistered_ = true;
}

void StudentCharAnimPlugin::tick(double dt) { static_cast<void>(dt); }

void StudentCharAnimPlugin::shutdown() {
    if (modelFactoryRegistry_ && animationRegistered_) {
        auto* prototypeBase = modelFactoryRegistry_->getRegisteredPrototype(modelType_);
        auto* prototypeAnim = dynamic_cast<arkheon::astsim::IAnimationModel*>(prototypeBase);
        if (prototypeAnim) {
            const char* clipNames[] = { "T-Pose", "Idle", "Idle Neutral", "Walk", "Student Walk", "Push", "Jump", "Stand", "Run" };
            for (auto name : clipNames) prototypeAnim->registerAnimation(name, arkheon::astsim::IAnimationModel::AnimationEvaluationFunction{});
        }
    }
    animationRegistered_  = false;
    shutdown_             = true;
    modelFactoryRegistry_ = nullptr;
}

} // namespace student::charanim

extern "C" {
ARKHEON_ASTLIB_API arkheon::astlib::IPlugin* create_plugin() { return new student::charanim::StudentCharAnimPlugin(); }
ARKHEON_ASTLIB_API void destroy_plugin(arkheon::astlib::IPlugin* plugin) { delete plugin; }
ARKHEON_ASTLIB_API const char* get_plugin_signature() { return "ARKHEON_PLUGIN_V1"; }
}
