// some ideas for improvements:
// - movement inaccuracy for more difficulty
// - good-samples based on time instead of samples
// - rotate sword 90 deg around z
// - add a cooldown to attacks
//     - add penalty to attacking within cooldown

enum class AttackType {
    None,
    Slash,
    Stab
};


struct DebugSample {
    XrTime time; // should be an epoch time
    glm::fvec3 position;
    glm::fquat rotation;
    glm::fvec3 linearVelocity;
    glm::fvec3 angularVelocity;

    AttackType debug_attackType;

    glm::fvec3 rotatedVelocity() const { return glm::inverse(rotation) * linearVelocity; }
    glm::fvec3 rotatedAngularVelocity() const { return glm::inverse(rotation) * angularVelocity; }
};

struct WeaponProfile {
    float swing_detectionThreshold; // minimum speed for swing detection
    std::chrono::nanoseconds swing_detectionUnderThreshold; // time in nanoseconds to still consider a swing if the speed is under the threshold

    // float SmoothingBufferLength; // samples off which the median is used to perform calculations (used to remove outliers/spasms)

    // float stab_DotThreshold; // dot product threshold for stab detection
    float stab_SpeedThreshold; // minimum linear velocity for stab detection
    float stab_SteadinessThreshold; // maximum angular velocity for stab steadiness
    float stab_travelDistance; // distance before stab activates

    float slash_SpeedThreshold; // minimum angular velocity for swing detection
    float slash_SteadinessThreshold; // maximum angular velocity away from swing direction
    float slash_travelAngle; // angle in [rad] before slash activates
};

struct SpearProfile : WeaponProfile {
    SpearProfile() {
        swing_detectionThreshold = 0.1f;
        swing_detectionUnderThreshold = std::chrono::milliseconds(20);

        // stab_DotThreshold = 0.85f;
        stab_SpeedThreshold = 1.0f; // m/s
        stab_SteadinessThreshold = glm::cos(glm::pi<float>()/6); // use 30 deg cone for steadiness
        stab_travelDistance = 0.3f;

        slash_SpeedThreshold = 7.0f;
        slash_SteadinessThreshold = glm::cos(glm::pi<float>() / 4); // 45 deg | portion of direction vector of normalized angular velocity pointed in the right direction
        slash_travelAngle = glm::pi<float>()/6.0f; // 30 deg minimum
    }
};

class WeaponMotionAnalyser {
public:
    WeaponMotionAnalyser() = default;

    static constexpr int MAX_SAMPLES = 90;
    static constexpr int BAD_SAMPLES_BUFFER = 5;
    static constexpr std::chrono::nanoseconds GOOD_SAMPLES_BEFORE_GOOD_STAB = std::chrono::milliseconds(4);

    WeaponProfile profile = SpearProfile();

    // New member variables for angular velocity plot
    glm::fvec3 m_lastPlottedAngularVelocity = {0.0f, 0.0f, 0.0f};
    bool m_hasValidLastPlottedAngularVelocity = false;
    static constexpr float ANGULAR_VELOCITY_THRESHOLD = 0.1f;

