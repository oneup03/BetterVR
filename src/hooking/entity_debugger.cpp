#include "pch.h"
#include "entity_debugger.h"
#include "instance.h"
#include "rendering/vulkan.h"

#include <imgui_memory_editor.h>

#include "implot3d_internal.h"

std::mutex g_actorListMutex;
std::unordered_map<uint32_t, std::pair<std::string, uint32_t>> s_knownActors;

void CemuHooks::hook_UpdateActorList(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    std::scoped_lock lock(g_actorListMutex);

    // r7 holds actor list size
    // r5 holds current actor index
    // r6 holds current actor* list entry

    // clear actor list when reiterating actor list again
    if (hCPU->gpr[5] == 0) {
        s_knownActors.clear();
    }

    uint32_t actorLinkPtr = hCPU->gpr[6] + offsetof(ActorWiiU, baseProcPtr);
    uint32_t actorNamePtr = 0;
    readMemoryBE(actorLinkPtr, &actorNamePtr);
    if (actorNamePtr == 0)
        return;

    char* actorName = (char*)s_memoryBaseAddress + actorNamePtr;

    if (actorName[0] != '\0') {
        // Log::print("Updating actor list [{}/{}] {:08x} - {}", hCPU->gpr[5], hCPU->gpr[7], hCPU->gpr[6], actorName);
        uint32_t actorId = hCPU->gpr[6] + stringToHash(actorName);
        s_knownActors.emplace(actorId, std::make_pair(actorName, hCPU->gpr[6]));
    }

    // if (strcmp(actorName, "Weapon_Sword_056") == 0) {
    //     // Log::print("Updating actor list [{}/{}] {:08x} - {}", hCPU->gpr[5], hCPU->gpr[7], hCPU->gpr[6], actorName);
    //     // float velocityY = 0.0f;
    //     // readMemoryBE(hCPU->gpr[6] + offsetof(ActorWiiU, velocity.y), &velocityY);
    //     // velocityY = velocityY * 1.5f;
    //     // writeMemoryBE(hCPU->gpr[6] + offsetof(ActorWiiU, velocity.y), &velocityY);
    //     s_currActorPtrs.emplace_back(hCPU->gpr[6]);
    // }
}

// ksys::phys::RigidBodyFromShape::create to create a RigidBody from a shape
// use Actor::getRigidBodyByName

std::unordered_map<uint32_t, std::pair<std::string, uint32_t>> s_alreadyAddedActors;

