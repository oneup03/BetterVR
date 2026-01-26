[BetterVR_FirstPersonMode_Events_V208]
moduleMatches = 0x6267BFD0

.origin = codecave

0x1046D3AC = EventMgr__sInstance:
0x031CA1C0 = EventMgr__getActiveEventName:

vr_updateSettings:
mflr r0
stwu r1, -0x20(r1)
stw r0, 0x24(r1)
stw r3, 0x1C(r1)
stw r4, 0x18(r1)
stw r5, 0x14(r1)
stw r6, 0x10(r1)

lis r6, data_TableOfCutsceneEventsSettings@ha
addi r6, r6, data_TableOfCutsceneEventsSettings@l
bl import.coreinit.hook_UpdateSettings
lwz r5, 0x14(r1)
lwz r6, 0x10(r1)

; get event and entrypoint strings
lis r3, EventMgr__sInstance@ha
lwz r3, EventMgr__sInstance@l(r3)
cmpwi r3, 0
beq skipGetEventName

; get active event name
lis r3, EventMgr__getActiveEventName@ha
addi r3, r3, EventMgr__getActiveEventName@l
mtctr r3
li r3, 0
addi r5, r1, 0x0C ; ptr to store entrypoint name
stw r3, 0x0C(r1)
addi r4, r1, 0x08 ; ptr to store event name
stw r3, 0x08(r1)
lis r3, EventMgr__sInstance@ha
lwz r3, EventMgr__sInstance@l(r3)
bctrl ; bl EventMgr::getActiveEventName

; call C++ hook to handle the results
lwz r4, 0x08(r1) ; event name
lwz r5, 0x0C(r1) ; entrypoint name
bla import.coreinit.hook_GetEventName

skipGetEventName:
; ; spawn check
; li r3, 0
; bl import.coreinit.hook_CreateNewActor
; cmpwi r3, 1
; bne notSpawnActor
; ;bl vr_spawnEquipment
; notSpawnActor:
; 
; ;bl checkIfDropWeapon
lwz r6, 0x10(r1)
lwz r5, 0x14(r1)
lwz r4, 0x18(r1)
lwz r3, 0x1C(r1)
lwz r0, 0x24(r1)
addi r1, r1, 0x20
mtlr r0

li r4, -1 ; Execute the instruction that got replaced
blr

0x031FAAF0 = bla vr_updateSettings

;0x031CA268 = ba import.coreinit.hook_GetEventName
;0x031CA288 = ba import.coreinit.hook_GetEventName