[BetterVR_Hands_V208]
moduleMatches = 0x6267BFD0

.origin = codecave


str_WeaponR:
.string "Weapon_R"

str_Head:
.string "Weapon_R"

str_head_SafeStringStart:
.int str_Head
.int 0x10263910


logModelAccessSearch:
mflr r0
stwu r1, -0x0C(r1)
stw r0, 0x10(r1)

stw r3, 0x08(r1)
stw r4, 0x04(r1)

; compare r3 with "Weapon_R"
lwz r3, 0x00(r5)
lis r4, str_WeaponR@ha
addi r4, r4, str_WeaponR@l
bl _compareString
bne exit_logModelAccessSearch


; log model access search
replaceAndLog:
lwz r3, 0x00(r5)
bl import.coreinit.hook_ModifyHandModelAccessSearch

lis r5, str_head_SafeStringStart@ha
addi r5, r5, str_head_SafeStringStart@l
mr r30, r5


exit_logModelAccessSearch:
lwz r3, 0x08(r1)
lwz r4, 0x04(r1)

lwz r0, 0x10(r1)
mtlr r0
addi r1, r1, 0x0C

; original instruction
mr r29, r4
blr


0x0398865C = bla logModelAccessSearch
