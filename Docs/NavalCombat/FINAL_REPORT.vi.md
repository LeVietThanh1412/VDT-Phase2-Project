# Historical 3v3 report — superseded navigation settings

Current navigation configuration and measured results: [PATROL_LOOP_REPORT.md](PATROL_LOOP_REPORT.md). Expanded task status: [NavalBattleV2_Progress.md](../NavalBattleV2_Progress.md). Values below describe the earlier layout and balance.

# Hải chiến AI 3v3 — VDTPhase2Project

Mở `/Game/Levels/MainMapLevel` và Play. Các mục kiểm chứng ở cuối báo cáo phân biệt dữ liệu PIE thực tế với kiểm tra mã và hình học.

## Cấu hình đã lưu

Chỉ có hai đội: Friendly/Blue = `0`, Enemy/Red = `1`. Giá trị `2` được xử lý như Neutral, không chiến đấu và không nhận/gây damage trong hệ thống này; không có tàu Neutral trong scene.

| Tàu | Đội / điều khiển | Điểm xuất phát East, North (m) | Cách bờ tối thiểu tại spawn |
|---|---|---|---|
| BP_Ship do GameMode tạo | Blue / player | 350, -1950 | 573 m |
| Friendly_AI_1 | Blue / AIController | 750, -1950 | 529 m |
| Friendly_AI_2 | Blue / AIController | 250, -2550 | 449 m |
| BP_ShipEnemy | Red / AIController | -250, -1950 | 361 m |
| Enemy_AI_2 | Red / AIController | -650, -2350 | 467 m |
| Enemy_AI_3 | Red / AIController | 950, -1050 | 345 m |

Đây là hệ tọa độ mét East/North của coastline chart, chuyển qua ECEF/Cesium để đặt actor. Tọa độ world và transform đầy đủ nằm trong [BATTLE_SAVED_CONFIGURATION.json](BATTLE_SAVED_CONFIGURATION.json). Spawn được chọn từ ô nước hợp lệ, đối chiếu polygon đất liền và đường A*; khoảng cách spawn nhỏ nhất 400 m.

Blue nằm trong vịnh phía nam đảo tuần tra, phía tây núi lớn ở đông nam; cửa ra biển ở hướng đông bắc. Red 1/2 nằm phía tây/tây nam đảo, Red 3 nằm phía đông. Xem [bản đồ chiến trường](BATTLEFIELD_OVERVIEW.png).

Level chứa năm BP_ShipEnemy kế thừa BP_Ship. GameMode tạo đúng một player BP_Ship từ PlayerStart; không đặt thêm player pawn. BP_AntiShip cũ được giữ trong editor nhưng đánh dấu editor-only để không thành tàu chiến thứ bảy. Các tàu và Target Point cần cho trận đấu được lưu với World Partition spatial loading tắt.

## Tuần tra và điều hướng

Hai Friendly AI dùng đúng `TP_Friendly_Patrol_A` và `TP_Friendly_Patrol_B`, đều bắt đầu hướng tới B ngoài vịnh. Ba Enemy dùng lại `TP_ShipEnemy_AStarDestination` và `TP_ShipEnemy_OpenSeaDestination`; không tạo bản sao hoặc thay transform hai anchor gốc.

| AI | Anchor A / B | Lệch đội hình East, North áp dụng cho A và B | Đích đầu |
|---|---|---|---|
| Friendly_AI_1 | Friendly Patrol A / B | +150, 0 m | B |
| Friendly_AI_2 | Friendly Patrol A / B | -150, -150 m | B |
| BP_ShipEnemy | Enemy AStar / OpenSea | 0, 0 m | A |
| Enemy_AI_2 | Enemy AStar / OpenSea | +250, +150 m | B |
| Enemy_AI_3 | Enemy AStar / OpenSea | +450, +150 m | A |

Offset chỉ được chấp nhận nếu điểm và đoạn nối từ anchor đều nằm trên nước hợp lệ. Các reference vẫn trỏ tới actor thật và được đọc lại ở PIE; không thay bằng tọa độ tuần tra cố định trong C++. Mỗi component có route, waypoint index, đích và trạng thái riêng. Dữ liệu chart chỉ được dùng để đọc.

Giữ geographic A*, polygon/coastal clearance, ECEF và world-origin handling hiện có. Chỉ navigation phát lệnh throttle/rudder qua `SetDriveCommand`; lực đẩy/rudder vẫn do Blueprint cũ áp vào hull physics. Không dùng NavMesh, teleport hoặc điều khiển transform tàu sống trong gameplay.

