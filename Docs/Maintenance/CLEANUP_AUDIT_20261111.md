# Kiểm tra khả năng dọn project — 11/11/2026

Phạm vi: kiểm tra tĩnh cây file hiện tại trên nhánh `refactor/code`, code C++, Build.cs, cấu hình, công cụ, JSON và các chuỗi tham chiếu trong asset. Không xóa, di chuyển asset, sửa gameplay hay chạy script setup/test. Không tuyên bố đã thử build hoặc PIE sau khi xóa vì chưa có thao tác xóa nào.

Dung lượng dưới đây là tổng kích thước logic, không phải dung lượng vật lý thực tế thu hồi trên ổ đĩa. Đã bỏ qua `.git` và không đi theo directory junction/reparse point. Các số có thể thay đổi khi Unreal đang chạy.

## 1. Nhóm có thể dọn trước

Đóng Unreal Editor và các tiến trình build/cook trước khi dọn các file đang được ghi.

| Đường dẫn | Kích thước hiện tại | Tác động khi xóa |
|---|---:|---|
| `Saved/Logs` | 5,68 MiB | Mất log cũ; lần chạy sau sinh log mới. Không tắt cơ chế ghi log. |
| `Saved/Crashes` | 262,43 MiB | Mất dump và hồ sơ chẩn đoán crash. |
| `Saved/Profiling` | 1.183 GiB | Chứa một trace `.uestats` khoảng 1,27 GB; mất dữ liệu profiling, không mất gameplay. |
| `Saved/Temp` | 0,61 MiB | File tạm; chỉ dọn sau khi đóng các tiến trình liên quan. |
| `Saved/ShaderDebugInfo` | 3,51 MiB | Dữ liệu chẩn đoán shader. |
| `Saved/Screenshots` | 13,28 MiB | Không cần để chạy game; sao lưu ảnh cần dùng cho báo cáo trước. |
| `Intermediate` | 6.364 GiB | Có thể tái tạo bằng UnrealBuildTool/project generation; mất cache/object/PCH, lần build sau lâu hơn. |
| `DerivedDataCache` tại project | 1,55 MiB | Có thể tái tạo; không phải toàn bộ DDC dùng chung của Unreal. |
| `Saved/Shaders` | 192,50 MiB | Dữ liệu shader sinh tự động; có thể cần biên dịch lại. |
| `Saved/Cooked` | 848,68 MiB | Mất output cook; lần đóng gói sau phải cook lại. Không xóa bản archive riêng đã tạo. |
| `Saved/StagedBuilds` | 1.349 GiB | Mất bản staging. Bản archive hiện nằm ở đường dẫn khác bên dưới. |
| `Build/Windows/FileOpenOrder/EditorOpenOrder.log`, `CookerOpenOrder.log` | Tổng 0,98 MiB | Log thứ tự mở file phục vụ packaging tối ưu; không phải cấu hình gameplay. Trong `Build` hiện chỉ thấy hai file này; không áp dụng kết luận đó cho mọi project Unreal. |
| Các thư mục `__pycache__` | Nhỏ | Python tự tạo lại. |

Ưu tiên `Intermediate` + `Saved/Profiling` + `Saved/StagedBuilds`: tổng kích thước logic khoảng **8,90 GiB**. Không cần dọn các file dữ liệu gameplay để lấy phần dung lượng này. Nếu muốn tiếp tục build nhanh, giữ `Intermediate` và chỉ dọn output/log không còn dùng.

## 2. Chỉ xóa khi chấp nhận mất khả năng tương ứng

| Nhóm | Nhận xét |
|---|---|
| `Binaries` — 2.061 GiB | File biên dịch có thể tạo lại, nhưng xóa sẽ mất DLL/executable dùng mở project hiện tại cho tới khi build lại thành công. Không cần xóa để dọn log. Tương tự đối với Binaries/Intermediate của plugin. |
| `.vscode` — 122,16 MiB | Compile database/response file có thể generate lại. Giữ `settings.json`, `tasks.json`, `c_cpp_properties.json` nếu còn muốn cấu hình IDE của mình. Không ảnh hưởng gameplay đã đóng gói. |
| `Saved/SeaNavigationTooling` — 2.227 GiB | Chủ yếu môi trường Python phục vụ pipeline navigation. Game dùng JSON đã tạo trong Content nên không cần môi trường này để chạy; xóa sẽ phải cài lại dependency khi tái tạo/kiểm định chart. |
| `Saved/Autosaves` — 14,55 MiB | Không cần cho chạy bình thường, nhưng mất khả năng khôi phục bản chưa lưu. |
| `Saved/Config` — 0,72 MiB | Cấu hình sinh tự động và tùy chỉnh người dùng/Editor. Xóa có thể reset layout và các override; không khuyên xóa nguyên để dọn log. |
| `cesium-request-cache.sqlite`, `-wal`, `-shm` | Cache Cesium ở gốc project, tổng khoảng 67 MiB cùng các file gốc khác. Có thể phải tải/đọc tile lại. Đóng Unreal trước khi đụng SQLite/WAL; giữ nếu demo phụ thuộc tile đã cache khi offline. |
| `Tools/Archive`, `Docs/Archive` | Lịch sử chẩn đoán đã được tách riêng, không được code runtime đọc trực tiếp. Có thể chuyển ra kho lưu trữ ngoài project nếu không còn cần hồ sơ cũ; các đường dẫn tài liệu liên quan sẽ cần cập nhật. |

