#pragma once

#include <imgui_memory_editor.h>

struct MemoryRange {
    uint32_t start;
    uint32_t end;
    std::unique_ptr<MemoryEditor> editor;
};

using ValueVariant = std::variant<BEType<uint32_t>, BEType<int32_t>, BEType<uint16_t>, BEType<uint8_t>, BEType<float>, BEVec3, BEMatrix34, MemoryRange, std::string>;


class EntityDebugger {
public:
    void AddOrUpdateEntity(uint32_t actorId, const std::string& entityName, const std::string& valueName, uint32_t address, ValueVariant&& value, bool isEntity = false);
    void SetPosition(uint32_t actorId, const BEVec3& ws_playerPos, const BEVec3& ws_entityPos);
    void SetRotation(uint32_t actorId, const glm::fquat rotation);
    void SetAABB(uint32_t actorId, glm::fvec3 min, glm::fvec3 max);
    void RemoveEntity(uint32_t actorId);
    void RemoveEntityValue(uint32_t actorId, const std::string& valueName);
    void UpdateEntityMemory();

    void UpdateKeyboardControls();
    void DrawEntityInspector();

    struct EntityValue {
        std::string value_name;
        bool frozen = false;
        bool expanded = false;
        uint32_t value_address;
        ValueVariant value;
    };

    struct Entity {
        std::string name;
        bool isEntity;
        float priority;
        BEVec3 position;
        glm::fquat rotation;
        glm::fvec3 aabbMin;
        glm::fvec3 aabbMax;
        std::vector<EntityValue> values;
    };

    std::unordered_map<uint32_t, Entity> m_entities;
    glm::fvec3 m_playerPos = {};
    bool m_resetPlot = false;

private:
    std::string m_filter = std::string(256, '\0');
    bool m_disablePoints = true;
    bool m_disableTexts = false;
    bool m_disableRotations = true;
    bool m_disableAABBs = false;
};