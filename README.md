# VDT Phase 2 — Mô phỏng hải chiến trên địa hình thực tế

Mô phỏng hải chiến tại Quảng Ninh bằng **Unreal Engine 5.6, Cesium, C++ và Blueprint**. Bạn có thể lái một tàu, quan sát hai đội AI, tự đặt điểm xuất phát/tuyến tuần tra A–B hoặc sinh bố trí từ seed.

![Tàu người chơi, HUD và bản đồ Quảng Ninh](reports/images/details_ship.jpg)

**[Báo cáo kỹ thuật và hướng dẫn tính năng](reports/reports.pdf)** · **[Ảnh demo](reports/images)**

## Tính năng

- Hai đội Blue/Red, mỗi đội **1–3 tàu**, chọn Small/Medium/Large; người chơi mặc định Medium.
- Địa hình và ảnh phủ Quảng Ninh từ **UnrealTerrainSet**, mặt nước và buoyancy từ Unreal Water.
- Lực đẩy, bánh lái, quán tính, giới hạn tốc độ và cản nước theo từng profile.
- AI dùng **A***, bám waypoint, phanh, tránh đảo/tàu và phục hồi khi bị cản.
- Tuần tra **xuất phát → A → B → A…**, hoặc bố trí tự động theo seed.
- Nhận diện địch, bắn đón, sát thương, băng **10 viên**, nạp **30 giây**, cháy/chìm.
- HUD, kill feed, camera, minimap **9× theo tàu**, bảng kết quả và chơi lại.
- Terrain/imagery dùng **đường dẫn động theo thư mục project**, có khai báo dữ liệu rời cho bản đóng gói.

## 1. Cài môi trường

Hướng dẫn dành cho **Windows 64-bit**, mở project từ mã nguồn.

| Thành phần | Cấu hình |
| --- | --- |
| Unreal Engine | **5.6**; dự án đã sử dụng 5.6.1 |
| Visual Studio | **2022**, từ 17.8; Epic khuyến nghị 17.14 với UE 5.6 |
| Toolchain | MSVC v143 **14.38.33130**, Windows SDK **10.0.22621.0** hoặc mới hơn tương thích |
| Workload | **Game development with C++**, **Desktop development with C++** |
| Plugin | **Cesium for Unreal** tương thích UE 5.6; **Water**, **Procedural Mesh Component** |
| Git | Git và **Git LFS** để tải đầy đủ asset/ảnh |
| Máy | GPU phù hợp DirectX 12/Shader Model 6; SSD và dung lượng cho asset, dữ liệu, shader cache |