void EntityDebugger::UpdateEntityMemory() {
    std::scoped_lock lock(g_actorListMutex);

    // remove actors in s_alreadyAddedActors that are no longer in s_knownActors
    for (const auto& hash : s_alreadyAddedActors | std::views::keys) {
        if (!s_knownActors.contains(hash)) {
            RemoveEntity(hash);
        }
    }

    s_alreadyAddedActors = s_knownActors;

    // find the current player (GameROMPlayer)
    BEMatrix34 playerPos = {};
    for (const auto& actorData : s_knownActors | std::views::values) {
        if (actorData.first == "GameROMPlayer") {
            CemuHooks::readMemory(actorData.second + offsetof(ActorWiiU, mtx), &playerPos);
            glm::fvec3 newPlayerPos = playerPos.getPos().getLE();
            if (glm::distance(newPlayerPos, m_playerPos) > 25.0f) {
                m_resetPlot = true;
            }
            m_playerPos = newPlayerPos;

            // // set invisibility flag
            // {
            //     BEType<int32_t> flags = 0;
            //     readMemory(actorData.second + offsetof(ActorWiiU, flags3), &flags);
            //     flags = flags.getLE() | 0x800;
            //     writeMemory(actorData.second + offsetof(ActorWiiU, flags3), &flags);
            // }
            // {
            //     BEType<int32_t> flags = 0;
            //     readMemory(actorData.second + offsetof(ActorWiiU, flags2), &flags);
            //     flags = flags.getLE() | 0x20;
            //     writeMemory(actorData.second + offsetof(ActorWiiU, flags2), &flags);
            //     writeMemory(actorData.second + offsetof(ActorWiiU, flags2Copy), &flags);
            // }
            // {
            //     float lodDrawDistanceMultiplier = 0;
            //     readMemory(actorData.second + offsetof(ActorWiiU, lodDrawDistanceMultiplier), &lodDrawDistanceMultiplier);
            //     lodDrawDistanceMultiplier = 0.0f;
            //     writeMemory(actorData.second + offsetof(ActorWiiU, lodDrawDistanceMultiplier), &lodDrawDistanceMultiplier);
            // }
            // {
            //     float startModelOpacity = 0;
            //     readMemory(actorData.second + offsetof(ActorWiiU, startModelOpacity), &startModelOpacity);
            //     startModelOpacity = 0.0f;
            //     writeMemory(actorData.second + offsetof(ActorWiiU, startModelOpacity), &startModelOpacity);
            // }
            // {
            //     BEType<float> modelOpacity = 1.0f;
            //     readMemory(actorData.second + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
            //     modelOpacity = 1.0f;
            //     writeMemory(actorData.second + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
            // }
            // {
            //     uint8_t opacityOrSomethingEnabled = 0;
            //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);
            //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled)+1, &opacityOrSomethingEnabled);
            //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled)-1, &opacityOrSomethingEnabled);
            //     writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled)-2, &opacityOrSomethingEnabled);
            // }
        }
        else if (actorData.first == "GameRomCamera") {
            CemuHooks::readMemory(actorData.second + offsetof(ActorWiiU, mtx), &playerPos);
            glm::fvec3 newPlayerPos = playerPos.getPos().getLE();
        }
        else if (actorData.first.starts_with("Weapon_Sword")) {
            // BEType<float> modelOpacity = 1.0f;
            // writeMemory(actorData.second + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
            // uint8_t opacityOrSomethingEnabled = 1;
            // writeMemory(actorData.second + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);
        }
    }

    // add actors that aren't in the overlay already
    for (auto& [actorId, actorInfo] : s_knownActors) {
        uint32_t actorPtr = actorInfo.second;
        const std::string& actorName = actorInfo.first;

        auto addField = [&]<typename T>(const std::string& name, uint32_t offset) -> void {
            uint32_t address = actorPtr + offset;
            AddOrUpdateEntity(actorId, actorName, name, address, CemuHooks::getMemory<T>(address), true);
        };

        auto addMemoryRange = [&](const std::string& name, const uint32_t addressPtr, const uint32_t size) -> void {
            uint32_t address = 0;
            if (CemuHooks::readMemoryBE(addressPtr, &address); address != 0) {
                // AddOrUpdateEntity(actorId, actorName, name, address, MemoryRange{ address, address + size, std::make_unique<struct MemoryEditor*>() }, true);
            }
        };

        if (actorName.starts_with("Weapon")) {
            addField.operator()<BEVec3>("Weapon::originalScale", offsetof(Weapon, originalScale));
            addMemoryRange("Weapon::actorAtk.struct7Ptr", actorPtr + offsetof(Weapon, actorAtk.struct7Ptr), 0x2D8);
            addMemoryRange("Weapon::actorAtk.attackSensorStruct7Ptr", actorPtr + offsetof(Weapon, actorAtk.attackSensorPtr), 0x5C);
            addMemoryRange("Weapon::actorAtk.struct8Ptr", actorPtr + offsetof(Weapon, actorAtk.struct8Ptr), 0x714);
            addMemoryRange("Weapon::actorAtk.attackSensorStruct8Ptr", actorPtr + offsetof(Weapon, actorAtk.attackSensorStruct8Ptr), 0x28);
            addField.operator()<BEType<uint16_t>>("Weapon::weaponFlags", offsetof(Weapon, weaponFlags));
            addField.operator()<BEType<uint16_t>>("Weapon::otherFlags", offsetof(Weapon, otherFlags));
        }

        BEMatrix34 mtx = CemuHooks::getMemory<BEMatrix34>(actorPtr + offsetof(ActorWiiU, mtx));
        AddOrUpdateEntity(actorId, actorName, "mtx", actorPtr + offsetof(ActorWiiU, mtx), mtx);
        if (playerPos.pos_x.getLE() != 0.0f) {
            SetPosition(actorId, playerPos.getPos(), mtx.getPos());
        }
        SetRotation(actorId, mtx.getRotLE());

        BEVec3 aabbMin = CemuHooks::getMemory<BEVec3>(actorPtr + offsetof(ActorWiiU, aabb.minX));
        BEVec3 aabbMax = CemuHooks::getMemory<BEVec3>(actorPtr + offsetof(ActorWiiU, aabb.maxX));
        if (aabbMin.x.getLE() != 0.0f) {
            SetAABB(actorId, aabbMin.getLE(), aabbMax.getLE());
        }

        // uint32_t physicsMtxPtr = 0;
        // if (readMemoryBE(actorPtr + offsetof(ActorWiiU, physicsMtxPtr), &physicsMtxPtr); physicsMtxPtr != 0) {
        //     overlay->AddOrUpdateEntity(actorId, actorName, "physicsMtx", physicsMtxPtr, getMemory<BEMatrix34>(physicsMtxPtr));
        // }
        addField.operator()<BEVec3>("velocity", offsetof(ActorWiiU, velocity));
        addField.operator()<BEVec3>("angularVelocity", offsetof(ActorWiiU, angularVelocity));
        addField.operator()<BEVec3>("scale", offsetof(ActorWiiU, scale));
        // addField.operator()<BEVec3>("previousPos", offsetof(ActorWiiU, previousPos));
        // addField.operator()<BEVec3>("previousPos2", offsetof(ActorWiiU, previousPos2));
        // addField.operator()<float>("dispDistSq", offsetof(ActorWiiU, dispDistSq));
        // addField.operator()<float>("deleteDistSq", offsetof(ActorWiiU, deleteDistSq));
        // addField.operator()<float>("loadDistP10", offsetof(ActorWiiU, loadDistP10));
        // addField.operator()<uint32_t>("modelBindInfoPtr", offsetof(ActorWiiU, modelBindInfoPtr));
        // addField.operator()<uint32_t>("gsysModelPtr", offsetof(ActorWiiU, gsysModelPtr));
        // addField.operator()<float>("startModelOpacity", offsetof(ActorWiiU, startModelOpacity));
        // addField.operator()<float>("modelOpacity", offsetof(ActorWiiU, modelOpacity));
        // addField.operator()<uint8_t>("opacityOrSomethingEnabled", offsetof(ActorWiiU, opacityOrSomethingEnabled));
        addField.operator()<BEVec3>("aabb_min", offsetof(ActorWiiU, aabb.minX));
        addField.operator()<BEVec3>("aabb_max", offsetof(ActorWiiU, aabb.maxX));
        addField.operator()<uint32_t>("flags2", offsetof(ActorWiiU, flags2));
        addField.operator()<uint32_t>("flags2Copy", offsetof(ActorWiiU, flags2Copy));
        addField.operator()<uint32_t>("flags", offsetof(ActorWiiU, flags));
        addField.operator()<uint32_t>("flags3", offsetof(ActorWiiU, flags3));

        addField.operator()<uint32_t>("hashId", offsetof(ActorWiiU, hashId));
        addMemoryRange("physics", actorPtr + offsetof(ActorWiiU, actorPhysicsPtr), 0xE0);
        addMemoryRange("actorX6A0", actorPtr + offsetof(ActorWiiU, actorX6A0Ptr), 0x6C);
        addMemoryRange("chemicals", actorPtr + offsetof(ActorWiiU, chemicalsPtr), 0x64);
        addMemoryRange("reactions", actorPtr + offsetof(ActorWiiU, reactionsPtr), 0x0C);
        // addField.operator()<float>("lodDrawDistanceMultiplier", offsetof(ActorWiiU, lodDrawDistanceMultiplier));
    }

    // other systems might've added memory to the overlay, so hence this is a separate loop
    for (auto& entity : m_entities | std::views::values) {
        if (!entity.isEntity)
            continue;

        for (auto& value : entity.values) {
            if (!value.frozen)
                continue;

            std::visit([&](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, BEType<uint32_t>>) {
                    CemuHooks::writeMemory(value.value_address, &arg);
                }
                else if constexpr (std::is_same_v<T, BEType<int32_t>>) {
                    CemuHooks::writeMemory(value.value_address, &arg);
                }
                else if constexpr (std::is_same_v<T, BEType<float>>) {
                    CemuHooks::writeMemory(value.value_address, &arg);
                }
                else if constexpr (std::is_same_v<T, BEVec3>) {
                    CemuHooks::writeMemory(value.value_address, &arg);
                }
                else if constexpr (std::is_same_v<T, BEMatrix34>) {
                    CemuHooks::writeMemory(value.value_address, &arg);
                }
                else if constexpr (std::is_same_v<T, uint8_t>) {
                    CemuHooks::writeMemory(value.value_address, &arg);
                }
            }, value.value);
        }
    }
}