Cruise speed 8 m/s, safe separation 400 m. Trong vùng gần, AI giảm tốc với khoảng dừng mục tiêu khoảng 260 m, dùng steering chỉ sau khi kiểm tra chart và sweep hull; cho tiến chậm tối đa 3 m/s khi hướng tránh đã an toàn. Tránh tàu đứng yên không bị tắt/bật theo ngưỡng closing-speed nhỏ. Exclusion tạm của tàu di động không được giữ thành vật cản vĩnh viễn khi A* thất bại.

Player luôn có quyền ưu tiên; giữa AI dùng thứ tự tên actor ổn định để tránh cả hai cùng phanh mềm. Tàu ưu tiên vẫn phải qua các kiểm tra hull/land và phanh khẩn cấp. Khi đã gần như đứng yên, nhánh phanh có thể cho rudder xoay chậm nếu overlap test của box hull thực ở góc kế tiếp và kiểm tra drift trên chart đều an toàn; không vô hiệu hóa collision hoặc di chuyển actor bằng transform.

## Pháo và hit-and-run

Giữ `ExecuteWeaponShot`, hai projectile Blueprint gốc, socket `front_gun_v_socket`, Niagara/audio/recoil/CanShoot và `ABP_Ship`. AI chỉ cung cấp GunYaw/GunPitch cho hai bone controller có sẵn `front_gun_h_jnt`, `front_gun_v_jnt`; manual aim của player tiếp tục đi qua nhánh cũ.

| Thông số | Giá trị |
|---|---|
| Detection / engagement | 1800 / 1200 m |
| Lose target | 2200 m |
| Retreat / safe reengagement | 450 / 800 m |
| AI cooldown / scan interval | 4 / 0.5 giây |
| Aim tolerance yaw / pitch | 1° / 1° |
| Turret yaw / elevation | -135°..135° / -10°..40° |
| Retreat dwell min / max | 12 / 75 giây |
| Retreat cooldown / return dwell | 25 / 3 giây |
| Projectile speed / gravity | 200 m/s / gravity scale 1 |
| AI projectile lifespan | 8.3 giây với range 1200 m |

Range lấy theo khu vực chiến đấu vài km và projectile gốc; không dùng tầm 6–10 km. Target selection duyệt nhiều đối thủ, giữ target ổn định, bỏ target chết/tự mình/đồng đội. Kiểm tra khoảng cách thật, góc turret, muzzle animation alignment, LOS và ballistic arc trước khi bắn. Shared weapon cũng kiểm tra lại đội và engagement distance khi có AI target. Visibility trace được giới hạn; navigation không chạy thêm nhánh targeting/trace legacy khi shared combat đã có mặt.

FSM: `Patrol → Engage → Retreat → ReturnToPatrol`. Engage bắn trong khi tàu vẫn theo route, không quay hull để ngắm. Retreat khi đối thủ gần hơn 450 m hoặc HP còn không quá 50% trong engagement range; chọn một đích nước làm tăng khoảng cách và xác nhận bằng A*. Hướng/đích retreat được giữ trong trạng thái, có dwell và hysteresis. Sau đủ khoảng cách hoặc hết thời hạn, tính lại đường từ vị trí hiện tại về hai patrol anchor cũ.

## Sức khỏe, damage và hủy tàu

Mọi tàu dùng `UShipWeaponComponent`: `MaxHealth=100`, damage trực tiếp mỗi projectile `25`. HP được clamp 0..MaxHealth. Có `OnHealthChanged` và `OnShipDestroyed`; hủy tàu chỉ chạy một lần.

Chốt miễn damage đồng đội nằm ở `UShipWeaponComponent::ReceiveShipDamage` trong `ShipCombatComponent.cpp`. Guard áp dụng tại điểm trừ HP, bao gồm player fire, AI fire và splash; không phụ thuộc việc AI chọn target đúng. Từ chối self hit, cùng đội, Neutral/nguồn không xác định, damage không hữu hạn/không dương và tàu đã chết. Projectile lưu `SourceTeam` và weak source ship trong `UShipProjectileDamageComponent` ngay khi spawn/FinishShot; đội vẫn có sẵn nếu tàu nguồn đã bị cleanup. Projectile ghi nhận victim đã xử lý và impact đã chạy để không trừ lặp.

Chỉ thay node radial damage không lọc đội của hai projectile bằng `ApplyProjectileImpact`. Giữ movement, collision, impact event, explosion và audio cũ. Projectile vẫn va chạm hull đồng đội nhưng gây 0 damage; không vô hiệu hóa hit của tàu địch. Owner/instigator và ignore chính tàu bắn được cấu hình ở spawn/FinishShot.

