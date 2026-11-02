# Báo cáo VDT Phase 2

- `reports.tex`: nguồn LaTeX UTF-8, chứa toàn bộ nội dung để chỉnh sửa.
- `reports.pdf`: bản biên dịch để đọc/demo.
- `images/`: hình minh họa; `SOURCES.json` ghi nguồn và phạm vi bằng chứng.
- `_build/`: file trung gian và ảnh render kiểm tra bố cục, được bỏ qua khi Git add.

Biên dịch trong thư mục này bằng **XeLaTeX**, hai lượt:

```powershell
xelatex -interaction=nonstopmode -halt-on-error reports.tex
xelatex -interaction=nonstopmode -halt-on-error reports.tex
```

Font: Times New Roman, Arial, Consolas. Hướng dẫn tự đặt tàu/target: Chương 3; điều khiển: Chương 4; quy trình demo: Chương 9. Các ảnh/lượt thử lịch sử được phân biệt với kết quả kiểm tra map mới.