## 3. Các phần cần giữ ngay lúc này

- **Bản build vừa đóng gói:** `Saved/BuildMacroFix/20261111/Package/Windows/`. Nếu xóa toàn bộ `Saved`, sẽ xóa luôn bản build này. Sao chép cả thư mục Windows ra nơi khác trước nếu muốn dọn mạnh.
- **Backup refactor chưa commit:** `Saved/SourceRefactor/20261111/original/`, `uht-repaired/`, các patch bảo toàn formatting và `Saved/BuildMacroFix/20261111/original/`, `BUILD_FIX_ONLY.patch`. Không coi mọi thứ dưới Saved là cache. Tổng Saved/SourceRefactor hiện khoảng 2.661 GiB, nhưng chỉ một phần là bản build/cache có thể tái tạo; phần source/patch là hồ sơ khôi phục quan trọng.
- **`Source`, `Content`, `Config`, `Plugins`, `.uproject`, Build.cs, Target.cs:** cấu trúc và đầu vào build/gameplay. Không dọn bằng cách xóa những file trông giống debug hoặc generated nếu chưa xác minh riêng.
- **`Data/SeaNavigation` chỉ 5,38 MiB:** giữ dữ liệu nguồn, attribution và báo cáo validation để tái tạo/test chart. Không có lợi đáng kể khi xóa.
- **`reports`:** báo cáo do người dùng yêu cầu. `reports/mentor_update.tex:19` dùng trực tiếp `../Docs/Integration/images/match-settings.png`; xóa toàn bộ Docs sẽ làm mất đầu vào ảnh khi biên dịch lại LaTeX.

Trong các recovery có junction trỏ về `Content`, `Plugins`, `Binaries` thật, ví dụ `Saved/MergeRecovery_20261020_180631/inspect/...` và `Saved/SourceRefactor/20261111/baseline-project/Content`. Không dùng công cụ xóa đệ quy có chế độ đi theo junction vào dữ liệu thật. Project còn nhiều thay đổi refactor chưa commit; không dùng một lệnh clean toàn project làm thay cho danh sách dọn đã chọn.

## 4. Ba JSON runtime cần giữ

| File | Vai trò | Tham chiếu đã kiểm tra |
|---|---|---|
| `Content/SeaNavigation/halong_navigation.json` | Chart A*: hệ tọa độ ECEF/ENU, grid nước/đất, clearance/geometry. Khoảng 3,66 MiB. | `ShipEnemyNavigationComponent.h:54`, `NavalSectorSubsystem.cpp:30`, `SeaTerrainCollisionComponent.cpp:75`; Build.cs:19 stage vào UFS. |
| `Content/SeaNavigation/quangninh_sectors.json` | Sector chiến trường, vùng hai đội, điều kiện navigable/hull và các anchor. Khoảng 469 KiB. | `NavalSectorSubsystem.cpp:38`; Build.cs:20 stage vào UFS. |
| `Content/SeaNavigation/quangninh_demo_routes.json` | Các điểm tham chiếu circuit/approach cho nhánh island layout còn trong code. | `NavalBattlefieldSubsystem.cpp:662`; Build.cs:21 vẫn yêu cầu staging. Không xóa chỉ vì tên preset từng bị bỏ khỏi UI. |

Kiểm tra tĩnh hiện tại: cả 3 parse JSON hợp lệ; chart 400×400, sector 25×25 với 625 sector; SHA1 chart khớp `navigation_sha1` trong sector. Đây là kiểm tra cú pháp/liên kết dữ liệu, không phải chạy lại toàn bộ navigation trong PIE. Sửa chart mà không tái tạo sector có thể bị code từ chối với lỗi stale sector.

Các JSON này là dữ liệu/cấu hình navigation, **không phải nơi chỉnh mọi setting của tàu**:

- Tốc độ, damping, buoyancy: `Content/NavalBattle/Profiles/DA_SmallShip`, `DA_MediumShip`, `DA_LargeShip` dưới dạng `.uasset`; code load trực tiếp trong `Private/Ships/Movement/ShipMovementComponent.cpp:41–44`.
- Vị trí tàu và Target A/B do tác giả bố trí: level/actor/component đã lưu trong Unreal.
- Số tàu, loại tàu, control mode, seed: `UNavalMatchSettings` và UI thiết lập trận; class hiện lưu trạng thái qua GameInstance, không đọc một file match-settings JSON riêng.
- Default map, GameMode, renderer, input, packaging/collision: `Config/Default*.ini` và asset tương ứng.

## 5. JSON còn lại và cấu hình project

