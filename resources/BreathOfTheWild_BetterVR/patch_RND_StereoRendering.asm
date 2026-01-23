[BetterVR_UpdateCamera_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

; ==================================================================================
; How Stereo Rendering is implemented in this file:
; - custom_sead_GameFramework_procFrame is modified to ran all the render & update steps twice, one for each eye. It changes currentEyeSide to 0 or 1 depending on the eye side.
; - custom_GameScene_calcAndRunStateMachine, hook_sead_MethodTreeNode_callRec and actor jobs functions (see patch_StereoRendering_ActorJobs.asm) filter the code & calls that would change the game state when currentEyeSide is 1.
;   - one major issue that the Actor::update() is also filtered, but Actor::update() is also responsible for requesting a draw and adding themselves into the gsys::ModelSceneContext. However, Actor::update() also requests the actor to be drawn, which
; - preventUnrequestingDraw and preventModelQueueClear are used to prevent the game from clearing the model queue at the start of the right eye rendering. This way they're still rendered despite the Actor::update() being filtered.
; - agl__lyr__Layer__getRenderCamera and agl__lyr__Layer__getRenderProjection are hooked to modify the draw calls for the left and right eye. The modifified camera and projection matrix data is computed by combining from OpenXR and the game's camera

; This modification causes the game to do all the draw calls for all actors, and then present that to the regular screen twice. The Vulkan layer waits for Cemu to draw to the regular screen twice, after which it has obtained both the rendered images for both eyes.
; Its not as optimized as it could be since Cemu has to translate the draw calls twice which is usually the bottleneck for emulation, but it provides a great stable image which can later be interpolated so that performance is less of an issue.

currentEyeSide:
.int 0

; This is just a 0-1 counter. This is enough to have a two in-flight frames. Could be expanded, but BotW is fine with two.
currentFrameCounter:
.int 0

0x10463EB0 = FadeProgress__sInstance:
0x031FB1B4 = sub_31FB1B4_getTimeForGameUpdateMaybe:
0x0309F72C = sead_GameFramework_lockFrameDrawContext:
0x0309F744 = sead_GameFramework_unlockFrameDrawContext:
0x030A27CC = sead__TaskMgr__calcDestruction:
0x031FAF00 = calculateUIMaybe:
0x031FA97C = jumpToFPSPlusPlus:


; sead::GameFrameworkCafe::presentAndDrawDone outputs the frame to the regular screen. We only need that for the right eye.
; this is a simplified version for the left eye, that does enough to keep the game happy but doesn't actually present anything.
simplified_sead_GameFrameworkCafe_presentAndDrawDone:
mflr r0
stwu r1, -0x10(r1)
stw r0, 0x14(r1)
stw r3, 0x0C(r1)
stw r4, 0x08(r1)

; check if drawdone callback is registered
lwz r4, 0x370(r3)
cmpwi r4, 0
beq exit_simplified_sead_GameFrameworkCafe_presentAndDrawDone

bla import.gx2.GX2Flush
lwz r7, 0x370(r31)
mtctr r7
li r3, 1
bctrl ; gsys__SystemTask__waitDrawDoneCallback_ptr(1)

exit_simplified_sead_GameFrameworkCafe_presentAndDrawDone:
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
lwz r0, 0x14(r1)
addi r1, r1, 0x10
mtlr r0
blr

; use GX2WaitTimeStamp(GX2GetLastSubmittedTimeStamp()) to prevent command buffers from being recycled too early before GX2DrawDone is called at the end of the frame for the right eye
storedTimestamp:
.int 0
.int 0

storeLastSubmittedTimeStamp:
mflr r0
stwu r1, -0x10(r1)
stw r0, 0x14(r1)
stw r3, 0x0C(r1)
stw r4, 0x08(r1)

bl import.gx2.GX2GetLastSubmittedTimeStamp
lis r12, storedTimestamp@ha
addi r12, r12, storedTimestamp@l
stw r3, 0(r12)
stw r4, 4(r12)

lwz r0, 0x14(r1)
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
mtlr r0
addi r1, r1, 0x10
blr

waitForLastSubmittedTimeStamp:
mflr r0
stwu r1, -0x10(r1)
stw r0, 0x14(r1)
stw r3, 0x0C(r1)
stw r4, 0x08(r1)

lis r12, storedTimestamp@ha
addi r12, r12, storedTimestamp@l
lwz r3, 0(r12)
lwz r4, 4(r12)
bl import.gx2.GX2WaitTimeStamp

lwz r0, 0x14(r1)
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
mtlr r0
addi r1, r1, 0x10
blr

custom_sead_GameFramework_procFrame:
mflr r0
stwu r1, -0x10(r1)
stw r30, 8(r1)
mr r30, r3
stw r31, 0xC(r1)
stw r0, 0x14(r1)
lis r3, sub_31FB1B4_getTimeForGameUpdateMaybe@ha
addi r3, r3, sub_31FB1B4_getTimeForGameUpdateMaybe@l
mtctr r3
addi r3, r30, 0x388
bctrl ; bl sub_31FB1B4
lis r3, sead_GameFramework_lockFrameDrawContext@ha
addi r3, r3, sead_GameFramework_lockFrameDrawContext@l
mtctr r3
mr r3, r30
bctrl ; bl sead_GameFramework_lockFrameDrawContext
lis r3, sead__TaskMgr__calcDestruction@ha
addi r3, r3, sead__TaskMgr__calcDestruction@l
mtctr r3
lwz r3, 0x1C(r30)
bctrl ; bl sead__TaskMgr__calcDestruction
lis r3, calculateUIMaybe@ha
addi r3, r3, calculateUIMaybe@l
mtctr r3
bctrl ; bl calculateUIMaybe

; ========================================================================
; FIRST EYE SIDE
; ========================================================================

lwz r12, 0(r30)
lwz r0, 0xF4(r12)
mtctr r0
mr r3, r30
bctrl ; sead__GameFrameworkCafe__procDraw

; doesn't seem to be necessary
;bl import.gx2.GX2DrawDone

li r0, 0
lis r12, currentEyeSide@ha
stw r0, currentEyeSide@l(r12)
li r3, 1
bl import.coreinit.hook_EndCameraSide

; increase frame counter
lis r12, currentFrameCounter@ha
lwz r3, currentFrameCounter@l(r12)
addi r3, r3, 1
cmpwi r3, 2 ; limit to 2 in-flight frames (so 0 and 1) for now
bne skip_resetFrameCounter
li r3, 0
skip_resetFrameCounter:
stw r3, currentFrameCounter@l(r12)

; start rendering for the left eye
li r0, 0
lis r12, currentEyeSide@ha
stw r0, currentEyeSide@l(r12)
li r3, 0
bl import.coreinit.hook_BeginCameraSide

lwz r12, 0(r30)
lwz r11, 0xFC(r12)
mtctr r11
mr r3, r30
bctrl ; sead__GameFrameworkCafe__calcDraw

lwz r12, 0(r30)
lwz r0, 0x7C(r12)
mtctr r0
mr r3, r30
bctrl ; sead__Framework__procReset

lwz r12, 0(r30)
lwz r12, 0x10C(r12)
mtctr r12
mr r3, r30
bl simplified_sead_GameFrameworkCafe_presentAndDrawDone
;bctrl ; sead__GameFrameworkCafe__presentAndDrawDone

; ========================================================================
; SECOND EYE SIDE
; ========================================================================

lwz r12, 0(r30)
lwz r0, 0xF4(r12)
mtctr r0
mr r3, r30
bctrl ; sead__GameFrameworkCafe__procDraw

; note: doesn't seem to be necessary
bl storeLastSubmittedTimeStamp
bl waitForLastSubmittedTimeStamp
;bl import.gx2.GX2DrawDone

stallDuringLoadingScreens_one:
lis r3, FadeProgress__sInstance@ha
lwz r3, FadeProgress__sInstance@l(r3)
cmpwi r3, 0
beq skipStallDuringLoadingScreens_one

lbz r3, 0x10(r3)
cmpwi r3, 0
beq skipStallDuringLoadingScreens_one

mr r11, r4
li r3, 0
li r4, 100
bla import.coreinit.OSSleepTicks
mr r4, r11

; also use gx2drawdone during loading screens to ensure rendering is done
bl import.gx2.GX2DrawDone
skipStallDuringLoadingScreens_one:

li r3, 0
bl import.coreinit.hook_EndCameraSide


li r0, 1
lis r12, currentEyeSide@ha
stw r0, currentEyeSide@l(r12)
li r3, 1
bl import.coreinit.hook_BeginCameraSide

lwz r12, 0(r30)
lwz r11, 0xFC(r12)
mtctr r11
mr r3, r30
bctrl ; sead__GameFrameworkCafe__calcDraw

lwz r12, 0(r30)
lwz r0, 0x7C(r12)
mtctr r0
mr r3, r30
bctrl ; sead__Framework__procReset

lwz r12, 0x74(r30)
clrlwi. r11, r12, 31
li r31, 1
beq loc_31FA90C
extrwi. r0, r12, 1,30
beq loc_31FA90C
li r31, 0

loc_31FA90C:
lwz r0, 0x4C(r30)
cmpwi r0, 0
clrlwi r31, r31, 24
beq loc_31FA928
mtctr r0
li r3, 1
bctrl ; some lockAndUnlockFrameFunc call

loc_31FA928:
cmpwi r31, 0
beq loc_31FA944
lwz r12, 0(r30)
lwz r12, 0x10C(r12)
mtctr r12
mr r3, r30
bctrl ; sead__GameFrameworkCafe__presentAndDrawDone

loc_31FA944:
lwz r0, 0x4C(r30)
cmpwi r0, 0
beq loc_31FA95C
mtctr r0
li r3, 0
bctrl ; lockOrUnlockDrawContextMgr

loc_31FA95C:

; ========================================================================
; ========================================================================

stallDuringLoadingScreens_two:
lis r3, FadeProgress__sInstance@ha
lwz r3, FadeProgress__sInstance@l(r3)
cmpwi r3, 0
beq skipStallDuringLoadingScreens_two

lbz r3, 0x10(r3)
cmpwi r3, 0
beq skipStallDuringLoadingScreens_two

mr r11, r4
li r3, 0
li r4, 100
bla import.coreinit.OSSleepTicks
mr r4, r11

; also use gx2drawdone during loading screens to ensure rendering is done
bl import.gx2.GX2DrawDone
skipStallDuringLoadingScreens_two:

continueWithRendering:
; regular continue code below
lis r3, sead_GameFramework_unlockFrameDrawContext@ha
addi r3, r3, sead_GameFramework_unlockFrameDrawContext@l
mtctr r3
mr r3, r30
bctrl ; bl sead::GameFramework::unlockFrameDrawContext((void))
bl import.coreinit.OSGetSystemTime
lwz r11, 0x84(r30)
lwz r0, 0x80(r30)
subfc r12, r11, r4
subfe r0, r0, r3
stw r12, 0x7C(r30)
;stw r0, 0x78(r30)

; todo: this is a hacky way of calling the FPS++ function, since we can't reference it directly
lis r12, jumpToFPSPlusPlus@ha
addi r12, r12, jumpToFPSPlusPlus@l
mtctr r12
lwz r12, 0x7C(r30) ; revert register steal
bctr ; jump to FPS++ function

bl import.coreinit.OSGetSystemTime
lwz r11, 0x28(r30)
stw r4, 0x84(r30)
cmpwi r11, 1
stw r3, 0x80(r30)
lwz r12, 0x74(r30)
bne loc_31FA9A4
li r0, 2
stw r0, 0x28(r30)

loc_31FA9A4:
clrlwi. r0, r12, 31
beq loc_31FA9B4
xori r0, r12, 2
stw r0, 0x74(r30)

loc_31FA9B4:
lwz r12, 0(r30)
lwz r0, 0x104(r12)
mtctr r0
mr r3, r30
; prevent vsync since OpenXR really wants to control the frame timing
;bctrl ; waitForVsync
lwz r0, 0x14(r1)
lwz r30, 0x08(r1)
mtlr r0
lwz r31, 0x0C(r1)
addi r1, r1, 0x10
blr

0x031FA880 = ba custom_sead_GameFramework_procFrame


; disable DOF since it seems to cause crashes at 0x039da5bc (lwz r0, 0x3C(r6)) when going in/out of your inventory (edit: it doesn't fix the crashes :/)
;0x039DA570 = li r3, 0
; draw bloom shit
; 0x03AA1224 = lwz r6, 0x1C(r23)

; disable DOF
;0x039AAC4C = nop
;0x039AAC88 = nop
;0x039AACC8 = nop
;0x039AAD2C = nop


; ==================================================================================
; ==================================================================================


0x02C57E4C = uking__frm__System__preCalc:
0x02C57E7C = calcAndRunStateMachine:
0x02C57ED8 = uking__frm__System__postCalc:

0x10463E7C = byte_10463E7C:
0x03414828 = gameScene__preCalc_:


0x0340EEDC = ksys__CalcPlacementMgr:
0x1047C370 = HavokWorkerMgr__sInstance:
0x1046D594 = MCMgr__sInstance:
0x031FCE6C = MCMgr__calcPostBgBaseProcMgr:
0x034155E4 = pushJobQueue2:
0x03415600 = ksys__PreCalcWorldPre:
0x03415584 = requestLODMgrModelsAndUpdateDebugInput:
0x03414D74 = runActorUpdateStuff:


0x10463E64 = GameScene_sBinderStatus:
0x02C4FAD4 = StageBinder_getStage:
0x101C1474 = stru_101C1474:

custom_PostCalc:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)
stw r7, 0x0C(r1)

lwz r3, 0x1C(r1)
lwz r3, 0x200(r3)
cmpwi r3, 0
beq skip_custom_PostCalc

lis r6, GameScene_sBinderStatus@ha
lwz r6, GameScene_sBinderStatus@l(r6)
cmpwi r6, 2
beq skip_custom_PostCalc

; get stage
lis r6, StageBinder_getStage@ha
addi r6, r6, StageBinder_getStage@l
mtctr r6
;lwz r3, 0x1C(r1)
bctrl ; bl StageBinder_getStage

; check if stage is valid
cmpwi r3, 0
beq skip_custom_PostCalc

lis r5, stru_101C1474@ha
addi r5, r5, stru_101C1474@l
stw r5, 0x08(r1) ; store callback vtable on stack
addi r4, r1, 0x08

0x02C44944 = MainFieldDungeonStage__postCalc:
lwz r6, 0x00(r3) ; stage->vtable
lwz r6, 0x4C(r6) ; get Stage::postCalc function pointer
lis r7, MainFieldDungeonStage__postCalc@ha
addi r7, r7, MainFieldDungeonStage__postCalc@l
cmpw r6, r7
bne skip_custom_PostCalc

; run ONE function of the stage's postCalc
0x02C42FF0 = BackgroundCamera_postCalc:
lis r7, BackgroundCamera_postCalc@ha
addi r7, r7, BackgroundCamera_postCalc@l
mtctr r7
bctrl ; bl BackgroundCamera::postCalc

; lwz r6, 0x00(r3) ; stage->vtable
; lwz r6, 0x4C(r6) ; get Stage::postCalc function pointer
; mtctr r6
; bctrl ; bl Stage::postCalc

skip_custom_PostCalc:
lwz r7, 0x0C(r1)
lwz r6, 0x10(r1)
lwz r5, 0x14(r1)
lwz r4, 0x18(r1)
lwz r3, 0x1C(r1)
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0
blr


custom_GameScene_calcAndRunStateMachine:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x0C(r1) ; stores System*
stw r4, 0x08(r1) ; stores Framework*
stw r5, 0x04(r1)

lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
cmpwi r3, 1
beq dontRunStateMachine

; for eye side 0, just run the regular code
lis r3, uking__frm__System__preCalc@ha
addi r3, r3, uking__frm__System__preCalc@l
mtctr r3
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
lwz r5, 0x04(r1)
bctrl

lis r3, calcAndRunStateMachine@ha
addi r3, r3, calcAndRunStateMachine@l
mtctr r3
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
lwz r5, 0x04(r1)
bctrl

lis r3, uking__frm__System__postCalc@ha
addi r3, r3, uking__frm__System__postCalc@l
mtctr r3
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
lwz r5, 0x04(r1)
bctrl

b continuecalcAndRunStateMachine__run

dontRunStateMachine:
; HERE WE RUN OUR CUSTOM CODE

;lis r3, uking__frm__System__preCalc@ha
;addi r3, r3, uking__frm__System__preCalc@l
;mtctr r3
;lwz r3, 0x0C(r1)
;lwz r4, 0x08(r1)
;lwz r5, 0x04(r1)
;bctrl

; ===== PRE CALC STATE MACHINE =====
lis r3, byte_10463E7C@ha
lbz r3, byte_10463E7C@l(r3)
;bl gameScene__preCalc_


; lis r3, requestLODMgrModelsAndUpdateDebugInput@ha
; addi r3, r3, requestLODMgrModelsAndUpdateDebugInput@l
; mtctr r3
; bctrl ; bl requestLODMgrModelsAndUpdateDebugInput

; 0x031FBA24 = MCMgr__preCalc:
; lis r3, MCMgr__preCalc@ha
; addi r3, r3, MCMgr__preCalc@l
; mtctr r3
; lis r3, MCMgr__sInstance@ha
; lwz r3, MCMgr__sInstance@l(r3)
; bctrl ; bl MCMgr__preCalc

; ===== RUN STATE MACHINE =====

; lis r3, ksys__PreCalcWorldPre@ha
; addi r3, r3, ksys__PreCalcWorldPre@l
; mtctr r3
; bctrl ; bl ksys__PreCalcWorldPre

;0x03417E5C = teraStuff:
;lis r3, teraStuff@ha
;addi r3, r3, teraStuff@l
;mtctr r3
;bctrl ; bl teraStuff

; run ksys::PreCalcWorldPre things
; lis r3, ksys__CalcPlacementMgr@ha
; addi r3, r3, ksys__CalcPlacementMgr@l
; mtctr r3
; bctrl ; bl ksys__CalcPlacementMgr

; this runs all actor related update code + uses gsys::Model::requestDraw() which is needed to get the models to render
; ; run MCMgr__calcPostBgBaseProcMgr(MCMgr::sInstance)
; lis r3, MCMgr__calcPostBgBaseProcMgr@ha
; addi r3, r3, MCMgr__calcPostBgBaseProcMgr@l
; mtctr r3
; lis r3, MCMgr__sInstance@ha
; lwz r3, MCMgr__sInstance@l(r3)
; bctrl ; bl MCMgr__calcPostBgBaseProcMgr

; lis r3, runActorUpdateStuff@ha
; addi r3, r3, runActorUpdateStuff@l
; mtctr r3
; bctrl ; bl runActorUpdateStuff

; lis r3, gsys__SystemTask__invokeCalcFrame_@ha
; addi r3, r3, gsys__SystemTask__invokeCalcFrame_@l
; mtctr r3
; lis r3, gsys__SystemTask__sInstance_0@ha
; lwz r3, gsys__SystemTask__sInstance_0@l(r3)
; bctrl ; bl gsys__SystemTask__invokeCalcFrame_
; ;bla gsys__SystemTask__invokeCalcFrame_

; lis r3, pushJobQueue2@ha
; addi r3, r3, pushJobQueue2@l
; mtctr r3
; li r3, 2
; bctrl ; bl pushJobQueue2

; 0x031FD360 = MCMgr__calc:
; lis r3, MCMgr__calc@ha
; addi r3, r3, MCMgr__calc@l
; mtctr r3
; lis r3, MCMgr__sInstance@ha
; lwz r3, MCMgr__sInstance@l(r3)
; bctrl ; bl MCMgr__calc

; 0x03415CD0 = ksys__calcEntryJob:
;lis r3, ksys__calcEntryJob@ha
;addi r3, r3, ksys__calcEntryJob@l
;mtctr r3
;bctrl ; bl ksys__calcEntryJob

; run gameScene::calcGraphicsStuff(*a2)
0x03416590 = gameScene__calcGraphicsStuff:

;lis r3, gameScene__calcGraphicsStuff@ha
;addi r3, r3, gameScene__calcGraphicsStuff@l
;mtctr r3
;lwz r3, 0x0C(r1)
;lwz r4, 0x08(r1)
;lwz r5, 0x04(r1)
;lwz r3, 0x0(r4)
;;bctrl
;
;lis r3, uking__frm__System__postCalc@ha
;addi r3, r3, uking__frm__System__postCalc@l
;mtctr r3
;lwz r3, 0x0C(r1)
;lwz r4, 0x08(r1)
;lwz r5, 0x04(r1)
;bctrl

; load system and framework pointers
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
lwz r5, 0x04(r1)
; store callback pointer
lwz r5, 0x0(r4)
addi r4, r1, 0x18
stw r5, 0x18(r1)

lwz r3, 0x08(r3) ; uking::frm::Scene->gameScene

bl custom_PostCalc


continuecalcAndRunStateMachine__run:
lwz r0, 0x24(r1)
mtlr r0
lwz r3, 0x0C(r1)
lwz r4, 0x08(r1)
lwz r5, 0x04(r1)
addi r1, r1, 0x20
blr

; hook the precall to run our custom code which runs the pre, run and post calc
0x02C4BFE0 = bl custom_GameScene_calcAndRunStateMachine
0x02C4BFEC = nop
0x02C4BFF8 = nop

; ==================================================================================
; ==================================================================================
; ==================================================================================
; ==================================================================================

0x03A14D74 = sead__Delegate_gsys__SystemTask___invoke:
0x030A0594 = sead__Delegate__RootTaskAndControllerMgr__invoke:
0x0309D890 = sead__Delegate__Root38AndControllerMgr__invoke:

; 0x02DAFABC = NOT USED
; 0x037D4B40 = NOT USED
; 0x0320549C = NOT USED
; 0x030A2C00 = NOT USED

str_callRec_didCall:
.string "sead::MethodTreeNode::callRec( %08X ) did get called"

str_callRec_didNotCall:
.string "sead::MethodTreeNode::callRec( %08X ) did not get called"

hook_sead_MethodTreeNode_callRec:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)

lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
cmpwi r3, 0
beq doCallRec

lis r3, sead__Delegate_gsys__SystemTask___invoke@ha
addi r3, r3, sead__Delegate_gsys__SystemTask___invoke@l
cmpw r10, r3
beq doCallRec


lis r3, sead__Delegate__RootTaskAndControllerMgr__invoke@ha
addi r3, r3, sead__Delegate__RootTaskAndControllerMgr__invoke@l
cmpw r10, r3
beq doCallRec

;b doCallRec
b dontCallRec

doCallRec:
; --- LOGGING ---
stwu r1, -0x20(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)
mr r6, r10
lis r5, str_callRec_didCall@ha
addi r5, r5, str_callRec_didCall@l
bl printToCemuConsoleWithFormat
lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r6, 0x10(r1)
addi r1, r1, 0x20
; --- LOGGING ---

lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
bctrl

b continue_callRec

dontCallRec:
; --- LOGGING ---
stwu r1, -0x20(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)
mr r6, r10
lis r5, str_callRec_didNotCall@ha
addi r5, r5, str_callRec_didNotCall@l
bl printToCemuConsoleWithFormat
lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r6, 0x10(r1)
addi r1, r1, 0x20
; --- LOGGING ---

b continue_callRec

continue_callRec:
lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x0309FA60 = bl hook_sead_MethodTreeNode_callRec

