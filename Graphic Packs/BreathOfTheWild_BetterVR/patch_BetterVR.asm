[BotW_BetterVR_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; Data from the native application
hookStatus:
.byte 1
.align 4
hmdQuatX:
.float 0.0
hmdQuatY:
.float 0.0
hmdQuatZ:
.float 0.0
hmdQuatW:
.float 0.0
hmdPosX:
.float 0.0
hmdPosY:
.float 0.0
hmdPosZ:
.float 0.0

; Inputs for the calculation code cave
oldPosX:
.float 0.0
oldPosY:
.float 0.0
oldPosZ:
.float 0.0
oldTargetX:
.float 0.0
oldTargetY:
.float 0.0
oldTargetZ:
.float 0.0
; Output from the calculation code cave
newPosX:
.float 0.0
newPosY:
.float 0.0
newPosZ:
.float 0.0
newTargetX:
.float 0.0
newTargetY:
.float 0.0
newTargetZ:
.float 0.0
newRotX:
.float 0.0
newRotY:
.float 0.0
newRotZ:
.float 0.0

; Additional settings
FOVSetting:
.float $FOV
HeadPositionSensitivitySetting:
.float $headPositionSensitivity
HeightPositionOffsetSetting:
.float $heightPositionOffset


CAM_OFFSET_POS = 0x5C0
CAM_OFFSET_TARGET = 0x5CC

changeCameraMatrix:
lwz r0, 0x1c(r1) ; original instruction

lis r7, hookStatus@ha
lbz r7, hookStatus@l(r7)
cmpwi r7, 3
bltlr

lfs f0, CAM_OFFSET_POS+0x0(r31)
lis r7, oldPosX@ha
stfs f0, oldPosX@l(r7)
lfs f0, CAM_OFFSET_POS+0x4(r31)
lis r7, oldPosY@ha
stfs f0, oldPosY@l(r7)
lfs f0, CAM_OFFSET_POS+0x8(r31)
lis r7, oldPosZ@ha
stfs f0, oldPosZ@l(r7)

lfs f0, CAM_OFFSET_TARGET+0x0(r31)
lis r7, oldTargetX@ha
stfs f0, oldTargetX@l(r7)
lfs f0, CAM_OFFSET_TARGET+0x4(r31)
lis r7, oldTargetY@ha
stfs f0, oldTargetY@l(r7)
lfs f0, CAM_OFFSET_TARGET+0x8(r31)
lis r7, oldTargetZ@ha
stfs f0, oldTargetZ@l(r7)

lis r7, hookStatus@ha
lbz r7, hookStatus@l(r7)
cmpwi r7, 3
beq calcNewRotationWrapper

finishStoreResults:

lis r7, newPosX@ha
lfs f0, newPosX@l(r7)
stfs f0, CAM_OFFSET_POS(r31)
lis r7, newPosY@ha
lfs f0, newPosY@l(r7)
stfs f0, CAM_OFFSET_POS+0x4(r31)
lis r7, newPosZ@ha
lfs f0, newPosZ@l(r7)
stfs f0, CAM_OFFSET_POS+0x8(r31)

lis r7, newTargetX@ha
lfs f0, newTargetX@l(r7)
stfs f0, CAM_OFFSET_TARGET+0x0(r31)
lis r7, newTargetY@ha
lfs f0, newTargetY@l(r7)
stfs f0, CAM_OFFSET_TARGET+0x4(r31)
lis r7, newTargetZ@ha
lfs f0, newTargetZ@l(r7)
stfs f0, CAM_OFFSET_TARGET+0x8(r31)

lis r7, FOVSetting@ha
lfs f0, FOVSetting@l(r7)
stfs f0, 0x5E4(r31)

blr

0x02C05500 = bla changeCameraMatrix
0x02C05598 = bla changeCameraMatrix

changeCameraRotation:
stfs f10, 0x18(r31)

lis r8, hookStatus@ha
lbz r8, hookStatus@l(r8)
cmpwi r8, 3
bltlr

lis r8, newRotX@ha
lfs f10, newRotX@l(r8)
stfs f10, 0x18(r31)

lis r8, newRotY@ha
lfs f10, newRotY@l(r8)
stfs f10, 0x1C(r31)

lis r8, newRotZ@ha
lfs f10, newRotZ@l(r8)
stfs f10, 0x20(r31)

blr

0x02E57FF0 = bla changeCameraRotation


0x101BF8DC = .float $linkOpacity
0x10216594 = .float $cameraDistance