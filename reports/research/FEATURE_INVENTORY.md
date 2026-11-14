# Feature inventory

Audit: main `ce3074f`; 14/11/2026. Verification is scoped to each named test, not a promise for every configuration. Current reflection/graph inspection: UNREAL_INSPECTION.json. Historical runtime files stay labeled by their recorded DLL and fixture.

## F01 — Địa hình Quảng Ninh và lọc collision

| Field | Evidence |
|---|---|
| Status | `KNOWN_LIMITATIONS` |
| Purpose | Giữ ràng buộc đất/bờ mà không tạo sàn va chạm giả trên biển. |
| Implementation | Giữ đất và hình học bờ; loại sea-plane/skirt đặc thù dataset bằng raw coast |
| Source | `Source/VDTPhase2Project/Private/Navigation/SeaTerrainCollisionComponent.cpp` |
| Unreal Assets | QuangNinh_Terrain, WaterBodyOcean |
| Dependencies | Cesium tileset/LOD, raw coast, ProceduralMeshComponent, Chaos. |
| Configuration | bFilterSeaLevelDEM; lưới bờ 25 m |
| User Interaction | Mở MainMapLevel; cấu hình URL terrain/imagery |
| Validation | USER_LAYOUT_SHORELINE_PIE.json |
| Evidence | `Docs/Testing/USER_LAYOUT_SHORELINE_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Dữ liệu và LOD cụ thể; không thay thủy đạc |

## F02 — A* trên chart ECEF/ENU

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Tìm đường nước có giới hạn tính toán và đoạn nối an toàn. |
| Implementation | Lưới tám láng giềng, không cắt góc, clearance cost và string pulling |
| Source | `Source/VDTPhase2Project/Private/Navigation/SeaNavigationMap.cpp` |
| Unreal Assets | halong_navigation.json |
| Dependencies | Chart JSON, clearance/connectivity, ECEF/ENU; caller navigator. |
| Configuration | 75000 expansions; 20 ms mặc định; open set 200000 |
| User Interaction | AI gọi planner qua navigator |
| Validation | NATIVE_BUILD_AND_BLUEPRINTS.json |
| Evidence | `Docs/Testing/NATIVE_BUILD_AND_BLUEPRINTS.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Đường hình học không chứng minh thân có thể quay |

## F03 — Tuần tra tác giả đặt A/B

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Cho phép tác giả tự đặt tuyến demo lặp lại được. |
| Implementation | Preflight ba chặng; arrival rồi hãm/chờ; giữ tuyến tác giả |
| Source | `Source/VDTPhase2Project/Private/Navigation/ShipEnemyNavigationComponent.cpp` |
| Unreal Assets | BP_ShipEnemy; TargetPoint A/B |
| Dependencies | TargetPoint A/B, operational chart, controller AI, physics/arrival guards. |
| Configuration | Instance radius 80 m, wait 3 s |
| User Interaction | GENERATE MANUALLY (A-B) |
| Validation | AUTHORED_SIX_SHIP_PIE.json |
| Evidence | `Docs/Testing/AUTHORED_SIX_SHIP_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Phải đủ clearance, separation, vùng quay |

## F04 — Sinh layout theo seed và sector

| Field | Evidence |
|---|---|
| Status | `KNOWN_LIMITATIONS` |
| Purpose | Tạo nhiều bố trí từ dữ liệu nước và tái lập lựa chọn layout. |
| Implementation | FRandomStream, sector suitability, collision survey, placement/settling |
| Source | `Source/VDTPhase2Project/Private/Subsystems/NavalBattlefieldSubsystem.cpp` |
| Unreal Assets | quangninh_sectors.json; quangninh_demo_routes.json |
| Dependencies | NavalSectorSubsystem, FRandomStream, chart fingerprint, per-hull survey. |
| Configuration | 25x25 ô 100 m; seed >=0 |
| User Interaction | GENERATE FROM SEED |
| Validation | FINAL_SEED_REPLAY.json |
| Evidence | `Docs/Testing/FINAL_SEED_REPLAY.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Không chứng minh mọi seed/hull/tuyến đều hợp lệ |

