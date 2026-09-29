# VDT Phase 2 Project

Project Unreal Engine cho prototype/trình diễn tác chiến hải quân, tập trung vào điều khiển tàu, ngắm bắn, projectile, hiệu ứng chiến đấu và môi trường biển. Gameplay hiện được tổ chức chủ yếu bằng Blueprint và Enhanced Input; module C++ chỉ đóng vai trò module game cơ bản.

## Mục lục

- [Tổng quan](#tổng-quan)
- [Yêu cầu môi trường](#yêu-cầu-môi-trường)
- [Mở project](#mở-project)
- [Chạy thử](#chạy-thử)
- [Build](#build)
- [Cấu trúc project](#cấu-trúc-project)
- [Gameplay và asset chính](#gameplay-và-asset-chính)
- [Input](#input)
- [Cấu hình runtime](#cấu-hình-runtime)
- [Cesium](#cesium)
- [Quy trình phát triển](#quy-trình-phát-triển)
- [Lưu ý khi làm việc với asset](#lưu-ý-khi-làm-việc-với-asset)
- [Giới hạn tài liệu](#giới-hạn-tài-liệu)

## Tổng quan

| Hạng mục | Giá trị |
| --- | --- |
| Tên project | `VDTPhase2Project` |
| Unreal Engine | `5.6` |
| Module runtime | `VDTPhase2Project` |
| Game target | `VDTPhase2Project` |
| Editor target | `VDTPhase2ProjectEditor` |
| Map mặc định | `/Game/Levels/MainMapLevel` |
| Game mode mặc định | `/Game/Ship/BluePrint/GMBP` |
| Nền tảng cấu hình chính | Windows 64-bit |
| Rendering | DirectX 12, shader target `PCD3D_SM6` |

Các plugin được bật trong project:

- `CesiumForUnreal`: dữ liệu/địa hình địa lý 3D và các thiết lập Cesium.
- `ModelingToolsEditorMode`: công cụ modeling trong Unreal Editor.

## Yêu cầu môi trường

### Bắt buộc

- Unreal Engine `5.6`.
- Windows với toolchain C++ cho Unreal Engine.
- Visual Studio với các workload Native Game/Native Desktop.
- MSVC x86/x64, Windows 11 SDK và Unreal debugger/IDE.
- Plugin `CesiumForUnreal` tương thích với Unreal Engine `5.6`.

Các thành phần Visual Studio được project ghi nhận trong [.vsconfig](.vsconfig). Có thể dùng Visual Studio Installer để import file này và cài đúng workload.

### Khuyến nghị

- GPU hỗ trợ DirectX 12 và shader model 6.
- SSD và đủ dung lượng trống cho asset, shader cache và Derived Data Cache.
- Đăng nhập Cesium ion nếu level yêu cầu dữ liệu Cesium từ ion.

## Mở project

1. Cài đúng Unreal Engine `5.6` và các plugin nêu trên.
2. Mở [VDTPhase2Project.uproject](VDTPhase2Project.uproject) bằng Unreal Editor.
3. Chờ Unreal hoàn tất load asset và compile shader lần đầu.
4. Mở map `/Game/Levels/MainMapLevel` nếu Editor không tự mở map mặc định.
5. Nếu cần build C++, mở [VDTPhase2Project.code-workspace](VDTPhase2Project.code-workspace) bằng VS Code hoặc mở solution do Unreal generate bằng Visual Studio.

> `VDTPhase2Project.code-workspace` hiện đang tham chiếu Unreal Engine tại `F:\Epic Games\UE_5.6` và workspace tại `E:\LeVietThanh1412-Product\VDT-Phase2-Project`. Nếu máy khác dùng đường dẫn khác, cần cập nhật các task trong file workspace hoặc dùng đường dẫn tới `Build.bat` tương ứng.

## Chạy thử

### Từ Unreal Editor

1. Mở `MainMapLevel`.
2. Nhấn **Play** để chạy trong editor.
3. Dùng **Standalone Game** nếu cần kiểm tra gần với runtime độc lập.
4. Có thể mở `TestLevel` để kiểm tra scene thử nghiệm.

Map mặc định và game mode được cấu hình trong [Config/DefaultEngine.ini](Config/DefaultEngine.ini):

```ini
GameDefaultMap=/Game/Levels/MainMapLevel.MainMapLevel
EditorStartupMap=/Game/Levels/MainMapLevel.MainMapLevel
GlobalDefaultGameMode=/Game/Ship/BluePrint/GMBP.GMBP_C
```

### Từ command line

Đường dẫn executable phụ thuộc vị trí cài Unreal Engine. Ví dụ với Unreal Engine cài tại `F:\Epic Games\UE_5.6`:

```powershell
& "F:\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe" `
	"E:\LeVietThanh1412-Product\VDT-Phase2-Project\VDTPhase2Project.uproject" `
	"/Game/Levels/MainMapLevel"
```

## Build

Project có sẵn task build cho game/editor ở các configuration `Debug`, `DebugGame`, `Development`, `Test` và `Shipping` trong [VDTPhase2Project.code-workspace](VDTPhase2Project.code-workspace).

### Build từ VS Code

1. Mở Command Palette.
2. Chọn **Tasks: Run Task**.
3. Chọn task có tiền tố `VDTPhase2Project` cho game hoặc `VDTPhase2ProjectEditor` cho editor.
4. Chọn `Development Build` cho build phát triển thông thường.

Các task `Rebuild` sẽ gọi `Clean` trước khi build. Chỉ dùng khi cần loại bỏ artifact build cũ hoặc khi build incremental cho kết quả không nhất quán.

### Build trực tiếp bằng PowerShell

```powershell
$ueRoot = "F:\Epic Games\UE_5.6"
$project = "E:\LeVietThanh1412-Product\VDT-Phase2-Project\VDTPhase2Project.uproject"

& "$ueRoot\Engine\Build\BatchFiles\Build.bat" `
	VDTPhase2Project Win64 Development $project -waitmutex
```

Build editor:

```powershell
& "$ueRoot\Engine\Build\BatchFiles\Build.bat" `
	VDTPhase2ProjectEditor Win64 Development $project -waitmutex
```

Nếu build lỗi sau khi đổi module hoặc target, có thể chạy `Clean.bat` với cùng target/configuration rồi build lại. Không nên đưa các thư mục build/cache sinh ra vào thay đổi gameplay.

## Cấu trúc project

```text
VDTPhase2Project/
├── Config/
│   ├── DefaultEditor.ini       # Thiết lập editor dùng chung
│   ├── DefaultEngine.ini       # Map, game mode, renderer, collision, platform
│   ├── DefaultGame.ini         # Thiết lập game/CommonUI/ProjectID
│   └── DefaultInput.ini        # Enhanced Input classes và axis mặc định
├── Content/
│   ├── AntiShip/                # Asset phương tiện/pháo và hiệu ứng đối kháng
│   ├── CesiumSettings/          # Thiết lập Cesium ion/server
│   ├── LandscapeMaterials/      # Material môi trường, gồm Ocean
│   ├── Levels/                  # Map chính, map test và HLOD data
│   ├── Ship/                    # Gameplay tàu, vehicle asset, input, UI, sound
│   └── __ExternalActors__/      # External actor data của level
├── Source/
│   ├── VDTPhase2Project/        # Module C++ runtime
│   ├── VDTPhase2Project.Target.cs
│   └── VDTPhase2ProjectEditor.Target.cs
├── VDTPhase2Project.uproject
├── VDTPhase2Project.code-workspace
└── .vsconfig
```

### Các thư mục cần biết trong `Content`

| Thư mục | Vai trò |
| --- | --- |
| `Content/Levels` | `MainMapLevel.umap`, `TestLevel.umap` và HLOD asset tương ứng |
| `Content/Ship/BluePrint` | `BP_Ship`, animation Blueprint, projectile Blueprint và `GMBP` |
| `Content/Ship/Inputs` | Input Action và Input Mapping Context cho tàu |
| `Content/Ship/Widgets` | UI, hiện có `WBP_Crosshair` |
| `Content/Ship/Assets/VigilanteContent/Vehicles/West_Ship_DDG102` | Model, skeletal asset, material, FX và các level showcase của tàu |
| `Content/AntiShip/Assets/Vehicles/East_Arty_D30122mm` | Asset pháo/đối tượng anti-ship, material và FX |
| `Content/Ship/Assets/VigilanteContent/Shared/Particles` | Particle system, Niagara/FX material, mesh và texture dùng chung |
| `Content/LandscapeMaterials/Ocean` | Material liên quan cảnh quan/mặt biển |
| `Content/__ExternalActors__` | Actor được Unreal lưu ngoài file map cho `MainMapLevel` và `TestLevel` |

## Gameplay và asset chính

Các asset gameplay có thể bắt đầu đọc/chỉnh sửa từ những đường dẫn sau:

```text
/Game/Ship/BluePrint/BP_Ship
/Game/Ship/BluePrint/ABP_Ship
/Game/Ship/BluePrint/BP_ShipProjectile
/Game/Ship/BluePrint/BP_ShipProjectileWithSpeed
/Game/Ship/BluePrint/GMBP
/Game/Ship/Widgets/WBP_Crosshair
```

Nhóm asset phương tiện và trình diễn:

- `West_Ship_DDG102`: tàu chính, skeletal mesh, physics asset, animation, material, damaged state, FX và các level phụ.
- `East_Arty_D30122mm`: asset pháo/đối tượng đối kháng, model, damaged material và hiệu ứng firing/shell ejection.
- `Content/Levels/MainMapLevel.umap`: map mặc định để chạy game.
- `Content/Levels/TestLevel.umap`: map dùng cho kiểm thử/thử nghiệm scene.

Module C++ hiện chỉ đăng ký game module trong [Source/VDTPhase2Project/VDTPhase2Project.cpp](Source/VDTPhase2Project/VDTPhase2Project.cpp). Nếu bổ sung gameplay C++, cần cập nhật dependency trong [Source/VDTPhase2Project/VDTPhase2Project.Build.cs](Source/VDTPhase2Project/VDTPhase2Project.Build.cs) và giữ code runtime trong `Source/VDTPhase2Project`.

Dependency C++ hiện tại:

- `Core`
- `CoreUObject`
- `Engine`
- `InputCore`
- `EnhancedInput`

## Input

Project dùng Enhanced Input làm hệ thống input mặc định:

- `IA_MoveForwardBackward`: di chuyển tiến/lùi.
- `IA_MoveLeftRight`: di chuyển trái/phải.
- `IA_Aim`: ngắm.
- `IA_Fire`: bắn.
- `IA_LookAround`: quan sát xung quanh.
- `IA_Zoom`: zoom.
- `IMC_Ship`: mapping context cho tàu.

Các asset này nằm trong `Content/Ship/Inputs`. Mapping phím thực tế được lưu trong asset nhị phân `IMC_Ship`, vì vậy cần mở asset trong Unreal Editor để xem hoặc thay đổi phím. [Config/DefaultInput.ini](Config/DefaultInput.ini) xác nhận `EnhancedPlayerInput` và `EnhancedInputComponent` là class input mặc định, nhưng không thay thế mapping trong `IMC_Ship`.

## Cấu hình runtime

Một số thiết lập đáng chú ý trong [Config/DefaultEngine.ini](Config/DefaultEngine.ini):

- DirectX 12 là graphics RHI mặc định trên Windows.
- Shader target Windows là `PCD3D_SM6`.
- Ray tracing được bật.
- Mesh distance fields được bật.
- Virtual Shadow Maps được bật.
- Dynamic Global Illumination và reflection method được cấu hình cho desktop/high-quality rendering.
- Collision profile `WaterBodyCollision` được khai báo cho tương tác với mặt nước.
- Hardware targeting là desktop và performance target là maximum.

Các thiết lập này ưu tiên chất lượng hình ảnh. Khi chạy trên máy yếu hoặc gặp lỗi shader/GPU, hãy kiểm tra DX12, driver GPU và shader cache trước khi thay đổi asset gameplay.

## Cesium

Plugin `CesiumForUnreal` được bật trong [VDTPhase2Project.uproject](VDTPhase2Project.uproject). Các thiết lập liên quan nằm trong `Content/CesiumSettings`.

Khi chuyển project sang máy khác:

1. Cài đúng plugin Cesium cho Unreal Engine `5.6`.
2. Kiểm tra plugin đang Enabled trong **Edit > Plugins**.
3. Kiểm tra thông tin Cesium ion/server trong `Content/CesiumSettings`.
4. Nếu level dùng dữ liệu trực tuyến, đăng nhập hoặc cấu hình token theo môi trường local thay vì commit secret vào repository.
5. Mở `MainMapLevel` và kiểm tra terrain/imagery sau khi shader và asset load xong.

## Quy trình phát triển

### Chỉnh gameplay Blueprint

1. Mở `BP_Ship`, `BP_ShipProjectile` hoặc `GMBP` trong Content Browser.
2. Trước khi thay đổi, kiểm tra component, variable và event đang được tham chiếu từ level/game mode.
3. Nếu thay đổi input, cập nhật `IA_*` và `IMC_Ship` cùng nhau.
4. Test trên `MainMapLevel`; dùng `TestLevel` hoặc level showcase của tàu để cô lập lỗi nếu cần.
5. Lưu asset và kiểm tra redirector/reference trước khi đổi tên hoặc di chuyển thư mục.

### Chỉnh C++

1. Thêm class vào `Source/VDTPhase2Project` theo convention của Unreal.
2. Thêm module dependency cần thiết vào `VDTPhase2Project.Build.cs`.
3. Build `VDTPhase2ProjectEditor Win64 Development`.
4. Mở lại editor nếu module hot reload không phản ánh thay đổi đầy đủ.

### Kiểm tra trước khi gửi thay đổi

- Mở được project bằng Unreal Engine `5.6`.
- Map mặc định vẫn là `MainMapLevel`.
- `GMBP` và các asset input không bị missing reference.
- Play được map chính và kiểm tra được điều khiển tàu, ngắm, bắn, zoom nếu các tính năng này đã được nối trong Blueprint.
- Build editor ở configuration phù hợp không có lỗi C++.
- Không commit token, file tạm, shader cache hoặc dữ liệu local của editor.

## Lưu ý khi làm việc với asset

- Không chỉnh sửa trực tiếp file `.uasset` hoặc `.umap` bằng text editor.
- Không xóa `Content/__ExternalActors__` khi chưa hiểu level đang tham chiếu actor nào.
- Khi di chuyển asset trong Unreal Editor, dùng **Migrate/Move** và xử lý redirector để giữ reference.
- `DerivedDataCache`, `Intermediate`, `Saved` và `Binaries` là dữ liệu sinh ra/local; không dùng chúng làm nguồn sự thật cho gameplay.
- Các thư mục `Content/Developers` và `Content/Collections` có thể chứa dữ liệu làm việc cá nhân/editor; kiểm tra kỹ trước khi chia sẻ hoặc đưa vào release.

## Giới hạn tài liệu

README này được lập từ các file text/config và inventory asset hiện có. Nội dung graph Blueprint, actor cụ thể đặt trong map, phím được gán trong `IMC_Ship`, thông số material/FX, thứ tự gameplay và trạng thái kết nối Cesium ion không thể xác nhận đầy đủ chỉ bằng tên file `.uasset`/`.umap`. Khi cần tài liệu gameplay chính xác hơn, nên mở project bằng Unreal Editor và bổ sung:

- sơ đồ flow của `GMBP` và `BP_Ship`;
- bảng phím thực tế từ `IMC_Ship`;
- danh sách actor/streaming layer trong `MainMapLevel`;
- quy trình cấu hình Cesium ion cho môi trường local/CI;
- ảnh chụp hoặc video build đã được QA xác nhận.
