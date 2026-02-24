#include "debug_draw.h"

// -----------------------------------------------------------------------
// Primitive submission (thread-safe)
// -----------------------------------------------------------------------

void DebugDraw::Line(const glm::vec3& a, const glm::vec3& b, uint32_t color, float thickness) {
    std::lock_guard lk(m_mutex);
    m_primitives.push_back({ PrimitiveType::LINE, color, thickness, a, b });
}

void DebugDraw::Box(const glm::vec3& min, const glm::vec3& max, uint32_t color, float thickness) {
    std::lock_guard lk(m_mutex);
    m_primitives.push_back({ PrimitiveType::AABB, color, thickness, min, max });
}

void DebugDraw::Box(const glm::vec3& center, const glm::vec3& halfExtents, const glm::quat& rotation, uint32_t color, float thickness) {
    std::lock_guard lk(m_mutex);
    m_primitives.push_back({ PrimitiveType::ORIENTED_BOX, color, thickness, center, halfExtents, rotation });
}

void DebugDraw::Frustum(const glm::mat4& viewProjection, uint32_t color, float thickness) {
    // Store the inverse VP so we can extract the 8 frustum corners during rendering
    glm::mat4 inv = glm::inverse(viewProjection);
    std::lock_guard lk(m_mutex);
    m_primitives.push_back({ PrimitiveType::FRUSTUM, color, thickness, {}, {}, glm::identity<glm::quat>(), inv });
}

// -----------------------------------------------------------------------
// Projection helpers
// -----------------------------------------------------------------------

static constexpr float NEAR_CLIP_W = 0.001f;

bool DebugDraw::ProjectPoint(const glm::mat4& vp, const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec3& worldPos, glm::vec4& clipOut, ImVec2& screenOut) {
    clipOut = vp * glm::vec4(worldPos, 1.0f);

    if (clipOut.w <= NEAR_CLIP_W) {
        return false; // behind near plane
    }

    float invW = 1.0f / clipOut.w;
    float ndcX = clipOut.x * invW;
    float ndcY = clipOut.y * invW;

    glm::vec2 uvRange = glm::max(uvMax - uvMin, glm::vec2(0.0001f));
    float u = ndcX * 0.5f + 0.5f;
    float v = -ndcY * 0.5f + 0.5f;

    // NDC [-1,1] -> cropped UV range -> viewport sub-region in ImGui logical coords
    screenOut.x = viewportPos.x + ((u - uvMin.x) / uvRange.x) * viewportSize.x;
    screenOut.y = viewportPos.y + ((v - uvMin.y) / uvRange.y) * viewportSize.y; // Y flipped (screen Y goes down)

    return true;
}

void DebugDraw::DrawClippedLine(ImDrawList* drawList, const glm::mat4& vp, const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec3& a, const glm::vec3& b, uint32_t color, float thickness) {
    glm::vec4 clipA = vp * glm::vec4(a, 1.0f);
    glm::vec4 clipB = vp * glm::vec4(b, 1.0f);

    bool behindA = clipA.w <= NEAR_CLIP_W;
    bool behindB = clipB.w <= NEAR_CLIP_W;

    if (behindA && behindB) {
        return; // entire line is behind camera
    }

    // Clip the line segment against the near plane (w = NEAR_CLIP_W)
    // Using parametric form: clip(t) = clipA + t * (clipB - clipA)
    // We want to find t where w(t) = NEAR_CLIP_W
    if (behindA) {
        float t = (NEAR_CLIP_W - clipA.w) / (clipB.w - clipA.w);
        clipA = clipA + t * (clipB - clipA);
    }
    else if (behindB) {
        float t = (NEAR_CLIP_W - clipA.w) / (clipB.w - clipA.w);
        clipB = clipA + t * (clipB - clipA);
    }

    // Perspective divide and viewport mapping
    // NDC [-1,1] maps to the viewport sub-region, not the full screen
    glm::vec2 uvRange = glm::max(uvMax - uvMin, glm::vec2(0.0001f));
    auto toScreen = [&](const glm::vec4& clip) -> ImVec2 {
        float invW = 1.0f / clip.w;
        float ndcX = clip.x * invW;
        float ndcY = clip.y * invW;
        float u = ndcX * 0.5f + 0.5f;
        float v = -ndcY * 0.5f + 0.5f;
        return ImVec2(
            viewportPos.x + ((u - uvMin.x) / uvRange.x) * viewportSize.x,
            viewportPos.y + ((v - uvMin.y) / uvRange.y) * viewportSize.y
        );
    };

    drawList->AddLine(toScreen(clipA), toScreen(clipB), color, thickness);
}

void DebugDraw::DrawEdges(ImDrawList* drawList, const glm::mat4& vp, const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax, const glm::vec3* corners, const int (*edges)[2], int edgeCount, uint32_t color, float thickness) {
    for (int i = 0; i < edgeCount; ++i) {
        DrawClippedLine(drawList, vp, viewportPos, viewportSize, uvMin, uvMax, corners[edges[i][0]], corners[edges[i][1]], color, thickness);
    }
}