## F05 — Ba cấu hình thân tàu

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Tách tuning thân tàu khỏi logic điều khiển/vũ khí. |
| Implementation | DataAsset áp profile vật lý, tốc độ, trang bị, HP và damage |
| Source | `Source/VDTPhase2Project/Private/Ships/Physics/ShipPhysicsProfile.cpp` |
| Unreal Assets | DA_SmallShip; DA_MediumShip; DA_LargeShip |
| Dependencies | DataAsset, ShipMovement, Buoyancy, hull/camera/weapon components. |
| Configuration | 90/150/240 kg mô phỏng; enum Small=0 Large=1 Medium=2 |
| User Interaction | Chọn từng slot trong setup |
| Validation | MERGED_MAIN_VERIFICATION.json |
| Evidence | `Docs/Testing/MERGED_MAIN_VERIFICATION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Hai họ hình học; không phải ba model độc lập |

## F06 — Small static mesh và pháo chủ động

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Dùng model Small thật mà giữ quyền ngắm/bắn hoạt động. |
| Implementation | Model static với atlas; rig DDG102 điều khiển các mesh pháo mũi |
| Source | `Source/VDTPhase2Project/Private/Ships/Editor/ShipArchetypeEditor.cpp` |
| Unreal Assets | BP_ShipSmall; BP_ShipSmallAI; sweedenmilitaryboat |
| Dependencies | Static mesh/atlas, DDG102 skeletal driver, gun meshes/socket, Small BPs. |
| Configuration | StaticVisualTransform; WeaponDriverTransform |
| User Interaction | Chọn Small trước Start |
| Validation | MERGED_MAIN_VERIFICATION.json |
| Evidence | `Docs/Testing/MERGED_MAIN_VERIFICATION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Model Small không có skeleton; Fab Standard do người dùng xác nhận |

## F07 — Lực đẩy/bánh lái và giới hạn tốc độ

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Giữ quán tính và lực lái vật lý dưới một đường điều khiển. |
| Implementation | Một pipeline lệnh AI; bộ lọc lực giữ actuator Blueprint |
| Source | `Source/VDTPhase2Project/Private/Ships/Movement/ShipMovementComponent.cpp` |
| Unreal Assets | BP_Ship ApplyAIForward/ApplyAIRudder |
| Dependencies | Original BP forces, Chaos hull, battle action gate, class profile. |
| Configuration | CruiseSpeed; ForwardMaxSpeed; PropulsionScale; RudderScale |
| User Interaction | W/S/A/D hoặc AI |
| Validation | MERGED_MAIN_VERIFICATION.json |
| Evidence | `Docs/Testing/MERGED_MAIN_VERIFICATION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Giới hạn tốc độ không phải bộ tăng lực; tàu có quán tính |

## F08 — Buoyancy và cản nước theo trục

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Giảm lắc mạnh nhưng giữ phản ứng sóng và khả năng rẽ. |
| Implementation | Sync Water; chia tải phao; cản heave/side/roll/pitch thụ động |
| Source | `Source/VDTPhase2Project/Private/Ships/Movement/ShipMovementComponent.cpp` |
| Unreal Assets | BuoyancyComponent; pontoons; profiles |
| Dependencies | Water BuoyancyComponent, pontoons, sprung mass, PrePhysics, Chaos. |
| Configuration | Small roll/pitch 1.5/0.8 s^-1; yaw không khóa |
| User Interaction | Mở profile rồi chạy PIE mới |
| Validation | SMALL_WAVE_FOLLOWUP.json |
| Evidence | `Docs/Testing/SMALL_WAVE_FOLLOWUP.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | 2-3 độ là một số peak pitch quan sát, không biên độ cưỡng ép |

## F09 — Hãm nguy cơ mắc cạn cho player

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Giảm nguy cơ player đẩy keel lên địa hình sát đảo. |
| Implementation | Sweep footprint theo hướng lực; hãm qua actuator, giữ rudder/reverse |
| Source | `Source/VDTPhase2Project/Private/Ships/Movement/ShipMovementComponent.cpp` |
| Unreal Assets | Physical hull; terrain collision |
| Dependencies | Hull sweep, terrain collision, force filter, HUD warning; reverse/rudder retained. |
| Configuration | bPreventPlayerGrounding; horizon 20-180 m |
| User Interaction | Cảnh báo shallow water; rẽ hoặc lùi |
| Validation | USER_LAYOUT_SHORELINE_PIE.json |
| Evidence | `Docs/Testing/USER_LAYOUT_SHORELINE_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Không cứu spawn trên đất; không tự lái |

## F10 — Phát hiện địch, đội và LOS

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Chỉ giao chiến với địch phù hợp và có quan sát hợp lệ. |
| Implementation | Lọc địch còn sống, tầm, visibility; target ổn định |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | ShipWeapon, ShipCombat, hull |
| Dependencies | ShipWeapon.Team(), physical hull, collision/LOS, target lifetime. |
| Configuration | Detection/Engagement/LoseTarget radius |
| User Interaction | AI tự động trong trận |
| Validation | CORE_GAMEPLAY_REGRESSION.json |
| Evidence | `Docs/Testing/CORE_GAMEPLAY_REGRESSION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Bị che không đồng nghĩa có phát bắn hợp lệ |