Khi chết: dừng navigation và command, tắt combat/tick/firing, bỏ target, ẩn health widget, tắt buoyancy, cho wreck chìm bằng physics, phát Niagara explosion sẵn có ở scale lớn hơn. AI wreck cleanup sau 7 giây. Player chết bị khóa movement/look/fire, giữ camera tại wreck và hiện `PLAYER DESTROYED`, để đồng đội tiếp tục; không respawn pawn thứ bảy.

## HUD / UMG

Hai asset được compile và lưu:

- `/Game/NavalBattle/UI/WBP_ShipHUD`: HP số và bar của player, số tàu sống Blue/Red, thông báo hủy tàu ngắn, VICTORY/DEFEAT từ số tàu thật.
- `/Game/NavalBattle/UI/WBP_ShipHealthBar`: nhãn đội Blue/Red riêng, số HP và bar trên tàu AI. HP >60% xanh lá; 30% < HP ≤60% vàng; HP ≤30% đỏ. Ẩn khi chết hoặc camera cách hơn 2200 m.

Widget Blueprint kế thừa lớp UMG C++ tạo presentation khi runtime. `UNavalBattleSubsystem` đăng ký tàu, cập nhật counter khi chết và tạo HUD đúng player controller. HP và counter cập nhật theo sự kiện thay đổi; timer 0.5 giây chỉ xử lý khởi tạo presentation/visibility theo khoảng cách, không quét HP mỗi Tick.

Debug C++ bật/tắt bằng `DrawDebug` của navigation/combat; có route, waypoint, team, state, target, range, aim, firing và HP. Scene mặc định tắt debug để HUD đọc được; PrintString/trace debug legacy của Enemy được tắt.

## File và asset chính

C++: `ShipCombatComponent.h/.cpp`, `ShipCombatEditor.cpp`, `ShipProjectileDamageComponent.h/.cpp`, `NavalBattleUI.h/.cpp`, `ShipEnemyNavigationComponent.h/.cpp`, `VDTPhase2Project.Build.cs` trong `Source/VDTPhase2Project`.

Blueprint: `BP_ShipProjectile`, `BP_ShipProjectileWithSpeed`, `BP_ShipEnemy`, `WBP_ShipHUD`, `WBP_ShipHealthBar`; integration chung dựa trên `BP_Ship`/`ABP_Ship` đã có. Level: `MainMapLevel`, các external actor của năm AI, PlayerStart, hai Friendly Target Point và editor-only BP_AntiShip. Transform Enemy Target Point giữ nguyên.

Automation: `Tools/NavalCombat/configure_battlefield.py`, `plan_battlefield.py`, `run_pie_suite.py` và các fixture `test_*.py`; telemetry, screenshot và tổng hợp nằm trong thư mục này. Bản khôi phục trước bulk edit: `Saved/NavalBattleRecovery_20261018`.

## Kết quả kiểm chứng

**Đang chạy các bài kiểm chứng còn lại; xem [VERIFICATION_SUMMARY.json](VERIFICATION_SUMMARY.json) và [PIE_SUITE.json](PIE_SUITE.json). Mục này sẽ được cập nhật sau lần chạy cuối.**

Các fixture damage/range/retreat dùng phiên PIE riêng và có thể tạm đóng băng/đặt đối thủ để cô lập điều kiện. Không lưu các thay đổi đó. Bài patrol và battle telemetry giữ nguyên spawn/physics/simulation speed; patrol chỉ tắt combat để đủ thời gian kiểm tra vòng tuần tra.

## Play thủ công

1. Mở MainMapLevel, Play, xác nhận `BLUE 3 | RED 3`, HP `100 / 100` và năm bar AI.
2. W/S tiến/lùi, A/D rudder, chuột xoay camera; giữ chuột phải để dùng GunCamera, nhấn chuột trái khi đang ngắm để bắn; cuộn chuột zoom.
3. Bắn vào tàu Blue: HP không đổi. Bắn Red: mỗi trúng đạn hợp lệ trừ 25 HP. Quan sát bar chuyển vàng ở 50, đỏ ở 25 và bộ đếm giảm khi tàu chết.
4. Cho các đội gặp nhau và quan sát turret bắn độc lập với lái; AI retreat rồi về patrol. Có thể bật DrawDebug để xem route/state.
5. Stop rồi Play lại: scene khởi tạo mới, không cần nối widget/node hoặc tạo tàu thủ công.
