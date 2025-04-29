#include "../instance.h"
#include "cemu_hooks.h"

#include <glm/gtx/quaternion.hpp>

#define PADDED_BYTES(from, up) uint8_t byte_##from##[ ## (up-from+0x04) ## ]


#pragma pack(push, 1)
namespace sead {
    struct SafeString {
        BEType<uint32_t> c_str;
        BEType<uint32_t> vtable;
    };

    struct PtrArrayImpl {
        BEType<uint32_t> size;
        BEType<uint32_t> capacity;
        BEType<uint32_t> data;
    };
};

struct ActorPhysics {
    BEType<uint32_t> __vftable;
    sead::SafeString actorName;
    BEType<uint32_t> *physicsParamSet;
    BEType<uint32_t> flags;
    BEType<uint32_t> dword14;
    BEType<uint32_t> dword18;
    BEType<float> scale;
    BEType<uint32_t> gsysModel;
    sead::PtrArrayImpl rigidBodies;
    sead::PtrArrayImpl collisionInfo;
    sead::PtrArrayImpl contactInfo;
};

struct ActorWiiU {
    uint32_t vtable;
    BEType<uint32_t> baseProcPtr;
    uint8_t unk_08[0xF4 - 0x08];
    uint32_t physicsMainBodyPtr; // 0xF4
    uint32_t physicsTgtBodyPtr; // 0xF8
    uint8_t unk_FC[0x1F8 - 0xFC];
    BEMatrix34 mtx;
    uint32_t physicsMtxPtr;
    BEMatrix34 homeMtx;
    BEVec3 velocity;
    BEVec3 angularVelocity;
    BEVec3 scale; // 0x274
    BEType<float> dispDistSq;
    BEType<float> deleteDistSq;
    BEType<float> loadDistP10;
    BEVec3 previousPos;
    BEVec3 previousPos2;
    PADDED_BYTES(0x2A4, 0x324);
    BEType<uint32_t> modelBindInfoPtr;
    PADDED_BYTES(0x32C, 0x32C);
    BEType<uint32_t> gsysModelPtr;
    PADDED_BYTES(0x334, 0x334);
    BEType<float> startModelOpacity;
    BEType<float> modelOpacity;
    PADDED_BYTES(0x340, 0x348);
    struct {
        BEType<float> minX;
        BEType<float> minY;
        BEType<float> minZ;
        BEType<float> maxX;
        BEType<float> maxY;
        BEType<float> maxZ;
    } aabb;
    BEType<uint32_t> flags2;
    BEType<uint32_t> flags2Copy;
    BEType<uint32_t> flags;
    BEType<uint32_t> flags3; // 0x370 or 880. However in IDA there's a 0xF4 offset

    PADDED_BYTES(0x374, 0x39C);
    BEType<uint32_t> actorPhysicsPtr; // 0x3A0
    PADDED_BYTES(0x3A4, 0x404);

    BEType<uint32_t> hashId;
    PADDED_BYTES(0x40C, 0x430);
    uint8_t unk_434;
    uint8_t unk_435;
    uint8_t opacityOrSomethingEnabled;
    uint8_t unk_437;
    PADDED_BYTES(0x438, 0x440);
    BEType<uint32_t> actorX6A0Ptr;
    BEType<uint32_t> chemicalsPtr;
    BEType<uint32_t> reactionsPtr;
    PADDED_BYTES(0x450, 0x48C);
    BEType<float> lodDrawDistanceMultiplier;
    PADDED_BYTES(0x494, 0x538);
};

struct WeaponBase : ActorWiiU {
    PADDED_BYTES(0x53C, 0x728);
};

struct Struct20 {
    BEType<uint32_t> __vftable;
    BEType<uint32_t> field_4;
    BEType<uint32_t> field_8;
    BEType<uint32_t> field_C;
    BEType<uint32_t> field_10;
    BEType<uint32_t> field_14;
};

