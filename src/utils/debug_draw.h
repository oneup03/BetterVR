#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <imgui.h>
#include <mutex>
#include <vector>

class DebugDraw {
public:
    static DebugDraw& instance() {
        static DebugDraw s_instance;
        return s_instance;
    }

    // -- Submit primitives (thread-safe, callable from any thread) --
    void Line(const glm::vec3& a, const glm::vec3& b, uint32_t color = IM_COL32(0, 255, 0, 255), float thickness = 1.0f);
    void Dot(const glm::vec3& position, float radius = 4.0f, uint32_t color = IM_COL32(0, 255, 0, 255));
    void Circle(const glm::vec3& position, float radius = 6.0f, uint32_t color = IM_COL32(0, 255, 0, 255), float thickness = 1.0f, int segments = 0);
    void Box(const glm::vec3& min, const glm::vec3& max, uint32_t color = IM_COL32(0, 255, 0, 255), float thickness = 1.0f);
    void Box(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, uint32_t color = IM_COL32(0, 255, 0, 255), float thickness = 1.0f);
    void Frustum(const glm::mat4& viewProjection, uint32_t color = IM_COL32(255, 255, 0, 255), float thickness = 1.0f);

    // -- VP matrix for rendering (set from camera hooks) --
    // Stores the view-projection matrix used for rendering debug primitives.
    // Must be a standard column-major VP matrix in game world space.
    // Call from the camera hook each frame.
    void SetViewProjection(const glm::mat4& vp);

    // -- Called by the overlay renderer each time it draws --

    // Projects all submitted primitives using the stored VP matrix and draws
    // them onto ImGui::GetForegroundDrawList(). Does NOT clear the buffer,
    // so the same primitives can be rendered for multiple eyes/passes.
    void Render(const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin = glm::vec2(0.0f), const glm::vec2& uvMax = glm::vec2(1.0f));

    /// Clears all submitted primitives. Call once per game frame, after all
    /// Render() calls for that frame are complete.
    void Clear();

private:
    DebugDraw() = default;

    enum class PrimitiveType : uint8_t {
        LINE,
        CIRCLE,
        AABB,
        ORIENTED_BOX,
        FRUSTUM,
    };

    struct DebugPrimitive {
        PrimitiveType type;
        uint32_t color;
        float thickness;

        // LINE: a, b
        // CIRCLE: a = center, radius/segments/filled used
        // AABB: a = min, b = max
        // ORIENTED_BOX: a = center, b = halfExtents, rotation used
        // FRUSTUM: inverseVP used
        glm::vec3 a = {};
        glm::vec3 b = {};
        glm::quat rotation = glm::identity<glm::quat>();
        glm::mat4 inverseVP = glm::mat4(1.0f);
        float radius = 1.0f;
        int segments = 0;
        bool filled = false;
    };

    std::mutex m_mutex;
    std::vector<DebugPrimitive> m_primitives;
    glm::mat4 m_viewProjection = glm::mat4(1.0f);
    bool m_hasVP = false;

    // Internal helpers
    static bool ProjectPoint(const glm::mat4& vp, const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec3& worldPos, glm::vec4& clipOut, ImVec2& screenOut);
    static void DrawClippedLine(ImDrawList* drawList, const glm::mat4& vp, const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec3& a, const glm::vec3& b, uint32_t color, float thickness);
    static void DrawEdges(ImDrawList* drawList, const glm::mat4& vp, const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec3* corners, const int (*edges)[2], int edgeCount, uint32_t color, float thickness);
};
