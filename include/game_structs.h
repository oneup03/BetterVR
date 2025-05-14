#pragma once

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