// -----------------------------------------------------------------------
// Box edge table (shared by AABB and oriented box)
// -----------------------------------------------------------------------

// 12 edges of a box connecting 8 corners
static constexpr int BOX_EDGES[12][2] = {
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 }, // bottom face
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 }, // top face
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 }, // vertical edges
};

// -----------------------------------------------------------------------
// VP matrix storage
// -----------------------------------------------------------------------

void DebugDraw::SetViewProjection(const glm::mat4& vp) {
    std::lock_guard lk(m_mutex);
    m_viewProjection = vp;
    m_hasVP = true;
}

// -----------------------------------------------------------------------
// Render
// -----------------------------------------------------------------------

void DebugDraw::Render(const glm::vec2& viewportPos, const glm::vec2& viewportSize, const glm::vec2& uvMin, const glm::vec2& uvMax) {
    std::lock_guard lk(m_mutex);

    if (m_primitives.empty() || !m_hasVP) {
        return;
    }

    const glm::mat4& viewProjection = m_viewProjection;

    ImDrawList* drawList = ImGui::GetForegroundDrawList();

    // Clip all debug draw output to the 3D viewport region
    ImVec2 clipMin = ImVec2(viewportPos.x, viewportPos.y);
    ImVec2 clipMax = ImVec2(viewportPos.x + viewportSize.x, viewportPos.y + viewportSize.y);
    drawList->PushClipRect(clipMin, clipMax, true);

    for (const auto& prim : m_primitives) {
        switch (prim.type) {
            case PrimitiveType::LINE: {
                DrawClippedLine(drawList, viewProjection, viewportPos, viewportSize, uvMin, uvMax, prim.a, prim.b, prim.color, prim.thickness);
                break;
            }

            case PrimitiveType::AABB: {
                const glm::vec3& mn = prim.a;
                const glm::vec3& mx = prim.b;

                glm::vec3 corners[8] = {
                    { mn.x, mn.y, mn.z },
                    { mx.x, mn.y, mn.z },
                    { mx.x, mn.y, mx.z },
                    { mn.x, mn.y, mx.z },
                    { mn.x, mx.y, mn.z },
                    { mx.x, mx.y, mn.z },
                    { mx.x, mx.y, mx.z },
                    { mn.x, mx.y, mx.z },
                };

                DrawEdges(drawList, viewProjection, viewportPos, viewportSize, uvMin, uvMax, corners, BOX_EDGES, 12, prim.color, prim.thickness);
                break;
            }

            case PrimitiveType::ORIENTED_BOX: {
                const glm::vec3& center = prim.a;
                const glm::vec3& half = prim.b;
                const glm::mat3 rot = glm::mat3_cast(prim.rotation);

                // Local-space corners of a unit box scaled by halfExtents
                const glm::vec3 localCorners[8] = {
                    { -half.x, -half.y, -half.z },
                    { +half.x, -half.y, -half.z },
                    { +half.x, -half.y, +half.z },
                    { -half.x, -half.y, +half.z },
                    { -half.x, +half.y, -half.z },
                    { +half.x, +half.y, -half.z },
                    { +half.x, +half.y, +half.z },
                    { -half.x, +half.y, +half.z },
                };

                glm::vec3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    corners[i] = center + rot * localCorners[i];
                }

                DrawEdges(drawList, viewProjection, viewportPos, viewportSize, uvMin, uvMax, corners, BOX_EDGES, 12, prim.color, prim.thickness);
                break;
            }

            case PrimitiveType::FRUSTUM: {
                // Extract the 8 corners of the frustum from the inverse VP matrix
                // NDC corners: x,y in [-1,1], z in [-1,1] (OpenGL convention, matching
                // the hand-written projection formula in calculateProjectionMatrix)
                const glm::mat4& invVP = prim.inverseVP;

                static constexpr glm::vec4 ndcCorners[8] = {
                    { -1, -1, -1, 1 }, // near bottom-left
                    { +1, -1, -1, 1 }, // near bottom-right
                    { +1, +1, -1, 1 }, // near top-right
                    { -1, +1, -1, 1 }, // near top-left
                    { -1, -1, +1, 1 }, // far bottom-left
                    { +1, -1, +1, 1 }, // far bottom-right
                    { +1, +1, +1, 1 }, // far top-right
                    { -1, +1, +1, 1 }, // far top-left
                };

                glm::vec3 corners[8];
                for (int i = 0; i < 8; ++i) {
                    glm::vec4 world = invVP * ndcCorners[i];
                    corners[i] = glm::vec3(world) / world.w;
                }

                DrawEdges(drawList, viewProjection, viewportPos, viewportSize, uvMin, uvMax, corners, BOX_EDGES, 12, prim.color, prim.thickness);
                break;
            }
        }
    }

    drawList->PopClipRect();
}

// -----------------------------------------------------------------------
// Clear
// -----------------------------------------------------------------------

void DebugDraw::Clear() {
    std::lock_guard lk(m_mutex);
    m_primitives.clear();
}