; ==================================================================================
; ==================================================================================
; ==================================================================================
; ==================================================================================


preventUnrequestingDraw:
lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
cmpwi r3, 1
beq keepRequestDraw

unsetRequestDraw:
ori r11, r11, 2

keepRequestDraw:
stb r11, 0x7D(r31)
li r3, 1
blr

0x03987CA4 = bla preventUnrequestingDraw


0x03A2406C = gsys__ModelJobQueue__clear:

preventModelQueueClear:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)

lis r3, currentEyeSide@ha
lwz r3, currentEyeSide@l(r3)
cmpwi r3, 1
beq exit_preventModelQueueClear

lis r3, gsys__ModelJobQueue__clear@ha
addi r3, r3, gsys__ModelJobQueue__clear@l
mtctr r3

lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
bctrl

exit_preventModelQueueClear:
lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r0, 0x24(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x039A8D80 = bla preventModelQueueClear

; ==================================================================================
; ==================================================================================

const_0:
.float 0.0
0x10323118 = const_epsilon:

0x105978C4 = sDefaultCameraMatrix:

modifiedCopy_seadLookAtCamera:
.float 0,0,0,0  ; mtx
.float 0,0,0,0
.float 0,0,0,0
.int 0x123 ; vtable
.float 10,2,-20 ; pos
.float 10,4,-19 ; target
.float 0,1,0    ; up

testForOverflow:
.int 0x60000000

agl__lyr__Layer__getRenderCamera:
lhz r0, 0x52(r3)
clrlwi. r12, r0, 31
bne testCameraFlags
extrwi. r11, r0, 1,26
beq returnCameraFrom1stParam

testCameraFlags:
extrwi. r11, r0, 1,30
beq returnCameraFrom1stParam
lwz r12, 0x19C(r3)
cmpwi r12, 0
bne returnDefaultCamera

returnCameraFrom1stParam:
lwz r0, 0x48(r3)
lis r3, sDefaultCameraMatrix@ha
addi r3, r3, sDefaultCameraMatrix@l
cmpwi r0, 0
beqlr
mr r3, r0

; camera.pos.x.getLE() != 0.0f
lis r12, const_0@ha
lfs f12, const_0@l(r12)
lfs f13, 0x34(r3)
fcmpu cr0, f12, f13
beqlr

; std::fabs(camera.at.z.getLE()) < std::numeric_limits<float>::epsilon()
fmr f13, f1
lfs f1, 0x48(r3)
.int 0xFC200A10 ; fabs f1, f1
fmr f12, f1
fmr f1, f13
lis r12, const_epsilon@ha
lfs f13, const_epsilon@l(r12)
fcmpu cr0, f12, f13
bltlr

lis r12, useStubHooks@ha
lwz r12, useStubHooks@l(r12)
cmpwi r12, 1
lis r12, stub_hook_GetRenderCamera@ha
addi r12, r12, stub_hook_GetRenderCamera@l
beq getRenderCamera
lis r12, import.coreinit.hook_GetRenderCamera@ha
addi r12, r12, import.coreinit.hook_GetRenderCamera@l
getRenderCamera:
mtctr r12

lis r11, currentEyeSide@ha
lwz r11, currentEyeSide@l(r11)
lis r12, modifiedCopy_seadLookAtCamera@ha
addi r12, r12, modifiedCopy_seadLookAtCamera@l
;beq stub_hook_GetRenderCamera
;ba import.coreinit.hook_GetRenderCamera
bctr ; jump to hook_GetRenderCamera
blr

returnDefaultCamera:
addi r3, r12, 4
blr

0x03AE4AA0 = ba agl__lyr__Layer__getRenderCamera


modifiedCopy_seadPerspectiveProjection:
.byte 0,0,0,0 ; dirty, deviceDirty, pad, pad
.int 0 ; devicePosture
.float 0,0,0,0 ; mtx
.float 0,0,0,0
.float 0,0,0,0
.float 0,0,0,0
.float 0,0,0,0 ; deviceMtx
.float 0,0,0,0
.float 0,0,0,0
.float 0,0,0,0
.float 0 ; deviceZScale
.float 0 ; deviceZOffset
.int 0x1027B54C ; vtable
.float 0.1 ; near
.float 25000 ; far
.float 0.8726647 ; angle
.float 0.4226183 ; fovySin
.float 0.9063078 ; fovyCos
.float 0.4663077 ; fovyTan
.float 1.7777778 ; aspect
.float 0 ; offsetX
.float 0 ; offsetY


0x1027B54C = seadPerspectiveProjection_vtbl:
0x10597928 = sDefaultSeadProjection:

agl__lyr__Layer__getRenderProjection:
lhz r0, 0x54(r3)
extrwi. r0, r0, 1,30
beq returnProjectionFrom1stParam
lwz r12, 0x19C(r3)
cmpwi r12, 0
bne useSpecialProjection

returnProjectionFrom1stParam:
lwz r12, 0x4C(r3)
lis r3, sDefaultSeadProjection@ha
addi r3, r3, sDefaultSeadProjection@l
cmpwi r12, 0
beqlr
mr r3, r12

; prevent modifying anything but sead::PerspectiveProjection
lwz r12, 0x90(r3)
lis r11, seadPerspectiveProjection_vtbl@ha
addi r11, r11, seadPerspectiveProjection_vtbl@l
cmpw r12, r11
bnelr

lis r12, useStubHooks@ha
lwz r12, useStubHooks@l(r12)
cmpwi r12, 1
lis r12, stub_hook_GetRenderProjection@ha
addi r12, r12, stub_hook_GetRenderProjection@l
beq getRenderProjection
lis r12, import.coreinit.hook_GetRenderProjection@ha
addi r12, r12, import.coreinit.hook_GetRenderProjection@l
getRenderProjection:
mtctr r12

lis r12, currentEyeSide@ha
lwz r0, currentEyeSide@l(r12)
lis r12, modifiedCopy_seadPerspectiveProjection@ha
addi r12, r12, modifiedCopy_seadPerspectiveProjection@l
bctr ; jump to hook_GetRenderProjection
blr

useSpecialProjection:
lwz r3, 0xEC(r12)
blr

0x03AE4AEC = ba agl__lyr__Layer__getRenderProjection

; =================================================================================
; most rendering functions use getRenderProjection to get the projection matrix
; however, the light pre-pass ALSO uses sead::Projection::getProjectionMatrix
; the patch below fixes the lights not lighting up objects/walls correctly in VR
; =================================================================================

0x030C0F4C = sead_projection_updateMatrixImpl:
0x033DBBB8 = returnAddress_lightPrePassProjectionMatrix:

custom_sead__Projection__getProjectionMatrix:
mflr r0
stwu r1, -0x20(r1)
stw r31, 0x0C(r1)
stw r0, 0x24(r1)
stw r11, 0x18(r1)
stw r12, 0x14(r1)

; random crash fix attempt
cmpwi r3, 0
beq exit_custom_sead__Projection__getProjectionMatrix

; prevent modifying anything but sead::PerspectiveProjection
lwz r12, 0x90(r3)
lis r11, seadPerspectiveProjection_vtbl@ha
addi r11, r11, seadPerspectiveProjection_vtbl@l
cmpw r12, r11
bne continue_sead__Projection__getProjectionMatrix

; only hook the light pre-pass projection matrix change
lis r11, returnAddress_lightPrePassProjectionMatrix@ha
addi r11, r11, returnAddress_lightPrePassProjectionMatrix@l
cmpw r0, r11
bne continue_sead__Projection__getProjectionMatrix

; call C++ code to modify the projection matrix to use the VR projection matrices for each eye
lis r11, currentEyeSide@ha
lwz r11, currentEyeSide@l(r11)
bla import.coreinit.hook_ModifyLightPrePassProjectionMatrix

continue_sead__Projection__getProjectionMatrix:
lis r31, sead_projection_updateMatrixImpl@ha
addi r31, r31, sead_projection_updateMatrixImpl@l
mtctr r31
mr r31, r3
bctrl ; bl sead::Projection::updateMatrix
addi r3, r31, 4

exit_custom_sead__Projection__getProjectionMatrix:
lwz r12, 0x14(r1)
lwz r11, 0x18(r1)
lwz r0, 0x24(r1)
lwz r31, 0x0C(r1)
mtlr r0
addi r1, r1, 0x20
blr

0x030C1008 = ba custom_sead__Projection__getProjectionMatrix

; =================================================================================

0x030C1958 = ba import.coreinit.hook_OverwriteSeadPerspectiveProjectionSet

0x03191BA0 = Camera__getPerspectiveProjection:
0x030C18F0 = sead__PerspectiveProjection__set:

custom_ModifyProjectionUsingCamera:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x08(r1)
stw r7, 0x04(r1)
stw r30, 0x10(r1)
stw r31, 0x0C(r1)

mr. r31, r4
mr r30, r3
beq exit_custom_ModifyProjectionUsingCamera

lis r3, Camera__getPerspectiveProjection@ha
addi r3, r3, Camera__getPerspectiveProjection@l
mtctr r3
lis r3, Camera__sInstance@ha
lwz r3, Camera__sInstance@l(r3)
bctrl ; bl Camera__getPerspectiveProjection

lwz r12, 0x90(r3)
lwz r0, 0x3C(r12)
mtctr r0
bctrl ; sead__PerspectiveProjection__getAspect

lfs f3, 0x68(r30)
fmr f4, f1
lfs f2, 0x64(r30)
lfs f1, 0x60(r30)
lis r3, sead__PerspectiveProjection__set@ha
addi r3, r3, sead__PerspectiveProjection__set@l
mtctr r3
mr r3, r31
bctrl ; bl sead__PerspectiveProjection__set
lfs f0, 0x6C(r30)
li r11, 1
stfs f0, 0xB0(r31)
lfs f0, 0x70(r30)
stb r11, 0(r31)
stfs f0, 0xB4(r31)

;lwz r3, 0x1C(r1)
;lwz r3, 0x30(r3) ; load vtable address
;lwz r3, 0x24(r3) ; load updateMatrix address
;mtctr r3
;lwz r3, 0x1C(r1)
;mr r4, r3
;bctrl ; bl sead::Projection::updateMatrix

lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lis r5, currentEyeSide@ha
lwz r5, currentEyeSide@l(r5)
lwz r6, 0x24(r1)

lwz r7, 0x10(r1)
lwz r7, 0x128(r7)

bla import.coreinit.hook_ModifyProjectionUsingCamera

exit_custom_ModifyProjectionUsingCamera:
lwz r3, 0x1C(r1)
lwz r4, 0x18(r1)
lwz r5, 0x14(r1)
lwz r6, 0x08(r1)
lwz r7, 0x04(r1)
lwz r30, 0x10(r1)
lwz r31, 0x0C(r1)
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0
blr

0x02C43450 = bla custom_ModifyProjectionUsingCamera

;0x0318FE7C = ba custom_ModifyProjectionUsingCamera
;0x0318FEFC = ba import.coreinit.hook_ModifyProjectionUsingCamera