struct DamageMgr {
    BEType<uint32_t> struct20Ptr;
    BEType<uint32_t> struct20PlayerRelatedPtr;
    BEType<uint32_t> actor;
    struct {
        BEType<uint32_t> size;
        BEType<uint32_t> pointer;
    } damageCallbacks;
    BEType<uint32_t> field_14;
    BEType<uint8_t> field_18;
    BEType<uint8_t> field_19;
    BEType<uint8_t> field_1A;
    BEType<uint8_t> field_1B;
    BEType<uint32_t> __vftable;
    BEType<uint32_t> deleter;
    BEType<uint32_t> field_24;
    BEType<uint32_t> damage;
    BEType<uint32_t> field_2C;
    BEType<uint32_t> minDmg;
    BEType<uint32_t> field_34;
    BEType<uint32_t> field_38;
    BEType<uint32_t> field_3C;
    BEType<uint32_t> damageType;
    BEType<uint32_t> field_44;
    BEType<uint8_t> field_48;
    BEType<uint8_t> field_49;
    BEType<uint8_t> field_4A;
    BEType<uint8_t> field_4B;
};

struct Weapon : WeaponBase {
    PADDED_BYTES(0x72C, 0x95C);
    BEType<uint32_t> weaponShockwaves;
    PADDED_BYTES(0x964, 0x96C);
    BEType<uint32_t> field_974_triggerEventsMaybe;
    BEVec3 field_978;
    BEType<uint32_t> field_984;
    BEType<uint32_t> field_988;
    BEType<uint32_t> field_98C;
    BEType<uint32_t> field_990;
    BEVec3 originalScale;
    BEType<uint32_t> field_99C;
    BEType<uint32_t> field_9A0;
    BEType<uint32_t> field_9A4;
    BEType<uint32_t> field_9A8;
    BEType<uint32_t> field_9AC;
    BEType<uint32_t> field_9B0;
    BEType<uint32_t> field_9B4;
    struct {
        PADDED_BYTES(0x00, 0x08);
        BEType<uint32_t> struct7Ptr; // stores attackinfo, so prolly post-hit stuff
        BEType<uint32_t> field_10;
        BEType<uint32_t> field_14;
        BEType<uint32_t> field_18;
        BEType<uint32_t> field_1C;
        BEType<uint32_t> attackSensorPtr;
        BEType<uint32_t> struct8Ptr; // stores attackinfo, so prolly post-hit stuff. seems to be more player specific
        BEType<uint32_t> field_28;
        BEType<uint32_t> field_2C;
        BEType<uint32_t> field_30;
        BEType<uint32_t> field_34;
        BEType<uint32_t> attackSensorStruct8Ptr;
        BEType<uint8_t> field_3C;
        PADDED_BYTES(0x3D, 0x3C);
    } actorAtk;
    BEType<float> field_9F8;
    BEType<float> field_9FC;
    BEType<uint32_t> damageMgrPtr;
    PADDED_BYTES(0xA04, 0xA10);
    BEType<uint16_t> weaponFlags;
    BEType<uint16_t> otherFlags;
    PADDED_BYTES(0xA18, 0xB58);
};

#pragma pack(pop)

static_assert(sizeof(ActorWiiU) == 0x53C);
static_assert(sizeof(WeaponBase) == 0x72C);
static_assert(sizeof(Weapon) == 0xB5C);
static_assert(sizeof(DamageMgr) == 0x4C);


static uint32_t stringToHash(const char* str) {
    uint32_t hash = 0;
    while (*str) {
        hash = (hash << 7) + *str++;
    }
    return hash;
}

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