Cài UE bằng Epic Games Launcher và cài Cesium vào **đúng bản UE 5.6**. Import [.vsconfig](.vsconfig) trong Visual Studio Installer để xem thành phần project dùng. Tham khảo [toolchain chính thức của UE 5.6](https://dev.epicgames.com/documentation/en-us/unreal-engine/setting-up-visual-studio-development-environment-for-cplusplus-projects-in-unreal-engine?application_version=5.6). `VisualStudioTools` là plugin tùy chọn, không thay compiler.

## 2. Tải project

Mở PowerShell tại nơi muốn lưu project:

```powershell
git lfs install
git clone https://github.com/Mr-Gwenchana/VDT-Phase2-Project.git
Set-Location .\VDT-Phase2-Project
git lfs pull
git lfs checkout
```

Chờ LFS tải xong trước khi mở UE. Nếu `.uasset`, `.jpg` hoặc `.png` chỉ chứa dòng `version https://git-lfs.github.com/spec/v1`, đó vẫn là con trỏ, chưa phải asset thật. Bản ZIP có thể chưa chứa đầy đủ dữ liệu LFS.

```text
VDT-Phase2-Project/
├── VDTPhase2Project.uproject
├── Config/
├── Source/
├── Content/
│   ├── Levels/MainMapLevel.umap
│   ├── __ExternalActors__/...
│   ├── Game/UI/Minimap/...
│   └── SeaNavigation/
│       ├── halong_navigation.json
│       ├── quangninh_sectors.json
│       └── quangninh_demo_routes.json
├── Data/UnrealTerrainSet/QuangNinh/
│   ├── Terrain/layer.json
│   ├── Terrain/{z}/{x}/{y}.terrain
│   ├── Imagery/tilemapresource.xml
│   └── Imagery/{z}/{x}/{y}.png
└── reports/
    ├── reports.pdf
    └── images/
```

`{z}/{x}/{y}` mô tả cấp và chỉ số tile, không phải tên thư mục cần tạo nguyên văn. Khi nhận project qua ổ cứng, cần chép cả metadata lẫn các tile con.

## 3. Dữ liệu bản đồ và đường dẫn động

### UnrealTerrainSet

Nguồn chính là **`Data/UnrealTerrainSet/QuangNinh`**:

| Tệp | Chức năng |
| --- | --- |
| `Terrain/layer.json` | Manifest **quantized-mesh-1.0**, EPSG:4326, sơ đồ TMS, phạm vi phủ và mẫu URL tile |
| `Terrain/{z}/{x}/{y}.terrain` | Hình học độ cao, các mức dữ liệu 0–13 |
| `Imagery/tilemapresource.xml` | Manifest ảnh TMS: EPSG:4326, tile PNG **256×256**, các mức 6–14 |
| `Imagery/{z}/{x}/{y}.png` | Ảnh bề mặt phủ lên terrain; không phải dữ liệu độ sâu hoặc vùng đi được |

Actor **`QuangNinh_Terrain`** dùng Cesium 3D Tileset; component **`QuangNinh_Imagery`** dùng Cesium Tile Map Service Raster Overlay. Component **`PortableLocalTerrain`** tự ghép:

```text
ProjectDir + Data/UnrealTerrainSet/QuangNinh/Terrain/layer.json
ProjectDir + Data/UnrealTerrainSet/QuangNinh/Imagery/tilemapresource.xml
```

Hai đường dẫn được đổi sang URL `file:///`, có mã hóa khoảng trắng và ký tự Unicode. Khi chuyển project hoặc bản đóng gói sang thư mục khác, hệ thống tính lại URL; không cần nhập ổ E:/F: của máy tác giả. Các thuộc tính `TerrainFile` và `ImageryFile` của component lưu **đường dẫn tương đối**. URL được để trống trong actor đã lưu và được dựng lại khi component đăng ký; lúc cook cũng xóa URL preview để không mang đường dẫn máy phát triển vào bản game.

Giữ đầy đủ cây `Data/UnrealTerrainSet/QuangNinh`. Không xóa manifest hoặc đổi cấu trúc tile khi chuyển máy. Hai nguồn local này không cần Cesium ion token; nguồn online bổ sung có cấu hình riêng. Tham khảo [định dạng Cesium hỗ trợ](https://cesium.com/learn/unreal/unreal-faq/#what-file-formats-can-i-load-with-cesium-for-unreal).

### Ba JSON gameplay

| Tệp trong `Content/SeaNavigation` | Chức năng |
| --- | --- |
| `halong_navigation.json` | Lưới nước/đất **400×400**, ô **100 m**, vùng đệm bờ và clearance cho A* |
| `quangninh_sectors.json` | **625 sector**, vùng nhiệm vụ, khả năng phù hợp từng loại tàu và mã kiểm tra chart |
| `quangninh_demo_routes.json` | Gợi ý tuần tra, tập kết, tiếp cận và seed mẫu |

Terrain/imagery tạo môi trường nhìn thấy; ba JSON trên phục vụ điều hướng và nhiệm vụ. Không dùng `halong_navigation.json` thay URL terrain. Khi thay chart cần cập nhật sector tương ứng vì hệ thống kiểm tra mã dữ liệu. Dữ liệu bờ dẫn xuất từ OpenStreetMap **không có độ sâu đáy biển**; xem [attribution](Data/SeaNavigation/ATTRIBUTION.md).

### Dữ liệu khi package

[VDTPhase2Project.Build.cs](Source/VDTPhase2Project/VDTPhase2Project.Build.cs) khai báo:

- Ba JSON gameplay là **UFS**.
- `.terrain`, `.png`, `.json`, `.xml` dưới `Data/UnrealTerrainSet/QuangNinh` là **NonUFS** để Cesium đọc file rời ngoài pak.

Giữ nguyên cả thư mục Windows được tạo khi package, gồm thư mục dữ liệu của project; không chỉ chép một file `.exe`.

## 4. Build và mở project

Đóng Editor trước khi build/link đầy đủ. Chạy PowerShell tại thư mục có `.uproject`:

```powershell
$ueRoot = 'F:\Epic Games\UE_5.6' # Sửa theo nơi cài UE.
$projectFile = (Resolve-Path '.\VDTPhase2Project.uproject').Path

& "$ueRoot\Engine\Build\BatchFiles\Build.bat" `
    VDTPhase2ProjectEditor Win64 Development `
    "-Project=$projectFile" -WaitMutex -NoHotReloadFromIDE
```

Build thành công rồi mở project:

```powershell
& "$ueRoot\Engine\Binaries\Win64\UnrealEditor.exe" `
    $projectFile '/Game/Levels/MainMapLevel'
```

Hoặc Generate Visual Studio project files từ menu chuột phải `.uproject` rồi build **Development Editor / Win64**. Không cần workspace riêng hoặc `.sln` đã tạo trên máy tác giả để chạy `Build.bat`.

Trong **Edit → Plugins**, kiểm tra Cesium, Water và Procedural Mesh Component đã bật. Mở **`/Game/Levels/MainMapLevel`**, chờ shader/asset lần đầu, nhấn **Play** rồi nhấp viewport để nhận input. Build Editor chỉ tạo module dùng trong Editor; để có bản Windows riêng, thực hiện **Platforms → Windows → Package Project** theo cấu hình bạn muốn và giữ đầy đủ dữ liệu được stage.

## 5. Thiết lập demo

<p align="center"><img src="reports/images/start_menu.jpg" alt="Match Settings với hai đội ba tàu" width="480"></p>

1. Chọn độ khó **Easy / Normal / Hard** và **Real-Time / Turn-Based**.
2. Chọn **GENERATE MANUALLY (A-B)** để dùng bố trí đã lưu, hoặc **GENERATE FROM SEED**.
3. Chọn **Player + AI** để tự lái, hoặc **Full AI / spectator** để quan sát.
4. Chọn đội, số lượng và loại tàu. PLAYER mặc định Medium, có thể đổi Small/Large.
5. Chọn Standard để dùng 100 HP/10 damage, hoặc cân bằng theo class.
6. Nhấn **START NEW MATCH** và chờ **PREPARING BATTLEFIELD** kết thúc.

![Chuẩn bị chiến trường](reports/images/preparing_battlefield.jpg)

Giai đoạn chuẩn bị xác nhận dữ liệu, vị trí và trạng thái thân tàu. Nếu **SETUP FAILED**, dùng P trở lại thiết lập và kiểm tra vị trí/tuyến.

### Tự đặt vị trí và tuyến A–B

Thực hiện khi **chưa Play**:

1. Đặt AI trên nước, chừa khoảng trống cho toàn bộ thân và vòng quay.
2. Đặt **PlayerStart** tại vị trí xuất phát của người chơi.
3. Tạo hai TargetPoint riêng cho mỗi AI, ví dụ `TP_Friendly_01_A/B`.
4. Gán **PatrolActorA / PatrolActorB** trong **ShipEnemyNavigationComponent**. Để **PatrolCircuitActors** trống nếu chỉ dùng hai điểm.
5. Dùng Blueprint đã tích hợp điều hướng, như `BP_ShipEnemy` hoặc `BP_ShipSmallAI`; lưu map/actor.
6. Play, chọn chế độ thủ công rồi bắt đầu trận.

Tàu chạy **xuất phát → A → B → A…**. Bố trí demo dùng bán kính đến 80 m; nên tách A/B hơn hai lần bán kính và chừa vùng quay. Chế độ thủ công giữ A/B nhưng vẫn ngắm/bắn; không tự bỏ tuyến để truy đuổi/rút lui.

Trong Player + AI, người chơi tự lái. Full AI cần thêm đúng một marker có tag **`Naval.PlayerPatrol.A`** và một marker khác có tag **`Naval.PlayerPatrol.B`** cho slot thay người chơi. AI đặt sẵn vẫn dùng tham chiếu A/B riêng.

### Sinh từ seed

Chọn GENERATE FROM SEED và nhập seed dương, ví dụ `20261028`; **0** yêu cầu seed mới. Cùng seed, dữ liệu và đội hình giúp tái lập bố trí, không đảm bảo cùng kết quả trận. Chế độ này cho phép AI thay tuyến theo nhiệm vụ/chiến thuật. Seed không xáo vị trí trong chế độ thủ công.

## 6. Điều khiển và quan sát

| Thao tác | Chức năng |
| --- | --- |
| W / S | Tiến; hãm hoặc lùi tùy vận tốc |
| A / D | Bẻ lái trái/phải; thân có quán tính |
| Di chuột | Đổi góc nhìn/hướng ngắm |
| Giữ chuột phải | Camera ngắm pháo |
| Chuột trái | Bắn khi tâm ngắm và vũ khí đủ điều kiện |
| Cuộn chuột | Zoom camera; khi tương tác minimap thì đổi zoom map |
| M | Minimap 1×/9× theo tàu đang xem |
| Giữ Left Alt | Con trỏ để kéo/cuộn minimap; thả để về gameplay |
| Q | Đổi tàu quan sát, không chuyển quyền lái từ AI |
| F | Toàn cảnh / trở lại tàu |
| I | Ẩn/hiện bảng thông tin |
| P | Tạm dừng và mở thiết lập |
| Tab | Chuyển đồng đội sống khi người chơi chết; đổi tàu trong Full AI |
| / | Mở/đóng hướng dẫn |
| Shift + F1 | Thả chuột ra Unreal Editor trong PIE |

<table>
<tr><td><img src="reports/images/switch_camera_and_map.jpg" alt="Quan sát tàu Small đội Red"></td><td><img src="reports/images/zoom_map.jpg" alt="Minimap phóng đại khi đang ngắm"></td></tr>
<tr><td>Q: đổi đối tượng camera và HUD.</td><td>M: xem gần đội hình; giữ Alt khi cần thao tác map.</td></tr>
</table>

Trong Full AI, camera tự do dùng W/S theo hướng nhìn, A/D dịch ngang, cuộn chuột thay tốc độ bay. Khi người chơi chết, có thể theo đồng đội còn sống hoặc về toàn cảnh.

### Bắn và quản lý đạn

Hướng camera ra ngoài boong, giữ **chuột phải**, chờ tâm ngắm rồi nhấn **chuột trái**. Ngắm vào chính thân có thể ẩn tâm ngắm và chặn bắn. Mỗi phát thành công trừ một viên; hết băng tự nạp 30 giây. **AMMO đỏ khi còn 0–3 viên**, nạp đầy trở lại xanh.

<table>
<tr><td><img src="reports/images/fighting.jpg" alt="Ngắm pháo trong giao chiến"></td><td><img src="reports/images/effect_fighting.jpg" alt="Khói khi khai hỏa"></td></tr>
<tr><td>Góc ngắm và thanh HP mục tiêu.</td><td>Pháo, projectile và hiệu ứng dùng chung.</td></tr>
</table>

Đạn phải trúng đối phương hợp lệ mới gây sát thương. Self/friendly damage và hit trùng được chặn; đảo có thể chắn LOS/đường đạn.

![Tiêu diệt tàu địch và AMMO đỏ khi hết đạn](reports/images/defeat_enemy.jpg)

### Kết thúc trận

Khi một đội hết tàu sống, **Match Summary** hiển thị kết quả và thống kê. Ảnh dưới là ví dụ **DEFEAT** của đội người chơi:

![Kết quả trận và các nút sau trận](reports/images/result.jpg)

- **RETURN TO MAP SETTING**: mở lại thiết lập.
- **RESTART**: tạo lại trận qua luồng khởi tạo mới.
- **QUIT**: thoát phiên chơi.

## 7. Minimap và bản package

`Content/Game/UI/Minimap` có đường dẫn asset Unreal là **`/Game/Game/UI/Minimap`**: `/Game` đầu tiên đại diện cho `Content`, phần `Game` thứ hai là thư mục thật. Đây là cùng một thư mục, không phải dữ liệu bị nhân đôi.

[DefaultGame.ini](Config/DefaultGame.ini) đã thêm thư mục này vào **Additional Asset Directories to Cook**, vì icon hai đội được nạp bằng đường dẫn chuỗi. Giữ các texture `ShipMarkerIcon-1` và `ShipMarkerIcon-2` khi cook để Blue/Red không bị rơi về cùng hình mặc định.

## 8. Hiệu chỉnh và xử lý lỗi

Mở DataAsset tại `/Game/NavalBattle/Profiles`. **Forward/Reverse Max Speed** đổi giới hạn tiến/lùi (m/s), **Cruise Speed** đổi tốc độ hành trình AI, **Propulsion Scale** đổi lực đẩy, **Rudder Scale** đổi phản ứng lái. Các hệ số heave/roll/pitch damping điều chỉnh độ cản nhún/lắc. Lưu rồi mở phiên Play mới; khi tăng tốc, thử lại phanh và quay hai chiều.

| Hiện tượng | Kiểm tra |
| --- | --- |
| Asset/ảnh không mở | Tải Git LFS đầy đủ; kiểm tra con trỏ |
| Thiếu Cesium | Cài plugin đúng UE 5.6 và mở lại Editor |
| Không thấy đảo/ảnh phủ | Giữ đủ `Data/UnrealTerrainSet/QuangNinh`, kiểm tra `PortableLocalTerrain` và `PathError` |
| SETUP FAILED | Xuất phát/A/B trên đất, tuyến không liên thông, vùng quay hẹp hoặc thiếu marker Full AI |
| AI đứng yên/không về B | Pause, chết/kết quả, lượt, thời gian chờ, A/B của đúng actor và vật cản |
| Không bắn/không mất HP | Camera/tâm ngắm, đạn/nạp, cooldown, lượt, địch còn sống, hit có bị chắn không |
| Chuột chỉ dùng cho map | Thả Left Alt; Shift+F1 là thao tác Editor riêng |
| C++ sửa mà UI vẫn cũ | Build/link đúng Editor target rồi mở lại UE; compile không link chưa cập nhật DLL |
| Dòng vàng VSM trên ảnh | Cảnh báo tải Virtual Shadow Map, cần kiểm tra shadow/hiệu năng; khác lỗi JSON hoặc kết quả trận |

## Tìm hiểu thêm

- `Source/VDTPhase2Project`: AI, điều hướng, chiến đấu, vật lý, UI và vòng đời trận.
- `Content/Ship`, `Content/ShipEnemy`, `Content/ShipSmall`: Blueprint và tài nguyên tàu.
- `Content/NavalBattle/Profiles`: cấu hình các loại tàu.
- `Content/Levels`, `Content/__ExternalActors__`: map và actor; giữ cả external actor khi sao chép level.
- `Data/UnrealTerrainSet`: địa hình/ảnh nền; `Content/SeaNavigation`: JSON gameplay.
- `reports/reports.pdf`: cơ chế, công thức và hướng dẫn tính năng; `reports/images`: ảnh demo.

Đổi tên/di chuyển `.uasset`, `.umap` trong Unreal Editor để giữ tham chiếu. Ảnh demo chụp trong Editor, không phải benchmark của một gói Windows trên mọi máy.
