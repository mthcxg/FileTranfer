# FileTranfer
Project Bài tập lớn môn Thực hành Lập Trình Mạng HUST
Các chức năng:
- Đăng ký tài khoản mới​
- Đăng nhập vào tài khoản để sử dụng​
- Đăng xuất khỏi tài khoản​
- Cho phép người dùng tạo nhóm chia sẻ hoặc tham gia một nhóm. Người dùng có thể rời nhóm​
- Có thể xem các file đã được gửi trong nhóm​
- Hiển thị tất cả người dùng trong nhóm​
- Hiển thị tất cả các nhóm​
- Upload file.​
- Download file.
- Chat trong nhóm
- Ghi log hoạt động
# Giao thức
- REG <username> <password>
- LOGIN <username> <password>
- ROOM <number>: chuyển tới phòng khác (1 người chỉ được ở 1 phòng)
- LEAVE
- POST <filename>
- GET <filename>
- LOGOUT
- USER: hiển thị hết tất cả client trong room​
- FILES: hiển thị hết tất cả các file đã được chia sẻ trong room​
- CREATE <number>: tạo room mới​
- ALL: hiển thị tất cả room​

(NOTE: tạo thêm 1 folder tên server_file_storage để lưu file sau khi GET)