## F11 — Bắn đón đạn đạo và kiểm tra quỹ đạo

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Tính vận tốc phóng và loại cung bắn không an toàn. |
| Implementation | Lặp ước lượng intercept; SuggestProjectileVelocity; sweep cung đạn |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | Original BP projectile; muzzle socket |
| Dependencies | Observed target motion, gravity, SuggestProjectileVelocity, hull/terrain sweep. |
| Configuration | 10 vòng; sai lệch thời gian 0.001 s; sweep 12.5 cm |
| User Interaction | AI tính trước khi bắn |
| Validation | NATURAL_MATCH_PIE.json |
| Evidence | `Docs/Testing/NATURAL_MATCH_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Không có solver thủy khí động học hay bảo đảm trúng |

## F12 — Sai số và độ khó AI

| Field | Evidence |
|---|---|
| Status | `IMPLEMENTED_NOT_RUNTIME_VERIFIED` |
| Purpose | Điều chỉnh phản ứng và phân tán thay vì bắn trúng tuyệt đối. |
| Implementation | Độ trễ quan sát, chu kỳ aim, acquisition và sai số theo đĩa |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | EShipAISkill |
| Dependencies | Aim history/cache, deterministic random stream, difficulty menu/config. |
| Configuration | Easy/Normal/Hard; AIAccuracy 0.35/0.6/0.9 |
| User Interaction | AI Difficulty trong setup |
| Validation | MATCH_UI_PIE.json |
| Evidence | `Docs/Testing/MATCH_UI_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Menu đã thử; chưa có benchmark tỷ lệ trúng so sánh ba mức |

## F13 — Role, rút lui và lùi có bắn

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Cho tàu phản ứng nguy hiểm nhưng vẫn về nhiệm vụ hợp lệ. |
| Implementation | Passive/Coward/Fighter/Hunter; retreat intent qua planner |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | ShipEnemyNavigation |
| Dependencies | Combat roles, navigator intent API, rear clearance, HP/range/time guards. |
| Configuration | HP/range/time guards; bKeepAuthoredPatrol |
| User Interaction | Generated cho phép chiến thuật |
| Validation | TACTICAL_RETREAT_REGRESSION.json |
| Evidence | `Docs/Testing/TACTICAL_RETREAT_REGRESSION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Bằng chứng cô lập có kiểm soát; manual giữ A/B |

## F14 — Phòng thủ, convoy và tìm lại địch

| Field | Evidence |
|---|---|
| Status | `KNOWN_LIMITATIONS` |
| Purpose | Nối giao chiến cục bộ với nhiệm vụ đội và tìm mục tiêu khuất. |
| Implementation | Memory chỉ lưu LOS thật; scout search, leader/retry/cooldown |
| Source | `Source/VDTPhase2Project/Private/AI/Tactics/NavalBattlefieldTactics.cpp` |
| Unreal Assets | NavalBattlefieldSubsystem; sectors |
| Dependencies | Genuine LOS observations, contact memory, sectors, existing navigator; generated mode. |
| Configuration | ContactMemoryLifetime; mission retry bounds |
| User Interaction | Generated / Full AI |
| Validation | SCOUT_REACQUISITION_CONTROLLED.json |
| Evidence | `Docs/Testing/SCOUT_REACQUISITION_CONTROLLED.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Đã có trận tự nhiên nhưng stress tuyến khác thất bại |