    void Update(const XrSpaceLocation& handLocation, const XrSpaceVelocity& handVelocity, const glm::fquat& headsetRotation, const XrTime inputTime) {
        // Get velocity expressed in controller space
        const glm::fvec3 linearVelocity = ToGLM(handVelocity.linearVelocity);
        const glm::fvec3 angularVelocity = ToGLM(handVelocity.angularVelocity);
        const glm::fquat rotation = ToGLM(handLocation.pose.orientation);
        const glm::fvec3 position = ToGLM(handLocation.pose.position);

        m_rollingSamples[m_rollingSamplesIt] = {
            .time = inputTime,
            .position = position,
            .rotation = rotation,
            .linearVelocity = linearVelocity,
            .angularVelocity = angularVelocity, //glm::inverse(rotation) * angularVelocity,
            .debug_attackType = AttackType::None
        };
        m_lastSampleIdx = m_rollingSamplesIt;
        m_rollingSamplesIt = (m_rollingSamplesIt + 1) % MAX_SAMPLES;

        // ----- Find if rotation is towards center of view -----
        const glm::fvec3 pos_rot_axis = glm::cross(headsetRotation * glm::fvec3(0, 0, -1), rotation * glm::fvec3(0, 0, 1)); // cross product from sword x to forward camera axis
        const bool swing_is_forwards = glm::dot(pos_rot_axis, angularVelocity) > 0;
        //Log::print("!! is forward: {}", swing_is_forwards ? "true" : "false");
        //const float ang = glm::asin(glm::length(pos_rot_axis));

        //Log::print("!! is_attacking: );
        
        const glm::fvec3 localLinearVelocity = glm::inverse(rotation) * linearVelocity;
        // For virtual desktop via steam vr -> use inv(rotation) * angular velocity
        const glm::fvec3 localAngularVelocity = m_rollingSamples[m_lastSampleIdx].rotatedAngularVelocity(); //glm::inverse(rotation) * angularVelocity;

        //Log::print("!! position_world: {}", position);
        //Log::print("!! v_local: {}", localLinearVelocity);
        profile.stab_SpeedThreshold = 1.0f;
        profile.stab_SteadinessThreshold = glm::cos(glm::pi<float>() / 6); // 30 deg accuracy cone
        profile.stab_travelDistance = 0.15f;
        profile.slash_SpeedThreshold = 5.0f;
        profile.slash_SteadinessThreshold = glm::cos(glm::pi<float>() / 3); // use 60 deg

        //Log::print("!! steadiness: {} / {}", glm::normalize(localAngularVelocity).y, profile.slash_SteadinessThreshold);
        profile.slash_travelAngle = glm::pi<float>() / 6;
        // Detect velocity threshold -> set attack type if not in attack & store original angle/position
        detect_attack_type(localLinearVelocity, localAngularVelocity, position, rotation, swing_is_forwards);

        // Log::print("!! attack_state: {}", (int)m_lockedAttackType);

        // Check if attack falls within weaponprofile velocity & angle margins -> if not cancel attack & go back to checking for attack
        check_attack_steadiness(localLinearVelocity, localAngularVelocity);
        //Log::print("!! m_badSampleCtr: {}", m_badSampleCtr);

        // Check if delta_angle/delta_translation is enough to enable attack mode
        set_attack_activity(rotation, position);

        // Log::print("!! AttackType: {} - IsAttacking = {} - bad_samples: {} - v_world: ({}): ", (int)m_lockedAttackType, IsAttacking() ? "true": "false", m_badSampleCtr, localLinearVelocity);

        m_rollingSamples[m_lastSampleIdx].debug_attackType = IsAttacking() ? m_lockedAttackType: AttackType::None;
    }

    void detect_attack_type(const glm::fvec3 localLinearVelocity, const glm::fvec3 localAngularVelocity, const glm::fvec3 position, const glm::fquat rotation, const bool swing_is_forward) {
        // Log::print("!! [WeaponMotionAnalyser] Detecting attack type with linear velocity: {}, attack type = {}, active: {}", localLinearVelocity, static_cast<int>(m_lockedAttackType), static_cast<int>(m_attackActivity));

        if (m_lockedAttackType == AttackType::None) {
            glm::fvec3 dir_ang = glm::normalize(localAngularVelocity);
            glm::fvec3 stab_ang = glm::normalize(localLinearVelocity);

            if (abs(- stab_ang.z) > profile.stab_SteadinessThreshold && -localLinearVelocity.z > profile.stab_SpeedThreshold) {
                m_goodSwingSampleCtr = 0;
                m_lockedPosition = position;
                m_goodStabSampleCtr++;
                if (m_goodStabSampleCtr > 3) {
                    m_lockedAttackType = AttackType::Stab;
                }
            }
            else if (abs(dir_ang.y) > profile.slash_SteadinessThreshold && abs( - localAngularVelocity.y) > profile.slash_SpeedThreshold && swing_is_forward) {
                m_goodStabSampleCtr = 0;
                m_goodSwingSampleCtr++;
                m_lockedAngle = rotation * glm::fvec3(0.0f, 0.0f, 1.0f); // store z-axis
                if (m_goodSwingSampleCtr > 3) {
                    m_lockedAttackType = AttackType::Slash;
                    //Log::print("!! Initiate swing");
                }
            }
            else {
                m_goodStabSampleCtr = 0;
                m_goodSwingSampleCtr = 0;
            }
        }
    }