| Nhóm | Có cần để game chạy? | Quyết định |
|---|---|---|
| 384 JSON trong `Docs` | Không thấy code runtime đọc trực tiếp. Phần lớn là báo cáo, telemetry, snapshot. | Có thể lưu trữ kết quả cũ, nhưng không xóa hàng loạt nếu vẫn dùng bộ test/report. Các verifier mới đọc `Docs/Architecture/SOURCE_MIGRATION_MANIFEST.json` và so với baseline hash. |
| `Data/SeaNavigation/validation_report.json` | Native automation RealOSM đọc trực tiếp; runtime không đọc. | Giữ nếu muốn test tiếp. |
| `Data/SeaNavigation/source_manifest.json` | Công cụ tạo chart đọc làm provenance. | Giữ cùng dữ liệu nguồn. |
| `Data/SeaNavigation/patrol_defaults.json` | Script cấu hình target đời cũ đọc; không thay thế actor A/B đã bố trí. | Có thể lưu trữ khi bỏ hẳn script đó; không cần đụng để demo. |
| `Data/SeaNavigation/cesium_alignment_report.json` | Báo cáo kiểm tra alignment. | Không cần cho runtime; giữ làm bằng chứng. |
| `Tools/Testing/ui_buoyancy_suite.json` | Danh sách 19 scenario QA; không thấy script trong repo tự tham chiếu tên file này. | Không cần cho runtime; có thể lưu trữ nếu không còn nạp danh sách suite này bằng công cụ bên ngoài/thủ công. |
| `Tools/Testing/Fixtures/shoreline_probe.json` | Input của bài kiểm tra gần bờ. | Không phải cấu hình runtime; giữ cùng fixture. |
| `reports/images/SOURCES.json` | Nguồn gốc và phạm vi bằng chứng ảnh. | Giữ với báo cáo. |
| `.vscode/*.json` | Cấu hình IDE, compile database, task. | Không cần cho game đóng gói; phân biệt generated với tùy chỉnh người dùng. `tasks.json` hiện là task g++ build một file, không phải UnrealBuildTool. |
| `.vsconfig` | JSON chọn workload Visual Studio. | Không phải gameplay config; nên giữ cho môi trường dev. |
| `.uproject`, hai `.uplugin` | Là descriptor JSON dù đuôi không phải `.json`. | Cần giữ. `SpecialAgent.uplugin` đang dùng `EditorNoCommandlet`, là sửa lỗi cooking vừa xác minh. |

399 file đuôi `.json` ngoài các cây generated đã quét đều parse hợp lệ; 4 descriptor `.uproject`/`.uplugin`/`.vsconfig` và 2 GeoJSON dữ liệu nguồn cũng parse hợp lệ. Kết luận không áp dụng cho mọi JSON sinh tạm trong Saved/Intermediate.

Terrain Cesium còn có đầu vào ngoài project: launcher `Tools/RunDemo.ps1` kiểm tra `Terrain/layer.json` và `Imagery/tilemapresource.xml` dưới TerrainRoot. Không nhầm các manifest/tile địa hình đó với log JSON. Bản build không tự mang theo toàn bộ bộ terrain bên ngoài chỉ vì ba JSON navigation đã được stage.

## 6. Không xóa file C++ chỉ vì tên Test/Debug

- `Private/Tests/ShipEnemyNavigationTests.cpp`: đăng ký native automation, có guard `WITH_DEV_AUTOMATION_TESTS`. Xóa sẽ mất test; source-preservation manifest hiện cũng yêu cầu file này.
- `Private/Tests/ShipWeaponFailureProbe.cpp/.h`: định nghĩa reflected pawn fixture và `UShipCombatLibrary::VerifyMagazineFailurePath`, được khai báo UFUNCTION trong public header. Xóa file đơn lẻ có thể làm hỏng UHT/link và công cụ kiểm thử.
- `Private/Combat/Editor/ShipDebugEditor.cpp`: định nghĩa `SampleOceanWaves`, `InspectImpactDebug`, gồm cả stub cần link ở cấu hình non-Editor. Đây không phải output debug có thể xóa như `.log`.
- `NavalCesiumCompatibility.h`: file mới sửa lỗi compiler; là dependency build thực tế, không phải test/debug rác.
- `bDrawDebug` ở navigation/combat mặc định false; xóa log trên ổ đĩa không thay đổi cờ debug, không tự ngừng tạo log lần chạy sau.

Các đường dẫn `Tools/` tìm được trong vài material asset đã đối chiếu là `/Engine/ArtTools/...`, không phải thư mục Tools của repository. Không thấy chuỗi tham chiếu Docs/Data/SeaNavigation trong các package asset đã quét. Tìm chuỗi không phải Asset Registry dependency certification hoặc bằng chứng loại trừ mọi đường dẫn được tạo động.

**Khuyến nghị:** dọn output sinh tự động lớn trước, giữ runtime JSON và backup source chưa commit. Toàn bộ Tools chỉ khoảng 0,73 MiB; bỏ công cụ QA để tiết kiệm phần dung lượng này không đáng so với 8,90 GiB ở ba nhóm generated đã chỉ ra.