void CemuHooks::updateFrames() {
    auto& overlay = VRManager::instance().VK->m_imguiOverlay;

    std::scoped_lock lock(g_actorListMutex);

#ifdef ENABLE_DEBUG_INSPECTOR
    if (overlay) {
        // remove actors in s_alreadyAddedActors that are no longer in s_knownActors
        for (const auto& hash : s_alreadyAddedActors | std::views::keys) {
            if (!s_knownActors.contains(hash)) {
                overlay->RemoveEntity(hash);
            }
        }

        s_alreadyAddedActors = s_knownActors;

        // find current player (GameROMPlayer)
        BEMatrix34 playerPos = {};
        for (const auto& [actorId, actorData] : s_knownActors) {
            if (actorData.first == "GameROMPlayer") {
                readMemory(actorData.second + offsetof(ActorWiiU, mtx), &playerPos);
                glm::fvec3 newPlayerPos = playerPos.getPos().getLE();
                if (glm::distance(newPlayerPos, overlay->m_playerPos) > 25.0f) {
                    overlay->m_resetPlot = true;
                }
                overlay->m_playerPos = newPlayerPos;

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
                readMemory(actorData.second + offsetof(ActorWiiU, mtx), &playerPos);
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
                overlay->AddOrUpdateEntity(actorId, actorName, name, address, getMemory<T>(address), true);
            };

            auto addMemoryRange = [&](const std::string& name, const uint32_t addressPtr, const uint32_t size) -> void {
                uint32_t address = 0;
                if (readMemoryBE(addressPtr, &address); address != 0) {
                    overlay->AddOrUpdateEntity(actorId, actorName, name, address, MemoryRange{ address, address + size, std::make_unique<MemoryEditor>() }, true);
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

            BEMatrix34 mtx = getMemory<BEMatrix34>(actorPtr + offsetof(ActorWiiU, mtx));
            overlay->AddOrUpdateEntity(actorId, actorName, "mtx", actorPtr + offsetof(ActorWiiU, mtx), mtx);
            if (playerPos.pos_x.getLE() != 0.0f) {
                overlay->SetPosition(actorId, playerPos.getPos(), mtx.getPos());
            }
            overlay->SetRotation(actorId, mtx.getRotLE());

            BEVec3 aabbMin = getMemory<BEVec3>(actorPtr + offsetof(ActorWiiU, aabb.minX));
            BEVec3 aabbMax = getMemory<BEVec3>(actorPtr + offsetof(ActorWiiU, aabb.maxX));
            if (aabbMin.x.getLE() != 0.0f) {
                overlay->SetAABB(actorId, aabbMin.getLE(), aabbMax.getLE());
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
        for (auto& entity : VRManager::instance().VK->m_imguiOverlay->m_entities | std::views::values) {
            if (!entity.isEntity)
                continue;

            for (auto& value : entity.values) {
                if (!value.frozen)
                    continue;

                std::visit([&](auto&& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, BEType<uint32_t>>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEType<int32_t>>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEType<float>>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEVec3>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, BEMatrix34>) {
                        writeMemory(value.value_address, &arg);
                    }
                    else if constexpr (std::is_same_v<T, uint8_t>) {
                        writeMemory(value.value_address, &arg);
                    }
                }, value.value);
            }
        }
    }
#endif
}

extern glm::fvec3 g_lookAtPos;
extern glm::fquat g_lookAtQuat;

// glm::fquat rotateHorizontalCounter = glm::quat(glm::vec3(0.0f, glm::pi<float>(), 0.0f));

void vrhook_changeWeaponMtx(OpenXR::EyeSide side, BEMatrix34& toBeAdjustedMtx, BEMatrix34& defaultMtx) {
    // convert VR controller info to glm
    OpenXR::InputState inputs = VRManager::instance().XR->m_input.load();

    glm::fvec3 controllerPos = glm::fvec3(0.0f);
    glm::fquat controllerQuat = glm::identity<glm::fquat>();

    if (inputs.inGame.in_game && inputs.inGame.pose[side].isActive) {
        auto& handPose = side == OpenXR::EyeSide::LEFT ? inputs.inGame.poseLocation[side] : inputs.inGame.poseLocation[side];

        if (handPose.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) {
            controllerPos = glm::fvec3(
                handPose.pose.position.x,
                handPose.pose.position.y,
                handPose.pose.position.z
            );
        }
        if (handPose.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) {
            controllerQuat = glm::fquat(
                handPose.pose.orientation.w,
                handPose.pose.orientation.x,
                handPose.pose.orientation.y,
                handPose.pose.orientation.z
            );
        }
    }

    // handPose.orientation.w = rotateHorizontalCounter.w;
    // handPose.orientation.x = rotateHorizontalCounter.x;
    // handPose.orientation.y = rotateHorizontalCounter.y;
    // handPose.orientation.z = rotateHorizontalCounter.z;
    // rotateHorizontalCounter = glm::rotate(rotateHorizontalCounter, glm::radians(360.0f/30.0f/1.0f), glm::fvec3(1.0f, 0.0f, 0.0f));

    // Next, calculate the rotation
    glm::fquat rotatedControllerQuat = glm::normalize(g_lookAtQuat * controllerQuat);
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(1.0f, 0.0f, 0.0f));
    rotatedControllerQuat = glm::rotate(rotatedControllerQuat, glm::radians(180.0f), glm::fvec3(0.0f, 0.0f, 1.0f));
    glm::fmat3 finalMtx = glm::toMat3(glm::inverse(rotatedControllerQuat));

    toBeAdjustedMtx.x_x = finalMtx[0][0];
    toBeAdjustedMtx.y_x = finalMtx[0][1];
    toBeAdjustedMtx.z_x = finalMtx[0][2];

    toBeAdjustedMtx.x_y = finalMtx[1][0];
    toBeAdjustedMtx.y_y = finalMtx[1][1];
    toBeAdjustedMtx.z_y = finalMtx[1][2];

    toBeAdjustedMtx.x_z = finalMtx[2][0];
    toBeAdjustedMtx.y_z = finalMtx[2][1];
    toBeAdjustedMtx.z_z = finalMtx[2][2];

    // First, calculate the position
    // Use player position as the origin since we want to overwrite the weapon position with the VR controller position
    glm::fvec3 rotatedControllerPos = g_lookAtQuat * controllerPos;
    glm::fvec3 finalPos = g_lookAtPos + rotatedControllerPos;

    toBeAdjustedMtx.pos_x = finalPos.x;
    toBeAdjustedMtx.pos_y = finalPos.y;
    toBeAdjustedMtx.pos_z = finalPos.z;
}

void CemuHooks::hook_changeWeaponMtx(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // r3 holds an actor pointer, I think?
    // r4 holds the bone name
    // r5 holds the matrix that is to be set
    // r6 holds the extra matrix that is to be set
    // r7 holds the ModelBindInfo->mtx

    hCPU->gpr[9] = 0;

    uint32_t actorLinkPtr = hCPU->gpr[3] + offsetof(ActorWiiU, baseProcPtr);
    uint32_t actorNamePtr = 0;
    readMemoryBE(actorLinkPtr, &actorNamePtr);
    if (actorNamePtr == 0)
        return;
    char* actorName = (char*)s_memoryBaseAddress + actorNamePtr;

    // read bone name
    uint32_t boneNamePtr = hCPU->gpr[4];
    if (boneNamePtr == 0)
        return;
    char* boneName = (char*)s_memoryBaseAddress + boneNamePtr;

    // real logic
    bool isHeldByPlayer = strcmp(actorName, "GameROMPlayer") == 0;
    bool isLeftHandWeapon = strcmp(boneName, "Weapon_L") == 0;
    bool isRightHandWeapon = strcmp(boneName, "Weapon_R") == 0;
    if (actorName[0] != '\0' && boneName[0] != '\0' && isHeldByPlayer && (isLeftHandWeapon || isRightHandWeapon)) {
        BEMatrix34 weaponMtx = {};
        readMemory(hCPU->gpr[5], &weaponMtx);

        BEMatrix34 playerMtx = {};
        readMemory(hCPU->gpr[6], &playerMtx);

        BEMatrix34 modelBindInfoMtx = {};
        readMemory(hCPU->gpr[7], &modelBindInfoMtx);

        vrhook_changeWeaponMtx(isLeftHandWeapon ? OpenXR::EyeSide::LEFT : OpenXR::EyeSide::RIGHT, weaponMtx, playerMtx);

        // prevent weapon transparency
        BEType<float> modelOpacity = 1.0f;
        writeMemory(hCPU->gpr[8] + offsetof(ActorWiiU, modelOpacity), &modelOpacity);
        uint8_t opacityOrSomethingEnabled = 1;
        writeMemory(hCPU->gpr[8] + offsetof(ActorWiiU, opacityOrSomethingEnabled), &opacityOrSomethingEnabled);

        writeMemory(hCPU->gpr[5], &weaponMtx);
        writeMemory(hCPU->gpr[6], &playerMtx);
        writeMemory(hCPU->gpr[7], &modelBindInfoMtx);

        hCPU->gpr[9] = 1;

#ifdef ENABLE_DEBUG_INSPECTOR
        if (auto& m_overlay = VRManager::instance().VK->m_imguiOverlay) {
            m_overlay->AddOrUpdateEntity(1337, "PlayerHeldWeapons", isLeftHandWeapon ? "left_weapon_mtx" : "right_weapon_mtx", hCPU->gpr[5], weaponMtx);
            m_overlay->AddOrUpdateEntity(1337, "PlayerHeldWeapons", isLeftHandWeapon ? "left_player_mtx" : "right_player_mtx", hCPU->gpr[6], playerMtx);
            m_overlay->AddOrUpdateEntity(1337, "PlayerHeldWeapons", isLeftHandWeapon ? "left_ModelBindInfo_mtx" : "right_ModelBindInfo_mtx", hCPU->gpr[7], modelBindInfoMtx);
            BEVec3 zeroMtx = {-100.0f, -100.0f, -100.0f};
            m_overlay->SetPosition(1337, zeroMtx, zeroMtx);

            // freeze the value so it doesn't get overwritten
            Entity& entity = m_overlay->m_entities[1337];
            for (auto& value : entity.values) {
                if (value.value_name == (isLeftHandWeapon ? "left_weapon_mtx" : "right_weapon_mtx") && value.frozen) {
                    weaponMtx = std::get<BEMatrix34>(value.value);
                    writeMemory(hCPU->gpr[5], &weaponMtx);
                }
                else if (value.value_name == (isLeftHandWeapon ? "left_player_mtx" : "right_player_mtx") && value.frozen) {
                    playerMtx = std::get<BEMatrix34>(value.value);
                    writeMemory(hCPU->gpr[6], &playerMtx);
                }
                else if (value.value_name == (isLeftHandWeapon ? "left_ModelBindInfo_mtx" : "right_ModelBindInfo_mtx") && value.frozen) {
                    modelBindInfoMtx = std::get<BEMatrix34>(value.value);
                    writeMemory(hCPU->gpr[7], &modelBindInfoMtx);
                }
            }
        }
#endif
    }
}

void CemuHooks::hook_modifyHandModelAccessSearch(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    if (hCPU->gpr[3] == 0) {
        return;
    }
#ifdef _DEBUG
    // r3 holds the address of the string to search for
    const char* actorName = (const char*)(s_memoryBaseAddress + hCPU->gpr[3]);

    if (actorName != nullptr) {
        // Weapon_R is presumably his right hand bone name
        Log::print("Searching for model handle using {}", actorName);
    }
#endif
}

void CemuHooks::hook_CreateNewActor(PPCInterpreter_t* hCPU) {
    hCPU->instructionPointer = hCPU->sprNew.LR;

    // if (VRManager::instance().XR->GetRenderer() == nullptr || VRManager::instance().XR->GetRenderer()->m_layer3D.GetStatus() == RND_Renderer::Layer3D::Status3D::UNINITIALIZED) {
    //     hCPU->gpr[3] = 0;
    //     return;
    // }
    hCPU->gpr[3] = 0;

    // OpenXR::InputState inputs = VRManager::instance().XR->m_input.load();
    // if (!inputs.inGame.in_game) {
    //     hCPU->gpr[3] = 0;
    //     return;
    // }
    //
    // // test if controller is connected
    // if (inputs.inGame.grab[OpenXR::EyeSide::LEFT].currentState == XR_TRUE && inputs.inGame.grab[OpenXR::EyeSide::LEFT].changedSinceLastSync == XR_TRUE) {
    //     Log::print("Trying to spawn new thing!");
    //     hCPU->gpr[3] = 1;
    // }
    // else if (inputs.inGame.grab[OpenXR::EyeSide::RIGHT].currentState == XR_TRUE && inputs.inGame.grab[OpenXR::EyeSide::RIGHT].changedSinceLastSync == XR_TRUE) {
    //     Log::print("Trying to spawn new thing!");
    //     hCPU->gpr[3] = 1;
    // }
    // else {
    //     hCPU->gpr[3] = 0;
    // }
}