    void check_attack_steadiness(const glm::fvec3 localLinearVelocity, const glm::fvec3 localAngularVelocity) {
        // check steadiness condition for attack types
        switch (m_lockedAttackType) {
            case AttackType::None: { 
                //m_badSampleCtr = 0;
                return;
            }
            case AttackType::Stab: {
                glm::fvec3 stab_ang = glm::normalize(localLinearVelocity);

                if (abs(stab_ang.z) < profile.stab_SteadinessThreshold || - localLinearVelocity.z < profile.stab_SpeedThreshold * 0.6f) {
                    m_badSampleCtr++;
                }
                else {
                    m_badSampleCtr = 0;
                }
                break;
            }
            case AttackType::Slash: {
                glm::fvec3 dir_ang = glm::normalize(localAngularVelocity);

                if (abs(dir_ang.y) < profile.slash_SteadinessThreshold || abs( - localAngularVelocity.y) < profile.slash_SpeedThreshold * 0.6f) {
                    m_badSampleCtr++;
                }
                else {
                    m_badSampleCtr  = 0;
                }
                break;
            }
        }
        
        // Remove attack type if too many bad samples
        if (m_badSampleCtr >= BAD_SAMPLES_BUFFER) {
            m_badSampleCtr = 0;
            //Log::print("!! removed attack due to bad samples");
            m_lockedAttackType = AttackType::None;
        }
    }

    void set_attack_activity(const glm::fquat rotation, const glm::fvec3 position) {
        if (m_lockedAttackType ==  AttackType::None) {
            m_attackActivity = false;
        }
        else if (!m_attackActivity) {
            switch (m_lockedAttackType) {
                case AttackType::Stab: {
                    const float travel_dist = glm::length(position - m_lockedPosition);
                    Log::print("!! travel distance: {}", travel_dist);
                    if (travel_dist > profile.stab_travelDistance) {
                        m_attackActivity = true;
                    }
                    break;
                }
                case AttackType::Slash: {
                    // Assumptions:
                    // - Rotation (wind-up) only occures along x axis
                    // - detection angle is smaller than 180 deg (calculated angle decreases after 180 degrees)
                    const glm::fvec3 z_start = m_lockedAngle;
                    const glm::fvec3 z_now = rotation * glm::fvec3(0.0f, 0.0f, 1.0f);
                    const float dot_product = glm::dot(z_now, z_start);
                    const float ang_difference = glm::acos(dot_product); // would be more performant to dot_product as comparison value and change profile.slash_travelAngle to cos(angle) instead of angle
                    //Log::print("!! angle_dif: {}", ang_difference);

                    if (ang_difference > profile.slash_travelAngle) {
                        m_attackActivity = true;
                    }
                    break;
                }
                default: {
                    break;
                };
            }
        }
    }

    bool IsAttacking() const {
        return m_attackActivity;
        // return m_lockedAttackType != AttackType::None;
    }

    bool IsHitboxEnabled() const {
        return m_isHitboxEnabled;
    }

    void SetHitboxEnabled(bool enabled) {
        m_isHitboxEnabled = enabled;
    }

    float GetAttackImpulse() const {
        if (m_lockedAttackType == AttackType::Slash) {
            return std::clamp(m_goodSwingSampleCtr / 5.0f, 0.0f, 1.0f);
        }
        else if (m_lockedAttackType == AttackType::Stab) {
            return std::clamp(m_goodStabSampleCtr / 5.0f, 0.0f, 1.0f);
        }
        return 0.0f;
    }

    float GetAttackDamage() const {
        if (m_lockedAttackType == AttackType::Slash) {
            return std::clamp(m_goodSwingSampleCtr / 5.0f, 0.0f, 1.0f);
        }
        else if (m_lockedAttackType == AttackType::Stab) {
            return std::clamp(m_goodStabSampleCtr / 5.0f, 0.0f, 1.0f);
        }
        return 0.0f;
    }

    void ResetSwing() {
        m_goodSwingSampleCtr = 0;
        m_badSwingSampleCtr = 0;
        if (m_lockedAttackType == AttackType::Slash) {
            m_lockedAttackType = AttackType::None;
        }
    }

    void ResetStab() {
        m_goodStabSampleCtr = 0;
        m_badStabSampleCtr = 0;
        if (m_lockedAttackType == AttackType::Stab) {
            m_lockedAttackType = AttackType::None;
        }
    }

    void Reset() {
        m_rollingSamples.fill({});
        ResetSwing();
        ResetStab();
    }

    void ResetIfWeaponTypeChanged(WeaponType weaponType) {
        if (m_weaponType != weaponType) {
            m_weaponType = weaponType;
            Reset();
        }
    }


    void DrawDebugOverlay() const {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "Weapon Motion Debugger");
        ImGui::Text("Weapon Type: %d", static_cast<int>(m_weaponType));
        ImGui::Text("Sample %d / %d", m_lastSampleIdx, MAX_SAMPLES);
        ImGui::Text("Bad samples: %d   Good samples: %d", m_badSwingSampleCtr, m_goodSwingSampleCtr);