## F15 — Vũ khí chung, băng đạn và reload

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Dùng chung điều kiện bắn và trạng thái đạn giữa player/AI. |
| Implementation | Player/AI dùng Weapon; spawn thành công mới trừ đạn |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | BP_ShipProjectile; BP_ShipProjectileWithSpeed |
| Dependencies | Original BP spawn/FX/recoil, projectile component, world timers, action gate. |
| Configuration | 10 viên; 30 giây game; AI cooldown profile 8 s |
| User Interaction | Chuột phải ngắm, trái bắn; HUD ammo |
| Validation | SHARED_MAGAZINE_REGRESSION.json |
| Evidence | `Docs/Testing/SHARED_MAGAZINE_REGRESSION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Graph CanShoot/recoil và lượt còn có quyền chặn |

## F16 — Damage theo vùng, splash và guard

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Tính sát thương nhất quán và chặn tự bắn/đồng đội/trùng. |
| Implementation | Quy đổi hit vào hull-local X; cache affiliation; claim một victim |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | ShipProjectileDamageComponent |
| Dependencies | Hull-local bounds, cached projectile affiliation, victim claims, battle score. |
| Configuration | Bow/Mid/Stern 0.8/1/0.8; standard 10 |
| User Interaction | Đạn chạm thân; HUD/score cập nhật |
| Validation | CORE_GAMEPLAY_REGRESSION.json |
| Evidence | `Docs/Testing/CORE_GAMEPLAY_REGRESSION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Tỷ lệ gameplay; chưa mô phỏng giáp thực |

## F17 — Chết, cháy, chìm và cleanup

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Kết thúc tàu một lần và dọn vòng đời trình diễn an toàn. |
| Implementation | Guard chết một lần; dừng AI/physics trước sinking presentation |
| Source | `Source/VDTPhase2Project/Private/Combat/ShipCombatComponent.cpp` |
| Unreal Assets | Niagara; wreck billboard; original FX |
| Dependencies | Weapon death guard, buoyancy/physics shutdown, Niagara, camera and battle authority. |
| Configuration | Dying/Burning/Sinking/Submerged/Cleanup |
| User Interaction | HP=0 hoặc capsize đủ điều kiện |
| Validation | RESULT_LIFECYCLE_PIE.json |
| Evidence | `Docs/Testing/RESULT_LIFECYCLE_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Sinking sau chết là trình diễn; không mô phỏng ngập khoang |

## F18 — Match authority, lượt và score

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Giữ bộ đếm/score/kết quả độc lập với UI và actor render. |
| Implementation | Roster/alive/Fallen; CanShipAct; turn timer; thống kê |
| Source | `Source/VDTPhase2Project/Private/UI/NavalBattleUI.cpp` |
| Unreal Assets | NavalBattleSubsystem; MatchSummaryWidget |
| Dependencies | World roster, registered/fallen sets, weapon notifications, action/turn timer. |
| Configuration | 1-3 mỗi đội; lượt 10 giây |
| User Interaction | Real-Time/Turn-Based; result/restart/return |
| Validation | NATURAL_MATCH_PIE.json |
| Evidence | `Docs/Testing/NATURAL_MATCH_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | World physics/projectile vẫn chạy ngoài lượt |

## F19 — Match Settings và lưu lựa chọn

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Cho người vận hành chọn cấu hình và tạo trận sạch. |
| Implementation | Giữ selections qua level travel; validate seed/count/team |
| Source | `Source/VDTPhase2Project/Private/UI/ShipSetupWidget.cpp` |
| Unreal Assets | NavalMatchSettings GameInstanceSubsystem |
| Dependencies | GameInstance settings, GameMode, battlefield readiness, OpenLevel/controller. |
| Configuration | Medium mặc định; seed 20261028; manual mặc định |
| User Interaction | P, +/X, Start, Back/Quit |
| Validation | MATCH_UI_PIE.json |
| Evidence | `Docs/Testing/MATCH_UI_PIE.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Lưu trong phiên GameInstance; không chứng minh save lâu dài |

## F20 — Camera và spectator không chiếm AI

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Quan sát toàn trận mà không lấy controller của AI. |
| Implementation | Q/Tab/F thay view target; Full AI có pawn camera riêng |
| Source | `Source/VDTPhase2Project/Private/Gameplay/CustomPlayerController.cpp` |
| Unreal Assets | FreeCameraPawn; cameras; selected ship |
| Dependencies | Selected ship/view targets, independent camera pawn, live roster and input. |
| Configuration | PlayerTeam; selected alive fallback |
| User Interaction | Q/Tab/F, W/S/A/D trong free camera |
| Validation | MERGED_MAIN_VERIFICATION.json |
| Evidence | `Docs/Testing/MERGED_MAIN_VERIFICATION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Xem AI không chuyển quyền lái sang AI đó |

