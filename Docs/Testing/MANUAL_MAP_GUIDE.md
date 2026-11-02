# Tự dựng demo A–B

1. Dừng Play. Trong `MainMapLevel`, đặt vị trí/hướng năm AI có sẵn; đặt `PlayerStart` cho player. Không thêm một player pawn thứ hai.
2. Với mỗi AI, chọn `ShipEnemyNavigationComponent`, gán **Patrol Actor A** và **Patrol Actor B** bằng eyedropper tới hai TargetPoint khác nhau. Các instance hiện tại dùng Arrival Radius 80 m: A/B phải cách hơn 160 m. Chừa nước rộng cho cả thân tàu và quay đầu.
3. **Save All** trước Play. **START NEW MATCH tải lại map đã lưu**, nên thay đổi trong PIE sẽ không thay bố trí demo.
4. Trong Match Setup, chọn **USE MY MAP (A-B)**. Player mặc định Medium; chọn Small tại slot PLAYER nếu muốn. Chọn Player + AI hoặc Full AI, rồi START NEW MATCH.
5. AI đi **vị trí đã đặt → A → B → A → B…**. Tọa độ và tham chiếu target không bị sinh lại. Scan/aim/fire vẫn chạy, nhưng pursuit/retreat không thay đường A/B trong chế độ này.

Các liên kết đang lưu trong map:

| Tàu | A | B |
|---|---|---|
| Friendly_AI_1 | TP_Friendly_Patrol_A | TP_Friendly_Patrol_B |
| Friendly_AI_2 | TP_Blue02_A | TP_Blue02_B |
| BP_ShipEnemy | TP_ShipEnemy_AStarDestination | TP_ShipEnemy_OpenSeaDestination |
| BP_ShipEnemy2 | TP_Red02_A | TP_Red02_B |
| BP_ShipEnemy3 | TP_Red03_A | TP_Red03_B |

## Full AI

Tàu thứ sáu thuộc Blue, dùng loại tàu ở slot Blue 1 và xuất phát tại `PlayerStart`. Player Team chỉ áp dụng cho Player + AI. Cặp marker hiện có:

- `TP_ShipDestination`: Actor Tag `Naval.PlayerPatrol.A`.
- `TP_Player_Patrol_B`: Actor Tag `Naval.PlayerPatrol.B`.

Mỗi tag phải xuất hiện đúng một lần trên hai TargetPoint khác nhau. Thiếu/trùng tag sẽ báo lỗi. Đặt marker luôn được load trong World Partition. Năm AI đặt sẵn vẫn dùng cặp Patrol Actor A/B của riêng chúng.

## Seed và chọn đội

- **GENERATE FROM SEED** sinh vị trí/route tự động; số dương lặp lại bố trí trong cùng cấu hình, `0` chọn seed mới.
- **USE MY MAP (A-B)** giữ seed để truy vết nhưng không dùng seed để dời tàu/marker.
- Chọn Red player trong My Map đổi phe, vẫn dùng `PlayerStart` và giữ vị trí các AI. Muốn đổi vị trí player, sửa `PlayerStart`.

## Khi tàu đứng lâu

- Xem active target, Navigation State, khoảng cách, tốc độ và rudder. Ga 0 khi hãm/quay đầu không đồng nghĩa mất AIController.
- Startup kiểm tra tham chiếu, khoảng cách A/B, chart/collision thực theo thân tàu, và cả ba chặng start→A, A→B, B→A. Sửa marker được nêu trong lỗi; không tắt collision để ép chạy.
- Người dựng map đã bố trí lại tàu và target. Bản sửa mới giữ nguyên các vị trí đã lưu này; checkpoint mới ở `Saved/AuthoredMapCheckpoints/20261102_205254_user_layout/`.
- Nếu marker nằm trên nước nhưng sát vùng đệm bờ, hệ thống tìm một điểm tiếp cận đủ an toàn ngay trong Arrival Radius. Tâm tàu vẫn phải vào bán kính của marker thật rồi hãm/chờ trước khi đổi A/B. Marker trên đất hoặc không có điểm tiếp cận an toàn vẫn bị từ chối.
- Combat/death/kết quả trận vẫn hoạt động. Để xem đủ nhiều vòng, tắt **Enable Combat** trên AI hoặc đặt hai đội ngoài tầm phát hiện. Khi trận kết thúc, dùng **RETURN TO SETUP**.

Biên bản map mới ở `USER_LAYOUT_SHORELINE_QA.md`; `MANUAL_MAP_QA.md` giữ kết quả bố trí trước đó. Hướng dẫn này không thay kết quả PIE.

## Chống mắc cạn khi lái

Tàu player có `Prevent Player Grounding` trên ShipMovementComponent. Khi hộp thân/keel dự báo chạm địa hình phía trước, lực tiến sẽ hãm qua actuator gốc; HUD hiện **SHALLOW WATER AHEAD - turn or reverse**. Dùng A/D để rẽ hoặc S để lùi ra. Đây là trợ giúp tránh mắc cạn, không phải tự lái; không khóa yaw, không tắt buoyancy/collision và không cứu tàu đã đặt trên đất.

Lưới bờ không đệm 25 m dùng để phân loại DEM mặt biển; lưới A* vẫn có clearance riêng và survey theo thân tàu. Không dùng vùng đệm điều hướng làm một vành địa hình rắn quanh đảo. Bằng chứng map mới: `USER_LAYOUT_SHORELINE_QA.md`; hồ sơ `MANUAL_MAP_QA.md` thuộc bố trí trước khi tác giả sửa lại.