        const auto oldestIdx = [this](uint32_t j) { return (m_lastSampleIdx + j) % MAX_SAMPLES; };

        auto drawSnapshot = [](const char* title, const glm::vec3& currDir, glm::vec3& lastDir, bool& lastValid, float threshold, const ImVec4& colLast, const ImVec4& colCurr) {
            const float mag = glm::length(currDir);
            glm::vec3 unit = mag > 0.f ? currDir / mag : glm::vec3{ 0 };
            if (mag >= threshold) {
                lastDir = unit;
                lastValid = true;
            }

            const float lastS[6] = { 0, 0, 0, lastDir.x, lastDir.z, lastDir.y };
            const float currS[6] = { 0, 0, 0, unit.x, unit.z, unit.y };

            if (ImPlot3D::BeginPlot(title, { 0, 230 }, ImPlot3DFlags_NoTitle)) {
                ImPlot3D::SetupAxes("X", "Z", "Y", ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax, ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax, ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax);
                ImPlot3D::SetupAxisLimits(ImAxis3D_X, -1.1f, 1.1f, ImPlot3DCond_Always);
                ImPlot3D::SetupAxisLimits(ImAxis3D_Y, -1.1f, 1.1f, ImPlot3DCond_Always);
                ImPlot3D::SetupAxisLimits(ImAxis3D_Z, -1.1f, 1.1f, ImPlot3DCond_Always);

                if (lastValid) {
                    ImPlot3D::SetNextLineStyle(colLast, 3);
                    ImPlot3D::PlotLine("Last", lastS, lastS + 1, lastS + 2, 2, ImPlot3DLineFlags_Segments, 0, sizeof(float) * 3);
                }
                if (mag > 0) {
                    ImPlot3D::SetNextLineStyle(colCurr, 2);
                    ImPlot3D::PlotLine("Curr", currS, currS + 1, currS + 2, 2, ImPlot3DLineFlags_Segments, 0, sizeof(float) * 3);
                }
                ImPlot3D::EndPlot();
            }
            ImGui::SameLine();
        };

        std::array<float, MAX_SAMPLES> posX{}, posY{}, posZ{};
        std::array<float, MAX_SAMPLES * 2> velLineX{}, velLineY{}, velLineZ{};
        float xMin = FLT_MAX, xMax = -FLT_MAX, yMin = FLT_MAX, yMax = -FLT_MAX, zMin = FLT_MAX, zMax = -FLT_MAX;

        for (uint32_t j = 0; j < MAX_SAMPLES; ++j) {
            const auto& s = m_rollingSamples[oldestIdx(j)];

            posX[j] = s.position.x;
            posY[j] = s.position.z; // swap Y/Z for nicer view
            posZ[j] = s.position.y;

            xMin = std::min(xMin, posX[j]);
            xMax = std::max(xMax, posX[j]);
            yMin = std::min(yMin, posY[j]);
            yMax = std::max(yMax, posY[j]);
            zMin = std::min(zMin, posZ[j]);
            zMax = std::max(zMax, posZ[j]);

            const auto av = s.rotatedAngularVelocity() * 0.05f;

            velLineX[j * 2] = posX[j];
            velLineX[j * 2 + 1] = posX[j] + av.x;
            velLineY[j * 2] = posY[j];
            velLineY[j * 2 + 1] = posY[j] + av.y;
            velLineZ[j * 2] = posZ[j];
            velLineZ[j * 2 + 1] = posZ[j] + av.z;
        }

        if (ImPlot3D::BeginPlot("Weapon Motion", { 0, 300 }, ImPlot3DFlags_NoTitle)) {
            ImPlot3D::SetupAxes("X", "Z", "Y", ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax, ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax, ImPlot3DAxisFlags_LockMin | ImPlot3DAxisFlags_LockMax);
            ImPlot3D::SetupAxisLimits(ImAxis3D_X, xMin - 0.1f, xMax + 0.1f, ImPlot3DCond_Always);
            ImPlot3D::SetupAxisLimits(ImAxis3D_Y, yMin - 0.1f, yMax + 0.1f, ImPlot3DCond_Always);
            ImPlot3D::SetupAxisLimits(ImAxis3D_Z, zMin - 0.1f, zMax + 0.1f, ImPlot3DCond_Always);

            ImPlot3D::SetNextMarkerStyle(ImPlot3DMarker_Circle, 1.5f);
            ImPlot3D::PlotScatter("Pos", posX.data(), posY.data(), posZ.data(), MAX_SAMPLES);

            ImPlot3D::SetNextLineStyle(ImVec4(0, 1, 0.5f, 0.5f), 1.2f);
            ImPlot3D::PlotLine("AngVel", velLineX.data(), velLineY.data(), velLineZ.data(), MAX_SAMPLES * 2, ImPlot3DLineFlags_Segments);
            ImPlot3D::EndPlot();
        }
        ImGui::SameLine();

