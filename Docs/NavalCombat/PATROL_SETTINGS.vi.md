# Cấu hình AI chạy đúng Target Point và lặp A/B

Áp dụng cho cả hai Friendly và ba Enemy. Tất cả dùng BP_ShipEnemy kế thừa BP_Ship, cùng component EnemyNavigation; mỗi pawn có AIController và trạng thái route riêng.

## Actor Details

| Mục | Giá trị |
|---|---|
| AI Enabled (`bAIEnabled`, mặc định Blueprint) | Bật |
| Auto Possess AI | Placed in World or Spawned |
| AI Controller Class | AIController |
| Auto Possess Player | Disabled trên AI |
| Is Spatially Loaded | Tắt cho các tàu/anchor của demo |
| ShipSM → Simulate Physics | Bật, giữ collision và Buoyancy |

## EnemyNavigation

| Mục | Giá trị đã lưu |
|---|---|
| New Navigation Enabled | Bật |
| Navigation Mode | Patrol |
| Role | Fighter |
| Patrol Actor A / B | Chọn **actor Target Point thật trong level**, theo bảng dưới |
| Patrol Offset A / B | (0, 0, 0) |
| Start At Patrol B | Bật |
| Cruise Speed Mps | 20 |
| Arrival Radius M | 80 |
| Patrol Wait Seconds | 3 |
| Safe Ship Separation M | 350 |
| Traffic Priority Wait Seconds | 15 |
| Traffic Priority Hold Seconds | 40 |
| Navigation Map File | SeaNavigation/halong_navigation.json |

`Patrol Point A/B` dạng Vector cũ là longitude/latitude/height, **không phải tọa độ World X/Y/Z**. Khi đã gán Patrol Actor A/B, component đọc vị trí actor thật mỗi lượt; không cần chép Location vào Vector này.

## Cặp đích hiện tại

| Tàu | Patrol Actor A | Patrol Actor B |
|---|---|---|
| Friendly_AI_1 | TP_Friendly_Patrol_A | TP_Friendly_Patrol_B |
| Friendly_AI_2 | TP_Blue02_A | TP_Blue02_B |
| BP_ShipEnemy | TP_ShipEnemy_AStarDestination | TP_ShipEnemy_OpenSeaDestination |
| BP_ShipEnemy2 | TP_Red02_A | TP_Red02_B |
| BP_ShipEnemy3 | TP_Red03_A | TP_Red03_B |

Tàu tới trong bán kính 80 m của đích, phanh xuống dưới 0,5 m/s, chờ 3 giây, rồi tự đổi A/B và tính lại A* từ vị trí hiện tại. Arrival radius là dung sai cập bến; không có teleport tới đúng tâm actor.

Quyền ưu tiên giữa AI có giới hạn chờ: tàu đợi lâu được ưu tiên tạm thời qua chỗ hẹp. Player luôn được ưu tiên; quyền ưu tiên AI vẫn không bỏ qua kiểm tra đất, hull sweep hoặc phanh an toàn. `Traffic Wait Seconds` và `Temporary Traffic Priority` là dữ liệu chẩn đoán trong PIE.

## Chẩn đoán khi tàu không đi

Trong PIE, chọn component EnemyNavigation và xem `Pathfinding Succeeded`, `Navigation State`, `Last Navigation Failure Reason`, `Desired Throttle`, `Desired Rudder`, `Current Waypoint Index`, `Patrol Legs Completed`.

Lỗi đã xác nhận trong scene trước sửa: hai Friendly xuất phát trong vùng bị buffer bờ biển loại; Target Point B cũ của Enemy cũng nằm trong ô bị loại. Controller đúng vẫn không thể chạy khi điểm đầu/đích không hợp lệ. Vị trí đã được chọn lại từ chart nước và đối chiếu coastline, không chỉ nhìn màu nước trên viewport.

Kiểm chứng vòng lặp thực tế được ghi riêng trong PATROL_LOOP_VERIFICATION.json và PATROL_LOOP_SUMMARY.json. Không suy ra kết quả gameplay chỉ từ các checkbox hoặc build thành công.
