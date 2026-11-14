# Báo cáo Viettel Aerospace — VDT 2026 giai đoạn 2

Nguồn duy nhất: `reports.tex`. Bản đọc: `reports.pdf`. Mã khảo sát: `main@ce3074f`, ngày 14/11/2026. Nhánh `reports/phase2-viettel-aerospace` chỉ chứa tài liệu; không tự merge vào main.

## Biên dịch

Windows, XeLaTeX/MiKTeX; font Times New Roman, Arial và Consolas:

```powershell
cd reports
powershell -NoProfile -File .\build.ps1
```

Ba lượt biên dịch dùng jobname riêng trong `_build/compiled`, tránh aux do IDE tạo ở root. PDF chỉ được sao chép ra ngoài khi các lượt thành công. Thông báo MiKTeX chưa kiểm tra cập nhật là thông báo môi trường, không phải lỗi LaTeX. Trung gian không commit.

## Khảo sát và bằng chứng

`research/FEATURE_INVENTORY.md`: 25 mục, gồm 19 VERIFIED_WORKING có phạm vi, 3 KNOWN_LIMITATIONS, 1 IMPLEMENTED_NOT_RUNTIME_VERIFIED, 1 UNMERGED_BRANCH_ONLY và 1 HISTORICAL_OR_REMOVED. Không có mục PARTIALLY_IMPLEMENTED/EXPERIMENTAL riêng. Độ khó có mã/menu nhưng chưa có benchmark hit-rate so sánh.

Đọc các tệp ARCHITECTURE_INVENTORY, IMPLEMENTATION_EVIDENCE, TEST_EVIDENCE_MATRIX, KNOWN_LIMITATIONS, IMAGE_INVENTORY, REPORT_OUTLINE, CHAPTER_EVIDENCE_MAP và PDF_QA trong research.

Từ root repository:

```powershell
python reports/research/discover_project.py
python reports/research/make_wave_figure.py
```

Khảo sát assets dùng commandlet riêng, không lưu tài nguyên:

```powershell
& '<UE_ROOT>/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' '<PROJECT>/VDTPhase2Project.uproject' -run=pythonscript '-script=<PROJECT>/reports/research/inspect_unreal.py' -NullRHI -NoSound -NoSplash -Unattended
```

Các bài trận 803 giây, endurance sáu tàu và đo Small là bằng chứng lịch sử theo DLL/fixture, không được gọi là lượt chạy mới của ce3074f. Phiên báo cáo chỉ chạy fresh asset/graph inspection và PDF QA. Không sửa Source, Content, Config, gameplay/map. Portable terrain trên nhánh package chưa vào main.

## Hình và dọn tệp

Logo gốc do người dùng cung cấp nằm trong icons. Sơ đồ TikZ lấy từ source; biểu đồ đo từ JSON; nguồn ảnh trong IMAGE_INVENTORY. Các ảnh được crop vùng nền, bảng kết quả hoặc minimap để đọc rõ, không đổi nội dung. Không tạo ảnh gameplay giả.

Nội dung draft/mentor đã gộp vào nguồn duy nhất. Chỉ bỏ bản sao ảnh không được sử dụng và output LaTeX sinh tự động; giữ nguyên nguồn/bằng chứng ngoài reports. Manifest: `research/REPORT_CLEANUP.md`.