## F21 — HUD, tên đội/tàu, kill feed

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Hiển thị rõ tàu, đội, damage và tiêu diệt cho người xem. |
| Implementation | HUD đọc selected ship; bảng tên, HP/ammo/knots; death notification |
| Source | `Source/VDTPhase2Project/Private/UI/ShipDetailsUI.cpp` |
| Unreal Assets | IdentityComponent; TeamCountBar; KillFeed |
| Dependencies | Identity + weapon/battle events, selected-ship controller, UMG/Slate. |
| Configuration | ShipName/TeamName; fallback; tags descriptive |
| User Interaction | I và HUD tự cập nhật |
| Validation | MERGED_MAIN_VERIFICATION.json |
| Evidence | `Docs/Testing/MERGED_MAIN_VERIFICATION.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Tên/màu trình bày không phải authority faction |

## F22 — Minimap 1-9x, ship follow, Alt cursor

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Đọc được khu vực quanh tàu mà không chặn gameplay mouse. |
| Implementation | Pan theo marker thật; M game-only; Alt explicit hit-testing |
| Source | `Source/VDTPhase2Project/Private/UI/NavalBattleUI.cpp` |
| Unreal Assets | WBP_ShipMinimap; marker slots |
| Dependencies | Existing BP marker slots, selected ship, arranged overlay, controller modal ownership. |
| Configuration | M 1/9x; clamp 1-9; manual suspends follow |
| User Interaction | M; giữ Left Alt để kéo/cuộn |
| Validation | Saved/MinimapInput/20261114/PIE_INPUT.json |
| Evidence | `Saved/MinimapInput/20261114/PIE_INPUT.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Physical desktop fire inconclusive; Enhanced Input firing đã pass |

## F23 — Công cụ tích hợp và kiểm chứng

| Field | Evidence |
|---|---|
| Status | `VERIFIED_WORKING` |
| Purpose | Kiểm tra hợp đồng C++/Blueprint và lưu bằng chứng hồi quy. |
| Implementation | Kiểm tra graph, reflection, profiles; editor-only dependency guards |
| Source | `Source/VDTPhase2Project/Private/Navigation/Editor/ShipEnemyNavigationEditor.cpp` |
| Unreal Assets | Blueprint installers; native tests; Python probes |
| Dependencies | Editor-only modules, native automation, Python commandlet/PIE tools. |
| Configuration | 41 types, 39 BP ở checkpoint 14/11 |
| User Interaction | Tools/Testing và editor helper |
| Validation | NATIVE_BUILD_AND_BLUEPRINTS.json |
| Evidence | `Docs/Testing/NATIVE_BUILD_AND_BLUEPRINTS.json` unless explicit Saved/source path; current read-only reflection in UNREAL_INSPECTION.json |
| Limitations | Helper sửa asset không tự chạy trong audit; compile không thay gameplay proof |

## F24 — Terrain portable trong gói Windows

| Field | Evidence |
|---|---|
| Status | `UNMERGED_BRANCH_ONLY` |
| Purpose | Giảm phụ thuộc đường dẫn dữ liệu tuyệt đối khi phân phối. |
| Implementation | Resolver dataset cục bộ, staging và thiếu dữ liệu có thông báo |
| Source | `Source/VDTPhase2Project/Private/Navigation/NavalLocalTerrainComponent.cpp (package branch)` |
| Unreal Assets | package f44013f |
| Dependencies | Unmerged package branch resolver, dataset staging and package tests. |
| Configuration | Ba commit riêng f38bdec/fe41869/f44013f |
| User Interaction | Không phải main đã audit |
| Validation | git main..package |
| Evidence | Branch/source comparison recorded in IMPLEMENTATION_EVIDENCE.md |
| Limitations | Không cấp chứng nhận package cho main |

## F25 — SeaGraph và projectile compatibility cũ

| Field | Evidence |
|---|---|
| Status | `HISTORICAL_OR_REMOVED` |
| Purpose | Giữ compatibility có điều kiện khi shared combat vắng mặt. |
| Implementation | Tài nguyên legacy/compatibility giữ theo điều kiện, không phải A* chính |
| Source | `Source/VDTPhase2Project/Private/Combat/Projectiles/ShipEnemyProjectile.cpp` |
| Unreal Assets | Content/SeaGraph; AntiShip |
| Dependencies | Legacy assets/projectile fallback; not the active primary navigation path. |
| Configuration | Guard fallback khi shared combat vắng mặt |
| User Interaction | Không có menu primary riêng |
| Validation | source/call path inspection |
| Evidence | Branch/source comparison recorded in IMPLEMENTATION_EVIDENCE.md |
| Limitations | Không gọi tên asset là chứng minh tính năng runtime |