        std::array<float, MAX_SAMPLES> t{}, avX{}, avY{}, avZ{}, maskSlash{}, maskStab{};
        for (uint32_t j = 0; j < MAX_SAMPLES; ++j) {
            const auto& s = m_rollingSamples[oldestIdx(j)];
            t[j] = static_cast<float>(j);
            avX[j] = s.rotatedAngularVelocity().x;
            avY[j] = s.rotatedAngularVelocity().y;
            avZ[j] = s.rotatedAngularVelocity().z;
            maskSlash[j] = (s.debug_attackType == AttackType::Slash) ? 100.0f : -100.0f;
            maskStab[j] = (s.debug_attackType == AttackType::Stab) ? 100.0f : -100.0f;
        }

        if (ImPlot::BeginPlot("Weapon Steadiness", { 0, 300 }, ImPlotFlags_NoTitle)) {
            ImPlot::SetupAxes("Sample", "Angular Velocity", ImPlotAxisFlags_Lock, ImPlotAxisFlags_Lock);
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, MAX_SAMPLES - 1, ImPlotCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -15, 15, ImPlotCond_Always);

            ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 0.3f), 3);
            ImPlot::PlotShaded("Slash", t.data(), maskSlash.data(), MAX_SAMPLES, -100.0f);
            ImPlot::SetNextLineStyle(ImVec4(1, 0.5f, 0, 0.3f), 3);
            ImPlot::PlotShaded("Stab", t.data(), maskStab.data(), MAX_SAMPLES, -100.0f);

            ImPlot::SetNextLineStyle(ImVec4(0, 1, 0.5f, 0.5f), 2);
            ImPlot::PlotLine("X", t.data(), avX.data(), MAX_SAMPLES);
            ImPlot::PlotLine("Y", t.data(), avY.data(), MAX_SAMPLES);
            ImPlot::PlotLine("Z", t.data(), avZ.data(), MAX_SAMPLES);
            ImPlot::EndPlot();
        }
        ImGui::SameLine();

        // pass references to be manipulated in the drawSnapshot function
        glm::vec3& lastAngDir = m_debugLastAngDir;
        bool& angValid = m_debugAngValid;
        glm::vec3& lastLinDir = m_debugLastLinDir;
        bool& linValid = m_debugLinValid;

        const glm::vec3 currAng = m_rollingSamples[m_lastSampleIdx].rotatedAngularVelocity();
        const glm::vec3 currLin = glm::inverse(m_rollingSamples[m_lastSampleIdx].rotation) * m_rollingSamples[m_lastSampleIdx].linearVelocity;

        drawSnapshot("AngVel Snapshot", currAng, lastAngDir, angValid, 1.5f, ImVec4(1, 0, 0, 1), ImVec4(0.4f, 0.7f, 1, 0.25f));
        drawSnapshot("LinVel Snapshot", currLin, lastLinDir, linValid, 1.5f, ImVec4(0, 1, 0, 1), ImVec4(1, 0.7f, 0.2f, 0.25f));
    }

private:
    WeaponType m_weaponType = LargeSword;
    WeaponProfile m_profile = {};

    std::array<DebugSample, MAX_SAMPLES> m_rollingSamples = {};
    uint32_t m_lastSampleIdx = 0;
    uint32_t m_rollingSamplesIt = 0;

    uint32_t m_goodSwingSampleCtr = 0;
    uint32_t m_badSwingSampleCtr = 0;
    bool m_attackActivity = false;
    bool m_isHitboxEnabled = false;
    uint32_t m_badSampleCtr = 0;
    AttackType m_lockedAttackType = AttackType::None;
    glm::fvec3 m_lockedPosition = {};
    glm::fvec3 m_lockedAngle = {};
    uint32_t m_goodStabSampleCtr = 0;
    uint32_t m_badStabSampleCtr = 0;

    mutable glm::vec3 m_debugLastAngDir = {1, 0, 0};
    mutable bool m_debugAngValid = false;
    mutable glm::vec3 m_debugLastLinDir = { 1, 0, 0 };
    mutable bool m_debugLinValid = false;
};