void DrawAABBInPlot(glm::fvec3 pos, glm::fvec3& min, glm::fvec3& max, glm::fquat& rotation) {
    glm::fvec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {max.x, max.y, min.z},
        {min.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {max.x, max.y, max.z},
        {min.x, max.y, max.z}
    };

    glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * glm::mat4_cast(rotation);

    ImPlot3DPoint aabbPoints[8];
    for (int i = 0; i < 8; ++i) {
        glm::vec4 worldPos = transform * glm::vec4(corners[i], 1.0f);
        aabbPoints[i] = ImPlot3DPoint(worldPos.x, worldPos.z, worldPos.y);
    }

    int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0}, // near plane
        {4, 5}, {5, 6}, {6, 7}, {7, 4}, // far plane
        {0, 4}, {1, 5}, {2, 6}, {3, 7}  // connecting edges
    };

    for (const auto& edge : edges) {
        ImVec2 p0 = ImPlot3D::PlotToPixels(aabbPoints[edge[0]]);
        ImVec2 p1 = ImPlot3D::PlotToPixels(aabbPoints[edge[1]]);
        ImPlot3D::GetPlotDrawList()->AddLine(p0, p1, IM_COL32(255, 0, 0, 255));
    }
}

void EntityDebugger::DrawEntityInspector() {
    ImGui::Begin("BetterVR Debugger");

    static char buf[256];
    ImGui::InputText("Entity Filter", buf, std::size(buf));
    m_filter = buf;

    ImGui::BeginChild("ScrollArea", ImVec2(0, 0));

    if (ImGui::CollapsingHeader("World Space Inspector")) {
        ImGui::Checkbox("Disable Points For Entities", &m_disablePoints);
        ImGui::Checkbox("Disable Text For Entities", &m_disableTexts);
        ImGui::Checkbox("Disable Rotations For Entities", &m_disableRotations);
        ImGui::Checkbox("Disable AABBs For Entities", &m_disableAABBs);

        if (ImPlot3D::BeginPlot("##plot", ImVec2(-1, 0), ImPlot3DFlags_NoLegend | ImPlot3DFlags_NoTitle)) {
            // add -50 and 50 to playerPos to make the plot centered around the player
            constexpr float zoomOutAxis = 30.0f;
            ImPlot3D::SetupAxesLimits(
                -zoomOutAxis, +zoomOutAxis,
                -zoomOutAxis, +zoomOutAxis,
                -zoomOutAxis, +zoomOutAxis,
                ImPlot3DCond_Once
            );
            ImPlot3D::SetupAxes("X", "Z", "Y");

            if (m_resetPlot) {
                ImPlot3D::GetCurrentPlot()->Axes[ImAxis3D_X].SetRange(m_playerPos.x-zoomOutAxis, m_playerPos.x+zoomOutAxis);
                ImPlot3D::GetCurrentPlot()->Axes[ImAxis3D_Y].SetRange(m_playerPos.z-zoomOutAxis, m_playerPos.z+zoomOutAxis);
                ImPlot3D::GetCurrentPlot()->Axes[ImAxis3D_Z].SetRange(m_playerPos.y-zoomOutAxis, m_playerPos.y+zoomOutAxis);
                m_resetPlot = false;
            }

            // plot entities in 3D space
            for (auto& entity : m_entities | std::views::values) {
                if (!m_disableTexts) {
                    ImPlot3D::PlotText(entity.name.c_str(), entity.position.x.getLE(), entity.position.z.getLE(), entity.position.y.getLE(), 0, ImVec2(0, 5));
                }
                if (!m_disablePoints) {
                    ImVec2 cntr = ImPlot3D::PlotToPixels(ImPlot3DPoint(entity.position.x.getLE(), entity.position.z.getLE(), entity.position.y.getLE()));
                    ImPlot3D::GetPlotDrawList()->AddCircleFilled(cntr, 2, IM_COL32(255, 255, 0, 255), 8);
                }
                if (!m_disableRotations) {
                    glm::fvec3 start = entity.position.getLE();
                    glm::fvec3 end = entity.rotation * entity.position.getLE() * 0.05f;
                    float xList[] = { start.x, end.x };
                    float yList[] = { start.z, end.z };
                    float zList[] = { start.y, end.y };
                    ImPlot3D::PlotLine(entity.name.c_str(), xList, yList, zList, 2);
                }
                if (!m_disableAABBs) {
                    DrawAABBInPlot(entity.position.getLE(), entity.aabbMin, entity.aabbMax, entity.rotation);
                }
            }

            ImPlot3D::EndPlot();
        }
    }

    // display entities
    if (ImGui::CollapsingHeader("Entity List")) {
        // sort m_entities by priority
        std::multimap<float, std::reference_wrapper<Entity>> sortedEntities;
        for (auto& entity : m_entities | std::views::values) {
            if (m_filter.empty() || entity.name.find(m_filter) != std::string::npos) {
                bool isAnyValueFrozen = std::ranges::any_of(entity.values, [](auto& value) { return value.frozen; });
                // give priority to frozen entities
                sortedEntities.emplace(isAnyValueFrozen ? 0.0f - entity.priority : entity.priority, entity);
            }
        }

        for (auto& entity : sortedEntities | std::views::values) {
            std::string id = entity.get().name + "##" + std::to_string(entity.get().values[0].value_address);
            ImGui::Text(std::format("{}: dist={}", entity.get().name, std::abs(entity.get().priority)).c_str());
            ImGui::PushID(id.c_str());

            for (auto& value : entity.get().values) {
                ImGui::PushID(value.value_name.c_str());

                ImGui::Checkbox("##Frozen", &value.frozen);
                ImGui::SameLine();
                if (ImGui::Button("Copy")) {
                    ImGui::SetClipboardText(std::format("0x{:08x}", value.value_address).c_str());
                }
                ImGui::SameLine();

                ImGui::BeginDisabled(!value.frozen && false);

                std::visit([&]<typename T0>(T0&& arg) {
                    using T = std::decay_t<T0>;

                    if constexpr (std::is_same_v<T, BEType<uint32_t>>) {
                        uint32_t val = std::get<BEType<uint32_t>>(value.value).getLE();
                        if (ImGui::DragScalar(value.value_name.c_str(), ImGuiDataType_U32, &val)) {
                            std::get<BEType<uint32_t>>(value.value) = val;
                        }
                    }
                    else if constexpr (std::is_same_v<T, BEType<int32_t>>) {
                        int32_t val = std::get<BEType<int32_t>>(value.value).getLE();
                        if (ImGui::DragScalar(value.value_name.c_str(), ImGuiDataType_S32, &val)) {
                            std::get<BEType<int32_t>>(value.value) = val;
                        }
                    }
                    else if constexpr (std::is_same_v<T, BEType<float>>) {
                        float val = std::get<BEType<float>>(value.value).getLE();
                        if (ImGui::DragScalar(value.value_name.c_str(), ImGuiDataType_Float, &val)) {
                            std::get<BEType<float>>(value.value) = val;
                        }
                    }
                    else if constexpr (std::is_same_v<T, BEType<uint8_t>>) {
                        uint8_t val = std::get<BEType<uint8_t>>(value.value).getLE();
                        if (ImGui::DragScalar(value.value_name.c_str(), ImGuiDataType_U8, &val)) {
                            std::get<BEType<uint8_t>>(value.value) = val;
                        }
                    }
                    else if constexpr (std::is_same_v<T, BEType<uint16_t>>) {
                        uint16_t val = std::get<BEType<uint16_t>>(value.value).getLE();
                        if (ImGui::DragScalar(value.value_name.c_str(), ImGuiDataType_U16, &val)) {
                            std::get<BEType<uint16_t>>(value.value) = val;
                        }
                    }
                    else if constexpr (std::is_same_v<T, BEVec3>) {
                        float xyz[3] = { std::get<BEVec3>(value.value).x.getLE(), std::get<BEVec3>(value.value).y.getLE(), std::get<BEVec3>(value.value).z.getLE() };
                        if (ImGui::DragFloat3(value.value_name.c_str(), xyz)) {
                            std::get<BEVec3>(value.value).x = xyz[0];
                            std::get<BEVec3>(value.value).y = xyz[1];
                            std::get<BEVec3>(value.value).z = xyz[2];
                        }
                    }
                    else if constexpr (std::is_same_v<T, BEMatrix34>) {
                        if (value.expanded) {
                            auto mtx = std::get<BEMatrix34>(value.value).getLE();
                            ImGui::Indent(); bool row0Changed = ImGui::DragFloat4("Row 0", mtx[0].data(), 10.0f, 0, 0, nullptr, ImGuiSliderFlags_NoRoundToFormat); ImGui::Unindent();
                            ImGui::Indent(); bool row1Changed = ImGui::DragFloat4("Row 1", mtx[1].data(), 10.0f, 0, 0, nullptr, ImGuiSliderFlags_NoRoundToFormat); ImGui::Unindent();
                            ImGui::Indent(); bool row2Changed = ImGui::DragFloat4("Row 2", mtx[2].data(), 10.0f, 0, 0, nullptr, ImGuiSliderFlags_NoRoundToFormat); ImGui::Unindent();
                            if (row0Changed || row1Changed || row2Changed) {
                                std::get<BEMatrix34>(value.value).setLE(mtx);
                            }
                        }
                        else {
                            float xyz[3] = { std::get<BEMatrix34>(value.value).pos_x.getLE(), std::get<BEMatrix34>(value.value).pos_y.getLE(), std::get<BEMatrix34>(value.value).pos_z.getLE() };
                            if (ImGui::DragFloat3(value.value_name.c_str(), xyz)) {
                                std::get<BEMatrix34>(value.value).pos_x = xyz[0];
                                std::get<BEMatrix34>(value.value).pos_y = xyz[1];
                                std::get<BEMatrix34>(value.value).pos_z = xyz[2];
                            }
                        }

                        // allow matrix to be expandable to show more
                        ImGui::SameLine();
                        if (ImGui::Button("...")) {
                            value.expanded = !value.expanded;
                        }
                    }
                    else if constexpr (std::is_same_v<T, MemoryRange>) {
                        if (ImGui::Button(std::format("{} '{}' memory at {:08X}", value.expanded ? "Close" : "Open", value.value_name, value.value_address).c_str())) {
                            value.expanded = !value.expanded;
                        }
                        if (value.expanded) {
                            auto& mem_edit = std::get<MemoryRange>(value.value).editor;
                            uint32_t data = std::get<MemoryRange>(value.value).start;
                            uint32_t size = std::get<MemoryRange>(value.value).end - std::get<MemoryRange>(value.value).start;
                            std::string windowName = std::format("{} at {:08X} with size of {:08X}", value.value_name, value.value_address, size);
                            mem_edit->DrawWindow(windowName.c_str(), (ImU8*)CemuHooks::GetMemoryBaseAddress()+(size_t)data, size, 0x0);
                        }
                    }
                    else if constexpr (std::is_same_v<T, std::string>) {
                        std::string val = std::get<std::string>(value.value);
                        ImGui::Text( val.c_str());
                    }
                }, value.value);

                ImGui::EndDisabled();

                ImGui::PopID();
            }

            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

void EntityDebugger::AddOrUpdateEntity(uint32_t actorId, const std::string& entityName, const std::string& valueName, uint32_t address, ValueVariant&& value, bool isEntity) {
    const auto& [entityIt, _] = m_entities.try_emplace(actorId, Entity{ entityName, isEntity, 0.0f, {}, {}, {}, {} });

    const auto& valueIt = std::ranges::find_if(entityIt->second.values, [&](EntityValue& val) {
        return val.value_name == valueName;
    });

    if (valueIt == entityIt->second.values.end()) {
        entityIt->second.values.emplace_back(valueName, false, false, address, std::move(value));
    }
    else if (!valueIt->frozen && !std::holds_alternative<MemoryRange>(value)) {
        valueIt->value = std::move(value);
    }
}

void EntityDebugger::SetPosition(uint32_t actorId, const BEVec3& ws_playerPos, const BEVec3& ws_entityPos) {
    if (const auto it = m_entities.find(actorId); it != m_entities.end()) {
        it->second.priority = ws_playerPos.DistanceSq(ws_entityPos);
        it->second.position = ws_entityPos;
    }
}

void EntityDebugger::SetRotation(uint32_t actorId, const glm::fquat rotation) {
    if (const auto it = m_entities.find(actorId); it != m_entities.end()) {
        it->second.rotation = rotation;
    }
}

void EntityDebugger::SetAABB(uint32_t actorId, glm::fvec3 min, glm::fvec3 max) {
    if (const auto it = m_entities.find(actorId); it != m_entities.end()) {
        it->second.aabbMin = min;
        it->second.aabbMax = max;
    }
}

void EntityDebugger::RemoveEntity(uint32_t actorId) {
    m_entities.erase(actorId);
}

void EntityDebugger::RemoveEntityValue(uint32_t actorId, const std::string& valueName) {
    if (const auto it = m_entities.find(actorId); it != m_entities.end()) {
        it->second.values.erase(std::ranges::remove_if(it->second.values, [&](const EntityValue& val) {
            return val.value_name == valueName;
        }).begin(), it->second.values.end());
    }
}


std::array<bool, 256> s_pressedKeyState = {};
std::array<bool, ImGuiKey_NamedKey_COUNT> s_pressedNamedKeyState = {};

#define IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN(key, isShiftPressed) { \
    bool isKeyDown = GetAsyncKeyState(key) & 0x8000; \
    bool wasKeyDown = s_pressedKeyState[key]; \
    s_pressedKeyState[key] = isKeyDown; \
    if (isKeyDown && !wasKeyDown) { \
        ImGui::GetIO().AddInputCharacter(isShiftPressed ? key : tolower(key)); \
    } \
}

#define IS_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN(key, imGuiKey) { \
    bool wasKeyDown = s_pressedNamedKeyState[imGuiKey - ImGuiKey_NamedKey_BEGIN]; \
    bool isKeyDown = GetAsyncKeyState(key) & 0x8000; \
    s_pressedNamedKeyState[imGuiKey - ImGuiKey_NamedKey_BEGIN] = isKeyDown; \
    if (isKeyDown && !wasKeyDown) { \
        ImGui::GetIO().AddKeyEvent(imGuiKey, true); \
    } \
    else if (!isKeyDown && wasKeyDown) { \
        ImGui::GetIO().AddKeyEvent(imGuiKey, false); \
    } \
}

void EntityDebugger::UpdateKeyboardControls() {
    // update mouse state depending on if the window is focused
    ImGui::GetIO().AddMouseButtonEvent(0, GetAsyncKeyState(VK_LBUTTON) & 0x8000);
    ImGui::GetIO().AddMouseButtonEvent(1, GetAsyncKeyState(VK_RBUTTON) & 0x8000);
    ImGui::GetIO().AddMouseButtonEvent(2, GetAsyncKeyState(VK_MBUTTON) & 0x8000);


    // capture keyboard input
    ImGui::GetIO().KeyAlt = GetAsyncKeyState(VK_MENU) & 0x8000;
    ImGui::GetIO().KeyCtrl = GetAsyncKeyState(VK_CONTROL) & 0x8000;
    ImGui::GetIO().KeyShift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
    ImGui::GetIO().KeySuper = GetAsyncKeyState(VK_LWIN) & 0x8000;

    // get asciinum from key state
    bool isShift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN(' ', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('0', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('1', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('2', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('3', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('4', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('5', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('6', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('7', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('8', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('9', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('A', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('B', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('C', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('D', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('E', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('F', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('G', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('H', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('I', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('J', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('K', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('L', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('M', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('N', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('O', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('P', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('R', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('Q', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('S', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('T', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('U', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('V', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('W', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('X', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('Y', isShift);
    IS_ALPHANUMERIC_KEY_DOWN_AND_WASNT_PREVIOUSLY_DOWN('Z', isShift);

    // check underscore
    bool isKeyDown = GetAsyncKeyState(VK_OEM_MINUS) & 0x8000;
    bool wasKeyDown = s_pressedKeyState[VK_OEM_MINUS];
    s_pressedKeyState[VK_OEM_MINUS] = isKeyDown;
    if (isKeyDown && !wasKeyDown) {
        ImGui::GetIO().AddInputCharacter(isShift ? '_' : '-');
    }

    // check backspace
    bool isBackspaceKeyDown = GetAsyncKeyState(VK_BACK) & 0x8000;
    bool wasBackspaceKeyDown = s_pressedKeyState[VK_BACK];
    s_pressedKeyState[VK_BACK] = isBackspaceKeyDown;
    if (isBackspaceKeyDown && !wasBackspaceKeyDown) {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, true);
    }
    else if (!isBackspaceKeyDown && wasBackspaceKeyDown) {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Backspace, false);
    }
}